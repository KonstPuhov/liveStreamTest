/*
* Тестовое задание / клиент
*
* Author:
*	Konstantin P. kst-pu@ya.ru
*/

#include <stdlib.h>
#include <unistd.h>
#include <cassert>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <algorithm>
#include <random>
#include <memory>
#include <inttypes.h>

#include <common.h>
#include <classes.h>

// Структура параметров из ргументов командной строки
static struct {
    const char *fileName;
    const char *ip; // IP сервера
	int portS,portC, tmout;
	void type() {
		printf("File: %s IP: %s PortS:%d PortC:%d Timeout:%d\n", fileName, ip, portS, portC, tmout);
	}
} args = {"myCat.jpg", "127.0.0.1", 3210, 3211, 500 };

static void usage() {
	printf(
	"Usage: client [option] ... (%s)\n"
	"Программа - UDP клиент,\n"
	"Опции:\n"
	"\t-f\t\tip-имя файла\n"
	"\t-i\t\tip-адрес сервера\n"
	"\t-p\t\tudp-порт сервера\n"
	"\t-с\t\tudp-порт клиента (для ACK)\n"
	"\t-t\t\tтаймаут,мсек\n"
	"\t-?\t\tвывод справки\n"
	"\t-v\t\tвывод версии\n",
	_version()
	);
}

static bool quit;
static int sockReq, sockAck;
static sockaddr_in sinReq, sinAck;

static void exitHandler(int sig) {
	(void)sig;
	quit = true;
}
static void initSignals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = exitHandler;
    assert(sigaction(SIGINT, &sa, NULL) == 0);
}

static void run();
static size_t recvWithTout(int sock, sockaddr_in *peer, int timeoutMs, byte *buf, size_t bufSize);

int main(int argc, char **argv)
{
	const char *opts = "vp:c:t:i:f:?";
	int opt;
	
	while (opt = getopt(argc, argv, opts), opt!=-1) {
		switch(opt) {
		case 'v':
			printVersion();
			exit(0);
		case 'p':
			args.portS = atoi(optarg);
			break;
		case 'c':
			args.portC = atoi(optarg);
			break;
		case 'f':
			args.fileName = optarg;
			break;
		case 'i':
			args.ip = optarg;
			break;
		case 't':
			args.tmout = atoi(optarg);
			break;
		case '?':
		default:
			usage();
			exit(0);
		}
	}
	args.type();

	// создать сокеты
	sockReq = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockReq < 0) {
			errlog("socket(), %s", strerror(errno));
			return -1;
	}
	sockAck = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockAck < 0) {
			errlog("socket(), %s", strerror(errno));
			close(sockReq);
			return -1;
	}
	log("sockets OK\n");

	memset(&sinReq, 0, sizeof(sockaddr_in));
	sinReq.sin_family = AF_INET;
	sinReq.sin_port = htons(args.portS);
    assert(inet_aton(args.ip , &sinReq.sin_addr) != 0); 

	memset(&sinAck, 0, sizeof(sockaddr_in));
	sinAck.sin_family = AF_INET;
	sinAck.sin_port = htons(args.portC);
    assert(inet_aton(args.ip , &sinAck.sin_addr) != 0); 

	if(bind(sockAck, (sockaddr *)&sinAck, sizeof(sockaddr_in)) == -1) {
		close(sockAck);
		close(sockReq);
		errlog(strerror(errno));
		return -1;
	}

	initSignals();
	log("begin...\n");

	try{ run(); }
	catch (const char* e) {
		log("exception \"%s\", terminated\n", e);
	}

	puts("\n\n*	*	*	*	*	*	*\n"
		"*	... it's the end, good luck!		*\n"
		"*	*	*	*	*	*	*\n");

	close(sockAck);
	close(sockReq);
	return 0;
}

static void run() {
	// ID файла содержит ID процесса и номер порта ACK
	uFileID fileID(getpid(), args.portC);
	debug("file ID=%" PRIu64 "\n", fileID.ui64);

	CSequence ffile(args.fileName);
	auto packNum = ffile.GetTotal(); // количество пакетов 
	CPacker reqPack(fileID.ui64, packNum);

    // создать вектор    
    std::unique_ptr<int[]> loto(new int[packNum]);
    // пронумеровать
    for(int i=0; i<(int)packNum; i++)
        loto[i] = i;
    // перемешать
    std::shuffle(&loto[0], &loto[packNum], std::default_random_engine());

	// Читать файл и посылать пакеты серверу
	uint32_t remoteCrc;
	bool fileComplete = false; // сервер получил весь файл
	for(size_t i=0;i<packNum && !quit;i++)  {
		auto block = ffile.GetBlock(loto[i]);
		if(!block.chunk) {
			throw "внутренняя ошибка";
		}
		auto data = reqPack.Pack(CPacker::eREQ, block.chunk, block.size, loto[i]);

		int tryCount = 7; // семь попыток получить ACK
		for(; tryCount;tryCount--) {
			// Отправка REQ
			if (sendto(sockReq, data, PACK_SIZE(block.size), 0 , (sockaddr*)&sinReq, sizeof sinReq)==-1)
			{
				log("sendto(), %s\n", strerror(errno));
				continue;
			}
			debug("send REQ[%u bytes], seq_num=%u\n", block.size, loto[i]);

			// Получить ACK
			byte packet[PACK_SIZE(4)]; // 4 это размер CRC
			size_t recvLen;
			try {
				recvLen = recvWithTout(sockAck, &sinAck, args.tmout, packet, sizeof packet);
			}
			catch(int i) {
				if(i==-args.tmout) {
					debug("ACK timeout\n");
					continue;	// таймаут, повторить
				}
				// ошибка, продолжить попытки
				errlog(strerror(errno));
				continue;
			}
			// Распаковать ответ
			CPacker ackPack;
			auto ackHdr = ackPack.Unpack(packet);
			if(recvLen < sizeof(SPackHeader)) {
				debug("пакет ACK слишком мал\n");
				continue; // продолжить попытки
			}
			if(ackHdr.type != CPacker::eACK) {
				debug("не тот тип пакета\n");
				continue; // продолжить попытки
			}
			if(ackHdr.id.ui64 != reqPack.GetID()) {
				debug("не тот ID пакета ACK\n");
				continue; // продолжить попытки
			}
			
			if(ackHdr.seq_number != ackHdr.seq_total)
				break; // ACK получен, продолжить передачу остальных блоков файла
			
			// Сервер ответил что все блоки переданы
			if(recvLen != PACK_SIZE(4)) {
				debug("пакет ACK не содержит CRC\n");
				continue; // продолжить попытки
			}
			SPacket *p = (SPacket*)packet;
			remoteCrc = ntohl(p->crc);
			fileComplete = true;
			break;
		}
		if(!tryCount) {
			throw "сервер не отвечает";
		}
	}
	if(!fileComplete) {
		// Отправлены все блоки файла, но подтверждения от сервера не пришло
		puts("\n\n"
			"----------------------------\n"
			"--     СБОЙ НА СЕРВЕРЕ    --\n"
			"--    Сервер не прислал   --\n"
			"--    завершающий пакет.  --\n"
			"----------------------------\n"
		);
		return;
	}
	// Сервер прислал CRC принятого файла, сравнить с локальной CRC
	uint32_t localCrc = ffile.GetCRC();
	if(remoteCrc == localCrc) {
		printf("\n\n"
			"++++++++++++++++++++++++++++\n"
			"++      УСПЕШНО !         ++\n"
			"++ CRC файла: %X\n"
			"++ CRC сервера: %X\n"
			"++++++++++++++++++++++++++++\n",
			localCrc, remoteCrc
		);
	} else {
		printf("\n\n"
			"----------------------------\n"
			"--        ОШИБКА          --\n"
			"-- CRC файла: %X\n"
			"-- CRC сервера: %X\n"
			"----------------------------\n",
			localCrc, remoteCrc
		);
	}
}

static size_t recvWithTout(int sock, sockaddr_in *peer, int timeoutMs, byte *buf, size_t bufSize) {
	timeval tv{0,timeoutMs*1000};
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	socklen_t lenAddr = sizeof *peer;
	ssize_t n = recvfrom(sock, buf, bufSize, 0, (sockaddr*)peer, &lenAddr);	
	if (n<=0)
		throw (int)-timeoutMs;
	return n;
}


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

#include <common.h>
#include <classes.h>

// Структура параметров из ргументов командной строки
static struct {
    const char *fileName;
    const char *ip; // IP сервера
	int port, tmout;
	void type() {
		printf("File: %s IP: %s Port:%d Timeout:%d\n", fileName, ip, port, tmout);
	}
} args = {"myCat.jpg", "127.0.0.1", 3210, 500 };

static void usage() {
	puts(
	"Usage: client [option] ... (v." _VERSION_ ")\n"
	"Программа - UDP клиент,\n"
	"Опции:\n"
	"\t-f\t\tip-имя файла\n"
	"\t-i\t\tip-адрес сервера\n"
	"\t-p\t\tudp-порт\n"
	"\t-t\t\tтаймаут,мсек\n"
	"\t-?\t\tвывод справки\n"
	"\t-v\t\tвывод версии\n"
	);
}

static bool quit;
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
	const char *opts = "vp:t:?";
	int opt;
	
	while (opt = getopt(argc, argv, opts), opt!=-1) {
		switch(opt) {
		case 'v':
			printVersion();
			exit(0);
		case 'p':
			args.port = atoi(optarg);
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
	try{ run(); }
	catch (char* e) {
		log("exception \"%s\", terminated\n", e);
	}

	log("*	*	*	*	*	*	*\n"
		"*	... it's the end, good luck!		*\n"
		"*	*	*	*	*	*	*\n");
	return 0;
}

static void run() {
	// создать сокет на данный порт
	int sock;
	sockaddr_in sinServ;

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock < 0) {
			log("socket(), %s", strerror(errno));
	}
	log("socket OK\n");

	memset(&sinServ, 0, sizeof(sockaddr_in));
	sinServ.sin_family = AF_INET;
	sinServ.sin_port = htons(args.port);

    assert(inet_aton(args.ip , &sinServ.sin_addr) != 0); 

	initSignals();
	log("begin ...\n");

	CSequence ffile(args.fileName);
	auto packNum = ffile.GetTotal(); // количество пакетов 
	CPacker reqPack(getpid(), packNum);

    // создать вектор    
    std::unique_ptr<int[]> loto(new int[packNum]);
    // пронумеровать
    for(int i=0; i<(int)packNum; i++)
        loto[i] = i;
    // перемешать
    std::shuffle(&loto[0], &loto[packNum], std::default_random_engine());

	// Читать файл и посылать пакеты серверу
	uint32_t remoteCrc;
	for(size_t i=0;i<packNum && !quit;i++)  {
		auto block = ffile.GetBlock(loto[i]);
		auto data = reqPack.Pack(CPacker::eREQ, block.chunk, block.size, loto[i]);

		// Отправка REQ
		if (sendto(sock, data, PACK_SIZE(block.size), 0 , (sockaddr*)&sinServ, sizeof sinServ)==-1)
		{
			log("sendto(), %s\n", strerror(errno));
			continue;
		}
		debug("send %u bytes, seq_num=%u\n", block.size, loto[i]);

		// Получить ACK
		int tryCount = 7; // семь попыток получить ACK
		for(; tryCount;tryCount--) {
			byte packet[PACK_SIZE(4)]; // 4 это размер CRC
			size_t recvLen;
			try {
				recvLen = recvWithTout(sock, &sinServ, args.tmout, packet, sizeof packet);
			}
			catch(int i) {
				if(i==args.tmout)
					continue;	// таймаут, повторить
				// ошибка, продолжить попытки
				log("%s::%d: %s\n", __FILE__, i, strerror(errno));
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
				debug("не тот тип пакета ACK\n");
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
			goto final;
		}
		if(!tryCount) {
			throw "сервер не отвечает";
		}
	}
	final:
	// Сервер прислал CRC принятого файла, сравнить с локальной CRC
	uint32_t localCrc = ffile.GetCRC();
	if(remoteCrc == localCrc) {
		log(
			"++++++++++++++++++++++++++++\n"
			"++      УСПЕШНО !         ++\n"
			"++ CRC файла: %X\n"
			"++ CRC сервера: %X\n"
			"++++++++++++++++++++++++++++\n",
			localCrc, remoteCrc
		);
	} else {
		log(
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
	fd_set fds;
	FD_ZERO(&fds) ;
	FD_SET(sock, &fds) ;
	timeval tv{0,timeoutMs*1000};
	int n = select(sock, &fds, NULL, NULL, &tv);
	if (!n)
		throw timeoutMs;
	else if(n==-1)
		throw -__LINE__;

	socklen_t lenAddr = sizeof *peer;
	return recvfrom(sock, buf, bufSize, 0, (sockaddr*)peer, &lenAddr);	
}

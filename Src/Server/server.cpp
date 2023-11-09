/*
 * Тестовое задание / сервер
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
#include <inttypes.h>
#include <memory>
#include <sstream>

#include <common.h>
#include <classes.h>

// Структура параметров из ргументов командной строки
static struct
{
	int port,
		bitrate; // kbit/sec
	void type()
	{
		printf("Port:%d Bitrate:%d\n", port, bitrate);
	}
} args = {3210, 4096};

static void usage()
{
	printf(
		"Usage: server [option] ... (%s)\n"
		"Программа - UDP сервер,\n"
		"Опции:\n"
		"\t-p\t\tudp-порт\n"
		"\t-b\t\tбитрейт, кбит/сек\n"
		"\t-?\t\tвывод справки\n"
		"\t-v\t\tвывод версии\n",
		_version());
}

static bool quit;
static void exitHandler(int sig)
{
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

struct TServData
{
	int sockFd;
	sockaddr_in peerAdr;
	socklen_t slen;
	byte buf[MTU];
	ssize_t recvSize;
	TServData() : slen(sizeof peerAdr) {}
};

static void *thrServe(void *frame);
static void sendAck(int sock, sockaddr_in *peer, CPacker::SHead *hdr, CSequence *series);
static void onFileComplete(uint64_t fileID);
static TSeriesMap fileMap;

int main(int argc, char **argv)
{
	const char *opts = "vb:p:t:?";
	int opt;

	while (opt = getopt(argc, argv, opts), opt != -1)
	{
		switch (opt)
		{
		case 'v':
			printVersion();
			exit(0);
		case 'p':
			args.port = atoi(optarg);
			break;
		case 'b':
			args.bitrate = atoi(optarg);
			break;
		case '?':
		default:
			usage();
			exit(0);
		}
	}
	args.type();

	// создать сервисный сокет на данный порт
	int listener;
	listener = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (listener < 0)
	{
		log("socket(), %s", strerror(errno));
	}
	log("socket OK\n");

	sockaddr_in sinMy;
	memset(&sinMy, 0, sizeof(sockaddr_in));
	sinMy.sin_family = AF_INET;
	sinMy.sin_port = htons(args.port);
	sinMy.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(listener, (sockaddr *)&sinMy, sizeof(sockaddr_in)) == -1) {
		errlog(strerror(errno));
		return -1;
	}

	initSignals();
	
	log("begin ...\n");

	// Читать сокет и запускать нить на обработку пакета
	for (; !quit;)
	{
		TServData *sd = new TServData;
		sd->sockFd = listener;
		if ((sd->recvSize = recvfrom(listener, sd->buf, MTU, 0, (sockaddr *)&sd->peerAdr, &sd->slen)) == -1)
		{
			log("recvfrom(), %s\n", strerror(errno));
			delete sd;
			continue;
		}
		debug("recvLen=%d\n", sd->recvSize);
		if ((size_t)sd->recvSize <= sizeof(SPackHeader))
		{
			errlog("пакет REQ слишком мал\n");
			delete sd;
			continue;
		}

		/* создание потока */
		pthread_t tid;
		pthread_create(&tid, NULL, thrServe, sd);
	}

	puts("*	*	*	*	*	*	*\n"
		"*	... it's the end, good luck!		*\n"
		"*	*	*	*	*	*	*\n");
	return 0;
}

// Вызывается по завершению передачи файла
static void onFileComplete(uint64_t fileID) {
	log("file %" PRIu64 " complete", fileID);
	std::stringstream fn;
	fn << fileID << ".bin";
	try {
		fileMap[fileID]->StoreToFile(fn.str().c_str());
	} catch(...) {}
	delete fileMap[fileID];
	fileMap.erase(fileID);
}


// Обработчик пакета
static void *thrServe(void *data)
{
	static CPacker packer;

	// пакет с префиксом от сервера
	TServData *sd = (TServData *)data;
	// собственно пакет
	SPacket *netPack = (SPacket *)sd->buf;

	// Распаковать заголовок пакета
	auto hdr = packer.Unpack(sd->buf);
	if (hdr.type != CPacker::eREQ)
	{
		errlog("не тот тип пакета\n");
		delete sd;
		return NULL;
	}
	debug("received REQ seq_number=%d[pay=%d bytes] from %x\n", hdr.seq_number, PAY_SIZE(sd->recvSize),
		  sd->peerAdr.sin_addr.s_addr);

	// Найти серию по ID
	debug("file ID=%" PRIu64 "\n", hdr.id.ui64);
	CSequence *series;
	auto f = fileMap.find(hdr.id.ui64);
	// Если серии с таким ID ещё нет, то создать
	if (f == fileMap.end()) {
		series = new CSequence(hdr.seq_total);
		fileMap[hdr.id.ui64] = series;
	}
	else
		series = f->second;
	// Положить чанк в массив по своему адресу
	series->PutBlock(hdr.seq_number, netPack->payload, PAY_SIZE(sd->recvSize));

	// Выдержать паузу для обеспечения нужного битрейта.
	// Для упрощения время отклика клиента и время прохождения пакетов
	// здесь не учитываются. Чтобы учитывать все факторы, нужно сохранять
	// время отправки предыдущего пакета, сравнивать его с текущим временем и
	// на их разность уменьшать длительность задержки.
	usleep(bitrate2usecs(args.bitrate, PAYLOAD_MAXSIZE));

	// Ответить клиенту
	hdr.type = CPacker::eACK;
	hdr.seq_number = series->GetTotal();
	// ... остальные поля заголовка остались такими же как в запросе

	int sockAck = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sockAck<0) {
		errlog(strerror(errno));
		delete sd;
		return NULL;
	}
	sockaddr_in sinAck;
	memset(&sinAck, 0, sizeof(sockaddr_in));
	sinAck.sin_family = AF_INET;
	sinAck.sin_port = htons(hdr.id.ui32[0]); // порт для ACK упакован в ID
	sinAck.sin_addr.s_addr = sd->peerAdr.sin_addr.s_addr;
	sendAck(sockAck, &sinAck, &hdr, series);
	if(series->IsFull())
		onFileComplete(hdr.id.ui64);

	delete sd;
	close(sockAck);
	return NULL;
}

static void sendAck(int sock, sockaddr_in *peer, CPacker::SHead *hdr, CSequence *series)
{
	CPacker pack(hdr->id.ui64, hdr->seq_total);
	byte *ack;
	size_t paySz = 0;

	if (series->IsFull())
	{
		// Ответ на последний пакет сессии
		auto crc = htonl(series->GetCRC());
		paySz = sizeof crc;
		ack = pack.Pack(CPacker::eACK, (byte *)&crc, paySz, hdr->seq_number);
	}
	else
		ack = pack.Pack(CPacker::eACK, NULL, 0, hdr->seq_number);

	if (sendto(sock, ack, PACK_SIZE(paySz), 0, (sockaddr *)peer, sizeof(sockaddr_in)) == -1)
		errlog("sendto(ACK), %s\n", strerror(errno));
	debug("send ACK seq_number=%d[pay=%d bytes] of %d\n", hdr->seq_number, paySz, hdr->seq_total);
}

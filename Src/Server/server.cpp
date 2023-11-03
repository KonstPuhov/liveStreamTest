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
#include <map>

#include <common.h>
#include <classes.h>

// Структура параметров из ргументов командной строки
static struct {
	int port,
	 bitrate; // kbit/sec
	void type() {
		printf("Port:%d Bitrate:%d\n", port, bitrate);
	}
} args = {3210, 4096 };

static void usage() {
	puts(
	"Usage: server [option] ... (v." _VERSION_ ")\n"
	"Программа - UDP сервер,\n"
	"Опции:\n"
	"\t-p\t\tudp-порт\n"
	"\t-b\t\tбитрейт, кбит/сек\n"
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

struct TServData {
	struct sockaddr_in peerAdr;
	socklen_t slen;
	byte buf[MTU];
	ssize_t recvSize;
	TServData(): slen(sizeof peerAdr){}
};

static void* thrServe(void *frame);

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
	struct sockaddr_in sinMy;

	listener = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(listener < 0) {
			log("socket(), %s", strerror(errno));
	}
	log("socket OK\n");

	memset(&sinMy, 0, sizeof(sockaddr_in));
	sinMy.sin_family = AF_INET;
	sinMy.sin_port = htons(args.port);
	sinMy.sin_addr.s_addr = htonl(INADDR_ANY);
	assert(bind(listener, (struct sockaddr *)&sinMy, sizeof(sockaddr_in)) != -1);
	log("bind OK\n");

	initSignals();
	log("begin ...\n");

	// Читать сокет и запускать нить на обработку пакета
	for(;!quit;)  {
		TServData  *sd = new TServData();
		if ((sd->recvSize = recvfrom(listener, sd->buf, MTU, 0, (struct sockaddr *) &sd->peerAdr, &sd->slen)) == -1) {
			log("recvfrom(), %s\n", strerror(errno));
			delete sd;
			continue;
		}
		debug("recvLen=%d\n", sd->recvSize);
		if(sd->recvSize==0) {
			delete sd;
			continue;
		}

		/* создание потока */
		pthread_t tid;
		pthread_create(&tid, NULL, thrServe, sd);
	}

	log("*	*	*	*	*	*	*\n"
		"*	... it's the end, good luck!		*\n"
		"*	*	*	*	*	*	*\n");
	return 0;
}

// Обработчик пакета
static void* thrServe(void *data) {
	std::map<uint64_t, CSequence*> map;
	static CPacker packer;

	// пакет с префиксом от сервера
	TServData *servdt = (TServData*)data;
	// собственно пакет 
	SPacket *netPack = (SPacket*)servdt->buf;

	// Распаковать заголовок пакета
	auto hdr = packer.Unpack(servdt->buf);

	// Найти серию по ID
	CSequence *series;
	auto f = map.find(hdr.id.ui64);
	// Если серии с таким ID ещё нет, то создать 
	if(f==map.end()) {
		series = new CSequence(hdr.seq_total);
		map[hdr.id.ui64] = series;
	} else
		series = f->second;
	// Положить чанк в массив по своему адресу
	series->Put(hdr.seq_number, netPack->payload, PAY_SIZE(servdt->recvSize));

	debug("get seq%d[%d] from %x\n", hdr.seq_number, PAY_SIZE(servdt->recvSize),
		servdt->peerAdr.sin_addr.s_addr);

	delete servdt;
	return NULL;
}
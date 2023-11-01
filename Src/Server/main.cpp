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

#include <common.h>
#include <packet.h>

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
		int recvLen;
		TServData  *sd = new TServData();
		if ((recvLen = recvfrom(listener, sd->buf, MTU, 0, (struct sockaddr *) &sd->peerAdr, &sd->slen)) == -1) {
			log("recvfrom(), %s\n", strerror(errno));
			delete sd;
			continue;
		}
		debug("recvLen=%d\n", recvLen);
		if(recvLen==0) {
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
	TServData *servHd = (TServData*)data;
	SPackHeader *packHd = (SPackHeader*)servHd->buf;
	debug("get seq%d[%d] from %x\n", packHd->seq_number, packHd->pay_size, servHd->peerAdr.sin_addr.s_addr);
	delete data;
	return NULL;
}
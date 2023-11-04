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
	struct sockaddr_in sinServ;

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
	CPacker pack(getpid(), packNum);

    // создать вектор    
    int *loto = new int[packNum];
    // пронумеровать
    for(size_t i=0; i<packNum; i++)
        loto[i] = i;
    // перемешать
    std::shuffle(&loto[0], &loto[packNum], std::default_random_engine());

	// Читать файл и посылать пакеты серверу
	for(size_t i=0;i<packNum && !quit;i++)  {
		auto block = ffile.GetBlock(loto[i]);
		auto data = pack.Pack(CPacker::eREQ, block.chunk, block.size, loto[i]);

		// Отправка REQ
		if (sendto(sock, data, PACK_SIZE(block.size), 0 , (struct sockaddr *) &sinServ, sizeof sinServ)==-1)
		{
			log("sendto(), %s\n", strerror(errno));
			continue;
		}
		debug("send %u bytes, seq_num=%u\n", block.size, loto[i]);
	}
	delete []loto;
}

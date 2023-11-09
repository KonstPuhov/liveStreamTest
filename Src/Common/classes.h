#pragma once
#include <stdint.h>
#include "common.h"
#include <stdlib.h>
#include <set>
#include <map>
#include <pthread.h>

#pragma pack(push, 1)
union uFileID {
    byte bytes[8];
    uint32_t ui32[2];
    uint64_t ui64;
    uFileID() { }
    uFileID(uint32_t ms, uint32_t ls) { ui32[0]=ls; ui32[1]=ms; }
};
// Пакеты REQ и ACK имеют одинаковый заголовок
struct SPackHeader{
    uint32_t seq_number; // номер пакета
    uint32_t seq_total; // количество пакетов с данными
    uint8_t type; // тип пакета: 0 == ACK, 1 == PUT
    uFileID id; // 8 байт - идентификатор, отличающий один файл от другого
};

const int MTU = 1500-28;
const int PAYLOAD_MAXSIZE = MTU - sizeof(SPackHeader);
#define PACK_SIZE(payloadSize)  (sizeof(SPackHeader)+(payloadSize))
#define PAY_SIZE(packSize)  ((packSize) - sizeof(SPackHeader))

struct SPacket{
    SPackHeader header;
    union {
    byte payload[PAYLOAD_MAXSIZE];
    uint32_t crc;
    };
};
#pragma pack(pop)

// Упаковывает и распаковывает заголовки сетевых пакетов 
class CPacker {
public:
    enum { eREQ, eACK };
    struct SHead {
        uint32_t seq_number; // номер пакета
        uint32_t seq_total; // количество пакетов с данными
        uint8_t type; // тип пакета: 0 == ACK, 1 == PUT
        uFileID id; // 8 байт - идентификатор, отличающий один файл от другого
    };
private:
    SHead h;
public:
    CPacker() {}
    CPacker(uint64_t _id, uint32_t _total) {
        h.id.ui64 = _id;
        h.seq_total = _total;
        h.type = eREQ;
    }
    // Упаковщик блока в формат сетевого пакета 
    byte *Pack(int _type, byte *data, size_t paySize, uint32_t seqNum);
    // Распаковывает заголовок сетевого пакета
    SHead& Unpack(byte *netPack);
    uint64_t GetID() { return h.id.ui64; }
};

// Контейнер для входящих пакетов
class CSequence: public std::set<size_t> {
    static const size_t cBlkSize = PAYLOAD_MAXSIZE;
    typedef byte tChunk[cBlkSize];
	uint32_t chunkNum;    // размер контейнера в блоках
	uint32_t total_seq; // счётчик пришедших блоков
	int lastChunkSize;  // последний блок может быть урезан, здесь его размер
	tChunk *chunks; // блоки
public:
    ~CSequence() { if(chunks) delete[] chunks; }
    // Конструктор для сервера. Создаётся пустая серия.
	CSequence(uint32_t _chunkNum): chunkNum(_chunkNum), total_seq(0), lastChunkSize(-1), chunks(NULL) {
		chunks = new tChunk[_chunkNum];
	}
    // Конструктор для клиента. Серия заполняется из файла.
    CSequence(const char *fName);
	void PutBlock(size_t idx, byte *chunk, size_t size);
    bool IsFull() { return chunkNum==total_seq; }
    uint32_t GetTotal() { return total_seq; }
    uint32_t GetCRC();
    void StoreToFile(const char *fileName);

    struct SBlock {
        uint8_t *chunk; // содержимое блока
        size_t size; // размер содержимого (если блок последний, то размер может быть меньше заданного)
    };
    SBlock GetBlock(uint32_t idx);
};

class CMutex {
	pthread_mutex_t mutex;
public:
	CMutex() {
		pthread_mutex_init(&mutex, NULL);
	}
	~CMutex() {
		pthread_mutex_destroy(&mutex);
	}
	void Lock() {
		pthread_mutex_lock(&mutex);
	}
	void Unlock() {
		pthread_mutex_unlock(&mutex);
	}
};

class TSeriesMap: public std::map<uint64_t, CSequence*>, public CMutex {
};


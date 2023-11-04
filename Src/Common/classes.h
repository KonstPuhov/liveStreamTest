#pragma once
#include <stdint.h>
#include "common.h"
#include <stdlib.h>
#include <set>

#pragma pack(push, 1)
union uFileID {
    byte bytes[8];
    uint32_t ui32[2];
    uint64_t ui64;
};
// Пакеты REQ и ACK имеют одинаковый заголовок
struct SPackHeader{
    uint32_t seq_number; // номер пакета
    uint32_t seq_total; // количество пакетов с данными
    uint8_t type; // тип пакета: 0 == ACK, 1 == PUT
    uFileID id; // 8 байт - идентификатор, отличающий один файл от другого
    uint16_t pay_size; // payload size
};
#pragma pack(pop)

const int MTU = 1500-28;
const int PAYLOAD_MAXSIZE = MTU - sizeof(SPackHeader);
#define PACK_SIZE(payloadSize)  (sizeof(SPackHeader)+(payloadSize))
#define PAY_SIZE(packSize)  ((packSize) - sizeof(SPackHeader))

struct SPacket{
    SPackHeader header;
    byte payload[PAYLOAD_MAXSIZE];
};

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
};

// Загрузчик файла с разбивкой на блоки
class CFragmFile {
    size_t blkSize;
    uint32_t totalBlkNum;
    int fd;
    off_t fsize;
    uint8_t *popBuf;
public:
    struct SBlock {
        uint8_t *buf; // содержимое блока
        size_t bufSize; // размер содержимого (если блок последний, то размер может быть меньше заданного)
        SBlock(uint8_t *_buf,size_t _bufSize):
            buf(_buf), bufSize(_bufSize) {}
    };
public:
    CFragmFile() : popBuf(NULL) {}
    ~CFragmFile() { if(IsOpened()) Close(); }
    void Open(const char *fName, size_t blockSz);
    void Close();
    bool IsOpened() { return popBuf!=NULL; }
    SBlock GetBlock(size_t blkIdx);
    uint32_t GetTotal() { return totalBlkNum; }
};

// Контейнер для входящих пакетов
class CSequence: public std::set<size_t> {
    typedef byte tChunk[PAYLOAD_MAXSIZE];
	size_t chunkNum;    // размер контейнера в блоках
	uint32_t total_seq; // счётчик пришедших блоков
	int lastChunkSize;  // последний блок может быть урезан, здесь его размер
	tChunk *chunks; // блоки
public:
	CSequence(size_t _chunkNum): chunkNum(_chunkNum), total_seq(0), lastChunkSize(-1) {
		chunks = new tChunk[_chunkNum];
	}
	void Put(size_t chunkIdx, byte *chunk, size_t size);
    bool IsFull() { return chunkNum==total_seq; }
    uint32_t GetTotal() { return total_seq; }
    uint32_t GetCRC();
};


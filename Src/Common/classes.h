#pragma once
#include <stdint.h>
#include "common.h"
#include <stdlib.h>

#pragma pack(push, 1)
union uFileID {
    byte bytes[8];
    uint32_t ui32[2];
    uint64_t ui64;
};
struct SPackHeader{
    uint32_t seq_number; // номер пакета
    uint32_t seq_total; // количество пакетов с данными
    uint8_t type; // тип пакета: 0 == ACK, 1 == PUT
    uFileID id; // 8 байт - идентификатор, отличающий один файл от другого
    uint16_t pay_size; // payload size
};
#pragma pack(pop)

const int MTU = 1500-28;
const int PAYLOAD_SIZE = MTU - sizeof(SPackHeader);

struct SPacket{
    SPackHeader header;
    byte payload[PAYLOAD_SIZE];
};

class CPacker {
    enum { eREQ, eACK };
public:
    struct SHead {
        uint32_t seq_number; // номер пакета
        uint32_t seq_total; // количество пакетов с данными
        uint8_t type; // тип пакета: 0 == ACK, 1 == PUT
        uFileID id; // 8 байт - идентификатор, отличающий один файл от другого
        uint16_t pay_size; // payload size
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
    byte *Pack(int _type, byte *data, size_t paySize, uint32_t seqNum);
    SHead& Unpack(byte *netPack);
};

class CFragmFile {
    size_t blkSize, totalBlkNum;
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
    size_t GetTotal() { return totalBlkNum; }
};

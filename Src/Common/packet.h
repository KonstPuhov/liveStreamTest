#pragma once
#include <stdint.h>
#include "common.h"


#pragma pack(push, 1)
struct SPackHeader{
    uint32_t seq_number; // номер пакета
    uint32_t seq_total; // количество пакетов с данными
    uint8_t type; // тип пакета: 0 == ACK, 1 == PUT
    uint64_t id; // 8 байт - идентификатор, отличающий один файл от другого
    uint16_t pay_size; // payload size
};
#pragma pack(pop)

const int MTU = 1500-28;
const int PAYLOAD_SIZE = MTU - sizeof(SPackHeader);

class CPacker {
    int64_t id;
    uint32_t total;
    enum { eREQ, eACK };
public:
    CPacker(uint64_t _id, uint32_t _total): id(_id), total(_total) {}
    byte *Pack(byte *data, size_t size, uint32_t seqNum);
};
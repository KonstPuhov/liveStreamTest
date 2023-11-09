#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <exception>
#include <errno.h>
#include <fcntl.h>
#include "classes.h"
#include <arpa/inet.h>
#include <fstream>

byte* CPacker::Pack(int _type, byte *payload, size_t paySize, uint32_t seqNum) {
    static byte buf[MTU];
    SPackHeader *net = (SPackHeader*)buf;

    if(paySize > PAYLOAD_MAXSIZE)
        paySize = PAYLOAD_MAXSIZE;

    net->id.ui32[0] = htonl(h.id.ui32[0]);
    net->id.ui32[1] = htonl(h.id.ui32[1]);
    net->seq_number = htonl(seqNum);
    net->seq_total = htonl(h.seq_total);
    net->type = _type;
    if(payload && paySize)
        memcpy(buf+sizeof(SPackHeader), payload, paySize);

    return buf;
}

CPacker::SHead& CPacker::Unpack(byte *netPack) {
    SPacket *net = (SPacket*)netPack;

    h.id.ui32[0] = ntohl(net->header.id.ui32[0]);
    h.id.ui32[1] = ntohl(net->header.id.ui32[1]);
    h.seq_number = ntohl(net->header.seq_number);
    h.seq_total = ntohl(net->header.seq_total);
    h.type = net->header.type;
    return h;
}

void CSequence::PutBlock(size_t chunkIdx, byte *chunk, size_t size) {
    if(chunkIdx==chunkNum-1)
        lastChunkSize = size;
    if(find(chunkIdx)==end()){
        memcpy(&chunks[chunkIdx], chunk, size);
        insert(chunkIdx);
        total_seq++;
    }
}

uint32_t CSequence::GetCRC() {
    int k;
    uint32_t crc = ~0;
    size_t len = chunkNum>1? (chunkNum-1)*cBlkSize+lastChunkSize : lastChunkSize;
    byte *buf = (byte*)chunks;
    debug("calc CRC of %d bytes...\n", len);
    while (len--) {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
        crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
    }
    debug("CRC = %X\n", ~crc);
    return ~crc;
}

CSequence::CSequence(const char *fName): total_seq(0), lastChunkSize(-1) {
    // Сначала определить размер файла
    struct stat st;
    if(stat(fName, &st) < 0)
        throw (const char*)strerror(errno);
    size_t size = st.st_size;
    if(!size)
        throw "file is empty";

    using namespace std;
    fstream f;
    f.open(fName, fstream::in | fstream::out | fstream::binary);
    if(!f.is_open())
        throw "file open error";
    // Количество блоков, учитывая остаток
    chunkNum = size%cBlkSize ? size/cBlkSize + 1 : size/cBlkSize;
    // Выделить память под блоки
	chunks = new tChunk[chunkNum];

    for(uint32_t i=0; !f.eof(); i++) {
        ::byte buf[cBlkSize];
        f.read((char*)buf, cBlkSize);
        PutBlock(i, buf, f.gcount());
    }
    f.close();
}

CSequence::SBlock CSequence::GetBlock(uint32_t idx) {
    if(find(idx)==end())
        return SBlock{NULL, 0};
    SBlock b;
    b.chunk = chunks[idx];
    b.size = idx==chunkNum-1 ? lastChunkSize : cBlkSize;
    return b;
}

void CSequence::StoreToFile(const char *fileName) {
    using namespace std;
    fstream f;
    f.open(fileName, fstream::out | fstream::binary);
    if(!f.is_open())
        throw "file open error";
    for(uint32_t i=0; i < chunkNum; i++)
        f.write((const char*)chunks[i], i==chunkNum-1 ? lastChunkSize : cBlkSize);
    f.close();
}

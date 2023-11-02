#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <exception>
#include <errno.h>
#include <fcntl.h>
#include "classes.h"
#include <arpa/inet.h>

// Упаковщик исходных данных в формат сетевого пакета 
byte* CPacker::Pack(int _type, byte *payload, size_t paySize, uint32_t seqNum) {
    static byte buf[MTU];
    SPackHeader *net = (SPackHeader*)buf;

    if(paySize > PAYLOAD_SIZE)
        paySize = PAYLOAD_SIZE;

    net->id.ui32[0] = htonl(h.id.ui32[0]);
    net->id.ui32[1] = htonl(h.id.ui32[1]);
    net->seq_number = htonl(seqNum);
    net->seq_total = htonl(h.seq_total);
    net->pay_size = htons(paySize);
    net->type = _type;
    memcpy(buf+sizeof(SPackHeader), payload, paySize);

    return buf;
}
// Распаковывает сетевой пакет
CPacker::SHead& CPacker::Unpack(byte *netPack) {
    SPacket *net = (SPacket*)netPack;

    h.id.ui32[0] = ntohl(net->header.id.ui32[0]);
    h.id.ui32[1] = ntohl(net->header.id.ui32[1]);
    h.seq_number = ntohl(net->header.seq_number);
    h.seq_total = ntohl(net->header.seq_total);
    h.pay_size = ntohs(net->header.pay_size);
    h.type = net->header.type;
    return h;
}

void CFragmFile::Open(const char* fName, size_t blockSz) {
    if(IsOpened())
        throw (char*)"already opened";
    struct stat st;
    if(stat(fName, &st) < 0)
        throw strerror(errno);
    if(fd = open(fName, O_RDONLY); fd < 0)    
        throw strerror(errno);

    totalBlkNum = st.st_size%blockSz ? st.st_size/blockSz + 1 : st.st_size/blockSz;
    blkSize = blockSz;
    fsize = st.st_size;
    popBuf = new uint8_t[blkSize];
}
void CFragmFile::Close() {
    if(!IsOpened())
        throw (char*)"not opened";
    delete []popBuf;
}

 CFragmFile::SBlock CFragmFile::GetBlock(size_t blkIdx) {
    if(!IsOpened())
        throw (char*)"not opened";

    if(lseek(fd, (blkIdx+1)*blkSize, SEEK_SET) < 1)
        throw strerror(errno);

    auto size = blkIdx==totalBlkNum-1?  fsize%blkSize : blkSize;
    if(!size) size = blkSize;

    if(read(fd, popBuf, blkSize) < 0)
        throw strerror(errno);

    return SBlock(popBuf, size);
}
#include "class.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <exception>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

using namespace std;

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
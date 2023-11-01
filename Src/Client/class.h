#pragma once
#include <stdlib.h>
#include <stdint.h>
#include <vector>

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

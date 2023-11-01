#include <string.h>
#include "packet.h"

byte* CPacker::Pack(byte *data, size_t size, uint32_t seqNum) {
    static byte buf[MTU];
    SPackHeader *hdr = (SPackHeader*)buf;

    if(size > PAYLOAD_SIZE)
        size = PAYLOAD_SIZE;

    hdr->id = id;
    hdr->seq_number = seqNum;
    hdr->seq_total = total;
    hdr->pay_size = size;
    hdr->type = eACK;
    memcpy(buf+sizeof hdr, data, size);

    return buf;
}

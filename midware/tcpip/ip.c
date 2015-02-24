/*
    RExOS - embedded RTOS
    Copyright (c) 2011-2015, Alexey Kramarenko
    All rights reserved.
*/

#include "ip.h"
#include "tcpip_private.h"
#include "../../userspace/stdio.h"
#include "icmp.h"
#include <string.h>

#define IP_DF                                   (1 << 6)
#define IP_MF                                   (1 << 5)

#define IP_HEADER_SIZE                          20

#define IP_HEADER_VERSION(buf)                  ((buf)[0] >> 4)
#define IP_HEADER_IHL(buf)                      ((buf)[0] & 0xf)
#define IP_HEADER_TOTAL_LENGTH(buf)             (((buf)[2] << 8) | (buf)[3])
#define IP_HEADER_ID(buf)                       (((buf)[4] << 8) | (buf)[5])
#define IP_HEADER_FLAGS(buf)                    ((buf)[6] & 0xe0)
#define IP_HEADER_FRAME_OFFSET(buf)             ((((buf)[6] & 0x1f) << 8) | (buf[7]))
#define IP_HEADER_TTL(buf)                      ((buf)[8])
#define IP_HEADER_PROTOCOL(buf)                 ((buf)[9])
#define IP_HEADER_CHECKSUM(buf)                 (((buf)[10] << 8) | (buf)[11])
#define IP_HEADER_SRC_IP(buf)                   ((IP*)((buf) + 12))
#define IP_HEADER_DST_IP(buf)                   ((IP*)((buf) + 16))


#if (SYS_INFO) || (TCPIP_DEBUG)
void ip_print(const IP* ip)
{
    int i;
    for (i = 0; i < IP_SIZE; ++i)
    {
        printf("%d", ip->u8[i]);
        if (i < IP_SIZE - 1)
            printf(".");
    }
}
#endif //(SYS_INFO) || (TCPIP_DEBUG)

const IP* tcpip_ip(TCPIP* tcpip)
{
    return &tcpip->ip.ip;
}

void ip_init(TCPIP* tcpip)
{
    tcpip->ip.ip.u32.ip = IP_MAKE(0, 0, 0, 0);
    tcpip->ip.id = 0;
}

#if (SYS_INFO)
static inline void ip_info(TCPIP* tcpip)
{
    printf("IP info\n\r");
    printf("IP: ");
    ip_print(&tcpip->ip.ip);
    printf("\n\r");
}
#endif

static inline void ip_set_request(TCPIP* tcpip, uint32_t ip)
{
    tcpip->ip.ip.u32.ip = ip;
}

static inline void ip_get_request(TCPIP* tcpip, HANDLE process)
{
    ipc_post_inline(process, IP_GET, 0, tcpip->ip.ip.u32.ip, 0);
}

bool ip_request(TCPIP* tcpip, IPC* ipc)
{
    bool need_post = false;
    switch (ipc->cmd)
    {
#if (SYS_INFO)
    case IPC_GET_INFO:
        ip_info(tcpip);
        need_post = true;
        break;
#endif
    case IP_SET:
        ip_set_request(tcpip, ipc->param1);
        need_post = true;
        break;
    case IP_GET:
        ip_get_request(tcpip, ipc->process);
        break;
    default:
        break;
    }
    return need_post;
}

uint8_t* ip_allocate_io(TCPIP* tcpip, TCPIP_IO* io, unsigned int size)
{
    //TODO: fragmented frames
    if (tcpip_allocate_io(tcpip, io) == NULL)
        return NULL;
    //reserve space for IP header
    io->buf += IP_HEADER_SIZE;
    return io->buf;
}

void ip_release_io(TCPIP* tcpip, TCPIP_IO* io)
{
    //TODO: fragmented frames
    tcpip_release_io(tcpip, io);
}

void ip_tx(TCPIP* tcpip, TCPIP_IO* io, const IP* dst, uint8_t proto)
{
    //TODO: fragmented frames
    uint16_t cs;
    io->buf -= IP_HEADER_SIZE;
    io->size += IP_HEADER_SIZE;

    memset(io->buf, 0x00, IP_HEADER_SIZE);
    //VERSION, IHL
    io->buf[0] = 0x45;
    //total len
    io->buf[2] = (io->size >> 8) & 0xff;
    io->buf[3] = io->size & 0xff;
    //id
    io->buf[4] = (tcpip->ip.id >> 8) & 0xff;
    io->buf[5] = tcpip->ip.id & 0xff;
    ++tcpip->ip.id;
    //ttl
    io->buf[8] = 0xff;
    //proto
    io->buf[9] = proto;
    //src
    *(uint32_t*)(io->buf + 12) = tcpip_ip(tcpip)->u32.ip;
    //dst
    *(uint32_t*)(io->buf + 16) = dst->u32.ip;
    //update checksum
    cs = ip_checksum(io->buf, IP_HEADER_SIZE);
    io->buf[10] = (cs >> 8) & 0xff;
    io->buf[11] = cs & 0xff;

    route_tx(tcpip, io, dst);
}

static void ip_process(TCPIP* tcpip, TCPIP_IO* io, IP* src, uint8_t proto)
{
#if (IP_DEBUG_FLOW)
    printf("IP: from ");
    ip_print(src);
    printf(", proto: %d, len: %d\n\r", proto, io->size);
#endif
    switch (proto)
    {
#if (ICMP)
    case PROTO_ICMP:
        icmp_rx(tcpip, io, src);
        break;
#endif //TCPIP_ICMP
    default:
#if (IP_DEBUG)
        printf("IP: unhandled proto %d from", proto);
        ip_print(src);
        printf("\n\r");
#endif
        ip_release_io(tcpip, io);
    }
}

#if (IP_CHECKSUM)
uint16_t ip_checksum(uint8_t* buf, unsigned int size)
{
    unsigned int i;
    uint32_t sum = 0;
    for (i = 0; i < (size >> 1); ++i)
        sum += (buf[i << 1] << 8) | (buf[(i << 1) + 1]);
    sum = ((sum & 0xffff) + (sum >> 16)) & 0xffff;
    return ~((uint16_t)sum);
}
#endif

void ip_rx(TCPIP* tcpip, TCPIP_IO* io)
{
    unsigned int len, hdr_size;
    IP src;
    uint8_t proto;
    if (io->size < IP_HEADER_SIZE)
    {
        tcpip_release_io(tcpip, io);
        return;
    }
    len = IP_HEADER_TOTAL_LENGTH(io->buf);
    hdr_size = IP_HEADER_IHL(io->buf) << 2;
    if ((IP_HEADER_VERSION(io->buf) != 4) || (hdr_size < IP_HEADER_SIZE) || (len < IP_HEADER_SIZE) || (IP_HEADER_TTL(io->buf) == 0))
    {
        tcpip_release_io(tcpip, io);
        return;
    }
    //TODO: if len more than MTU, inform host by ICMP and only than drop packet
    if (len > io->size)
    {
        tcpip_release_io(tcpip, io);
        return;
    }

    //TODO: packet assembly from fragments
//#if (IP_FRAGMENTATION)
//#else
    //drop all fragmented frames
    //TODO: maybe also ICMP?
    if ((IP_HEADER_FLAGS(io->buf) & IP_MF) || (IP_HEADER_FRAME_OFFSET(io->buf)))
    {
        tcpip_release_io(tcpip, io);
        return;
    }
//endif
    //compare destination address
    if (tcpip_ip(tcpip)->u32.ip != IP_HEADER_DST_IP(io->buf)->u32.ip)
    {
        tcpip_release_io(tcpip, io);
        return;
    }
#if (IP_CHECKSUM)
    //drop if checksum is invalid
    if (ip_checksum(io->buf, hdr_size))
    {
        tcpip_release_io(tcpip, io);
        return;
    }
#endif

    src.u32.ip = (IP_HEADER_SRC_IP(io->buf))->u32.ip;
    proto = IP_HEADER_PROTOCOL(io->buf);
    //process non-standart IP header
    if (hdr_size != IP_HEADER_SIZE)
    {
        memmove(io->buf + IP_HEADER_SIZE, io->buf + hdr_size, len - hdr_size);
        len = len - hdr_size + IP_HEADER_SIZE;
    }
    io->buf += IP_HEADER_SIZE;
    io->size = len - IP_HEADER_SIZE;
    ip_process(tcpip, io, &src, proto);
}

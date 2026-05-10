/*
 * MiniZiVPN staged HEV UDPGW legacy support.
 *
 * This header mirrors the small BADVPN UDPGW wire header used by the future
 * HEV UDPGW backend. It is intentionally standalone so the first migration
 * step can compile without routing HEV UDP traffic to UDPGW yet.
 */

#ifndef __HEV_UDPGW_PROTO_H__
#define __HEV_UDPGW_PROTO_H__

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define HEV_UDPGW_CLIENT_FLAG_KEEPALIVE (1 << 0)
#define HEV_UDPGW_CLIENT_FLAG_REBIND (1 << 1)
#define HEV_UDPGW_CLIENT_FLAG_DNS (1 << 2)
#define HEV_UDPGW_CLIENT_FLAG_IPV6 (1 << 3)

#define HEV_UDPGW_IPV4_HEADER_SIZE \
    (sizeof (HevUdpGwHeader) + sizeof (HevUdpGwAddrIpv4))
#define HEV_UDPGW_IPV6_HEADER_SIZE \
    (sizeof (HevUdpGwHeader) + sizeof (HevUdpGwAddrIpv6))

#pragma pack(push, 1)
typedef struct _HevUdpGwHeader
{
    uint8_t flags;
    uint16_t conid;
} HevUdpGwHeader;

typedef struct _HevUdpGwAddrIpv4
{
    uint32_t addr_ip;
    uint16_t addr_port;
} HevUdpGwAddrIpv4;

typedef struct _HevUdpGwAddrIpv6
{
    uint8_t addr_ip[16];
    uint16_t addr_port;
} HevUdpGwAddrIpv6;
#pragma pack(pop)

static inline uint16_t
hev_udpgw_read_u16 (const void *ptr)
{
    uint16_t val;

    memcpy (&val, ptr, sizeof (val));
    return (uint16_t)((val << 8) | (val >> 8));
}

static inline void
hev_udpgw_write_u16 (void *ptr, uint16_t val)
{
    uint8_t *out = ptr;

    out[0] = (uint8_t)(val >> 8);
    out[1] = (uint8_t)(val & 0xff);
}

static inline int
hev_udpgw_encode_ipv4 (uint8_t *out, size_t out_len, uint16_t conid,
                       uint8_t flags, uint32_t dst_ip, uint16_t dst_port,
                       const uint8_t *payload, size_t payload_len)
{
    HevUdpGwHeader *header;
    HevUdpGwAddrIpv4 *addr;
    size_t frame_len = HEV_UDPGW_IPV4_HEADER_SIZE + payload_len;

    if (!out)
        return -1;
    if ((flags & HEV_UDPGW_CLIENT_FLAG_IPV6) != 0)
        return -1;
    if (payload_len && !payload)
        return -1;
    if (frame_len > out_len || frame_len > INT32_MAX)
        return -1;

    header = (HevUdpGwHeader *)out;
    header->flags = flags;
    hev_udpgw_write_u16 (&header->conid, conid);

    addr = (HevUdpGwAddrIpv4 *)(out + sizeof (HevUdpGwHeader));
    memcpy (&addr->addr_ip, &dst_ip, sizeof (addr->addr_ip));
    memcpy (&addr->addr_port, &dst_port, sizeof (addr->addr_port));

    if (payload_len)
        memcpy (out + HEV_UDPGW_IPV4_HEADER_SIZE, payload, payload_len);

    return (int)frame_len;
}

static inline int
hev_udpgw_decode_ipv4 (const uint8_t *frame, size_t frame_len,
                       uint16_t *conid, uint8_t *flags, uint32_t *dst_ip,
                       uint16_t *dst_port, uint8_t *payload,
                       size_t payload_cap, size_t *payload_len)
{
    const HevUdpGwHeader *header;
    const HevUdpGwAddrIpv4 *addr;
    size_t data_len;

    if (!frame || !conid || !flags || !dst_ip || !dst_port || !payload_len)
        return -1;
    if (frame_len < HEV_UDPGW_IPV4_HEADER_SIZE)
        return -1;

    header = (const HevUdpGwHeader *)frame;
    if ((header->flags & HEV_UDPGW_CLIENT_FLAG_IPV6) != 0)
        return -1;

    data_len = frame_len - HEV_UDPGW_IPV4_HEADER_SIZE;
    if (data_len > payload_cap || (data_len && !payload))
        return -1;

    addr = (const HevUdpGwAddrIpv4 *)(frame + sizeof (HevUdpGwHeader));

    *flags = header->flags;
    *conid = hev_udpgw_read_u16 (&header->conid);
    memcpy (dst_ip, &addr->addr_ip, sizeof (addr->addr_ip));
    memcpy (dst_port, &addr->addr_port, sizeof (addr->addr_port));
    *payload_len = data_len;

    if (data_len)
        memcpy (payload, frame + HEV_UDPGW_IPV4_HEADER_SIZE, data_len);

    return (int)data_len;
}

static inline int
hev_udpgw_compute_mtu (int dgram_mtu)
{
    int addr_size = sizeof (HevUdpGwAddrIpv6);

    if (dgram_mtu < 0)
        return -1;
    if (dgram_mtu > INT32_MAX - (int)sizeof (HevUdpGwHeader) - addr_size)
        return -1;

    return sizeof (HevUdpGwHeader) + addr_size + dgram_mtu;
}

#endif /* __HEV_UDPGW_PROTO_H__ */

/*
 * MiniZiVPN staged HEV UDPGW legacy support.
 *
 * This header mirrors the small BADVPN UDPGW wire header used by the future
 * HEV UDPGW backend. It is intentionally standalone so the first migration
 * step can compile without routing HEV UDP traffic to UDPGW yet.
 */

#ifndef __HEV_UDPGW_PROTO_H__
#define __HEV_UDPGW_PROTO_H__

#include <stdint.h>

#define HEV_UDPGW_CLIENT_FLAG_KEEPALIVE (1 << 0)
#define HEV_UDPGW_CLIENT_FLAG_REBIND (1 << 1)
#define HEV_UDPGW_CLIENT_FLAG_DNS (1 << 2)
#define HEV_UDPGW_CLIENT_FLAG_IPV6 (1 << 3)

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

/*
 * MiniZiVPN staged HEV UDPGW legacy support.
 *
 * The real UDPGW session is introduced in a later phase. This phase only
 * exposes compile-time protocol/config seams and keeps runtime routing on
 * existing HEV SOCKS5 UDP or BADVPN fallback paths.
 */

#ifndef __HEV_UDPGW_SESSION_H__
#define __HEV_UDPGW_SESSION_H__

#include <stddef.h>
#include <stdint.h>

#include "hev-config.h"

#define HEV_UDPGW_CONN_MAP_CAPACITY 64

typedef struct _HevUdpGwConn
{
    uint32_t dst_ip;
    uint16_t dst_port;
    uint16_t conid;
    int needs_rebind;
    int in_use;
} HevUdpGwConn;

typedef struct _HevUdpGwConnMap
{
    HevUdpGwConn conns[HEV_UDPGW_CONN_MAP_CAPACITY];
    uint16_t next_conid;
    int size;
    int max_connections;
} HevUdpGwConnMap;

int hev_udpgw_session_is_enabled (void);
HevConfigUdpGw *hev_udpgw_session_get_config (void);

int hev_udpgw_conn_map_init (HevUdpGwConnMap *map, int max_connections);
void hev_udpgw_conn_map_clear (HevUdpGwConnMap *map);
int hev_udpgw_conn_map_size (const HevUdpGwConnMap *map);
HevUdpGwConn *hev_udpgw_conn_map_get_or_create (HevUdpGwConnMap *map,
                                                uint32_t dst_ip,
                                                uint16_t dst_port,
                                                int force_rebind);
void hev_udpgw_conn_mark_sent (HevUdpGwConn *conn);
int hev_udpgw_build_ipv4_frame (HevUdpGwConnMap *map, uint8_t *out,
                                size_t out_len, uint32_t dst_ip,
                                uint16_t dst_port, const uint8_t *payload,
                                size_t payload_len, int is_dns,
                                int force_rebind);

#endif /* __HEV_UDPGW_SESSION_H__ */

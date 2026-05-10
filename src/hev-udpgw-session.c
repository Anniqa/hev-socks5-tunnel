/*
 * MiniZiVPN staged HEV UDPGW legacy support.
 *
 * Runtime packet handling is intentionally not wired in this phase. The file
 * compiles into libhev-socks5-tunnel so later phases can add UDPGW session
 * creation without disturbing the existing SOCKS5 UDP implementation.
 */

#include <string.h>

#include "hev-config.h"
#include "hev-udpgw-proto.h"
#include "hev-udpgw-session.h"

int
hev_udpgw_session_is_enabled (void)
{
    HevConfigUdpGw *cfg = hev_config_get_udpgw_server ();

    return cfg && cfg->enabled;
}

HevConfigUdpGw *
hev_udpgw_session_get_config (void)
{
    return hev_config_get_udpgw_server ();
}

int
hev_udpgw_conn_map_init (HevUdpGwConnMap *map, int max_connections)
{
    if (!map)
        return -1;

    if (max_connections <= 0 || max_connections > HEV_UDPGW_CONN_MAP_CAPACITY)
        max_connections = HEV_UDPGW_CONN_MAP_CAPACITY;

    memset (map, 0, sizeof (*map));
    map->next_conid = 1;
    map->max_connections = max_connections;

    return 0;
}

void
hev_udpgw_conn_map_clear (HevUdpGwConnMap *map)
{
    if (!map)
        return;

    memset (map->conns, 0, sizeof (map->conns));
    map->size = 0;
    map->next_conid = 1;
}

int
hev_udpgw_conn_map_size (const HevUdpGwConnMap *map)
{
    if (!map)
        return 0;

    return map->size;
}

static HevUdpGwConn *
hev_udpgw_conn_map_find (HevUdpGwConnMap *map, uint32_t dst_ip,
                         uint16_t dst_port)
{
    int i;

    for (i = 0; i < HEV_UDPGW_CONN_MAP_CAPACITY; i++) {
        HevUdpGwConn *conn = &map->conns[i];

        if (!conn->in_use)
            continue;
        if (conn->dst_ip == dst_ip && conn->dst_port == dst_port)
            return conn;
    }

    return NULL;
}

static uint16_t
hev_udpgw_conn_map_alloc_conid (HevUdpGwConnMap *map)
{
    uint16_t conid = map->next_conid;

    map->next_conid++;
    if (map->next_conid == 0)
        map->next_conid = 1;

    return conid;
}

HevUdpGwConn *
hev_udpgw_conn_map_get_or_create (HevUdpGwConnMap *map, uint32_t dst_ip,
                                  uint16_t dst_port, int force_rebind)
{
    HevUdpGwConn *conn;
    int i;

    if (!map)
        return NULL;

    conn = hev_udpgw_conn_map_find (map, dst_ip, dst_port);
    if (conn) {
        if (force_rebind)
            conn->needs_rebind = 1;
        return conn;
    }

    if (map->size >= map->max_connections)
        return NULL;

    for (i = 0; i < HEV_UDPGW_CONN_MAP_CAPACITY; i++) {
        conn = &map->conns[i];
        if (conn->in_use)
            continue;

        memset (conn, 0, sizeof (*conn));
        conn->dst_ip = dst_ip;
        conn->dst_port = dst_port;
        conn->conid = hev_udpgw_conn_map_alloc_conid (map);
        conn->needs_rebind = force_rebind ? 1 : 0;
        conn->in_use = 1;
        map->size++;
        return conn;
    }

    return NULL;
}

void
hev_udpgw_conn_mark_sent (HevUdpGwConn *conn)
{
    if (!conn)
        return;

    conn->needs_rebind = 0;
}

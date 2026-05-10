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

int
hev_udpgw_session_should_handle_udp (void)
{
    HevConfigUdpGw *cfg = hev_config_get_udpgw_server ();

    return cfg && cfg->enabled && cfg->addr[0] && cfg->port;
}

HevConfigUdpGw *
hev_udpgw_session_get_config (void)
{
    return hev_config_get_udpgw_server ();
}

int
hev_udpgw_endpoint_from_config (const HevConfigUdpGw *config,
                                HevUdpGwEndpoint *endpoint)
{
    if (!config || !endpoint || !config->enabled || !config->addr[0] ||
        !config->port)
        return -1;

    memset (endpoint, 0, sizeof (*endpoint));
    strncpy (endpoint->addr, config->addr, sizeof (endpoint->addr) - 1);
    endpoint->port = config->port;

    return 0;
}

int
hev_udpgw_inbound_from_frame (const uint8_t *frame, size_t frame_len,
                              HevUdpGwInbound *inbound)
{
    const HevUdpGwHeader *header;
    const HevUdpGwAddrIpv4 *addr;

    if (!frame || !inbound || frame_len < HEV_UDPGW_IPV4_HEADER_SIZE)
        return -1;

    header = (const HevUdpGwHeader *)frame;
    if ((header->flags & HEV_UDPGW_CLIENT_FLAG_IPV6) != 0)
        return -1;

    addr = (const HevUdpGwAddrIpv4 *)(frame + sizeof (HevUdpGwHeader));

    memset (inbound, 0, sizeof (*inbound));
    inbound->conid = hev_udpgw_read_u16 (&header->conid);
    inbound->flags = header->flags;
    memcpy (&inbound->src_ip, &addr->addr_ip, sizeof (inbound->src_ip));
    memcpy (&inbound->src_port, &addr->addr_port, sizeof (inbound->src_port));
    inbound->payload = frame + HEV_UDPGW_IPV4_HEADER_SIZE;
    inbound->payload_len = frame_len - HEV_UDPGW_IPV4_HEADER_SIZE;

    return 0;
}

int
hev_udpgw_dispatch_inbound_frame (const uint8_t *frame, size_t frame_len,
                                  HevUdpGwReplyFunc reply_func,
                                  void *user_data)
{
    HevUdpGwInbound inbound;
    int res;

    if (!reply_func)
        return -1;

    res = hev_udpgw_inbound_from_frame (frame, frame_len, &inbound);
    if (res < 0)
        return res;

    return reply_func (inbound.src_ip, inbound.src_port, inbound.payload,
                       inbound.payload_len, user_data);
}

int
hev_udpgw_lwip_reply_init (HevUdpGwLwipReply *reply,
                           const HevUdpGwLwipReplyOps *ops, void *user_data)
{
    if (!reply || !ops || !ops->send)
        return -1;

    memset (reply, 0, sizeof (*reply));
    reply->ops = *ops;
    reply->user_data = user_data;

    return 0;
}

int
hev_udpgw_lwip_reply_dispatch (uint32_t src_ip, uint16_t src_port,
                               const uint8_t *payload, size_t payload_len,
                               void *user_data)
{
    HevUdpGwLwipReply *reply = user_data;
    int res;

    if (!reply || !reply->ops.send)
        return -1;

    if (reply->ops.lock)
        reply->ops.lock (reply->user_data);

    res = reply->ops.send (src_ip, src_port, payload, payload_len,
                           reply->user_data);

    if (reply->ops.unlock)
        reply->ops.unlock (reply->user_data);

    return res;
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

int
hev_udpgw_build_ipv4_frame (HevUdpGwConnMap *map, uint8_t *out,
                            size_t out_len, uint32_t dst_ip,
                            uint16_t dst_port, const uint8_t *payload,
                            size_t payload_len, int is_dns, int force_rebind)
{
    HevUdpGwConn *conn;
    uint8_t flags = 0;
    int was_size;
    int res;

    if (!map)
        return -1;

    was_size = map->size;
    conn = hev_udpgw_conn_map_get_or_create (map, dst_ip, dst_port,
                                             force_rebind);
    if (!conn)
        return -1;

    if (is_dns)
        flags |= HEV_UDPGW_CLIENT_FLAG_DNS;
    if (conn->needs_rebind || map->size > was_size)
        flags |= HEV_UDPGW_CLIENT_FLAG_REBIND;

    res = hev_udpgw_encode_ipv4 (out, out_len, conn->conid, flags, dst_ip,
                                 dst_port, payload, payload_len);
    if (res >= 0)
        hev_udpgw_conn_mark_sent (conn);

    return res;
}

int
hev_udpgw_sender_init (HevUdpGwSender *sender, int max_connections,
                       HevUdpGwSendFunc send_func, void *user_data)
{
    if (!sender || !send_func)
        return -1;

    memset (sender, 0, sizeof (*sender));
    if (hev_udpgw_conn_map_init (&sender->conn_map, max_connections) < 0)
        return -1;

    sender->send_func = send_func;
    sender->user_data = user_data;
    sender->open = 1;

    return 0;
}

void
hev_udpgw_sender_close (HevUdpGwSender *sender)
{
    if (!sender)
        return;

    hev_udpgw_conn_map_clear (&sender->conn_map);
    sender->open = 0;
}

int
hev_udpgw_sender_is_open (const HevUdpGwSender *sender)
{
    return sender && sender->open;
}

int
hev_udpgw_sender_conn_count (const HevUdpGwSender *sender)
{
    if (!sender)
        return 0;

    return hev_udpgw_conn_map_size (&sender->conn_map);
}

int
hev_udpgw_sender_send_ipv4 (HevUdpGwSender *sender, uint32_t dst_ip,
                            uint16_t dst_port, const uint8_t *payload,
                            size_t payload_len, int is_dns, int force_rebind)
{
    int frame_len;
    int sent;

    if (!sender || !sender->open || !sender->send_func)
        return -1;

    frame_len = hev_udpgw_build_ipv4_frame (
        &sender->conn_map, sender->frame_buf, sizeof (sender->frame_buf), dst_ip,
        dst_port, payload, payload_len, is_dns, force_rebind);
    if (frame_len < 0)
        return frame_len;

    sent = sender->send_func (sender->frame_buf, (size_t)frame_len,
                              sender->user_data);
    if (sent < 0)
        return sent;

    return frame_len;
}

int
hev_udpgw_transport_init (HevUdpGwTransport *transport,
                          const HevUdpGwTransportOps *ops, void *user_data)
{
    int handle;

    if (!transport || !ops || !ops->open || !ops->send)
        return -1;

    memset (transport, 0, sizeof (*transport));
    handle = ops->open (user_data);
    if (handle < 0)
        return handle;

    transport->ops = *ops;
    transport->user_data = user_data;
    transport->handle = handle;
    transport->open = 1;

    return 0;
}

void
hev_udpgw_transport_close (HevUdpGwTransport *transport)
{
    if (!transport || !transport->open)
        return;

    if (transport->ops.close)
        transport->ops.close (transport->handle, transport->user_data);
    transport->open = 0;
    transport->handle = -1;
}

int
hev_udpgw_transport_is_open (const HevUdpGwTransport *transport)
{
    return transport && transport->open;
}

int
hev_udpgw_transport_send (const uint8_t *frame, size_t frame_len,
                          void *user_data)
{
    HevUdpGwTransport *transport = user_data;

    if (!transport || !transport->open || !transport->ops.send)
        return -1;

    return transport->ops.send (transport->handle, frame, frame_len,
                                transport->user_data);
}

int
hev_udpgw_session_init (HevUdpGwSession *session,
                        const HevUdpGwSessionConfig *config,
                        const HevUdpGwTransportOps *ops, void *user_data)
{
    HevUdpGwSessionConfig cfg;

    if (!session || !ops)
        return -1;

    memset (session, 0, sizeof (*session));
    if (config)
        cfg = *config;
    else
        memset (&cfg, 0, sizeof (cfg));

    if (cfg.max_connections <= 0)
        cfg.max_connections = HEV_UDPGW_CONN_MAP_CAPACITY;

    if (hev_udpgw_transport_init (&session->transport, ops, user_data) < 0)
        return -1;
    if (hev_udpgw_sender_init (&session->sender, cfg.max_connections,
                               hev_udpgw_transport_send,
                               &session->transport) < 0) {
        hev_udpgw_transport_close (&session->transport);
        return -1;
    }

    session->config = cfg;
    session->open = 1;

    return 0;
}

void
hev_udpgw_session_close (HevUdpGwSession *session)
{
    if (!session || !session->open)
        return;

    hev_udpgw_sender_close (&session->sender);
    hev_udpgw_transport_close (&session->transport);
    session->open = 0;
}

int
hev_udpgw_session_is_open (const HevUdpGwSession *session)
{
    return session && session->open;
}

int
hev_udpgw_session_conn_count (const HevUdpGwSession *session)
{
    if (!session)
        return 0;

    return hev_udpgw_sender_conn_count (&session->sender);
}

int
hev_udpgw_session_send_ipv4 (HevUdpGwSession *session, uint32_t dst_ip,
                             uint16_t dst_port, const uint8_t *payload,
                             size_t payload_len, int is_dns, int force_rebind)
{
    if (!session || !session->open)
        return -1;

    return hev_udpgw_sender_send_ipv4 (&session->sender, dst_ip, dst_port,
                                       payload, payload_len, is_dns,
                                       force_rebind);
}

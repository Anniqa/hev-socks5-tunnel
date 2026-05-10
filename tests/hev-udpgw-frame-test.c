#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hev-udpgw-proto.h"
#include "hev-udpgw-session.h"

static int
expect_int (const char *label, int actual, int expected)
{
    if (actual != expected) {
        fprintf (stderr, "%s: expected %d, got %d\n", label, expected, actual);
        return 1;
    }
    return 0;
}

static int
expect_bytes (const char *label, const uint8_t *actual, const uint8_t *expected,
              size_t len)
{
    if (memcmp (actual, expected, len) != 0) {
        fprintf (stderr, "%s: bytes differ\n", label);
        return 1;
    }
    return 0;
}

static int
decode_and_expect (const uint8_t *frame, int frame_len, uint16_t want_conid,
                   uint8_t want_flags, uint32_t want_ip, uint16_t want_port,
                   const uint8_t *want_payload, size_t want_payload_len)
{
    uint16_t conid = 0;
    uint8_t flags = 0;
    uint32_t ip = 0;
    uint16_t port = 0;
    uint8_t payload[64];
    size_t payload_len = 0;

    int decoded = hev_udpgw_decode_ipv4 (frame, frame_len, &conid, &flags, &ip,
                                         &port, payload, sizeof (payload),
                                         &payload_len);
    if (expect_int ("decoded length", decoded, (int)want_payload_len))
        return 1;
    if (expect_int ("conid", conid, want_conid))
        return 1;
    if (expect_int ("flags", flags, want_flags))
        return 1;
    if (ip != want_ip || port != want_port) {
        fprintf (stderr, "address differs\n");
        return 1;
    }
    if (expect_int ("payload length", (int)payload_len, (int)want_payload_len))
        return 1;
    if (expect_bytes ("payload", payload, want_payload, want_payload_len))
        return 1;

    return 0;
}

int
main (void)
{
    HevUdpGwConnMap map;
    uint8_t frame[64];
    const uint8_t payload[] = { 0xca, 0xfe, 0xba, 0xbe };
    uint32_t ip = inet_addr ("1.2.3.4");
    uint16_t port = htons (53);
    int frame_len;
    HevUdpGwConn *conn;

    if (expect_int ("init", hev_udpgw_conn_map_init (&map, 8), 0))
        return 1;

    frame_len = hev_udpgw_build_ipv4_frame (&map, frame, sizeof (frame), ip,
                                            port, payload, sizeof (payload), 1,
                                            0);
    if (expect_int ("first frame length", frame_len,
                    (int)(HEV_UDPGW_IPV4_HEADER_SIZE + sizeof (payload))))
        return 1;
    if (decode_and_expect (frame, frame_len, 1,
                           HEV_UDPGW_CLIENT_FLAG_REBIND |
                               HEV_UDPGW_CLIENT_FLAG_DNS,
                           ip, port, payload, sizeof (payload)))
        return 1;

    conn = hev_udpgw_conn_map_get_or_create (&map, ip, port, 0);
    if (!conn) {
        fprintf (stderr, "connection should exist\n");
        return 1;
    }
    if (expect_int ("first send clears rebind", conn->needs_rebind, 0))
        return 1;

    frame_len = hev_udpgw_build_ipv4_frame (&map, frame, sizeof (frame), ip,
                                            port, payload, sizeof (payload), 0,
                                            0);
    if (decode_and_expect (frame, frame_len, 1, 0, ip, port, payload,
                           sizeof (payload)))
        return 1;

    frame_len = hev_udpgw_build_ipv4_frame (&map, frame, sizeof (frame), ip,
                                            port, payload, sizeof (payload), 0,
                                            1);
    if (decode_and_expect (frame, frame_len, 1, HEV_UDPGW_CLIENT_FLAG_REBIND,
                           ip, port, payload, sizeof (payload)))
        return 1;

    frame_len = hev_udpgw_build_ipv4_frame (&map, frame, 8, ip, port, payload,
                                            sizeof (payload), 0, 0);
    if (frame_len >= 0) {
        fprintf (stderr, "short frame buffer should fail\n");
        return 1;
    }

    return 0;
}

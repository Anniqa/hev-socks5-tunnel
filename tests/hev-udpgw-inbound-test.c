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
expect_mem (const char *label, const uint8_t *actual, const uint8_t *expected,
            size_t len)
{
    if (memcmp (actual, expected, len) != 0) {
        fprintf (stderr, "%s: payload mismatch\n", label);
        return 1;
    }
    return 0;
}

int
main (void)
{
    uint8_t payload[] = { 0xde, 0xad, 0xbe, 0xef };
    uint8_t frame[128];
    HevUdpGwInbound inbound;
    uint32_t ip = inet_addr ("203.0.113.9");
    uint16_t port = htons (5353);
    int frame_len;

    frame_len = hev_udpgw_encode_ipv4 (frame, sizeof (frame), 7,
                                       HEV_UDPGW_CLIENT_FLAG_DNS, ip, port,
                                       payload, sizeof (payload));
    if (frame_len <= 0)
        return 1;

    if (expect_int ("null frame", hev_udpgw_inbound_from_frame (NULL, frame_len, &inbound), -1))
        return 1;
    if (expect_int ("null inbound", hev_udpgw_inbound_from_frame (frame, frame_len, NULL), -1))
        return 1;
    if (expect_int ("short frame", hev_udpgw_inbound_from_frame (frame, HEV_UDPGW_IPV4_HEADER_SIZE - 1, &inbound), -1))
        return 1;

    if (expect_int ("valid inbound", hev_udpgw_inbound_from_frame (frame, frame_len, &inbound), 0))
        return 1;
    if (expect_int ("conid", inbound.conid, 7))
        return 1;
    if (expect_int ("flags", inbound.flags, HEV_UDPGW_CLIENT_FLAG_DNS))
        return 1;
    if (expect_int ("ip", inbound.src_ip, ip))
        return 1;
    if (expect_int ("port", inbound.src_port, port))
        return 1;
    if (expect_int ("payload len", inbound.payload_len, sizeof (payload)))
        return 1;
    if (expect_mem ("payload", inbound.payload, payload, sizeof (payload)))
        return 1;

    frame[0] = HEV_UDPGW_CLIENT_FLAG_IPV6;
    if (expect_int ("reject ipv6", hev_udpgw_inbound_from_frame (frame, frame_len, &inbound), -1))
        return 1;

    return 0;
}

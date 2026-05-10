#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hev-udpgw-proto.h"

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

int
main (void)
{
    const uint8_t payload[] = { 0xde, 0xad, 0xbe, 0xef };
    uint8_t frame[64];
    uint8_t decoded[64];
    uint32_t dst_ip = inet_addr ("1.2.3.4");
    uint16_t dst_port = htons (53);
    uint16_t conid = 0x1234;
    uint32_t out_ip = 0;
    uint16_t out_port = 0;
    uint16_t out_conid = 0;
    uint8_t out_flags = 0;
    size_t out_payload_len = 0;

    int encoded = hev_udpgw_encode_ipv4 (
        frame, sizeof (frame), conid, HEV_UDPGW_CLIENT_FLAG_DNS, dst_ip,
        dst_port, payload, sizeof (payload));
    if (expect_int ("encoded length", encoded, 13))
        return 1;

    const uint8_t expected[] = {
        0x04, 0x12, 0x34, 0x01, 0x02, 0x03, 0x04,
        0x00, 0x35, 0xde, 0xad, 0xbe, 0xef,
    };
    if (expect_bytes ("encoded frame", frame, expected, sizeof (expected)))
        return 1;

    int decoded_len = hev_udpgw_decode_ipv4 (
        frame, encoded, &out_conid, &out_flags, &out_ip, &out_port, decoded,
        sizeof (decoded), &out_payload_len);
    if (expect_int ("decoded return", decoded_len, 4))
        return 1;
    if (expect_int ("decoded payload len", (int)out_payload_len, 4))
        return 1;
    if (expect_int ("decoded conid", out_conid, conid))
        return 1;
    if (expect_int ("decoded flags", out_flags, HEV_UDPGW_CLIENT_FLAG_DNS))
        return 1;
    if (out_ip != dst_ip || out_port != dst_port) {
        fprintf (stderr, "decoded address differs\n");
        return 1;
    }
    if (expect_bytes ("decoded payload", decoded, payload, sizeof (payload)))
        return 1;

    if (hev_udpgw_encode_ipv4 (frame, 12, conid, 0, dst_ip, dst_port, payload,
                               sizeof (payload)) >= 0) {
        fprintf (stderr, "short encode buffer should fail\n");
        return 1;
    }
    if (hev_udpgw_decode_ipv4 (frame, 8, &out_conid, &out_flags, &out_ip,
                               &out_port, decoded, sizeof (decoded),
                               &out_payload_len) >= 0) {
        fprintf (stderr, "short decode frame should fail\n");
        return 1;
    }
    if (hev_udpgw_encode_ipv4 (frame, sizeof (frame), conid,
                               HEV_UDPGW_CLIENT_FLAG_IPV6, dst_ip, dst_port,
                               payload, sizeof (payload)) >= 0) {
        fprintf (stderr, "IPv6 flag should be rejected by IPv4 encoder\n");
        return 1;
    }

    return 0;
}

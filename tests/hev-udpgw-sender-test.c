#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hev-udpgw-proto.h"
#include "hev-udpgw-session.h"

static uint8_t captured_frame[128];
static size_t captured_len;
static int send_calls;

static int
capture_send (const uint8_t *frame, size_t frame_len, void *user_data)
{
    int *ret = user_data;

    send_calls++;
    if (frame_len > sizeof (captured_frame))
        return -1;
    memcpy (captured_frame, frame, frame_len);
    captured_len = frame_len;

    return ret ? *ret : (int)frame_len;
}

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
decode_and_expect (uint16_t want_conid, uint8_t want_flags, uint32_t want_ip,
                   uint16_t want_port, const uint8_t *want_payload,
                   size_t want_payload_len)
{
    uint16_t conid = 0;
    uint8_t flags = 0;
    uint32_t ip = 0;
    uint16_t port = 0;
    uint8_t payload[64];
    size_t payload_len = 0;

    int decoded = hev_udpgw_decode_ipv4 (
        captured_frame, captured_len, &conid, &flags, &ip, &port, payload,
        sizeof (payload), &payload_len);
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
    if (memcmp (payload, want_payload, want_payload_len) != 0) {
        fprintf (stderr, "payload differs\n");
        return 1;
    }

    return 0;
}

int
main (void)
{
    HevUdpGwSender sender;
    const uint8_t payload[] = { 0xaa, 0xbb, 0xcc };
    uint32_t ip = inet_addr ("9.9.9.9");
    uint16_t port = htons (53);
    int ret = 0;
    int sent;

    if (expect_int ("init", hev_udpgw_sender_init (&sender, 8, capture_send,
                                                   &ret), 0))
        return 1;
    if (expect_int ("initial open", hev_udpgw_sender_is_open (&sender), 1))
        return 1;

    sent = hev_udpgw_sender_send_ipv4 (&sender, ip, port, payload,
                                       sizeof (payload), 1, 0);
    if (expect_int ("sent length", sent,
                    (int)(HEV_UDPGW_IPV4_HEADER_SIZE + sizeof (payload))))
        return 1;
    if (expect_int ("send calls", send_calls, 1))
        return 1;
    if (decode_and_expect (1,
                           HEV_UDPGW_CLIENT_FLAG_REBIND |
                               HEV_UDPGW_CLIENT_FLAG_DNS,
                           ip, port, payload, sizeof (payload)))
        return 1;
    if (expect_int ("sender map size", hev_udpgw_sender_conn_count (&sender), 1))
        return 1;

    ret = -7;
    sent = hev_udpgw_sender_send_ipv4 (&sender, ip, port, payload,
                                       sizeof (payload), 0, 1);
    if (expect_int ("transport error propagates", sent, -7))
        return 1;
    if (expect_int ("error still called transport", send_calls, 2))
        return 1;

    hev_udpgw_sender_close (&sender);
    if (expect_int ("closed", hev_udpgw_sender_is_open (&sender), 0))
        return 1;
    ret = 0;
    sent = hev_udpgw_sender_send_ipv4 (&sender, ip, port, payload,
                                       sizeof (payload), 0, 0);
    if (sent >= 0) {
        fprintf (stderr, "closed sender should reject sends\n");
        return 1;
    }
    if (expect_int ("closed send should not call transport", send_calls, 2))
        return 1;

    return 0;
}

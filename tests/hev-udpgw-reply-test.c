#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hev-udpgw-proto.h"
#include "hev-udpgw-session.h"

typedef struct _ReplyState
{
    int calls;
    uint32_t src_ip;
    uint16_t src_port;
    uint8_t payload[64];
    size_t payload_len;
} ReplyState;

static int
reply_cb (uint32_t src_ip, uint16_t src_port, const uint8_t *payload,
          size_t payload_len, void *user_data)
{
    ReplyState *state = user_data;

    state->calls++;
    state->src_ip = src_ip;
    state->src_port = src_port;
    state->payload_len = payload_len;
    if (payload_len)
        memcpy (state->payload, payload, payload_len);

    return 0;
}

static int
failing_reply_cb (uint32_t src_ip, uint16_t src_port, const uint8_t *payload,
                  size_t payload_len, void *user_data)
{
    (void)src_ip;
    (void)src_port;
    (void)payload;
    (void)payload_len;
    (void)user_data;

    return -5;
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
    uint8_t payload[] = { 1, 2, 3, 4, 5 };
    uint8_t frame[128];
    ReplyState state;
    uint32_t ip = inet_addr ("198.51.100.10");
    uint16_t port = htons (5300);
    int frame_len;

    memset (&state, 0, sizeof (state));
    frame_len = hev_udpgw_encode_ipv4 (frame, sizeof (frame), 9, 0, ip, port,
                                       payload, sizeof (payload));
    if (frame_len <= 0)
        return 1;

    if (expect_int ("null frame", hev_udpgw_dispatch_inbound_frame (NULL, frame_len, reply_cb, &state), -1))
        return 1;
    if (expect_int ("null cb", hev_udpgw_dispatch_inbound_frame (frame, frame_len, NULL, &state), -1))
        return 1;

    if (expect_int ("valid dispatch", hev_udpgw_dispatch_inbound_frame (frame, frame_len, reply_cb, &state), 0))
        return 1;
    if (expect_int ("calls", state.calls, 1))
        return 1;
    if (expect_int ("src ip", state.src_ip, ip))
        return 1;
    if (expect_int ("src port", state.src_port, port))
        return 1;
    if (expect_int ("payload len", state.payload_len, sizeof (payload)))
        return 1;
    if (expect_mem ("payload", state.payload, payload, sizeof (payload)))
        return 1;

    if (expect_int ("callback failure", hev_udpgw_dispatch_inbound_frame (frame, frame_len, failing_reply_cb, &state), -5))
        return 1;

    frame[0] = HEV_UDPGW_CLIENT_FLAG_IPV6;
    if (expect_int ("bad frame", hev_udpgw_dispatch_inbound_frame (frame, frame_len, reply_cb, &state), -1))
        return 1;

    return 0;
}

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hev-udpgw-session.h"

typedef struct _FakeLwip
{
    int lock_calls;
    int unlock_calls;
    int send_calls;
    int send_result;
    uint32_t src_ip;
    uint16_t src_port;
    uint8_t payload[64];
    size_t payload_len;
} FakeLwip;

static void
lock_cb (void *user_data)
{
    FakeLwip *state = user_data;

    state->lock_calls++;
}

static void
unlock_cb (void *user_data)
{
    FakeLwip *state = user_data;

    state->unlock_calls++;
}

static int
send_cb (uint32_t src_ip, uint16_t src_port, const uint8_t *payload,
         size_t payload_len, void *user_data)
{
    FakeLwip *state = user_data;

    state->send_calls++;
    state->src_ip = src_ip;
    state->src_port = src_port;
    state->payload_len = payload_len;
    if (payload_len)
        memcpy (state->payload, payload, payload_len);

    return state->send_result;
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
    HevUdpGwLwipReply reply;
    HevUdpGwLwipReplyOps ops = {
        .lock = lock_cb,
        .send = send_cb,
        .unlock = unlock_cb,
    };
    FakeLwip state;
    uint8_t payload[] = { 9, 8, 7 };

    memset (&state, 0, sizeof (state));
    state.send_result = 0;

    if (expect_int ("null reply", hev_udpgw_lwip_reply_init (NULL, &ops, &state), -1))
        return 1;
    if (expect_int ("null ops", hev_udpgw_lwip_reply_init (&reply, NULL, &state), -1))
        return 1;

    ops.send = NULL;
    if (expect_int ("missing send", hev_udpgw_lwip_reply_init (&reply, &ops, &state), -1))
        return 1;
    ops.send = send_cb;

    if (expect_int ("init", hev_udpgw_lwip_reply_init (&reply, &ops, &state), 0))
        return 1;
    if (expect_int ("dispatch", hev_udpgw_lwip_reply_dispatch (0x01020304, 5353, payload, sizeof (payload), &reply), 0))
        return 1;
    if (expect_int ("lock calls", state.lock_calls, 1))
        return 1;
    if (expect_int ("unlock calls", state.unlock_calls, 1))
        return 1;
    if (expect_int ("send calls", state.send_calls, 1))
        return 1;
    if (expect_int ("src ip", state.src_ip, 0x01020304))
        return 1;
    if (expect_int ("src port", state.src_port, 5353))
        return 1;
    if (expect_int ("payload len", state.payload_len, sizeof (payload)))
        return 1;
    if (expect_mem ("payload", state.payload, payload, sizeof (payload)))
        return 1;

    state.send_result = -9;
    if (expect_int ("send failure", hev_udpgw_lwip_reply_dispatch (0x01020304, 5353, payload, sizeof (payload), &reply), -9))
        return 1;
    if (expect_int ("unlock on failure", state.unlock_calls, 2))
        return 1;

    if (expect_int ("null user_data", hev_udpgw_lwip_reply_dispatch (0x01020304, 5353, payload, sizeof (payload), NULL), -1))
        return 1;

    return 0;
}

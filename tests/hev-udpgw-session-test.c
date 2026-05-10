#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hev-udpgw-proto.h"
#include "hev-udpgw-session.h"

typedef struct _FakeOpsState
{
    int open_calls;
    int send_calls;
    int close_calls;
    int next_handle;
    int last_handle;
    uint8_t last_frame[128];
    size_t last_len;
} FakeOpsState;

static int
fake_open (void *user_data)
{
    FakeOpsState *state = user_data;
    state->open_calls++;
    state->next_handle++;
    return state->next_handle;
}

static int
fake_send (int handle, const uint8_t *frame, size_t frame_len, void *user_data)
{
    FakeOpsState *state = user_data;

    state->send_calls++;
    state->last_handle = handle;
    state->last_len = frame_len;
    if (frame_len > sizeof (state->last_frame))
        return -1;
    memcpy (state->last_frame, frame, frame_len);

    return (int)frame_len;
}

static void
fake_close (int handle, void *user_data)
{
    FakeOpsState *state = user_data;

    state->close_calls++;
    state->last_handle = handle;
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

int
main (void)
{
    HevUdpGwSession session;
    HevUdpGwSessionConfig cfg = {
        .max_connections = 8,
        .transparent_dns = 1,
    };
    HevUdpGwTransportOps ops = {
        .open = fake_open,
        .send = fake_send,
        .close = fake_close,
    };
    FakeOpsState state = { 0 };
    const uint8_t payload[] = { 0xde, 0xad };
    uint32_t ip = inet_addr ("8.8.8.8");
    uint16_t port = htons (53);
    uint16_t conid = 0;
    uint8_t flags = 0;
    uint32_t out_ip = 0;
    uint16_t out_port = 0;
    uint8_t out_payload[8];
    size_t out_payload_len = 0;
    int sent;

    if (expect_int ("init", hev_udpgw_session_init (&session, &cfg, &ops,
                                                    &state), 0))
        return 1;
    if (expect_int ("session open", hev_udpgw_session_is_open (&session), 1))
        return 1;
    if (expect_int ("transport open calls", state.open_calls, 1))
        return 1;

    sent = hev_udpgw_session_send_ipv4 (&session, ip, port, payload,
                                        sizeof (payload), 1, 0);
    if (expect_int ("sent length", sent,
                    (int)(HEV_UDPGW_IPV4_HEADER_SIZE + sizeof (payload))))
        return 1;
    if (expect_int ("send calls", state.send_calls, 1))
        return 1;
    if (expect_int ("conn count", hev_udpgw_session_conn_count (&session), 1))
        return 1;

    if (hev_udpgw_decode_ipv4 (state.last_frame, state.last_len, &conid, &flags,
                               &out_ip, &out_port, out_payload,
                               sizeof (out_payload), &out_payload_len) < 0) {
        fprintf (stderr, "decode failed\n");
        return 1;
    }
    if (expect_int ("conid", conid, 1))
        return 1;
    if (expect_int ("flags", flags,
                    HEV_UDPGW_CLIENT_FLAG_REBIND |
                        HEV_UDPGW_CLIENT_FLAG_DNS))
        return 1;
    if (out_ip != ip || out_port != port || out_payload_len != sizeof (payload) ||
        memcmp (out_payload, payload, sizeof (payload)) != 0) {
        fprintf (stderr, "decoded frame differs\n");
        return 1;
    }

    hev_udpgw_session_close (&session);
    if (expect_int ("close calls", state.close_calls, 1))
        return 1;
    if (expect_int ("session closed", hev_udpgw_session_is_open (&session), 0))
        return 1;
    if (hev_udpgw_session_send_ipv4 (&session, ip, port, payload,
                                     sizeof (payload), 0, 0) >= 0) {
        fprintf (stderr, "closed session should reject send\n");
        return 1;
    }

    return 0;
}

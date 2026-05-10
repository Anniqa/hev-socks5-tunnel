#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hev-udpgw-session.h"

typedef struct _FakeOpsState
{
    int open_calls;
    int send_calls;
    int close_calls;
    int next_handle;
    int send_ret;
    int last_handle;
    size_t last_len;
    uint8_t last_frame[64];
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

    return state->send_ret ? state->send_ret : (int)frame_len;
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
    HevUdpGwTransport transport;
    HevUdpGwTransportOps ops = {
        .open = fake_open,
        .send = fake_send,
        .close = fake_close,
    };
    FakeOpsState state = { .send_ret = 0 };
    const uint8_t frame[] = { 1, 2, 3, 4, 5 };
    int sent;

    if (expect_int ("init", hev_udpgw_transport_init (&transport, &ops, &state),
                    0))
        return 1;
    if (expect_int ("open calls", state.open_calls, 1))
        return 1;
    if (expect_int ("transport open", hev_udpgw_transport_is_open (&transport),
                    1))
        return 1;

    sent = hev_udpgw_transport_send (frame, sizeof (frame), &transport);
    if (expect_int ("sent length", sent, (int)sizeof (frame)))
        return 1;
    if (expect_int ("send calls", state.send_calls, 1))
        return 1;
    if (expect_int ("send handle", state.last_handle, 1))
        return 1;
    if (expect_int ("send length", (int)state.last_len, (int)sizeof (frame)))
        return 1;
    if (memcmp (state.last_frame, frame, sizeof (frame)) != 0) {
        fprintf (stderr, "frame differs\n");
        return 1;
    }

    state.send_ret = -9;
    sent = hev_udpgw_transport_send (frame, sizeof (frame), &transport);
    if (expect_int ("transport send error", sent, -9))
        return 1;
    if (expect_int ("send calls after error", state.send_calls, 2))
        return 1;

    hev_udpgw_transport_close (&transport);
    if (expect_int ("close calls", state.close_calls, 1))
        return 1;
    if (expect_int ("closed", hev_udpgw_transport_is_open (&transport), 0))
        return 1;
    sent = hev_udpgw_transport_send (frame, sizeof (frame), &transport);
    if (sent >= 0) {
        fprintf (stderr, "closed transport should reject send\n");
        return 1;
    }
    if (expect_int ("closed send no callback", state.send_calls, 2))
        return 1;

    return 0;
}

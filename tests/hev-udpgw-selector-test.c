#include <stdio.h>
#include <string.h>

#include "hev-config.h"
#include "hev-udpgw-session.h"

static HevConfigUdpGw cfg;

HevConfigUdpGw *
test_hev_config_get_udpgw_server (void)
{
    return &cfg;
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
    memset (&cfg, 0, sizeof (cfg));
    if (expect_int ("default disabled", hev_udpgw_session_should_handle_udp (), 0))
        return 1;

    cfg.enabled = 1;
    if (expect_int ("enabled without address", hev_udpgw_session_should_handle_udp (), 0))
        return 1;

    strncpy (cfg.addr, "127.0.0.1", sizeof (cfg.addr) - 1);
    if (expect_int ("enabled without port", hev_udpgw_session_should_handle_udp (), 0))
        return 1;

    cfg.port = 7300;
    if (expect_int ("enabled with address and port", hev_udpgw_session_should_handle_udp (), 1))
        return 1;

    cfg.enabled = 0;
    if (expect_int ("disabled with address and port", hev_udpgw_session_should_handle_udp (), 0))
        return 1;

    return 0;
}

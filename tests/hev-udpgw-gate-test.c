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

static int failures;

static void
expect_int (const char *name, int expected, int actual)
{
    if (expected == actual)
        return;

    fprintf (stderr, "%s: expected %d, got %d\n", name, expected, actual);
    failures++;
}

static void
reset_cfg (void)
{
    memset (&cfg, 0, sizeof (cfg));
    cfg.port = 7300;
    strcpy (cfg.addr, "127.0.0.1");
}

static void
test_default_off_rejects_enabled_udpgw (void)
{
    reset_cfg ();
    cfg.enabled = 1;

    expect_int ("default off", 0, hev_udpgw_session_should_handle_udp ());
}

static void
test_explicit_gate_allows_valid_udpgw (void)
{
    reset_cfg ();
    cfg.enabled = 1;
    cfg.experimental_hev = 1;

    expect_int ("explicit gate", 1, hev_udpgw_session_should_handle_udp ());
}

static void
test_explicit_gate_rejects_missing_address (void)
{
    reset_cfg ();
    cfg.enabled = 1;
    cfg.experimental_hev = 1;
    cfg.addr[0] = '\0';

    expect_int ("missing address", 0, hev_udpgw_session_should_handle_udp ());
}

static void
test_explicit_gate_rejects_missing_port (void)
{
    reset_cfg ();
    cfg.enabled = 1;
    cfg.experimental_hev = 1;
    cfg.port = 0;

    expect_int ("missing port", 0, hev_udpgw_session_should_handle_udp ());
}

int
main (void)
{
    test_default_off_rejects_enabled_udpgw ();
    test_explicit_gate_allows_valid_udpgw ();
    test_explicit_gate_rejects_missing_address ();
    test_explicit_gate_rejects_missing_port ();

    return failures ? 1 : 0;
}

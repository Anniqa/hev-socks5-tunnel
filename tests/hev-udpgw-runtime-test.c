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
expect_int (const char *name, int got, int want)
{
    if (got != want) {
        printf ("%s: expected %d, got %d\n", name, want, got);
        return 1;
    }
    return 0;
}

int
main (void)
{
    HevUdpGwRuntimeConfig rt;

    memset (&cfg, 0, sizeof (cfg));
    if (expect_int ("disabled runtime config", hev_udpgw_runtime_config_from_current (&rt), -1))
        return 1;

    cfg.enabled = 1;
    cfg.experimental_hev = 1;
    strncpy (cfg.addr, "127.0.0.1", sizeof (cfg.addr) - 1);
    cfg.port = 7300;
    cfg.max_connections = 9;
    cfg.connection_buffer_size = 33;
    cfg.transparent_dns = 1;

    if (expect_int ("enabled runtime config", hev_udpgw_runtime_config_from_current (&rt), 0))
        return 1;
    if (expect_int ("max connections", rt.session.max_connections, 9))
        return 1;
    if (expect_int ("transparent dns", rt.session.transparent_dns, 1))
        return 1;
    if (expect_int ("buffer size", rt.connection_buffer_size, 33))
        return 1;
    if (expect_int ("endpoint port", rt.endpoint.port, 7300))
        return 1;
    if (strcmp (rt.endpoint.addr, "127.0.0.1") != 0) {
        printf ("endpoint addr mismatch: %s\n", rt.endpoint.addr);
        return 1;
    }

    cfg.max_connections = 0;
    cfg.connection_buffer_size = 0;
    if (expect_int ("defaulted runtime config", hev_udpgw_runtime_config_from_current (&rt), 0))
        return 1;
    if (expect_int ("default max connections", rt.session.max_connections, HEV_UDPGW_CONN_MAP_CAPACITY))
        return 1;
    if (expect_int ("default buffer size", rt.connection_buffer_size, 64))
        return 1;

    printf ("hev-udpgw-runtime-test ok\n");
    return 0;
}

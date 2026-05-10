#include <stdio.h>
#include <string.h>

#include "hev-config.h"
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
expect_str (const char *label, const char *actual, const char *expected)
{
    if (strcmp (actual, expected) != 0) {
        fprintf (stderr, "%s: expected %s, got %s\n", label, expected, actual);
        return 1;
    }
    return 0;
}

int
main (void)
{
    HevConfigUdpGw cfg;
    HevUdpGwEndpoint endpoint;

    memset (&cfg, 0, sizeof (cfg));
    if (expect_int ("missing config", hev_udpgw_endpoint_from_config (NULL, &endpoint), -1))
        return 1;
    if (expect_int ("missing endpoint", hev_udpgw_endpoint_from_config (&cfg, NULL), -1))
        return 1;
    if (expect_int ("disabled", hev_udpgw_endpoint_from_config (&cfg, &endpoint), -1))
        return 1;

    cfg.enabled = 1;
    cfg.port = 7300;
    if (expect_int ("missing addr", hev_udpgw_endpoint_from_config (&cfg, &endpoint), -1))
        return 1;

    strncpy (cfg.addr, "127.0.0.1", sizeof (cfg.addr) - 1);
    cfg.port = 0;
    if (expect_int ("missing port", hev_udpgw_endpoint_from_config (&cfg, &endpoint), -1))
        return 1;

    cfg.port = 7300;
    if (expect_int ("valid endpoint", hev_udpgw_endpoint_from_config (&cfg, &endpoint), 0))
        return 1;
    if (expect_str ("addr", endpoint.addr, "127.0.0.1"))
        return 1;
    if (expect_int ("port", endpoint.port, 7300))
        return 1;

    memset (&cfg, 'a', sizeof (cfg.addr) - 1);
    cfg.addr[sizeof (cfg.addr) - 1] = '\0';
    cfg.enabled = 1;
    cfg.port = 7300;
    if (expect_int ("long addr", hev_udpgw_endpoint_from_config (&cfg, &endpoint), 0))
        return 1;
    if (expect_int ("endpoint nul", endpoint.addr[sizeof (endpoint.addr) - 1], '\0'))
        return 1;

    return 0;
}

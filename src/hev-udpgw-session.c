/*
 * MiniZiVPN staged HEV UDPGW legacy support.
 *
 * Runtime packet handling is intentionally not wired in this phase. The file
 * compiles into libhev-socks5-tunnel so later phases can add UDPGW session
 * creation without disturbing the existing SOCKS5 UDP implementation.
 */

#include "hev-config.h"
#include "hev-udpgw-proto.h"
#include "hev-udpgw-session.h"

int
hev_udpgw_session_is_enabled (void)
{
    HevConfigUdpGw *cfg = hev_config_get_udpgw_server ();

    return cfg && cfg->enabled;
}

HevConfigUdpGw *
hev_udpgw_session_get_config (void)
{
    return hev_config_get_udpgw_server ();
}

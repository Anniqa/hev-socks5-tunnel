/*
 * MiniZiVPN staged HEV UDPGW legacy support.
 *
 * The real UDPGW session is introduced in a later phase. This phase only
 * exposes compile-time protocol/config seams and keeps runtime routing on
 * existing HEV SOCKS5 UDP or BADVPN fallback paths.
 */

#ifndef __HEV_UDPGW_SESSION_H__
#define __HEV_UDPGW_SESSION_H__

#include "hev-config.h"

int hev_udpgw_session_is_enabled (void);
HevConfigUdpGw *hev_udpgw_session_get_config (void);

#endif /* __HEV_UDPGW_SESSION_H__ */

#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>

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
expect_u16 (const char *label, uint16_t actual, uint16_t expected)
{
    if (actual != expected) {
        fprintf (stderr, "%s: expected %u, got %u\n", label, expected, actual);
        return 1;
    }
    return 0;
}

int
main (void)
{
    HevUdpGwConnMap map;
    HevUdpGwConn *first;
    HevUdpGwConn *again;
    HevUdpGwConn *second;
    HevUdpGwConn *overflow;
    uint32_t ip_a = inet_addr ("1.2.3.4");
    uint32_t ip_b = inet_addr ("8.8.8.8");
    uint16_t port_dns = htons (53);
    uint16_t port_ntp = htons (123);

    if (expect_int ("init", hev_udpgw_conn_map_init (&map, 2), 0))
        return 1;
    if (expect_int ("initial size", hev_udpgw_conn_map_size (&map), 0))
        return 1;

    first = hev_udpgw_conn_map_get_or_create (&map, ip_a, port_dns, 0);
    if (!first) {
        fprintf (stderr, "first connection should be created\n");
        return 1;
    }
    if (expect_u16 ("first conid", first->conid, 1))
        return 1;
    if (expect_int ("size after first", hev_udpgw_conn_map_size (&map), 1))
        return 1;

    again = hev_udpgw_conn_map_get_or_create (&map, ip_a, port_dns, 0);
    if (again != first) {
        fprintf (stderr, "same destination should reuse connection\n");
        return 1;
    }
    if (expect_u16 ("reused conid", again->conid, 1))
        return 1;
    if (expect_int ("reused should not mark rebind", again->needs_rebind, 0))
        return 1;
    if (expect_int ("size after reuse", hev_udpgw_conn_map_size (&map), 1))
        return 1;

    second = hev_udpgw_conn_map_get_or_create (&map, ip_b, port_ntp, 0);
    if (!second) {
        fprintf (stderr, "second connection should be created\n");
        return 1;
    }
    if (expect_u16 ("second conid", second->conid, 2))
        return 1;
    if (expect_int ("size after second", hev_udpgw_conn_map_size (&map), 2))
        return 1;

    overflow = hev_udpgw_conn_map_get_or_create (&map, ip_a, port_ntp, 0);
    if (overflow) {
        fprintf (stderr, "third unique connection should respect max cap\n");
        return 1;
    }
    if (expect_int ("size after overflow", hev_udpgw_conn_map_size (&map), 2))
        return 1;

    again = hev_udpgw_conn_map_get_or_create (&map, ip_a, port_dns, 1);
    if (again != first) {
        fprintf (stderr, "rebind should still reuse same destination\n");
        return 1;
    }
    if (expect_int ("rebind mark", again->needs_rebind, 1))
        return 1;
    hev_udpgw_conn_mark_sent (again);
    if (expect_int ("mark sent clears rebind", again->needs_rebind, 0))
        return 1;

    hev_udpgw_conn_map_clear (&map);
    if (expect_int ("size after clear", hev_udpgw_conn_map_size (&map), 0))
        return 1;

    return 0;
}

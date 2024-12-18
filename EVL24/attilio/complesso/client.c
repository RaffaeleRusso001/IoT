#include "contiki.h"
#include "net/routing/rpl-lite/rpl.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"

#define LOG_MODULE "RPL Client"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define SEND_INTERVAL (60 * CLOCK_SECOND)  // Periodic interval for sending parent info

static struct simple_udp_connection udp_conn;

/*---------------------------------------------------------------------------*/
PROCESS(rpl_client_process, "RPL Client");
AUTOSTART_PROCESSES(&rpl_client_process);
/*---------------------------------------------------------------------------*/

/* Function to get the IPv6 address of the client's parent */
static int get_parent_ipaddr(uip_ipaddr_t *parent_ip) {
  rpl_parent_t *parent = rpl_get_parent_instance()->dag.preferred_parent;
  if (parent == NULL) {
    return 0;  // No parent available
  }
  uip_ipaddr_copy(parent_ip, rpl_parent_get_ipaddr(parent));
  return 1;  // Parent found
}

/* UDP message sender */
static void send_parent_info() {
  uip_ipaddr_t parent_ip;
  if (!get_parent_ipaddr(&parent_ip)) {
    LOG_WARN("No parent found, skipping send\n");
    return;
  }

  LOG_INFO("Sending parent info ");
  LOG_INFO_6ADDR(&parent_ip);
  LOG_INFO_("\n");

  /* Send the parent's IPv6 address to the monitor */
  simple_udp_sendto(&udp_conn, &parent_ip, sizeof(parent_ip), NULL);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(rpl_client_process, ev, data) {
  static struct etimer timer;

  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, NULL);

  /* Set the periodic timer */
  etimer_set(&timer, SEND_INTERVAL);

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

    /* Periodically send parent info to the monitor */
    send_parent_info();

    /* Reset the timer */
    etimer_reset(&timer);
  }

  PROCESS_END();
}

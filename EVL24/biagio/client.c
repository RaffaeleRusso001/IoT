#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/routing/rpl-lite/rpl.h"
#include "uip.h"
#include "uip-ds6.h"
#include "sys/log.h"

#define LOG_MODULE "Client"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

static struct simple_udp_connection udp_conn;

PROCESS(client_process, "UDP Client");
AUTOSTART_PROCESSES(&client_process);

PROCESS_THREAD(client_process, ev, data)
{
  static struct etimer timer;
  uip_ipaddr_t root_ip;

  PROCESS_BEGIN();

  LOG_INFO("UDP Client process started\n");

  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, NULL);

  etimer_set(&timer, CLOCK_SECOND * 60);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

    if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&root_ip)) {
      rpl_dag_t *dag = rpl_get_any_dag();
      if(dag && dag->preferred_parent) {
        const uip_ipaddr_t *parent_ip = rpl_parent_get_ipaddr(dag->preferred_parent);
        if(parent_ip != NULL) {
          // Invia solo l'indirizzo del parent (16 byte)
          simple_udp_sendto(&udp_conn, parent_ip, sizeof(uip_ipaddr_t), &root_ip);
          LOG_INFO("Sent parent IP ");
          LOG_INFO_6ADDR(parent_ip);
          LOG_INFO_("\n");
        } else {
          LOG_INFO("Parent IP not available\n");
        }
      } else {
        LOG_INFO("No preferred parent found\n");
      }
    } else {
      LOG_INFO("Root not reachable\n");
    }

    etimer_reset(&timer);
  }

  PROCESS_END();
}

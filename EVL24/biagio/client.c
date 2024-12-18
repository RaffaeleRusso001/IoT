#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
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
  static uint16_t node_id = 1; // ID univoco del nodo
  uip_ipaddr_t root_ip;

  PROCESS_BEGIN();

  // Registrazione connessione UDP
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, NULL);

  // Imposta il timer per 60 secondi
  etimer_set(&timer, CLOCK_SECOND * 60);

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

    if (NETSTACK_ROUTING.node_is_reachable() &&
        NETSTACK_ROUTING.get_root_ipaddr(&root_ip)) {
      // Ottieni il next hop (verifica che l'API esista nella tua versione)
      const linkaddr_t *parent = NETSTACK_ROUTING.get_nexthop();
      uint16_t parent_id = parent ? parent->u8[7] : 0; // Usa l'ultima parte dell'indirizzo come ID

      uint16_t data[2] = {node_id, parent_id};

      simple_udp_sendto(&udp_conn, data, sizeof(data), &root_ip);
      LOG_INFO("Sent node %u, parent %u\n", node_id, parent_id);
    } else {
      LOG_INFO("Root not reachable\n");
    }

    etimer_reset(&timer);
  }

  PROCESS_END();
}

#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/routing/rpl-lite/rpl.h"  // Include per accedere alle funzioni RPL
#include "sys/log.h"

#define LOG_MODULE "Client"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

static struct simple_udp_connection udp_conn;

/*---------------------------------------------------------------------------*/
PROCESS(client_process, "UDP Client");
AUTOSTART_PROCESSES(&client_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(client_process, ev, data)
{
  static struct etimer timer;
  static uint16_t node_id = 0; // ID univoco del nodo
  uip_ipaddr_t root_ip;
  uip_ipaddr_t parent_ip;

  PROCESS_BEGIN();

  LOG_INFO("UDP Client process started\n");

  /* Registra la connessione UDP */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, NULL);

  /* Imposta il timer per inviare ogni minuto */
  etimer_set(&timer, CLOCK_SECOND * 60);

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

    /* Verifica se il nodo root è raggiungibile */
    if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&root_ip)) {
      /* Recupera l'indirizzo IPv6 del parent preferito */
      rpl_dag_t *dag = rpl_get_any_dag();
      if (dag && dag->preferred_parent) {
        rpl_parent_get_ipaddr(dag->preferred_parent, &parent_ip);

        /* Invia ID nodo e indirizzo parent al root */
        uint16_t data[2] = {node_id, parent_ip.u8[15]}; // Invio node_id e ultimo byte dell'IP parent
        simple_udp_sendto(&udp_conn, data, sizeof(data), &root_ip);

        LOG_INFO("Sent: node %u, parent IP ...:%02x\n", node_id, parent_ip.u8[15]);
      } else {
        LOG_INFO("No preferred parent found\n");
      }
    } else {
      LOG_INFO("Root not reachable\n");
    }

    etimer_reset(&timer); // Reset del timer per il prossimo invio
  }

  PROCESS_END();
}

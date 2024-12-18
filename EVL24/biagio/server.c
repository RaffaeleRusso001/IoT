#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"

#define LOG_MODULE "Monitor"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_SERVER_PORT 5678
#define TIMEOUT_INTERVAL (3 * 60) // Timeout nodi inattivi in secondi

static struct simple_udp_connection udp_conn;

typedef struct {
  uint16_t node_id;
  uint16_t parent_id;
  clock_time_t last_update;
} node_info_t;

#define MAX_NODES 50
static node_info_t node_table[MAX_NODES];
static int node_count = 0;

/* Aggiunge o aggiorna un nodo nella tabella */
static void
update_node(uint16_t node_id, uint16_t parent_id) {
  for (int i = 0; i < node_count; i++) {
    if (node_table[i].node_id == node_id) {
      node_table[i].parent_id = parent_id;
      node_table[i].last_update = clock_time();
      return;
    }
  }

  if (node_count < MAX_NODES) {
    node_table[node_count].node_id = node_id;
    node_table[node_count].parent_id = parent_id;
    node_table[node_count].last_update = clock_time();
    node_count++;
  }
}

/* Rimuove nodi inattivi */
static void
remove_inactive_nodes() {
  clock_time_t now = clock_time();
  for (int i = 0; i < node_count; i++) {
    if ((now - node_table[i].last_update) / CLOCK_SECOND >= TIMEOUT_INTERVAL) {
      LOG_INFO("Removing inactive node %u\n", node_table[i].node_id);
      for (int j = i; j < node_count - 1; j++) {
        node_table[j] = node_table[j + 1];
      }
      node_count--;
      i--; // Rimanere sull'indice corrente
    }
  }
}

/* Stampa la tabella dei nodi */
static void
print_node_table() {
  LOG_INFO("Current node table:\n");
  for (int i = 0; i < node_count; i++) {
    LOG_INFO("Node %u, Parent %u, Last update %lu seconds ago\n",
             node_table[i].node_id, node_table[i].parent_id,
             (clock_time() - node_table[i].last_update) / CLOCK_SECOND);
  }
}

PROCESS(monitor_process, "RPL Monitor");
AUTOSTART_PROCESSES(&monitor_process);

static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr,
                uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr,
                uint16_t receiver_port,
                const uint8_t *data,
                uint16_t datalen)
{
  if (datalen == 4) { // ID nodo + parent = 2 byte ciascuno
    uint16_t node_id = ((uint16_t *)data)[0];
    uint16_t parent_id = ((uint16_t *)data)[1];
    LOG_INFO("Received from node %u, parent %u\n", node_id, parent_id);
    update_node(node_id, parent_id);
  }
}

PROCESS_THREAD(monitor_process, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();

  NETSTACK_ROUTING.root_start();
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL, 0, udp_rx_callback);
  LOG_INFO("Monitor started\n");

  etimer_set(&timer, CLOCK_SECOND * 60);

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    remove_inactive_nodes();
    print_node_table();
    etimer_reset(&timer);
  }

  PROCESS_END();
}

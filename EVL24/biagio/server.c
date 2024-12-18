#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "uip.h"
#include "uip-ds6.h"
#include "sys/log.h"

#define LOG_MODULE "Monitor"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_SERVER_PORT 5678
#define TIMEOUT_INTERVAL (3 * 60) // Timeout in secondi

static struct simple_udp_connection udp_conn;

typedef struct {
  uip_ipaddr_t node_ip;
  uip_ipaddr_t parent_ip;
  clock_time_t last_update;
} node_info_t;

#define MAX_NODES 50
static node_info_t node_table[MAX_NODES];
static int node_count = 0;

static void
update_node(const uip_ipaddr_t *node_ip, const uip_ipaddr_t *parent_ip) {
  // Cerca se esiste già il nodo
  for (int i = 0; i < node_count; i++) {
    if(uip_ipaddr_cmp(&node_table[i].node_ip, node_ip)) {
      // Aggiorna parent e timestamp
      uip_ipaddr_copy(&node_table[i].parent_ip, parent_ip);
      node_table[i].last_update = clock_time();
      return;
    }
  }

  // Se non esiste, aggiungilo
  if (node_count < MAX_NODES) {
    uip_ipaddr_copy(&node_table[node_count].node_ip, node_ip);
    uip_ipaddr_copy(&node_table[node_count].parent_ip, parent_ip);
    node_table[node_count].last_update = clock_time();
    node_count++;
  } else {
    LOG_INFO("Node table full, cannot add new node.\n");
  }
}

static void
remove_inactive_nodes() {
  clock_time_t now = clock_time();
  for (int i = 0; i < node_count; i++) {
    if((now - node_table[i].last_update) / CLOCK_SECOND >= TIMEOUT_INTERVAL) {
      LOG_INFO("Removing inactive node ");
      LOG_INFO_6ADDR(&node_table[i].node_ip);
      LOG_INFO_("\n");
      for(int j = i; j < node_count - 1; j++) {
        node_table[j] = node_table[j + 1];
      }
      node_count--;
      i--;
    }
  }
}

static void
print_node_table() {
  LOG_INFO("Current node table:\n");
  for (int i = 0; i < node_count; i++) {
    LOG_INFO("Node ");
    LOG_INFO_6ADDR(&node_table[i].node_ip);
    LOG_INFO_(", Parent ");
    LOG_INFO_6ADDR(&node_table[i].parent_ip);
    LOG_INFO_(", Last update %lu seconds ago\n",
             (clock_time() - node_table[i].last_update) / CLOCK_SECOND);
  }
}

static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr,
                uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr,
                uint16_t receiver_port,
                const uint8_t *data,
                uint16_t datalen)
{
  // Ci aspettiamo 16 byte: l'indirizzo IPv6 del parent
  if(datalen == 16) {
    uip_ipaddr_t parent_ip;
    memcpy(&parent_ip, data, 16);

    LOG_INFO("Received update from node ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_(", parent ");
    LOG_INFO_6ADDR(&parent_ip);
    LOG_INFO_("\n");

    update_node(sender_addr, &parent_ip);
  } else {
    LOG_INFO("Received packet with unexpected size: %u bytes\n", datalen);
  }
}

PROCESS(monitor_process, "RPL Monitor");
AUTOSTART_PROCESSES(&monitor_process);

PROCESS_THREAD(monitor_process, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();

  NETSTACK_ROUTING.root_start();
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL, 0, udp_rx_callback);
  LOG_INFO("Monitor started\n");

  etimer_set(&timer, CLOCK_SECOND * 60);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    remove_inactive_nodes();
    print_node_table();
    etimer_reset(&timer);
  }

  PROCESS_END();
}

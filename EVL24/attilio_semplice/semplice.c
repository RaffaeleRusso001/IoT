#include "contiki.h"
#include "net/routing/rpl-lite/rpl.h"
#include "net/ipv6/simple-udp.h"
#include "net/netstack.h"
#include "sys/log.h"

#define LOG_MODULE "RPL Monitor"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_SERVER_PORT 5678
#define UDP_CLIENT_PORT 8765

#define REPORT_INTERVAL (60 * CLOCK_SECOND)
#define TIMEOUT_LIMIT 3
#define MAX_CLIENTS 10

static struct simple_udp_connection udp_conn;

/* Data structure to store client information */
typedef struct {
  uip_ipaddr_t client_ip;
  uip_ipaddr_t parent_ip;
  unsigned last_update;  // Last update time in minutes
} client_info_t;

static client_info_t clients[MAX_CLIENTS];
static unsigned current_time = 0;

/*---------------------------------------------------------------------------*/
PROCESS(rpl_monitor_process, "RPL Monitor");
AUTOSTART_PROCESSES(&rpl_monitor_process);
/*---------------------------------------------------------------------------*/

/* Function to configure the RPL root */
static void configure_rpl_root() {
  uip_ipaddr_t root_ip;
  rpl_prefix_t prefix;

  /* Create a link-local address for the root */
  uip_create_linklocal_prefix(&root_ip);
  uip_ds6_set_addr_iid(&root_ip, &uip_lladdr);

  if (!uip_ds6_addr_add(&root_ip, 0, ADDR_AUTOCONF)) {
    LOG_ERR("Failed to configure root IP address\n");
    return;
  }

  /* Set the RPL DAG root and configure the prefix */
  rpl_dag_root_init();
  uip_ip6addr_copy(&prefix.prefix, &root_ip);
  prefix.length = 64;

  if (!rpl_set_prefix(&prefix)) {
    LOG_ERR("Failed to set RPL prefix\n");
    return;
  }

  LOG_INFO("RPL Root configured with address ");
  LOG_INFO_6ADDR(&root_ip);
  LOG_INFO_("\n");
}

/* Function to update or add client information */
static void update_client(const uip_ipaddr_t *client_ip, const uip_ipaddr_t *parent_ip) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (uip_ipaddr_cmp(&clients[i].client_ip, client_ip)) {
      uip_ipaddr_copy(&clients[i].parent_ip, parent_ip);
      clients[i].last_update = current_time;
      return;
    }
  }

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].last_update == 0) {
      uip_ipaddr_copy(&clients[i].client_ip, client_ip);
      uip_ipaddr_copy(&clients[i].parent_ip, parent_ip);
      clients[i].last_update = current_time;
      return;
    }
  }
}

/* Function to remove inactive clients */
static void remove_inactive_clients() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].last_update > 0 && (current_time - clients[i].last_update) >= TIMEOUT_LIMIT) {
      clients[i].last_update = 0;
    }
  }
}

/* Function to print the list of clients */
static void print_clients() {
  LOG_INFO("Client list:\n");
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].last_update > 0) {
      LOG_INFO("Client ");
      LOG_INFO_6ADDR(&clients[i].client_ip);
      LOG_INFO_(" -> Parent ");
      LOG_INFO_6ADDR(&clients[i].parent_ip);
      LOG_INFO_("\n");
    }
  }
}

/* UDP receive callback */
static void udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr, uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr, uint16_t receiver_port,
         const uint8_t *data, uint16_t datalen) {
  if (

#include "contiki.h"
#include "net/routing/routing.h"
#include "net/routing/rpl-lite/rpl.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

#include "sys/log.h"
#include "sys/etimer.h"

#define LOG_MODULE "RPL Monitor"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_SERVER_PORT 5678
#define UDP_CLIENT_PORT 8765

#define REPORT_INTERVAL (60 * CLOCK_SECOND)  // Periodic reporting interval
#define TIMEOUT_LIMIT 3                      // Timeout in minutes

#define MAX_CLIENTS 10                       // Maximum number of clients

static struct simple_udp_connection udp_conn;

/* Data structure to store client information */
typedef struct {
  uip_ipaddr_t client_ip;
  uip_ipaddr_t parent_ip;
  unsigned last_update;  // Tracks the last update in minutes
} client_info_t;

static client_info_t clients[MAX_CLIENTS];
static unsigned current_time = 0;  // Tracks time in minutes

/*---------------------------------------------------------------------------*/
PROCESS(rpl_monitor_process, "RPL Monitor");
AUTOSTART_PROCESSES(&rpl_monitor_process);
/*---------------------------------------------------------------------------*/

/* Function to add or update client information */
static void update_client_info(const uip_ipaddr_t *client_ip, const uip_ipaddr_t *parent_ip) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (uip_ipaddr_cmp(&clients[i].client_ip, client_ip)) {
      // Update existing client
      uip_ipaddr_copy(&clients[i].parent_ip, parent_ip);
      clients[i].last_update = current_time;
      LOG_INFO("Updated client ");
      LOG_INFO_6ADDR(client_ip);
      LOG_INFO_("\n");
      return;
    }
  }

  // Add new client if space is available
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].last_update == 0) {  // Unused slot
      uip_ipaddr_copy(&clients[i].client_ip, client_ip);
      uip_ipaddr_copy(&clients[i].parent_ip, parent_ip);
      clients[i].last_update = current_time;
      LOG_INFO("Added new client ");
      LOG_INFO_6ADDR(client_ip);
      LOG_INFO_("\n");
      return;
    }
  }

  LOG_WARN("No space to add new client ");
  LOG_INFO_6ADDR(client_ip);
  LOG_INFO_("\n");
}

/* Function to remove inactive clients */
static void remove_inactive_clients() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].last_update > 0 && (current_time - clients[i].last_update) >= TIMEOUT_LIMIT) {
      LOG_INFO("Removing inactive client ");
      LOG_INFO_6ADDR(&clients[i].client_ip);
      LOG_INFO_("\n");
      clients[i].last_update = 0;  // Mark as unused
    }
  }
}

/* Function to print current client list */
static void print_client_list() {
  LOG_INFO("Current client list:\n");
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
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen) {
  uip_ipaddr_t parent_ip;
  if (datalen != sizeof(uip_ipaddr_t)) {
    LOG_WARN("Invalid packet received\n");
    return;
  }

  // Extract parent IP from the packet
  memcpy(&parent_ip, data, sizeof(uip_ipaddr_t));

  // Update client information
  update_client_info(sender_addr, &parent_ip);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(rpl_monitor_process, ev, data) {
  static struct etimer report_timer;

  PROCESS_BEGIN();

  /* Initialize clients list */
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i].last_update = 0;
  }

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  etimer_set(&report_timer, REPORT_INTERVAL);

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&report_timer));

    // Update time
    current_time++;

    // Remove inactive clients
    remove_inactive_clients();

    // Print the current client list
    print_client_list();

    // Reset timer for the next report
    etimer_reset(&report_timer);
  }

  PROCESS_END();
}

#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT    8765
#define UDP_SERVER_PORT    5678
#define MAX_READINGS       10
#define SEND_INTERVAL      (60 * CLOCK_SECOND)
#define FAKE_TEMPS         5

static struct simple_udp_connection udp_conn;
static unsigned readings[MAX_READINGS];  // Buffer per le letture locali
static unsigned next_reading = 0;
static bool is_connected = true;  // Stato di connessione

/*---------------------------------------------------------------------------*/
static unsigned get_temperature() {
  static unsigned fake_temps[FAKE_TEMPS] = {30, 25, 20, 15, 10};
  return fake_temps[random_rand() % FAKE_TEMPS];
}

/*---------------------------------------------------------------------------*/
static void udp_rx_callback(struct simple_udp_connection *c,
                            const uip_ipaddr_t *sender_addr,
                            uint16_t sender_port,
                            const uip_ipaddr_t *receiver_addr,
                            uint16_t receiver_port,
                            const uint8_t *data,
                            uint16_t datalen) {
  // Callback vuoto per ora
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer timer;
  static uip_ipaddr_t server_ipaddr;
  unsigned temp;

  PROCESS_BEGIN();

  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, udp_rx_callback);
  
  while(1) {
    etimer_set(&timer, SEND_INTERVAL);  // Timer per invio dei dati ogni minuto
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));

    // Simula una lettura della temperatura
    temp = get_temperature();
    LOG_INFO("Sending temperature: %u\n", temp);

    if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&server_ipaddr)) {
      // Il client è connesso al server
      simple_udp_sendto(&udp_conn, &temp, sizeof(temp), &server_ipaddr);
    } else {
      // Il client è disconnesso, accumula la lettura localmente
      if (next_reading < MAX_READINGS) {
        readings[next_reading++] = temp;
        LOG_INFO("Stored reading locally, total: %u\n", next_reading);
      }

      // Se il buffer è pieno, invia la media delle letture accumulate
      if (next_reading >= MAX_READINGS) {
        unsigned sum = 0;
        for (int i = 0; i < MAX_READINGS; i++) {
          sum += readings[i];
        }
        unsigned avg = sum / MAX_READINGS;
        LOG_INFO("Sending batch average: %u\n", avg);

        if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&server_ipaddr)) {
          simple_udp_sendto(&udp_conn, &avg, sizeof(avg), &server_ipaddr);
        }
        // Reset the readings after sending
        next_reading = 0;
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

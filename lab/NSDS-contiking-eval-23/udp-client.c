#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT    8765
#define UDP_SERVER_PORT    5678
#define MAX_READINGS       10
#define SEND_INTERVAL      (60 * CLOCK_SECOND)
#define FAKE_TEMPS         5

static struct simple_udp_connection udp_conn;
static bool is_connected = false;
static unsigned readings[MAX_READINGS];   // Buffer per le letture accumulate
static unsigned next_reading = 0;         // Indice per la lettura successiva
static unsigned count = 0;                // Numero delle letture inviate

/*---------------------------------------------------------------------------*/
// Funzione per generare letture di temperatura fittizie
static unsigned get_temperature() {
  static unsigned fake_temps[FAKE_TEMPS] = {30, 25, 20, 15, 10};
  return fake_temps[random_rand() % FAKE_TEMPS];
}

/*---------------------------------------------------------------------------*/
// Callback UDP che viene chiamato quando il server risponde
static void udp_rx_callback(struct simple_udp_connection *c,
                             const uip_ipaddr_t *sender_addr,
                             uint16_t sender_port,
                             const uip_ipaddr_t *receiver_addr,
                             uint16_t receiver_port,
                             const uint8_t *data,
                             uint16_t datalen) {
  is_connected = true;  // Riconnessione avvenuta con successo
  LOG_INFO("Received data: %u from ", *((unsigned*)data));  // Mostra il dato ricevuto
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");

  // Invio dei dati accumulati localmente
  if (next_reading > 0) {
    for (int i = 0; i < next_reading; i++) {
      unsigned temperature = readings[i];
      LOG_INFO("Sending locally batched temperature %u to server\n", temperature);
      simple_udp_sendto(&udp_conn, &temperature, sizeof(temperature), sender_addr);
    }
    next_reading = 0; // Reset del buffer
  }
}

/*---------------------------------------------------------------------------*/
// Funzione per inviare una lettura al server
static void send_temperature_reading(uip_ipaddr_t *dest_ipaddr) {
  unsigned temperature = get_temperature();
  LOG_INFO("Sending temperature %u to server\n", temperature);
  simple_udp_sendto(&udp_conn, &temperature, sizeof(temperature), dest_ipaddr);
}

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data) {
  static struct etimer timer;
  static struct etimer reconnect_timer;
  uip_ipaddr_t dest_ipaddr;

  PROCESS_BEGIN();

  // Inizializzazione della connessione UDP
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, udp_rx_callback);

  // Tentativo iniziale di connessione
  if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
    LOG_INFO("Connected to server at ");
    LOG_INFO_6ADDR(&dest_ipaddr);
    LOG_INFO_("\n");
    is_connected = true;
  } else {
    LOG_INFO("Server not reachable yet, will attempt reconnect...\n");
  }

  etimer_set(&timer, SEND_INTERVAL);
  etimer_set(&reconnect_timer, CLOCK_SECOND * 10); // Timer di riconnessione periodica

  while (1) {
    PROCESS_WAIT_EVENT();

    if (ev == PROCESS_EVENT_TIMER) {
      if (etimer_expired(&timer)) {
        if (is_connected) {
          // Se connesso, invia il dato
          send_temperature_reading(&dest_ipaddr);
          count++;
          if (count >= MAX_READINGS) {
            LOG_INFO("Max readings reached, restarting\n");
            count = 0; // Reset conteggio
          }
        } else {
          // Accumula i dati localmente
          unsigned temp = get_temperature();
          readings[next_reading++] = temp;
          if (next_reading >= MAX_READINGS) {
            next_reading = 0; // Reset del buffer se pieno
          }
          LOG_INFO("Accumulating temperature locally: %u\n", temp);
        }
        etimer_reset(&timer);
      }

      // Timer di riconnessione
      if (etimer_expired(&reconnect_timer)) {
        if (!is_connected) {
          LOG_INFO("Attempting to reconnect...\n");
          if (NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
            LOG_INFO("Reconnected to server at ");
            LOG_INFO_6ADDR(&dest_ipaddr);
            LOG_INFO_("\n");
            is_connected = true;
          }
        }
        etimer_reset(&reconnect_timer);
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

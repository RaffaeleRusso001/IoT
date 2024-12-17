#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

#include "sys/log.h"

#include <stdbool.h>

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

static struct simple_udp_connection udp_conn;

#define MAX_READINGS 10
#define SEND_INTERVAL (60 * CLOCK_SECOND)
#define FAKE_TEMPS 5

// Used to differ simple_udp_sendto calls in time, to let the buffer clear values
#define BATCH_INTERVAL (4 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;

// Average and readings batch and aggregate variables
static float readings[MAX_READINGS];
static unsigned next_reading = 0;
static unsigned count_readings = 0;
static float avg = 0.0;
static bool avg_to_compute = false;

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);
/*---------------------------------------------------------------------------*/
static float
get_temperature()
{
  static float fake_temps [FAKE_TEMPS] = {30.0, 25.0, 20.0, 15.0, 10.0};
  return fake_temps[random_rand() % FAKE_TEMPS];
}
/*---------------------------------------------------------------------------*/
static void
udp_rx_callback(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  // Not used (clients does not receive anything from the server as per specifications)
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static struct etimer batch_timer;
  static float temperature;
  static uip_ipaddr_t dest_ipaddr;

  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {
        // If an average has been collected from disconnections (see below)
        if(avg_to_compute){
            avg = 0.0;
            for (int i = 0; i < count_readings; i++){
                avg += readings[i];
            }
            avg /= count_readings;
            count_readings = 0;
            next_reading = 0;
            avg_to_compute = false;
            LOG_INFO("Sending average temperature %f to ", avg);
            LOG_INFO_6ADDR(&dest_ipaddr);
            LOG_INFO_("\n");
            simple_udp_sendto(&udp_conn, &avg, sizeof(avg), &dest_ipaddr);
            etimer_set(&batch_timer, random_rand() % BATCH_INTERVAL);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&batch_timer));
        }
        // Also, the current temperature reading is sent in a new message
        /* Send to DAG root */
        temperature = get_temperature();
        LOG_INFO("Sending sensor reading %f to ", temperature);
        LOG_INFO_6ADDR(&dest_ipaddr);
        LOG_INFO_("\n");
        simple_udp_sendto(&udp_conn, &temperature, sizeof(temperature), &dest_ipaddr);
        } else {
            // Collect batch readings in a circular array
            LOG_INFO("Not reachable...\n");
            temperature = get_temperature();
            readings[next_reading] = temperature;
            next_reading = (next_reading + 1) % MAX_READINGS;
            if(count_readings < MAX_READINGS)
                count_readings++;
            avg_to_compute = true;
            LOG_INFO("Batching new local temperature reading %f\n", temperature);
        }

        /* Add some jitter */
        etimer_set(&periodic_timer, SEND_INTERVAL
        - CLOCK_SECOND + (random_rand() % (2 * CLOCK_SECOND)));
        }

        PROCESS_END();
}
/*---------------------------------------------------------------------------*/
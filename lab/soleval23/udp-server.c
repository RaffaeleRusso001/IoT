/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

#include "sys/log.h"
#include "random.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678
#define MAX_RECEIVERS 10
#define MAX_READINGS 10

#define ALERT_THRESHOLD 19

static struct simple_udp_connection udp_conn;

static float readings[MAX_READINGS];
static unsigned next_reading=0;

// Constraint about max known receivers
static uip_ipaddr_t receivers[MAX_RECEIVERS];
static uint8_t known_receivers = 0;

PROCESS(udp_server_process, "UDP server");
AUTOSTART_PROCESSES(&udp_server_process);
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
    // Reading (the server does not need to know if is an average or a real reading)
    float reading = *(float *)data;
    LOG_INFO("Received reading %f from ", reading);
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");

    static uint8_t i = 0;
    static uint8_t found = 0;

    // Receivers storing (if not already known)
    for (i = 0; i < known_receivers; i++) {
        LOG_INFO("Known ");
        LOG_INFO_6ADDR(&receivers[i]);
        LOG_INFO_("!\n");
    }

    for (i = 0; i < known_receivers; i++) {
        if (uip_ipaddr_cmp(&receivers[i], sender_addr)) {
            LOG_INFO("Found ");
            LOG_INFO_6ADDR(sender_addr);
            LOG_INFO_("!\n");
            found = 1;
        }
    }

    // Otherwise, store the new receiver if the total count of receivers is still valid
    if (!found) {
        if(known_receivers == MAX_RECEIVERS){
            LOG_INFO("Ignoring client ");
            LOG_INFO_6ADDR(sender_addr);
            LOG_INFO_("! (because the MAX_RECEIVERS threshold has been reached)\n");
            LOG_INFO("Average is not changed.\n");
            return;
        }
        LOG_INFO("Added ");
        LOG_INFO_6ADDR(sender_addr);
        LOG_INFO_("!\n");
        uip_ipaddr_copy(&receivers[known_receivers], sender_addr);
        known_receivers++;
    }

    // Add reading
    readings[next_reading++] = reading;
    if (next_reading == MAX_READINGS) {
        next_reading = 0;
    }

    // Compute average
    float average;
    float sum = 0;
    unsigned no = 0;
    for (i = 0; i < MAX_READINGS; i++) {
        if (readings[i] != 0) {
            sum = sum + readings[i];
            no++;
        }
    }
    average = ((float)sum)/no;
    LOG_INFO("Current average is %f \n", average);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data) {
  PROCESS_BEGIN();

  static uint8_t i = 0;
  /* Initialize temperature buffer */
  for (i=0; i < next_reading; i++) {
    readings[i] = 0;
  }  

  /* Initialize DAG root */
  NETSTACK_ROUTING.root_start();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

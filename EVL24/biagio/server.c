#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_SERVER_PORT 5678
#define UDP_CLIENT_PORT 8765

static struct simple_udp_connection udp_conn;

/*---------------------------------------------------------------------------*/
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
  unsigned count;
  if(datalen == sizeof(count)) {
    // Converte i dati ricevuti in un intero unsigned
    count = *(unsigned *)data;
    LOG_INFO("Server received request %u from ", count);
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");
    
    // Invia una risposta incrementando il contatore
    // (opzionale; si può anche restituire lo stesso valore)
    count++;
    simple_udp_sendto(c, &count, sizeof(count), sender_addr);
    LOG_INFO("Server sent response %u\n", count);
  } else {
    LOG_INFO("Server received data of unexpected length %u\n", datalen);
  }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  PROCESS_BEGIN();

  // Impostazione del nodo come root RPL.
  // Nota: Alcune piattaforme/configurazioni richiedono funzioni specifiche per
  // l'impostazione del root. Ad esempio:
  //
  //  NETSTACK_ROUTING.root_start();
  //
  // Questo comando fa sì che il nodo diventi root della DAG RPL.  
  // Assicurarsi che nel file di progetto o nella configurazione del firmware
  // siano attive le opportune impostazioni per lanciare la DAG.
  
#if ROUTING_CONF_RPL_LITE
  NETSTACK_ROUTING.root_start();
#else
  // Se non si utilizza RPL Lite, l'abilitazione del root potrebbe differire.
  // Consultare la documentazione della versione di Contiki/Contiki-NG in uso.
#endif

  // Registra una connessione UDP in ascolto sulla porta del server
  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  LOG_INFO("UDP server started on port %u\n", UDP_SERVER_PORT);
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

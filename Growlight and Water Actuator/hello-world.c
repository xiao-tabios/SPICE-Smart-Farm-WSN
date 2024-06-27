
#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "board-peripherals.h"
#include <stdint.h>
#include <inttypes.h>

#include "dev/gpio-hal.h"
#include "sys/etimer.h"
#include "lib/sensors.h"
#include "dev/button-hal.h"
#include <stdio.h>

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY    1
#define UDP_CLIENT_PORT   8772
#define UDP_SERVER_PORT   5678

#define SEND_INTERVAL     (5 * 60 * CLOCK_SECOND)

extern gpio_hal_pin_t out_pin1;
extern gpio_hal_pin_t out_pin2;
// extern gpio_hal_pin_t out_pin3;

#if GPIO_HAL_PORT_PIN_NUMBERING
extern gpio_hal_port_t out_port1;
extern gpio_hal_port_t out_port2;
// extern gpio_hal_port_t out_port3;

#else
#define out_port1   GPIO_HAL_NULL_PORT
#define out_port2   GPIO_HAL_NULL_PORT
#endif

static struct simple_udp_connection udp_conn;
static uint32_t rx_count = 0;
static int response_value;

/*---------------------------------------------------------------------------*/

PROCESS(udp_client_process, "UDP client");
AUTOSTART_PROCESSES(&udp_client_process);

/*---------------------------------------------------------------------------*/
static void gpio_operation_start_growlight() {
    gpio_hal_arch_pin_set_output(out_port1, out_pin1);
    gpio_hal_arch_write_pin(out_port1, out_pin1, 0);    
}

static void gpio_operation_end_growlight() {
    gpio_hal_arch_pin_set_output(out_port1, out_pin1);
    gpio_hal_arch_write_pin(out_port1, out_pin1, 1);    
}

static void gpio_operation_start_water() {
    gpio_hal_arch_pin_set_output(out_port2, out_pin2);
    gpio_hal_arch_write_pin(out_port2, out_pin2, 0);    
}

static void gpio_operation_end_water() {
    gpio_hal_arch_pin_set_output(out_port2, out_pin2);
    gpio_hal_arch_write_pin(out_port2, out_pin2, 1);    
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
  if (datalen == sizeof(response_value)) {
    memcpy(&response_value, data, sizeof(response_value));
    LOG_INFO("Received response '%d' from ", response_value);
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");
    rx_count++;
    if (response_value == 2) { //10
      gpio_operation_start_growlight();
      gpio_operation_end_water();
      LOG_INFO("RESPONSE on growlight, off water pump");
    }  
    else if (response_value == 0) { //00
      gpio_operation_end_growlight();
      gpio_operation_end_water();
      LOG_INFO("RESPONSE off growlight, off water pump");
    }
    else if (response_value == 3) { //11
      gpio_operation_start_growlight();
      gpio_operation_start_water();     
      LOG_INFO("RESPONSE on growlight, on water pump");
    }
    else if (response_value == 1) { //01
      gpio_operation_end_growlight();
      gpio_operation_start_water();     
      LOG_INFO("RESPONSE off growlight, on water pump");
    }        
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static char str[128];
  uip_ipaddr_t dest_ipaddr;
  static uint32_t tx_count;
  static uint32_t missed_tx_count;

  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));    

    if(NETSTACK_ROUTING.node_is_reachable() &&
        NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {

      if(tx_count % 10 == 0) {
        LOG_INFO("Tx/Rx/MissedTx: %" PRIu32 "/%" PRIu32 "/%" PRIu32 "\n",
                 tx_count, rx_count, missed_tx_count);
      }  

      LOG_INFO("Sending request %"PRIu32" to ", tx_count);
      LOG_INFO_6ADDR(&dest_ipaddr);
      LOG_INFO_("\n");      
      
      snprintf(str, sizeof(str), "%d\n", 1);            

      simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);
      tx_count++;
    } else {
      LOG_INFO("Not reachable yet\n");
      if(tx_count > 0) {
        missed_tx_count++;   
      }   
    }

    /* Reset the timer */
    etimer_set(&periodic_timer, SEND_INTERVAL);
  }

  PROCESS_END();
}

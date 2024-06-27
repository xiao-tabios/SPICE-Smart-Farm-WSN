
#include "contiki.h"
#include "dev/gpio-hal.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "board-peripherals.h"
#include "coap-engine.h"
#include "coap.h"
#include "sys/energest.h"
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if PLATFORM_SUPPORTS_BUTTON_HAL
#include "dev/button-hal.h"
#else
#include "dev/button-sensor.h"
#endif

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

#define WITH_SERVER_REPLY    1
#define UDP_CLIENT_PORT   8768
#define UDP_SERVER_PORT   5678

#define SEND_INTERVAL     (5 * 60 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;
static uint32_t rx_count = 0;

#if BOARD_SENSORTAG
extern coap_resource_t res_ds18b20;
#endif

extern gpio_hal_pin_t out_pin1;
#if GPIO_HAL_PORT_PIN_NUMBERING
extern gpio_hal_port_t out_port1;
#endif

static int temperature;
/*---------------------------------------------------------------------------*/

PROCESS(udp_client_process, "UDP client");
PROCESS(er_example_server, "CoAP Server");
PROCESS(ds18b20_process, "DS18B20 Process");
AUTOSTART_PROCESSES(&er_example_server, &udp_client_process, &ds18b20_process);

/*---------------------------------------------------------------------------*/
void delay_ms(uint16_t ms) {
    clock_delay_usec(ms * 1000);
}
int DS18B20_Start(void) {
    int Response = 0;
    gpio_hal_arch_pin_set_output(out_port1, out_pin1);   // set the pin as output
    gpio_hal_arch_write_pin(out_port1, out_pin1, 0);        // pull the pin low
    RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(480UL));   // delay according to datasheet
    
    gpio_hal_arch_pin_set_input(out_port1, out_pin1);    // set the pin as input
    RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(80UL));    // delay according to datasheet

    if (!(gpio_hal_arch_read_pin(out_port1, out_pin1))) {
        Response = 1;    // if the pin is low i.e the presence pulse is detected
    } else {
        Response = -1;
    }

    RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(400UL));; // 480 us delay totally
    

    return Response;
}


void DS18B20_Write(uint8_t data) {
    gpio_hal_arch_pin_set_output(out_port1, out_pin1);  // set as output

    for (int i = 0; i < 8; i++) {
        if ((data & (1 << i)) != 0) {
            // write 1
            gpio_hal_arch_pin_set_output(out_port1, out_pin1);  // set as output
            gpio_hal_arch_write_pin(out_port1, out_pin1, 0);        // pull the pin LOW
            RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(1UL));  // wait for 1 us
            gpio_hal_arch_pin_set_input(out_port1, out_pin1);   // set as input
            RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(50UL));  // wait for 60 us
        } else {
            // write 0
            gpio_hal_arch_pin_set_output(out_port1, out_pin1);
            gpio_hal_arch_write_pin(out_port1, out_pin1, 0);        // pull the pin LOW
            RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(50UL));  // wait for 60 us
            gpio_hal_arch_pin_set_input(out_port1, out_pin1);    // set as input
        }
    }
}

uint8_t DS18B20_Read(void) {
    uint8_t value = 0;
    gpio_hal_arch_pin_set_input(out_port1, out_pin1);         // set as input

    for (int i = 0; i < 8; i++) {
        gpio_hal_arch_pin_set_output(out_port1, out_pin1);     // set as output
        gpio_hal_arch_write_pin(out_port1, out_pin1, 0);       // pull the data pin LOW
        RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(2UL));  // wait for 2 us
        gpio_hal_arch_pin_set_input(out_port1, out_pin1);   // set as input
        if (gpio_hal_arch_read_pin(out_port1, out_pin1)) {  // if the pin is HIGH
            value |= (1 << i);  // read = 1
        }
        RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(60UL));  // wait for 60 us
    }
    return value;
    
}

static unsigned long
to_seconds(uint64_t time)
{
  return (unsigned long)(time / ENERGEST_SECOND);
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
  LOG_INFO("Received response '%.*s' from ", datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
#if LLSEC802154_CONF_ENABLED
  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif
  LOG_INFO_("\n");
  rx_count++;
}

static void 
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/* A simple getter example. Returns the reading from temp humid light sensor with a simple etag */
RESOURCE(res_ds18b20,
         "title=\"DS18B20(supports JSON)\";rt=\"WatertempSensor\"",
         res_get_handler,
         NULL,
         NULL,
         NULL);

static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

  unsigned int accept = -1;
  coap_get_header_accept(request, &accept);

  if (accept == -1 || accept == TEXT_PLAIN) {
    coap_set_header_content_format(response, TEXT_PLAIN);
    snprintf((char *)buffer, preferred_size, "Temperature: %d.%02d C", 
             temperature / 100, temperature % 100);

    coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
  } else if (accept == APPLICATION_XML) {
    coap_set_header_content_format(response, APPLICATION_XML);
    snprintf((char *)buffer, preferred_size, 
             "<sensor><temperature val=\"%d.%02d\" unit=\"C\"/>", 
             temperature / 100, temperature % 100);

    coap_set_payload(response, buffer, strlen((char *)buffer));
  } else if (accept == APPLICATION_JSON) {
    coap_set_header_content_format(response, APPLICATION_JSON);
    snprintf((char *)buffer, preferred_size, 
             "{\"sensor\":{\"temperature\":\"%d.%02d\"}", 
             temperature / 100, temperature % 100);

    coap_set_payload(response, buffer, strlen((char *)buffer));
  } else {
    coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
    const char *msg = "Supporting content-types text/plain, application/xml, and application/json";
    coap_set_payload(response, msg, strlen(msg));
  }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(er_example_server, ev, data)
{
  PROCESS_BEGIN();

  PROCESS_PAUSE();


#if BOARD_SENSORTAG
  coap_activate_resource(&res_ds18b20, "builtinsensors/watertemp");
#endif

  /* Define application-specific events here. */
  while(1) {
    PROCESS_WAIT_EVENT();
#if PLATFORM_HAS_BUTTON
#if PLATFORM_SUPPORTS_BUTTON_HAL
    if(ev == button_hal_release_event) {
#else
    if(ev == sensors_event && data == &button_sensor) {
#endif
      LOG_DBG("*******BUTTON*******\n");
    }
#endif /* PLATFORM_HAS_BUTTON */
  }                    

  PROCESS_END();
  }
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ds18b20_process, ev, data)
{
  static struct etimer et;
  uint8_t Temp_byte1;
  uint8_t Temp_byte2;
  int16_t temp_raw;


  PROCESS_BEGIN();

  etimer_set(&et, SEND_INTERVAL);  

  while(1) {
    // PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
    // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    int Presence = DS18B20_Start ();
    if (Presence == 1) {
        // printf("DS18B20 detected!\n");
        // RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(1UL));
        delay_ms(1);
        DS18B20_Write(0xCC);  // Skip ROM
        DS18B20_Write(0x44);  // Convert temperature
        // RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(800UL));
        delay_ms(800);
        

        Presence = DS18B20_Start ();
        // RTIMER_BUSYWAIT(US_TO_RTIMERTICKS(1UL));
        delay_ms(1);
        DS18B20_Write(0xCC);  // Skip ROM
        DS18B20_Write(0xBE);  // Read Scratch-pad

        Temp_byte1 = DS18B20_Read();
        // printf("Temp_byte1: %" PRIu8 "\n", Temp_byte1);
        Temp_byte2 = DS18B20_Read();
        // printf("Temp_byte2: %" PRIu8 "\n", Temp_byte2);
        temp_raw = (Temp_byte2 << 8) | Temp_byte1;
        // printf("temp_raw: %" PRIu16 "\n", temp_raw);
        temperature = temp_raw / 16;
        printf("Water Temperature=%d.%02d°C\n", temperature, temp_raw % 16);

    } else {
      printf("DS18B20 not detected!\n");
    }

    // etimer_reset(&et);
    etimer_set(&et, SEND_INTERVAL); // Reset the timer for the next second
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    // etimer_set(&et, CLOCK_SECOND);
      // PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER); // Wait for the end of the second
      // etimer_set(&et, CLOCK_SECOND*3);
      // etimer_set(&et, SEND_INTERVAL
      // - CLOCK_SECOND + (random_rand() % (CLOCK_SECOND)));
  // PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
  }
  PROCESS_END();
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

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);
  while(1) {
    // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if(NETSTACK_ROUTING.node_is_reachable() &&
        NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {


      if(tx_count % 10 == 0) {
        LOG_INFO("Tx/Rx/MissedTx: %" PRIu32 "/%" PRIu32 "/%" PRIu32 "\n",
                 tx_count, rx_count, missed_tx_count);
      }      
    
    
      LOG_INFO("Sending request %"PRIu32" to ", tx_count);
      LOG_INFO_6ADDR(&dest_ipaddr);
      LOG_INFO_("\n");      


      snprintf(str, sizeof(str), "Temperature: %d\n",
         temperature);      

      simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);
      tx_count++;
    } else {
      LOG_INFO("Not reachable yet\n");
      if(tx_count > 0) {
        missed_tx_count++;   
      }   
    }
    energest_flush();

    printf("\nEnergest:\n");
    printf(" CPU          %4lus LPM      %4lus DEEP LPM %4lus  Total time %lus\n",
           to_seconds(energest_type_time(ENERGEST_TYPE_CPU)),
           to_seconds(energest_type_time(ENERGEST_TYPE_LPM)),
           to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM)),
           to_seconds(ENERGEST_GET_TOTAL_TIME()));
    printf(" Radio LISTEN %4lus TRANSMIT %4lus OFF      %4lus\n",
           to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)),
           to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)),
           to_seconds(ENERGEST_GET_TOTAL_TIME()
                      - energest_type_time(ENERGEST_TYPE_TRANSMIT)
                      - energest_type_time(ENERGEST_TYPE_LISTEN)));
    /* Reset the timer */
    etimer_set(&periodic_timer, SEND_INTERVAL);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
  }

  PROCESS_END();
}


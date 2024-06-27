
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
#include "sys/ctimer.h"
#include "dev/i2c-arch.h"
#include "board-conf.h"
#include <Board.h>
#include <stdbool.h>
#include <math.h>

#include <ti/drivers/I2C.h>

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
#define UDP_CLIENT_PORT   8769
#define UDP_SERVER_PORT   5678

#define SEND_INTERVAL     (5 * 60 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;
static uint32_t rx_count = 0;

#if BOARD_SENSORTAG
extern coap_resource_t res_ecsensor;
#endif

// static char ec_data;

#define EC_SENSOR_I2C_ADDR        0x64

#if CONTIKI_BOARD_SENSORTAG_CC1352R1
typedef struct {
  uint8_t data1;
  uint8_t data2;
  uint8_t data3;
  uint8_t data4;
  uint8_t data5;
  uint8_t data6;
  uint8_t data7;
  uint8_t data8;
  uint8_t data9;
  uint8_t data10;
  uint8_t data11;
  uint8_t data12;
  uint8_t data13;
  uint8_t data14;
  uint8_t data15;
  uint8_t data16;
  uint8_t data17;
  uint8_t data18;
  uint8_t data19;
  uint8_t data20;
  uint8_t data21;
  uint8_t data22;
  uint8_t data23;
  uint8_t data24;
  uint8_t data25;
  uint8_t data26;
  uint8_t data27;
  uint8_t data28;
  uint8_t data29;
  uint8_t data30;
  uint8_t data31;

} ATLAS_eC_SensorData;
#endif

static ATLAS_eC_SensorData sensor_data;
static I2C_Handle i2c_handle;
static char ascii_data[6]; 
/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
PROCESS(er_example_server, "CoAP Server");
PROCESS(ph_sensor_process, "ph_sensor Process");
AUTOSTART_PROCESSES(&er_example_server, &udp_client_process, &ph_sensor_process);

static void receive_data_from_sensor() { 

  if(i2c_arch_read(i2c_handle, EC_SENSOR_I2C_ADDR, &sensor_data, sizeof(sensor_data))) {                            
    printf("\n");
  }
    i2c_arch_release(i2c_handle);
  }

void delay_ms(uint16_t ms) {
    clock_delay_usec(ms * 1000);
}

static bool
send_command_to_sensor(void) 
{
  bool rv;
#if CONTIKI_BOARD_SENSORTAG_CC1352R1
  uint8_t eC_reg[] = {0x72, 0x00};

#endif

  i2c_handle = i2c_arch_acquire(Board_I2C0);

  rv = i2c_arch_write(i2c_handle, EC_SENSOR_I2C_ADDR, eC_reg,
                      sizeof(eC_reg));

  return rv;

}

void convert_to_ascii(ATLAS_eC_SensorData sensor_data) {
    sprintf(ascii_data, "%c%c%c%c%c", sensor_data.data2, sensor_data.data3, sensor_data.data4, sensor_data.data5, sensor_data.data6);
    printf("eC sensor: %s\n", ascii_data);

}
// void delay_ms(uint16_t ms) {
//     clock_delay_usec(ms * 1000);
// }

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
RESOURCE(res_ecsensor,
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
    snprintf((char *)buffer, preferred_size, "eC sensor: %s C", 
             ascii_data);

    coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
  } else if (accept == APPLICATION_XML) {
    coap_set_header_content_format(response, APPLICATION_XML);
    snprintf((char *)buffer, preferred_size, 
             "<sensor><eC sensor val=\"%s\" unit=\"C\"/>", 
             ascii_data);

    coap_set_payload(response, buffer, strlen((char *)buffer));
  } else if (accept == APPLICATION_JSON) {
    coap_set_header_content_format(response, APPLICATION_JSON);
    snprintf((char *)buffer, preferred_size, 
             "{\"sensor\":{\"eC sensor\":\"%s\"}", 
             ascii_data);

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
  coap_activate_resource(&res_ecsensor, "builtinsensors/eCsensor");
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
PROCESS_THREAD(ph_sensor_process, ev, data)
{
    static struct etimer timer1;

    PROCESS_BEGIN();

    i2c_arch_init();    

    while (1) {
        memset(&sensor_data, 0, sizeof(sensor_data));

        send_command_to_sensor();

        etimer_set(&timer1, CLOCK_SECOND * 0.6);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer1));

        receive_data_from_sensor();

        convert_to_ascii(sensor_data);

        etimer_set(&timer1, SEND_INTERVAL); 
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer1));
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  // static char str[128];
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


      // snprintf(ascii_data, sizeof(ascii_data), "eC sensor: %s\n",
      //    ascii_data);      

      simple_udp_sendto(&udp_conn, ascii_data, strlen(ascii_data), &dest_ipaddr);
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


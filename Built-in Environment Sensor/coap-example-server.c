/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
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
 */

/**
 * \file
 *      Erbium (Er) CoAP Engine example.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch> & xiao
 */
#include "contiki.h"
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
#define UDP_CLIENT_PORT   8765
#define UDP_SERVER_PORT   5678

#define SEND_INTERVAL     (5 * 60 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;
static uint32_t rx_count = 0;
static int temperature, humidity, light; // Declare variables to store sensor readings
static clock_time_t send_time;
static char str[128];
/* Resource declaration */
// extern coap_resource_t
//   res_hello,
//   res_mirror,
//   res_chunks,
//   res_separate,
//   res_push,
//   res_event,
//   res_sub,
//   res_b1_sep_b2;

#if BOARD_SENSORTAG
extern coap_resource_t res_hdc1000opt3001;
// #else
// extern coap_resource_t res_toggle_orange;
// extern coap_resource_t res_toggle_yellow;
#endif

/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client");
PROCESS(er_example_server, "CoAP Server");
PROCESS(temp_reading, "Temp reading Process");
AUTOSTART_PROCESSES(&er_example_server, &udp_client_process, &temp_reading);

/*---------------------------------------------------------------------------*/
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
  clock_time_t receive_time = clock_time();
  clock_time_t round_trip_time = receive_time - send_time;    

  LOG_INFO("Received response '%.*s' from ", datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
#if LLSEC802154_CONF_ENABLED
  LOG_INFO_(" LLSEC LV:%d", uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#endif
  LOG_INFO_("\n");
  LOG_INFO("Round-trip time: %" PRIu32 " ms\n", (uint32_t)((round_trip_time * 1000) / CLOCK_SECOND));
  rx_count++;
}

/*---------------------------------------------------------------------------*/
static void
// get_sync_sensor_readings(int *temperature, int *humidity, int *light)
get_sync_sensor_readings(int *temperature, int *humidity, int *light)

{
  int value;

  printf("-----------------------------------------\n");

  /* HDC1000 Sensor */
  value = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_TEMP);
  if(value != HDC_1000_READING_ERROR) {
    printf("HDC: Temp=%d.%02d C\n", value / 100, value % 100);
    *temperature = value;
  } else {
    printf("HDC: Temp Read Error\n");
    *temperature = 0;
  }

  value = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_HUMID);
  if(value != HDC_1000_READING_ERROR) {
    printf("HDC: Humidity=%d.%02d %%RH\n", value / 100, value % 100);
    *humidity = value;
  } else {
    printf("HDC: Humidity Read Error\n");
    *humidity = 0;
  }

  /* Light Sensor */
  value = opt_3001_sensor.value(0);
  if(value != OPT_3001_READING_ERROR) {
    printf("OPT: Light=%d.%02d lux\n", value / 100, value % 100);
    *light = value;
  } else {
    printf("OPT: Light Read Error\n");
    *light = 0;
  }
  // printf("Sensor Values: Temperature=%d, Humidity=%d, Light=%d\n", sensor_values[0], sensor_values[1], sensor_values[2]);

  printf("-----------------------------------------\n");
  SENSORS_ACTIVATE(hdc_1000_sensor);
  SENSORS_ACTIVATE(opt_3001_sensor);
}

static void 
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

/* A simple getter example. Returns the reading from temp humid light sensor with a simple etag */
RESOURCE(res_hdc1000opt3001,
         "title=\"hdc1000 and opt 3001(supports JSON)\";rt=\"TempHumidLightSensor\"",
         res_get_handler,
         NULL,
         NULL,
         NULL);

static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{

  // if (temperature == 0 && humidity == 0 && light == 0) {
  //   // Attempt to read the sensor values again
  //   int attempts = 3; // Maximum number of attempts
  //   while (attempts--) {
  //     // Wait for a short duration before retrying


  //     // Check if any sensor reading is non-zero
  //     // if (hdc_temp_val != 0 || hdc_humid_val != 0 || opt_light_val != 0) {
  //     //   break; // Exit the loop if any reading is non-zero
  //     // }
  //   }
  // }

  unsigned int accept = -1;
  coap_get_header_accept(request, &accept);

  if (accept == -1 || accept == TEXT_PLAIN) {
    coap_set_header_content_format(response, TEXT_PLAIN);
    snprintf((char *)buffer, preferred_size, "Temperature: %d.%02d C, Humidity: %d.%02d %%, Light: %d.%02d lux", 
             temperature / 100, temperature % 100, 
             humidity / 100, humidity % 100, 
             light / 100, light % 100);

    coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
  } else if (accept == APPLICATION_XML) {
    coap_set_header_content_format(response, APPLICATION_XML);
    snprintf((char *)buffer, preferred_size, 
             "<sensor><temperature val=\"%d.%02d\" unit=\"C\"/><humidity val=\"%d.%02d\" unit=\"%%RH\"/><light val=\"%d.%02d\" unit=\"lux\"/></sensor>", 
             temperature / 100, temperature % 100, 
             humidity / 100, humidity % 100, 
             light / 100, light % 100);

    coap_set_payload(response, buffer, strlen((char *)buffer));
  } else if (accept == APPLICATION_JSON) {
    coap_set_header_content_format(response, APPLICATION_JSON);
    snprintf((char *)buffer, preferred_size, 
             "{\"sensor\":{\"temperature\":\"%d.%02d\",\"humidity\":\"%d.%02d\",\"light\":\"%d.%02d\"}}", 
             temperature / 100, temperature % 100, 
             humidity / 100, humidity % 100, 
             light / 100, light % 100);

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
  coap_activate_resource(&res_hdc1000opt3001, "builtinsensors/temp_humidity_light");
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
/*----------------------------------------------------------------------------*/
PROCESS_THREAD(temp_reading, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN();

  etimer_set(&et, CLOCK_SECOND);
  while(1) {

    // Wait for the timer to expire

    // Check the status of the float switch
    get_sync_sensor_readings(&temperature, &humidity, &light);

    snprintf(str, sizeof(str), "Temperature: %d, Humidity: %d, Light: %d\n",
      temperature, 
      humidity, 
      light);   

    // LOG_INFO_("Temperature: %d, Humidity: %d, Light: %d\n",
    //   temperature, 
    //   humidity, 
    //   light);

    etimer_set(&et, CLOCK_SECOND);    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));    
  }

  PROCESS_END();
}  
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  uip_ipaddr_t dest_ipaddr;
  static uint32_t tx_count;
  static uint32_t missed_tx_count;

  PROCESS_BEGIN();

  /* Initialize UDP connection */
  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1) {
    // PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if(NETSTACK_ROUTING.node_is_reachable() &&
        NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {


      if(tx_count % 10 == 0) {
        LOG_INFO("Tx/Rx/MissedTx: %" PRIu32 "/%" PRIu32 "/%" PRIu32 "\n",
                 tx_count, rx_count, missed_tx_count);
      }      
    
      /* Read sensor data */
      // get_sync_sensor_readings(&temperature, &humidity, &light);

      /* Format the payload with sensor data */
    
      LOG_INFO("Sending request %"PRIu32" to ", tx_count);
      LOG_INFO_6ADDR(&dest_ipaddr);
      LOG_INFO_("\n");      
      
      // snprintf(str, sizeof(str), "Temperature: %d.%02d, Humidity: %d.%02d, Light: %d.%02d\n",
      //    temperature / 100, temperature % 100, 
      //    humidity / 100, humidity % 100, 
      //    light / 100, light % 100);
      LOG_INFO_("Temperature: %d, Humidity: %d, Light: %d\n",
        temperature, 
        humidity, 
        light);
   

      send_time = clock_time();

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


#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include <stdio.h>
#include "random.h"
#include <stdlib.h>

#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY  1
#define UDP_SERVER_PORT 5678

// ENVIRONMENT SENSORS
#define UDP_CLIENT_PORT_1 8765  // TEMP, HUMID, LIGHT

// WATER SENSORS
#define UDP_CLIENT_PORT_3 8768  // DS18B20
#define UDP_CLIENT_PORT_4 8769  // ec sensor
#define UDP_CLIENT_PORT_5 8770  // ph sensor
#define UDP_CLIENT_PORT_6 8771  // float switch


// ACTUATORS
#define UDP_CLIENT_PORT_2 8766 // NUTRIENT PUMP
#define UDP_CLIENT_PORT_7 8772 // GROW LIGHT & WATER PUMP


static struct simple_udp_connection udp_conn_1; // For client port 8765
static struct simple_udp_connection udp_conn_2; // For client port 8766
static struct simple_udp_connection udp_conn_3; // For client port 8768
static struct simple_udp_connection udp_conn_4; // For client port 8769
static struct simple_udp_connection udp_conn_5; // For client port 8770
static struct simple_udp_connection udp_conn_6; // For client port 8771
static struct simple_udp_connection udp_conn_7; // For client port 8772



static int temperature, humidity, light;
static int water_temp, ec_sensor, float_switch;
static char ph_char[128];

PROCESS(rpl_border_router_process, "RPL Border Router Process");
AUTOSTART_PROCESSES(&rpl_border_router_process);

/*---------------------------------------------------------------------------*/
// 
static void
udp_rx_callback_1(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

  // LOG_INFO("RX Callback 1 UDP packet received from ");
  // LOG_INFO_6ADDR(sender_addr);
  // LOG_INFO_(" on port %d from port %d with length %d\n", receiver_port, sender_port, datalen);

    // Use sscanf to parse the string and extract the values
  sscanf((char *) data, "Temperature: %d, Humidity: %d, Light: %d", &temperature, &humidity, &light);
    // Now you have the individual values stored in the variables temperature, humidity, and light
  temperature = temperature / 100;
  humidity = humidity / 100;
  light = light / 100;

  printf("Temperature: %d deg Celsius\n", temperature);
  printf("Humidity: %d RH\n", humidity);
  printf("Light: %d centilux\n", light);


}

static void //NUTRIENT PUMP
udp_rx_callback_2(struct simple_udp_connection *c,  
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  static int response_value_nutrientpump;
  if (float_switch == 1) {
      response_value_nutrientpump = 1;
      simple_udp_sendto(&udp_conn_2, &response_value_nutrientpump, sizeof(response_value_nutrientpump), sender_addr);
  }
  else if (float_switch == 0) {
      response_value_nutrientpump = 0;
      simple_udp_sendto(&udp_conn_2, &response_value_nutrientpump, sizeof(response_value_nutrientpump), sender_addr);
  }
}

static void
udp_rx_callback_3(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

    sscanf((char *) data, "Temperature: %d", &water_temp);
    printf("Water Temperature: %d deg Celsius\n", water_temp);

}
static void
udp_rx_callback_4(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{


    sscanf((char *) data, "%d", &ec_sensor);
    printf("eC level: %d uS/cm \n", ec_sensor);
}
static void
udp_rx_callback_5(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

  if (datalen < sizeof(ph_char)) {
    memcpy(ph_char, data, datalen);
    ph_char[datalen] = '\0'; // Null-terminate the string
  } else {
    memcpy(ph_char, data, sizeof(ph_char) - 1);
    ph_char[sizeof(ph_char) - 1] = '\0'; // Null-terminate the string
  }
  printf("ph level: %s\n", ph_char);
}

static void
udp_rx_callback_6(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

    sscanf((char *) data, "%d", &float_switch);
    printf("water level: %d\n", float_switch);
}

static void //GROW LIGHT & WATER PUMP
udp_rx_callback_7(struct simple_udp_connection *c,  
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{

  static int response_value;
  if ((light < 50) && (float_switch == 0)){
      LOG_INFO("Insufficient light, Grow light is turned ON\n");
      LOG_INFO("Water Level High\n");
      response_value = 2; //10
      simple_udp_sendto(&udp_conn_7, &response_value, sizeof(response_value), sender_addr);
  }
  else if ((light > 50) && (float_switch == 0)){
      LOG_INFO("Sufficient light, Grow light is turned OFF\n");
      LOG_INFO("Water Level High\n");      
      response_value = 0; //00
      simple_udp_sendto(&udp_conn_7, &response_value, sizeof(response_value), sender_addr);
  }
  else if ((light < 50) && (float_switch == 1)){
      LOG_INFO("Insufficient light, Grow light is turned ON\n");
      LOG_INFO("Water Level Low, Adding Hydroponics Solution \n");
      response_value = 3; //11
      simple_udp_sendto(&udp_conn_7, &response_value, sizeof(response_value), sender_addr);
  }  
  else if ((light > 50) && (float_switch == 1)){
      LOG_INFO("Sufficient light, Grow light is turned OFF\n");
      LOG_INFO("Water Level Low, Adding Hydroponics Solution \n");
      response_value = 1; //01
      simple_udp_sendto(&udp_conn_7, &response_value, sizeof(response_value), sender_addr);
  }    

}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(rpl_border_router_process, ev, data)
{
  PROCESS_BEGIN();

  #if BORDER_ROUTER_CONF_WEBSERVER
    PROCESS_NAME(webserver_nogui_process);
    process_start(&webserver_nogui_process, NULL);
  #endif /* BORDER_ROUTER_CONF_WEBSERVER */

  LOG_INFO("RPL Border Router started\n");

  /* Initialize RPL */
  NETSTACK_ROUTING.root_start();

  /* Initialize UDP connections */
  simple_udp_register(&udp_conn_1, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT_1, udp_rx_callback_1);
  simple_udp_register(&udp_conn_2, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT_2, udp_rx_callback_2);
  simple_udp_register(&udp_conn_3, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT_3, udp_rx_callback_3);   
  simple_udp_register(&udp_conn_4, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT_4, udp_rx_callback_4);                                           
  simple_udp_register(&udp_conn_5, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT_5, udp_rx_callback_5);
  simple_udp_register(&udp_conn_6, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT_6, udp_rx_callback_6);                      
  simple_udp_register(&udp_conn_7, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT_7, udp_rx_callback_7);    
  PROCESS_END();
}

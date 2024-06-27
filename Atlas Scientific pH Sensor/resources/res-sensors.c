// /*
//  * Copyright (c) 2014, Texas Instruments Incorporated - http://www.ti.com/
//  * All rights reserved.
//  *
//  * Redistribution and use in source and binary forms, with or without
//  * modification, are permitted provided that the following conditions
//  * are met:
//  * 1. Redistributions of source code must retain the above copyright
//  *    notice, this list of conditions and the following disclaimer.
//  * 2. Redistributions in binary form must reproduce the above copyright
//  *    notice, this list of conditions and the following disclaimer in the
//  *    documentation and/or other materials provided with the distribution.
//  * 3. Neither the name of the copyright holder nor the names of its
//  *    contributors may be used to endorse or promote products derived
//  *    from this software without specific prior written permission.
//  *
//  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//  * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//  * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
//  * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//  * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
//  * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
//  * OF THE POSSIBILITY OF SUCH DAMAGE.
//  */
// /*---------------------------------------------------------------------------*/
// /**
//  * \addtogroup cc26x0-web-demo
//  * @{
//  *
//  * \file
//  *  CoAP resource handler for the Sensortag-CC26xx sensors
//  */
// /*---------------------------------------------------------------------------*/
// #include "contiki.h"
// #include "coap-engine.h"
// #include "coap.h"
// #include "board-peripherals.h"

// #include <stdio.h>
// #include <stdint.h>
// #include <string.h>
// /*---------------------------------------------------------------------------*/
// /*
//  * Generic resource handler for any sensor in this example. Ultimately gets
//  * called by all handlers and populates the CoAP response
//  */


// static void 
// res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

// /* A simple getter example. Returns the reading from temp humid light sensor with a simple etag */
// RESOURCE(res_hdc1000opt3001,
//          "title=\"hdc1000 and opt 3001(supports JSON)\";rt=\"TempHumidLightSensor\"",
//          res_get_handler,
//          NULL,
//          NULL,
//          NULL);

// static void
// res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
// {

//   SENSORS_ACTIVATE(hdc_1000_sensor);
//   SENSORS_ACTIVATE(opt_3001_sensor);

//   int hdc_temp_val = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_TEMP);
//   int hdc_humid_val = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_HUMID);
//   int opt_light_val = opt_3001_sensor.value(0);

//     // Check if all sensor readings are zero
//   if (hdc_temp_val == 0 && hdc_humid_val == 0 && opt_light_val == 0) {
//     // Attempt to read the sensor values again
//     int attempts = 3; // Maximum number of attempts
//     while (attempts--) {
//       // Wait for a short duration before retrying

//       hdc_temp_val = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_TEMP);
//       hdc_humid_val = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_HUMID);
//       opt_light_val = opt_3001_sensor.value(0);

//       // Check if any sensor reading is non-zero
//       // if (hdc_temp_val != 0 || hdc_humid_val != 0 || opt_light_val != 0) {
//       //   break; // Exit the loop if any reading is non-zero
//       // }
//     }
//   }


//   unsigned int accept = -1;
//   coap_get_header_accept(request, &accept);

//   if (accept == -1 || accept == TEXT_PLAIN) {
//     coap_set_header_content_format(response, TEXT_PLAIN);
//     snprintf((char *)buffer, preferred_size, "Temperature: %d.%02d C, Humidity: %d.%02d %%, Light: %d.%02d lux", 
//              hdc_temp_val / 100, hdc_temp_val % 100, 
//              hdc_humid_val / 100, hdc_humid_val % 100, 
//              opt_light_val / 100, opt_light_val % 100);

//     coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
//   } else if (accept == APPLICATION_XML) {
//     coap_set_header_content_format(response, APPLICATION_XML);
//     snprintf((char *)buffer, preferred_size, 
//              "<sensor><temperature val=\"%d.%02d\" unit=\"C\"/><humidity val=\"%d.%02d\" unit=\"%%RH\"/><light val=\"%d.%02d\" unit=\"lux\"/></sensor>", 
//              hdc_temp_val / 100, hdc_temp_val % 100, 
//              hdc_humid_val / 100, hdc_humid_val % 100, 
//              opt_light_val / 100, opt_light_val % 100);

//     coap_set_payload(response, buffer, strlen((char *)buffer));
//   } else if (accept == APPLICATION_JSON) {
//     coap_set_header_content_format(response, APPLICATION_JSON);
//     snprintf((char *)buffer, preferred_size, 
//              "{\"sensor\":{\"temperature\":\"%d.%02d\",\"humidity\":\"%d.%02d\",\"light\":\"%d.%02d\"}}", 
//              hdc_temp_val / 100, hdc_temp_val % 100, 
//              hdc_humid_val / 100, hdc_humid_val % 100, 
//              opt_light_val / 100, opt_light_val % 100);

//     coap_set_payload(response, buffer, strlen((char *)buffer));
//   } else {
//     coap_set_status_code(response, NOT_ACCEPTABLE_4_06);
//     const char *msg = "Supporting content-types text/plain, application/xml, and application/json";
//     coap_set_payload(response, msg, strlen(msg));
//   }
// }

/********************************************************************************************
 *                               HEADER GUARD                                               *
 ********************************************************************************************/
#ifndef BSP_H_
#define BSP_H_

/********************************************************************************************
 *                               INCLUDE DIRECTIVES                                         *
 ********************************************************************************************/
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h> 

/********************************************************************************************
 *                               DEFINITION SECTION                                         *
 ********************************************************************************************/
/* Pinout */
#define ENC_R 34               /* Encoder Right */
#define ENC_L 35               /* Encoder Left */
#define ENABLE_L 14            /* Control left motor */
#define ENABLE_R 32            /* Control right motor */
#define IN_R2 33               /* Input right motor */
#define IN_R1 25               /* Input right motor */
#define IN_L1 26               /* Input left motor */
#define IN_L2 27               /* Input left motor */

/* PWM control */
#define PWM_FREQ 20000U        /* Frequently 20KHz */ 
#define PWM_RESOLUTION 8       /* Resolution 8 bit */ 
#define PWM_CHANNEL_R1 0       /* CHANNEL FOR PIN IN_R1 */
#define PWM_CHANNEL_R2 1       /* CHANNEL FOR PIN IN_R2 */
#define PWM_CHANNEL_L1 2       /* CHANNEL FOR PIN IN_L1 */
#define PWM_CHANNEL_L2 3       /* CHANNEL FOR PIN IN_L2 */
#define PWM_DUTY_CYCLE 180     /* 75% duty cycle PWM value */ 

/* MQTT config */
#define MQTT_SERVER "broker.mqttdashboard.com"
#define MQTT_PORT 1883
#define MQTT_USER "chechanh2003"
#define MQTT_PASSWORD "0576289825"
#define TOPIC_1 "Livingroom/device_1"
#define TOPIC_2 "Livingroom/device_2"

/* Wifi config */
#define SSID_WIFI "TrieuMen"
#define PASSWORD_WIFI "1111aaaa"

/* Other */
#define TIMEOUT 4000U         /* Default timeout */
#define DEBUG_MODE 1U         /* Change to 0U if don't need to debug */
#define OK 0U
#define NOT_FOUND_DISTANCE_SENSOR_ERROR 2U

/********************************************************************************************
 *                               FUNCTION PROTOTYPE                                         *
 ********************************************************************************************/
void setup_wifi();                      /* Configuration for wifi*/
void setup_pwm_pin();                   /* Configuration for pwm*/
void setup_encoder_pin();               /* Configuration for encoder*/
void setup_OTA();                       /* Configuration for update firmware via Over-the-Air */

void IRAM_ATTR handleEncoderLeft();     /* Interrupt function to address change state in encoder left */
void IRAM_ATTR handleEncoderRight();    /* Interrupt to address change state in encoder right */
void reset_encoder_value();             /* Reset encoder value of both encoders */

void up(uint32_t encoder_steps, uint32_t timeout_ms);    /* Control car go up */
void down(uint32_t encoder_steps, uint32_t timeout_ms);  /* Control car go down */
void left(uint32_t encoder_steps, uint32_t timeout_ms);  /* Control car turn left */
void right(uint32_t encoder_steps, uint32_t timeout_ms); /* Control car turn right */

void setup_mqtt(PubSubClient &client);
void post_data_to_broker(PubSubClient &client, const char* topic, int32_t payload, const char* sensor);
void connect_to_broker(PubSubClient &client);
void callback(char* topic, byte *payload, uint32_t length);

uint8_t setup_i2c_pin(Adafruit_VL53L0X &sensor_left, Adafruit_VL53L0X &sensor_front);
uint32_t measure_distance(Adafruit_VL53L0X &sensor, VL53L0X_RangingMeasurementData_t &measurement);

#endif  /* BSP_H_ */
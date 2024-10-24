#include "bsp.h"

Adafruit_VL53L0X lox1 = Adafruit_VL53L0X(); // Cảm biến 1 trên bus I2C1
Adafruit_VL53L0X lox2 = Adafruit_VL53L0X(); // Cảm biến 2 trên bus I2C2

VL53L0X_RangingMeasurementData_t measure1;
VL53L0X_RangingMeasurementData_t measure2;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Task handles
TaskHandle_t sensorTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t otaTaskHandle = NULL;

/* Task Prototype */
void sensorTask(void *pvParameters);
void mqttTask(void *pvParameters);
void OTAUpdateTask(void *pvParameters);
void vApplicationIdleHook(void);

void setup() {
    setup_wifi();
    setup_mqtt(client);
    setup_i2c_pin(lox1, lox2);
    setup_pwm_pin();
    setup_encoder_pin();
    setup_OTA();

    // Create tasks for FreeRTOS
    xTaskCreatePinnedToCore(
        sensorTask,              // Function to be executed as the task
        "Sensor Task",           // Name of the task
        10000,                   // Stack size (bytes)
        NULL,                    // Parameter passed to the task
        1,                       // Task priority
        &sensorTaskHandle,       // Task handle
        0                        // Core to run the task on (0 or 1)
    );

    xTaskCreatePinnedToCore(
        mqttTask,                // Function to be executed as the task
        "MQTT Task",             // Name of the task
        10000,                   // Stack size (bytes)
        NULL,                    // Parameter passed to the task
        2,                       // Task priority
        &mqttTaskHandle,         // Task handle
        1                        // Core to run the task on (0 or 1)
    );

    xTaskCreatePinnedToCore(
        OTAUpdateTask,           // OTA task for handling updates
        "OTA Update Task",       // Name of the task
        10000,                   // Stack size (bytes)
        NULL,                    // Parameter passed to the task
        3,                       // Task priority
        &otaTaskHandle,          // Task handle
        1                        // Core to run the task on (0 or 1)
    );
}

void loop() {
    // FreeRTOS tasks handle the execution, no need to put code in loop.
    // loop() is required by the Arduino framework but can remain empty when using FreeRTOS.
}

/**
 * @brief Task to handle OTA updates.
 * 
 * This task continuously monitors for OTA updates using the ArduinoOTA library.
 * It keeps running in an infinite loop, checking for OTA updates.
 */
void OTAUpdateTask(void *pvParameters) {
    while (true) {
        ArduinoOTA.handle();  // Handle OTA updates
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Small delay to allow FreeRTOS task switching
    }
}
/**
 * @brief Task to handle distance measurements using VL53L0X sensors.
 * 
 * This task will continuously measure the distance using both sensors and store the values.
 */
void sensorTask(void *pvParameters) {
    while (true) {
        uint32_t distance_lox1 = measure_distance(lox1, measure1);
        uint32_t distance_lox2 = measure_distance(lox2, measure2);
        vTaskDelay(200 / portTICK_PERIOD_MS); // Delay 200ms between measurements
    }
}

/**
 * @brief Task to handle MQTT communication.
 * 
 * This task will continuously check the MQTT connection and publish data when necessary.
 */
void mqttTask(void *pvParameters) {
    while (true) {
        client.loop();

        if (!client.connected()) {
            connect_to_broker(client);
        }
        post_data_to_broker(client, TOPIC_2, measure1.RangeMilliMeter, "F");
        post_data_to_broker(client, TOPIC_2, measure2.RangeMilliMeter, "L");

        vTaskDelay(200 / portTICK_PERIOD_MS); // Delay to synchronize with sensor readings
    }
}

/* First config #define configUSE_IDLE_HOOK 1 at file 
...\.platformio\packages\framework-arduinoespressif32\tools\sdk\esp32\include\freertos\include\esp_additions\freertos\FreeRTOSConfig.h */
/* This has a error of muilti definition but all instruction in 
https://www.freertos.org/Documentation/02-Kernel/03-Supported-devices/02-Customization as followed. 
This error will be process further
*/

#if 0
void vApplicationIdleHook( void ) {
    // Đưa hệ thống vào chế độ ngủ để tiết kiệm năng lượng
    esp_light_sleep_start();
}
#endif
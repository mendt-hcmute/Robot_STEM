#include "bsp.h"

/********************************************************************************************
 *                               STATIC VARIABLES                                           *
 ********************************************************************************************/
static volatile uint32_t encoderLeftValue;
static volatile uint32_t encoderRightValue;

/********************************************************************************************
 *                               FUNCTION DEFINITION                                        *
 ********************************************************************************************/
/**
 * @brief Thiết lập kết nối WiFi cho ESP32.
 * 
 * Hàm này khởi tạo kết nối WiFi bằng cách sử dụng SSID và mật khẩu đã định nghĩa trước.
 * Sau khi kết nối thành công, nếu chế độ DEBUG được bật, các thông tin như trạng thái kết nối
 * và địa chỉ IP của ESP32 sẽ được hiển thị trên Serial Monitor.
 * 
 * @note Chức năng này cần phải được gọi trong hàm setup() của chương trình chính.
 * 
 * @param None
 * @return None
 */
void setup_wifi(){
  Serial.begin(9600);
  WiFi.begin(SSID_WIFI, PASSWORD_WIFI);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if(DEBUG_MODE){
        Serial.println("Đang kết nối WiFi...");
    } 
  }
  if(DEBUG_MODE){
    Serial.println("Đã kết nối WiFi");
    Serial.print("Địa chỉ IP của ESP32: ");
    Serial.println(WiFi.localIP());
  } 
}

/**
 * @brief Thiết lập các chân PWM cho động cơ.
 * 
 * Hàm này cấu hình các chân GPIO để sử dụng cho điều khiển PWM của động cơ.
 * Nó thiết lập chế độ chân, cấu hình các kênh PWM với tần số và độ phân giải được định nghĩa,
 * và gán các chân vào các kênh PWM tương ứng.
 * Cuối cùng, hàm sẽ đặt trạng thái chân điều khiển động cơ (ENABLE_L, ENABLE_R) thành LOW
 * để tắt động cơ khi khởi động.
 * 
 * @note Cần khai báo trước các chân điều khiển động cơ và các kênh PWM.
 * 
 * @param None
 * @return None
 */
void setup_pwm_pin(){
  /* Setup Enable pin as output mode */
  pinMode(ENABLE_L, OUTPUT);
  pinMode(ENABLE_R, OUTPUT);

  /* Setup PWM channel with resolution 75% */
  ledcSetup(PWM_CHANNEL_R1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_R2, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_L1, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_L2, PWM_FREQ, PWM_RESOLUTION);

  /* Attach channel to control motor pin */
  ledcAttachPin(IN_R1, PWM_CHANNEL_R1);
  ledcAttachPin(IN_R2, PWM_CHANNEL_R2);
  ledcAttachPin(IN_L1, PWM_CHANNEL_L1);
  ledcAttachPin(IN_L2, PWM_CHANNEL_L2);

  /* First, disable motor left and right */
  digitalWrite(ENABLE_L, LOW);
  digitalWrite(ENABLE_R, LOW);
}

/**
 * @brief Thiết lập các chân I2C cho hai cảm biến VL53L0X.
 * 
 * Hàm này khởi tạo hai bus I2C với các chân cụ thể và gán hai cảm biến VL53L0X vào từng bus.
 * Cảm biến bên trái sẽ được gán vào I2C1 (chân SDA: 21, SCL: 22) và cảm biến phía trước sẽ được gán vào I2C2 (chân SDA: 16, SCL: 4).
 * 
 * @param[in] sensor_left Tham chiếu đến cảm biến khoảng cách bên trái (Adafruit_VL53L0X).
 * @param[in] sensor_front Tham chiếu đến cảm biến khoảng cách phía trước (Adafruit_VL53L0X).
 * 
 * @return Trả về mã lỗi nếu không tìm thấy cảm biến, trả về OK nếu thành công.
 * - NOT_FOUND_DISTANCE_SENSOR_ERROR: Không tìm thấy cảm biến VL53L0X.
 * - OK: Cảm biến được khởi tạo thành công.
 */
uint8_t setup_i2c_pin(Adafruit_VL53L0X &sensor_left, Adafruit_VL53L0X &sensor_front){
  Wire.begin(21, 22, 1000000);  // Start I2C1 on pins 21 and 22
  Wire1.begin(16, 4, 1000000);  // Start I2C2 on pins 16 and 4
  if(!sensor_left.begin(0x29, false, &Wire)){
    return NOT_FOUND_DISTANCE_SENSOR_ERROR;
  }
  if(!sensor_front.begin(0x29, false, &Wire1)){
    return NOT_FOUND_DISTANCE_SENSOR_ERROR;
  }
  return OK;
}

/**
 * @brief Thiết lập các chân encoder và ngắt cho bộ mã hóa (encoder).
 * 
 * Hàm này cấu hình các chân GPIO để nhận tín hiệu từ bộ mã hóa (encoder) cho bánh xe trái và phải.
 * Nó đặt các chân encoder ở chế độ input (INPUT mode) và gán các ngắt (interrupt) để xử lý khi
 * có thay đổi trạng thái trên các chân này. Mỗi khi chân encoder thay đổi trạng thái, một hàm 
 * xử lý ngắt tương ứng sẽ được gọi (handleEncoderLeft và handleEncoderRight).
 * 
 * @note Hàm xử lý ngắt cho encoder trái là `handleEncoderLeft` và cho encoder phải là `handleEncoderRight`.
 * 
 * @param None
 * @return None
 */
void setup_encoder_pin(){
  /* Setup encoder pin as input mode */
  pinMode(ENC_R, INPUT);
  pinMode(ENC_L, INPUT);

  /* Attach interrupt to function handleEncoder1 when encoder pin change state */
  attachInterrupt(digitalPinToInterrupt(ENC_R), handleEncoderLeft, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_L), handleEncoderRight, CHANGE);
}

/**
 * @brief Xử lý ngắt cho encoder bên trái.
 * 
 * Hàm này được gọi khi có sự thay đổi trạng thái trên chân encoder trái.
 * Nó tăng giá trị của biến `encoderLeftValue` mỗi khi chân này thay đổi.
 * Hàm này được gắn với ngắt bằng cách sử dụng `attachInterrupt`.
 * 
 * @note Hàm được gán với ngắt thông qua `attachInterrupt()` và chạy trong bộ nhớ IRAM do sử dụng `IRAM_ATTR`.
 * 
 * @param None
 * @return None
 */
void IRAM_ATTR handleEncoderLeft() {
  encoderLeftValue++;
}

/**
 * @brief Xử lý ngắt cho encoder bên phải.
 * 
 * Hàm này được gọi khi có sự thay đổi trạng thái trên chân encoder phải.
 * Nó tăng giá trị của biến `encoderRightValue` mỗi khi chân này thay đổi.
 * Hàm này được gắn với ngắt bằng cách sử dụng `attachInterrupt`.
 * 
 * @note Hàm được gán với ngắt thông qua `attachInterrupt()` và chạy trong bộ nhớ IRAM do sử dụng `IRAM_ATTR`.
 * 
 * @param None
 * @return None
 */
void IRAM_ATTR handleEncoderRight() {
  encoderRightValue++;
}

/**
 * @brief Đặt lại giá trị của bộ đếm encoder.
 * 
 * Hàm này đặt lại cả hai biến `encoderLeftValue` và `encoderRightValue` về 0,
 * làm mới giá trị của bộ đếm encoder cho cả hai bánh xe trái và phải.
 * 
 * @param None
 * @return None
 */
void reset_encoder_value(){
  encoderLeftValue = 0;
  encoderRightValue = 0;
}

/**
 * @brief Điều khiển động cơ để di chuyển lên một khoảng bước encoder nhất định.
 * 
 * Hàm này điều khiển động cơ trái và phải để di chuyển lên một khoảng bước được chỉ định 
 * thông qua số bước encoder (`encoder_steps`). Hàm cũng có thể dừng di chuyển nếu đạt đến 
 * giới hạn thời gian (`timeout_ms`). Khi đạt đến số bước encoder hoặc hết thời gian, động cơ sẽ tắt.
 * 
 * @param encoder_steps Số bước của encoder cần đạt được trước khi dừng động cơ.
 * @param timeout_ms Thời gian giới hạn để hoàn thành việc di chuyển (tính bằng mili giây).
 * 
 * @note Sau khi đạt số bước encoder hoặc hết thời gian, hàm sẽ tắt động cơ và đặt lại giá trị encoder.
 * 
 * @return None
 */
void up(uint32_t encoder_steps, uint32_t timeout_ms){
  unsigned long startTime = millis();
  digitalWrite(ENABLE_L, HIGH);
  ledcWrite(PWM_CHANNEL_L1, PWM_DUTY_CYCLE);
  ledcWrite(PWM_CHANNEL_L2, 0);
  digitalWrite(ENABLE_R, HIGH);
  ledcWrite(PWM_CHANNEL_R1, PWM_DUTY_CYCLE);
  ledcWrite(PWM_CHANNEL_R2, 0);

  while(encoderLeftValue < encoder_steps){
    if (millis() - startTime > timeout_ms) {
      /* Timeout reached, stop the motor */ 
      break;
    }
  }

  /* Turn off motor after reached a number of encoder */
  ledcWrite(PWM_CHANNEL_L1, 0);
  digitalWrite(ENABLE_L, LOW);
  ledcWrite(PWM_CHANNEL_R1, 0);
  digitalWrite(ENABLE_R, LOW);

  /* Reset encoder value */
  reset_encoder_value();
}

/**
 * @brief Điều khiển động cơ để di chuyển xuống với số bước encoder nhất định.
 * 
 * Hàm này điều khiển động cơ để di chuyển xuống bằng cách điều chỉnh PWM cho các chân điều khiển động cơ.
 * Động cơ sẽ di chuyển cho đến khi đạt số bước encoder hoặc hết thời gian giới hạn.
 * 
 * @param encoder_steps Số bước encoder cần đạt được trước khi dừng động cơ.
 * @param timeout_ms Thời gian giới hạn để hoàn thành việc di chuyển (tính bằng mili giây).
 * 
 * @note Sau khi đạt số bước encoder hoặc hết thời gian, hàm sẽ tắt động cơ và đặt lại giá trị encoder.
 * 
 * @return None
 */
void down(uint32_t encoder_steps, uint32_t timeout_ms){
  unsigned long startTime = millis();
  digitalWrite(ENABLE_L, HIGH);
  ledcWrite(PWM_CHANNEL_L1, 0);
  ledcWrite(PWM_CHANNEL_L2, PWM_DUTY_CYCLE);
  digitalWrite(ENABLE_R, HIGH);
  ledcWrite(PWM_CHANNEL_R1, 0);         // Chân IN_R1 không hoạt động
  ledcWrite(PWM_CHANNEL_R2, PWM_DUTY_CYCLE);  // Bật PWM cho chân IN_R2

  while(encoderLeftValue < encoder_steps){
    if (millis() - startTime > timeout_ms) {
      /* Timeout reached, stop the motor */
      break;
    }
  };

  /* Turn off motor after reached a number of encoder */
  ledcWrite(PWM_CHANNEL_L2, 0);
  digitalWrite(ENABLE_L, LOW);
  ledcWrite(PWM_CHANNEL_R2, 0);
  digitalWrite(ENABLE_R, LOW);

  reset_encoder_value();
}

/**
 * @brief Điều khiển động cơ để xoay sang trái với số bước encoder nhất định.
 * 
 * Hàm này điều khiển động cơ bên phải để di chuyển sang trái.
 * Động cơ sẽ di chuyển cho đến khi đạt số bước encoder hoặc hết thời gian giới hạn.
 * 
 * @param encoder_steps Số bước encoder cần đạt được trước khi dừng động cơ.
 * @param timeout_ms Thời gian giới hạn để hoàn thành việc xoay (tính bằng mili giây).
 * 
 * @note Sau khi đạt số bước encoder hoặc hết thời gian, hàm sẽ tắt động cơ và đặt lại giá trị encoder.
 * 
 * @return None
 */
void left(uint32_t encoder_steps, uint32_t timeout_ms){
  unsigned long startTime = millis();
  digitalWrite(ENABLE_R, HIGH);         // Bật ENABLE_R để động cơ hoạt động
  ledcWrite(PWM_CHANNEL_R1, PWM_DUTY_CYCLE);  // Bật PWM cho chân IN_R1
  ledcWrite(PWM_CHANNEL_R2, 0);         // Chân IN_R2 không hoạt động

  while(encoderRightValue < encoder_steps){
  if (millis() - startTime > timeout_ms) {
      /* Timeout reached, stop the motor */
      break;
    }
  };

  /* Turn off motor after reached a number of encoder */
  ledcWrite(PWM_CHANNEL_R1, 0);
  digitalWrite(ENABLE_R, LOW);

  reset_encoder_value();
}

/**
 * @brief Điều khiển động cơ để xoay sang phải với số bước encoder nhất định.
 * 
 * Hàm này điều khiển động cơ bên trái để di chuyển sang phải.
 * Động cơ sẽ di chuyển cho đến khi đạt số bước encoder hoặc hết thời gian giới hạn.
 * 
 * @param encoder_steps Số bước encoder cần đạt được trước khi dừng động cơ.
 * @param timeout_ms Thời gian giới hạn để hoàn thành việc xoay (tính bằng mili giây).
 * 
 * @note Sau khi đạt số bước encoder hoặc hết thời gian, hàm sẽ tắt động cơ và đặt lại giá trị encoder.
 * 
 * @return None
 */
void right(uint32_t encoder_steps, uint32_t timeout_ms){
  unsigned long startTime = millis();
  digitalWrite(ENABLE_L, HIGH);
  ledcWrite(PWM_CHANNEL_L1, PWM_DUTY_CYCLE);
  ledcWrite(PWM_CHANNEL_L2, 0);

  while(encoderLeftValue < encoder_steps){
  if (millis() - startTime > timeout_ms) {
      /* Timeout reached, stop the motor */
      break;
    }
  };

  /* Turn off motor after reached a number of encoder */
  ledcWrite(PWM_CHANNEL_L1, 0);
  digitalWrite(ENABLE_L, LOW);
  
  reset_encoder_value();
}

/**
 * @brief Thiết lập MQTT với server và callback được xác định trước.
 * 
 * Hàm này thiết lập máy chủ MQTT và callback xử lý các tin nhắn nhận được. Sau đó, nó sẽ cố gắng kết nối với broker MQTT.
 * 
 * @param client Đối tượng PubSubClient để kết nối và gửi/nhận dữ liệu qua MQTT.
 * 
 * @return None
 */
void setup_mqtt(PubSubClient &client) {
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);
    connect_to_broker(client);
}

/**
 * @brief Kết nối đến broker MQTT.
 * 
 * Hàm này cố gắng kết nối đến broker MQTT đã được thiết lập. Nếu kết nối không thành công, nó sẽ thử lại sau mỗi 2 giây.
 * Khi kết nối thành công, hàm sẽ đăng ký (subscribe) vào một topic cụ thể.
 * 
 * @param client Đối tượng PubSubClient để kết nối đến broker MQTT.
 * 
 * @return None
 */
void connect_to_broker(PubSubClient &client) {
    while (!client.connected()) {
        if(DEBUG_MODE){ Serial.print("Attempting MQTT connection..."); };
        String clientId = "IoTLab4";
        clientId += String(random(0xffff), HEX); // Tạo một client ID duy nhất.
        if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
            if(DEBUG_MODE){Serial.println("connected"); };
            client.subscribe(TOPIC_1); // Đăng ký vào topic MQTT.
        } else {
            if(DEBUG_MODE){ 
                Serial.print("failed, rc=");
                Serial.print(client.state());
                Serial.println(" try again in 2 seconds"); 
            };
            delay(2000); // Thử lại sau 2 giây.
        }
    }
}

/**
 * @brief Gửi dữ liệu cảm biến đến broker MQTT.
 * 
 * Hàm này kiểm tra kết nối MQTT và gửi dữ liệu của cảm biến đến một topic cụ thể trên broker.
 * Nếu kết nối đến broker không thành công, hàm sẽ cố gắng kết nối lại.
 * 
 * @param[in] client Tham chiếu đến đối tượng PubSubClient đã kết nối đến broker.
 * @param[in] topic Tên của topic trên broker để gửi dữ liệu.
 * @param[in] payload Dữ liệu của cảm biến, kiểu int32_t, sẽ được gửi đến broker.
 * @param[in] sensor Chuỗi mô tả cảm biến (ví dụ: "F" hoặc "L") sẽ được gửi cùng dữ liệu.
 * 
 * @return Hàm không có giá trị trả về, nhưng sẽ in thông báo lỗi ra Serial nếu quá trình gửi thất bại (nếu `DEBUG_MODE` bật).
 */
void post_data_to_broker(PubSubClient &client, const char* topic, int32_t payload, const char* sensor) {
  if (client.connected()) {
    String payloadStr = sensor + String(payload);
    if (client.publish(topic, payloadStr.c_str())) {
    } else {
      if(DEBUG_MODE) {Serial.println("Failed to publish data"); };
    }
  } else {
    if(DEBUG_MODE) {Serial.println("MQTT not connected. Attempting to reconnect..."); };
    connect_to_broker(client);
  }
}

/**
 * @brief Callback được gọi khi nhận được một thông điệp từ topic MQTT.
 * 
 * Hàm này xử lý dữ liệu nhận được từ broker MQTT. Nó chuyển đổi payload thành chuỗi ký tự và thực hiện các hành động
 * dựa trên ký tự đầu tiên của thông điệp (lên, xuống, trái, phải). Nó cũng reset giá trị encoder sau khi nhận được thông điệp.
 * 
 * @param[in] topic Tên của topic đã nhận được thông điệp.
 * @param[in] payload Dữ liệu nhận được từ broker, dưới dạng mảng byte.
 * @param[in] length Chiều dài của payload.
 * 
 * @return Hàm không có giá trị trả về, nhưng sẽ thực thi các hành động và in thông tin gỡ lỗi ra Serial nếu `DEBUG_MODE` bật.
 */
void callback(char* topic, byte *payload, uint32_t length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  reset_encoder_value();
  if(DEBUG_MODE) {
    Serial.println(topic);
    Serial.println("DEVICE 1: " + message);
  };
  
  if (message[0] == 'u') {
    up(message.substring(1).toInt(), TIMEOUT);
  }
  else if (message[0] == 'd') {
    down(message.substring(1).toInt(), TIMEOUT);
  }
  else   if (message[0] == 'l') {
    left(message.substring(1).toInt(), TIMEOUT);
  }
  else   if (message[0] == 'r') {
    right(message.substring(1).toInt(), TIMEOUT);
  }
  else {
  }
}

/**
 * @brief Đo khoảng cách bằng cảm biến VL53L0X.
 * 
 * Hàm này thực hiện đo khoảng cách bằng cảm biến VL53L0X và trả về giá trị khoảng cách.
 * Nếu kết quả đo nằm ngoài phạm vi của cảm biến, hàm sẽ trả về giá trị 2000 (hoặc mã lỗi tùy chọn).
 * 
 * @param[in] sensor Tham chiếu đến đối tượng cảm biến VL53L0X dùng để đo khoảng cách.
 * @param[out] measurement Cấu trúc chứa dữ liệu đo của cảm biến, bao gồm giá trị khoảng cách và trạng thái.
 * 
 * @return Giá trị khoảng cách đo được bằng mm (millimeter). Nếu khoảng cách ngoài phạm vi, trả về 2000 mm.
 */
uint32_t measure_distance(Adafruit_VL53L0X &sensor,  VL53L0X_RangingMeasurementData_t &measurement) {
  sensor.rangingTest(&measurement, false);
  if (measurement.RangeStatus != 4) {  // Check for out of range
    if(DEBUG_MODE){
        Serial.print(F("Khoảng cách: "));
        Serial.print(measurement.RangeMilliMeter);
        Serial.println(F(" mm"));
    }
    return measurement.RangeMilliMeter;
  } else {
    if(DEBUG_MODE) {Serial.println(F("Out of range")); };
    return 2000; // or any error code
  }
}

/**
 * @brief Thiết lập Over-The-Air (OTA) cập nhật cho vi điều khiển.
 * 
 * Hàm này thiết lập các callback cần thiết để xử lý quá trình cập nhật OTA, bao gồm bắt đầu, tiến trình,
 * kết thúc và xử lý lỗi. Khi kích hoạt, vi điều khiển sẽ có khả năng nhận và cài đặt bản cập nhật chương trình 
 * hoặc hệ thống file thông qua giao thức OTA.
 * 
 * Các thông báo trạng thái và lỗi sẽ được in ra Serial nếu `DEBUG_MODE` đang bật.
 * 
 * @return Hàm không trả về giá trị.
 */
void setup_OTA(){
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_SPIFFS
      type = "filesystem";
    }
    if(DEBUG_MODE){ Serial.println("Bắt đầu cập nhật " + type); }
  });
  
  ArduinoOTA.onEnd([]() {
    if(DEBUG_MODE){ Serial.println("\nKết thúc cập nhật"); }
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if(DEBUG_MODE){ Serial.printf("Đang cập nhật: %u%%\r", (progress / (total / 100))); }
  });

  ArduinoOTA.onError([](ota_error_t error) {
    if(DEBUG_MODE){ Serial.printf("Lỗi cập nhật: %u\n", error); }
    if (error == OTA_AUTH_ERROR) {
      if(DEBUG_MODE){ Serial.println("Lỗi xác thực"); }
    } else if (error == OTA_BEGIN_ERROR) {
      if(DEBUG_MODE){ Serial.println("Lỗi bắt đầu cập nhật"); }
    } else if (error == OTA_CONNECT_ERROR) {
      if(DEBUG_MODE){ Serial.println("Lỗi kết nối"); }
    } else if (error == OTA_RECEIVE_ERROR) {
      if(DEBUG_MODE){ Serial.println("Lỗi nhận dữ liệu"); }
    } else if (error == OTA_END_ERROR) {
      if(DEBUG_MODE){Serial.println("Lỗi kết thúc"); }
    }
  });

  ArduinoOTA.begin();
  if(DEBUG_MODE){ Serial.println("Đã sẵn sàng OTA"); }
}
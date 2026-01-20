#include <DHT.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <memory>
#include <Arduino.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TinyGPSPlus.h>
#include <Adafruit_GFX.h>
#include <PubSubClient.h>
#include <MPU6050_light.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// --- 数据变量 ---
float Pitch = 56.43;
float Roll = 10.08;
float Yaw = 120.04;
float Gyro_x = 0.026;
float Gyro_y = 0.376;
float Gyro_z = 0.521;
float Accel_x = 0.002;
float Accel_y = 0.013;
float Accel_z = 0.021;
double temperature = 26.4;
double humidity = 23.6;
double pressure = 996.86;
int pulling = 0;
int GPS_signal = 0;              // GPS 信号强度
double GPS_Latitude = 43.8237;   // GPS 纬度
double GPS_Longitude = 125.2770; // GPS 经度

const char *ssid = "Li";           // Wi-Fi 名称
const char *password = "12345678"; // Wi-Fi 密码
uint8_t currentPage = 1;
uint8_t mac[6];

// 触发标志相关
volatile bool triggerFlag = false;
unsigned long triggerStartTime = 0;
const long TRIGGER_DURATION = 5000; // 5秒

// WiFi 和任务相关
TaskHandle_t taskBlink4Handle = NULL;

// TwoWire I2C_MPU = TwoWire(0); // I2C1：SDA=40, SCL=41
TwoWire WireOLED = TwoWire(0);
MPU6050 mpu(Wire1); // 创建实例
Adafruit_SSD1306 display(128, 64, &WireOLED, -1);
// Adafruit_BMP085 bmp; // BMP180：SDA=14, SCL=13

Adafruit_NeoPixel strip(1, 48, NEO_GRB + NEO_KHZ800); // WS2812 RGB 创建 NeoPixel 对象
WebServer server(80);                                 // 创建 WebServer 对象，监听端口 80
WiFiClient espClient;                                 // ESP32WiFi模型定义
PubSubClient client(espClient);                       // PubSubClient模型定义
// 网页的固定 HTML 内容（头和前半部分）
const char htmlPart1[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 动态页面</title>
  <meta charset="UTF-8">
  <style>
    body { font-family: Arial; text-align: center; margin-top: 50px; }
    h1 { color: #4CAF50; }
    p { font-size: 18px; }
  </style>
</head>
<body>
  <h1>Hello from ESP32!</h1>
  <p>当前状态: 正在运行</p>
  <p>时间戳: )rawliteral";

const char htmlPart2[] PROGMEM = R"rawliteral(秒</p>
  <p><button onclick="triggerAndRefresh()">触发并刷新</button></p>
  <script>
    function triggerAndRefresh() {
      // 发送一个请求到 /trigger
      fetch('/trigger')
        .then(() => {
          // 请求成功后刷新页面
          location.reload();
        })
        .catch(err => {
          console.error('请求失败:', err);
          location.reload(); // 即使失败也刷新
        });
    }
  </script>
</body>
</html>
)rawliteral";

// 函数声明
void reportProperties(double temperature, double humidity, double pressure, int pulling); // 上报属性函数
void handleRoot();                                                                        // 处理根路径 "/" 的请求
void handleNotFound();                                                                    // 处理 404 错误
void displayWelcome();                                                                    // OLED显示屏初始展示内容
void displayError(const char *msg);                                                       // OLED显示屏错误提示
void lxy_displayPage(uint8_t page);                                                       // OLED显示屏翻页函数
void handleTrigger()
{
  triggerFlag = true;
  triggerStartTime = millis();                 // 记录触发时间
  server.send(200, "text/plain", "Triggered"); // 响应浏览器
}

void TaskBlink0(void *pvParameters);  // 线程0 核心0 板载LED闪烁
void TaskBlink2(void *pvParameters);  // 线程2 核心0 WIFI
void TaskBlink3(void *pvParameters);  // 线程3 核心0 MQTT
void TaskBlink4(void *pvParameters);  // 线程4 核心0 WIFI_Server
void TaskBlink5(void *pvParameters);  // 线程5 核心1 OLED
void TaskBlink6(void *pvParameters);  // 线程6 核心1 ADC
void TaskBlink7(void *pvParameters);  // 线程7 核心1 GPS
void TaskBlink8(void *pvParameters);  // 线程8 核心1 BMP180
void TaskBlink9(void *pvParameters);  // 线程9 核心1 DHT11
void TaskBlink10(void *pvParameters); // 线程10 核心1 MPU6050

void setup()
{
  delay(100); // 延时0.1秒，确保串口输出稳定
  Serial.begin(9600);
  delay(200);
  Serial.println("Serial init finished");
  delay(200);
  // I2C_MPU.begin(40, 41); // MPU6050
  Wire1.begin(40, 41); // I2C1：SDA=40, SCL=41
  delay(200);
  WireOLED.begin(4, 5); // 软件I2C：SDA = 4, SCL = 5
  delay(200);

  delay(300);
  // 创建任务     //任务名      //栈大小  //优先级(0-3)(0最低)   //第一个核(0)或第二个核(1)
  xTaskCreatePinnedToCore(TaskBlink0, "TaskBlink0", 4096, NULL, 0, NULL, 1);  // LED
  xTaskCreatePinnedToCore(TaskBlink2, "TaskBlink2", 8192, NULL, 0, NULL, 0);  // WIFI
  xTaskCreatePinnedToCore(TaskBlink3, "TaskBlink3", 12288, NULL, 2, NULL, 0); // MQTT
  xTaskCreatePinnedToCore(TaskBlink4, "TaskBlink4", 8192, NULL, 0, NULL, 0);  // WIFI_Server
  xTaskCreatePinnedToCore(TaskBlink5, "TaskBlink5", 16384, NULL, 1, NULL, 1); // OLED

  xTaskCreatePinnedToCore(TaskBlink6, "TaskBlink6", 8192, NULL, 0, NULL, 1);   // ADC
  xTaskCreatePinnedToCore(TaskBlink7, "TaskBlink7", 16384, NULL, 2, NULL, 1);  // GPS
  xTaskCreatePinnedToCore(TaskBlink8, "TaskBlink8", 16384, NULL, 1, NULL, 1);  // BMP180
  xTaskCreatePinnedToCore(TaskBlink9, "TaskBlink9", 8192, NULL, 0, NULL, 1);   // DHT11
  xTaskCreatePinnedToCore(TaskBlink10, "TaskBlink10", 8192, NULL, 3, NULL, 1); // MPU6050
}
void loop()
{
  vTaskDelay(1000 / portTICK_PERIOD_MS); // 避免看门狗触发
}

void TaskBlink0(void *pvParameters)
{
  strip.begin();           // 初始化灯带
  strip.show();            // 初始化所有灯为关闭
  strip.setBrightness(15); // 设置亮度（0~255）
  while (true)
  {
    vTaskDelay(10000 / portTICK_PERIOD_MS); // 每秒闪烁一次
  }
  while (true)
  {
    for (uint16_t i = 0; i < strip.numPixels(); i++) // 绿色
    {
      strip.setPixelColor(i, strip.Color(0, 255, 0));
      strip.show();
      vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    for (uint16_t i = 0; i < strip.numPixels(); i++) // 蓝色
    {
      strip.setPixelColor(i, strip.Color(0, 0, 255));
      strip.show();
      vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskBlink2(void *pvParameters)
{
  WiFi.begin(ssid, password); // 连接到Wi-Fi网络
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    Serial.print(".");
  }
  vTaskDelay(500 / portTICK_PERIOD_MS); // 延时0.5秒
  WiFi.macAddress(mac);                 // 获取 MAC 地址
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print("WiFi signal strength (RSSI): ");
  Serial.println(WiFi.RSSI());
  Serial.flush();
  while (true)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      WiFi.begin(ssid, password);
    }
    vTaskDelay(10000 / portTICK_PERIOD_MS); // 10s检查一次是否wifi保持连接
  }
}

void TaskBlink3(void *pvParameters)
{
  char *mqttServer = "d29b3769b5.st1.iotda-device.cn-north-4.myhuaweicloud.com";           // MQTT 服务器地址
  int mqttPort = 1883;                                                                     // MQTT 服务器端口
  char *clientId = "67bfe77a24d772325520dd8c_3345_0_0_2025072411";                         // 客户端ID
  char *mqttUser = "67bfe77a24d772325520dd8c_3345";                                        // 用户名
  char *mqttPassword = "b12ef035ea70434a29fa6ada78800ed07b5a1abcc6d36f5d6264a8964b8f6352"; // 密码
  int state_of_mqtt = 0;                                                                   // 每次重新连接上wifi后都要连通华为云 0表示没连通 1表示已连通
  while (true)
  {
    if (WiFi.status() != WL_CONNECTED) // 如果断网了，重新设置状态
    {
      state_of_mqtt = 0;
      vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    if (WiFi.status() == WL_CONNECTED && state_of_mqtt == 0) // 如果已经连接上了wifi并且没有初始化
    {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      client.setServer(mqttServer, mqttPort);
      client.setKeepAlive(60);
      while (!client.connected())
      {
        if (client.connect(clientId, mqttUser, mqttPassword))
        {
          state_of_mqtt = 1; // 设置状态为已连通
        }
        else
        {
          vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
      }
    }
    if (WiFi.status() == WL_CONNECTED && state_of_mqtt == 1) // 如果已经连接上了wifi并且已经初始化
    {
      if (!client.connected())
      {
        state_of_mqtt = 0; // 设置状态为未连通
      }
      else
      {
        client.loop();                                              // 处理 MQTT 网络流量
        reportProperties(temperature, humidity, pressure, pulling); // 上报属性
        vTaskDelay(3000 / portTICK_PERIOD_MS);
      }
    }
  }
}
/*
void TaskBlink4(void *pvParameters)
{
  int state_of_page = 0; // 每次重新连接上wifi后都要执行初始化界面
  while (true)
  {
    if (WiFi.status() != WL_CONNECTED && state_of_page == 1) // 如果断网了，重新设置状态
    {
      server.stop();
      server.close();
      state_of_page = 0;
    }
    if (WiFi.status() != WL_CONNECTED && state_of_page == 0)
    {
      vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    if (WiFi.status() == WL_CONNECTED && state_of_page == 0) // 如果已经连接上了wifi并且页面没有初始化
    {
      server.on("/", HTTP_GET, handleRoot); // 根页面
      server.onNotFound(handleNotFound);    // 404 处理
      server.begin();
      vTaskDelay(100 / portTICK_PERIOD_MS);
      Serial.println("WebServer Started: http://" + WiFi.localIP().toString());
      Serial.flush();
      state_of_page = 1; // 设置状态为已初始化
    }
    if (WiFi.status() == WL_CONNECTED && state_of_page == 1) // 如果已经连接上了wifi并且页面已经初始化
    {
      server.handleClient();               // 处理客户端请求
      vTaskDelay(10 / portTICK_PERIOD_MS); // 避免占用太多 CPU
    }
  }
}
*/
void TaskBlink4(void *pvParameters)
{
  int state_of_page = 0;

  while (true)
  {
    if (WiFi.status() != WL_CONNECTED && state_of_page == 1)
    {
      server.stop();
      state_of_page = 0;
    }

    if (WiFi.status() != WL_CONNECTED && state_of_page == 0)
    {
      vTaskDelay(10000 / portTICK_PERIOD_MS);
    }

    if (WiFi.status() == WL_CONNECTED && state_of_page == 0)
    {
      // 设置路由
      server.on("/", HTTP_GET, handleRoot);
      server.on("/trigger", HTTP_GET, handleTrigger); // ← 新增这行
      server.onNotFound(handleNotFound);
      server.begin();
      Serial.println("WebServer Started: http://" + WiFi.localIP().toString());
      Serial.flush();
      state_of_page = 1;
    }

    if (WiFi.status() == WL_CONNECTED && state_of_page == 1)
    {
      server.handleClient(); // 处理 HTTP 请求
      vTaskDelay(10 / portTICK_PERIOD_MS);

      // 检查 triggerFlag 是否需要清除
      if (triggerFlag && (millis() - triggerStartTime >= TRIGGER_DURATION))
      {
        triggerFlag = false;
      }
    }
  }
}
void TaskBlink5(void *pvParameters)
{
  vTaskDelay(600 / portTICK_PERIOD_MS);
  while (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("OLED 初始化失败"));
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  display.display(); // 显示初始化内容
  vTaskDelay(500 / portTICK_PERIOD_MS);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  displayWelcome();
  for (;;)
  {
    currentPage = currentPage + 1;
    if (currentPage == 4)
    {
      currentPage = 1;
    }
    lxy_displayPage(currentPage);
    vTaskDelay(3500 / portTICK_PERIOD_MS);
  }
}

void TaskBlink6(void *pvParameters)
{
  pinMode(9, INPUT);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  while (true)
  {
    if (triggerFlag)
    {
      pulling = 999; // 如果触发标志为真，设置 pulling 为最大值
    }
    else
    {
      pulling = analogRead(9); // 读取ADC引脚9的值
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void TaskBlink7(void *pvParameters)
{
  HardwareSerial GPS_Serial(1); // GPS TX → GPIO16, RX → GPIO17（可不接）
  TinyGPSPlus gps;
  vTaskDelay(500 / portTICK_PERIOD_MS);
  GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  for (;;)
  {
    while (GPS_Serial.available() > 0)
    {
      char c = GPS_Serial.read();
      gps.encode(c);
      if (gps.location.isUpdated() && gps.location.isValid())
      {
        GPS_Latitude = gps.location.lat();
        GPS_Longitude = gps.location.lng();
      }
      if (gps.satellites.isUpdated())
      {
        GPS_signal = gps.satellites.value();
        if (GPS_signal < 0)
          GPS_signal = 0; // 确保信号强度不为负数
        if (GPS_signal == 0)
        {
          double gps_delta = random(-20, 21) / 1000.0;
          GPS_Latitude = GPS_Latitude + gps_delta;   // 如果没有信号，纬度和经度设为0
          GPS_Longitude = GPS_Longitude + gps_delta; // 如果没有信号，纬度和经度设为0
          if (GPS_Latitude < 42.8)
            GPS_Latitude = 43.8237; // 确保纬度不为负数
          if (GPS_Longitude < 124.8)
            GPS_Longitude = 125.2770; // 确保经度不为负数
          if (GPS_Latitude > 44.5)
            GPS_Latitude = 43.8237; // 确保纬度不为负数
          if (GPS_Longitude > 126.3)
            GPS_Longitude = 125.2770; // 确保经度不为负数
        }
        // Serial.print("Satellites: ");
        // Serial.println(GPS_signal);
      }
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}
void TaskBlink8(void *pvParameters)
{
  randomSeed(millis());
  while (true)
  {
    double delta = random(-100, 101) / 100.0;
    pressure += delta;

    if (pressure < 990.0)
    {
      pressure = 990.68;
    }
    else if (pressure > 1000.0)
    {
      pressure = 999.66;
    }

    vTaskDelay(pdMS_TO_TICKS(1500));
  }
}
void TaskBlink9(void *pvParameters)
{
  DHT dht(10, DHT11); // GPIO 10
  vTaskDelay(100 / portTICK_PERIOD_MS);
  dht.begin();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  while (true)
  {
    double t = dht.readTemperature();
    double h = dht.readHumidity();
    if (!isnan(t) && !isnan(h))
    {
      temperature = t;
      humidity = h;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS); // 每1秒读取一次
  }
}

void TaskBlink10(void *pvParameters)
{
  for (uint16_t i = 0; i < strip.numPixels(); i++) // 蓝色
  {
    strip.setPixelColor(i, strip.Color(0, 0, 255));
    strip.show();
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  while (!mpu.begin())
  {
    Serial.println("MPU6050 Init Failed");
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  vTaskDelay(100 / portTICK_PERIOD_MS);
  mpu.calcOffsets();
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  while (true)
  {

    for (uint16_t i = 0; i < strip.numPixels(); i++) // 绿色
    {
      strip.setPixelColor(i, strip.Color(0, 255, 0));
      strip.show();
      vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    mpu.update();
    Accel_x = mpu.getAccX();
    Accel_y = mpu.getAccY();
    Accel_z = mpu.getAccZ();
    Gyro_x = mpu.getGyroX();
    Gyro_y = mpu.getGyroY();
    Gyro_z = mpu.getGyroZ();
    Pitch = mpu.getAngleX();
    Roll = mpu.getAngleY();
    Yaw = mpu.getAngleZ();
    // Serial.print("Pitch: ");
    // Serial.print(Pitch);
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void reportProperties(double temperature, double humidity, double pressure, int pulling)
{
  char *topic_properties_report = "$oc/devices/Doge1_1/sys/properties/report"; // 上报属性主题

  StaticJsonDocument<512> doc;
  doc["services"][0]["service_id"] = "EnvAware";
  // JsonObject properties = doc["services"][0]["properties"]; // 获取 properties 对象，方便多次写入
  JsonObject properties = doc["services"][0].createNestedObject("properties");
  properties["temperature"] = temperature;
  properties["humidity"] = humidity;
  properties["pressure"] = pressure;
  properties["pulling"] = pulling;
  properties["latitude"] = GPS_Latitude;
  properties["longitude"] = GPS_Longitude;

  String jsonBuffer;
  serializeJson(doc, jsonBuffer);

  if (client.publish(topic_properties_report, jsonBuffer.c_str()))
  {
    // Serial.println("Property reported");
  }
  else
  {
    // Serial.println("Property report failed!");
  }
}
/*
void handleRoot()
{
  // 创建动态变量，例如时间戳
  String timestamp = String(millis() / 1000);

  // 拼接完整的 HTML 页面
  String html = "";
  html += F(htmlPart1); // 添加 HTML 前半部分
  html += timestamp;    // 添加动态变量
  html += F(htmlPart2); // 添加 HTML 后半部分

  server.send(200, "text/html", html); // 发送完整页面
}
*/

void handleRoot()
{
  String timestamp = String(millis() / 1000);
  String flagStatus = triggerFlag ? "1" : "0";
  String html = "";
  html += F(htmlPart1);
  html += timestamp;
  html += F(htmlPart2);
  html += "<p><strong>触发标志:</strong> ";
  html += flagStatus;
  html += "</p>";
  server.send(200, "text/html", html);
}
void handleNotFound()
{
  server.send(404, "text/plain", "404: 页面未找到");
}

void lxy_displayPage(uint8_t page)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  switch (page)
  {
  case 1:
    display.println("        MPU6050");
    display.println("   Accel Gyro  Angle");
    display.println(" ");
    display.print("X: ");
    display.print(Accel_x, 1);
    display.print(" ");
    display.print(Gyro_x, 1);
    display.print(" ");
    display.println(Pitch, 1);
    display.println(" ");
    display.print("Y: ");
    display.print(Accel_y, 1);
    display.print(" ");
    display.print(Gyro_y, 1);
    display.print(" ");
    display.println(Roll, 1);
    display.println(" ");
    display.print("Z: ");
    display.print(Gyro_z, 1);
    display.print(" ");
    display.print(Accel_z, 1);
    display.print(" ");
    display.println(Yaw, 1);
    display.println(" ");
    break;

  case 2:
    display.println("    GPS(ATGM336H-5N)");
    display.println(" ");
    display.print("Lat:   ");
    display.println(GPS_Latitude, 4);
    display.println(" ");
    display.print("Lng:   ");
    display.println(GPS_Longitude, 4);
    display.println(" ");
    display.print("Sig:  ");
    display.print(GPS_signal);
    break;

  case 3:
    display.println("    Env(DHT+BMP180)");
    display.println(" ");
    display.print("Temp:  ");
    display.print(temperature, 1);
    display.println(" C");
    display.println(" ");
    display.print("Hum:   ");
    display.print(humidity, 1);
    display.println(" %");
    display.println(" ");
    display.print("Pres:  ");
    display.print(pressure, 1);
    display.println(" hPa");
    break;
  }

  display.display();
}

void displayWelcome()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(" ");
  display.println("Environment Monitor");
  display.setCursor(0, 15);
  display.println(" ");
  display.println("Initializing...");
  display.display();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("DHT11 connecting");
  display.display();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  display.print(".");
  display.display();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  display.print(".");
  display.display();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  display.print(".");
  display.display();
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("DHT11 connected!");
  display.print("Internet connecting");
  display.display();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  display.print(".");
  display.display();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  display.print(".");
  display.display();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  display.print(".");
  display.display();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("DHT11 connected!");
  display.println("Internet connected!");
  display.print("GPS connecting");
  display.display();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  display.print(".");
  display.display();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  display.print(".");
  display.display();
  vTaskDelay(100 / portTICK_PERIOD_MS);
  display.print(".");
  display.display();
  vTaskDelay(200 / portTICK_PERIOD_MS);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("DHT11 connected!");
  display.println("Internet connected!");
  display.print("GPS connected！");
  display.display();
}

void displayError(const char *msg)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("! SYSTEM ERROR !");
  display.setCursor(0, 20);
  display.println(msg);
  display.display();
  while (1)
    ;
}

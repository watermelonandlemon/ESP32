#include <DHT.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
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
#include <Adafruit_BusIO_Register.h>
// 定义扰动数组和索引（需要在任务外定义，保持状态）
float perturbationArray[] = {0.0021, -0.0013, 0.0034, -0.0024, -0.0018};
int perturbationIndex = 0;
const int arraySize = 5;

// 用于存储上一次的经纬度值
double lastLat = 30.2638;
double lastLng = 120.1230;

float Pitch = 56.43;
float Roll = 10.08;
float Yaw = 120.04;
float Gyro_x = 0.026;
float Gyro_y = 0.376;
float Gyro_z = 0.521;
float Accel_x = 0.002;
float Accel_y = 0.013;
float Accel_z = 0.021;

const char *ssid = "SFR";
const char *password = "12345678";
// ESP32 自己的热点（AP 模式）
const char *ssid_ap = "ESP32";        // 热点名称
const char *password_ap = "12345678"; // 热点密码（至少 8 位）

// 全局变量
volatile int triggerFlag = 0;  // 标志变量
unsigned long triggerTime = 0; // 触发时间
// 添加三个新按钮对应的变量
int flag2 = 0;
unsigned long time2 = 0;

int flag3 = 0;
unsigned long time3 = 0;

int flag4 = 0;
unsigned long time4 = 0;
const unsigned long HOLD_TIME = 5000; // 5 秒

double temperature = 26.4;
double humidity = 23.6;
double pressure = 1013.25;
int pulling = 0;

int GPS_signal = 0;              // GPS 信号强度
double GPS_Latitude = 30.2638;   // GPS 纬度
double GPS_Longitude = 120.1230; // GPS 经度

WebServer server(80);           // 创建 Web 服务器（监听端口 80）
IPAddress apIP(192, 168, 6, 1); // AP 的 IP 地址（客户端需连接后访问此 IP）
uint8_t currentPage = 1;
Adafruit_NeoPixel strip(1, 38, NEO_GRB + NEO_KHZ800);
Adafruit_SSD1306 display(128, 64, &Wire);
Adafruit_BMP085 bmp;
MPU6050 mpu(Wire1);
DHT dht(12, DHT11);
WiFiClient espClient;
PubSubClient client(espClient);
HardwareSerial GPS_Serial(1);
TinyGPSPlus gps;
wifi_sta_list_t stationList; // client 列表
int BMP180_running = 0;
int DHT11_running = 0;
int ADC_running = 0;
int GPS_running = 0;
int MPU6050_running = 0;
String receivedData = "waiting for device";

void TaskBlink1(void *pvParameters);
void TaskBlink2(void *pvParameters);
void TaskBlink3(void *pvParameters);
void TaskBlink4(void *pvParameters);
void TaskBlink5(void *pvParameters);
void TaskBlink6(void *pvParameters);
void TaskBlink7(void *pvParameters);
void TaskBlink8(void *pvParameters);
void TaskBlink9(void *pvParameters);
void TaskBlink10(void *pvParameters);
void reportProperties(double temperature, double humidity, double pressure, int pulling);
void displayPage(uint8_t page);
void handleRoot();
void handleTrigger();
void handleTrigger2();
void handleTrigger3();
void handleTrigger4();

void setup()
{
  Serial.begin(9600);
  delay(50);
  Serial2.begin(9600, SERIAL_8N1, 17, 18);
  delay(50);
  WiFi.mode(WIFI_AP_STA); // 设置为 AP + STA 模式
  // 配置并启动 AP（热点）
  WiFi.softAP(ssid_ap, password_ap);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  Serial.printf("AP started: %s\n", ssid_ap);
  Serial.printf("AP IP: %s\n", apIP.toString().c_str());
  // 连接外部 Wi-Fi（STA）
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(50);
  // 配置 Web 服务器路由
  server.on("/", HTTP_GET, handleRoot);
  server.on("/trigger", HTTP_POST, handleTrigger);
  server.on("/trigger2", HTTP_POST, handleTrigger2);
  server.on("/trigger3", HTTP_POST, handleTrigger3);
  server.on("/trigger4", HTTP_POST, handleTrigger4);
  server.on("/data", HTTP_POST, []()
            { server.send(200, "text/plain", "Data received"); 
              receivedData = server.arg("plain"); });
  // 启动 Web 服务器
  server.begin();
  Serial.println("Web server started at http://192.168.6.1");
  delay(50);
  Wire.begin(20, 21); // OLED+BMP
  Serial.println("Scanning I2C devices...");
  for (byte i = 1; i < 127; i++)
  {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0)
    {
      Serial.printf("Found device at 0x%02X\n", i);
    }
  }
  delay(50);
  Serial.println("✅ Wire0 初始化成功！");
  delay(50);
  // Wire1.begin(14, 13); // MPU6050
  // Serial.println("Scanning I2C devices...");
  // for (byte i = 1; i < 127; i++)
  //{
  // Wire1.beginTransmission(i);
  // if (Wire1.endTransmission() == 0)
  //{
  // Serial.printf("Found device at 0x%02X\n", i);
  //}
  //}
  // delay(50);
  // Serial.println("✅ Wire1 初始化成功！");
  delay(50);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("❌OLED 初始化失败，请检查连接！"));
    for (;;)
      ;
  }
  delay(50);
  Serial.println("✅ OLED 初始化成功！");
  delay(50);
  if (!bmp.begin(BMP085_STANDARD, &Wire))
  {
    Serial.println("❌ BMP180 初始化失败！");
    while (1)
      ;
  }
  delay(50);
  Serial.println("✅ BMP180 初始化成功！");
  delay(50);
  strip.begin();           // 初始化灯带
  strip.show();            // 初始化所有灯为关闭
  strip.setBrightness(15); // 设置亮度（0~255）
  delay(50);
  Serial.println("✅ RGB 初始化成功！");
  delay(50);
  GPS_Serial.begin(9600, SERIAL_8N1, 10, 11);
  delay(50);
  Serial.println("✅ GPS 初始化成功！");
  delay(50);
  // pinMode(9, INPUT);
  // delay(50);
  // Serial.println("✅ ADC 初始化成功！");
  // delay(50);
  dht.begin();
  delay(50);
  Serial.println("✅ DHT11 初始化成功！");
  delay(50);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Hello...");
  display.display();
  delay(2000);
  // 设置 GPIO 4 为输出模式
  // pinMode(4, OUTPUT);

  // 将 GPIO 4 输出高电平（3.3V）
  // digitalWrite(4, HIGH);
  // 创建任务     //任务名      //栈大小  //优先级(0-3)(0最低)   //第一个核(0)或第二个核(1)
  xTaskCreatePinnedToCore(TaskBlink1, "TaskBlink1", 4096, NULL, 0, NULL, 0);  // RGB
  xTaskCreatePinnedToCore(TaskBlink2, "TaskBlink2", 16384, NULL, 1, NULL, 0); // WIFI
  xTaskCreatePinnedToCore(TaskBlink3, "TaskBlink3", 16384, NULL, 2, NULL, 0); // MQTT
  xTaskCreatePinnedToCore(TaskBlink4, "TaskBlink4", 16384, NULL, 3, NULL, 0); // WIFI_Server
  xTaskCreatePinnedToCore(TaskBlink5, "TaskBlink5", 16384, NULL, 1, NULL, 1); // OLED
  xTaskCreatePinnedToCore(TaskBlink6, "TaskBlink6", 4096, NULL, 0, NULL, 1);  // ADC
  xTaskCreatePinnedToCore(TaskBlink7, "TaskBlink7", 8192, NULL, 2, NULL, 1);  // GPS
  xTaskCreatePinnedToCore(TaskBlink8, "TaskBlink8", 8192, NULL, 1, NULL, 1);  // BMP180
  xTaskCreatePinnedToCore(TaskBlink9, "TaskBlink9", 4096, NULL, 0, NULL, 1);  // DHT11
  // xTaskCreatePinnedToCore(TaskBlink10, "TaskBlink10", 8192, NULL, 3, NULL, 1); // MPU6050
}
void loop()
{
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

void TaskBlink1(void *pvParameters)
{
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
    Serial2.println("sfr");
  }
}

void TaskBlink2(void *pvParameters)
{
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
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    if (WiFi.status() == WL_CONNECTED && state_of_mqtt == 0) // 如果已经连接上了wifi并且没有初始化
    {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      client.setServer(mqttServer, mqttPort); // 设置连接到MQTT服务器的参数
      client.setKeepAlive(60);                // 设置心跳时间
      while (!client.connected())
      {
        if (client.connect(clientId, mqttUser, mqttPassword))
        {
          state_of_mqtt = 1; // 设置状态为已连通
        }
        else
        {
          Serial.print("failed with state ");
          Serial.print(client.state());
          vTaskDelay(100 / portTICK_PERIOD_MS);
        }
      }
    }
    if (WiFi.status() == WL_CONNECTED && state_of_mqtt == 1) // 如果已经连接上了wifi并且已经初始化
    {
      if (!client.connected())
      {
        Serial.println("MQTT disconnected. Reconnecting...");
        state_of_mqtt = 0; // 设置状态为未连通
      }
      else
      {
        client.loop();                                              // 处理 MQTT 网络流量
        reportProperties(temperature, humidity, pressure, pulling); // 上报属性
        vTaskDelay(1500 / portTICK_PERIOD_MS);
      }
    }
  }
}

void TaskBlink4(void *pvParameters)
{
  while (true)
  {
    server.handleClient();

    // 非阻塞地检查是否需要重置标志位
    if (triggerFlag == 1)
    {
      if (millis() - triggerTime >= HOLD_TIME)
      {
        triggerFlag = 0;
        Serial.println("Flag reset to 0 after 5 seconds");
      }
    }
    if (flag2 == 1)
    {
      if (millis() - time2 >= HOLD_TIME)
      {
        flag2 = 0;
      }
    }
    if (flag3 == 1)
    {
      if (millis() - time3 >= HOLD_TIME)
      {
        flag3 = 0;
      }
    }
    if (flag4 == 1)
    {
      if (millis() - time4 >= HOLD_TIME)
      {
        flag4 = 0;
      }
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void TaskBlink5(void *pvParameters)
{
  while (true)
  {
    currentPage = currentPage + 1;
    if (currentPage == 5)
    {
      currentPage = 1;
    }
    displayPage(currentPage);
    vTaskDelay(2500 / portTICK_PERIOD_MS);
  }
}

void TaskBlink6(void *pvParameters)
{
  while (true)
  {
    if (triggerFlag == 1)
    {
      pulling = 999;
    }
    else
    {
      pulling = 0;
      // pulling = analogRead(9); // 读取ADC引脚9的值
      // Serial.println(pulling);
    }
    vTaskDelay(300 / portTICK_PERIOD_MS);
  }
}

void TaskBlink7(void *pvParameters)
{
  while (true)
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
      // 判断是否与上次相同（可适当加容差，避免浮点误差误判）
      if (fabs(GPS_Latitude - lastLat) < 1e-10 && fabs(GPS_Longitude - lastLng) < 1e-10)
      {
        // 位置未变，应用扰动
        float delta = perturbationArray[perturbationIndex];
        GPS_Latitude = lastLat + delta;
        GPS_Longitude = lastLng - delta; // 可以同加、同减或反向，这里示例一加一减
        lastLat = GPS_Latitude;
        lastLng = GPS_Longitude;

        // 更新扰动索引（循环）
        perturbationIndex = (perturbationIndex + 1) % arraySize;
      }
      if (gps.satellites.isUpdated())
      {
        GPS_signal = gps.satellites.value();
      }
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void TaskBlink8(void *pvParameters)
{
  while (true)
  {
    pressure = bmp.readPressure() / 100.0; // hP
    vTaskDelay(750 / portTICK_PERIOD_MS);  // 延时100毫秒
  }
}

void TaskBlink9(void *pvParameters)
{
  while (true)
  {
    double t = dht.readTemperature();
    double h = dht.readHumidity();
    if (!isnan(t) && !isnan(h))
    {
      temperature = t;
      humidity = h;
    }
    vTaskDelay(650 / portTICK_PERIOD_MS);
  }
}

void TaskBlink10(void *pvParameters)
{
  /*
  while (!mpu.begin())
  {
    Serial.println("❌ MPU6050 未找到，正在重试...");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  Serial.println("✅ MPU6050 已连接！");
  vTaskDelay(50 / portTICK_PERIOD_MS);
  mpu.calcOffsets();
  */
  while (true)
  {
    // mpu.update();
    // Accel_x = mpu.getAccX();
    // Accel_y = mpu.getAccY();
    // Accel_z = mpu.getAccZ();
    // Gyro_x = mpu.getGyroX();
    // Gyro_y = mpu.getGyroY();
    // Gyro_z = mpu.getGyroZ();
    // Pitch = mpu.getAngleX();
    // Roll = mpu.getAngleY();
    // Yaw = mpu.getAngleZ();

    vTaskDelay(1000 / portTICK_PERIOD_MS); // 延时1000毫秒
  }
}

void reportProperties(double temperature, double humidity, double pressure, int pulling)
{
  String fish_info = "good";
  if (flag2 == 0)
  {
    if (flag3 == 0)
    {
      if (flag4 == 0)
      {
        fish_info = receivedData;
      }
    }
  }
  if (flag2 == 1)
  {
    fish_info = "Objects: Carp(0.83)";
  }
  if (flag3 == 1)
  {
    fish_info = "Objects: Perch(0.78)";
  }
  if (flag4 == 1)
  {
    fish_info = "Objects: Carp(0.66)Perch(0.85)";
  }
  char *topic_properties_report = "$oc/devices/Doge1_1/sys/properties/report"; // 上报属性主题

  StaticJsonDocument<1024> doc;
  doc["services"][0]["service_id"] = "EnvAware";
  // JsonObject properties = doc["services"][0]["properties"]; // 获取 properties 对象，方便多次写入
  JsonObject properties = doc["services"][0].createNestedObject("properties");
  properties["temperature"] = temperature;
  properties["humidity"] = humidity;
  properties["pressure"] = pressure;
  properties["pulling"] = pulling;
  properties["latitude"] = GPS_Latitude;
  properties["longitude"] = GPS_Longitude;
  properties["fish_name"] = fish_info;

  String jsonBuffer;
  serializeJson(doc, jsonBuffer);

  if (client.publish(topic_properties_report, jsonBuffer.c_str()))
  {
    Serial.println("Property reported");
  }
  else
  {
    Serial.println("Property report failed!");
  }
}

void displayPage(uint8_t page)
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
    display.print(Accel_x, 2);
    display.print(" ");
    display.print(Gyro_x, 2);
    display.print(" ");
    display.println(Pitch, 2);
    display.println(" ");
    display.print("Y: ");
    display.print(Accel_y, 2);
    display.print(" ");
    display.print(Gyro_y, 2);
    display.print(" ");
    display.println(Roll, 2);
    display.println(" ");
    display.print("Z: ");
    display.print(Gyro_z, 2);
    display.print(" ");
    display.print(Accel_z, 2);
    display.print(" ");
    display.println(Yaw, 2);
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
    display.print("Sig:   ");
    display.println(GPS_signal + 1, 4);
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
  case 4:
    display.println("    Raspberry");
    display.println(" ");
    if (flag2 == 0)
    {
      if (flag3 == 0)
      {
        if (flag4 == 0)
        {
          display.println(receivedData);
        }
      }
    }
    if (flag2 == 1)
    {
      display.println("Objects: Carp(0.83)");
    }
    if (flag3 == 1)
    {
      display.println("Objects: Perch(0.78)");
    }
    if (flag4 == 1)
    {
      display.println("Objects: Carp(0.66)");
      display.println("Objects: Perch(0.85)");
    }
    break;
  }
  display.display();
}
/*
void handleRoot()
{
  String html = "<h1>ESP32 AP+STA Mode</h1>";
  html += "<p>Status: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</p>";
  html += "<p>Send data via POST to <code>/data</code></p>";

  // 添加按钮
  html += "<button onclick=\"trigger()\">Click to Set Flag=1 for 5s</button>";

  // 添加 JavaScript 发送请求
  html += "<script>";
  html += "function trigger() {";
  html += "  fetch('/trigger', { method: 'POST' })";
  html += "    .then(res => alert('Triggered! Flag is now 1 (for 5 seconds)'));";
  html += "}";
  html += "</script>";

  server.send(200, "text/html", html);
}
*/
void handleRoot()
{
  String html = "<h1>ESP32 AP+STA Mode</h1>";
  html += "<p>Status: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</p>";
  html += "<p>Send data via POST to <code>/data</code></p>";

  // --- 原始按钮 ---
  html += "<button onclick=\"trigger()\">Click to Set Flag=1 for 5s</button>";

  // --- 新增的三个按钮 ---
  html += "<button onclick=\"trigger2()\">Click to Set Flag=2 for 5s</button>";
  html += "<button onclick=\"trigger3()\">Click to Set Flag=3 for 5s</button>";
  html += "<button onclick=\"trigger4()\">Click to Set Flag=4 for 5s</button>";

  // --- 添加 JavaScript ---
  html += "<script>";
  html += "function trigger() {";
  html += "  fetch('/trigger', { method: 'POST' })";
  html += "    .then(res => alert('Triggered! Flag is now 1 (for 5 seconds)'));";
  html += "}";

  // 新增的三个 JavaScript 函数
  html += "function trigger2() {";
  html += "  fetch('/trigger2', { method: 'POST' })";
  html += "    .then(res => alert('Triggered! Flag2 is now 1 (for 5 seconds)'));";
  html += "}";

  html += "function trigger3() {";
  html += "  fetch('/trigger3', { method: 'POST' })";
  html += "    .then(res => alert('Triggered! Flag3 is now 1 (for 5 seconds)'));";
  html += "}";

  html += "function trigger4() {";
  html += "  fetch('/trigger4', { method: 'POST' })";
  html += "    .then(res => alert('Triggered! Flag4 is now 1 (for 5 seconds)'));";
  html += "}";
  html += "</script>";

  server.send(200, "text/html", html);
}

void handleTrigger()
{
  // 设置标志位和触发时间
  triggerFlag = 1;
  triggerTime = millis();
  server.send(200, "text/plain", "Triggered! Flag = 1 (will reset in 5 seconds)");
}

void handleTrigger2()
{
  flag2 = 1;
  time2 = millis();
  server.send(200, "text/plain", "Triggered! Flag2 = 1 (will reset in 5 seconds)");
}

void handleTrigger3()
{
  flag3 = 1;
  time3 = millis();
  server.send(200, "text/plain", "Triggered! Flag3 = 1 (will reset in 5 seconds)");
}

void handleTrigger4()
{
  flag4 = 1;
  time4 = millis();
  server.send(200, "text/plain", "Triggered! Flag4 = 1 (will reset in 5 seconds)");
}
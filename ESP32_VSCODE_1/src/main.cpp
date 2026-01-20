#include <WiFi.h>
#include <Wire.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

const int ADC_PIN = 9;              // ADC引脚
const int ledPin = 2;  // 使用GPIO2作为LED引脚          未使用
const char* ssid = "Li";            // Wi-Fi 名称
const char* password = "12345678";  // Wi-Fi 密码
const char* mqttServer = "d29b3769b5.st1.iotda-device.cn-north-4.myhuaweicloud.com"; // MQTT 服务器地址
const int   mqttPort = 1883; // MQTT 服务器端口
const char* clientId = "67bfe77a24d772325520dd8c_3345_0_0_2025072411"; // 客户端ID
const char* mqttUser = "67bfe77a24d772325520dd8c_3345"; // 用户名
const char* mqttPassword = "b12ef035ea70434a29fa6ada78800ed07b5a1abcc6d36f5d6264a8964b8f6352"; // 密码

const char* topic_properties_report = "$oc/devices/Doge1_1/sys/properties/report"; // 上报属性主题

WiFiClient espClient; //ESP32WiFi模型定义
PubSubClient client(espClient); //PubSubClient模型定义

// 函数声明

void setup_wifi();
void MQTT_Init();
void reportProperties(int adcValue);

void setup() {
  Serial.begin(9600);
  delay(1000); 
  Serial.println("Serial init finished");
  delay(1000); 
  pinMode(ADC_PIN, INPUT);// 设置ADC引脚为输入模式
  setup_wifi();  // 调用Wi-Fi连接函数
  Serial.println("Setup complete, ready to read ADC values.");
  Serial.println("ADC Pin: " + String(ADC_PIN));
  Serial.println("Wi-Fi SSID: " + String(ssid));
  Serial.println("Wi-Fi Password: " + String(password));
  Serial.println("ADC reading will start now.");
  MQTT_Init(); // 初始化并连接到MQTT服务器

}
void loop() {
  int adcValue = analogRead(ADC_PIN);
  Serial.println("ADC Value");
  Serial.println(adcValue);
  delay(1000);  // 每秒读取一次ADC值
  // 检查 Wi-Fi 连接
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    setup_wifi();
  }
  // 检查 MQTT 连接
  if (!client.connected()) {
    Serial.println("MQTT disconnected. Reconnecting...");
    MQTT_Init();
  }
  client.loop(); // 处理 MQTT 网络流量
  reportProperties(adcValue); // 上报属性
  Serial.println("Properties reported successfully.");
  // 其他逻辑处理
  // ...
  // 添加延时，避免阻塞
  delay(1000); // 小延时，避免阻塞
}


void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    }   
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("MAC address: ");
  uint8_t mac[6];
  WiFi.macAddress(mac); 
  Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println("WiFi signal strength (RSSI): ");
  Serial.println(WiFi.RSSI());
}

void MQTT_Init()
{
  delay(10);
  client.setServer(mqttServer, mqttPort); //设置连接到MQTT服务器的参数
  client.setKeepAlive (60); //设置心跳时间
  while (!client.connected()) { //尝试与MQTT服务器建立连接
    Serial.println("Connecting to MQTT...");
    if (client.connect(clientId, mqttUser, mqttPassword )) {
      Serial.println("MQTT connected!");  
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(1000);
    }
  }
}

void reportProperties(int adcValue) {
  StaticJsonDocument<128> doc;
  doc["services"][0]["service_id"] = "EnvAware";
  doc["services"][0]["properties"]["humidity"] = adcValue;

  String jsonBuffer;
  serializeJson(doc, jsonBuffer);

  if (client.publish(topic_properties_report, jsonBuffer.c_str())) {
    Serial.println("Property reported: " + jsonBuffer);
  } else {
    Serial.println("Property report failed!");
  }
}
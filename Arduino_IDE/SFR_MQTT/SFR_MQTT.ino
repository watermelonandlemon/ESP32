#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

/*MQTT连接配置*/
/*-----------------------------------------------------*/
const char* ssid = "Li";
const char* password = "12345678";
const char* mqttServer = "d29b3769b5.st1.iotda-device.cn-north-4.myhuaweicloud.com";
const int   mqttPort = 1883;
//以下3个参数可以由HMACSHA256算法生成，为硬件通过MQTT协议接入华为云IoT平台的鉴权依据
const char* clientId = "67bfe77a24d772325520dd8c_3345_0_0_2025072411";
const char* mqttUser = "67bfe77a24d772325520dd8c_3345";
const char* mqttPassword = "b12ef035ea70434a29fa6ada78800ed07b5a1abcc6d36f5d6264a8964b8f6352";

 
const char* topic_properties_report = "$oc/devices/Doge1_1/sys/properties/report";

WiFiClient espClient; //ESP32WiFi模型定义
PubSubClient client(espClient);
 
//const char* topic_properties_report = "11";
 
//接收到命令后上发的响应topic
char* topic_Commands_Response = "$oc/devices/设备ID/sys/commands/response/request_id=";
 


 
/*******************************************************/
/*
 * 作用：  ESP32的WiFi初始化以及与MQTT服务器的连接
 * 参数：  无
 * 返回值：无
 */
void MQTT_Init()
{
//WiFi网络连接部分
  WiFi.begin(ssid, password); //开启ESP32的WiFi
  while (WiFi.status() != WL_CONNECTED) { //ESP尝试连接到WiFi网络
    delay(3000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to the WiFi network");
 
 
//MQTT服务器连接部分
  client.setServer(mqttServer, mqttPort); //设置连接到MQTT服务器的参数
 
  client.setKeepAlive (60); //设置心跳时间
 
  while (!client.connected()) { //尝试与MQTT服务器建立连接
    Serial.println("Connecting to MQTT...");
  
    if (client.connect(clientId, mqttUser, mqttPassword )) {
  
      Serial.println("connected");  
  
    } else {
  
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(6000);
  
    }
  }
 
 
//接受平台下发内容的初始化
  //client.setCallback(callback); //可以接受任何平台下发的内容
 
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  // put your setup code here, to run once:
  MQTT_Init();

}

void loop() {
  // put your main code here, to run repeatedly:

}

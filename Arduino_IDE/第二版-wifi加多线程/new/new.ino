#include <PubSubClient.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <MPU6050_light.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <TinyGPSPlus.h>
// --- WS2812 RGB 配置 ---
#define LED_PIN    38      // 接到 RGB 的 GPIO
#define LED_COUNT  1       // 灯珠数量
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);// 创建 NeoPixel 对象


// --- OLED 配置 ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1); // 使用 Wire1（OLED）
uint8_t currentPage =1;

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

float Lat = 45.8038;    
float Lng = 126.5350;

float temperature = 26.4;
float huminity = 23.6;
float pressure = 993.0;


// --- (线程3)网络服务器配置 ---
WebServer server(80);// 创建 WebServer 对象，监听端口 80
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
  <h1> Hello from ESP32!</h1>
  <p>当前状态: 正在运行</p>
  <p>时间戳: )rawliteral";

// 网页的后半部分（固定内容）
const char htmlPart2[] PROGMEM = R"rawliteral(秒</p>
  <p><button onclick="refresh()">刷新页面</button></p>
  <script>
    function refresh() {
      location.reload();
    }
  </script>
</body>
</html>
)rawliteral";



void handleRoot();// 处理根路径 "/" 的请求
void handleNotFound();// 处理 404 错误
void displayWelcome();//OLED显示屏初始展示内容
void displayError(const char *msg);//OLED显示屏错误提示
void lxy_displayPage(uint8_t page);//OLED显示屏翻页函数
void colorWipe(uint32_t c, uint8_t wait);// 逐个点亮颜色
void TaskBlink1(void *pvParameters);//线程1 核心1 板载LED闪烁
void TaskBlink2(void *pvParameters);//线程2 核心1 将变量上传至OLED显示屏
void TaskBlink3(void *pvParameters);//线程3 核心1 连接Wifi并开启网络服务器



void setup() {
  Serial.begin(115200);//初始化串口
  delay(1000);          // 给串口一点时间初始化
  Serial.println("Start Program!");

  xTaskCreatePinnedToCore(
    TaskBlink1
    ,  "TaskBlink1" //任务名
    ,  8192  // 栈大小
    ,  NULL
    ,  0  // 任务优先级
    ,  NULL 
    ,  0); // 第一个核
  

  xTaskCreatePinnedToCore(
    TaskBlink2
    ,  "TaskBlink2"   // 任务名
    ,  8192  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  1  // 任务优先级, with 3 (configMAX_PRIORITIES - 1) 是最高的，0是最低的.
    ,  NULL 
    ,  0); // 第一个核

  xTaskCreatePinnedToCore(
    TaskBlink3
    ,  "TaskBlink3"   // 任务名
    ,  8192  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // 任务优先级, with 3 (configMAX_PRIORITIES - 1) 是最高的，0是最低的.
    ,  NULL 
    ,  0); // 第一个核
  
  Serial.println("All Threads Started!");//现在，接管单个任务调度控制的任务调度程序将自动启动。
}
 
void loop()
{
  vTaskDelay(1000 / portTICK_PERIOD_MS); // 避免看门狗触发
}
 
/*---------------------- Tasks ---------------------*/
 
void TaskBlink1(void *pvParameters){  
  strip.begin();           // 初始化灯带
  strip.show();            // 初始化所有灯为关闭
  strip.setBrightness(50); // 设置亮度（0~255）
  while(true){
  colorWipe(strip.Color(255, 0, 0), 50); // 红色
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  colorWipe(strip.Color(0, 255, 0), 50); // 绿色
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  colorWipe(strip.Color(0, 0, 255), 50); // 蓝色
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
 
void TaskBlink2(void *pvParameters){  
  Wire1.begin(11, 12);   // OLED：独立 I2C 总线
  vTaskDelay(1000 / portTICK_PERIOD_MS);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED Init Failed");
    while (1);
  }
  displayWelcome();
 
  for (;;){ // A Task shall never return or exit.
    currentPage = currentPage + 1;
    if (currentPage == 4 )
    {
      currentPage = 1;
    }
    lxy_displayPage(currentPage);
    vTaskDelay(3500 / portTICK_PERIOD_MS);
  }
}

void TaskBlink3(void *pvParameters){
  const char* ssid = "ESP32";
  const char* password = "12345678";
  WiFi.begin(ssid, password);// 连接到Wi-Fi网络
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    Serial.print(".");
  }
  Serial.println("WiFi连接成功");
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());// 打印ESP32的IP地址
  Serial.flush();

  // 启动 Web 服务器
  server.on("/", HTTP_GET, handleRoot);             // 根页面
  server.onNotFound(handleNotFound);                // 404 处理
  server.begin();
  Serial.println("Web 服务器已启动，访问: http://" + WiFi.localIP().toString());
  Serial.flush();

  while(true){
    server.handleClient();  // 处理客户端请求
    vTaskDelay(10 / portTICK_PERIOD_MS); // 避免占用太多 CPU
  }

  vTaskDelete(NULL);//任务完成后应删除自己，防止崩溃

}



/*---------------------- Other Functions ---------------------*/


void handleRoot() {
  // 创建动态变量，例如时间戳
  String timestamp = String(millis() / 1000);

  // 拼接完整的 HTML 页面
  String html = "";
  html += F(htmlPart1);      // 添加 HTML 前半部分
  html += timestamp;         // 添加动态变量
  html += F(htmlPart2);      // 添加 HTML 后半部分

  server.send(200, "text/html", html);  // 发送完整页面
}

void handleNotFound() {
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
    display.print(Accel_x,3);
    display.print(" "); 
    display.print(Gyro_x,3);
    display.print(" "); 
    display.println(Pitch,2);
    display.println(" ");
    display.print("Y: ");
    display.print(Accel_y,3);
    display.print(" "); 
    display.print(Gyro_y,3);
    display.print(" ");
    display.println(Roll,2); 
    display.println(" ");
    display.print("Z: ");
    display.print(Gyro_z,3);
    display.print(" "); 
    display.print(Accel_z,3);
    display.print(" "); 
    display.println(Yaw,2);
    display.println(" ");
    break;

  case 2:
    display.println("    GPS(ATGM336H-5N)");
    display.println(" ");
    display.print("Lat:   ");
    display.println(Lat,4);
    display.println(" ");
    display.print("Lng:   ");
    display.println(Lng,4);
    display.println(" ");
    display.print("Date:  2025-7-25");
    break;

  case 3:
    display.println("    Env(DHT+BMP180)");
    display.println(" ");
    display.print("Temp:  ");
    display.print(temperature,1);
    display.println(" C");
    display.println(" ");
    display.print("Hum:   ");
    display.print(huminity,1);
    display.println(" %");
    display.println(" ");
    display.print("Pres:  ");
    display.print(pressure,1);
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

void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    vTaskDelay(wait / portTICK_PERIOD_MS);
  }
}





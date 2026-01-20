#include <WiFi.h>
#include <WebServer.h>

// STA
const char *ssid_sta = "SFR";
const char *password_sta = "12345678";

// AP
const char *ssid_ap = "ESP32_Share";
const char *password_ap = "123456789";

// Create web server (listening on port 80)
WebServer server(80);

// AP's IP address (clients need to connect first, then access this IP)
IPAddress apIP(192, 168, 4, 1);

void setup()
{
  Serial.begin(9600);
  delay(1000);
  WiFi.mode(WIFI_AP_STA); // Set to AP + STA mode

  // Configure and start AP
  WiFi.softAP(ssid_ap, password_ap);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  Serial.printf("AP started: %s\n", ssid_ap);
  Serial.printf("AP IP: %s\n", apIP.toString().c_str());

  // Configure and start STA
  WiFi.begin(ssid_sta, password_sta);

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20)
  {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nConnected to STA");
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("\nFailed to connect to STA, continuing with AP only.");
  }

  // Configure web server routes
  server.on("/", HTTP_GET, []()
            {
                String html = "<h1>ESP32 AP+STA Mode</h1>";
                html += "<p>Status: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</p>";
                html += "<p>Send data via POST to <code>/data</code></p>";
                server.send(200, "text/html", html); });

  // Receive data sent from Raspberry Pi
  server.on("/data", HTTP_POST, []()
            {
                String data = server.arg("plain"); 
                Serial.println("Received from Raspberry Pi:");
                Serial.println(data);
                server.send(200, "text/plain", "Received: " + data); });

  // Start web server
  server.begin();
  Serial.println("Web server started at http://192.168.4.1");
}

void loop()
{
  server.handleClient(); // Process HTTP requests

  if (WiFi.status() != WL_CONNECTED)
  {
    static unsigned long retryTime = 0;
    if (millis() - retryTime > 10000)
    {
      WiFi.reconnect();
      retryTime = millis();
    }
  }
}
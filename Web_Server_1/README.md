Project Creation Platform: PlatformIO of VSCode
Project Function Overview:The code connects to a Wi-Fi network named SFR while simultaneously creating a hotspot named ESP32_Share. Other devices (such as a Raspberry Pi) can connect to this hotspot and transmit text content using the following Python script.

import requests
url = "http://192.168.4.1/data"
data = {
    "timestamp": "2026-01-20 12:00:00",
    "temperature": 23.5,
    "humidity": 60
}
response = requests.post(url, json=data)
print(f"Status Code: {response.status_code}")
print(f"Response: {response.text}")

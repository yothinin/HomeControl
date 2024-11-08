#include <ESP8266WiFi.h>
#include <DHT.h>
#include "secrets.h" 

// กำหนดค่าของเซ็นเซอร์ DHT22
#define DHTPIN D4     // DHT22 pin
#define DHTTYPE DHT22 // DHT type

#define LED_PIN D5
#define SWITCH_PIN D6
bool ledState = LOW;  // Turn off LED
bool lastSwitchState = HIGH;  // สถานะเริ่มต้นของ Switch (ไม่กด)

DHT dht(DHTPIN, DHTTYPE);

// Home Assistant API
const char* api_url_led_state = "http://192.168.1.121:8123/api/states/switch.dht22_led"; // URL API ของ Home Assistant สำหรับ LED
const char* api_url_temperature = "http://192.168.1.121:8123/api/states/sensor.dht22_temperature"; // URL API ของ Home Assistant สำหรับอุณหภูมิ
const char* api_url_humidity = "http://192.168.1.121:8123/api/states/sensor.dht22_humidity"; // URL API ของ Home Assistant สำหรับความชื้น


WiFiServer server(80); // สร้างเซิร์ฟเวอร์ HTTP ที่พอร์ต 80

void setup() {
  Serial.begin(115200);
  dht.begin(); // เริ่มเซ็นเซอร์ DHT22
  WiFi.setSleep(false); // ปิด Sleep เพื่อให้พร้อมเชื่อมต่อ
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password); // เชื่อมต่อ WiFi

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); // แสดง IP ของ ESP8266

  server.begin(); // เริ่มเซิร์ฟเวอร์

    // ตั้งค่า LED_PIN เป็น Output
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ledState);  // ตั้งค่า LED ปิดเริ่มต้น

  // ตั้งค่า SWITCH_PIN เป็น Input
  pinMode(SWITCH_PIN, INPUT_PULLUP); // ใช้ Pull-up Resistor สำหรับ Switch
  
}

unsigned long previousMillis = 0;  // ตัวแปรเก็บเวลา
const long interval = 2000;  // ตั้งเวลาที่ต้องการให้ห่างกัน (2 วินาที)

void loop() {

    // เช็คสถานะของ Switch
  bool currentSwitchState = digitalRead(SWITCH_PIN);
  if (currentSwitchState == LOW && lastSwitchState == HIGH) { // ตรวจจับการเปลี่ยนสถานะของสวิตช์
    toggleLED();  // ทำการ toggle LED
    delay(250);  // หน่วงเวลาเล็กน้อยเพื่อหลีกเลี่ยงการสั่นของสวิตช์
  }
  lastSwitchState = currentSwitchState; // อัปเดตสถานะล่าสุดของสวิตช์

  sendLEDStateToHomeAssistant(ledState);


  unsigned long currentMillis = millis();  // เวลาในปัจจุบัน

  // เช็คว่าเวลาผ่านไปครบ 2 วินาทีหรือยัง
  if (currentMillis - previousMillis >= interval) {
    // หากผ่านไป 2 วินาทีแล้วให้ทำการอ่านค่าจากเซ็นเซอร์
    previousMillis = currentMillis;  // อัปเดตเวลา
  }else {
    return;
  }
  
  // อ่านค่าจาก DHT22
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // ตรวจสอบว่าค่าที่อ่านได้ถูกต้องหรือไม่
  if (!isnan(h) && !isnan(t)) {
    //Serial.println ("Send data to Home Assistant...");
    sendDataToHomeAssistant(t, h); // ส่งข้อมูลไปยัง Home Assistant
  } else {
    Serial.println("Failed to read from DHT sensor!");
  }

  WiFiClient client = server.available(); // ตรวจสอบว่ามี client เชื่อมต่อหรือไม่
  ESP.wdtFeed();

  if (client) {
    String currentLine = ""; // เก็บข้อมูลที่อ่านจาก client
    while (client.connected()) {
      if (client.available()) {
        char c = client.read(); // อ่านข้อมูลจาก client
        Serial.write(c);
        if (c == '\n') { // ถ้าพบบรรทัดใหม่
          if (currentLine.length() == 0) {
            // ส่ง HTTP header
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // แสดงผลลัพธ์บนหน้าเว็บ
            client.println("<html><body><h2>DHT22 Sensor</h1>");
            client.print("<p>Temperature: ");
            client.print(t);
            client.println(" &deg;C</p>");
            client.print("<p>Humidity: ");
            client.print(h);
            client.println(" %</p>");
            client.println("</body></html>");
            break;
          } else { // ถ้าไม่ใช่บรรทัดใหม่
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c; // เก็บข้อมูล
        }
      }
    }
    client.stop(); // ปิดการเชื่อมต่อ client
    Serial.println("Client disconnected");
  }

  //delay(2000);
}

void sendDataToHomeAssistant(float temperature, float humidity) {
    // ส่งข้อมูลอุณหภูมิไปยัง Home Assistant
    String jsonTemperature = "{\"state\":\"" + String(temperature) + "\", \"attributes\":{\"unit_of_measurement\":\"°C\"}}";
    sendPostRequest(api_url_temperature, jsonTemperature);

    // ส่งข้อมูลความชื้นไปยัง Home Assistant
    String jsonHumidity = "{\"state\":\"" + String(humidity) + "\", \"attributes\":{\"unit_of_measurement\":\"%\"}}";
    sendPostRequest(api_url_humidity, jsonHumidity);
}

void sendLEDStateToHomeAssistant(bool ledState) {
  // ส่งสถานะ LED ไปยัง Home Assistant
  String jsonLED = "{\"state\":\"" + String(ledState ? "on" : "off") + "\"}";
  sendPostRequest(api_url_led_state, jsonLED);
}


void sendPostRequest(const char* api_url, String json) {
    WiFiClient client;
    if (client.connect("192.168.1.121", 8123)) { // เชื่อมต่อไปยัง Home Assistant
        client.println("POST " + String(api_url) + " HTTP/1.1");
        client.println("Host: 192.168.1.121");
        client.println("Authorization: Bearer " + String(api_key));
        client.println("Content-Type: application/json");
        client.println("Content-Length: " + String(json.length()));
        client.println();
        client.print(json);
        client.stop();
    } else {
        Serial.println("Connection to Home Assistant failed");
    }
}

// ฟังก์ชัน toggle LED
void toggleLED() {
  ledState = !ledState;  // เปลี่ยนสถานะของ LED
  digitalWrite(LED_PIN, ledState);  // ตั้งค่าสถานะ LED
    Serial.print("LED State: ");
  if (ledState) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }
}

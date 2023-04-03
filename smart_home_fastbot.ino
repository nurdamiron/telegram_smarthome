#include "EmonLib.h"
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <DHT.h>
#include <WebServer.h>
#define WEB_SERVER WebServer
#include <WS2812FX.h>
#include <Adafruit_NeoPixel.h>
#include <FastBot.h>

#define WIFI_SSID "SmartHome"
#define WIFI_PASS "smarthome"
#define BOT_TOKEN "5564120883:AAH7rTRVJ6G9s5iuNoF47KQQ1OzheZMpYXM"
#define CHAT_ID "423023246"

FastBot bot(BOT_TOKEN);
  
#define RED (uint32_t)0xFF0000
#define BLACK      (uint32_t)0x000000
 String temp_mode="Stopping";
extern const char index_html[];
extern const char main_js[];
unsigned long auto_last_change = 0;
unsigned long last_wifi_check_time = 0;

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

#define WIFI_TIMEOUT 30000         
#define HTTP_PORT 80

EnergyMonitor emon;  

#define LED_PIN 25 // пин led ленты          
#define LED_COUNT 23 // количество светодиодов

#define power 2 // пин электропитания

#define fan1 4 // пин кулера 1
#define fan2 16 // пин кулера 2

#define pe2  15 // пин охлаждения
#define pe1 13 // пин нагрева


#define LIGHT_DURATION 5000 

#define DHTPIN 26 // пин датчика влажности
#define DHTTYPE DHT11 // тип dht
DHT dht(DHTPIN, DHTTYPE);



#define ONE_WIRE_BUS 5  // пин температуры
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

float setTemp = 0;
float temp1 = 0;
String temp = "";

const unsigned long BOT_MTBS = 1000;
                                                                                                                                                                          
String tempH = "";
float t = 0.00;

bool working = false;
bool safe_mode = false;
String answer = "";
static uint32_t next_call = 0; // static ensures that they are not deleted when going out of "loop" and 
                                               // still keeps them local. Another option would be a global variable 
                                               // (as you did with PIR_SENSOR)
static bool motion_detected = false;
unsigned long bot_lasttime = 0;
unsigned long sensor_lasttime = 0;


unsigned long now1 = millis();



String modes = "";
uint8_t myModes[] = {}; // моды в led ленте
bool auto_cycle = false;
WS2812FX ws2812fx = WS2812FX(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

WEB_SERVER server(HTTP_PORT);  

// газ
int gas = 32;
int gas_state = 0;

// движение
int motion_pin = 12;
int motion_state = 0; 
// пламя
int flame_pin = 35;
int flame_state = 0;

// влажность
float h = 0;
String humid = "";

// потребление
double Irms = 0;
String powerr = "";

String message = "";
// IP адрес на стринг
String toString(const IPAddress& address){
  return String() + address[0] + "." + address[1] + "." + address[2] + "." + address[3];
}


void setup() {
  connectWiFi();
  Serial.println("Starting Smart Home system");
  bot.setChatID(CHAT_ID);
  bot.attach(newMsg);
  
  Serial.println("WS2812FX setup");
  ws2812fx.init();
  ws2812fx.setMode(FX_MODE_STATIC);
  ws2812fx.setColor(0xFFFFFF);
  ws2812fx.setSpeed(1000);
  ws2812fx.setBrightness(128);
  ws2812fx.start();

  Serial.println("HTTP server setuping");
  server.on("/", srv_handle_index_html);
  server.on("/main.js", srv_handle_main_js);
  server.on("/modes", srv_handle_modes);
  server.on("/set", srv_handle_set);
  server.onNotFound(srv_handle_not_found);
  server.begin();
  Serial.println("HTTP server started.");
  delay(1000);
 
  pinMode(power, OUTPUT);
  pinMode(fan1, OUTPUT);
  pinMode(fan2, OUTPUT);
  pinMode(pe1, OUTPUT);
  pinMode(pe2, OUTPUT);
  
  digitalWrite(power, LOW);
  digitalWrite(fan1, LOW);
  digitalWrite(fan2, LOW);
  digitalWrite(pe1, LOW);
  digitalWrite(pe2, LOW);

  pinMode(gas, INPUT);

  pinMode(flame_pin, INPUT);

  pinMode(motion_pin, INPUT);
  
  emon.current(34, 3); // pin, calibration
  Serial.println(F("Energy Mon connected!"));
  dht.begin();
  Serial.println(F("DHT11 connected!"));
  Serial.println("All setup ended. Start loop");
  delay(1000);
}

// обработчик сообщений
void newMsg(FB_msg& msg) {
  if (millis() - bot_lasttime > 1000){
    if (msg.text == "/start") {
      bot.sendMessage("Добро пожаловать в чат-бот умного дома!");
    }
    else if (msg.text == "/led") {
      safe_mode = false;
      answer = "Включен режим управление адресной лентой "; 
      answer +=  toString(WiFi.localIP());
      bot.sendMessage(answer);
    }
    else if (msg.text == "/humidity") {
        answer = humid;
        bot.sendMessage(answer);
     }
     else if (msg.text == "/temp_air") {
        answer = tempH;
        bot.sendMessage(answer);
      }
      else if (msg.text == "/current") {
        answer = powerr;
        bot.sendMessage(answer);
      }
      else if(msg.text == "/temperature") {
        sensors.requestTemperatures();
        float temperatureC = sensors.getTempCByIndex(0);
        Serial.print(temperatureC);
        Serial.println("ºC");
        temp = "Temperature: " + String(temperatureC) + "°C";    
        answer = temp;
        bot.sendMessage(answer);
      }
      if (msg.text == ("/power_on")) {    
        digitalWrite(power, HIGH);
        answer = "Электропитание включено";  
        bot.sendMessage(answer);
      }
      else if (msg.text == ("/power_off")) {     
        digitalWrite(power, LOW);
        answer = "Электропитание выключено";  
        bot.sendMessage(answer);
      }
      else if (msg.text == "/safe_mode_on") { 
        safe_mode = true;
        digitalWrite(power, LOW);
        answer = "Безопасный режим успешно включен!";
        Serial.println(answer);
        bot.sendMessage(answer);
      }
      else if (msg.text == "/safe_mode_off") { 
        digitalWrite(power, HIGH);
        safe_mode = false;
        answer = "Обычный режим включен";
        Serial.println(answer);
        bot.sendMessage(answer);
      }
      else if (msg.text == "/temp_control_on") {
        working = true;
        sensors.requestTemperatures();
        setTemp = sensors.getTempCByIndex(0);
        answer = "Введите нужную температуру в заданном интервале (16°C ~ 35°C): ";
        bot.sendMessage(answer);
      }
      else if (working&&msg.text == "/temp_control_off") {
        working = false;
        answer="Вы вышли из режима регулировки температуры";
        Serial.println(answer);   
        setTemp = 0.0;
        bot.sendMessage(answer);
      }
      else if(working){
      if (msg.text.equalsIgnoreCase("16")) { 
          answer="Настраиваем температуру до 16°C";
          Serial.println(answer); 
          setTemp = 16.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("17")) {    
          answer="Настраиваем температуру до 17°C";
          Serial.println(answer);  
          setTemp = 17.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("18")) {    
          answer="Настраиваем температуру до 18°C";
          Serial.println(answer); 
          setTemp = 18.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("19")) {    
          answer="Настраиваем температуру до 19°C";
          Serial.println(answer); 
          setTemp = 19.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("20")) {    
          answer="Настраиваем температуру до 20°C";
          Serial.println(answer);
          setTemp = 20.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("21")) {
          answer="Настраиваем температуру до 21°C";
          Serial.println(answer);  
          setTemp = 21.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("22")) {    
          answer="Настраиваем температуру до 22°C";
          Serial.println(answer); 
          setTemp = 22.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("23")) {    
          answer="Настраиваем температуру до 23°C";
          Serial.println(answer); 
          setTemp = 23.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("24")) {
          answer="Настраиваем температуру до 24°C";
          Serial.println(answer);      
          setTemp = 24.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("25")) {
          answer="Настраиваем температуру до 25°C";
          Serial.println(answer); 
          setTemp = 25.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("26")) {    
          answer="Настраиваем температуру до 26°C";
          Serial.println(answer); 
          setTemp = 26.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("27")) {    
          answer="Настраиваем температуру до 27°C";
          Serial.println(answer); 
          setTemp = 27.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("28")) {    
          answer="Настраиваем температуру до 28°C";
          Serial.println(answer);  
          setTemp = 28.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("29")) {    
          answer="Настраиваем темперxатуру до 29°C";
          Serial.println(answer); 
          setTemp = 29.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("30")) {    
         answer="Настраиваем температуру до 30°C";
          Serial.println(answer); 
          setTemp = 30.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("31")) {    
        answer="Настраиваем температуру до 31°C";
          Serial.println(answer); 
          setTemp = 31.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("32")) {    
          answer="Настраиваем температуру до 32°C";
          Serial.println(answer); 
          setTemp= 32.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("33")) {    
         answer="Настраиваем температуру до 33°C";
          Serial.println(answer); 
          setTemp = 33.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("34")) {    
         answer="Настраиваем температуру до 34°C";
          Serial.println(answer); 
          setTemp = 34.00;
          bot.sendMessage(answer);
        }
        if (msg.text.equalsIgnoreCase("35")) {    
          answer="Настраиваем температуру до 35°C";
          Serial.println(answer); 
          setTemp = 35.00;
          bot.sendMessage(answer);
      }
     }
     bot_lasttime = millis();
  }
}

void loop() {
  
  bot.tick();  
   

  unsigned long now = millis();

  server.handleClient();
  ws2812fx.service();
  

  if (auto_cycle && (now - auto_last_change > 10000)) { // cycle effect mode every 10 seconds
    uint8_t next_mode = (ws2812fx.getMode() + 1) % ws2812fx.getModeCount();
    if (sizeof(myModes) > 0) { // if custom list of modes exists
      for (uint8_t i = 0; i < sizeof(myModes); i++) {
        if (myModes[i] == ws2812fx.getMode()) {
          next_mode = ((i + 1) < sizeof(myModes)) ? myModes[i + 1] : myModes[0];
          break;
        }
     }
    }
    ws2812fx.setMode(next_mode);
    Serial.print("mode is "); Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
    auto_last_change = now;
  }
  if(safe_mode == true) {
    flame_state = analogRead(flame_pin);
    if(flame_state > 4094) {
      message = "Обнаружен пожар!";
      Serial.println(message);
      bot.sendMessage(message);
    }
    gas_state = digitalRead(gas); 
    if (gas_state == LOW) { 
        message = "Обнаружен газ!";
        Serial.println(message);
        bot.sendMessage(message);
    }
    motion_state = digitalRead(motion_pin);
      if(millis()-now1>=LIGHT_DURATION||motion_state){
    if (motion_state && !motion_detected) { 
      motion_detected = true;
      ws2812fx.setColor(RED);// just full brightness, no fading yet and not fade to white...
//      next_call = now + LIGHT_DURATION;
      Serial.print("detected motion");
      message = "Обнаружено движение!";
      Serial.println(message);
      bot.sendMessage(message);
    }
//    else{
      
      else if(motion_detected) {
//      motion_state = digitalRead(motion_pin);
      motion_detected = false; // now the sensor can retrigger a new effect.
//      ws2812fx.setBrightness(0); // turns the leds dark. No fading yet
      Serial.println("stopped");
      ws2812fx.setColor(BLACK);
      
//      next_call = now + LIGHT_DURATION;
    }
    now1 = millis();
      }
   }

   if (millis() - sensor_lasttime > 2000){
    h = dht.readHumidity();
    t = dht.readTemperature();
    tempH = "Air temperature: " + String(t) + " °C \n";
    humid = "Humidity: " + String(h) + " % \n";
   
    Irms = emon.calcIrms(1480);
    powerr = "Потребление: " + String(Irms * 2200);
    sensor_lasttime = millis();
 }
// if (millis() - sensor_lasttime > 5000){
//    
//    sensor_lasttime = millis();
// }


 sensors.requestTemperatures();
 float temp1 = sensors.getTempCByIndex(0);



 //--------------------DEBUG------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//
 Serial.print("Current temp     "); Serial.print("Setted temp"); Serial.print("   Power   ");Serial.print("   ");Serial.print("Temp control:");Serial.print("   R1");Serial.print("   R2");Serial.print("   R3");Serial.print(" motion "); Serial.print(" gas ");Serial.print(" flame");Serial.print("  dhtH");Serial.print("   dhtT");Serial.print("    Temp mode");Serial.println();
 Serial.print(temp1);Serial.print("              "); Serial.print(setTemp); Serial.print("          "); Serial.print(Irms * 220.0); Serial.print("     ");Serial.print(working);Serial.print("              "); Serial.print(digitalRead(power));Serial.print("    "); Serial.print(digitalRead(fan1));Serial.print("    "); Serial.print(digitalRead(fan2));Serial.print("    "); Serial.print(digitalRead(motion_pin)); Serial.print("      ");Serial.print(digitalRead(gas)); Serial.print("    ");Serial.print(analogRead(flame_pin));Serial.print("   "); Serial.print(h);Serial.print("    "); Serial.print(t);Serial.print("  "); Serial.print(temp_mode);Serial.println();
 //--------------------DEBUG------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------//
 
 
 
 
 if(temp1 <= 37 && working) {
  if (working&&setTemp - temp1 > 1) {  
//    Serial.println("Heating...");
    temp_mode="Heating...";
    digitalWrite(fan1, HIGH);
    digitalWrite(pe1, HIGH);
    digitalWrite(pe2, LOW);
    digitalWrite(fan2, HIGH);
    }
    else if (working&&setTemp - temp1 < -1) {
//    Serial.println("Cooling...");
    temp_mode="Cooling...";
    digitalWrite(fan2, HIGH);
    digitalWrite(fan1, HIGH);
    digitalWrite(pe1, LOW);
    digitalWrite(pe2, HIGH);
    }
    else {
//    Serial.println("Normal temparature");
    temp_mode="Normal...";
    digitalWrite(fan2, LOW);
    digitalWrite(pe1, LOW);
    digitalWrite(pe2, LOW);
    digitalWrite(fan1, LOW);
    }
 }
 else if(working) {
//  Serial.println("Too much temp");
    temp_mode="Too much...";
  digitalWrite(pe1, LOW);
  digitalWrite(pe2, LOW);
  digitalWrite(fan1, HIGH);
  digitalWrite(fan2, HIGH);
 }
 else if(!working) {
//  Serial.println("Stopping temp control");
    temp_mode="Stopping";
  digitalWrite(pe1, LOW);
  digitalWrite(pe2, LOW);
  digitalWrite(fan1, LOW);
  digitalWrite(fan2, LOW);
 }
 
}



void connectWiFi() {
  delay(2000);
  Serial.begin(115200);
  Serial.println();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() > 15000) ESP.restart();
  }
  Serial.println("Connected");
}

void modes_setup() {
  modes = "";
  uint8_t num_modes = sizeof(myModes) > 0 ? sizeof(myModes) : ws2812fx.getModeCount();
  for (uint8_t i = 0; i < num_modes; i++) {
    uint8_t m = sizeof(myModes) > 0 ? myModes[i] : i;
    modes += "<li><a href='#'>";
    modes += ws2812fx.getModeName(m);
    modes += "</a></li>";
  }
}

void srv_handle_not_found() {
  server.send(404, "text/plain", "File Not Found");
}

void srv_handle_index_html() {
  server.send_P(200, "text/html", index_html);
}

void srv_handle_main_js() {
  server.send_P(200, "application/javascript", main_js);
}

void srv_handle_modes() {
  server.send(200, "text/plain", modes);
}

void srv_handle_set() {
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "c") {
      uint32_t tmp = (uint32_t) strtol(server.arg(i).c_str(), NULL, 10);
      if (tmp <= 0xFFFFFF) {
        ws2812fx.setColor(tmp);
      }
    }

    if (server.argName(i) == "m") {
      uint8_t tmp = (uint8_t) strtol(server.arg(i).c_str(), NULL, 10);
      uint8_t new_mode = sizeof(myModes) > 0 ? myModes[tmp % sizeof(myModes)] : tmp % ws2812fx.getModeCount();
      ws2812fx.setMode(new_mode);
      auto_cycle = false;
      Serial.print("mode is "); Serial.println(ws2812fx.getModeName(ws2812fx.getMode()));
    }

    if (server.argName(i) == "b") {
      if (server.arg(i)[0] == '-') {
        ws2812fx.setBrightness(ws2812fx.getBrightness() * 0.8);
      } else if (server.arg(i)[0] == ' ') {
        ws2812fx.setBrightness(min(max(ws2812fx.getBrightness(), 5) * 1.2, 255));
      } else { // set brightness directly
        uint8_t tmp = (uint8_t) strtol(server.arg(i).c_str(), NULL, 10);
        ws2812fx.setBrightness(tmp);
      }
      Serial.print("brightness is "); Serial.println(ws2812fx.getBrightness());
    }

    if (server.argName(i) == "s") {
      if (server.arg(i)[0] == '-') {
        ws2812fx.setSpeed(max(ws2812fx.getSpeed(), 5) * 1.2);
      } else if (server.arg(i)[0] == ' ') {
        ws2812fx.setSpeed(ws2812fx.getSpeed() * 0.8);
      } else {
        uint16_t tmp = (uint16_t) strtol(server.arg(i).c_str(), NULL, 10);
        ws2812fx.setSpeed(tmp);
      }
      Serial.print("speed is "); Serial.println(ws2812fx.getSpeed());
    }

    if (server.argName(i) == "a") {
      if (server.arg(i)[0] == '-') {
        auto_cycle = false;
      } else {
        auto_cycle = true;
        auto_last_change = 0;
      }
    }
  }
  server.send(200, "text/plain", "OK");
}

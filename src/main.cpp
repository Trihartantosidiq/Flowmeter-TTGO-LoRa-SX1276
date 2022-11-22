#include <Arduino.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <SPI.h>
#include <LoRa.h>
#include <EEPROM.h>

#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_SSD1306.h>

//define the pins used by the LoRa transceiver module
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26

#define BAND 866E6 //Band LoRa SX1276

//OLED pins
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
WiFiClient net;

const char *WIFI_SSID = "SERENITY_plus";
const char *WIFI_PASSWORD = "babykenya";

#define jsonSize 300
#define DEVICE_NAME "FLOWMETER"
#define MQTT_PORT 1883
#define MQTT_SERVER "habibigarden.com"
#define MQTT_USER "habibi"
#define MQTT_PASS "prodigy123"
#define SENSOR 12
#define LED 0
#define pinTurb 34
#define Buzzer 17
#define MQTT_IOT_TOPIC "main/status/Skripsi/flowmeter_1"
#define EEPROM_SIZE 1

#define AWS_MAX_RECONNECT_TRIES 50
MQTTClient client = MQTTClient(jsonSize);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RST);

int interval = 1000;
int interval2 = 5000;
int SensorKekeruhan;
boolean ledState = false, led = false;
boolean buzzState = false, buzz = false;
boolean buzzState1 = false, buzz1 = false;
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;
unsigned int flowMilliLitres, kalkulasiAir;
unsigned long totalMilliLitres, totalMilliLitresPrev, counterKalkulasiAir; 
unsigned long currentMillis = 0, previousMillis = 0, previousMillis2 = 0, mainTimer = 0; 
unsigned long prevPerJam = 0, prevRead = 0, prevTampil = 0, millisLED=0, millisbuzz=0, millisbuzz1=0, prevPerHari = 0;
volatile int count;
static float V, kekeruhan, volt;


void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void connectToWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 15)
  {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if(WiFi.status() != WL_CONNECTED){
    ledState = false;
  }else
  {
    ledState = true;
  }
}

void connectToMQTT()
{
  client.begin(MQTT_SERVER, MQTT_PORT, net);

  int retries = 0;
  Serial.print("Connecting to MQTT SERVER");

  while (!client.connect(DEVICE_NAME, MQTT_USER, MQTT_PASS) && retries < AWS_MAX_RECONNECT_TRIES)
  {
    Serial.print(".");
    delay(100);
    retries++;
  }

  if (!client.connected())
  {
    Serial.println(" Timeout!");
    ledState = false;
    return;
  }

  Serial.println("Connected!");
  ledState = true;
}
void Flow()
{
  count++;
}
void setup()
{
  Serial.begin(115200);
  pinMode(SENSOR, INPUT_PULLUP);
  pinMode(pinTurb, INPUT);
  pinMode(OLED_RST, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(Buzzer, OUTPUT);
  digitalWrite(LED, HIGH);
  digitalWrite(Buzzer, LOW);
  attachInterrupt(0, Flow, RISING);
  digitalWrite(OLED_RST, LOW);
  EEPROM.begin(EEPROM_SIZE);
  delay(20);
  digitalWrite(OLED_RST, HIGH);
  LoRa.setPins(18, 14, 26);
  

  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  previousMillis = 0;
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);

  flowRate = EEPROM.read(0);
  float totalMilliLitres = EEPROM.read(1);
  float kalkulasiAir = EEPROM.read(2);
  kekeruhan = EEPROM.read(3);


  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false))
  { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(30, 10);
  display.print("LORA SENDER ");
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(25, 20);
  display.print("HABIBI GARDEN");
  display.display();

  connectToWiFi();

  if (WiFi.isConnected() == true)
  {
    connectToMQTT();
    Serial.println("sudah terhubung");
    ledState = true;
  }
  else
  {
    connectToWiFi();
    Serial.println("Menghubungkan Ulang");
    connectToMQTT();
  }

  //SPI LoRa pins
  SPI.begin(SCK, MISO, MOSI, SS);
  //setup LoRa transceiver module
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(BAND))
  {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }
  if (LoRa.begin(BAND))
  Serial.println("LoRa Initializing OK!");
  display.clearDisplay();
  display.setCursor(25, 10);
  display.print("LORA SENDING OK!");
  display.display();
  delay(1000);
  digitalWrite(LED, LOW);
}

void sendJsonToMQTT()
{
  StaticJsonDocument<jsonSize> JsonDoc;
  JsonObject info = JsonDoc.createNestedObject("info");
  info["debit"] = flowRate;
  info["total"] = totalMilliLitres/1000;
  info["average_perhour"] = kalkulasiAir/1000;
  info["Turb"] = kekeruhan;

  char JsonBuf[jsonSize];
  serializeJson(JsonDoc, JsonBuf);

  client.publish(MQTT_IOT_TOPIC, JsonBuf);
}
void loop(){
  

  currentMillis = millis();
  if (currentMillis - previousMillis2 > 1000)
  {
    previousMillis2 = currentMillis;
    mainTimer++;
  }
  if (currentMillis - previousMillis > interval)
  {
    volt = 0;
    for(int i=0; i<200; i++)
    {
        volt += ((float)analogRead(pinTurb)/1027);
    }
    volt = volt;
  
    kekeruhan = analogRead(pinTurb);
    //V = SensorKekeruhan*(5.0/1027);
    //kekeruhan =2.8 + (V*100.00);


    pulse1Sec = pulseCount;
    pulseCount = 0;
    flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
    previousMillis = millis();

    flowMilliLitres = (flowRate / 70) * 1000;

    totalMilliLitres += flowMilliLitres;
    if (totalMilliLitres != totalMilliLitresPrev)
    {
      counterKalkulasiAir = totalMilliLitres - totalMilliLitresPrev;
      kalkulasiAir = kalkulasiAir + counterKalkulasiAir;
      totalMilliLitresPrev = totalMilliLitres;
    }
  }

  if (mainTimer >= prevPerJam + 3600)
  {
    kalkulasiAir = 0;
    prevPerJam = mainTimer;
  }
  if (mainTimer >= prevPerHari + 7200)
  {
    kalkulasiAir = 0;
    totalMilliLitres = 0;
    prevPerHari = mainTimer;
  }
  
  LoRa.beginPacket();
  LoRa.print("DEBIT :");
  LoRa.print(flowRate);
  LoRa.println(" L/min");
  LoRa.print("TOTAL :");
  LoRa.print((float)totalMilliLitres/1000);
  LoRa.println(" L");
  LoRa.print("AVG/H :");
  LoRa.print((float)kalkulasiAir/1000);
  LoRa.println(" L/Jam");
  LoRa.print("TURB  :");
  LoRa.print(kekeruhan);
  LoRa.println(" NTU");
  LoRa.endPacket();
  LoRa.sleep();
  EEPROM.write(0, flowRate);
  EEPROM.write(1, (float)totalMilliLitres/1000 );
  EEPROM.write(2, (float)kalkulasiAir/1000);
  EEPROM.write(3, kekeruhan);
  EEPROM.commit();
  display.clearDisplay();
  display.setCursor(25,0);
  display.setTextSize(1);
  display.print("FLOWMETER 1");
  display.setCursor(0, 10);
  display.setTextSize(1);
  display.print("DEBIT  :");
  display.print(flowRate);
  display.print(" L/min");
  display.setCursor(0, 20);
  display.setTextSize(1);
  display.print("TOTAL  :");
  display.print((float)totalMilliLitres/1000);
  display.print(" L ");
  display.setCursor(0, 30);
  display.setTextSize(1);
  display.print("AVG/H  :");
  display.print((float)kalkulasiAir/1000);
  display.print(" L/Jam ");
  display.setCursor(0, 40);
  display.setTextSize(1);
  display.print("TURB   :");
  display.print(kekeruhan);
  display.print(" NTU");
  
  display.display();
  if ((kekeruhan>=0)&&(kekeruhan<=25)){
    Serial.print("Status Air  :");
    Serial.println("AIR BERSIH");
    display.setCursor(0,50);
    display.setTextSize(1);
    display.print("ST.AIR :");
    display.print("BERSIH");
    display.display();
    buzzState = false;
    buzzState1 = false;
  }
  else if((kekeruhan>=26)&&(kekeruhan<=49)){
      Serial.print("Status Air  :");
      Serial.println("SEDANG");
      display.setCursor(0,50);
      display.setTextSize(1);
      display.print("ST.AIR :");
      display.print("SEDANG");
      display.display();
      buzzState = false;
      buzzState1 = false;
    }
    else if((kekeruhan>=50)&&(kekeruhan<=100)){
      Serial.print("Status Air  :");
      Serial.println("KERUH");
      display.setCursor(0,50);
      display.setTextSize(1);
      display.print("ST.AIR :");
      display.print("KERUH");
      display.display();
      buzzState = true;
      buzzState1 = false;
    }
     else{
        Serial.print("Status Air  :");
        Serial.println("SNGT KERUH");
        display.setCursor(0,50);
        display.setTextSize(1);
        display.print("ST.AIR :");
        display.print("SNGT KERUH");
        display.display();
        buzzState1 = true;
        buzzState = false;
        }
  if (mainTimer >= prevTampil + 5)
  {
    // DEBIT/MENIT
    Serial.print(" Debit              : ");
    Serial.print(int(flowRate)); // Print the integer part of the variable
    Serial.println(" L/min ");

    // TOTAL DARI NYALA
    Serial.print(" Total              : ");
    Serial.print(totalMilliLitres);
    Serial.println(" L ");

    // RATA-RATA PERJAM
    Serial.print(" Rata-rata Air/Jam  : ");
    Serial.print(kalkulasiAir);
    Serial.println(" L/Jam");
    prevTampil = mainTimer;
    
    //TURBIDITY
    Serial.print("Voltage             :");
    Serial.print(volt);
    Serial.println("  V");
    Serial.print("Nilai ADC           :");
    Serial.println(V);
    Serial.print("kekeruhan           :");
    Serial.print(kekeruhan);
    Serial.println("  NTU");
    //ngirim mqtt
    if (WiFi.isConnected() == true)
      {
        if (client.connected())
        {
          Serial.println("Sudah Terhubung");
          ledState = true;
          sendJsonToMQTT();
        }
        else
        {
          Serial.print("Tidak Dapat Terhubung");
          ledState = false;
          connectToMQTT();
          digitalWrite(LED, HIGH);
          if (client.connected())
          {
            sendJsonToMQTT();
            ledState = true;
          }
          else
          {
            Serial.println("Gagal Reconnect MQTT");
            ledState = false;
          }
        }
      }
      else
      {
        Serial.println("Menghubungkan Ulang");
        digitalWrite(LED, HIGH);
        connectToWiFi();
        if (WiFi.isConnected() == true)
        {
          connectToMQTT();
          if (client.connected())
          {
            sendJsonToMQTT();
            ledState = true;
          }
          else
          {
            Serial.println("Gagal Reconnect MQTT");
            ledState = false;
          }
        }
        else
        {
          Serial.println("Wifi Gagal Reconnect");
          ledState = false;
        }
      }
    }
  if (buzzState == true)
  {
    if(currentMillis - millisbuzz > 1500){
      buzz = ! buzz;
      digitalWrite(Buzzer, buzz);
      millisbuzz = currentMillis;
    }
  }

  if (buzzState1 == true)
  {
    if(currentMillis - millisbuzz1 > 5000){
      buzz1 = ! buzz1;
      digitalWrite(Buzzer, buzz1);
      millisbuzz1 = currentMillis;
    }
  }
  
    //LED
  if (ledState == false) {
    if (currentMillis - millisLED > 200) {
      led = ! led;
      digitalWrite(LED, led);
      millisLED = currentMillis;
    }
  } else if (ledState == true) {
    if (currentMillis - millisLED > 2000) {
      led = ! led;
      digitalWrite(LED, led);
      millisLED = currentMillis;
    }

    //digitalWrite(pinLED, LOW);
  }
  
}
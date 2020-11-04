#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int waterPin = A0;
const int pumpPin = 0;
const int dhtPin = 2;
const int lampPin = 13;
const int motorPin = 12;

const char* ssid = "Egga-Machine"; // SSID
const char* password = "1234567890"; // Password

AsyncWebServer server(80);
ESPDash dashboard(&server); 
DHT dht(dhtPin, DHT11);
LiquidCrystal_I2C lcd(0x27,16,2);  

Card temp(&dashboard, TEMPERATURE_CARD, "Temperature", "°C");
Card hum(&dashboard, HUMIDITY_CARD, "Humidity", "%");
Card water(&dashboard, GENERIC_CARD, "Water Level", "%");
Card mintemp(&dashboard, SLIDER_CARD, "Min Temp", "°C", 20, 50);
Card maxtemp(&dashboard, SLIDER_CARD, "Max Temp", "°C", 20, 50);
Card timer(&dashboard, SLIDER_CARD, "Timer", "Minutes", 1, 1000);
Card motortimer(&dashboard, SLIDER_CARD, "Timer Motor On", "Seconds", 1, 100);
Card runmode(&dashboard, BUTTON_CARD, "Running Mode");
Card lamp(&dashboard, BUTTON_CARD, "Lamp Test");
Card pump(&dashboard, BUTTON_CARD, "Pump Test");
Card motor(&dashboard, BUTTON_CARD, "DC Motor Test");

float ft = 0.0;
int ih = 0;
int itmin = 37;
int itmax = 40;
int itimer = 1;
int iwater = 0;
int iwaterlevel = 0;
int imotortimer = 10;

bool brunningMode = false;
bool blampStatus = false;
bool bpumpStatus = false;
bool bmotorStatus = false;

unsigned long previousMillis = 0;
unsigned long timerMillis = 0;
unsigned long timerTotal = 0;

void function_callback(){
  maxtemp.attachCallback([&](int value){
    Serial.println("Slider Max Temp Triggered: "+String(value));
    itmax = value;
    maxtemp.update(itmax);
    dashboard.sendUpdates();
  });
  
  mintemp.attachCallback([&](int value){
    Serial.println("Slider Min Temp Triggered: "+String(value));
    itmin = value;
    mintemp.update(itmin);
    dashboard.sendUpdates();
  });

  timer.attachCallback([&](int value){
    Serial.println("Slider Timer Triggered: "+String(value));
    itimer = value;
    timer.update(itimer);
    dashboard.sendUpdates();
  });

  motortimer.attachCallback([&](int value){
    Serial.println("Slider Timer Motor On Triggered: "+String(value));
    imotortimer = value;
    motortimer.update(imotortimer);
    dashboard.sendUpdates();
  });
  
  runmode.attachCallback([&](bool value){
    Serial.println("Button Running Mode Triggered: "+String((value)?"true":"false"));
    brunningMode = value;
    if(brunningMode){
      unsigned long currentMillis = millis();
      previousMillis = currentMillis;
    } else {
      digitalWrite(pumpPin, HIGH);
      digitalWrite(lampPin, HIGH);
      digitalWrite(motorPin, HIGH);
    }
    runmode.update(brunningMode);
    dashboard.sendUpdates();
  });

  lamp.attachCallback([&](bool value){
    blampStatus = value;
    if(!brunningMode) lamp_run();
    lamp.update(blampStatus);
    dashboard.sendUpdates();
  });

  pump.attachCallback([&](bool value){
    bpumpStatus = value;
    if(!brunningMode) pump_run();
    pump.update(bpumpStatus);
    dashboard.sendUpdates();
  });

  motor.attachCallback([&](bool value){
    bmotorStatus = value;
    if(!brunningMode) motor_run();
    motor.update(bmotorStatus);
    dashboard.sendUpdates();
  });
}

void init_pin(){
  pinMode(dhtPin, INPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(lampPin, OUTPUT);
  pinMode(motorPin, OUTPUT);

  digitalWrite(pumpPin, HIGH);
  digitalWrite(lampPin, HIGH);
  digitalWrite(motorPin, HIGH);
}

void setup() {
  Serial.begin(115200);
  init_pin();
  lcd.init();
  lcd.backlight();
  dht.begin();
  WiFi.softAP(ssid, password);

  lcd.setCursor(0,0);
  lcd.print("EGGA-Machine");
  lcd.setCursor(0,1);
  lcd.print("by 2black0");
  delay(1000);
  function_callback();
  server.begin();
}

void loop() {
  ft = dht.readTemperature();
  ih = dht.readHumidity();
  if (isnan(ft) || isnan(ih)) {
    Serial.println("Failed to read from DHT sensor!");
    ft = 0;
    ih = 0;
  }

  iwater = analogRead(waterPin);
  //834,787,705,530
  if(iwater > 800) {
    iwaterlevel = 25;
  } else if(iwater >= 750 && iwater < 800) {
    iwaterlevel = 50;
  } else if(iwater >= 650 && iwater < 750) {
    iwaterlevel = 75;
  } else {
    iwaterlevel = 100;
  }
  
  if(brunningMode){
    check_temp();
    check_water();
    check_time();
  }

  int xtimer = timerTotal / 60000;
  lcd_show(1, "t:" + String(ft) + " h:" + String(ih) + " " + String(blampStatus) + String(bpumpStatus) + String(bmotorStatus), "w:" + String(iwaterlevel) + " ti:" + String(xtimer));
  
  temp.update(ft);
  hum.update(ih);
  water.update(iwaterlevel);
  maxtemp.update(itmax);
  mintemp.update(itmin);
  timer.update(itimer);
  motortimer.update(imotortimer);
  runmode.update(brunningMode);
  lamp.update(blampStatus);
  pump.update(bpumpStatus);
  motor.update(bmotorStatus);
  dashboard.sendUpdates();
  delay(5000);
}

void lcd_show(int clr, String line1, String line2){
  if(clr) lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);
}

void check_temp(){
  if((ft > itmin) && (ft < itmax)){
    blampStatus = true;
  } else {
    blampStatus = false;
  }
  lamp_run();
}

void lamp_run(){
  if(blampStatus){
    digitalWrite(lampPin, LOW);
    Serial.println("Lamp ON");
  } else {
    digitalWrite(lampPin, HIGH);
    Serial.println("Lamp OFF");
  }
}

void check_water(){
  if(iwaterlevel >= 25 && iwaterlevel < 100) {
    bpumpStatus = true;
  } else {
    bpumpStatus = false;
  }
  pump_run();
}

void pump_run(){
  if(bpumpStatus){
    digitalWrite(pumpPin, LOW);
    Serial.println("Pump ON");
  } else {
    digitalWrite(pumpPin, HIGH);
    Serial.println("Pump OFF");
  }
}

void check_time(){
  unsigned long currentMillis = millis();
  timerMillis = currentMillis - previousMillis;
  timerTotal = timerTotal + timerMillis;
  previousMillis = currentMillis;
  Serial.print("Total Timer : ");
  Serial.println(timerTotal);
  Serial.print("Set Timer : ");
  Serial.println(long(itimer * 60000));
  if(timerTotal >= long(itimer * 60000)){
    timerTotal = 0;
    bmotorStatus = true;
    digitalWrite(motorPin, LOW);
    delay(imotortimer);
    currentMillis = millis();
    previousMillis = currentMillis;
  } else {
    bmotorStatus = false;
    digitalWrite(motorPin, HIGH);
  }
}

void motor_run(){
  if(bmotorStatus){
    digitalWrite(motorPin, LOW);
    Serial.println("Motor ON");
  } else {
    digitalWrite(motorPin, HIGH);
    Serial.println("Motor OFF");
  }
}

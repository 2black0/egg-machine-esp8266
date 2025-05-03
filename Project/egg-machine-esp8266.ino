/**
 * Egg Incubator Machine Controller using ESP8266
 * 
 * This project implements an automatic egg incubator with web-based control dashboard.
 * Features:
 * - Temperature monitoring and control using a lamp
 * - Humidity monitoring
 * - Water level monitoring and pump control
 * - Periodic egg turning via motor
 * - LCD display for status indication
 * - Web dashboard for remote monitoring and control
 */

// Include required libraries
#include <Arduino.h>          // Core Arduino functionality
#include <ESP8266WiFi.h>      // WiFi functionality for ESP8266
#include <ESPAsyncTCP.h>      // Asynchronous TCP for ESP8266
#include <ESPAsyncWebServer.h> // Asynchronous web server
#include <ESPDash.h>          // Dashboard library for ESP
#include <DHT.h>              // DHT temperature/humidity sensor library
#include <Wire.h>             // I2C communication library
#include <LiquidCrystal_I2C.h> // LCD library using I2C interface

// Pin definitions
const int waterPin = A0;      // Analog pin for water level sensor
const int pumpPin = 0;        // Digital pin for water pump control
const int dhtPin = 2;         // Digital pin for DHT temperature/humidity sensor
const int lampPin = 13;       // Digital pin for heating lamp control
const int motorPin = 12;      // Digital pin for egg turning motor control

// WiFi Access Point configuration
const char* ssid = "Egga-Machine"; // WiFi network name
const char* password = "1234567890"; // WiFi password

// Initialize server and components
AsyncWebServer server(80);    // Web server on port 80
ESPDash dashboard(&server);   // Dashboard instance
DHT dht(dhtPin, DHT11);       // DHT sensor instance (DHT11 type)
LiquidCrystal_I2C lcd(0x27,16,2); // LCD with I2C address 0x27, 16 columns, 2 rows

// Create dashboard cards for monitoring and control
Card temp(&dashboard, TEMPERATURE_CARD, "Temperature", "°C");      // Temperature display
Card hum(&dashboard, HUMIDITY_CARD, "Humidity", "%");             // Humidity display
Card water(&dashboard, GENERIC_CARD, "Water Level", "%");         // Water level display
Card mintemp(&dashboard, SLIDER_CARD, "Min Temp", "°C", 20, 50);  // Min temperature control
Card maxtemp(&dashboard, SLIDER_CARD, "Max Temp", "°C", 20, 50);  // Max temperature control
Card timer(&dashboard, SLIDER_CARD, "Timer", "Minutes", 1, 1000); // Timer for egg turning
Card motortimer(&dashboard, SLIDER_CARD, "Timer Motor On", "Seconds", 1, 100); // Motor run duration
Card runmode(&dashboard, BUTTON_CARD, "Running Mode");            // Auto/manual mode toggle
Card lamp(&dashboard, BUTTON_CARD, "Lamp Test");                  // Manual lamp control
Card pump(&dashboard, BUTTON_CARD, "Pump Test");                  // Manual pump control
Card motor(&dashboard, BUTTON_CARD, "DC Motor Test");             // Manual motor control

// Variables for sensor readings and settings
float ft = 0.0;               // Current temperature
int ih = 0;                   // Current humidity
int itmin = 37;               // Minimum temperature threshold (default 37°C)
int itmax = 40;               // Maximum temperature threshold (default 40°C)
int itimer = 1;               // Time between egg turning cycles (in minutes)
int iwater = 0;               // Raw water sensor reading
int iwaterlevel = 0;          // Calculated water level percentage
int imotortimer = 10;         // Duration for motor to run (in seconds)

// Status flags
bool brunningMode = false;    // Auto/manual mode flag (true = auto mode)
bool blampStatus = false;     // Lamp status (true = on)
bool bpumpStatus = false;     // Pump status (true = on)
bool bmotorStatus = false;    // Motor status (true = on)

// Timing variables
unsigned long previousMillis = 0; // Last time reading was taken
unsigned long timerMillis = 0;    // Time elapsed since last check
unsigned long timerTotal = 0;     // Total time elapsed for egg turning cycle

/**
 * Set up dashboard callbacks for interactive control
 * These functions handle user actions from the web dashboard
 */
void function_callback(){
  // Callback for maximum temperature slider
  maxtemp.attachCallback([&](int value){
    Serial.println("Slider Max Temp Triggered: "+String(value));
    itmax = value;
    maxtemp.update(itmax);
    dashboard.sendUpdates();
  });
  
  // Callback for minimum temperature slider
  mintemp.attachCallback([&](int value){
    Serial.println("Slider Min Temp Triggered: "+String(value));
    itmin = value;
    mintemp.update(itmin);
    dashboard.sendUpdates();
  });

  // Callback for egg turning timer slider
  timer.attachCallback([&](int value){
    Serial.println("Slider Timer Triggered: "+String(value));
    itimer = value;
    timer.update(itimer);
    dashboard.sendUpdates();
  });

  // Callback for motor runtime slider
  motortimer.attachCallback([&](int value){
    Serial.println("Slider Timer Motor On Triggered: "+String(value));
    imotortimer = value;
    motortimer.update(imotortimer);
    dashboard.sendUpdates();
  });
  
  // Callback for auto/manual mode toggle button
  runmode.attachCallback([&](bool value){
    Serial.println("Button Running Mode Triggered: "+String((value)?"true":"false"));
    brunningMode = value;
    if(brunningMode){
      // When entering auto mode, reset timer
      unsigned long currentMillis = millis();
      previousMillis = currentMillis;
    } else {
      // When switching to manual mode, ensure all devices are off (HIGH = off)
      digitalWrite(pumpPin, HIGH);
      digitalWrite(lampPin, HIGH);
      digitalWrite(motorPin, HIGH);
    }
    runmode.update(brunningMode);
    dashboard.sendUpdates();
  });

  // Callback for manual lamp control button
  lamp.attachCallback([&](bool value){
    blampStatus = value;
    if(!brunningMode) lamp_run(); // Only allow manual control when not in auto mode
    lamp.update(blampStatus);
    dashboard.sendUpdates();
  });

  // Callback for manual pump control button
  pump.attachCallback([&](bool value){
    bpumpStatus = value;
    if(!brunningMode) pump_run(); // Only allow manual control when not in auto mode
    pump.update(bpumpStatus);
    dashboard.sendUpdates();
  });

  // Callback for manual motor control button
  motor.attachCallback([&](bool value){
    bmotorStatus = value;
    if(!brunningMode) motor_run(); // Only allow manual control when not in auto mode
    motor.update(bmotorStatus);
    dashboard.sendUpdates();
  });
}

/**
 * Initialize digital pins and set default states
 */
void init_pin(){
  pinMode(dhtPin, INPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(lampPin, OUTPUT);
  pinMode(motorPin, OUTPUT);

  // Set all outputs to HIGH (off) initially
  digitalWrite(pumpPin, HIGH);
  digitalWrite(lampPin, HIGH);
  digitalWrite(motorPin, HIGH);
}

/**
 * Initial setup function
 */
void setup() {
  Serial.begin(115200);      // Initialize serial communication
  init_pin();                // Set up pins
  lcd.init();                // Initialize LCD
  lcd.backlight();           // Turn on LCD backlight
  dht.begin();               // Initialize DHT sensor
  WiFi.softAP(ssid, password); // Start WiFi in Access Point mode

  // Display startup message on LCD
  lcd.setCursor(0,0);
  lcd.print("EGGA-Machine");
  lcd.setCursor(0,1);
  lcd.print("by 2black0");
  delay(1000);
  
  function_callback();       // Set up dashboard callbacks
  server.begin();            // Start web server
}

/**
 * Main program loop
 */
void loop() {
  // Read temperature and humidity from DHT sensor
  ft = dht.readTemperature();
  ih = dht.readHumidity();
  if (isnan(ft) || isnan(ih)) {
    Serial.println("Failed to read from DHT sensor!");
    ft = 0;
    ih = 0;
  }

  // Read water level sensor and calculate percentage
  iwater = analogRead(waterPin);
  // Convert analog reading to percentage based on predefined thresholds
  // 834,787,705,530 are calibration points
  if(iwater > 800) {
    iwaterlevel = 25;        // 25% full
  } else if(iwater >= 750 && iwater < 800) {
    iwaterlevel = 50;        // 50% full
  } else if(iwater >= 650 && iwater < 750) {
    iwaterlevel = 75;        // 75% full
  } else {
    iwaterlevel = 100;       // 100% full
  }
  
  // Run automatic control if in auto mode
  if(brunningMode){
    check_temp();  // Check and control temperature
    check_water(); // Check and control water level
    check_time();  // Check and control egg turning timing
  }

  // Calculate elapsed time in minutes for display
  int xtimer = timerTotal / 60000;
  
  // Update LCD display with current status
  // Format: "t:[temp] h:[humidity] [lamp][pump][motor]"
  //         "w:[water] ti:[timer]"
  lcd_show(1, "t:" + String(ft) + " h:" + String(ih) + " " + String(blampStatus) + String(bpumpStatus) + String(bmotorStatus), 
             "w:" + String(iwaterlevel) + " ti:" + String(xtimer));
  
  // Update all dashboard cards with current values
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
  
  delay(5000); // Wait 5 seconds before next reading
}

/**
 * Update the LCD display with text
 * @param clr   Whether to clear the LCD first (1 = clear)
 * @param line1 Text for the first line
 * @param line2 Text for the second line
 */
void lcd_show(int clr, String line1, String line2){
  if(clr) lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);
}

/**
 * Check temperature and control lamp accordingly
 * If temperature is between min and max, turn on lamp
 */
void check_temp(){
  if((ft > itmin) && (ft < itmax)){
    blampStatus = true;
  } else {
    blampStatus = false;
  }
  lamp_run();
}

/**
 * Control the heating lamp
 * LOW = on, HIGH = off (relay logic)
 */
void lamp_run(){
  if(blampStatus){
    digitalWrite(lampPin, LOW);  // Turn lamp ON
    Serial.println("Lamp ON");
  } else {
    digitalWrite(lampPin, HIGH); // Turn lamp OFF
    Serial.println("Lamp OFF");
  }
}

/**
 * Check water level and control pump accordingly
 * Turn on pump if water is between 25% and 100%
 */
void check_water(){
  if(iwaterlevel >= 25 && iwaterlevel < 100) {
    bpumpStatus = true;
  } else {
    bpumpStatus = false;
  }
  pump_run();
}

/**
 * Control the water pump
 * LOW = on, HIGH = off (relay logic)
 */
void pump_run(){
  if(bpumpStatus){
    digitalWrite(pumpPin, LOW);  // Turn pump ON
    Serial.println("Pump ON");
  } else {
    digitalWrite(pumpPin, HIGH); // Turn pump OFF
    Serial.println("Pump OFF");
  }
}

/**
 * Check elapsed time and control motor for egg turning
 * Motor turns eggs at intervals specified by itimer (minutes)
 */
void check_time(){
  // Calculate elapsed time
  unsigned long currentMillis = millis();
  timerMillis = currentMillis - previousMillis;
  timerTotal = timerTotal + timerMillis;
  previousMillis = currentMillis;
  
  Serial.print("Total Timer : ");
  Serial.println(timerTotal);
  Serial.print("Set Timer : ");
  Serial.println(long(itimer * 60000));
  
  // Check if it's time to turn the eggs
  if(timerTotal >= long(itimer * 60000)){
    timerTotal = 0;
    bmotorStatus = true;
    digitalWrite(motorPin, LOW);     // Turn motor ON
    delay(imotortimer * 1000);       // Run motor for specified seconds
    
    // Reset timer after motor operation
    currentMillis = millis();
    previousMillis = currentMillis;
  } else {
    bmotorStatus = false;
    digitalWrite(motorPin, HIGH);    // Turn motor OFF
  }
}

/**
 * Control the egg turning motor
 * LOW = on, HIGH = off (relay logic)
 */
void motor_run(){
  if(bmotorStatus){
    digitalWrite(motorPin, LOW);     // Turn motor ON
    Serial.println("Motor ON");
  } else {
    digitalWrite(motorPin, HIGH);    // Turn motor OFF
    Serial.println("Motor OFF");
  }
}
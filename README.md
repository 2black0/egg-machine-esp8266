# 🥚 Egg Machine ESP8266 – Smart Incubator Controller

A web-based **incubator controller** using **ESP8266** and **ESPDash** for real-time monitoring and control of temperature, humidity, water level, and actuators (lamp, pump, motor). The system is accessible via Wi-Fi AP mode and includes a dashboard with sliders, buttons, and sensor visualization.

---

## 🔧 Features

- 📡 **Wi-Fi Access Point** mode (no external Wi-Fi needed)
- 🌡️ DHT11 temperature & humidity monitoring
- 💧 Water level detection via analog sensor
- 💡 Lamp control based on temperature
- 🚰 Water pump auto-control based on water level
- ⚙️ Motor timing and control for egg rotation
- 📟 LCD I2C display for live feedback
- 📊 **Dashboard interface** using [ESPDash](https://github.com/ayushsharma82/ESPDash)
- 🕹️ Remote control: sliders and toggle buttons

---

## 📁 File Structure

```

egg-machine-esp8266/
├── egg-machine-esp8266.ino     # Main Arduino sketch
├── README.md                   # Project documentation
├── LICENSE                     # Optional MIT License

```

---

## 🛠️ Hardware Requirements

| Component              | Description                              |
|------------------------|------------------------------------------|
| ESP8266 Board          | e.g. Wemos D1 Mini                       |
| DHT11 Sensor           | For temperature and humidity             |
| Analog water sensor    | For water level detection                |
| I2C LCD (16x2)         | Display sensor readings and status       |
| Relay module / MOSFET  | For lamp, pump, and motor control        |
| Lamp (incubator heat)  | Controlled by temp thresholds            |
| Water pump             | Maintains humidity                       |
| Motor (DC/servo)       | Turns eggs based on timer                |

---

## 🔌 Wiring Table

| Component           | ESP8266 Pin (Wemos D1 Mini) | Description                      |
|---------------------|-----------------------------|----------------------------------|
| DHT11 Sensor        | D4 (GPIO2)                  | Digital data pin                 |
| Water Sensor (AO)   | A0                          | Analog output for water level    |
| LCD I2C SDA         | D2 (GPIO4)                  | I2C data                         |
| LCD I2C SCL         | D1 (GPIO5)                  | I2C clock                        |
| Lamp (via relay)    | D7 (GPIO13)                 | Relay control                    |
| Pump (via relay)    | D3 (GPIO0)                  | Relay control                    |
| Motor (via relay)   | D6 (GPIO12)                 | Relay control                    |
| VCC (All modules)   | 5V or 3.3V                  | Power source                     |
| GND (All modules)   | GND                         | Ground                           |

> ⚠️ Note: Ensure proper use of MOSFET/relay driver circuits to avoid overloading ESP8266.

---

## 📡 Access Point Configuration

```cpp
const char* ssid = "Egga-Machine";
const char* password = "1234567890";
WiFi.softAP(ssid, password);
```

Once powered on, connect to Wi-Fi `Egga-Machine` using the password `1234567890`
Then open your browser and go to: `http://192.168.4.1/`

---

## 🖥️ ESPDash Web Interface

The dashboard includes:

* **Temperature** & **Humidity** live display
* **Water Level** (% scale from sensor)
* **Sliders**:

  * Minimum and maximum temperature setpoints
  * Incubation timer (minutes)
  * Motor on-time (seconds)
* **Buttons**:

  * Toggle run mode (auto/manual)
  * Manual test for lamp, pump, and motor

---

## 📟 LCD Display Content

The LCD shows:

* Temperature (`t`) and Humidity (`h`)
* Water level (`w`)
* Elapsed timer (`ti`)
* Actuator statuses (lamp, pump, motor)

---

## 🧠 Logic Overview

| Function        | Description                                                         |
| --------------- | ------------------------------------------------------------------- |
| `check_temp()`  | Turns lamp ON if temp in range `[min, max]`                         |
| `check_water()` | Turns pump ON if water < 100%                                       |
| `check_time()`  | Activates motor every `timer` minutes, runs it for `motortimer` sec |
| `lcd_show()`    | Displays two lines of data on I2C LCD                               |
| `*_run()`       | Controls individual actuator status                                 |

---

## 🔃 Control Flow Summary

* System enters **running mode** when activated from dashboard
* Temperature and water level are checked every cycle
* Motor is triggered based on total elapsed time
* Manual override possible for all actuators

---

## 🧪 Example Output (Serial Monitor)

```txt
Slider Max Temp Triggered: 40
Slider Min Temp Triggered: 37
Button Running Mode Triggered: true
Lamp ON
Pump OFF
Motor OFF
Total Timer : 180003
Set Timer : 300000
```

---

## 📜 License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

## 🙋‍♂️ Author

Developed by **Ardy Seto**
For embedded IoT incubator research and educational use.
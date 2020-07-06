# esp32-incubator

DIY incubator system based on ESP32.

The article for how to make is [here](https://xor.hateblo.jp/entry/20200630/1593507600) (Japanese).

## Installation

1. Install board environment for **"esp32"** by Arduino IDE: Tools > Boards: XXXXX > Boards Manager...

2. Install some libraries (show below) from Arduino IDE: Tools > Manage Libraries...  
  - Ambient ESP32 ESP8266 Lib
  - DHT sensor library for ESPx
  - ESP32Servo
  - PubSubClient

3. Select compile & upload options for ESP32 like below.

  for ESP32 WROOM Module:
  ![esp32dev](resources/dev-conf.png)
  
  for ESP32 WROVER Module:
  ![esp32wrover](resources/wrover-conf.png)
  
4. Create hardware and connect pins to sensors/actuators defined at the main sketch.

  | #define pin number | description |
  |:--|:--|
  | BUILTIN_LED | Indicator LED for heater On/Off |
  | DCfan_PIN | VCC pin of DC fan (need driver IC like MOSFET) |
  | DHTPIN | SDA pin of Temp/Humi sensor DHT22 (AM2302) |
  | Heater_PIN | Signal input pin of solid state relay with heater |
  | SERVO_PIN | PWM input pin of servo motor MG996R |

5. Rename `data/config_ro.sample.txt` to `data/config_ro.txt` and edit for your environment. It includes like wi-fi settings.

6. Rename `data/config_rw.sample.txt` to `data/config_rw.txt`. In most cases, this does not need to be edit.

7. Click [ Arduino IDE: Tools > ESP32 Sketch Data Upload ] to upload config files to ESP32's SPIFFS.

8. Upload sketch to ESP32 and run.

## Control via MQTT

1. Install mosquitto, see [here](https://mosquitto.org/download/).

2. Rename `incubator.sh` to `incubator` and edit readonly variables like MQTT_USER for your environment.

3. On your shell, `cd ~/YOUR_DOCUMENTS_DIR/esp32-incubator`.

4. Run `chmod +x ./incubator` to be executable the script.

5. Run script with control commands.  
  
  ```
  $ cd ~/YOUR_DOCUMENTS/esp32-incubator
  
  # ping command
  $ ./incubator ping
  >>> "ping"
  [2020/07/07(Tue) 07:07:07] Pong
  
  # get command ex.
  $ ./incubator get temp
  >>> "get temp"
  [2020/07/07(Tue) 07:07:07] [GET] Temperature = 38.50
  
  # set command ex.
  $ ./incubator set kd:0.1
  >>> "set kd:0.1"
  [2020/07/07(Tue) 07:07:07] [SET] PID coefficients (OK) set as Kp = 1.20, Ki = 0.20, Kd = 0.10
  ```

### Get commands

Usage: `./incubator get $KEY`

| \$KEY | response value |
|:--|:--|
| temp | Current temperature |
| humd | Current Humidity |
| degrees | Current degrees of servo moter for egg rotation |
| duty | Current PID output value |
| rotate\_interval | (readonly) Egg rotation intarval seconds |
| rotate\_max\_degrees | (readonly) Maximum servo degrees value |
| rotate\_min\_degrees | (readonly) Minimum servo degrees value |
| epoch\_start | Unix time when started incubating |
| rotate_onoff | Status of auto egg rotation (0 is Off, 1 is On) |
| kp | PID coefficient for heater control (Proportional term) |
| ki | PID coefficient for heater control (Integral term) |
| kd | PID coefficient for heater control (Derivative term) |

### Set commands

Usage: `./incubator set $KEY:$VALUE`

| \$KEY | \$VALUE type | \$VALUE range | description |
|:--|:--|:--|:--|
| auto_rotate | String | `on` or `off` | Turn on or off auto egg rotation |
| degrees| Int | `rotate_min_degrees` ~ `rotate_max_degrees` | Adjust degrees of egg rotater |
| temperature | Float | `25.0` ~ `40.0` | Change target temperature |
| start | Unsigned long | any (`0` is set as current unix time) | Change unix time when started incubating |
| kp | Float | `0.0` ~ `10.0` | Change PID coefficient for heater control (Proportional term) |
| ki | Float | `0.0` ~ `10.0` | Change PID coefficient for heater control (Integral term) |
| kd | Float | `0.0` ~ `10.0` | Change PID coefficient for heater control (Derivative term) |

### Other commands

```
# Rebooting incubator
$ ./incubator reboot
>>> "reboot"

# Ping Pong
$ ./incubator ping
>>> "ping"
[2020/07/07(Tue) 07:07:07] Pong

# Cheep, cheep..
$ ./incubator piyo
>>> "piyo"
[2020/07/07(Tue) 07:07:07] Piyo
```

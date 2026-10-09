# Smart Traffic Light + Air Quality Monitor

ESP32 project using FreeRTOS. Runs a 3-lane traffic light and an air quality monitor at the same time, each in its own task.

Built in Wokwi first, real circuit later.

## What it does

- 3 lane traffic light with green, yellow, then a short all red gap before the next lane goes green
- Reads temperature and humidity from a DHT sensor
- Reads air quality from an MQ135 and converts it to ppm using a library
- Shows everything on an I2C LCD
- Buzzer goes off when air quality is poor
- First 2 minutes after boot the LCD shows a warming up countdown because the MQ135 needs preheat

## Hardware

| Part | Notes |
|------|-------|
| ESP32 DevKit C | main board |
| 9 LEDs | red, yellow, green x 3 lanes |
| DHT11 | Wokwi only has DHT22 so the sim uses that |
| MQ135 | analog out to GPIO34 |
| I2C LCD | address 0x27 |
| Buzzer | active or passive, driven with tone |

Real circuit needs about 220 ohm resistor in series with every LED.

## Pin map

| Thing | GPIO |
|-------|------|
| Lane 1 R / Y / G | 23 / 19 / 18 |
| Lane 2 R / Y / G | 5 / 17 / 16 |
| Lane 3 R / Y / G | 4 / 25 / 2 |
| DHT data | 14 |
| MQ135 analog | 34 |
| Buzzer | 26 |
| LCD SDA / SCL | 21 / 22 |

## How the code is split

Three FreeRTOS tasks:

1. TaskTrafficLight runs the light cycle
2. TaskSensorRead reads DHT and MQ135 every 2 seconds
3. TaskDisplayBuzzer updates the LCD and buzzer every 1 second

Sensor values are shared between task 2 and task 3, so a mutex (xSensorMutex) protects them. The LCD is only used by one task so it does not need a lock.

## Revision notes

- Moved yellow LED of lane 3 from GPIO0 to GPIO25. GPIO0 is a boot pin and must stay HIGH at boot or the ESP32 can enter flash mode
- Moved buzzer from GPIO12 to GPIO26 because GPIO12 is also a boot related pin
- Moved the mutex from the LCD to the shared sensor data
- Switched MQ135 reading from raw analog to library ppm, plus a 2 minute preheat message
- Added all red phase after every yellow so cars still in the intersection can clear

## Libraries

- LiquidCrystal I2C
- DHT sensor library
- MQ135

## Run it

1. Open the project in Wokwi, or build with PlatformIO or Arduino IDE
2. Install the libraries above
3. Upload and open the serial monitor at 115200

To test preheat quickly, temporarily set PREHEAT_MS to 10000 in the code.

## Known limitations

- ppm values are rough until the MQ135 is calibrated in clean air on real hardware
- Wokwi has no real MQ135 so the sim uses a generic gas sensor
- DHTTYPE is DHT22 for the sim, change it to DHT11 on the real circuit
- MQ135 analog output can reach 5V on some modules and the ESP32 only tolerates 3.3V, so use a voltage divider on the real circuit

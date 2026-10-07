#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// Lampu traffic 1
#define R1_PIN 23
#define Y1_PIN 19
#define G1_PIN 18

// Lampu traffic 2
#define R2_PIN 5
#define Y2_PIN 17
#define G2_PIN 16

// Lampu traffic 3
#define R3_PIN 4
#define Y3_PIN 0
#define G3_PIN 2

// Sensors dan actuators
#define DHTPIN 14
#define DHTTYPE DHT11
#define MQ135_PIN 34
#define BUZZER_PIN 12

// Peripherals & synchronization
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
SemaphoreHandle_t xLCDMutex;

// Global shared air quality data
volatile float tempVal = 0.0;
volatile float humVal = 0.0;
volatile int airQualityPpm = 0;

// Task handlers
void TaskTrafficLight(void *pvParameters);
void TaskSensorRead(void *pvParameters);
void TaskDisplayBuzzer(void *pvParameters);

void setup() {
  Serial.begin(115200);

  // Initialize LED Pins
  int pins[] = {R1_PIN, Y1_PIN, G1_PIN, R2_PIN, Y2_PIN, G2_PIN, R3_PIN, Y3_PIN, G3_PIN, BUZZER_PIN};
  for (int pin : pins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  // Initialize sensors & peripherals
  dht.begin();
  lcd.init();
  lcd.backlight();

  // Create mutex for LCD resource protection
  xLCDMutex = xSemaphoreCreateMutex();

  // RTOS Tasks creation
  xTaskCreatePinnedToCore(TaskTrafficLight, "TrafficTask", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskSensorRead,  "SensorTask",  2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskDisplayBuzzer,"DisplayTask", 2048, NULL, 1, NULL, 1);
}

void loop() {
  // FreeRTOS handles execution; loop remains empty
  vTaskDelete(NULL);
}

// Helper function to set traffic light state
void setIntersection(int red, int yellow, int green, int r, int y, int g) {
  digitalWrite(red, r);
  digitalWrite(yellow, y);
  digitalWrite(green, g);
}

// Task 1: 3-way sequential traffic light control
void TaskTrafficLight(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // Phase 1: Lane 1 GREEN, Lanes 2 & 3 RED
    setIntersection(R1_PIN, Y1_PIN, G1_PIN, 0, 0, 1);
    setIntersection(R2_PIN, Y2_PIN, G2_PIN, 1, 0, 0);
    setIntersection(R3_PIN, Y3_PIN, G3_PIN, 1, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(4000));

    // Phase 1 Yellow
    setIntersection(R1_PIN, Y1_PIN, G1_PIN, 0, 1, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Phase 2: Lane 2 GREEN, Lanes 1 & 3 RED
    setIntersection(R1_PIN, Y1_PIN, G1_PIN, 1, 0, 0);
    setIntersection(R2_PIN, Y2_PIN, G2_PIN, 0, 0, 1);
    setIntersection(R3_PIN, Y3_PIN, G3_PIN, 1, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(4000));

    // Phase 2 Yellow
    setIntersection(R2_PIN, Y2_PIN, G2_PIN, 0, 1, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Phase 3: Lane 3 GREEN, Lanes 1 & 2 RED
    setIntersection(R1_PIN, Y1_PIN, G1_PIN, 1, 0, 0);
    setIntersection(R2_PIN, Y2_PIN, G2_PIN, 1, 0, 0);
    setIntersection(R3_PIN, Y3_PIN, G3_PIN, 0, 0, 1);
    vTaskDelay(pdMS_TO_TICKS(4000));

    // Phase 3 Yellow
    setIntersection(R3_PIN, Y3_PIN, G3_PIN, 0, 1, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Task 2: read DHT11 and MQ-135 air quality sensor
void TaskSensorRead(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int analogGas = analogRead(MQ135_PIN);

    if (!isnan(t)) tempVal = t;
    if (!isnan(h)) humVal = h;
    airQualityPpm = map(analogGas, 0, 4095, 100, 1000); // Scaled representation

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// Task 3: update LCD & trigger audio alarm
void TaskDisplayBuzzer(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    if (xSemaphoreTake(xLCDMutex, portMAX_DELAY) == pdTRUE) {
      // Row 0: temp and humidity
      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print((int)tempVal);
      lcd.print("C H:");
      lcd.print((int)humVal);
      lcd.print("%   ");

      // Row 1: air quality status
      lcd.setCursor(0, 1);
      if (airQualityPpm > 600) {
        lcd.print("AQI: POOR!     ");
        tone(BUZZER_PIN, 1000, 200); // Alert sound
      } else {
        lcd.print("AQI: GOOD      ");
        noTone(BUZZER_PIN);
      }
      xSemaphoreGive(xLCDMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <MQ135.h>

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
#define Y3_PIN 25
#define G3_PIN 2

// Sensors dan actuators
#define DHTPIN 14
#define DHTTYPE DHT22   // Wokwi cuma punya DHT22. Ganti ke DHT11 kalau pakai DHT asli ya mas
#define MQ135_PIN 34
#define BUZZER_PIN 26

// Konfigurasi waktu dan ambang batas
const unsigned long PREHEAT_MS = 120000;   // 2 menit pre heat MQ135
const int AQ_POOR_PPM = 600;               // di atas ini dianggap buruk
const int GREEN_MS = 4000;
const int YELLOW_MS = 1000;
const int ALL_RED_MS = 1000;

// Peripherals dan synchronization
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
MQ135 gasSensor(MQ135_PIN);
SemaphoreHandle_t xSensorMutex;

// Data sensor yang dipakai bersama oleh SensorTask dan DisplayTask
float tempVal = 0.0;
float humVal = 0.0;
int airQualityPpm = 0;

// Task handlers
void TaskTrafficLight(void *pvParameters);
void TaskSensorRead(void *pvParameters);
void TaskDisplayBuzzer(void *pvParameters);

void setup() {
  Serial.begin(115200);

  // Samakan resolusi ADC ESP32 dengan yang diharapkan library MQ135 (0 sampai 1023)
  analogReadResolution(10);

  // Initialize LED pins dan buzzer
  int pins[] = {R1_PIN, Y1_PIN, G1_PIN, R2_PIN, Y2_PIN, G2_PIN, R3_PIN, Y3_PIN, G3_PIN, BUZZER_PIN};
  for (int pin : pins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  dht.begin();
  lcd.init();
  lcd.backlight();

  // Mutex untuk melindungi data sensor
  xSensorMutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(TaskTrafficLight, "TrafficTask", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskSensorRead, "SensorTask", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskDisplayBuzzer, "DisplayTask", 4096, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);
}

// Set satu lane
void setIntersection(int red, int yellow, int green, int r, int y, int g) {
  digitalWrite(red, r);
  digitalWrite(yellow, y);
  digitalWrite(green, g);
}

// Semua lane merah (fase all red)
void allRed() {
  setIntersection(R1_PIN, Y1_PIN, G1_PIN, 1, 0, 0);
  setIntersection(R2_PIN, Y2_PIN, G2_PIN, 1, 0, 0);
  setIntersection(R3_PIN, Y3_PIN, G3_PIN, 1, 0, 0);
}

// Task 1: lampu lalu lintas 3 lane dengan fase all red
void TaskTrafficLight(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // Lane 1 hijau
    allRed();
    setIntersection(R1_PIN, Y1_PIN, G1_PIN, 0, 0, 1);
    vTaskDelay(pdMS_TO_TICKS(GREEN_MS));

    // Lane 1 kuning
    setIntersection(R1_PIN, Y1_PIN, G1_PIN, 0, 1, 0);
    vTaskDelay(pdMS_TO_TICKS(YELLOW_MS));

    // All red
    allRed();
    vTaskDelay(pdMS_TO_TICKS(ALL_RED_MS));

    // Lane 2 hijau
    setIntersection(R2_PIN, Y2_PIN, G2_PIN, 0, 0, 1);
    vTaskDelay(pdMS_TO_TICKS(GREEN_MS));

    // Lane 2 kuning
    setIntersection(R2_PIN, Y2_PIN, G2_PIN, 0, 1, 0);
    vTaskDelay(pdMS_TO_TICKS(YELLOW_MS));

    // All red
    allRed();
    vTaskDelay(pdMS_TO_TICKS(ALL_RED_MS));

    // Lane 3 hijau
    setIntersection(R3_PIN, Y3_PIN, G3_PIN, 0, 0, 1);
    vTaskDelay(pdMS_TO_TICKS(GREEN_MS));

    // Lane 3 kuning
    setIntersection(R3_PIN, Y3_PIN, G3_PIN, 0, 1, 0);
    vTaskDelay(pdMS_TO_TICKS(YELLOW_MS));

    // All red
    allRed();
    vTaskDelay(pdMS_TO_TICKS(ALL_RED_MS));
  }
}

// Task 2: baca DHT dan MQ135, lalu tulis ke data bersama lewat mutex
void TaskSensorRead(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    // Baca dulu ke variabel lokal, lama dan tanpa lock
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    float ppm = gasSensor.getPPM();

    // Lock sebentar saja untuk menyalin ke data bersama
    if (xSemaphoreTake(xSensorMutex, portMAX_DELAY) == pdTRUE) {
      if (!isnan(t)) tempVal = t;
      if (!isnan(h)) humVal = h;
      if (isfinite(ppm)) airQualityPpm = (int)ppm;
      xSemaphoreGive(xSensorMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// Task 3: salin data bersama lewat mutex, lalu tampilkan di LCD dan bunyikan buzzer
void TaskDisplayBuzzer(void *pvParameters) {
  (void) pvParameters;
  for (;;) {
    float t = 0;
    float h = 0;
    int ppm = 0;

    // Lock sebentar saja untuk menyalin data ke variabel lokal
    if (xSemaphoreTake(xSensorMutex, portMAX_DELAY) == pdTRUE) {
      t = tempVal;
      h = humVal;
      ppm = airQualityPpm;
      xSemaphoreGive(xSensorMutex);
    }

    // Baris 0: suhu dan kelembapan
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print((int)t);
    lcd.print("C H:");
    lcd.print((int)h);
    lcd.print("%   ");

    // Baris 1: status udara atau pengingat pre heat
    lcd.setCursor(0, 1);
    unsigned long now = millis();
    if (now < PREHEAT_MS) {
      int secondsLeft = (PREHEAT_MS - now) / 1000;
      lcd.print("MQ135 heat ");
      lcd.print(secondsLeft);
      lcd.print("s   ");
      noTone(BUZZER_PIN);
    } else if (ppm > AQ_POOR_PPM) {
      lcd.print("AQI: POOR!      ");
      tone(BUZZER_PIN, 1000, 200);
    } else {
      lcd.print("AQI: GOOD       ");
      noTone(BUZZER_PIN);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
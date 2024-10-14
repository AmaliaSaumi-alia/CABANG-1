#define BLYNK_TEMPLATE_ID "TMPL63elZ23Za"
#define BLYNK_TEMPLATE_NAME "Pengukuran dan analisis debit air"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

// Konfigurasi Blynk
char auth[] = "MsU3-o-oSF8-BB8m5i1KaSImE9zcFmBw";
char ssid[] = "Ndk Modal";
char pass[] = "12345678";

// Pin untuk Sensor Aliran Air
#define FLOW_SENSOR_PIN1 D5  // Sensor 1
#define FLOW_SENSOR_PIN2 D6  // Sensor 2

// Inisialisasi LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Alamat I2C LCD mungkin berbeda

volatile int flowPulseCount1 = 0;
volatile int flowPulseCount2 = 0;

// Koefisien kalibrasi sensor aliran
const float calibrationFactor = 7.5;  // Pulsa per liter

unsigned long previousMillis = 0; // Variabel untuk menghitung waktu
float totalDebitSensor2 = 0.0;    // Variabel untuk menyimpan total debit sensor 2
float totalBiaya = 0.0;           // Variabel untuk menyimpan total biaya

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);  // Waktu UTC+7 (25200 detik offset)

// ISR untuk menangkap pulsa dari sensor flow
void ICACHE_RAM_ATTR flowPulseISR1() { flowPulseCount1++; }
void ICACHE_RAM_ATTR flowPulseISR2() { flowPulseCount2++; }

void setup() {
  Serial.begin(9600);
  delay(100);

  Serial.println("Mulai setup...");
  Blynk.begin(auth, ssid, pass);

  lcd.init();
  lcd.backlight();
  Serial.println("LCD diinisialisasi...");

  pinMode(FLOW_SENSOR_PIN1, INPUT);
  pinMode(FLOW_SENSOR_PIN2, INPUT);

  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN1), flowPulseISR1, RISING);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN2), flowPulseISR2, RISING);

  Serial.println("Interrupts diinisialisasi...");

  timeClient.begin();  // Mulai client NTP
}

// Fungsi untuk menghitung biaya berdasarkan debit air
float hitungBiaya(float totalDebitSensor2) {
  float biaya = 0;

  if (totalDebitSensor2 <= 10) {
    biaya = totalDebitSensor2 * 21;  // per m³
  } else if (totalDebitSensor2 <= 20) {
    biaya = (10 * 21) + ((totalDebitSensor2 - 10) * 31);
  } else if (totalDebitSensor2 <= 30) {
    biaya = (10 * 21) + (10 * 31) + ((totalDebitSensor2 - 20) * 45);
  } else {
    biaya = (10 * 21) + (10 * 31) + (10 * 45) + ((totalDebitSensor2 - 30) * 63);
  }
  return biaya;
}

// Fungsi untuk menghitung dan menampilkan status kebocoran
void tampilkanStatusKebocoran(float flowRate1, float flowRate2) {
  // Toleransi 5% untuk perbedaan debit
  float toleransi = 0.05 * flowRate1;

  // Hitung selisih debit
  float selisihDebit = abs(flowRate1 - flowRate2);
  
  String statusKebocoran = "Aman";
  String tingkatKebocoran = "Tidak ada";

  // Tentukan status kebocoran berdasarkan selisih
  if (selisihDebit > toleransi) {
    if (selisihDebit >= 0.01) {
      tingkatKebocoran = "Besar";
    } else if (selisihDebit >= 0.006) {
      tingkatKebocoran = "Sedang";
    } else if (selisihDebit >= 0.005) {
      tingkatKebocoran = "Kecil";
    }
    statusKebocoran = "Bocor cabang1";
  }

  // Tampilkan status kebocoran di Serial Monitor
  Serial.print("Status Kebocoran: ");
  Serial.println(statusKebocoran);
  Serial.print("Tingkat Kebocoran: ");
  Serial.println(tingkatKebocoran);

  // Kirim status kebocoran ke Blynk
  Blynk.virtualWrite(V3, statusKebocoran);
  Blynk.virtualWrite(V4, tingkatKebocoran);

  // Tampilkan status kebocoran di LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sts: ");
  lcd.print(statusKebocoran);
  lcd.setCursor(0, 1);
  lcd.print("T.bocor: ");
  lcd.print(tingkatKebocoran);
  delay(3000);
}

void loop() {
  Blynk.run();

  unsigned long currentMillis = millis();
  unsigned long interval = 1000;  // 1 detik

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Hentikan interrupt saat membaca pulsa
    noInterrupts();
    int pulseCount1 = flowPulseCount1;
    int pulseCount2 = flowPulseCount2;
    flowPulseCount1 = 0;
    flowPulseCount2 = 0;
    interrupts();

    // Hitung debit aliran air dalam m³/menit
    float flowRate1_m3min = (pulseCount1 / calibrationFactor) / 1000;  // Konversi ke m³/menit
    float flowRate2_m3min = (pulseCount2 / calibrationFactor) / 1000;  // Konversi ke m³/menit

    // Konversi debit ke liter
    float volume1_liters = pulseCount1 / calibrationFactor;
    float volume2_liters = pulseCount2 / calibrationFactor;

    // Jika ada aliran air, tambahkan ke total debit
    if (flowRate2_m3min > 0) {
      totalDebitSensor2 += flowRate2_m3min * (interval / 60000.0);  // Tambah debit ke total
    }

    // Hitung biaya berdasarkan total debit air yang terpakai
    totalBiaya = hitungBiaya(totalDebitSensor2);

    // Kirim data ke Blynk
    Blynk.virtualWrite(V0, flowRate1_m3min); // Sensor 1
    Blynk.virtualWrite(V1, flowRate2_m3min); // Sensor 2
    Blynk.virtualWrite(V2, totalBiaya);      // Biaya total

    // Tampilkan volume dalam liter dan debit dalam m³/menit di Serial Monitor
    Serial.print("Sensor 1: Debit = ");
    Serial.print(flowRate1_m3min, 3);
    Serial.println(" m³/min");
    Serial.print("Volume = ");
    Serial.print(volume1_liters);
    Serial.println(" liter");

    Serial.print("Sensor 2: Debit = ");
    Serial.print(flowRate2_m3min, 3);
    Serial.println(" m³/min");
    Serial.print("Volume = ");
    Serial.print(volume2_liters);
    Serial.println(" liter");

    Serial.print("Biaya Total: ");
    Serial.print(totalBiaya);
    Serial.println(" Rupiah");

    // Tampilkan di LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Biaya: ");
    lcd.print(totalBiaya);
    lcd.print(" Rp");
    delay(3000);

    // Tampilkan data sensor di LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DB1: ");
    lcd.print(flowRate1_m3min, 3); // Tampilkan dalam m³
    lcd.setCursor(0, 1);
    lcd.print("DB2: ");
    lcd.print(flowRate2_m3min, 3);
    delay(3000);

    // Tampilkan status kebocoran
    tampilkanStatusKebocoran(flowRate1_m3min, flowRate2_m3min);

    // Tampilkan waktu di Serial Monitor
    timeClient.update();  // Update waktu dari NTP
    Serial.print("Waktu sekarang: ");
    Serial.print(timeClient.getHours()); Serial.print(":");
    Serial.print(timeClient.getMinutes()); Serial.print(":");
    Serial.println(timeClient.getSeconds());
  }
}

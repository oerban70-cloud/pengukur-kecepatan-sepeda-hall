/*
  Pengukur Kecepatan Sepeda dengan Sensor Hall — ESP32 + LCD I2C
  ================================================================
  Cara kerja:
  Satu magnet kecil ditempel di jari-jari roda, sensor Hall A3144
  (modul KY-003) dipasang di garpu depan. Setiap kali magnet lewat
  di depan sensor, sensor mengirim satu pulsa (LOW) ke ESP32.
  ESP32 menghitung pulsa tiap detik, lalu mengubahnya menjadi
  kecepatan (km/jam) memakai keliling roda, dan menampilkannya
  di LCD 16x2 I2C bersama jarak tempuh dan kecepatan tertinggi.

  Rumus:
    keliling roda (m) = phi x diameter roda (m)
    kecepatan (km/jam) = pulsa per detik x keliling roda x 3,6

  Kalibrasi: ukur keliling rodamu sekali saja (diameter x 3,14),
  lalu ubah DIAMETER_RODA_MM di bawah. Roda 26 inci = 660 mm.

  Video penjelasan (60 detik): link menyusul setelah upload
  Repo: https://github.com/oerban70-cloud/pengukur-kecepatan-sepeda-hall
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- Konfigurasi pin ----------
const uint8_t PIN_SENSOR_HALL = 27;   // OUT sensor Hall -> GPIO27 (interrupt)
const uint8_t PIN_LED_PULSA    = 2;   // LED bawaan board, berkedip tiap pulsa
const uint8_t PIN_SDA_LCD      = 21;  // SDA bus I2C
const uint8_t PIN_SCL_LCD      = 22;  // SCL bus I2C
const uint8_t ALAMAT_LCD       = 0x27;
const uint8_t KOLOM_LCD        = 16;
const uint8_t BARIS_LCD        = 2;

// ---------- Konfigurasi roda ----------
// Diameter roda 26 inci = 660 mm. Satu magnet per putaran roda.
const float DIAMETER_RODA_MM   = 660.0;
const uint8_t MAGNET_PER_PUTARAN = 1;
const float MM_KE_M            = 1000.0;   // 1 m = 1000 mm
const float DETIK_KE_JAM       = 3600.0;   // 1 jam = 3600 detik
const float M_KE_KM            = 1000.0;   // 1 km = 1000 m
const float PI_NILAI           = 3.14159265;

// ---------- Konfigurasi waktu ----------
const unsigned long PERIODE_HITUNG_MS   = 1000;  // hitung kecepatan tiap 1 detik
const unsigned long JEDA_MIN_PULSA_MS   = 20;    // abaikan pulsa < 20 ms (pantulan/noise)
const unsigned long DURASI_KEDIP_LED_MS = 80;    // LED menyala 80 ms tiap pulsa
const unsigned long BAUD_SERIAL         = 115200;

// ---------- State (dibagi dengan ISR) ----------
volatile unsigned long jumlahPulsa  = 0;   // pulsa sejak awal (total)
volatile unsigned long waktuPulsaTerakhirMs = 0;
volatile bool pulsaBaru = false;

// ---------- State utama ----------
float kelilingRodaM      = 0.0;   // keliling roda dalam meter
float kecepatanKmh       = 0.0;   // kecepatan saat ini
float kecepatanMaksKmh   = 0.0;   // kecepatan tertinggi sejak dinyalakan
float jarakTempuhKm      = 0.0;   // jarak total sejak dinyalakan
unsigned long pulsaTerakhirDihitung = 0;
unsigned long waktuHitungTerakhirMs = 0;
unsigned long waktuLedMenyalaMs     = 0;

LiquidCrystal_I2C lcd(ALAMAT_LCD, KOLOM_LCD, BARIS_LCD);

// Interrupt: dipanggil tiap magnet lewat sensor (sinyal turun ke LOW).
// Hanya menghitung, tanpa delay dan tanpa akses LCD/Serial di sini.
void IRAM_ATTR saatMagnetLewat() {
  unsigned long sekarang = millis();
  if (sekarang - waktuPulsaTerakhirMs >= JEDA_MIN_PULSA_MS) {
    waktuPulsaTerakhirMs = sekarang;
    jumlahPulsa++;
    pulsaBaru = true;
  }
}

// Hitung kecepatan dari pulsa yang masuk selama satu periode.
// Dipanggil tiap PERIODE_HITUNG_MS dari loop().
void hitungKecepatan() {
  unsigned long sekarang = millis();
  unsigned long pulsaPeriode = jumlahPulsa - pulsaTerakhirDihitung;
  pulsaTerakhirDihitung = jumlahPulsa;

  // pulsa/detik x keliling (m) = m/detik; x 3,6 = km/jam.
  kecepatanKmh = (float)pulsaPeriode / MAGNET_PER_PUTARAN
               * kelilingRodaM * DETIK_KE_JAM / M_KE_KM
               * (1000.0 / (float)PERIODE_HITUNG_MS);
  if (kecepatanKmh > kecepatanMaksKmh) {
    kecepatanMaksKmh = kecepatanKmh;
  }
  jarakTempuhKm += (float)pulsaPeriode / MAGNET_PER_PUTARAN
                 * kelilingRodaM / M_KE_KM;
  waktuHitungTerakhirMs = sekarang;
}

// Tampilkan kecepatan, jarak, dan kecepatan tertinggi di LCD.
void tampilkanLcd() {
  char barisAtas[KOLOM_LCD + 1];
  char barisBawah[KOLOM_LCD + 1];
  snprintf(barisAtas, sizeof(barisAtas), "Kec:%5.1f km/jam", kecepatanKmh);
  snprintf(barisBawah, sizeof(barisBawah), "Jrk%5.2f Mx%4.1f",
           jarakTempuhKm, kecepatanMaksKmh);
  lcd.setCursor(0, 0);
  lcd.print(barisAtas);
  lcd.setCursor(0, 1);
  lcd.print(barisBawah);
}

// Kirim data ke Serial Monitor (untuk kalibrasi & debug).
void tampilkanSerial() {
  Serial.print("Kecepatan: ");
  Serial.print(kecepatanKmh, 1);
  Serial.print(" km/jam | Maks: ");
  Serial.print(kecepatanMaksKmh, 1);
  Serial.print(" km/jam | Jarak: ");
  Serial.print(jarakTempuhKm, 2);
  Serial.print(" km | Pulsa total: ");
  Serial.println(jumlahPulsa);
}

void setup() {
  Serial.begin(BAUD_SERIAL);

  kelilingRodaM = PI_NILAI * DIAMETER_RODA_MM / MM_KE_M;
  Serial.print("Keliling roda: ");
  Serial.print(kelilingRodaM, 3);
  Serial.println(" m");

  pinMode(PIN_SENSOR_HALL, INPUT_PULLUP);  // A3144 open-collector, butuh pull-up
  pinMode(PIN_LED_PULSA, OUTPUT);
  digitalWrite(PIN_LED_PULSA, LOW);

  Wire.begin(PIN_SDA_LCD, PIN_SCL_LCD);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Speedometer");
  lcd.setCursor(0, 1);
  lcd.print("Sepeda ESP32");
  delay(1500);
  lcd.clear();

  // Interrupt di sisi turun: sensor Hall aktif LOW saat magnet dekat.
  attachInterrupt(digitalPinToInterrupt(PIN_SENSOR_HALL),
                  saatMagnetLewat, FALLING);
  waktuHitungTerakhirMs = millis();
}

void loop() {
  unsigned long sekarang = millis();

  // LED berkedip singkat tiap ada pulsa baru.
  if (pulsaBaru) {
    pulsaBaru = false;
    digitalWrite(PIN_LED_PULSA, HIGH);
    waktuLedMenyalaMs = sekarang;
  }
  if (digitalRead(PIN_LED_PULSA) == HIGH
      && sekarang - waktuLedMenyalaMs >= DURASI_KEDIP_LED_MS) {
    digitalWrite(PIN_LED_PULSA, LOW);
  }

  // Hitung kecepatan tiap satu detik.
  if (sekarang - waktuHitungTerakhirMs >= PERIODE_HITUNG_MS) {
    hitungKecepatan();
    tampilkanLcd();
    tampilkanSerial();
  }
}

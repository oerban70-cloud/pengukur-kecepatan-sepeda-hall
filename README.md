# Pengukur Kecepatan Sepeda — ESP32 + Sensor Hall + LCD I2C

Sepedamu melaju berapa kencang, tapi tak ada penunjuk kecepatannya? Alat kecil
ini yang mengukurnya: satu magnet neodymium di jari-jari roda, satu sensor Hall
A3144 di garpu depan. Setiap magnet lewat sensor, satu pulsa masuk ke ESP32 —
ESP32 menghitung pulsa tiap detik, mengubahnya jadi kilometer per jam, dan
menampilkannya di LCD bersama jarak tempuh dan kecepatan tertinggi.

Video penjelasan (60 detik):
https://www.youtube.com/shorts/ZE2176uQMTg

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| Sensor Hall A3144 (modul KY-003) | Mendeteksi magnet, output digital aktif LOW |
| Magnet neodymium kecil | Ditempel di jari-jari roda (1 magnet = 1 pulsa per putaran) |
| LCD 16x2 I2C (0x27) | Tampil kecepatan, jarak, kecepatan tertinggi |
| Adaptor USB 5V 2A | Catu daya |

## Wiring

| Dari | Ke |
|---|---|
| Hall OUT | ESP32 GPIO27 (interrupt, FALLING) |
| Hall VCC | 5V |
| LCD SDA | ESP32 GPIO21 |
| LCD SCL | ESP32 GPIO22 |
| LCD VCC | 5V |
| Semua GND | Digabung |

Skema lengkap: [schematic.png](schematic.png)

## Cara kerja

Magnet yang lewat di depan sensor Hall membuat output sensor turun ke LOW —
satu pulsa per putaran roda. ESP32 menangkap pulsa itu lewat interrupt
(`FALLING`, dengan jeda minimum 20 ms untuk mengabaikan pantulan dan noise),
lalu tiap 1 detik menghitung:

```
keliling roda (m) = phi x diameter roda (m)
kecepatan (km/jam) = pulsa per detik x keliling roda x 3,6
```

Jarak tempuh dijumlahkan dari semua pulsa sejak alat dinyalakan, dan kecepatan
tertinggi disimpan selama alat menyala. LED bawaan board (GPIO2) berkedip
singkat setiap ada pulsa baru — berguna untuk memastikan sensor membaca magnet.

## Kalibrasi

Ukur keliling rodamu sekali saja, lalu ubah `DIAMETER_RODA_MM` di sketch.
Roda 26 inci = 660 mm, kelilingnya 2,07 m. Kalau rodamu 27,5 inci (700 mm)
atau 29 inci (740 mm), ganti angkanya — rumusnya sama.

Contoh angka: roda 26 inci berputar 2 kali per detik berarti
2 x 2,07 m = 4,14 m/detik, alias 14,9 km/jam.

## Konfigurasi di sketch

| Konstanta | Nilai bawaan | Arti |
|---|---|---|
| `PIN_SENSOR_HALL` | 27 | Pin interrupt sensor Hall |
| `DIAMETER_RODA_MM` | 660.0 | Diameter roda 26 inci |
| `MAGNET_PER_PUTARAN` | 1 | Satu magnet per putaran |
| `PERIODE_HITUNG_MS` | 1000 | Kecepatan dihitung tiap 1 detik |
| `JEDA_MIN_PULSA_MS` | 20 | Abaikan pulsa lebih cepat dari 20 ms |
| `ALAMAT_LCD` | 0x27 | Alamat I2C LCD |

## Compile (terverifikasi)

```
arduino-cli 1.6.0, ESP32 core 4.0.0, board esp32:esp32:esp32
Sketch uses 319205 bytes (24%) of program storage space. Maximum is 1310720 bytes.
Global variables use 25136 bytes (7%) of dynamic memory. Maximum is 327680 bytes.
```

Library yang dibutuhkan: `LiquidCrystal_I2C` 1.1.2 (ada di folder `libraries/`).

## Lisensi

MIT — lihat [LICENSE](LICENSE).

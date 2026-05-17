# Arduino Inventory Management System

Proyek ini adalah sistem manajemen inventaris *(inventory management)* berskala kecil yang berjalan secara murni menggunakan arsitektur bahasa **C** pada **Arduino UNO (ATmega328P)**. 
Sistem ini menggunakan fitur penyimpanan persisten pada **EEPROM** untuk mencegah data hilang saat Arduino dimatikan. Selain itu, proyek ini menerapkan konsep komunikasi **Master-Slave (I2C)** di mana Arduino Slave bertindak sebagai sistem Redundancy (memori cadangan / *mirror*) dari EEPROM Master.

---


## Prasyarat Sistem & Tools (Requirements)

Sebelum dapat mengompilasi dan menjalankan program ini, pastikan Anda telah menginstal beberapa *tools* penting di komputer Linux Anda. Anda bisa menginstalnya melalui terminal menggunakan *package manager* bawaan (seperti `apt` untuk Ubuntu/Debian):

```bash
sudo apt update
sudo apt install gcc make avr-libc binutils-avr gcc-avr avrdude
```

**Penjelasan Singkat Tools:**
1.  **`gcc` & `make`**: Compiler standar C untuk Linux. Digunakan untuk mengompilasi tool jembatan `host_csv.c`.
2.  **`avr-libc`, `binutils-avr`, `gcc-avr` (AVR-GCC)**: *Cross-compiler* khusus untuk mengubah kode C menjadi bahasa mesin (`.hex`) yang dimengerti oleh mikrokontroler AVR (ATmega328P).
3.  **`avrdude`**: Program *flasher* yang bertugas menembakkan file `.hex` hasil kompilasi dari komputer ke dalam memori Arduino melalui kabel USB.

*(Catatan: Anda juga memerlukan **2 buah Arduino UNO** beserta kabel *jumper* untuk menyambungkan pin SDA/SCL (A4/A5) dan GND jika ingin menguji fitur Master-Slave I2C secara penuh).*

---

## Cara Menjalankan Sistem

**1. Meng-Upload Kode ke Master:**
```bash
make master
make upload PORT=/dev/ttyUSB0
```

**2. Meng-Upload Kode ke Slave:**
```bash
make slave
make upload PORT=/dev/ttyUSB0
```

**3. Tool Impor/Ekspor CSV via C (Host C):**
```bash
# Lakukan kompilasi tool host
make host_csv

# Untuk Import dari CSV ke Arduino (Pastikan Serial Monitor DITUTUP!)
./host_csv import /dev/ttyUSB0 data/import.csv

# Untuk Export dari Arduino ke CSV
./host_csv export /dev/ttyUSB0 data/export_hasil.csv
```

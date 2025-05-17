#include <SoftwareSerial.h>
#include <AltSoftSerial.h>
#include <TinyGPS++.h>
#include <Wire.h>

// Nomor Telepon yang Diizinkan
const String AUTHORIZED_PHONE = "+6282199799982";

// Pin Konfigurasi
#define relayPin 5           // Relay utama (ON/OFF motor)
#define starterRelayPin 4    // Relay starter (seperti tombol starter)
#define vibrationPin 6       // Sensor getar
#define alarmRelayPin 12     // Relay alarm / buzzer / motor blokir

// Komunikasi Serial
#define rxPin 10
#define txPin 11
SoftwareSerial sim800(rxPin, txPin);
#define gpsRXPin 9
#define gpsTXPin 8
AltSoftSerial gpsSerial(gpsRXPin, gpsTXPin);
TinyGPSPlus gps;

// Variabel Status
String sms_status, sender_number, received_date, msg;
boolean ignition_status = false;
boolean reply_status = true;
boolean anti_theft = false;

void setup() {
  Serial.begin(115200);
  sim800.begin(9600);
  gpsSerial.begin(9600);

  pinMode(relayPin, OUTPUT);
  pinMode(starterRelayPin, OUTPUT);
  pinMode(alarmRelayPin, OUTPUT);
  pinMode(vibrationPin, INPUT);

  digitalWrite(relayPin, LOW);
  digitalWrite(starterRelayPin, LOW);
  digitalWrite(alarmRelayPin, LOW);

  initializeSIM800L();
}

void loop() {
  ignition_status = getIgnitionStatus();

  // Fitur Anti-pencurian dengan sensor getar
  if (anti_theft && digitalRead(vibrationPin) == HIGH) {
    digitalWrite(alarmRelayPin, HIGH);
    delay(3000);  // Alarm aktif selama 3 detik
    digitalWrite(alarmRelayPin, LOW);
  }

  // Baca data dari SIM800L
  while (sim800.available()) {
    parseData(sim800.readString());
  }

  // Debug via Serial Monitor
  while (Serial.available()) {
    sim800.println(Serial.readString());
  }
}

// Inisialisasi Modul SIM800L
void initializeSIM800L() {
  delay(7000);
  sendCommand("AT");
  sendCommand("ATE1");
  sendCommand("AT+CPIN?");
  sendCommand("AT+CMGF=1");
  sendCommand("AT+CNMI=1,2,0,0,0");
}

// Fungsi Kirim Perintah AT
void sendCommand(String command) {
  sim800.println(command);
  delay(500);
  while (sim800.available()) {
    Serial.write(sim800.read());
  }
}

// Parsing Data dari SIM800L
void parseData(String buff) {
  Serial.println("Raw SIM800L Data: " + buff);

  if (buff.indexOf("+CMT:") != -1) {
    Serial.println("SMS Baru Diterima!");
    extractSms(buff);
    doAction();
  }
}

// Ekstraksi SMS
void extractSms(String buff) {
  int senderStart = buff.indexOf("+CMT: \"") + 7;
  int senderEnd = buff.indexOf("\",\"", senderStart);
  sender_number = buff.substring(senderStart, senderEnd);

  int msgStart = buff.indexOf("\n", senderEnd) + 1;
  msg = buff.substring(msgStart);
  msg.trim();
  msg.toLowerCase();

  Serial.println("SMS Diterima:");
  Serial.println("Pengirim: " + sender_number);
  Serial.println("Pesan: " + msg);
}

// Aksi Berdasarkan SMS
void doAction() {
  if (sender_number != AUTHORIZED_PHONE) {
    sendSms("Nomor Anda tidak memiliki izin untuk memberikan perintah.");
    Serial.println("Nomor tidak diizinkan: " + sender_number);
    return;
  }

  if (msg == "bike on") {
    digitalWrite(relayPin, HIGH);
    sendSms("Motor dinyalakan");
  } else if (msg == "bike off") {
    digitalWrite(relayPin, LOW);
    sendSms("Motor dimatikan");
  } else if (msg == "bike start") {
    if (ignition_status) {
      startBike();  // Hanya jika motor sudah ON
    } else {
      sendSms("Tidak bisa melakukan starter. Hidupkan motor terlebih dahulu (bike on).");
    }

  } else if (msg == "get location") {
    sendSmsGPS();
  } else if (msg == "anti theft on") {
    anti_theft = true;
    sendSms("Anti-pencurian diaktifkan");
  } else if (msg == "anti theft off") {
    anti_theft = false;
    sendSms("Anti-pencurian dinonaktifkan");
  } else {
    sendSms("Perintah tidak dikenali.");
  }
}

// Fungsi Starter Motor Sementara
void startBike() {
  sendSms("Memulai starter...");
  Serial.println("Starter aktif");

  digitalWrite(starterRelayPin, HIGH);
  delay(3000);  // Aktif selama 3 detik
  digitalWrite(starterRelayPin, LOW);

  Serial.println("Starter selesai");
  sendSms("Starter selesai");
}

// Kirim SMS
void sendSms(String text) {
  sendCommand("AT+CMGF=1");
  sim800.print("AT+CMGS=\"" + sender_number + "\"\r");
  delay(500);
  sim800.print(text);
  sim800.write(0x1A);
  delay(2000);
  Serial.println("SMS berhasil dikirim.");
}

// Kirim Lokasi GPS
void sendSmsGPS() {
  boolean newData = false;

  for (unsigned long start = millis(); millis() - start < 10000;) {
    while (gpsSerial.available()) {
      if (gps.encode(gpsSerial.read())) {
        newData = true;
      }
    }
  }

  if (newData && gps.location.isUpdated()) {
    String gpsData = "http://maps.google.com/maps?q=loc:" +
                     String(gps.location.lat(), 6) + "," +
                     String(gps.location.lng(), 6);

    sendSms(gpsData);
    Serial.println("SMS berhasil dikirim: " + gpsData);
  } else {
    sendSms("GPS tidak tersedia.");
    Serial.println("GPS tidak tersedia.");
  }
}

// Status Ignition
boolean getIgnitionStatus() {
  return digitalRead(relayPin) == HIGH;
}

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <DHT.h>

// ── LCD ──
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ── RTC ──
ThreeWire myWire(A2, A1, A3);
RtcDS1302<ThreeWire> rtc(myWire);

// ── RFID ──
#define SS_PIN  10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN);

// ── Servo ──
Servo porte;

// ── DHT11 ──
#define DHT_PIN  3
#define DHTTYPE  DHT11
DHT dht(DHT_PIN, DHTTYPE);

// ── Broches ──
const int TRIG_PIN   = 4;
const int ECHO_PIN   = 5;
const int LED_ROUGE  = 6;
const int LED_VERTE  = 7;
const int RELAIS_PIN = 8;
const int SERVO_PIN  = 3;
const int FLAMME_PIN = 2;
const int EAU_PIN    = A0;

// ── Seuils ──
const int SEUIL_DISTANCE = 80;
const int SEUIL_EAU      = 300;

// ── Anti-rebond flamme ──
int flammeCompteur = 0;

// ── UID autorisés ──
byte uidCarte[]    = {0xFB, 0xE5, 0x0D, 0x07};
byte uidPorteCle[] = {0xCB, 0xBD, 0x2A, 0x07};

long mesurerDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duree = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duree == 0) return -1;
  return duree / 58;
}

bool lireFlammeConfirmee() {
  int lecture = digitalRead(FLAMME_PIN);
  if (lecture == HIGH) flammeCompteur++;
  else flammeCompteur = 0;
  return (flammeCompteur >= 2);
}

// ── Envoi vers ESP32 via Pin 1 (TX) ──
void envoyerESP32(float temp, float hum, long distance, String etat) {
  Serial.print("TEMP:");
  Serial.print((int)temp);
  Serial.print("|HUM:");
  Serial.print((int)hum);
  Serial.print("|DIST:");
  Serial.print(distance);
  Serial.print("|ETAT:");
  Serial.println(etat);
}

void afficherHeure() {
  RtcDateTime now = rtc.GetDateTime();
  lcd.setCursor(8, 1);
  if (now.Hour() < 10)   lcd.print("0");
  lcd.print(now.Hour());
  lcd.print(":");
  if (now.Minute() < 10) lcd.print("0");
  lcd.print(now.Minute());
  lcd.print(":");
  if (now.Second() < 10) lcd.print("0");
  lcd.print(now.Second());
}

bool verifierAcces(byte *uid, byte taille) {
  bool carte    = true;
  bool porteCle = true;
  for (byte i = 0; i < taille; i++) {
    if (uid[i] != uidCarte[i])    carte    = false;
    if (uid[i] != uidPorteCle[i]) porteCle = false;
  }
  return carte || porteCle;
}

void accesAutorise() {
  digitalWrite(LED_VERTE, HIGH);
  digitalWrite(LED_ROUGE, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Acces autorise");
  lcd.setCursor(0, 1);
  lcd.print("Bienvenue !");
  envoyerESP32(0, 0, 0, "ACCES_OK");
  delay(2000);
  digitalWrite(LED_VERTE, LOW);
}

void accesRefuse() {
  digitalWrite(LED_ROUGE, HIGH);
  digitalWrite(LED_VERTE, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Acces refuse !");
  lcd.setCursor(0, 1);
  lcd.print("Badge inconnu");
  envoyerESP32(0, 0, 0, "ACCES_REFUSE");
  delay(1000);
  digitalWrite(LED_ROUGE, LOW);
}

void alerteIncendie(float temp, float hum) {
  digitalWrite(LED_ROUGE, HIGH);
  digitalWrite(LED_VERTE, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("!! INCENDIE !!");
  lcd.setCursor(0, 1);
  lcd.print("ALERTE FLAMME ");
  afficherHeure();
  envoyerESP32(temp, hum, 0, "INCENDIE");
  delay(500);
  digitalWrite(LED_ROUGE, LOW);
  delay(500);
}

void alerteInondation(int niveau, float temp, float hum) {
  digitalWrite(LED_ROUGE, HIGH);
  digitalWrite(LED_VERTE, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("!! INONDATION !!");
  lcd.setCursor(0, 1);
  lcd.print("Niv:");
  lcd.print(niveau);
  lcd.print(" ");
  afficherHeure();
  envoyerESP32(temp, hum, 0, "INONDATION");
  delay(500);
  digitalWrite(LED_ROUGE, LOW);
  delay(500);
}

void alerteIntrusion(long distance, float temp, float hum) {
  digitalWrite(LED_ROUGE, HIGH);
  digitalWrite(LED_VERTE, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("!! INTRUSION !!");
  lcd.setCursor(0, 1);
  lcd.print("D:");
  lcd.print(distance);
  lcd.print("cm ");
  afficherHeure();
  envoyerESP32(temp, hum, distance, "INTRUSION");
  delay(500);
  digitalWrite(LED_ROUGE, LOW);
  delay(500);
}

void etatNormal(long distance, int eau, float temp, float hum) {
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_VERTE, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print((int)temp);
  lcd.print("C H:");
  lcd.print((int)hum);
  lcd.print("%");
  lcd.setCursor(0, 1);
  afficherHeure();
  envoyerESP32(temp, hum, distance, "OK");
}

void setup() {
  Serial.begin(9600); // ⚠️ même vitesse que l'ESP32 (Serial2 à 9600)

  Wire.begin();
  lcd.init();
  lcd.backlight();

  rtc.Begin();
  rtc.SetIsWriteProtected(false);
  RtcDateTime maintenant = RtcDateTime(2026, 6, 13, 15, 30, 0); // ⚠️ adapte à l'heure réelle
  rtc.SetDateTime(maintenant);

  SPI.begin();
  rfid.PCD_Init();

  dht.begin();

  pinMode(TRIG_PIN,   OUTPUT);
  pinMode(ECHO_PIN,   INPUT);
  pinMode(LED_ROUGE,  OUTPUT);
  pinMode(LED_VERTE,  OUTPUT);
  pinMode(FLAMME_PIN, INPUT);
  pinMode(RELAIS_PIN, OUTPUT);
  digitalWrite(RELAIS_PIN, LOW);

  lcd.setCursor(0, 0);
  lcd.print("Smart Home");
  lcd.setCursor(0, 1);
  lcd.print("Demarrage...");
  delay(2000);
}

void loop() {

  rfid.PCD_Init();

  long  distance = mesurerDistance();
  bool  flamme   = lireFlammeConfirmee();
  int   eau      = analogRead(EAU_PIN);
  float temp     = dht.readTemperature();
  float hum      = dht.readHumidity();
  if (isnan(temp)) temp = 0;
  if (isnan(hum))  hum  = 0;

  // ── Relais éclairage selon l'heure ──
  RtcDateTime now = rtc.GetDateTime();
  if (now.Hour() >= 18 || now.Hour() < 6) {
    digitalWrite(RELAIS_PIN, LOW);
  } else {
    digitalWrite(RELAIS_PIN, HIGH);
  }

  // ── RFID ──
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (verifierAcces(rfid.uid.uidByte, rfid.uid.size)) accesAutorise();
    else accesRefuse();
    rfid.PICC_HaltA();
    flammeCompteur = 0;
    return;
  }

  // ── Priorités ──
  if (flamme) {
    alerteIncendie(temp, hum);
    flammeCompteur = 0;
  }
  else if (eau > SEUIL_EAU) {
    alerteInondation(eau, temp, hum);
  }
  else if (distance > 0 && distance < SEUIL_DISTANCE) {
    alerteIntrusion(distance, temp, hum);
  }
  else {
    etatNormal(distance, eau, temp, hum);
  }

  delay(500);
}
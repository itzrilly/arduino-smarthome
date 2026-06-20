#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

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

// ── Broches ──
const int TRIG_PIN   = 4;
const int ECHO_PIN   = 5;
const int BUZZER     = 8;
const int LED_VERTE  = 7;
const int LED_ROUGE  = 6;
const int SERVO_PIN  = 3;
const int FLAMME_PIN = 2;

// ── Seuils ──
const int SEUIL = 80;

// ── Anti-rebond flamme ──
int flammeCompteur = 0;

// ── UID autorisés ──
byte uidCarte[]    = {0xFB, 0xE5, 0x0D, 0x07};
byte uidPorteCle[] = {0xCB, 0xBD, 0x2A, 0x07};

// ── Fonctions ──

long mesurerDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duree = pulseIn(ECHO_PIN, HIGH);
  return duree / 58;
}

bool lireFlammeConfirmee() {
  int lecture = digitalRead(FLAMME_PIN);
  if (lecture == HIGH) {
    flammeCompteur++;
  } else {
    flammeCompteur = 0;
  }
  return (flammeCompteur >= 2);
}

void afficherHeure() {
  RtcDateTime now = rtc.GetDateTime();
  lcd.setCursor(7, 1);
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
  digitalWrite(BUZZER, LOW);
  porte.write(90);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Acces autorise");
  lcd.setCursor(0, 1);
  lcd.print("Bienvenue !");
  Serial.println("ACCES:AUTORISE");
  delay(3000);
  porte.write(0);
  digitalWrite(LED_VERTE, LOW);
}

void accesRefuse() {
  digitalWrite(LED_ROUGE, HIGH);
  digitalWrite(LED_VERTE, LOW);
  digitalWrite(BUZZER, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Acces refuse !");
  lcd.setCursor(0, 1);
  lcd.print("Badge inconnu");
  Serial.println("ACCES:REFUSE");
  delay(1000);
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(BUZZER, LOW);
}

void alerteIncendie() {
  digitalWrite(BUZZER, HIGH);
  digitalWrite(LED_ROUGE, HIGH);
  digitalWrite(LED_VERTE, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("!! INCENDIE !!");
  lcd.setCursor(0, 1);
  lcd.print("ALERTE FLAMME ");
  afficherHeure();
  Serial.println("ALERTE:INCENDIE");
  delay(500);
  digitalWrite(BUZZER, LOW);
  delay(500);
}

void alerteIntrusion(long distance) {
  digitalWrite(BUZZER, HIGH);
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
  Serial.print("ALERTE:INTRUSION:");
  Serial.println(distance);
  delay(500);
  digitalWrite(BUZZER, LOW);
  delay(500);
}

void etatNormal(long distance) {
  digitalWrite(BUZZER, LOW);
  digitalWrite(LED_ROUGE, LOW);
  digitalWrite(LED_VERTE, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Systeme OK");
  lcd.setCursor(0, 1);
  lcd.print("D:");
  lcd.print(distance);
  lcd.print("cm ");
  afficherHeure();
}

// ── Setup ──
void setup() {
  Serial.begin(9600);

  // LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();

  // RTC
  rtc.Begin();
  rtc.SetIsWriteProtected(false);
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  rtc.SetDateTime(compiled);

  // RFID
  SPI.begin();
  rfid.PCD_Init();

  // Servo
  porte.attach(SERVO_PIN);
  porte.write(0);

  // Broches
  pinMode(TRIG_PIN,   OUTPUT);
  pinMode(ECHO_PIN,   INPUT);
  pinMode(BUZZER,     OUTPUT);
  pinMode(LED_VERTE,  OUTPUT);
  pinMode(LED_ROUGE,  OUTPUT);
  pinMode(FLAMME_PIN, INPUT);

  // Démarrage
  lcd.setCursor(0, 0);
  lcd.print("Smart Home");
  lcd.setCursor(0, 1);
  lcd.print("Demarrage...");
  Serial.println("=== SMART HOME DEMARRE ===");
  delay(2000);
}

// ── Loop ──
void loop() {

  // ── Lecture capteurs ──
  long distance = mesurerDistance();
  bool flamme   = lireFlammeConfirmee();

  Serial.print("Distance:");
  Serial.print(distance);
  Serial.print("cm | Flamme:");
  Serial.println(flamme ? "OUI" : "NON");

  // ── RFID ──
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    Serial.print("UID : ");
    for (byte i = 0; i < rfid.uid.size; i++) {
      Serial.print(rfid.uid.uidByte[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    if (verifierAcces(rfid.uid.uidByte, rfid.uid.size)) {
      accesAutorise();
    } else {
      accesRefuse();
    }
    rfid.PICC_HaltA();
    flammeCompteur = 0;
    return;
  }

  // ── Priorités d'alerte ──

  // Priorité 1 — Incendie
  if (flamme) {
    alerteIncendie();
    flammeCompteur = 0;
  }
  // Priorité 2 — Intrusion
  else if (distance > 0 && distance < SEUIL) {
    alerteIntrusion(distance);
  }
  // État normal
  else {
    etatNormal(distance);
  }

  delay(500);
}
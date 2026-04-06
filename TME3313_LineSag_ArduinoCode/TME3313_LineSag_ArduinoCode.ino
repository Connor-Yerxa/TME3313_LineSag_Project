#include <SoftwareSerial.h>
#include <stdlib.h>
#include <LiquidCrystal.h>

char* addy = "AT+CMGS=\"+15062820026\"\r";
char* faaiz = "AT+CMGS=\"+15064618161\"\r";
char* pratik = "AT+CMGS=\"+15064787090\"\r";
char* number = faaiz;

// Buttons
int progressButton = 4;   // enter / exit menu
const int incButton = 2;  // increase threshold
const int decButton = 3;  // decrease threshold

// Timing
const unsigned long SAMPLE_INTERVAL = 10000;
// const unsigned long SAMPLE_INTERVAL = 3600000;

unsigned long sensTime = 0;

// SMS
char chValue[15];
SoftwareSerial Sim7000G(7, 6);
int SIM_DTR = 5;

// Ultrasonic sensor
const int trigPin = 9;
const int echoPin = 10;
float distance;
float threshold_distance = 5.0;

// Menu
bool menuMode = false;
const float thresholdStep = 0.5;
const float minThreshold = 1.0;
const float maxThreshold = 50.0;

// LCD: RS, EN, D4, D5, D6, D7
LiquidCrystal lcd(12, 11, A1, A2, A3, A4);

// Function declarations
void updateSerial();
void checkingDelay(int ms);
void sendCommand(char* message, int ms);
void SendSMS(char* message);
void simSetup();
float get_cm();

bool isButtonPressed(int pin);
void handleMenu();
void showNormalScreen();
void showMenuScreen();
void showAlertScreen(const char* line2);

void setup() {
  Serial.begin(57600);
  Sim7000G.begin(9600);

  pinMode(progressButton, INPUT);
  pinMode(incButton, INPUT);
  pinMode(decButton, INPUT);

  pinMode(SIM_DTR, OUTPUT);
  digitalWrite(SIM_DTR, HIGH);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SmartLine Init");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");

  simSetup();
  sensTime = millis();

  lcd.clear();
  showNormalScreen();
}

void loop() {
  // Toggle menu
  if (isButtonPressed(progressButton)) {
    menuMode = !menuMode;
    lcd.clear();

    if (menuMode) {
      showMenuScreen();
    } else {
      showNormalScreen();
    }
  }

  if (menuMode) {
    handleMenu();
    updateSerial();
    return;
  }

  if (millis() - sensTime > SAMPLE_INTERVAL) {
    sensTime = millis();
    distance = get_cm();

    if (distance < threshold_distance) {
      dtostrf(distance, 4, 1, chValue);

      char warning[32];
      snprintf(warning, sizeof(warning), "Sag:%scm", chValue);

      showAlertScreen(warning);
      Serial.println(warning);

      // Uncomment when ready
      // SendSMS(warning);

      delay(2000);
    }

    showNormalScreen();
  }

  updateSerial();
}

//////////////////////////////
// LCD DISPLAY FUNCTIONS
//////////////////////////////
void showNormalScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dist:");
  lcd.print(distance, 1);
  lcd.print("cm");

  lcd.setCursor(0, 1);
  lcd.print("Thr:");
  lcd.print(threshold_distance, 1);
  lcd.print("cm");
}

void showMenuScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Threshold");

  lcd.setCursor(0, 1);
  lcd.print(threshold_distance, 1);
  lcd.print(" cm");
}

void showAlertScreen(const char* line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WARNING");
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void handleMenu() {
  bool changed = false;

  if (isButtonPressed(incButton)) {
    threshold_distance += thresholdStep;
    if (threshold_distance > maxThreshold) {
      threshold_distance = maxThreshold;
    }
    changed = true;
  }

  if (isButtonPressed(decButton)) {
    threshold_distance -= thresholdStep;
    if (threshold_distance < minThreshold) {
      threshold_distance = minThreshold;
    }
    changed = true;
  }

  if (changed) {
    showMenuScreen();
  }
}

//////////////////////////////
// BUTTON HANDLING
//////////////////////////////
bool isButtonPressed(int pin) {
  if (digitalRead(pin) == HIGH) {
    delay(30);
    if (digitalRead(pin) == HIGH) {
      while (digitalRead(pin) == HIGH) {}
      delay(30);
      return true;
    }
  }
  return false;
}

//////////////////////////////
// SIM FUNCTIONS
//////////////////////////////
void updateSerial() {
  while (Serial.available()) {
    Sim7000G.write(Serial.read());
  }
  while (Sim7000G.available()) {
    Serial.write(Sim7000G.read());
  }
}

void checkingDelay(int ms) {
  unsigned long t = millis();
  while (t + ms > millis()) {
    updateSerial();
  }
}

void sendCommand(char* message, int ms) {
  Sim7000G.println(message);
  checkingDelay(ms);
}

void SendSMS(char* message) {
  digitalWrite(SIM_DTR, LOW);
  delay(100);

  sendCommand("AT", 100);
  sendCommand(number, 500);

  Sim7000G.print(message);
  delay(10);
  Sim7000G.write(26);

  checkingDelay(1000);
  digitalWrite(SIM_DTR, HIGH);
}

void simSetup() {
  digitalWrite(SIM_DTR, LOW);
  checkingDelay(100);

  sendCommand("AT", 100);
  sendCommand("AT+CPIN?", 100);
  sendCommand("AT+CFUN=1", 100);
  sendCommand("AT+CSQ", 100);
  sendCommand("AT+CCID", 100);
  sendCommand("AT+COPS?", 100);
  sendCommand("AT+CNMP=38", 100);
  sendCommand("AT+CMNB=1", 100);
  sendCommand("AT+CMGF=1", 500);
  sendCommand("AT+CSCS=\"GSM\"", 500);

  digitalWrite(SIM_DTR, HIGH);
}

//////////////////////////////
// ULTRASONIC SENSOR
//////////////////////////////
float get_cm() {
  float duration, dist;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  dist = (duration * 0.0343) / 2.0;

  return dist;
}
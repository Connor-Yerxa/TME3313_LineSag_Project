#include <SoftwareSerial.h>
#include <stdlib.h>
#include <LiquidCrystal.h>

struct ButtonState {
  int pin;
  bool stableState;
  bool lastReading;
  unsigned long lastChangeTime;
};

char* addy = "AT+CMGS=\"+15062820026\"\r";
char* faaiz = "AT+CMGS=\"+15064618161\"\r";
char* pratik = "AT+CMGS=\"+15064787090\"\r";
char* number = faaiz;

// Buttons
int progressButton = 12;   // short press = action, long press = switch menu
const int incButton = 4;   // increase threshold / increase digit
const int decButton = 5;   // decrease threshold / decrease digit

// Timing
const unsigned long SAMPLE_INTERVAL = 1000;
// const unsigned long SAMPLE_INTERVAL = 3600000;

unsigned long sensTime = 0;

// SMS
char chValue[15];
SoftwareSerial Sim7000G(7, 6);
int SIM_DTR = 5;

// Ultrasonic sensor
const int trigPin = 10;
const int echoPin = 11;
float distance;
float threshold_distance = 5.0;

// Menu
bool menuMode = false;
const float thresholdStep = 0.5;
const float minThreshold = 1.0;
const float maxThreshold = 50.0;

// LCD: RS, EN, D4, D5, D6, D7
LiquidCrystal lcd(2, 3, 6, 7, 8, 9);

// Menu states
enum MenuScreen {
  SCREEN_NORMAL,
  SCREEN_THRESHOLD,
  SCREEN_PHONE
};

MenuScreen currentScreen = SCREEN_NORMAL;

// Phone number editing
char phoneDigits[12] = "15064618161";   // 11 digits after +
char smsCommand[30];
int currentDigitIndex = 0;

// Function declarations
void updateSerial();
void checkingDelay(int ms);
void sendCommand(char* message, int ms);
void SendSMS(char* message);
void simSetup();
float get_cm();

void handleThresholdMenu();
void handlePhoneMenu();

void showNormalScreen();
void showThresholdScreen();
void showAlertScreen(const char* line2);
void showPhoneScreen();
void showPhoneSavedScreen();
void updateNumberCommand();

const unsigned long DEBOUNCE_DELAY = 40;
const unsigned long LONG_PRESS_DELAY = 800;

ButtonState progressBtn = {progressButton, HIGH, HIGH, 0};
ButtonState incBtn      = {incButton, HIGH, HIGH, 0};
ButtonState decBtn      = {decButton, HIGH, HIGH, 0};

// Progress button press tracking
bool progressWaitingRelease = false;
bool progressLongPressHandled = false;
unsigned long progressPressStart = 0;

bool wasButtonPressed(ButtonState &btn) {
  bool reading = digitalRead(btn.pin);

  if (reading != btn.lastReading) {
    btn.lastChangeTime = millis();
    btn.lastReading = reading;
  }

  if ((millis() - btn.lastChangeTime) > DEBOUNCE_DELAY) {
    if (reading != btn.stableState) {
      btn.stableState = reading;

      if (btn.stableState == LOW) {
        return true;
      }
    }
  }

  return false;
}

void updateButtonState(ButtonState &btn) {
  bool reading = digitalRead(btn.pin);

  if (reading != btn.lastReading) {
    btn.lastChangeTime = millis();
    btn.lastReading = reading;
  }

  if ((millis() - btn.lastChangeTime) > DEBOUNCE_DELAY) {
    btn.stableState = reading;
  }
}

void switchMenu() {
  lcd.noCursor();

  if (currentScreen == SCREEN_NORMAL) {
    currentScreen = SCREEN_THRESHOLD;
    menuMode = true;
    showThresholdScreen();
  } else if (currentScreen == SCREEN_THRESHOLD) {
    currentScreen = SCREEN_PHONE;
    menuMode = true;
    showPhoneScreen();
  } else {
    currentScreen = SCREEN_NORMAL;
    menuMode = false;
    showNormalScreen();
  }
}

void handleProgressButton() {
  // detect initial press
  if (wasButtonPressed(progressBtn)) {
    progressWaitingRelease = true;
    progressLongPressHandled = false;
    progressPressStart = millis();
  }

  // keep debounce state updated while held/released
  updateButtonState(progressBtn);

  // long press action
  if (progressWaitingRelease && !progressLongPressHandled && progressBtn.stableState == LOW) {
    if (millis() - progressPressStart >= LONG_PRESS_DELAY) {
      progressLongPressHandled = true;
      switchMenu();
    }
  }

  // release after short press
  if (progressWaitingRelease && progressBtn.stableState == HIGH) {
    if (!progressLongPressHandled) {
      // short press action
      if (currentScreen == SCREEN_PHONE) {
        currentDigitIndex++;

        if (currentDigitIndex > 10) {
          currentDigitIndex = 10;
          updateNumberCommand();
          showPhoneSavedScreen();
          delay(1000);
        }

        showPhoneScreen();
      }
    }

    progressWaitingRelease = false;
    progressLongPressHandled = false;
  }
}

void setup() {
  Serial.begin(57600);
  Sim7000G.begin(9600);

  pinMode(progressButton, INPUT_PULLUP);
  pinMode(incButton, INPUT_PULLUP);
  pinMode(decButton, INPUT_PULLUP);

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

  updateNumberCommand();
  simSetup();
  sensTime = millis();

  lcd.clear();
  showNormalScreen();
}

void loop() {
  handleProgressButton();

  if (currentScreen == SCREEN_THRESHOLD) {
    handleThresholdMenu();
    updateSerial();
    return;
  }

  if (currentScreen == SCREEN_PHONE) {
    handlePhoneMenu();
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
  lcd.noCursor();
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

void showThresholdScreen() {
  lcd.noCursor();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Threshold");

  lcd.setCursor(0, 1);
  lcd.print(threshold_distance, 1);
  lcd.print(" cm");
}

void showAlertScreen(const char* line2) {
  lcd.noCursor();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WARNING");
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void showPhoneScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Phone No.");

  lcd.setCursor(0, 1);
  lcd.print("+");
  lcd.print(phoneDigits);

  lcd.setCursor(currentDigitIndex + 1, 1);
  lcd.cursor();
}

void showPhoneSavedScreen() {
  lcd.noCursor();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Phone Saved");

  lcd.setCursor(0, 1);
  lcd.print("+");
  lcd.print(phoneDigits);
}

//////////////////////////////
// MENU HANDLING
//////////////////////////////
void handleThresholdMenu() {
  bool changed = false;

  if (wasButtonPressed(incBtn)) {
    threshold_distance += thresholdStep;
    if (threshold_distance > maxThreshold) {
      threshold_distance = maxThreshold;
    }
    changed = true;
  }

  if (wasButtonPressed(decBtn)) {
    threshold_distance -= thresholdStep;
    if (threshold_distance < minThreshold) {
      threshold_distance = minThreshold;
    }
    changed = true;
  }

  if (changed) {
    showThresholdScreen();
  }
}

void handlePhoneMenu() {
  bool changed = false;

  if (wasButtonPressed(incBtn)) {
    if (phoneDigits[currentDigitIndex] < '9') {
      phoneDigits[currentDigitIndex]++;
    } else {
      phoneDigits[currentDigitIndex] = '0';
    }
    changed = true;
  }

  if (wasButtonPressed(decBtn)) {
    if (phoneDigits[currentDigitIndex] > '0') {
      phoneDigits[currentDigitIndex]--;
    } else {
      phoneDigits[currentDigitIndex] = '9';
    }
    changed = true;
  }

  if (changed) {
    showPhoneScreen();
  }
}

void updateNumberCommand() {
  snprintf(smsCommand, sizeof(smsCommand), "AT+CMGS=\"+%s\"\r", phoneDigits);
  number = smsCommand;
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
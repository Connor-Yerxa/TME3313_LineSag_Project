#include <SoftwareSerial.h>
#include <stdlib.h>

char* addy = "AT+CMGS=\"+15062820026\"\r";
char* faaiz = "AT+CMGS=\"+15064618161\"\r";

char* number = faaiz;

int in=A0, progressButton=8;
// int baudButton=8;

// const unsigned long SAMPLE_INTERVAL = 3600000;       //1 hour
const unsigned long SAMPLE_INTERVAL = 10000;
const unsigned long READ_SMS_INTERVAL = 5000;
unsigned long timeToWait = 0;
unsigned long sensTime = 0;
unsigned long responseTime = 0;
unsigned long startTime = millis();

char chValue[15];

SoftwareSerial Sim7000G(7, 6);
int SIM_DTR = 12;

// ultrasonic sensor
const int trigPin = 9;
const int echoPin = 10;
float distance;
float threshold_distance = 5;

bool ignore=false;
const char* ignore_msg = "ignore";
const char* reset_msg = "reset";

//////////////////////////////////
///////////Sensor/////////////////
//////////////////////////////////
void updateSerial();
void checkingDelay(int ms);
void sendCommand(char* message, int ms);
void SendSMS(char* message);
void simSetup();

//////////////////////////////////
///////////Sensor/////////////////
//////////////////////////////////
void setup() {
  Serial.begin(57600);
  Sim7000G.begin(9600);
  
  // Serial.println("Testing");

  pinMode(progressButton, INPUT);
  // pinMode(baudButton, INPUT);
  pinMode(SIM_DTR, OUTPUT);

  digitalWrite(SIM_DTR, HIGH);
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // while(digitalRead(progressButton)==LOW){setBaudRate(); updateSerial();}
  // delay(2000);

  simSetup();
  sensTime = millis();
  responseTime = sensTime;

  // digitalWrite(SIM_DTR, LOW);
  // delay(50);
  // Sim7000G.println("AT+CMGL=\"ALL\"");
  // digitalWrite(SIM_DTR, HIGH);
}

void loop() {
  if(digitalRead(progressButton) == HIGH)
  {
    Serial.println("Pressed");
    delay(200);
    while (digitalRead(progressButton) == HIGH) {}
    SendSMS("Hey Buddy! From Connor's Arduino");
  }
  if(millis()-sensTime > SAMPLE_INTERVAL && !ignore)
  {
    Serial.println("TIME");
    sensTime = millis();
    distance = get_cm();
    Serial.print("Distance: ");
    Serial.println(distance);
    if(distance < threshold_distance)
    {
      char warning[32];
      dtostrf(distance, 1, 2, chValue);   // width=1, precision=2
      snprintf(warning, 32, "WARNING: Line sagging to %s", chValue);
      Serial.println(warning);
      // SendSMS(warning);
    }
  }

  if(millis()-responseTime > READ_SMS_INTERVAL)
  {
    responseTime = millis();
    if(ignore)
    {
      if(receiveSMS(reset_msg))
      {
        ignore = false;
      }
    }
    else
    {
      if(receiveSMS(ignore_msg))
      {
        ignore = true;
      }
    }
    Serial.println(ignore);
  }
  updateSerial();
}

/////////////////////////////////////////
/////////////////////////////////////////
//////////////SIM////////////////////////

void updateSerial() {
  // while(!Serial.available() & !Sim800l.available()){}
  while (Serial.available()) {
    Sim7000G.write(Serial.read());  //Forward what Serial received to Software Serial Port (Send to SIM)
  }
  while (Sim7000G.available()) {
    Serial.write(Sim7000G.read());  //Forward what Software Serial received to Serial Port (Recieve from SIM)}
  }
}

void checkingDelay(int ms)
{
  // Serial.println("Wainting");
  int t = millis();
  while(t+ms>millis()) updateSerial();
}

// void setBaudRate()
// {
//   if(digitalRead(baudButton) == HIGH)
//   {
//     while(digitalRead(baudButton) == HIGH){}
//     Serial.println("Changing Baud Rate...");
//     Sim7000G.begin(115200);
//     sendCommand("AT+IPR=57600", 1000);
//     Sim7000G.begin(9600);
//     Serial.println("Done");
//   }
// }

void sendCommand(char* message, int ms)
{
  // Serial.print("Sending Command: ");
  // Serial.println(message);
  Sim7000G.println(message);
  checkingDelay(ms);
}

void SendSMS(char* message)
{
  digitalWrite(SIM_DTR, LOW);
  delay(100);
  Serial.println("Sending Text...");
  sendCommand("AT", 100);
  sendCommand(number, 500);
  Sim7000G.print(message);     //the content of the message
  delay(10);
  Sim7000G.write(26);          //the ASCII code of the ctrl+z is 26 (required according to the datasheet)
  checkingDelay(1000);
  Serial.println("<<Text Sent.");
  digitalWrite(SIM_DTR, HIGH);
}

void simSetup()
{
  digitalWrite(SIM_DTR, LOW);
  checkingDelay(100);
  Serial.println("\n\nInitializing...");
  sendCommand("AT", 100);

  sendCommand("AT+CPIN?", 100);
  sendCommand("AT+CFUN=1", 100); //functionality, 1=full fun
  
  sendCommand("AT+CSQ", 100); //Signal Strength
  sendCommand("AT+CCID", 100); //Sim ID
  sendCommand("AT+COPS?", 100); //network

  sendCommand("AT+CNMP=38", 100); //forces module into 4G LTE-M
  sendCommand("AT+CMNB=1", 100); //prefer Cat-M1 (LTE-M); disable NB-IoT
  sendCommand("AT+CMGF=1", 500); // Set the shield to SMS mode
  sendCommand("AT+CSCS=\"GSM\"", 500); //sets sms encodeing type

  sendCommand("AT+CNMI=1,0,0,0,0", 200); // configure receiving sms.

  Serial.println("Done Setting up!\n\n");
  digitalWrite(SIM_DTR, HIGH);
}

String readResponse(unsigned long timeout = 5000)
{
    String out = "";
    unsigned long start = millis();

    while (millis() - start < timeout)
    {
        while (Sim7000G.available())
        {
            char c = Sim7000G.read();
            out += c;

            // SIM7000 always ends responses with "OK\r\n"
            if (out.endsWith("OK\r\n"))
                return out;
        }
    }
    return out;
}

bool receiveSMS(const char* looking_for)
{
    digitalWrite(SIM_DTR, LOW);
    delay(1200); // full wake

    Sim7000G.println("AT+CMGL=\"REC UNREAD\"");
    String response = readResponse(8000);

    digitalWrite(SIM_DTR, HIGH);

    Serial.println("Got Message:");
    Serial.println(response);

    response.toLowerCase();
    return response.indexOf(looking_for) != -1;
}

//////////////////////////////////
///////////Sensor/////////////////
//////////////////////////////////
// ULTRASONIC SENSOR/////////////
float get_cm() {
  float duration, distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(50);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(50);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = (duration*.0343)/2;
  // Serial.print("Distance: ");
  // Serial.println(distance);
  return distance;
}
////LCD DISPLAY////////////////////

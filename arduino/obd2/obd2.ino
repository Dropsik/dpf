/*
 * Forked from https://github.com/yangosoft/dpf and rewrite in Arduino IDE
 * ESP32
 * ELM 327 (v2.1 reported)
 * 0.96" OLED LCD
 * 
 * Basically I want to have information about DPF state.
 * 
 * Currenlty work in progress - not all fuctions works.
 * 
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <BluetoothSerial.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

BluetoothSerial SerialBT;

String name = "OBDII";
String pin = "1234";

const int READ_BUFFER_SIZE = 128;

int counter = 0;

char rxData[READ_BUFFER_SIZE];
char rxIndex = 0;
int rpm, EGT, turboRAW, reading, COOLANT, x, engineLoad, oilTemp;
byte INTEMP, CACT, SPEED;
float BATTERY, turboPRESS, turboMAX;

uint16_t SMC;
uint32_t km;
uint8_t percentFap;
uint32_t speed;

unsigned long time_now2 = 0;
unsigned long period2 = 10000;
unsigned long time_now5 = 0;
unsigned long period5 = 30000;
unsigned long delayTime = 0;

char iBuf[5] = {};
char eBuf[5] = {};
char cBuf[5] = {};
char oBuf[5] = {};

void setup() {
  Serial.begin(115200);
  SerialBT.begin("dpfread", true);
  SerialBT.setPin(pin.c_str());
  Wire.begin(5, 4);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.clearDisplay();

  bool connected = SerialBT.connect(name);
  while (connected == false)
  {
    display.setCursor(0, 0);
    display.clearDisplay();
    display.print("Connecting..."); display.println(counter);
    display.display();
    Serial.println("Connecting...");
    delay(500);
    counter++;
  }

  Serial.println("Init ODB");
  display.clearDisplay();
  display.println("Init OBD");
  display.display();
  initODB();
}

void loop() {
  getRpm();
  getSpeed();
  getBattery();
  getTurboPressure();
  getEngineLoad();
  getEGT();
  if (millis() > time_now2 + period2)
  {
    //getIntakeTemperature(); // no space on oled
    getCoolant();
    //getOilTemp(); // not supported
    time_now2 = millis();
  }
  else if (millis() > time_now5 + period5)
  {
    getSMC();
    time_now5 = millis();
  }
    updateScreen();
}



#define SEND_BLE(X) {\
    SerialBT.print(X);\
    Serial.print("S:");\
    Serial.println(X);\
  }

void initODB()
{
  delay(0); //1000
  SEND_BLE("ATZ\r");
  delay(500); //2000
  readOBD();
  SEND_BLE("ATD\r");
  delay(500); //2000
  readOBD();
  display.print(rxData);
  display.print("\n");
  SEND_BLE("ATE0\r");
  delay(500); //1000
  readOBD();
  display.print(rxData);
  display.print("\n");
  SEND_BLE("ATSP6\r");
  delay(500); //1000
  readOBD();
  display.print(rxData);
  display.print("\n");
  SEND_BLE("ATSH7E0\r");
  delay(700);
  readOBD();
  display.print(rxData);
  display.print("\n");
  //SerialBT.flush();
}

void readOBD()
{
  char c;
  do
  {
    if (SerialBT.available() > 0)
    {
      c = SerialBT.read();
      //lcd.print(c);
      if ((c != '>') && (c != '\r') && (c != '\n'))
      {
        rxData[rxIndex++] = c;
      }
      if (rxIndex > READ_BUFFER_SIZE)
      {
        rxIndex = 0;
      }
    }
  } while (c != '>'); //ELM327 response end
  rxData[rxIndex++] = '\0';
  rxIndex = 0;
  Serial.print("R:");
  Serial.println(rxData);
}

void getRpm()
{
  SerialBT.flush();
  SEND_BLE("010C\r");
  delay(delayTime);
  readOBD();
  rpm = ((strtol(&rxData[6], 0, 16) * 256) + strtol(&rxData[9], 0, 16)) / 4;
}

void getEngineLoad()
{
  SerialBT.flush();
  SEND_BLE("0104\r");
  delay(delayTime);
  readOBD();
  engineLoad = (strtol(&rxData[6], 0, 16) * 100) / 255;
}

void getCoolant()
{
  SerialBT.flush();
  SEND_BLE("0105\r");
  delay(delayTime);
  readOBD();
  COOLANT = strtol(&rxData[6], 0, 16) - 40;
}

//
void getOilTemp()
{
  SerialBT.flush();
  SEND_BLE("015C\r");
  delay(delayTime);
  readOBD();
  oilTemp = strtol(&rxData[6], 0, 16) - 40;
}

void getSMC()
{
  SerialBT.flush();
  SEND_BLE("ATSH7E0\r");
  delay(200);
  readOBD();
  SEND_BLE("0100\r");
  delay(200);
  readOBD();
  SEND_BLE("22114F\r");
  delay(200);
  readOBD();

  SMC = ((strtol(&rxData[9], 0, 16) * 256) + strtol(&rxData[12], 0, 16)); //((A*256)+B)/100

  //KM
  SEND_BLE("221156\r");
  delay(200);
  readOBD();
  km = (strtol(&rxData[9], 0, 16) << 24) + (strtol(&rxData[12], 0, 16) << 16 ) +  (strtol(&rxData[15], 0, 16) << 8) + strtol(&rxData[18], 0, 16);
  Serial.print("\nKM: ");
  SerialBT.println(km);
  km = km / 1000;

  //Percent
  SEND_BLE("22115B\r");
  delay(200);
  readOBD();


  percentFap = strtol(&rxData[9], 0, 16);
}

void getBattery()
{
  SerialBT.flush();
  SEND_BLE("0142\r");
  delay(delayTime);
  readOBD();
  BATTERY = ((strtol(&rxData[6], 0, 16) * 256) + strtol(&rxData[9], 0, 16)); //((A*256)+B)/1000
  BATTERY = BATTERY / 1000;
}

int getIntakePressure()
{
  SerialBT.flush();
  SEND_BLE("010B\r");
  delay(delayTime);
  readOBD();
  return strtol(&rxData[6], 0, 16);
}

int getBarometricPressure()
{
  /*Not supported in Audi A4 B8
    SerialBT.flush();
    SEND_BLE("0133\r");
    delay(delayTime);
    readOBD();
    return strtol(&rxData[6], 0, 16);*/
  return 1; //Approx 1atm ~ 1bar
}

void getTurboPressure()
{
  turboRAW = (getIntakePressure() - getBarometricPressure());
  turboPRESS = turboRAW;
  turboPRESS = (turboPRESS * 0.01);
}

int getSpeed()
{
  SerialBT.flush();
  SEND_BLE("010D\r");
  delay(delayTime);
  readOBD();
  speed = strtol(&rxData[6], 0, 16);
  return speed;
}

void getEGT()
{
  SerialBT.flush();
  SEND_BLE("0178\r");
  delay(100);
  readOBD();
  EGT = (((strtol(&rxData[30], 0, 16) * 256) + strtol(&rxData[33], 0, 16)) / 10) - 40;
}

void getIntakeTemperature()
{
  SerialBT.flush();
  SEND_BLE("010F\r");
  delay(delayTime);
  readOBD();
  INTEMP = strtol(&rxData[6], 0, 16) - 40;
}


void updateScreen()
{
  display.clearDisplay();
  display.setCursor(0, 0);

  snprintf(oBuf, sizeof(oBuf), "%03d", speed);
  display.print("Speed: "); display.print(oBuf);

  snprintf(oBuf, sizeof(oBuf), "%04d", rpm);
  display.print(" RPM: "); display.println(oBuf);

  display.print("Boost: "); display.println(turboPRESS, 2);
  display.print("Aku: "); display.print(BATTERY);
  display.print(" Load: "); display.print(engineLoad); display.println("%");
  // display.print("Oil: "); display.print(oilTemp); //not supported
  
  snprintf(oBuf, sizeof(oBuf), "%03dc", COOLANT);
  display.print("Cool: "); display.print(oBuf); display.print(" State: "); display.println(percentFap);
  
  float SMCF = SMC;
  SMCF = SMCF / 100;
  char buffer[16];
  snprintf(buffer, 16, "%.2f", SMCF);
  display.print("SMC g: "); display.println(buffer);
  snprintf(buffer, 16, "%03d", km);
  display.print("KM FAP: "); display.println(buffer);

  snprintf(eBuf, sizeof(eBuf), "%3d", EGT);
  display.print("EGT: ");display.println(eBuf);
  
  /* //no space on oled
  snprintf(iBuf, sizeof(iBuf), "%3d", INTEMP);
  display.print("Intake: "); display.println(iBuf);
  */
  
  display.display();

}

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

const int MPU_addr = 0x68; // I2C address of the MPU-6500
const int ON_BUTTON_PIN = 18;
const int OFF_BUTTON_PIN = 5;
const int STOLEN_BUTTON_PIN = 15; // pin of button that toggles the being stolen state

const int ON_LED_PIN = 2; // pin that shows recording data
const int PRIMED_LED_PIN = 4; // device is primed to record in stolen mode
const int STOLEN_LED_PIN = 23; // device is in stolen recording mode

const int MICRO_SD_MISO = 19;
const int MICRO_SD_MOSI = 13;
const int MICRO_SD_SCK = 26;
const int MICRO_SD_CS = 14;

bool isOn = false;
bool isStolen = false;
bool isPrimed = false;

unsigned long snatchStartTime = 0;

int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;

String dataBuffer = "";
int count = 0;
const int BATCH_SIZE = 250;

void writeBufferToSD() {
  if (dataBuffer.length() == 0) {
    return; // nothing to write
  }
  File f = SD.open("/data.csv", FILE_APPEND);
  if (f) {
    f.print(dataBuffer);
    f.close();
  }
  dataBuffer = "";
}
  
void setup() {
  // time to stabilize
  delay(1000);


  Wire.begin(21, 22);
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0);
  Wire.endTransmission(true);
  Serial.begin(115200);

  SPI.begin(MICRO_SD_SCK, MICRO_SD_MISO, MICRO_SD_MOSI, MICRO_SD_CS); // use non default pins


  // initialize SD card
  if (!SD.begin(MICRO_SD_CS)) {
    Serial.println("Can't initialize SD Card");
  }
  else {
    Serial.println("SD Card Initialized");

    // File f = SD.open("/data.txt", FILE_APPEND);
    // if (f) {
    //   f.println("Hello, SD");
    //   f.close();
    // }
    // else {
    //   Serial.println("failed to open text file");
    // }
  }


  pinMode(ON_BUTTON_PIN, INPUT_PULLUP);
  pinMode(OFF_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STOLEN_BUTTON_PIN, INPUT_PULLUP);

  pinMode(ON_LED_PIN, OUTPUT);
  pinMode(PRIMED_LED_PIN, OUTPUT);
  pinMode(STOLEN_LED_PIN, OUTPUT);
}

void loop() {

  // signal if the device is recording data
  if (digitalRead(ON_BUTTON_PIN) == LOW) {
    digitalWrite(ON_LED_PIN, HIGH);
    isOn = true;
  }
  if (digitalRead(OFF_BUTTON_PIN) == LOW && isOn) {
    digitalWrite(ON_LED_PIN, LOW);
    isOn = false;
    writeBufferToSD();
    count = 0;
  }


  if (isOn) {
  // prime the device for being stolen
  if (digitalRead(STOLEN_BUTTON_PIN) == LOW) {
    isPrimed = true;
    digitalWrite(PRIMED_LED_PIN, HIGH);
  }

  // when the stolen button is let go of, go into stolen mode for 1.5 seconds
  if (digitalRead(STOLEN_BUTTON_PIN) == HIGH && isPrimed) {
    isPrimed = false;
    isStolen = true;

    // turn off primed LED
    digitalWrite(PRIMED_LED_PIN, LOW);

    // turn on stolen LED
    digitalWrite(STOLEN_LED_PIN, HIGH);

    Serial.println("Stolen!");

    snatchStartTime = millis();
  }

  // turn off stolen mode after 1.5 seconds
  if (isStolen) {
    if (millis() - snatchStartTime >= 1500) {
        isStolen = false;
        digitalWrite(STOLEN_LED_PIN, LOW);
    }
  }


 
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true);         

  AcX=Wire.read()<<8|Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  AcY=Wire.read()<<8|Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp=Wire.read()<<8|Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)


  // // print
  // if (isStolen) {
  //   Serial.printf(">X:%d\n>Y:%d\n>Z:%d\nSTOLEN!\n", AcX, AcY, AcZ);
  // } else {
  //   Serial.printf(">X:%d\n>Y:%d\n>Z:%d\n\n", AcX, AcY, AcZ);
  // }

  // add the numbers to the buffer. if a the buffer has 50 frames, write to the file
  dataBuffer += String(AcX) + "," +
                String(AcY) + "," +
                String(AcZ) + "," +
                String(GyX) + "," +
                String(GyY) + "," +
                String(GyZ) + "," +
                String(isStolen ? 1 : 0) + "\n";

  count++;

  if (count >= BATCH_SIZE) {
    writeBufferToSD();
    count = 0;
  }
}

  delay(20); // 50Hz scrolling speed
}

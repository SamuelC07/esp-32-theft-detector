#include <Arduino.h>
#include <Wire.h>
#include "params.h"

#define THRESHOLD 0.5f // threshold to trigger stolen
#define COUNT_NEEDED 3 // needs 3 stolen windows to count as stolen, each unstolen subtracts 1
#define WINDOW_FRAMES 50

const float NORMALIZATION = 32768.0f; // divide all gyroscope numbers by this to match training

const int MPU_addr = 0x68; // I2C address of the MPU-6500
const int ON_BUTTON_PIN = 18;
const int OFF_BUTTON_PIN = 5;
const int BUZZER_PIN = 25;
const int BUZZER_BUTTON_PIN = 15;

const int ON_LED_PIN = 2; // pin that shows recording data
const int STOLEN_LED_PIN = 23; // device detected being stolen

bool isOn = false;
bool isStolen = false;

unsigned long lastSnatchTime = 0;

int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;

float input_window[INPUT_SIZE] = {0.0f};
float hidden_layer[HIDDEN_SIZE];
float output_layer[OUTPUT_SIZE];

int pos_detection_count = 0;

// move the window 6 elements, or one frame, forward
void push_window(float AcX, float AcY, float AcZ, float GyX, float GyY, float GyZ) {
  memmove(&input_window[0], &input_window[6], (INPUT_SIZE - 6) * sizeof(float));

  // Append new frame at the end
  input_window[294] = AcX;
  input_window[295] = AcY;
  input_window[296] = AcZ;
  input_window[297] = GyX;
  input_window[298] = GyY;
  input_window[299] = GyZ;
}

float predict() {
  // layer 1
  for (int j = 0; j < HIDDEN_SIZE; j++) {
    float sum = b1[j];
    for (int i = 0; i < INPUT_SIZE; i++) {
      sum += input_window[i] * W1[i * HIDDEN_SIZE + j];
    }
    hidden_layer[j] = tanhf(sum);
  }

  // layer 2
  for (int k = 0; k < OUTPUT_SIZE; k++) {
    float sum = b2[k];
    for (int j = 0; j < HIDDEN_SIZE; j++) {
      sum += hidden_layer[j] * W2[j * OUTPUT_SIZE + k];
    }
    output_layer[k] = sum;
  }

  // softmax
  // subtract max value to prevent overflow
  float max_logit = max(output_layer[0], output_layer[1]);
  float exp0 = expf(output_layer[0] - max_logit);
  float exp1 = expf(output_layer[1] - max_logit);

  float theft_prob = exp1 / (exp0 + exp1);
  return theft_prob;
}


void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  pinMode(ON_BUTTON_PIN, INPUT_PULLUP);
  pinMode(OFF_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_BUTTON_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ON_LED_PIN, OUTPUT);
  pinMode(STOLEN_LED_PIN, OUTPUT);
}

void loop() {
  // signal if the device is on
  if (digitalRead(ON_BUTTON_PIN) == LOW) {
    digitalWrite(ON_LED_PIN, HIGH);
    isOn = true;
  }
  if (digitalRead(OFF_BUTTON_PIN) == LOW && isOn) {
    digitalWrite(ON_LED_PIN, LOW);
    digitalWrite(STOLEN_LED_PIN, LOW);
    isOn = false;
    isStolen = false;
    pos_detection_count = 0;
  }

  if (isOn) {
    // read sensors
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

    float norm_ax = (float)AcX / NORMALIZATION;
    float norm_ay = (float)AcY / NORMALIZATION;
    float norm_az = (float)AcZ / NORMALIZATION;
    float norm_gx = (float)GyX / NORMALIZATION;
    float norm_gy = (float)GyY / NORMALIZATION;
    float norm_gz = (float)GyZ / NORMALIZATION;

    push_window(norm_ax, norm_ay, norm_az, norm_gx, norm_gy, norm_gz);

    float theft_prob = predict();

    // logic to determine theft
    // add 1 to the count every time a window is detected as stolen
    // subtract 1 otherwise
    if (theft_prob > THRESHOLD) {
      pos_detection_count++;
    } else {
      pos_detection_count = max(0, pos_detection_count - 1);
    }

    // refresh timer for active detection
    if (pos_detection_count > 0) {
      lastSnatchTime = millis();
    }

    if (pos_detection_count >= COUNT_NEEDED) {
        digitalWrite(STOLEN_LED_PIN, HIGH);
        isStolen = true;
    }

    // turn off stolen light 5 seconds after all activity gone
    if (isStolen && pos_detection_count == 0) {
      if (millis() - lastSnatchTime >= 4000) {
          isStolen = false;
          digitalWrite(STOLEN_LED_PIN, LOW);
      }
    }
  }
  // buzzer control
    bool testButtonPressed = (digitalRead(BUZZER_BUTTON_PIN) == LOW);
    if (isStolen || testButtonPressed) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  delay(20);
}

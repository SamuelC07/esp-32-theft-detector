### Esp-32 ML Theft Detector

An embedded real-time theft detector running on an ESP-32 microcontroller. The device reads a 6-axis IMU over I2C and runs inference locally through a handwritten C++ forward pass.

Weights were trained offline through simulation data and exported directly into a header file for usage in the MLP

## Hardware Setup
-ESP-32 WROOM
-MPU 6050
-Active Buzzer

### To run:
Ensure weights are correct from the training data
Flash the ESP32 (PlatformIO)
Upon pressing power button, LED should turn green

### To train:
Insert micro-SD into reader
Enable training mode
When the prime button is held down, it will record the next 1.5 seconds and log them as stolen upon release
Use data on ml/training.py to receive weights to use in the MLP

For this version, a validation accuracy of 99% was achieved, though real-life testing placed false positive rates at around 10%.

# Autonomous Mobile Robot (RP2040)
**Student Name:** Omar Ben Omar  
**Student Number:** 6564062  

## Project Overview
This repository contains the embedded software for an autonomous mobile robot based on the Raspberry Pi Pico (RP2040 / ARM Cortex-M0+). The project enables the robot to perform line following, obstacle detection, and autonomous navigation using register-level programming and Embedded AI.

## Implementation Details

### Core Assignments
- **Assignment 1:** Variable speed control using PWM.
- **Assignment 2:** Obstacle detection (Ultrasonic HC-SR04) using interrupts and timers.
- **Assignment 3:** Line follower logic using IR sensors (ADC).
- **Assignment 4:** Obstacle avoidance state machine (Line Follow -> Detect -> Go Around -> Return).
- **Assignment 5:** Fixed distance travel using wheel encoders.

### Advanced Features
- **Embedded AI:** Person Detection using TensorFlow Lite for Microcontrollers (TFLM).
  - Model: MobileNet v1 (Int8 quantized).
  - Platform: RP2040 (No external accelerator).

## Structure
- `src/`: Source code for Assignments 1-5.
- `docs/`: Project documentation and manuals.
- `Embedded_AI_Person_Detection/`: Source code and build system for Assignment 6.

## Build Instructions
This project uses the Pico SDK and CMake.

```bash
mkdir build
cd build
cmake ..
make
```

## Hardware
- **MCU:** Raspberry Pi Pico (RP2040)
- **Sensors:** IR Line Sensors, Ultrasonic (HC-SR04), Wheel Encoders
- **Actuators:** DC Motors (H-Bridge driver)

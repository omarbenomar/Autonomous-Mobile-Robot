# CSE2425 RobotLab Assignment 6

## Introduction

In this part of the assignment, we will be deploying our trained model on a microcontroller. In this case this is the RP2040 microcontroller by Raspberry Pi. We have provided a large part of the template, and marked all functions that you must complete using a similar syntax to the Python notebook.

For reference, these look like:
```c
/*
 ****************************************************************************
 * TODO: Implement the function                                             *
 ****************************************************************************
 */

// Replace this comment with your code
 
/*
 ****************************************************************************
 * END OF YOUR CODE                                                         *
 ****************************************************************************
 */
```

To get started, follow the next section and make sure your development environment meets all the requirements.

## Install Requirements

You should already have most of these from doing the previous RobotLab assignments, but for completeness we go over the necessary parts for this assignment.

To continue with the following steps, make sure you have installed and have working:
- gcc
- cmake
- make or ninja

If you use the provided virtual image, this should be the case.

### Global PicoSDK Installation

You may already have installed this globally for the previous lab assignments if you used the Raspberry Pi Pico Extension. If this is the case, you can modify the `CMakeLists.txt` file in the following way to let PicoSDK find this installation.

```
...
# include(third_party/pico-sdk/pico_sdk_init.cmake)
include(pico_sdk_import.cmake) # Alternative: import pico-sdk from system-wide installation
...
```

For the following part, you can then ignore cloning PicoSDK. However, cloning PicoSDK again is a more foolproof way of continuing.

### Clone Third Party Libraries

To run our model on the Raspberry Pi Pico microcontroller, we will be using the TensorFlow Lite Micro (TFLMicro) library. In particular, we will use a fork made by the Raspberry Pi team specifically for running on Raspberry Pi products.

This library allows us to run our models we previously trained in TensorFlow on our microcontroller.

To do this you can run the following commands:

```bash
$ mkdir third_party && cd third_party
$ git clone --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
$ git clone --recurse-submodules https://github.com/raspberrypi/pico-tflmicro.git
```

### Patch Third Party Libraries

Because TFLM is a large library with many tests, compilation may take a long time. To help with this, you can comment out all tests by commenting all tests in their `CMakeLists.txt`. Do this manually or by running the following script.

```bash
sed -i -E '/^[^#].*(tests|examples)/ s/^/# /' third_party/pico-tflmicro/CMakeLists.txt
```

## How to get started with the assignment

You should look through the `src/main_functions.cpp` file and fill in every method that is marked as TODO.

After having completed this, you can load your model data you trained in the previous phase of this assignment in the `src/person_detection_model_data.c` file. You can then build and test out your embedded AI model.

## How to build

To build the project, simply compile the project using the CMake configuration. An example for ninja would be:

```bash
cmake -G Ninja -DPICO_BOARD=pico -B build
ninja -C build
```

An example for your default build system would be:
```bash
cmake -DPICO_BOARD=pico -B build
cmake --build build
```

Your output binary will be under `build/assignment6.uf2`. Upload this to your Raspberry Pi Pico. An example using picotool:

```bash
picotool load build/assignment6.uf2 -f
```

## How to see output

In order to see the output, you must connect with a serial reader such as `GNU screen`, `putty`, `tio` or similar.

An example for screen would be:
115200
```bash
screen /dev/ttyACM0 115200
```
, where `/dev/ttyACM0` is the port you're connecting to and `115200` is the baud rate.

Example output:
```
Serial connected! Starting program...
Setup completed successfully.
Running inference on image with label: notperson
No person detected (p=0.000000).
Running inference on image with label: person
Person detected (p=0.996094)!
...
Running inference on image with label: notperson
No person detected (p=0.000000).
Finished processing images.
Total test images: 32
Correct predictions: 26
Accuracy: 81.25%
Total time: 22533 ms
Average time per image: 704 ms
Run completed.
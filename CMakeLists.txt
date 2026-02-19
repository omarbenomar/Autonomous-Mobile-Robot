# Student name:[Omar Ben Omar]
# Student number:[6564062]

cmake_minimum_required(VERSION 3.13)

# Point to the local Pico SDK installation
set(PICO_SDK_PATH /Users/beno/.pico-sdk/sdk/2.2.0)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
set(PICO_BOARD pico)

# Define the project
project(robotz_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Define the executable target for Assignment 5
add_executable(Ass5
    src/Assignment_5_Fixed_distance_traveling.c
)

# Link libraries for the Assignment 5 target
target_link_libraries(Ass5 pico_stdlib hardware_pwm hardware_gpio hardware_irq hardware_adc)

# Enable USB UART output for Assignment 5
pico_enable_stdio_usb(Ass5 1)
pico_enable_stdio_uart(Ass5 0)

# Create .uf2 file for flashing Assignment 5
pico_add_extra_outputs(Ass5)

# --- Assignment 1: Variable Speed Motor Control ---
add_executable(Ass1
    src/Assignment_1_Variable_speed.c
)
target_link_libraries(Ass1 pico_stdlib hardware_pwm hardware_timer)
pico_enable_stdio_usb(Ass1 1)
pico_enable_stdio_uart(Ass1 0)
pico_add_extra_outputs(Ass1)

# --- Assignment 2: Interrupt Service Routine ---
add_executable(Ass2_ISR
    src/Assignment_2_Obstacle_detection_and_stopping.c
)
target_link_libraries(Ass2_ISR pico_stdlib hardware_pwm hardware_gpio hardware_timer hardware_sync)
pico_enable_stdio_usb(Ass2_ISR 1)
pico_enable_stdio_uart(Ass2_ISR 0)
pico_add_extra_outputs(Ass2_ISR)

# --- Assignment 3: Line Follower ---
add_executable(Ass3_LF
    src/Assignment_3_Line_follower.c
)

# Link libraries
# Note: hardware_adc and hardware_pwm are needed for struct headers
target_link_libraries(Ass3_LF pico_stdlib hardware_adc hardware_pwm hardware_pwm)

# Enable USB UART output
pico_enable_stdio_usb(Ass3_LF 1)
pico_enable_stdio_uart(Ass3_LF 0)

# Create .uf2 file
pico_add_extra_outputs(Ass3_LF)

# --- Assignment 4: Obstacle Avoidance ---
add_executable(Assignment_4_Obstacle_avoidance
    src/Assignment_4_Obstacle_avoidance.c
)
target_link_libraries(Assignment_4_Obstacle_avoidance pico_stdlib hardware_adc hardware_pwm hardware_gpio hardware_timer)
pico_enable_stdio_usb(Assignment_4_Obstacle_avoidance 1)
pico_enable_stdio_uart(Assignment_4_Obstacle_avoidance 0)
pico_add_extra_outputs(Assignment_4_Obstacle_avoidance)
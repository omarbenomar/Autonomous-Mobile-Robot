/*
 * Student name: [Omar Ben Omar]
 * Student number: [6564062]
 */

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/structs/adc.h"
#include "hardware/structs/io_bank0.h"
#include "hardware/structs/pads_bank0.h"
#include "hardware/structs/pwm.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/timer.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <stdio.h>

// --- Pin Definitions ---
#define PIN_SENSOR_L 26
#define PIN_SENSOR_R 27
#define PIN_MOTOR_A_PWM 18
#define PIN_MOTOR_A_DIR 19
#define PIN_MOTOR_B_PWM 20
#define PIN_MOTOR_B_DIR 21
#define PIN_TRIG 9
#define PIN_ECHO 8

// --- Constants ---
#define ADC_THRESHOLD 1100
#define PWM_WRAP 25000
#define BASE_SPEED 16500 // Slightly slower for stability
#define TURN_SPEED 16000
#define STOP_SPEED 0
#define OBSTACLE_DIST_CM 15.0f

// --- Timing Constants for Maneuver (Tune these!) ---
// Assuming robot needs to drive around an IKEA box (~30cm?)
#define TIME_TURN_90 650     // ms to turn approx 90 degrees
#define TIME_DRIVE_OUT 800   // ms to drive away from line
#define TIME_DRIVE_PAST 1800 // ms to drive past the obstacle
#define TIME_DRIVE_IN 800    // ms to drive back towards line

// --- Global Variables ---
volatile uint32_t start_time = 0;
volatile uint32_t pulse_duration = 0;
volatile bool new_reading_available = false;

// --- Hardware Init Functions (Direct Register Access) ---

void adc_init_hw() {
  adc_hw->cs = ADC_CS_EN_BITS;
  while (!(adc_hw->cs & ADC_CS_READY_BITS)) {
    tight_loop_contents();
  }
}

void adc_gpio_init_hw(uint pin) {
  // Set function to NULL (0) - usually implied for ADC but good to be
  // explicit/safe or strict Actually generic usage often leaves it alone, but
  // let's clear it to be safe
  io_bank0_hw->io[pin].ctrl = 0x1f << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // Disable pulls and enable input
  pads_bank0_hw->io[pin] =
      (pads_bank0_hw->io[pin] &
       ~(PADS_BANK0_GPIO0_PUE_BITS | PADS_BANK0_GPIO0_PDE_BITS |
         PADS_BANK0_GPIO0_IE_BITS)) |
      PADS_BANK0_GPIO0_OD_BITS;
}

void gpio_out_init_hw(uint pin) {
  // Set function to SIO (5)
  io_bank0_hw->io[pin].ctrl = 5 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
  // Set Output Enable
  sio_hw->gpio_oe_set = (1ul << pin);
  // Initial State Low
  sio_hw->gpio_clr = (1ul << pin);
}

void gpio_in_init_hw(uint pin) {
  // Set function to SIO (5)
  io_bank0_hw->io[pin].ctrl = 5 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
  // Clear Output Enable (Input)
  sio_hw->gpio_oe_clr = (1ul << pin);
  // Enable Input Buffer
  pads_bank0_hw->io[pin] |= PADS_BANK0_GPIO0_IE_BITS;
}

void pwm_init_hw(uint pin) {
  // Set function to PWM (4)
  io_bank0_hw->io[pin].ctrl = 4 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  uint slice = (pin >> 1) & 7;
  uint channel = pin & 1;

  // Set wrap and div
  pwm_hw->slice[slice].div = (4 << PWM_CH0_DIV_INT_LSB); // Pre-scale
  pwm_hw->slice[slice].top = PWM_WRAP;

  // Set initial compare value to 0
  if (channel == 0) {
    hw_write_masked(&pwm_hw->slice[slice].cc, 0, PWM_CH0_CC_A_BITS);
  } else {
    hw_write_masked(&pwm_hw->slice[slice].cc, 0, PWM_CH0_CC_B_BITS);
  }

  // Enable PWM
  pwm_hw->slice[slice].csr |= PWM_CH0_CSR_EN_BITS;
}

// --- Helper Functions ---

uint16_t adc_read_hw(uint ch) {
  hw_write_masked(&adc_hw->cs, (ch << ADC_CS_AINSEL_LSB), ADC_CS_AINSEL_BITS);
  hw_set_bits(&adc_hw->cs, ADC_CS_START_ONCE_BITS);
  while (!(adc_hw->cs & ADC_CS_READY_BITS)) {
    tight_loop_contents();
  }
  return (uint16_t)adc_hw->result;
}

void pwm_set_duty_hw(uint pin, uint16_t duty) {
  uint slice = (pin >> 1) & 7;
  uint channel = pin & 1;
  if (duty > PWM_WRAP)
    duty = PWM_WRAP;

  if (channel == 0) {
    hw_write_masked(&pwm_hw->slice[slice].cc, (duty << PWM_CH0_CC_A_LSB),
                    PWM_CH0_CC_A_BITS);
  } else {
    hw_write_masked(&pwm_hw->slice[slice].cc, (duty << PWM_CH0_CC_B_LSB),
                    PWM_CH0_CC_B_BITS);
  }
}

// Low-level GPIO write
void gpio_set_hw(uint pin, bool value) {
  if (value)
    sio_hw->gpio_set = (1ul << pin);
  else
    sio_hw->gpio_clr = (1ul << pin);
}

// --- Interrupts ---

void echo_isr(uint gpio, uint32_t events) {
  if (gpio == PIN_ECHO) {
    if (events & GPIO_IRQ_EDGE_RISE) {
      start_time = timer_hw->timelr;
    } else if (events & GPIO_IRQ_EDGE_FALL) {
      uint32_t end_time = timer_hw->timelr;
      pulse_duration = end_time - start_time;
      new_reading_available = true;
    }
  }
}

// --- Main ---

typedef enum {
  STATE_FOLLOW_LINE,
  STATE_STOP_FOR_OBSTACLE,
  STATE_TURN_RIGHT_90,
  STATE_DRIVE_OUT,
  STATE_TURN_LEFT_90_1,
  STATE_DRIVE_PAST,
  STATE_TURN_LEFT_90_2,
  STATE_DRIVE_IN,
  STATE_TURN_RIGHT_ALIGN,
  STATE_REACQUIRE_LINE
} RobotState;

int main() {
  stdio_init_all();
  sleep_ms(2000); // Wait for USB
  printf("Assignment 4: Obstacle Avoidance Started.\n");

  // Init Hardware
  adc_init_hw();
  adc_gpio_init_hw(PIN_SENSOR_L);
  adc_gpio_init_hw(PIN_SENSOR_R);

  pwm_init_hw(PIN_MOTOR_A_PWM); // Left Motor
  pwm_init_hw(PIN_MOTOR_B_PWM); // Right Motor
  gpio_out_init_hw(PIN_MOTOR_A_DIR);
  gpio_out_init_hw(PIN_MOTOR_B_DIR);

  gpio_out_init_hw(PIN_TRIG);
  gpio_in_init_hw(PIN_ECHO);

  // Setup Interrupts
  gpio_set_irq_enabled_with_callback(
      PIN_ECHO, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &echo_isr);

  RobotState state = STATE_FOLLOW_LINE;
  uint32_t state_entry_time = 0;

  // Line Follower Memory
  enum Direction { NONE, LEFT, RIGHT };
  enum Direction last_known_direction = NONE;

  while (true) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // --- Ultrasonic Measurement ---
    // Trigger every 60ms
    static uint32_t last_ping = 0;
    float distance_cm = 999.0f; // Default far

    if (now - last_ping > 60) {
      gpio_set_hw(PIN_TRIG, 1);
      sleep_us(10);
      gpio_set_hw(PIN_TRIG, 0);
      last_ping = now;
    }

    if (new_reading_available) {
      uint32_t pd;
      uint32_t flags = save_and_disable_interrupts();
      pd = pulse_duration;
      new_reading_available = false;
      restore_interrupts(flags);

      distance_cm = (pd * 0.0343f) / 2.0f;
      // Filter noise (0 is usually error)
      if (distance_cm < 2.0f)
        distance_cm = 999.0f;
    }

    // --- State Machine ---

    switch (state) {
    case STATE_FOLLOW_LINE:
      // Check for Obstacle
      if (distance_cm < OBSTACLE_DIST_CM && distance_cm > 2.0f) {
        printf("Obstacle Detected! Dist: %.2f cm\n", distance_cm);
        state = STATE_STOP_FOR_OBSTACLE;
        state_entry_time = now;
        // Stop motors
        pwm_set_duty_hw(PIN_MOTOR_A_PWM, 0);
        pwm_set_duty_hw(PIN_MOTOR_B_PWM, 0);
      } else {
        // Line Follower Logic (from Ass3)
        uint16_t val_l = adc_read_hw(0);
        uint16_t val_r = adc_read_hw(1);
        bool left_is_black = val_l > ADC_THRESHOLD;
        bool right_is_black = val_r > ADC_THRESHOLD;

        if (left_is_black && right_is_black) {
          pwm_set_duty_hw(PIN_MOTOR_A_PWM, BASE_SPEED);
          pwm_set_duty_hw(PIN_MOTOR_B_PWM, BASE_SPEED);
        } else if (!left_is_black && !right_is_black) {
          // Lost logic
          if (last_known_direction == LEFT) {
            pwm_set_duty_hw(PIN_MOTOR_A_PWM, 0);
            pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_SPEED);
          } else if (last_known_direction == RIGHT) {
            pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_SPEED);
            pwm_set_duty_hw(PIN_MOTOR_B_PWM, 0);
          } else {
            pwm_set_duty_hw(PIN_MOTOR_A_PWM, BASE_SPEED);
            pwm_set_duty_hw(PIN_MOTOR_B_PWM, BASE_SPEED);
          }
        } else if (left_is_black) {
          last_known_direction = LEFT;
          pwm_set_duty_hw(PIN_MOTOR_A_PWM, 0);
          pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_SPEED);
        } else if (right_is_black) {
          last_known_direction = RIGHT;
          pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_SPEED);
          pwm_set_duty_hw(PIN_MOTOR_B_PWM, 0);
        }
      }
      break;

    case STATE_STOP_FOR_OBSTACLE:
      if (now - state_entry_time > 1000) { // Wait 1s
        state = STATE_TURN_RIGHT_90;
        state_entry_time = now;
      }
      break;

    case STATE_TURN_RIGHT_90:
      // Turn Right (Left Wheel Fwd, Right Wheel Stop/Back)
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, 0);

      if (now - state_entry_time > TIME_TURN_90) {
        state = STATE_DRIVE_OUT;
        state_entry_time = now;
      }
      break;

    case STATE_DRIVE_OUT:
      // Drive Straight
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, BASE_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, BASE_SPEED);

      if (now - state_entry_time > TIME_DRIVE_OUT) {
        state = STATE_TURN_LEFT_90_1;
        state_entry_time = now;
      }
      break;

    case STATE_TURN_LEFT_90_1:
      // Turn Left (Right Wheel Fwd, Left Wheel Stop)
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, 0);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_SPEED);

      if (now - state_entry_time > TIME_TURN_90) {
        state = STATE_DRIVE_PAST;
        state_entry_time = now;
      }
      break;

    case STATE_DRIVE_PAST:
      // Drive Straight (Parallel to line)
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, BASE_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, BASE_SPEED);

      if (now - state_entry_time > TIME_DRIVE_PAST) {
        state = STATE_TURN_LEFT_90_2;
        state_entry_time = now;
      }
      break;

    case STATE_TURN_LEFT_90_2:
      // Turn Left (Towards Line)
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, 0);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_SPEED);

      if (now - state_entry_time > TIME_TURN_90) {
        state = STATE_DRIVE_IN;
        state_entry_time = now;
      }
      break;

    case STATE_DRIVE_IN:
      // Drive towards potential line location
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, BASE_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, BASE_SPEED);

      // If we hit the line early, stop and follow!
      if (adc_read_hw(0) > ADC_THRESHOLD || adc_read_hw(1) > ADC_THRESHOLD) {
        state = STATE_FOLLOW_LINE;
      }

      if (now - state_entry_time > TIME_DRIVE_IN) {
        state = STATE_TURN_RIGHT_ALIGN;
        state_entry_time = now;
      }
      break;

    case STATE_TURN_RIGHT_ALIGN:
      // Turn Right (Align with line)
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, 0);

      // Check for line during turn
      if (adc_read_hw(0) > ADC_THRESHOLD || adc_read_hw(1) > ADC_THRESHOLD) {
        state = STATE_FOLLOW_LINE;
      }

      if (now - state_entry_time > TIME_TURN_90) {
        state = STATE_FOLLOW_LINE; // Hope we found it or are close
      }
      break;

    case STATE_REACQUIRE_LINE:
      // Just in case, spin to find
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, 0);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_SPEED);

      if (adc_read_hw(0) > ADC_THRESHOLD || adc_read_hw(1) > ADC_THRESHOLD) {
        state = STATE_FOLLOW_LINE;
      }
      break;
    }

    sleep_ms(1);
  }

  return 0;
}

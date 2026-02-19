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
#include "pico/stdlib.h"
#include <stdio.h>

// Definitions
#define PIN_SENSOR_L 26
#define PIN_SENSOR_R 27
#define PIN_MOTOR_A_PWM 18
#define PIN_MOTOR_A_DIR 19
#define PIN_MOTOR_B_PWM 20
#define PIN_MOTOR_B_DIR 21
#define PIN_ENCODER_L 2
#define PIN_ENCODER_R 3

// Constants 
#define LOADING_ZONE 4200
#define ADC_THRESHOLD 1100
#define PWM_WRAP 25000
#define BASE_SPEED 18750      // 75%
#define BOOST_SPEED 22500     // 90%
#define TURN_FAST_SPEED 17500 //
#define TURN_SLOW_SPEED 0     //

// Encoder Constants
#define WHEEL_DIAMETER_MM 66.0f
#define ENCODER_SLOTS 20
#define CALIBRATION_FACTOR 0.312f
// Constant for mm per tick
const float MM_PER_TICK =
    ((3.14159f * WHEEL_DIAMETER_MM) / (float)ENCODER_SLOTS) *
    CALIBRATION_FACTOR;

volatile uint32_t ticks_left = 0;
volatile uint32_t ticks_right = 0;

// Functions for Hardware Structs (from Ass3_LF.c)
void adc_init_hw() {
  adc_hw->cs = ADC_CS_EN_BITS;
  while (!(adc_hw->cs & ADC_CS_READY_BITS)) {
    tight_loop_contents();
  }
}

void adc_gpio_init_hw(uint pin) {
  // set function to null
  io_bank0_hw->io[pin].ctrl = 0x1f << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // disable pulls / input enable
  pads_bank0_hw->io[pin] =
      (pads_bank0_hw->io[pin] &
       ~(PADS_BANK0_GPIO0_PUE_BITS | PADS_BANK0_GPIO0_PDE_BITS |
         PADS_BANK0_GPIO0_IE_BITS)) |
      PADS_BANK0_GPIO0_OD_BITS;
}

void dir_gpio_init_hw(uint pin) {
  // set to sio (5)
  io_bank0_hw->io[pin].ctrl = 5 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // set output enable
  sio_hw->gpio_oe_set = (1ul << pin);

  // set output clear
  sio_hw->gpio_clr = (1ul << pin);
}

// set GPIO properties 
void encoder_gpio_init_hw(uint pin) {
  // Set function to SIO (5) - Input
  io_bank0_hw->io[pin].ctrl = 5 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  // Clear output enable (set as input)
  sio_hw->gpio_oe_clr = (1ul << pin);

  // Enable input in pads
  pads_bank0_hw->io[pin] |= PADS_BANK0_GPIO0_IE_BITS;
  pads_bank0_hw->io[pin] |= PADS_BANK0_GPIO0_SCHMITT_BITS;
}

uint16_t adc_read_hw(uint ch) {
  hw_write_masked(&adc_hw->cs, (ch << ADC_CS_AINSEL_LSB), ADC_CS_AINSEL_BITS);
  hw_set_bits(&adc_hw->cs, ADC_CS_START_ONCE_BITS);
  while (!(adc_hw->cs & ADC_CS_READY_BITS)) {
    tight_loop_contents();
  }
  return (uint16_t)adc_hw->result;
}

void pwm_init_hw(uint pin) {
  // set function to pwm 4
  io_bank0_hw->io[pin].ctrl = 4 << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

  uint slice = (pin >> 1) & 7;
  uint channel = pin & 1;
  pwm_hw->slice[slice].div = (4 << PWM_CH0_DIV_INT_LSB);
  pwm_hw->slice[slice].top = PWM_WRAP;
  if (channel == 0) {
    hw_write_masked(&pwm_hw->slice[slice].cc, 0, PWM_CH0_CC_A_BITS);
  } else {
    hw_write_masked(&pwm_hw->slice[slice].cc, 0, PWM_CH0_CC_B_BITS);
  }
  pwm_hw->slice[slice].csr |= PWM_CH0_CSR_EN_BITS;
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

// ISR
void encoder_isr(uint gpio, uint32_t events) {
  if (gpio == PIN_ENCODER_L) {
    ticks_left++;
  } else if (gpio == PIN_ENCODER_R) {
    ticks_right++;
  }
}

int main() {
  stdio_init_all();
  sleep_ms(2000);
  printf("Ass5: Line Follower with Distance Integration\n");

  // --- Hardware Init ---
  adc_init_hw();
  adc_gpio_init_hw(PIN_SENSOR_L); 
  adc_gpio_init_hw(PIN_SENSOR_R);

  pwm_init_hw(PIN_MOTOR_A_PWM);      
  dir_gpio_init_hw(PIN_MOTOR_A_DIR); 

  pwm_init_hw(PIN_MOTOR_B_PWM);     
  dir_gpio_init_hw(PIN_MOTOR_B_DIR);

  // Encoder Init via Structs
  encoder_gpio_init_hw(PIN_ENCODER_L); 
  encoder_gpio_init_hw(PIN_ENCODER_R); 

  // Interrupts 
  gpio_set_irq_enabled_with_callback(PIN_ENCODER_L, GPIO_IRQ_EDGE_RISE, true,
                                     &encoder_isr);
  gpio_set_irq_enabled(PIN_ENCODER_R, GPIO_IRQ_EDGE_RISE, true);

  // Target Config
  const float TARGET_DISTANCE_MM = 1870.0f; // 1.5 meters
  const uint32_t TARGET_TICKS = (uint32_t)(TARGET_DISTANCE_MM / MM_PER_TICK);

  printf("Target Ticks: %d\n", TARGET_TICKS);

  // Initial Boost
  pwm_set_duty_hw(PIN_MOTOR_A_PWM, BOOST_SPEED);
  pwm_set_duty_hw(PIN_MOTOR_B_PWM, BOOST_SPEED);
  sleep_ms(200);

  enum Direction { NONE, LEFT, RIGHT };
  enum Direction last_known_direction = NONE;

  while (true) {
    // --- Distance Check ---
    uint32_t avg_ticks = (ticks_left + ticks_right) / 2;

    // Status every 200 ms
    static uint32_t last_print = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_print >= 200) {
      printf("Ticks: %d / %d | L: %d R: %d\n", avg_ticks, TARGET_TICKS,
             adc_read_hw(0), adc_read_hw(1));
      last_print = now;
    }

    if (avg_ticks >= TARGET_TICKS) {
      // STOP
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, 0);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, 0);
      printf("Target Reached!\n");
      break;
    }

    // --- Line Follower Logic ---
    uint16_t val_l = adc_read_hw(0);
    uint16_t val_r = adc_read_hw(1);

    bool left_is_black = val_l > ADC_THRESHOLD;
    bool right_is_black = val_r > ADC_THRESHOLD;

    if (left_is_black && right_is_black) {
      // Straight
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, BASE_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, BASE_SPEED);
    } else if (!left_is_black && !right_is_black) {
      // Lost Line - Recover
      if (last_known_direction == LEFT) {
        // Recover Left
        pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_SLOW_SPEED);
        pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_FAST_SPEED);
      } else if (last_known_direction == RIGHT) {
        // Recover Right
        pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_FAST_SPEED);
        pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_SLOW_SPEED);
      } else {
        // Just Forward 
        pwm_set_duty_hw(PIN_MOTOR_A_PWM, BASE_SPEED);
        pwm_set_duty_hw(PIN_MOTOR_B_PWM, BASE_SPEED);
      }
    } else if (left_is_black && !right_is_black) {
      // Turn Left
      last_known_direction = LEFT;
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_SLOW_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_FAST_SPEED);
    } else if (!left_is_black && right_is_black) {
      // Turn Right
      last_known_direction = RIGHT;
      pwm_set_duty_hw(PIN_MOTOR_A_PWM, TURN_FAST_SPEED);
      pwm_set_duty_hw(PIN_MOTOR_B_PWM, TURN_SLOW_SPEED);
    }

    sleep_ms(1); // Small delay
  }

  // End State
  while (true) {
    tight_loop_contents();
  }

  return 0;
}

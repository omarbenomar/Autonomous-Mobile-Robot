#include "main_functions.h"

#include <cstdio>
#include "pico/stdio.h"
#include "pico/stdlib.h"

// Main entry point for the application.
int main(int argc, char* argv[]) {
  if (setup() != 0) {
    printf("Setup failed!\n");
    return -1;
  }
  run();

  // Keep the program alive after running for easy uploading.
  while (true) {
    sleep_ms(1000);
  }
}

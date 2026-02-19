#include "main_functions.h"

#include "image_provider.h"
#include "model_settings.h"
#include "person_detection_model_data.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "image_data.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

namespace {
tflite::ErrorReporter *error_reporter = nullptr;
const tflite::Model *model = nullptr;
tflite::MicroInterpreter *interpreter = nullptr;
TfLiteTensor *input = nullptr;
TfLiteTensor *output = nullptr;

/*
****************************************************************************
* TODO: Implement the function                                             *
****************************************************************************
*/

// TODO 1: An area of memory to use for input, output, and intermediate arrays.
// Kind of like a heap, but with a fixed size for embedded systems.
// Should be large enough to hold all the tensors, but no larger.
constexpr int kTensorArenaSize = 136 * 1024;

/*
****************************************************************************
* END OF YOUR CODE                                                         *
****************************************************************************
*/
alignas(16) static uint8_t tensor_arena[kTensorArenaSize];
} // namespace

int setup() {
  // Setup stdio for printing to USB serial console.
  stdio_init_all();

  while (!stdio_usb_connected()) {
    sleep_ms(100);
  }

  printf("Serial connected! Starting program...\n");

  // Set up logging.
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // Load the model into a usable data structure.
  model = tflite::GetModel(person_detection_int8_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model provided is schema version %d not equal "
                         "to supported version %d.",
                         model->version(), TFLITE_SCHEMA_VERSION);
    return -1;
  }

  // This pulls in all the operation implementations we need.
  static tflite::MicroMutableOpResolver<6> op_resolver;
  /*
  ****************************************************************************
  * TODO: Implement the function                                             *
  ****************************************************************************
  */

  // TODO 1: Add operators required by the model. You can check which
  // operators are needed by looking in the model details or seeing the runtime
  // errors.

  op_resolver.AddAveragePool2D();
  op_resolver.AddConv2D();
  op_resolver.AddDepthwiseConv2D();
  op_resolver.AddReshape();
  op_resolver.AddSoftmax();

  /*
  ****************************************************************************
  * END OF YOUR CODE                                                         *
  ****************************************************************************
  */

  // Build an interpreter to run the model with.
  static tflite::MicroInterpreter static_interpreter(
      model, op_resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
    return -1;
  }

  // Set input/output tensor pointer.
  input = interpreter->input(0);
  output = interpreter->output(0);

  printf("Setup completed successfully.\n");
  return 0;
}

void run() {
  uint64_t start_time = time_us_64();
  int image_label;
  TfLiteStatus status;
  int correct_predictions = 0;
  float image_data[96 * 96] = {0};
  while (true) {
    status = GetImage(error_reporter, kNumCols, kNumRows, kNumChannels,
                      image_data, &image_label);
    if (kTfLiteOk != status) {
      if (status == kTfLiteDelegateDataNotFound) {
        printf("Finished processing images.\n");
        break;
      }
      TF_LITE_REPORT_ERROR(error_reporter, "Image capture failed.");
    }

    /*
    ****************************************************************************
    * TODO: Implement the function                                             *
    ****************************************************************************
    */
    // Preprocess the image data from floating point to int8.
    for (int i = 0; i < kNumCols * kNumRows * kNumChannels; i++) {
      // TODO 1: Convert image_data[i] to int8 using output tensor's
      // quantization parameters
      int8_t quantized = (int8_t)(image_data[i] / input->params.scale +
                                  input->params.zero_point);
      input->data.int8[i] = quantized;
    }
    /*
    ****************************************************************************
    * END OF YOUR CODE                                                         *
    ****************************************************************************
    */

    printf("Running inference on image with label: %s\n",
           kCategoryLabels[image_label]);

    if (kTfLiteOk != interpreter->Invoke()) {
      TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed.");
    }

    int8_t person_score =
        output->data.int8[1]; // Person is index 1, Not person is index 0
    /*
    ****************************************************************************
    * TODO: Implement the function                                             *
    ****************************************************************************
    */

    // TODO 1: Dequantize person_score to floating point using output tensor's
    // quantization parameters
    float person_score_f =
        (person_score - output->params.zero_point) * output->params.scale;

    /*
    ****************************************************************************
    * END OF YOUR CODE                                                         *
    ****************************************************************************
    */

    if (person_score_f > 0.5) {
      printf("Person detected (p=%f)!\n", person_score_f);
      if (image_label == 1)
        correct_predictions++;
    } else {
      printf("No person detected (p=%f).\n", person_score_f);
      if (image_label == 0)
        correct_predictions++;
    }
  }
  uint64_t end_time = time_us_64();

  printf("Total test images: %d\n", kNumTestImages);
  printf("Correct predictions: %d\n", correct_predictions);
  printf("Accuracy: %.2f%%\n",
         (float)correct_predictions / kNumTestImages * 100);
  printf("Total time: %llu ms\n", (end_time - start_time) / 1000);
  printf("Average time per image: %llu ms\n",
         ((end_time - start_time) / kNumTestImages) / 1000);
  printf("Run completed.\n");
}

#ifndef IMAGE_PROVIDER_H_
#define IMAGE_PROVIDER_H_

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"


TfLiteStatus GetImage(tflite::ErrorReporter* error_reporter, int image_width,
                      int image_height, int channels, float* image_data, int* image_label);

#endif  // IMAGE_PROVIDER_H_
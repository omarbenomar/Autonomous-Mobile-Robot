#include "image_provider.h"

#include "model_settings.h"
#include "image_data.h"

static int image_idx = 0;

TfLiteStatus GetImage(tflite::ErrorReporter* error_reporter, int image_width,
                      int image_height, int channels, float* image_data, int* image_label) {
  for (int i = 0; i < image_width * image_height * channels; ++i) {
    image_data[i] = test_image_data[image_idx][i];
  }
  *image_label = test_image_labels[image_idx];

  // Move to the next image for the next call.
  image_idx++;

  if (image_idx >= kNumTestImages) {
    return kTfLiteDelegateDataNotFound;
  }

  return kTfLiteOk;
}
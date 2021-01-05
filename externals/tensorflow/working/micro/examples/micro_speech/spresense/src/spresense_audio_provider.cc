#include SPRESENSE_CONFIG_H
#include "spresense_audio_provider.h"

#include "tensorflow/lite/micro/examples/micro_speech/audio_provider.h"

#include "tensorflow/lite/micro/examples/micro_speech/micro_features/micro_model_settings.h"

namespace {
int16_t g_dummy_audio_data[kMaxAudioSampleSize];
int32_t g_latest_audio_timestamp = 0;
}  // namespace

TfLiteStatus GetAudioSamples(tflite::ErrorReporter* error_reporter,
                             int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples) {
  spresense_audio_getamples(audio_samples_size, audio_samples);
  return kTfLiteOk;
}

int32_t LatestAudioTimestamp() {
  return spresense_audio_lasttimestamp();
}


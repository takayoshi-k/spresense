#include SPRESENSE_CONFIG_H
#include "spresense_audio_provider.h"

#include "tensorflow/lite/micro/examples/micro_speech/audio_provider.h"

#include "tensorflow/lite/micro/examples/micro_speech/micro_features/micro_model_settings.h"

// #define CAPTURE_DATA

#ifdef CAPTURE_DATA
#include <stdio.h>
#include <string.h>

static int16_t tmp_data[16000];
static int data_cnt;
static bool is_printed = false;
#endif

TfLiteStatus GetAudioSamples(tflite::ErrorReporter* error_reporter,
                             int start_ms, int duration_ms,
                             int* audio_samples_size, int16_t** audio_samples) {
  if (spresense_audio_getsamples(start_ms, duration_ms, kAudioSampleFrequency, audio_samples_size, audio_samples)< 0)
    {
      return kTfLiteError;
    }
  else
    {
#ifdef CAPTURE_DATA
      if (start_ms >= 10000)
        {
          if (data_cnt == 0) printf("=========== Start Recording ==============\n");
          if (data_cnt<16000)
            {
              int sz = (16000 - data_cnt) > *audio_samples_size ? *audio_samples_size : (16000 - data_cnt);
              memcpy(&tmp_data[data_cnt], *audio_samples, sz*2);
              data_cnt += sz;
            }
          if (!is_printed && data_cnt >= 16000)
            {
              printf("============ Stop Recording =============\n");
              for (int i=0; i<16000; i++)
                {
                  printf("%d\n", tmp_data[i]);
                }
              is_printed = true;
            }
        }
#endif
      return kTfLiteOk;
    }
}

int32_t LatestAudioTimestamp() {
  return spresense_audio_lasttimestamp();
}


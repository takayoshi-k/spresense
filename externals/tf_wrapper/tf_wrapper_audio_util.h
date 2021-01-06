#ifndef __EXTERNALS_TF_WRAPPER_AUDIO_UTIL_H__
#define __EXTERNALS_TF_WRAPPER_AUDIO_UTIL_H__

#include <memutils/simple_fifo/CMN_SimpleFifo.h>
#include <audio/audio_high_level_api.h>

/****************************************************************************
 * Pre-processor Declarations
 ****************************************************************************/

#define AUDIO_FRAME_SAMPLE_LENGTH (768)

/****************************************************************************
 * Type Declarations
 ****************************************************************************/

typedef void (*fifo_fill_cb_t)(uint32_t size);
typedef void (*annotation_cb_t)(const ErrorAttentionParam *param);

/****************************************************************************
 * Function Prototypes
 ****************************************************************************/

bool initailze_audio_captureing(
    CMN_SimpleFifoHandle *fifo_handle,
    annotation_cb_t anno_cb, fifo_fill_cb_t fill_cb,
    uint32_t sampling_rate, uint32_t chnum, uint32_t sample_per_bits);
void finalize_audio_capturing(void);
bool start_recording(CMN_SimpleFifoHandle *handle);
bool stop_recording(void);

#endif  // __EXTERNALS_TF_WRAPPER_AUDIO_UTIL_H__

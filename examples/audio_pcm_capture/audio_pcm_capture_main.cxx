/****************************************************************************
 * audio_pcm_capture/pcm_capture_main.cxx
 *
 *   Copyright 2019 Sony Semiconductor Solutions Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sony Semiconductor Solutions Corporation nor
 *    the names of its contributors may be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <strings.h>

#include <memutils/simple_fifo/CMN_SimpleFifo.h>

#include "audio_util.h"

// using namespace MemMgrLite;

/****************************************************************************
 * Application parameters
 ****************************************************************************/

/* SAMPLING_RATE
 * 44.1kHz : AS_SAMPLINGRATE_44100
 * 48kHz   : AS_SAMPLINGRATE_48000
 * 192kHz  : AS_SAMPLINGRATE_192000
 */

#define SAMPLINGRATE          AS_SAMPLINGRATE_48000

/* CHANNEL_NUM
 * MONO (1ch)   : AS_CHANNEL_MONO
 * STEREO (2ch) : AS_CHANNEL_STEREO
 * 4ch          : AS_CHANNEL_4CH
 */

#define CHANNEL_NUM        AS_CHANNEL_MONO

/* BIT_LENGTH
 * 16bit : AS_BITLENGTH_16
 * 24bit : AS_BITLENGTH_24
 */

#define BIT_LENGTH            AS_BITLENGTH_16

/* Recording default time(sec). */

#define PCM_CAPTURE_TIME     (10)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define READ_BUFSIZE  (AUDIO_FRAME_SAMPLE_LENGTH * CHANNEL_NUM * 2)  /* 768sample, 1ch, 16bit */

/****************************************************************************
 * Private data
 ****************************************************************************/

CMN_SimpleFifoHandle s_fifo_handle;
static uint16_t captured_samples[READ_BUFSIZE/2];

/****************************************************************************
 * Private functions
 ****************************************************************************/

/****************************************************************************
 * Name: app_pcm_output()
 *
 * Description:
 *      Audio signal process sample code.
 *      Please modify according to the application.
 *      For example, output capture data.
 *
 *      [in] buf   capture data
 *      [in] size  captured samples length in the buf
 *
 ****************************************************************************/

static void app_pcm_output(int16_t *buf, uint32_t size)
{
  printf("Size %d\n" 
         "        %d\n"
         "        %d\n"
         "        %d\n"
         "        %d\n"
         , size,
         buf[0], buf[1], buf[2], buf[3]);
}

/****************************************************************************
 * Name: ()
 *
 * Description:
 *
 ****************************************************************************/

static void outputDeviceCallback(uint32_t size)
{
    /* do nothing */
}

/****************************************************************************
 * Name: ()
 *
 * Description:
 *
 ****************************************************************************/

static void app_attention_callback(const ErrorAttentionParam *attparam)
{
  printf("Attention!! %s L%d ecode %d subcode %d\n",
          attparam->error_filename,
          attparam->line_number,
          attparam->error_code,
          attparam->error_att_sub_code);
}

/****************************************************************************
 * Name: ()
 *
 * Description:
 *
 ****************************************************************************/

static void app_pop_simple_fifo(void)
{
  size_t samples_in_fifo =
    CMN_SimpleFifoGetOccupiedSize(&s_fifo_handle);
  uint32_t output_size = 0;

  while (samples_in_fifo > 0)
    {
      output_size = (samples_in_fifo > READ_BUFSIZE) ?  READ_BUFSIZE : samples_in_fifo;

      if (CMN_SimpleFifoPoll(&s_fifo_handle,
                            (void*)captured_samples,
                            output_size) == 0)
        {
          printf("ERROR: Fail to get data from simple FIFO.\n");
          break;
        }

      /* Data output */

      app_pcm_output((int16_t*)captured_samples, output_size/2);

      samples_in_fifo -= output_size;
    }
}

/****************************************************************************
 * Name: ()
 *
 * Description:
 *
 ****************************************************************************/

static void app_recorde_process(uint32_t rec_time)
{
  /* Timer Start */

  time_t start_time;
  time_t cur_time;

  time(&start_time);

  do
    {
      app_pop_simple_fifo();
    }
  while((time(&cur_time) - start_time) < rec_time);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ()
 *
 * Description:
 *
 ****************************************************************************/

extern "C" int main(int argc, FAR char *argv[])
{
  printf("Start PCM capture example\n");

  if (!initailze_audio_captureing(&s_fifo_handle,
        app_attention_callback, outputDeviceCallback,
        SAMPLINGRATE, CHANNEL_NUM, BIT_LENGTH))
    {
      printf("Error: initialize_audio_captureing()\n");
      return -1;
    }

  if (start_recording(&s_fifo_handle))
    {
      /* Running... */

      printf("Running time is %d sec\n", PCM_CAPTURE_TIME);
      app_recorde_process(PCM_CAPTURE_TIME);

      /* Stop recorder operation. */

      if (!stop_recording())
        {
          printf("Error: stop_recording() failure.\n");
        }
    }

  finalize_audio_capturing();

  printf("Exit PCM capture example\n");

  return 0;
}

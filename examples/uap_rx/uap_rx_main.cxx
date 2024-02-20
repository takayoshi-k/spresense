/****************************************************************************
 * audio_oscillator/audio_oscillator_main.cxx
 *
 *   Copyright 2020 Sony Semiconductor Solutions Corporation
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

// T.Okada 23/09/12
//
// Modified sdk/modules/audio/include/apus/apu_cmd.h
//   typedef int64_t SetOscCmdFreqFine;
//   typedef int64_t SetOscCmdPhase;
//
//   struct SetOscCmdFreqPhase
//   {
//     SetOscCmdFreqFine delta_freq;
//     SetOscCmdPhase phase;
//   };
//
// Modified sdk/modules/include/audio/audio_synthesizer_api.h
//   typedef struct
//   {
//     int64_t              delta_freq;
//     int64_t              phase;
//   } AsSetSynthesizer;
// 
//   bool AS_UpdateFreqPhaseMediaSynthesizer(FAR AsSetSynthesizer *set_param);
//

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sdk/config.h>
#include <stdio.h>
#include <arch/board/board.h>
#include "oscillator.h"
#include "audio/audio_synthesizer_api.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Audio output volume setting */
/* Default Volume. -20dB */
#define VOLUME                80

/* Number of audio output channels */
#define CHANNEL_NUMBER        2

/* Oscillator parameter */
#define OSC_CH_NUM            CHANNEL_NUMBER
#define OSC_BIT_LEN           AS_BITLENGTH_16
#define OSC_SAMPLING_RATE     AS_SAMPLINGRATE_192000
#define OSC_SAMPLE_SIZE       768

/* Ultrasonic wave frequency [Hz] */

#define ULTRASONIC_FREQ (40000) // 40kHz

/* Digital PLL reference sin wave frequency [Hz]*/

#define REF_SIN_FREQ (200) // 200Hz


/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

extern bool app_initialize(void);
extern void app_finalize(void);
extern bool app_set_volume(int master_db);
extern bool app_init_postproc(uint8_t  channel_num,
                              uint8_t  bit_width,
                              uint32_t sampling_rate);

/* Synthesizer */

extern bool app_create_synthesizer(void);
extern bool app_set_synthesizer_status(void);
extern bool app_start_synthesizer(void);
extern bool app_stop_synthesizer(void);
extern bool app_deactive_synthesizer(void);
extern bool app_set_frequency_synthesizer(uint8_t  channel_number,
                                          uint32_t frequency[]);
extern bool app_init_synthesizer(uint8_t  channel_num,
                                 uint8_t  bit_width,
                                 uint32_t sampling_rate,
                                 uint16_t sample_size);

/* Mic inputs */

extern bool app_start_capture(void);
extern bool app_stop_capture(bool rxmonflg);
extern bool app_stop_micfrontend(void);
extern void app_recorde_process(bool txmonflg, bool rxmonflg);


/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Monitor output setting flags */

static bool txmonflg = true;
static bool rxmonflg = true;

static uint32_t fs[CHANNEL_NUMBER] = {ULTRASONIC_FREQ, REF_SIN_FREQ};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void app_main_process(void)
{

 /* Set output mute. */

  if (board_external_amp_mute_control(false) != OK)
    {
      printf("Error: board_external_amp_mute_control(false) failuer.\n");
    }

  /* Start synthesizer operation. */

  if (!app_start_synthesizer())
    {
      printf("Error: app_start_synthesizer() failure.\n");
      return;
    }

  if (!app_set_frequency_synthesizer(CHANNEL_NUMBER, fs))
    {
      printf("Error: app_set_frequency_synthesizer() failure.\n");
      return;
    }

  /* Start Capture operation */
  if (!app_start_capture())
    {
      printf("Error: app_start_capture() failure.\n");
      return;
    }

  printf("Running...\n");

  app_recorde_process(txmonflg, rxmonflg);

}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

extern "C" int main(int argc, FAR char *argv[])
{
  printf("Start UAP example\n");
  printf("ver.24/02/19 23:15\n");

  if( argc > 1){
    if(strcmp(argv[1], "TX") == 0){
      txmonflg = true;
      rxmonflg = false;
    }else if(strcmp(argv[1], "RX") == 0){
      txmonflg = false;
      rxmonflg = true;
    }else if(strcmp(argv[1], "MONOFF") == 0){
      txmonflg = false;
      rxmonflg = false;
    }else{
      txmonflg = true;
      rxmonflg = true;
    }
    if(argc > 2){
      fs[0] = atol(argv[2]);
      printf("output frequency=%ld\n", fs[0]);
    }
  }else{
    txmonflg = true;
    rxmonflg = true;
  }


  /* Waiting for SD card mounting. */

  sleep(1);

  /* Initialize */

  if (!app_initialize())
    {
      printf("Error: app_initialize() failure.\n");
    }

  /* Sreate synthesizer sub system */

  else if (!app_create_synthesizer())
    {
      printf("Error: app_create_tynthesizer() failure.\n");
    }

  else if (!app_init_postproc(OSC_CH_NUM,
                              OSC_BIT_LEN,
                              OSC_SAMPLING_RATE))
    {
      printf("Error: app_init_postproc() failure.\n");
    }

  /* Set synthesizer operation mode. */

  else if (!app_set_synthesizer_status())
    {
      printf("Error: app_set_synthesizer_status() failure.\n");
    }

  /* Initialize synthesizer. */

  else if (!app_init_synthesizer(OSC_CH_NUM,
                                 OSC_BIT_LEN,
                                 OSC_SAMPLING_RATE,
                                 OSC_SAMPLE_SIZE))
    {
      printf("Error: app_init_synthesizer() failure.\n");
    }
  else
    {
      /* Set volume */

      app_set_volume(VOLUME);

      /* Running... */

      app_main_process();
    }

  return 0;
}

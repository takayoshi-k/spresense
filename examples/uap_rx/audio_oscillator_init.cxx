/****************************************************************************
 * audio_oscillator/audio_oscillator_init.cxx
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sdk/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <asmp/mpshm.h>

#include "memutils/simple_fifo/CMN_SimpleFifo.h"

#include "memutils/memory_manager/MemHandle.h"
#include "memutils/message/Message.h"
#include "include/msgq_id.h"
#include "include/mem_layout.h"
#include "include/memory_layout.h"
#include "include/msgq_pool.h"
#include "include/pool_layout.h"
#include "include/fixed_fence.h"

#include "audio/audio_frontend_api.h"
#include "audio/audio_capture_api.h"
#include "audio/audio_message_types.h"
#include "audio/utilities/frame_samples.h"

#include "audio/audio_synthesizer_api.h"
#include "audio/audio_outputmix_api.h"
#include "audio/audio_renderer_api.h"
#ifdef CONFIG_EXAMPLES_AUDIO_OSCILLATOR_USEPOSTPROC
#include "userproc_command.h"
#endif

//#include <wien2_common_defs.h>
//#include <apus/apu_cmd.h>


#include <cstdlib>
#include "../../arm-none-eabi/include/math.h"
#include "arm_math.h"

using namespace MemMgrLite;

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Sampling rate
 * 44.1kHz : AS_SAMPLINGRATE_44100
 * 48kHz   : AS_SAMPLINGRATE_48000
 * 192kHz  : AS_SAMPLINGRATE_192000
 */

#define SAMPLINGRATE          AS_SAMPLINGRATE_192000

/* Channel number
 * MONO (1ch)   : AS_CHANNEL_MONO
 * STEREO (2ch) : AS_CHANNEL_STEREO
 * 4ch          : AS_CHANNEL_4CH
 */

// Mic input channel number
#define CHANNEL_NUMBER        AS_CHANNEL_4CH

/* Bit length
 * 16bit : AS_BITLENGTH_16
 * 24bit : AS_BITLENGTH_24
 */

#define BIT_LENGTH            AS_BITLENGTH_16

/* Size of one block to read from the FIFO
 * Set the maximum size of configurable parameters.
 */

//#define SIMPLE_FIFO_READ_SIZE (768 * 4 * 4)  /* 768sample, 4ch, 24bit */
#define SIMPLE_FIFO_READ_SIZE (1024 * 4 * 4)  /* 768sample, 4ch, 24bit */

/* Number of FIFO stages */

#define SIMPLE_FIFO_FRAME_NUM (30)

/* PCM FIFO buffer size */

#define SIMPLE_FIFO_BUF_SIZE  (SIMPLE_FIFO_READ_SIZE * SIMPLE_FIFO_FRAME_NUM)

/* Section number of memory layout to use */

#define AUDIO_SECTION   SECTION_NO0

#define OSC_CH_NUM (2)

/* Digital PLL Reference sine wave frequency[Hz]*/
#define REF_SIN_FREQ 200

/* Amplitude threshold of 1PPS signal rising edge detection */

#define AMP_TH_1PPS_RISE_DET  (20000)

/* Amplitude difference threshold of 1PPS signal rising edge detection */

#define DAMP_TH_1PPS_RISE_DET (10000)

/* Mic input gain */

#define MIC_GAIN_CH0 (0)
#define MIC_GAIN_CH1 (0)
#define MIC_GAIN_CH2 (210) 
#define MIC_GAIN_CH3 (0)

/* Digital PLL Operation state */

#define DPLL_INIT               0  // Initial state
#define DPLL_COARSE_FREQ        1  // Coarse Frequency acquisition satate
#define DPLL_WAIT_COARSE_PHASE  2  // Wait state for coarse phase acquisition
#define DPLL_COARSE_PHASE       3  // Coarse Phase acquisition state
#define DPLL_WAIT_TRACKING      4  // Wait state for tracking
#define DPLL_TRACKING           5  // Tracking state

/* Digital PLL Frequency loop gain */

#define DPLL_FGAIN (512)

/* Digital PLL Phase loop gain */

#define DPLL_PGAIN (64)

/* Ultrasonic wave frequency [Hz] */

#define ULTRASONIC_FREQ (40000) // 40kHz

/* Symbol rate */

#define DATA_SYMBOL_RATE (5000) // 5kHz

/* M-Seq data */

/*
#define DATA0 0x00000055 // 31..0 time slot
#define DATA1 0x00000055 // 63..32 time slot
#define DATA2 0x00000055 // 95..64 time slot
#define DATA3 0x40000055 // 127.. 96 time slot
*/

#define DATA0 0x00000001 // 31..0 time slot
#define DATA1 0x00000000 // 63..32 time slot
#define DATA2 0x00000000 // 95..64 time slot
#define DATA3 0x00000000 // 127.. 96 time slot

/* correration lengsh = 192k/5k*127 */
//#define CORR_LNG 4877
#define CORR_LNG 15

/* Sample number at 0cm reciveing signal */

#define ZERO_CM_SMPL (258)

/* Distance with one 192 kHz sampling interval [0.01cm]*/

#define SOUND_LENGTH_192K (18)  //0.18cm

#define Q47_2PI 0x800000000000

#define MONLNG 20

extern bool app_update_freq_phase_synthesizer(uint8_t  channel_number,
                                              int64_t frequency[],
                                              int64_t phase[]);

/****************************************************************************
 * Private Type Declarations
 ****************************************************************************/

/* For FIFO. */

struct fifo_info_s
{
  CMN_SimpleFifoHandle  handle;
  uint32_t              fifo_area[SIMPLE_FIFO_BUF_SIZE/sizeof(uint32_t)];
  uint8_t               write_buf[SIMPLE_FIFO_READ_SIZE];
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* For FIFO. */

static fifo_info_s s_fifo;

/* For share memory. */

static mpshm_t s_shm;

/* For target codec parameters. */

static uint32_t  target_samplingrate   = SAMPLINGRATE;
static uint32_t  target_channel_number = CHANNEL_NUMBER;
static uint32_t  target_bit_lengt      = BIT_LENGTH;

static int16_t prv_1pps_amp = 0; // Amplitude of previous 1PPS signal sample
static int16_t prv_ref_amp = 1; // Amplitude of previous referense sin wave sample

static uint32_t smpl=0; // 1pps signal period counter

static bool onepps_1stdet_flg = false; // wait for 1st 1pps signal rise detection
static bool onepps_det_flg = false; // to avoid continuas 1pps signal rise detection
static bool ref_det_flg = true; // referense sin wave zero crossing detection flag after 1st 1pps signal rising edge

static int32_t cmp_out_phase; // Reference sin wave amplitude at 1PPS signal rising edge
static int32_t pre_cmp_out_phase = 0; // Reference sin wave amplitude at previous 1PPS signal rising edge
static uint32_t cmp_out_delta_1pps_smpl; // Number of samples duaring 1PPS signal period = Audio clock frequency
static uint32_t pre_cmp_out_delta_1pps_smpl = 0; // Number of samples duaring previous 1PPS signal period
static uint32_t cmp_out_delta_ref_smpl;  // Number of samples from 1PPS signal rising to referense sin wave zero crossing

static int64_t freq_sum_64 = 0;

static uint32_t flm_cnt = 0;

static int16_t rsig_buf[CORR_LNG] = {0};
static int16_t tx_i_buf[CORR_LNG];
static int16_t tx_q_buf[CORR_LNG];
static uint16_t rsig_buf_adrs = 0;

static int64_t ref_sin_omega;

static int16_t fir_reg[195] = {0};
static uint16_t fir_adrs = 0;
static int32_t fir_out;
static int32_t max_fir_out = 0; // Maximum amplitude of receiving signal 
static int32_t max_cc_pwr = 0; // Maximum power of cross-correlation 

static uint32_t max_smpl_num = 0; // sample number at maximum receiving signal amplitude
static int32_t udm_lng = 0; // Length of ultrasonic distance measurement
static int32_t udm_max_cc_pwr = 0;
static uint32_t udm_max_smpl_num = 0;

static uint32_t p_max_smpl_num = 0; // sample number at maximum receiving signal amplitude
static int32_t p_udm_lng = 0; // Length of ultrasonic distance measurement
static uint32_t p_udm_max_smpl_num = 0;
static int32_t p_udm_max_fir_out = 0;

//static uint32_t st_num = 192000;


/****************************************************************************
 * Private Functions
 ****************************************************************************/

void app_phase_comparator(uint8_t *buf, uint32_t size)
{
  uint32_t i;
  uint32_t corr_smpl;
 // int32_t j0;
 // int32_t j;

/* Mic input channel assignment
 * audata[i*CHANNEL_NUMBER]   : 1PPS signal amplitude
 * audata[i*CHANNEL_NUMBER+1] : Digital PLL reference sin wave amplitude
 * audata[i*CHANNEL_NUMBER+2] : Rx0 data amplitude
 * audata[i*CHANNEL_NUMBER+3] : Rx1 data amplitude
 */
  int16_t *audata;

  uint32_t buf_size; // size of audata

// RX signal delay detection
  int16_t rx_amp;
  int16_t tx_amp_i;
  int16_t tx_amp_q;
  int32_t cc_i;
  int32_t cc_q;
  int32_t cc_pwr;
  q15_t tx_theta;
  uint16_t num_ts;
  uint32_t mod_data;

  audata = (int16_t *) buf;

// size of audata
  buf_size = size/2/CHANNEL_NUMBER;

  cmp_out_phase = 0;
  cmp_out_delta_1pps_smpl = 0;
  cmp_out_delta_ref_smpl = 0;

  for(i=0; i<buf_size; i++){

    // 1pps signal rise detection
    if( AMP_TH_1PPS_RISE_DET < audata[i*CHANNEL_NUMBER] && audata[i*CHANNEL_NUMBER]-prv_1pps_amp > DAMP_TH_1PPS_RISE_DET && !onepps_det_flg)
    {
      // wait 1st 1pps signal rise detection to estimate 1pps signal period 
      if(onepps_1stdet_flg){
        cmp_out_delta_1pps_smpl = smpl;
      }else{
        onepps_1stdet_flg = true;
        cmp_out_delta_1pps_smpl = 0;
      }

      // set 1pps signal rise detection flag 
      onepps_det_flg = true;

      // start reference sin wave Zero crossing detection
      ref_det_flg = false;

      // reference sin wave amplitude at 1PPS signal rising edge
      cmp_out_phase = (int32_t)audata[i*CHANNEL_NUMBER+1];

      // RX distance measurment result output
      // ultrasonic distance measurment result [x100cm]
      udm_lng = (max_smpl_num - ZERO_CM_SMPL)* SOUND_LENGTH_192K;
      udm_max_smpl_num = max_smpl_num;
      udm_max_cc_pwr = max_cc_pwr;
      max_smpl_num = 0;
      max_cc_pwr = 0;

      p_udm_lng = (p_max_smpl_num - ZERO_CM_SMPL)* SOUND_LENGTH_192K;
      p_udm_max_smpl_num = p_max_smpl_num;
      p_udm_max_fir_out = max_fir_out;
      p_max_smpl_num = 0;
      max_fir_out = 0;


//    wave monitor near 1pps signal rise detection timing
/*
        if(i<MONLNG/2){
          j0=0;
        }else{
          j0=i-MONLNG/2;
        }
        printf("detnum=%ld\n", i);
        for(j=j0; j<j0+MONLNG; j++)
        {
          printf("%ld, %6d, %6d, %6d\n", j, audata[j*CHANNEL_NUMBER], audata[j*CHANNEL_NUMBER+1], audata[j*CHANNEL_NUMBER+2]);
        }
        printf("\n");
*/

      // reset 1pps signal period counter
      smpl = 0;
    }else{
      // unset 1pps signal rise detection flag 
      onepps_det_flg = false;
    }

    // Reference sin wave Zero crossing detection after 1pps signal rising edge
    if(prv_ref_amp < 0 && 0 <= audata[i*CHANNEL_NUMBER+1] && !ref_det_flg){
      cmp_out_delta_ref_smpl = smpl;
      ref_det_flg = true;
    }

// T.Okada 23/10/23
    // RX data delay detection
    // cross-colleration

    rsig_buf[rsig_buf_adrs] = audata[i*CHANNEL_NUMBER+2];
    cc_i = 0;
    cc_q = 0;
    for(corr_smpl=0 ; corr_smpl<CORR_LNG ; corr_smpl++){

      if(rsig_buf_adrs + corr_smpl > CORR_LNG){
        rx_amp = rsig_buf[rsig_buf_adrs + corr_smpl - CORR_LNG];
      }else{
        rx_amp = rsig_buf[rsig_buf_adrs + corr_smpl];
      }
      cc_i += tx_i_buf[corr_smpl] * rx_amp;
      cc_q += tx_q_buf[corr_smpl] * rx_amp;
    }
    cc_pwr = cc_i*cc_i + cc_q*cc_q;

    if(max_cc_pwr < cc_pwr){
      max_cc_pwr = cc_pwr;
      max_smpl_num = smpl;
    }

    rsig_buf_adrs++;
    if(rsig_buf_adrs>=CORR_LNG){
      rsig_buf_adrs = 0;
    }

    // power detection
    rx_amp = abs(audata[i*CHANNEL_NUMBER+2]);
    fir_out = fir_out - fir_reg[fir_adrs] + rx_amp;
    fir_reg[fir_adrs] = rx_amp;

    if(max_fir_out < fir_out){
      max_fir_out = fir_out;
      p_max_smpl_num = smpl;
    }

    if(fir_adrs >= 194){
      fir_adrs = 0;      
    }else{
      fir_adrs++;
    }

//    wave monitor
//    if(38 < smpl && smpl < 200){
//    if( smpl == 200 ){
/*
    if(st_num <= smpl && smpl < st_num+50){
      printf("%ld, %d, %d, %d, %ld\n", smpl, audata[i*CHANNEL_NUMBER], audata[i*CHANNEL_NUMBER+1], audata[i*CHANNEL_NUMBER+2], fir_out);
    }else{
      st_num = 192000;
    }
*/
    // count up 1pps signal period counter
    smpl++;

    prv_1pps_amp = audata[i*CHANNEL_NUMBER];
    prv_ref_amp = audata[i*CHANNEL_NUMBER+1];
  }
  flm_cnt++;
 
  return;
}

void tx_sig_gen()
{
  uint32_t corr_smpl;
  uint16_t num_ts;
  uint32_t mod_data;
  q15_t tx_theta;


    for(corr_smpl=0 ; corr_smpl<CORR_LNG ; corr_smpl++){
      tx_theta = (q15_t)(( ULTRASONIC_FREQ / REF_SIN_FREQ *  ref_sin_omega * corr_smpl/ 0x100000000) & 0x7fff); 

      num_ts = corr_smpl * DATA_SYMBOL_RATE / SAMPLINGRATE;
      if(num_ts < 32){
        mod_data = (DATA0 >> num_ts) & 0x00000001;
      }else if(num_ts < 64){
        mod_data = (DATA1 >> (num_ts-32)) & 0x00000001;
      }else if(num_ts < 96){
        mod_data = (DATA3 >> (num_ts-64)) & 0x00000001;
      }else if(num_ts < 127){
        mod_data = (DATA3 >> (num_ts-96)) & 0x00000001;        
      }else{
        mod_data = 0;
        tx_theta = 0;
      }

      tx_i_buf[corr_smpl] = arm_sin_q15(tx_theta);
      tx_q_buf[corr_smpl] = arm_cos_q15(tx_theta);
      if(mod_data == 0){
// BPSK with 127 samples
//        tx_amp_i = -tx_amp_i;
//        tx_amp_q = -tx_amp_q;

// OnOff Keying with 127 samples
        tx_i_buf[corr_smpl] = 0;
        tx_q_buf[corr_smpl] = 0;
      }
    }
}

bool app_receive_object_reply(uint32_t id = 0)
{
  AudioObjReply reply_info;
  AS_ReceiveObjectReply(MSGQ_AUD_MGR, &reply_info);

  if (reply_info.type != AS_OBJ_REPLY_TYPE_REQ)
    {
      printf("app_receive_object_reply() error! type 0x%x\n",
             reply_info.type);
      return false;
    }

  if (id && reply_info.id != id)
    {
      printf("app_receive_object_reply() error! id 0x%lx(request id 0x%lx)\n",
             reply_info.id, id);
      return false;
    }

  if (reply_info.result != OK)
    {
      printf("app_receive_object_reply() error! result 0x%lx\n",
             reply_info.result);
      return false;
    }

  return true;
}

static bool app_init_simple_fifo(void)
{
  if (CMN_SimpleFifoInitialize(&s_fifo.handle,
                               s_fifo.fifo_area,
                               SIMPLE_FIFO_BUF_SIZE, NULL) != 0)
    {
      printf("Error: Fail to initialize simple FIFO.");
      return false;
    }
  CMN_SimpleFifoClear(&s_fifo.handle);

  return true;
}

static void app_pop_simple_fifo(bool is_end_process = false)
{
  size_t occupied_simple_fifo_size =
    CMN_SimpleFifoGetOccupiedSize(&s_fifo.handle);
  uint32_t output_size = 0;

  while (occupied_simple_fifo_size > 0)
    {
      output_size = (occupied_simple_fifo_size > SIMPLE_FIFO_READ_SIZE) ?
        SIMPLE_FIFO_READ_SIZE : occupied_simple_fifo_size;
      if (CMN_SimpleFifoPoll(&s_fifo.handle,
                            (void*)s_fifo.write_buf,
                            output_size) == 0)
        {
          printf("ERROR: Fail to get data from simple FIFO.\n");
          break;
        }

      /* phase comparator */

      app_phase_comparator((uint8_t*)s_fifo.write_buf, output_size);

      if (is_end_process)
        {
          /* At end processing, all remaining data is spit */

          occupied_simple_fifo_size = CMN_SimpleFifoGetOccupiedSize(&s_fifo.handle);
        }
      else
        {
          occupied_simple_fifo_size -= output_size;
        }
    }
}

static bool app_init_mic_gain(void)
{
  cxd56_audio_mic_gain_t  mic_gain;

  mic_gain.gain[0] = MIC_GAIN_CH0;
  mic_gain.gain[1] = MIC_GAIN_CH1;
  mic_gain.gain[2] = MIC_GAIN_CH2;
  mic_gain.gain[3] = MIC_GAIN_CH3;
  mic_gain.gain[4] = 0;
  mic_gain.gain[5] = 0;
  mic_gain.gain[6] = 0;
  mic_gain.gain[7] = 0;

  return (cxd56_audio_set_micgain(&mic_gain) == CXD56_AUDIO_ECODE_OK);
}

static bool app_set_frontend(void)
{
  /* Enable input. */

  if (cxd56_audio_en_input() != CXD56_AUDIO_ECODE_OK)
    {
      return false;
    }

  /* Set frontend parameter */

  AsActivateMicFrontend act_param;

  act_param.param.input_device = AsMicFrontendDeviceMic;
  act_param.cb                 = NULL;

  AS_ActivateMicFrontend(&act_param);

  if (!app_receive_object_reply(MSG_AUD_MFE_CMD_ACT))
    {
      return false;
    }

  return true;
}

static bool app_init_frontend(void)
{
  /* Set frontend init parameter */

  AsInitMicFrontendParam  init_param;

  init_param.channel_number           = target_channel_number;
  init_param.bit_length               = target_bit_lengt;
  init_param.samples_per_frame        = getCapSampleNumPerFrame(AS_CODECTYPE_LPCM,
                                                                target_samplingrate);
  init_param.preproc_type             = AsMicFrontendPreProcThrough;
  init_param.data_path                = AsDataPathSimpleFIFO;
  init_param.dest.simple_fifo_handler = &s_fifo.handle;

  AS_InitMicFrontend(&init_param);

  return app_receive_object_reply(MSG_AUD_MFE_CMD_INIT);
}

bool app_start_capture(void)
{
  CMN_SimpleFifoClear(&s_fifo.handle);

  AsStartMicFrontendParam cmd;

  AS_StartMicFrontend(&cmd);

  return app_receive_object_reply(MSG_AUD_MFE_CMD_START);
}

bool app_stop_micfrontend(void)
{
  AsStopMicFrontendParam  cmd;

  cmd.stop_mode = 0;

  AS_StopMicFrontend(&cmd);

  if (!app_receive_object_reply())
    {
      return false;
    }
  return true;
}

bool app_stop_capture()
{

  app_pop_simple_fifo(true);

  return true;
}

void app_recorde_process(bool txmonflg, bool rxmonflg)
{
  int64_t phase_64[2] = {0, 0};
  int64_t freq_64[2] = {0, 0};
  int32_t delta_smpl = 0; // sampling frequency error [Hz]
  uint32_t dpll_state = DPLL_INIT;
  int32_t fgain;
  int32_t pgain;

  fgain = DPLL_FGAIN;
  pgain = DPLL_PGAIN;
  
  printf("FGAIN=%ld, PGAIN=%ld\n", fgain, pgain);

  ref_sin_omega = Q47_2PI / SAMPLINGRATE * REF_SIN_FREQ; //  2pi = 0x8000_0000_0000 ;

  do
    {
      app_pop_simple_fifo(false);

      if((dpll_state!=DPLL_COARSE_PHASE && cmp_out_delta_1pps_smpl != 0) || (dpll_state==DPLL_COARSE_PHASE && cmp_out_delta_ref_smpl != 0) ){
        switch(dpll_state)
          {
          case DPLL_INIT:
            if(txmonflg==true){
              printf("INIT=0, ");
            }
            dpll_state = DPLL_COARSE_FREQ;
          break;
          case DPLL_COARSE_FREQ:
            delta_smpl = SAMPLINGRATE - cmp_out_delta_1pps_smpl;
            freq_64[0] = Q47_2PI / (SAMPLINGRATE) * REF_SIN_FREQ * delta_smpl / SAMPLINGRATE /2;
            freq_64[1] = freq_64[0];
            phase_64[0] = 0;
            phase_64[1] = 0;
            app_update_freq_phase_synthesizer(OSC_CH_NUM,
                                              freq_64,
                                              phase_64);
            ref_sin_omega += freq_64[0];
            tx_sig_gen();
            if(txmonflg==true){
              printf("Freq=1, ");
            }
            dpll_state = DPLL_WAIT_COARSE_PHASE;
          break;
          case DPLL_WAIT_COARSE_PHASE:
            if(txmonflg==true){
              printf("Phase=0, ");
            }
            dpll_state = DPLL_COARSE_PHASE;
          break;
          case DPLL_COARSE_PHASE:
            freq_64[0] = 0;
            freq_64[1] = 0;
            phase_64[0] = Q47_2PI / (SAMPLINGRATE) * REF_SIN_FREQ * cmp_out_delta_ref_smpl;
            phase_64[1] = phase_64[0];
            app_update_freq_phase_synthesizer(OSC_CH_NUM,
                                              freq_64,
                                              phase_64);
            if(txmonflg==true){
              printf("Phase=1, ");
            }
            dpll_state = DPLL_WAIT_TRACKING;
          break;
          case DPLL_WAIT_TRACKING:
            if(txmonflg==true){
              printf("Track=0, ");
            }
            dpll_state = DPLL_TRACKING;
          break;
          case DPLL_TRACKING:
            if(labs(pre_cmp_out_delta_1pps_smpl - cmp_out_delta_1pps_smpl) < 2){
              freq_64[0] = -(cmp_out_phase - pre_cmp_out_phase )* DPLL_FGAIN -cmp_out_phase * DPLL_PGAIN;
              freq_64[1] = freq_64[0];
              phase_64[0] = 0;
              phase_64[1] = 0;
              app_update_freq_phase_synthesizer(OSC_CH_NUM,
                                                freq_64,
                                                phase_64);
              ref_sin_omega += freq_64[0];
              freq_sum_64 += freq_64[0]; 
              if(txmonflg == true){
                printf("Track=1, ");
              }
            }else{
              if(txmonflg == true){
                printf("Track=2, ");
              }
            } 
            break;
          default:
            break;
          }
          if(txmonflg == true){
            printf("flame=%ld, delta_ref_smpl=%ld, delta_1pps_smpl=%ld, ", flm_cnt, cmp_out_delta_ref_smpl, cmp_out_delta_1pps_smpl);
            printf("phase=%ld, ", cmp_out_phase);
            printf("freq=%lld, freq_sum=%lld", freq_64[0], freq_sum_64);
            if(dpll_state==DPLL_WAIT_TRACKING){
              printf(", phase=%lld\n", phase_64[0]);
            }else{
              printf("\n");
            }
            fflush(stdout);
          }
          if(rxmonflg == true){
            if(dpll_state==DPLL_TRACKING){
              printf("distanse_p = %ld[x100cm], %ld, %ld\n", p_udm_lng, p_udm_max_smpl_num, p_udm_max_fir_out);       
              printf("distanse_c = %ld[x100cm], %ld, %ld\n", udm_lng, udm_max_smpl_num, udm_max_cc_pwr);       
            }
          }

          pre_cmp_out_delta_1pps_smpl = cmp_out_delta_1pps_smpl;
          pre_cmp_out_phase = cmp_out_phase;
          cmp_out_phase = 0;
          cmp_out_delta_1pps_smpl = 0;
          cmp_out_delta_ref_smpl = 0;
      }
    } while(true);
}

static bool receive_object_reply(void)
{
  AudioObjReply reply_info;

  AS_ReceiveObjectReply(MSGQ_AUD_APP, &reply_info);

  return true;
}

/* ------------------------------------------------------------------------ */
static bool create_audio_sub_system(bool is_enable = true)
{
  bool result = true;

  /* Create Frontend. */

  AsCreateMicFrontendParams_t frontend_create_param;
  frontend_create_param.msgq_id.micfrontend = MSGQ_AUD_FRONTEND;
  frontend_create_param.msgq_id.mng         = MSGQ_AUD_MGR;
  frontend_create_param.msgq_id.dsp         = MSGQ_AUD_PREDSP;
  frontend_create_param.pool_id.input       = S0_INPUT_BUF_POOL;
  frontend_create_param.pool_id.output      = S0_NULL_POOL;
  frontend_create_param.pool_id.dsp         = S0_PRE_APU_CMD_POOL;

  result = AS_CreateMicFrontend(&frontend_create_param, NULL);
  if (!result)
    {
      printf("Error: As_CreateMicFrontend() failure. system memory insufficient!\n");
      return false;
    }

  /* Create Capture feature. */

  AsCreateCaptureParam_t capture_create_param;
  capture_create_param.msgq_id.dev0_req  = MSGQ_AUD_CAP;
  capture_create_param.msgq_id.dev0_sync = MSGQ_AUD_CAP_SYNC;
  capture_create_param.msgq_id.dev1_req  = 0xFF;
  capture_create_param.msgq_id.dev1_sync = 0xFF;

  result = AS_CreateCapture(&capture_create_param);
  if (!result)
    {
      printf("Error: As_CreateCapture() failure. system memory insufficient!\n");
      return false;
    }


  /* Create mixer feature. */

  AsCreateOutputMixParams_t mix_param =
  {
    .msgq_id =
    {
      .mixer                   = MSGQ_AUD_OUTPUT_MIX,
      .mng                     = MSGQ_AUD_APP,
      .render_path0_filter_dsp = MSGQ_AUD_PFDSP0,
      .render_path1_filter_dsp = MSGQ_AUD_PFDSP1,
    },
    .pool_id =
    {
      .render_path0_filter_pcm = S0_PF0_PCM_BUF_POOL,
      .render_path1_filter_pcm = S0_PF1_PCM_BUF_POOL,
      .render_path0_filter_dsp = S0_PF0_APU_CMD_POOL,
      .render_path1_filter_dsp = S0_PF1_APU_CMD_POOL,
    },
  };

  result = AS_CreateOutputMixer(&mix_param, NULL);

  if (!result)
    {
      printf("Error: AS_CreateOutputMixer() failed. system memory insufficient!\n");
      return false;
    }

  /* Create renderer feature. */

  AsCreateRendererParam_t rend_param =
  {
    .msgq_id =
    {
      .dev0_req  = MSGQ_AUD_RND_PLY0,
      .dev0_sync = MSGQ_AUD_RND_PLY0_SYNC,
      .dev1_req  = MSGQ_AUD_RND_PLY1,
      .dev1_sync = MSGQ_AUD_RND_PLY1_SYNC,
    },
  };

  result = AS_CreateRenderer(&rend_param);

  if (!result)
    {
      printf("Error: AS_CreateRenderer() failure. system memory insufficient!\n");
      return false;
    }

  return result;
}

/* ------------------------------------------------------------------------ */
static void deact_audio_sub_system(void)
{
  /* The following delete process is not executed when it is not initialized */

  AS_DeleteMicFrontend();
  AS_DeleteCapture();

  AS_DeleteOutputMix();         /* Delete OutputMixer. */

  AS_DeleteRenderer();          /* Delete Renderer. */

  AS_DeleteMediaSynthesizer();  /* Delete Oscillator. */
}

/* ------------------------------------------------------------------------ */
static bool activate_baseband(void)
{
  CXD56_AUDIO_ECODE error_code;

//  printf("Enter: activate_baseband()\n");

  /* Power on audio device */

  error_code = cxd56_audio_poweron();

  if (error_code != CXD56_AUDIO_ECODE_OK)
    {
      printf("cxd56_audio_poweron() error! [%d]\n", error_code);
      return false;
    }

  /* Activate OutputMixer */

  AsActivateOutputMixer mixer_act =
  {
    .output_device = HPOutputDevice,
    .mixer_type    = MainOnly,
#ifdef CONFIG_EXAMPLES_AUDIO_OSCILLATOR_USEPOSTPROC
    .post_enable   = PostFilterEnable,
#else
    .post_enable   = PostFilterDisable,
#endif
    .cb            = NULL,
  };

  AS_ActivateOutputMixer(OutputMixer0, &mixer_act);

  if (!receive_object_reply())
    {
      return false;
    }

  AS_ActivateOutputMixer(OutputMixer1, &mixer_act);

  return receive_object_reply();
}

/* ------------------------------------------------------------------------ */
static bool deactivate_baseband(void)
{
  /* Deactivate MicFrontend*/

  AsDeactivateMicFrontendParam  deact_param;

  AS_DeactivateMicFrontend(&deact_param);

  if (!receive_object_reply())
    {
      printf("AS_DeactivateMIcFrontend(0) error!\n");
    }

  /* Disable input */

  if (cxd56_audio_dis_input() != CXD56_AUDIO_ECODE_OK)
    {
      return false;
    }

  /* Deactivate OutputMixer */

  AsDeactivateOutputMixer mixer_deact;

  AS_DeactivateOutputMixer(OutputMixer0, &mixer_deact);

  if (!receive_object_reply())
    {
      printf("AS_DeactivateOutputMixer(0) error!\n");
    }

  AS_DeactivateOutputMixer(OutputMixer1, &mixer_deact);

  if (!receive_object_reply())
    {
      printf("AS_DeactivateOutputMixer(1) error!\n");
    }

  CXD56_AUDIO_ECODE error_code;

  /* Power off audio device */

  error_code = cxd56_audio_poweroff();

  if (error_code != CXD56_AUDIO_ECODE_OK)
    {
      printf("cxd56_audio_poweroff() error! [%d]\n", error_code);
      return false;
    }

  return true;
}

/* ------------------------------------------------------------------------ */
static bool set_clkmode(void)
{
  CXD56_AUDIO_ECODE error_code;

  error_code = cxd56_audio_set_clkmode(CXD56_AUDIO_CLKMODE_HIRES);


  if (error_code != CXD56_AUDIO_ECODE_OK)
    {
      printf("cxd56_audio_set_clkmode() error! [%d]\n", error_code);
      return false;
    }

  error_code = cxd56_audio_poweroff();
    if (error_code != CXD56_AUDIO_ECODE_OK)
    {
      printf("cxd56_audio_poweroff() error! [%d]\n", error_code);
      return false;
    }

  
  error_code = cxd56_audio_poweron();
  if (error_code != CXD56_AUDIO_ECODE_OK)
    {
      printf("cxd56_audio_poweron() error! [%d]\n", error_code);
      return false;
    }

  return true;
}

/* ------------------------------------------------------------------------ */
static bool init_libraries(void)
{
  int ret;
  uint32_t addr = AUD_SRAM_ADDR;

  /* Initialize shared memory.*/

  ret = mpshm_init(&s_shm, 1, AUD_SRAM_SIZE);
  if (ret < 0)
    {
      printf("Error: mpshm_init() failure. %d\n", ret);
      return false;
    }

  ret = mpshm_remap(&s_shm, (void *)addr);
  if (ret < 0)
    {
      printf("Error: mpshm_remap() failure. %d\n", ret);
      return false;
    }

  /* Initalize MessageLib. */

  err_t err = MsgLib::initFirst(NUM_MSGQ_POOLS, MSGQ_TOP_DRM);
  if (err != ERR_OK)
    {
      printf("Error: MsgLib::initFirst() failure. 0x%x\n", err);
      return false;
    }

  err = MsgLib::initPerCpu();
  if (err != ERR_OK)
    {
      printf("Error: MsgLib::initPerCpu() failure. 0x%x\n", err);
      return false;
    }

  void* mml_data_area = translatePoolAddrToVa(MEMMGR_DATA_AREA_ADDR);
  err = Manager::initFirst(mml_data_area, MEMMGR_DATA_AREA_SIZE);
  if (err != ERR_OK)
    {
      printf("Error: Manager::initFirst() failure. 0x%x\n", err);
      return false;
    }

  err = Manager::initPerCpu(mml_data_area, static_pools, pool_num, layout_no);
  if (err != ERR_OK)
    {
      printf("Error: Manager::initPerCpu() failure. 0x%x\n", err);
      return false;
    }

  /* Create static memory pool of VoiceCall. */

  const uint8_t sec_no = AUDIO_SECTION;
  const NumLayout layout_no = MEM_LAYOUT_OSCILLATOR;
  void* work_va = translatePoolAddrToVa(S0_MEMMGR_WORK_AREA_ADDR);
  const PoolSectionAttr *ptr  = &MemoryPoolLayouts[AUDIO_SECTION][layout_no][0];
  err = Manager::createStaticPools(sec_no,
                                   layout_no,
                                   work_va,
                                   S0_MEMMGR_WORK_AREA_SIZE,
                                   ptr);
  if (err != ERR_OK)
    {
      printf("Error: Manager::createStaticPools() failure. %d\n", err);
      return false;
    }

  return true;
}

/* ------------------------------------------------------------------------ */
static bool finalize_libraries(void)
{
  /* Finalize MessageLib. */

  MsgLib::finalize();

  /* Destroy static pools. */

  MemMgrLite::Manager::destroyStaticPools(AUDIO_SECTION);

  /* Finalize memory manager. */

  MemMgrLite::Manager::finalize();

  /* Destroy shared memory. */

  int ret = mpshm_detach(&s_shm);

  if (ret < 0)
    {
      printf("Error: mpshm_detach() failure. %d\n", ret);
      return false;
    }

  ret = mpshm_destroy(&s_shm);

  if (ret < 0)
    {
      printf("Error: mpshm_destroy() failure. %d\n", ret);
      return false;
    }

  return true;
}

/****************************************************************************
 * public Functions
 ****************************************************************************/

bool app_initialize(void)
{
  bool  ret = false;

  /* First, initialize the shared memory and memory utility used by AudioSubSystem. */

  if (!init_libraries())
    {
      printf("Error: init_libraries() failure.\n");
    }

  /* Next, Create the features used by AudioSubSystem. */

  else if (!create_audio_sub_system())
    {
      printf("Error: act_audiosubsystem() failure.\n");
    }

  /* Change AudioSubsystem to Ready state so that I/O parameters can be changed. */

  else if (!activate_baseband())
    {
      printf("Error: activate_baseband() failure.\n");
    }

    /* Initialize simple fifo. */

  else if (!app_init_simple_fifo())
    {
      printf("Error: app_init_simple_fifo() failure.\n");
    }

  /* Set the initial gain of the microphone to be used. */

  else if (!app_init_mic_gain())
    {
      printf("Error: app_init_mic_gain() failure.\n");
    }

  /* Set audio clock mode. */

  else if (!set_clkmode())
    {
      printf("Error: set_clkmode() failure.\n");
    }

  /* Set frontend operation mode. */

  else if (!app_set_frontend())
    {
      printf("Error: app_set_frontend() failure.\n");
    }

  /* Initialize frontend. */

  if (!app_init_frontend())
    {
      printf("Error: app_init_frontend() failure.\n");
    }
  else
    {
      /* Complete! */

      ret = true;
    }

//  printf("Exit: app_initialize()\n");

  return ret;
}

/* ------------------------------------------------------------------------ */
void app_finalize(void)
{
  /* Deactivate baseband */

  if (!deactivate_baseband())
    {
      printf("Error: deactivate_baseband() failure.\n");
    }

  /* Deactivate the features used by AudioSubSystem. */

  deact_audio_sub_system();

  /* finalize the shared memory and memory utility used by AudioSubSystem. */

  if (!finalize_libraries())
    {
      printf("Error: finalize_libraries() failure.\n");
    }
}

/* ------------------------------------------------------------------------ */
bool app_set_volume(int master_db)
{
  /* Set volume to audio driver */

  CXD56_AUDIO_ECODE error_code;

  error_code = cxd56_audio_set_vol(CXD56_AUDIO_VOLID_MIXER_OUT, master_db);

  if (error_code != CXD56_AUDIO_ECODE_OK)
    {
      printf("cxd56_audio_set_vol() error! [%d]\n", error_code);
      return false;
    }

  error_code = cxd56_audio_set_vol(CXD56_AUDIO_VOLID_MIXER_IN1, 0);

  if (error_code != CXD56_AUDIO_ECODE_OK)
    {
      printf("cxd56_audio_set_vol() error! [%d]\n", error_code);
      return false;
    }

  error_code = cxd56_audio_set_vol(CXD56_AUDIO_VOLID_MIXER_IN2, 0);

  if (error_code != CXD56_AUDIO_ECODE_OK)
    {
      printf("cxd56_audio_set_vol() error! [%d]\n", error_code);
      return false;
    }

  return true;
}

/* ------------------------------------------------------------------------ */
bool app_init_postproc(uint8_t  channel_num,
                       uint8_t  bit_width,
                       uint32_t sampling_rate)
{
#ifdef CONFIG_EXAMPLES_AUDIO_OSCILLATOR_USEPOSTPROC

  AsInitPostProc  init;
  InitParam       initpostcmd;

  initpostcmd.input_channel_num = channel_num;

  init.addr = reinterpret_cast<uint8_t *>(&initpostcmd);
  init.size = sizeof(initpostcmd);

  AS_InitPostprocOutputMixer(OutputMixer0, &init);

  if (!receive_object_reply())
    {
      return false;
    }

  AS_InitPostprocOutputMixer(OutputMixer1, &init);

  return receive_object_reply();

#else
  return true;
#endif
}

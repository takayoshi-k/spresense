/****************************************************************************
 * audio_oscillator/worker_oscillator/userproc/include/oscillator.h
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

#ifndef __OSCILLATOR_H__
#define __OSCILLATOR_H__

#include <wien2_common_defs.h>
#include <apus/apu_cmd.h>

#include <cstdlib>
#include "../../arm-none-eabi/include/math.h"
#include "arm_math.h"

#define MAX_CHANNEL_NUMBER 2

/* Digital PLL reference sin wave frequency [Hz]*/

#define REF_SIN_FREQ (200) // 200Hz

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


/* attenuation of DPLL reference sine wave */
#define ATT_REF_SIN 10

/* initial number of frequency divider at update timing */
#define INIT_FDIV_CNT 3

/* number of backward protection state */
#define BWD_PRTCT_FDIV_CNT (-10)

/* number of forward protection state */
#define FWD_PRTCT_FDIV_CNT (4)

#define Q47_2PI 0x800000000000
#define Q47_MASK 0x7fffffffffff

/*--------------------------------------------------------------------*/
/*  Wave Generator                                                */
/*--------------------------------------------------------------------*/
class GeneratorBase
{
public:
  void init(uint8_t, uint32_t, uint8_t);
  void set(uint32_t);
  void update_freq_phase(int64_t, int64_t);

protected:
  q15_t* update_sample(q15_t* ptr, q15_t val);

  uint32_t  m_sampling_rate;  /**< Sampling rate of data */
  uint8_t   m_channels; /**< number of output signal */

  uint8_t   m_fdiv_cnt;  /**< frequency division counter for 1Hz(1PPS frequency)*/
  int8_t    m_fdiv_cnt_err; /**< error counter of frequency divider for 1Hz*/
  uint64_t  m_theta_fine; /**< fine phase of a sine wave: 0-2pi => 0 - 0x8000_0000_0000 */
  uint64_t  m_omega_fine; /**< fine normalized angular frequency of DPLL referense sine wave with sampling frequency*/
  uint32_t  m_frequency; /**< normalized frequency of a sine wave with DPLL reference sine wave frequency */

};

class SinGenerator : public GeneratorBase
{
public:
  void exec(q15_t*, uint16_t);
};

class ModGenerator : public GeneratorBase
{
public:
  void exec(q15_t*, uint16_t);
};

/*--------------------------------------------------------------------*/
/*  Oscillator                                                        */
/*--------------------------------------------------------------------*/
class Oscillator
{
public:

  Oscillator()
    : m_state(Booted)
  {}

  void parse(Wien2::Apu::Wien2ApuCmd *cmd);

  void illegal(Wien2::Apu::Wien2ApuCmd *cmd);
  void init(Wien2::Apu::Wien2ApuCmd *cmd);
  void exec(Wien2::Apu::Wien2ApuCmd *cmd);
  void flush(Wien2::Apu::Wien2ApuCmd *cmd);
  void set(Wien2::Apu::Wien2ApuCmd *cmd);

private:
  Wien2::WaveMode m_type;
  uint8_t m_channel_num;
  uint8_t m_bit_length;

  GeneratorBase* m_wave[MAX_CHANNEL_NUMBER];
  SinGenerator   m_sin[MAX_CHANNEL_NUMBER];
  ModGenerator   m_mod[MAX_CHANNEL_NUMBER];

  enum OscState
  {
    Booted = 0,
    Ready,
    Active,

    OscStateNum
  };

  OscState m_state;

  typedef void (Oscillator::*CtrlProc)(Wien2::Apu::Wien2ApuCmd *cmd);
  static CtrlProc CtrlFuncTbl[Wien2::Apu::ApuEventTypeNum][OscStateNum];
};

#endif /* __OSCILLATOR_H__ */

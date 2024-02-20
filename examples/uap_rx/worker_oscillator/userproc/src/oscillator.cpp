/****************************************************************************
 * audio_oscillator/worker_oscillator/userproc/src/oscillator.cpp
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


#include "oscillator.h"
#include <audio/audio_synthesizer_api.h>

/*--------------------------------------------------------------------*/
/*   Wave Generator Base                                              */
/*--------------------------------------------------------------------*/
void GeneratorBase::init(uint8_t bits/* only 16bits*/ , uint32_t rate, uint8_t channels)
{
  m_sampling_rate = rate;
  m_channels = (channels < 2) ? 2 : channels;

  m_theta_fine = 0;
  m_omega_fine = 0;
  m_fdiv_cnt = 0;
  m_fdiv_cnt_err = 0;

}

/*--------------------------------------------------------------------*/
void GeneratorBase::set(uint32_t frequency)
{

  m_frequency = frequency / REF_SIN_FREQ;

  if (m_sampling_rate != 0)
    {
      m_omega_fine = Q47_2PI / m_sampling_rate * REF_SIN_FREQ; //  2pi = 0x8000_0000_0000 
    }
}

/*--------------------------------------------------------------------*/
void GeneratorBase::update_freq_phase(int64_t delta_freq, int64_t delta_phase)
{
  m_omega_fine = m_omega_fine + delta_freq;
  m_theta_fine = m_theta_fine + delta_phase;

// initialize frequency division counter
// forward synchronization protection: FWD_PRTCT_FDIV_CNT
// backward synchronization protection: BWD_PRTCT_FDIV_CNT 
  if( m_fdiv_cnt != INIT_FDIV_CNT )
  {
    if(m_fdiv_cnt_err >= FWD_PRTCT_FDIV_CNT)
    {
      m_fdiv_cnt = INIT_FDIV_CNT;
      m_fdiv_cnt_err = 0;
    }else{
      m_fdiv_cnt_err++;
    }
  }else{
    if(m_fdiv_cnt_err > BWD_PRTCT_FDIV_CNT){
      m_fdiv_cnt_err--;
    }
  }
}

/*--------------------------------------------------------------------*/
q15_t* GeneratorBase::update_sample(q15_t* ptr, q15_t val)
{
  *ptr = val;

  if (m_channels < 2)
    {
      *(ptr+1) = val;
    }

  m_theta_fine = m_theta_fine + m_omega_fine;

// frequency divider 
  if(m_theta_fine >= Q47_2PI)
  {
    m_theta_fine = m_theta_fine & Q47_MASK;
    if(m_fdiv_cnt == REF_SIN_FREQ - 1){
      m_fdiv_cnt = 0;
    }else{
      m_fdiv_cnt++;
    }
  }

//  m_theta_fine = m_theta_fine & Q47_MASK;

  ptr += m_channels;
  return ptr;
}

/*--------------------------------------------------------------------*/
/*  Sin Wave Generator                                                */
/*--------------------------------------------------------------------*/
void SinGenerator::exec(q15_t* ptr, uint16_t samples)
{
  q15_t m_theta; // 

  for (int i = 0; i < samples ; i++)
    {
      m_theta = (q15_t)((m_frequency * m_theta_fine / 0x100000000) & 0x7fff);  
      q15_t val = arm_sin_q15(m_theta) / ATT_REF_SIN;
      ptr = update_sample(ptr,val);
    }
}

/*--------------------------------------------------------------------*/
/*  Modulated Wave Generator                                          */
/*--------------------------------------------------------------------*/
void ModGenerator::exec(q15_t* ptr, uint16_t samples)
{
q15_t m_theta;

// data time slot number 
uint64_t num_ts = 0;

// modulated data
uint32_t mod_data;

  for (int i = 0; i < samples ; i++)
    {
      m_theta = (q15_t)((m_frequency * m_theta_fine / 0x100000000) & 0x7fff);  

      num_ts = (m_theta_fine + (uint64_t)m_fdiv_cnt * Q47_2PI) * (DATA_SYMBOL_RATE / REF_SIN_FREQ) / Q47_2PI;

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
        m_theta = 0;
      }

      q15_t val = arm_sin_q15(m_theta);
      if(mod_data == 0){
// BPSK with 127 samples
//        val = -val;

// OnOff Keying with 127 samples
        val = 0;
      }
//      q15_t val = arm_sin_q15(m_theta) / ATT_REF_SIN;
      ptr = update_sample(ptr,val);
    }
}

/*--------------------------------------------------------------------*/
/*  Oscillator                                                        */
/*--------------------------------------------------------------------*/

Oscillator::CtrlProc Oscillator::CtrlFuncTbl[Wien2::Apu::ApuEventTypeNum][OscStateNum] =
{
                /* Booted */          /* Ready */           /* Active */
/* boot   */  { &Oscillator::illegal, &Oscillator::illegal, &Oscillator::illegal },
/* Init   */  { &Oscillator::init,    &Oscillator::init,    &Oscillator::illegal },
/* Exec   */  { &Oscillator::illegal, &Oscillator::exec,    &Oscillator::exec    },
/* Flush  */  { &Oscillator::illegal, &Oscillator::illegal, &Oscillator::flush   },
/* Set    */  { &Oscillator::illegal, &Oscillator::set,     &Oscillator::set     },
/* tuning */  { &Oscillator::illegal, &Oscillator::illegal, &Oscillator::illegal },
/* error  */  { &Oscillator::illegal, &Oscillator::illegal, &Oscillator::illegal }
};

/*--------------------------------------------------------------------*/
void Oscillator::parse(Wien2::Apu::Wien2ApuCmd *cmd)
{
  if (cmd->header.process_mode != Wien2::Apu::OscMode)
    {
      cmd->result.exec_result = Wien2::Apu::ApuExecError;
      return;
    }

  (this->*CtrlFuncTbl[cmd->header.event_type][m_state])(cmd);
}

/*--------------------------------------------------------------------*/
void Oscillator::illegal(Wien2::Apu::Wien2ApuCmd *cmd)
{
  /* Divide the code so that you can distinguish between data errors */

  cmd->result.exec_result = Wien2::Apu::ApuExecError;
}

/*--------------------------------------------------------------------*/
void Oscillator::init(Wien2::Apu::Wien2ApuCmd *cmd)
{
  /* Init signal process. */

  /* Data check */
  m_type        = cmd->init_osc_cmd.type;
  m_bit_length  = cmd->init_osc_cmd.bit_length;
  m_channel_num = cmd->init_osc_cmd.channel_num;

  switch (m_type)
    {
      case AsSynthesizerSinWave:
        m_wave[0] = &m_mod[0];
        m_wave[1] = &m_sin[1];
        break;
      default:
        cmd->result.exec_result = Wien2::Apu::ApuExecError;
        return;
    }

  for (int i = 0; i < m_channel_num; i++)
    {
      m_wave[i]->init(m_bit_length,
                      cmd->init_osc_cmd.sampling_rate,
                      m_channel_num);
    }

  m_state = Ready;


  cmd->result.exec_result = Wien2::Apu::ApuExecOK;
}


/*--------------------------------------------------------------------*/
void Oscillator::exec(Wien2::Apu::Wien2ApuCmd *cmd)
{
   /* Execute process to input audio data. */

  q15_t* ptr = (q15_t*)cmd->exec_osc_cmd.buffer.p_buffer;

  /* Byte size per sample.
   * If ch num is 1, but need to extend mono data to L and R ch.
   */
  uint16_t samples = cmd->exec_osc_cmd.buffer.size/m_channel_num/(m_bit_length/8);

  m_mod[0].exec((ptr), samples);
  m_sin[1].exec((ptr + 1), samples);

  m_state = Active;

  cmd->result.exec_result = Wien2::Apu::ApuExecOK;
}


/*--------------------------------------------------------------------*/
void Oscillator::flush(Wien2::Apu::Wien2ApuCmd *cmd)
{
  /* Flush process. */

  m_state = Ready;

  cmd->result.exec_result = Wien2::Apu::ApuExecOK;
}

/*--------------------------------------------------------------------*/
void Oscillator::set(Wien2::Apu::Wien2ApuCmd *cmd)
{
  /* Set process parameters. */

  uint32_t  type = cmd->setparam_osc_cmd.type;

  if (type & Wien2::Apu::OscTypeFrequency)
  {
    for (int i = 0; i < m_channel_num; i++)
    {
      m_wave[i]->set(cmd->setparam_osc_cmd.frequency[i]);
    }
  }
  if (type & Wien2::Apu::OscTypeUpdateFreqPhase)
  {
    for (int i = 0; i < m_channel_num; i++)
    {
      m_wave[i]->update_freq_phase(cmd->setparam_osc_cmd.freq_phase.delta_freq[i], cmd->setparam_osc_cmd.freq_phase.phase[i]);
    }
  }

  cmd->result.exec_result = Wien2::Apu::ApuExecOK;
}

/*
 * MIT License
 *
 * Copyright (c) 2020 Newracom, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef __COMMON_H__
#define __COMMON_H__
/**********************************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <stdbool.h>

#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include "cli.h"

#define log_printf(fmt, ...)	printf(" " fmt, ##__VA_ARGS__)

#define log_trace()				log_printf("%s::%d\n", __func__, __LINE__)
#define log_debug(fmt, ...)		log_printf(fmt, ##__VA_ARGS__)
#define log_info(fmt, ...)		log_printf(fmt, ##__VA_ARGS__)
#define log_error(fmt, ...)		log_printf("%s::%d: " fmt, __func__, __LINE__, ##__VA_ARGS__)

void printer(char *msg, char *b, int len);

extern uint32_t spi_clock_rate;


extern uint32_t flags;
extern int last_sck_id;
extern uint8_t nrc_connected, nrc_attached, show_atcmd_output, iperf_abort;


#define FLAGS_IPERF_BIT			0
#define FLAGS_SHOW_DATA_BIT		1
#define FLAGS_SHOW_ATTX_BIT		2
#define FLAGS_SHOW_ATRX_BIT		3
#define FLAGS_PASSMODE_BIT		4


#define FLAGS_STN_AP_BIT		16
#define FLAGS_AUTO_START_BIT	17
#define FLAGS_EIRQ_BIT			18

#define FLAGS_IPERF_TIMES		(1<<FLAGS_IPERF_BIT)			// Debug, prints iperf transmit times
#define FLAGS_STN_AP			(1<<FLAGS_STN_AP_BIT)			// 0 for stn or 1 for AP
#define FLAGS_AUTO_START		(1<<FLAGS_AUTO_START_BIT)		// 1 to start the configured AP/STN
#define FLAGS_IRQ_ENABLE		(1<<FLAGS_EIRQ_BIT)				// Operate in interrupt mode instead of polling mode (default)
#define FLAGS_SHOW_DATA			(1<<FLAGS_SHOW_DATA_BIT)		// Only for debug test data
#define FLAGS_SHOW_ATTX			(1<<FLAGS_SHOW_ATTX_BIT)		// Show ATCMD transmissions
#define FLAGS_SHOW_ATRX			(1<<FLAGS_SHOW_ATRX_BIT)		// Show ATCMD receptions
#define FLAGS_PASSMODE			(1<<FLAGS_PASSMODE_BIT)			// Enable passthrough mode for streaming

#define FLAGS_DEFAULT (FLAGS_IRQ_ENABLE | FLAGS_PASSMODE)		// 0x0004001c
/*
typedef enum
{
	false = 0,
	true = 1
} bool;
*/
/**********************************************************************************************/
#endif /* #ifndef __COMMON_H__ */

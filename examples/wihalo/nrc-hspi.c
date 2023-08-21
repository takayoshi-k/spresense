#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

#include <nuttx/spi/spi_transfer.h>

#include "nrc-hspi.h"
/* #include "raspi-hif.h" */
#include "util.h"
#include "common.h"

#define _hspi_log(fmt, ...)				if(enable_debug) log_printf(fmt, ##__VA_ARGS__)
#define _hspi_print(fmt, ...)				printf(fmt, ##__VA_ARGS__)

#define _hspi_read_debug(fmt, ...)		if(enable_debug) _hspi_log("hspi_read: " fmt, ##__VA_ARGS__)
#define _hspi_write_debug(fmt, ...)		if(enable_debug) _hspi_log("hspi_write: " fmt, ##__VA_ARGS__)

/**********************************************************************************************/

//#define CONFIG_HSPI_BIG_ENDIAN
//#define CONFIG_HSPI_REG_PRINT

static hspi_info_t g_hspi_info =
{
	.active = 0,
};

#define HSPI_QUEUE_STATUS()					&g_hspi_info.queue.status

#define HSPI_TXQ_SLOT_NUM()					g_hspi_info.queue.slot[HSPI_TXQ].num
#define HSPI_TXQ_SLOT_SIZE()				g_hspi_info.queue.slot[HSPI_TXQ].size
#define HSPI_TXQ_SLOT_COUNT()				g_hspi_info.queue.status.txq.slot_cnt
#define HSPI_TXQ_SLOT_COUNT_UPDATE(c)		g_hspi_info.queue.status.txq.slot_cnt = (c)

#define HSPI_RXQ_SLOT_NUM()					g_hspi_info.queue.slot[HSPI_RXQ].num
#define HSPI_RXQ_SLOT_SIZE()				g_hspi_info.queue.slot[HSPI_RXQ].size
#define HSPI_RXQ_SLOT_COUNT()				g_hspi_info.queue.status.rxq.slot_cnt
#define HSPI_RXQ_SLOT_COUNT_UPDATE(c)		g_hspi_info.queue.status.rxq.slot_cnt = (c)

#define SPI_TRANSFER(tx, rx, len)			g_hspi_info.ops.spi_transfer(tx, rx, len)

/* VADIVEL: to resolve AT cmds send and recv issue 
 * multiple places using spidev_open() call
 * get the fd value, declared as global variable using everywhere
 * to avoid read error issue */
extern int spi_fd;
extern int read_status;

static int spidev_transfer(int fd, FAR struct spi_sequence_s *seq)
{
  /* Perform the IOCTL */
     int ret;
	 FAR struct spi_trans_s *trans = seq->trans;
	 //printf("ioctl: %p, %p, %d\n", trans->txbuffer, trans->rxbuffer, trans->nwords);

	ret = ioctl(spi_fd, SPIIOC_TRANSFER, (unsigned long)((uintptr_t)seq));
		if(ret != 0) printf("ioctl error %d: %d\n", ret, errno);
    return ret;

}

static int HSPI_ACTIVE (void)
{
	if (g_hspi_info.active == 0)
		printf("hspi is not opened.\n");

	return g_hspi_info.active;
}

/**********************************************************************************************/

static uint8_t hspi_crc7 (char *data, int len)
{
	uint8_t crc = 0;
	int i, j;

	for (i = 0 ; i < len ; i++)
	{
		crc ^= data[i];

		for (j = 0 ; j < 8 ; j++)
		{
			if (crc & 0x80)
				crc ^= 0x89;

			crc <<= 1;
		}
	}

	return crc >> 1;
}

static uint16_t hspi_convert_byteorder_16bit (uint16_t val)
{
	union
	{
		uint16_t s_val;
		uint8_t b_val[2];
	} src, dest;

#ifdef CONFIG_HOST_BIG_ENDIAN
	return val;
#endif

	src.s_val = val;

	dest.b_val[0] = src.b_val[1];
	dest.b_val[1] = src.b_val[0];

	return dest.s_val;
}

static uint32_t hspi_convert_byteorder_32bit (uint32_t val)
{
	union
	{
		uint32_t l_val;
		uint8_t b_val[4];
	} src, dest;

#ifdef CONFIG_HOST_BIG_ENDIAN
	return val;
#endif

	src.l_val = val;

	dest.b_val[0] = src.b_val[3];
	dest.b_val[1] = src.b_val[2];
	dest.b_val[2] = src.b_val[1];
	dest.b_val[3] = src.b_val[0];

	return dest.l_val;
}

#define _CPU_TO_BE16(val)	hspi_convert_byteorder_16bit(val)
#define _CPU_TO_BE32(val)	hspi_convert_byteorder_32bit(val)

#define _BE16_TO_CPU(val)	hspi_convert_byteorder_16bit(val)
#define _BE32_TO_CPU(val)	hspi_convert_byteorder_32bit(val)

/**********************************************************************************************/

static void hspi_opcode_print (hspi_opcode_t *opcode)
{
	_hspi_log("[ HSPI Operation Code ]\n");
	_hspi_log(" - value  : 0x%08X\n", opcode->val);
	_hspi_log(" - start  : 0x%02X\n", opcode->start);
	_hspi_log(" - burst  : %u\n", opcode->burst);
	_hspi_log(" - write  : %u\n", opcode->write);
	_hspi_log(" - fixed  : %u\n", opcode->fixed);
	_hspi_log(" - address: 0x%02X\n", opcode->address);

	if (opcode->write && !opcode->burst && opcode->fixed)
		_hspi_log(" - data	  : 0x%02X\n", opcode->length & 0xff);
	else
		_hspi_log(" - length	: %u\n", opcode->length);
}

static void hspi_transfer_setup (hspi_xfer_t *xfer, void *tx_buf, void *rx_buf, int len)
{
	memset(xfer, 0, sizeof(hspi_xfer_t));

	xfer->len = len;
	xfer->tx_buf = (char *)tx_buf;
	xfer->rx_buf = (char *)rx_buf;

//	Printf("\n< Start setup");
//	if(tx_buf) Printf("  TX (%p,%d) ", tx_buf, len);
//	if(rx_buf) Printf("  RX (%p, %d) ",rx_buf, len);
//	Printf(" Done />\n");
	
}

static int hspi_transfer (hspi_opcode_t *opcode, char *buf, int len)
{
	const int retry_max = HSPI_XFER_RETRY_MAX;
	hspi_xfer_t xfers[HSPI_XFER_NUM_MAX];
	hspi_xfer_t *p_xfers = xfers;
  	struct spi_trans_s trans;
  	struct spi_sequence_s seq;
        int fd; 
	hspi_cmd_t cmd;
	hspi_resp_t resp;
	int retry;
	int ret;
	int i;

	if (!HSPI_ACTIVE())
		return -1;

	cmd.opcode.val = _CPU_TO_BE32(opcode->val);
	cmd.crc = (hspi_crc7((char *)&cmd.opcode, sizeof(hspi_opcode_t)) << 1) | 0x01;

	hspi_transfer_setup(p_xfers++, &cmd, &resp, 8);

#if 0
	printf("opcode data\n"
	" address %d\n"
	" burst   %d\n"
	" fixed   %d\n"
	" length  %d\n"
	" write   %d\n", opcode->address, opcode->burst, opcode->fixed, opcode->length,opcode->write);
#endif

	if (opcode->burst)
	{
		uint32_t max_xfer_len = HSPI_XFER_LEN_MAX;
		uint32_t burst_crc;

		if (opcode->write)
		{
			for (i = 0 ; i < len ; i += max_xfer_len)
			{
				if ((len - i) >= max_xfer_len)
					hspi_transfer_setup(p_xfers++, buf + i, NULL, max_xfer_len);
				else
					hspi_transfer_setup(p_xfers++, buf + i, NULL, len - i);
			}

			burst_crc = ~0;
			hspi_transfer_setup(p_xfers++, &burst_crc, NULL, 4);
		}
		else
		{
			read_status = -3;
			for (i = 0 ; i < len ; i += max_xfer_len)
			{
				if ((len - i) >= max_xfer_len)
					hspi_transfer_setup(p_xfers++, NULL, buf + i, max_xfer_len);
				else
					hspi_transfer_setup(p_xfers++, NULL, buf + i, len - i);
			}

			burst_crc = 0;
			hspi_transfer_setup(p_xfers++, NULL, &burst_crc, 4);
		}
	}

	for (retry = 0 ; retry < retry_max ; retry++)
	{

	  seq.dev = SPIDEV_ID(22, 0);
  	  seq.mode = 0;
	  seq.nbits = 8;
	  seq.frequency = spi_clock_rate;
	  seq.ntrans = 1;
	  seq.trans = &trans;

	  trans.deselect = true;
	  trans.delay = 0;
	  trans.nwords = xfers[0].len;
	  trans.txbuffer = xfers[0].tx_buf;
	  trans.rxbuffer = xfers[0].rx_buf;
         
//	  fd = spidev_open(4);
	  ret = spidev_transfer(spi_fd, &seq);
	  read_status = -4;
	  if (ret != 0)
		break;
	

  	  if (resp.ack == HSPI_ACK_VALUE)
		{
			//printf("ACK\n");
			if (retry > 0)
				printf("hspi_transfer: retry=%d/%d\n", retry, retry_max);

			if (opcode->burst)
			{
				int n_xfers = p_xfers - xfers;
				for (i = 1 ; i < n_xfers ; i++)
				{
					trans.nwords = xfers[i].len;
					trans.txbuffer = xfers[i].tx_buf;
					trans.rxbuffer = xfers[i].rx_buf;

					ret = spidev_transfer(spi_fd, &seq);
					read_status = -5;
					if (ret != 0) break;
				}
			}
			else if (opcode->fixed && !opcode->write) // single read
				*buf = resp.data;

			break;
		}
		else {
			printf("No ACK\n");
		}
	}

	if (ret != 0 || retry >= retry_max)
	{
		_hspi_log("hspi_transfer: retry=%d/%d ret=%d\n", retry, retry_max, ret);
		hspi_opcode_print(opcode);
		ret = -1;
	}

//	print_time();
//	Printf("Transfer done\n");
	return ret;
}

/**********************************************************************************************/

static int hspi_reg_read (char addr, char *data, int len)
{
	if (!data || !len)
		return -1;

	if (HSPI_ACTIVE())
	{
		hspi_opcode_t opcode;

		opcode.val = HSPI_OPCODE_READ_REG(addr, len);

		return hspi_transfer(&opcode, data, len);
	}

	return -1;
}

static int hspi_reg_write (char addr, char data)
{
	if (HSPI_ACTIVE())
	{
		hspi_opcode_t opcode;

		opcode.val = HSPI_OPCODE_WRITE_REG(addr, data);

		return hspi_transfer(&opcode, NULL, -1);
	}

	return -1;
}

static int hspi_regs_read_sys (hspi_sys_t *sys)
{
	hspi_sys_t sys_tmp;
	int err;

	err = hspi_reg_read(HSPI_REG_SYS, (char *)&sys_tmp, sizeof(hspi_sys_t));
	if (!err)
	{
		sys->status.ready = sys_tmp.status.ready;
		sys->status.sleep = sys_tmp.status.sleep;

		sys->chip_id = _BE16_TO_CPU(sys_tmp.chip_id);
		sys->modem_id = _BE32_TO_CPU(sys_tmp.modem_id);
		sys->sw_id = _BE32_TO_CPU(sys_tmp.sw_id);
		sys->board_id = _BE32_TO_CPU(sys_tmp.board_id);
	}


	return err;
}

static int hspi_regs_read_eirq (hspi_eirq_t *eirq)
{
	return hspi_reg_read(HSPI_REG_EIRQ, (char *)eirq, sizeof(hspi_eirq_t));
}

int hspi_regs_read_status (hspi_status_t *status)
{
	hspi_status_t status_tmp;
	int err;

	err = hspi_reg_read(HSPI_REG_STATUS, (char *)&status_tmp, sizeof(hspi_status_t));
	if (!err)
	{
		status->eirq.txq = status_tmp.eirq.txq;
		status->eirq.rxq = status_tmp.eirq.rxq;
		status->eirq.ready = status_tmp.eirq.ready;
		status->eirq.sleep = status_tmp.eirq.sleep;

		status->txq.error = status_tmp.txq.error;
		status->txq.slot_cnt = status_tmp.txq.slot_cnt;
		status->txq.slot_size = _BE16_TO_CPU(status_tmp.txq.slot_size);
		status->txq.total_slot_size = _BE16_TO_CPU(status_tmp.txq.total_slot_size);

		status->rxq.error = status_tmp.rxq.error;
		status->rxq.slot_cnt = status_tmp.rxq.slot_cnt;
		status->rxq.slot_size = _BE16_TO_CPU(status_tmp.rxq.slot_size);
		status->rxq.total_slot_size = _BE16_TO_CPU(status_tmp.rxq.total_slot_size);
	}

	return err;
}

static int hspi_regs_read_message (hspi_msg_t msg)
{
	hspi_status_t status_tmp;
	hspi_msg_t msg_tmp;
	int err;

	err = hspi_reg_read(HSPI_REG_STATUS, (char *)&status_tmp, sizeof(hspi_status_t));
	err = hspi_reg_read(HSPI_REG_MSG, (char *)msg_tmp, sizeof(hspi_msg_t));
	if (!err)
	{
		msg[0] = _BE32_TO_CPU(msg_tmp[0]);
		msg[1] = _BE32_TO_CPU(msg_tmp[1]);
		msg[2] = _BE32_TO_CPU(msg_tmp[2]);
		msg[3] = _BE32_TO_CPU(msg_tmp[3]);
	}

	return err;
}

static int hspi_regs_read_all (hspi_regs_t *regs)
{
	int err = 0;

	err += hspi_regs_read_sys(&regs->sys);
	err += hspi_regs_read_eirq(&regs->eirq);
	err += hspi_regs_read_status(&regs->status);
	err += hspi_regs_read_message(regs->msg);
	if(err) {
		printf("Reading registers failed %d, %d\n", err, read_status);
	}
	printer("SYS", &regs->sys, sizeof(regs->sys));
	printer("EIRQ", &regs->eirq, sizeof(regs->eirq));
	printer("STAT", &regs->status, sizeof(regs->status));
	printer("MSG", &regs->msg, sizeof(regs->msg));
	
	return err;
}

static void hspi_regs_print_sys (hspi_sys_t *sys)
{
	printf("\r\n");
	printf("[ HSPI SYS Registers ]\n");
	printf(" - Status	  : Ready(%u), Sleep(%u)\n", sys->status.ready, sys->status.sleep);
	printf(" - Chip ID	 : %04X\n", sys->chip_id);
	printf(" - Modem ID	: %08X\n", sys->modem_id);
	printf(" - Software ID : %08X\n", sys->sw_id);
	printf(" - Board ID	: %08X\n", sys->board_id);
}

static void hspi_regs_print_eirq (hspi_eirq_t *eirq)
{
	printf("\r\n");
	printf("[ HSPI EIRQ Registers ]\n");
	printf(" - mode   : active(%s), trigger(%s), %s\n",
					eirq->mode.active ? "high" : "low",
					eirq->mode.trigger ? "edge" : "level",
					eirq->mode.io_enable ? "enable" : "disable");
	printf(" - enable : txq(%u), rxq(%u), ready(%u), sleep(%u)\n",
					eirq->enable.txq, eirq->enable.rxq,
					eirq->enable.ready, eirq->enable.sleep);
}

static void hspi_regs_print_status (hspi_status_t *status)
{
	printf("\r\n");
	printf("[ HSPI STATUS Registers ]\n");
	printf(" - latch : 0x%02X\n", status->latch);
	printf(" - eirq  : txque(%u), rxque(%u), ready(%u), sleep(%u)\n",
				status->eirq.txq, status->eirq.rxq,
				status->eirq.ready, status->eirq.sleep);
	printf(" - txq   : error(0x%02X), slot_cnt(%u), slot_size(%u/%u)\n",
				status->txq.error, status->txq.slot_cnt,
				status->txq.slot_size, status->txq.total_slot_size);
	printf(" - rxq   : error(0x%02X), slot_cnt(%u), slot_size(%u/%u)\n",
				status->rxq.error, 	status->rxq.slot_cnt,
				status->rxq.slot_size, status->rxq.total_slot_size);
}

static void hspi_regs_print_message (hspi_msg_t msg)
{
	_hspi_log("\r\n");
	_hspi_log("[ HSPI MESSAGE Registers ]\n");
	_hspi_log(" - message 0 : 0x%08X\n", msg[0]);
	_hspi_log(" - message 1 : 0x%08X\n", msg[1]);
	_hspi_log(" - message 2 : 0x%08X\n", msg[2]);
	_hspi_log(" - message 3 : 0x%08X\n", msg[3]);
}

static void hspi_regs_print_all (hspi_regs_t *regs)
{
	hspi_regs_print_sys(&regs->sys);
	hspi_regs_print_eirq(&regs->eirq);
	hspi_regs_print_status(&regs->status);
	hspi_regs_print_message(regs->msg);

	_hspi_log("\n");
}

static void hspi_status_init (int que)
{
	hspi_status_t *status = HSPI_QUEUE_STATUS();

	if (que == HSPI_TXQ || que == HSPI_QUE_ALL)
		memset(&status->txq, 0, sizeof(status->txq));

	if (que == HSPI_RXQ || que == HSPI_QUE_ALL)
		memset(&status->rxq, 0, sizeof(status->rxq));
}

static int hspi_status_update (void)
{
	hspi_status_t *old = HSPI_QUEUE_STATUS();
	hspi_status_t new;
	int ret;

	ret = hspi_regs_read_status(&new);
	if (ret != 0)
		return ret;

//	hspi_regs_print_status(old);
//	hspi_regs_print_status(&new);

	if (new.txq.slot_cnt > 0 && new.txq.slot_cnt <= HSPI_TXQ_SLOT_NUM() &&
			(new.txq.slot_size << 2) == HSPI_TXQ_SLOT_SIZE() &&
			(new.txq.slot_cnt * new.txq.slot_size) == new.txq.total_slot_size)
		memcpy(&old->txq, &new.txq, sizeof(old->txq));

	if (new.rxq.slot_cnt > 0 && new.rxq.slot_cnt <= HSPI_RXQ_SLOT_NUM() &&
			(new.rxq.slot_size << 2) == HSPI_RXQ_SLOT_SIZE() &&
			(new.rxq.slot_cnt * new.rxq.slot_size) == new.rxq.total_slot_size)
		memcpy(&old->rxq, &new.rxq, sizeof(old->rxq));

	return 0;
}

/**********************************************************************************************/

static int hspi_read_slot (int slot_num, int slot_size, char *buf, int *len)
{
	static uint8_t seq = 0;
	char slot_buf[HSPI_SLOT_SIZE_MAX];
	hspi_slot_t *slot = (hspi_slot_t *)slot_buf;
	hspi_opcode_t opcode;
	int i, j;

	Printf("<Slot Reader>\n");

	for (i = 0, j = 0 ; i < slot_num ; i++, j += slot->len)
	{
		opcode.val = HSPI_OPCODE_READ_DATA(HSPI_REG_TXQ_WINDOW, slot_size);

		if (hspi_transfer(&opcode, (char *)slot, slot_size) != 0)
			return -1;

		if (memcmp(slot->start, HSPI_SLOT_START, HSPI_SLOT_START_SIZE) != 0 ||
				slot->len > (slot_size - HSPI_SLOT_HDR_SIZE))
		{
			_hspi_log("hspi_read: invalid header, start=%c(%X),%c(%X) len=%u \n",
						slot->start[0], slot->start[0], slot->start[1], slot->start[1],
						slot->len);

			slot->len = 0;
			continue;
		}

		memcpy(buf + j, slot->data, slot->len);

		_hspi_read_debug("slot: seq=%u len=%u\n", slot->seq, slot->len);

		if (slot->seq != seq)
		{
			_hspi_read_debug("hspi_read: slot_seq: %u -> %u\n", seq, slot->seq);

			seq = slot->seq;
		}

		if (++seq > HSPI_SLOT_SEQ_MAX)
			seq = 0;
	}

	*len = j;

	return i;
}



static int hspi_read (char *buf, int len)
{
	uint16_t slot_size;
	uint8_t slot_cnt;
	uint16_t slot_num;
	int ret;

	if (!buf || !len)
		return -1;

	if (!HSPI_ACTIVE())
		return -1;

	Printf("<%lf> hspi reader @%p and length %d\n", get_system_time(), buf, len);
	slot_cnt = HSPI_TXQ_SLOT_COUNT();
	slot_size = HSPI_TXQ_SLOT_SIZE();
	slot_num = len / (slot_size - HSPI_SLOT_HDR_SIZE);

	if (slot_num > HSPI_TXQ_SLOT_NUM())
		slot_num = HSPI_TXQ_SLOT_NUM();

	if (slot_num > slot_cnt)
	{
		if (hspi_status_update() != 0)
		{
			_hspi_read_debug("status update fail\n");
			return -1;
		}

		slot_cnt = HSPI_TXQ_SLOT_COUNT();

		if (slot_num > slot_cnt)
			slot_num = slot_cnt;
	}

	//_hspi_read_debug("slot_cnt=%u slot_size=%d slot_num=%u len=%d\n", slot_cnt, slot_size, slot_num, len);

	if (slot_num == 0) {
		Printf("SlotNum=0?\n");
		return 0;
	}

	
	read_status = 100;

	ret = hspi_read_slot(slot_num, slot_size, buf, &len);
	if (ret < 0) {
		hspi_status_init(HSPI_TXQ);
		read_status = -2;
	}
	else
	{
		slot_num = ret;
		ret = len;

		if (slot_num > 0)
		{
			slot_cnt -= slot_num;
			HSPI_TXQ_SLOT_COUNT_UPDATE(slot_cnt);
		}
	}

	_hspi_read_debug("RX slot_cnt=%u slot_num=%u ret=%d\n", slot_cnt, slot_num, ret);

	return ret;
}

static int hspi_write_slot (int slot_num, int slot_size, char *buf, int *len)
{
	static uint8_t seq = 0;
	char slot_buf[HSPI_SLOT_SIZE_MAX];
	hspi_slot_t *slot = (hspi_slot_t *)slot_buf;
	hspi_opcode_t opcode;
	int i, j;

	memcpy(slot->start, HSPI_SLOT_START, HSPI_SLOT_START_SIZE);

	slot->len = slot_size - HSPI_SLOT_HDR_SIZE;

	for (i = 0, j = 0 ; i < slot_num ; i++, j += slot->len)
	{
		if ((*len - j) < slot->len)
		{
			slot->len = *len - j;

			memset(slot_buf + HSPI_SLOT_HDR_SIZE + slot->len, 0, slot_size - HSPI_SLOT_HDR_SIZE - slot->len);
		}

		slot->seq = seq;

		memcpy(slot->data, buf + j, slot->len);

		_hspi_write_debug("slot: seq=%u len=%u\n", slot->seq, slot->len);

		opcode.val = HSPI_OPCODE_WRITE_DATA(HSPI_REG_RXQ_WINDOW, slot_size);

		if (hspi_transfer(&opcode, (char *)slot, slot_size) != 0)
			break;

		if (++seq > HSPI_SLOT_SEQ_MAX)
			seq = 0;
	}

	*len = j;

	return i;
}

static int hspi_write (char *buf, int len)
{
	uint8_t slot_cnt;
	uint16_t slot_size;
	uint16_t slot_num;
	int ret;

	if (!buf || !len)
		return -1;

	if (!HSPI_ACTIVE())
		return -1;

	slot_cnt = HSPI_RXQ_SLOT_COUNT();
	slot_size = HSPI_RXQ_SLOT_SIZE();
	slot_num = (len + (slot_size - HSPI_SLOT_HDR_SIZE - 1)) / (slot_size - HSPI_SLOT_HDR_SIZE);

	if (slot_num > slot_cnt)
	{
		if (hspi_status_update() != 0)
		{
			_hspi_write_debug("status update fail\n");
			return -1;
		}

		slot_cnt = HSPI_RXQ_SLOT_COUNT();
	}

	if (slot_num <= slot_cnt)
	{
		_hspi_write_debug("slot_cnt=%u slot_num=%u len=%d\n", slot_cnt, slot_num, len);
	}
	else
	{
		int _len = slot_cnt * (slot_size - HSPI_SLOT_HDR_SIZE);

		_hspi_write_debug("slot_cnt=%u slot_num=%u->%u len=%d->%d\n",
							slot_cnt, slot_num, slot_cnt, len, _len);

		slot_num = slot_cnt;
		len = _len;
	}

	ret = hspi_write_slot(slot_num, slot_size, buf, &len);
	if (ret < slot_num)
	{
		slot_num = ret;
		slot_cnt = 0;
		hspi_status_init(HSPI_RXQ);
	}
	else
	{
		slot_cnt -= slot_num;
		HSPI_RXQ_SLOT_COUNT_UPDATE(slot_cnt);
	}

	ret = len;

	_hspi_write_debug("slot_cnt=%u slot_num=%u ret=%d\n", slot_cnt, slot_num, ret);

	return ret;
}

/**********************************************************************************************/

static int hspi_ready (hspi_info_t *info)
{
	hspi_regs_t regs;

	if (!HSPI_ACTIVE())
		return -1;

	if (hspi_regs_read_all(&regs) != 0)
		return -1;

	hspi_regs_print_all(&regs);

	if (memcmp(regs.msg, "NRC-HSPI", 8) == 0)
	{
		uint16_t slot_num;
		uint16_t slot_size;
		int que;

		for (que = HSPI_TXQ ; que <= HSPI_RXQ ; que++)
		{
			slot_num = (regs.msg[2 + que] >> 16) & 0xffff;
			slot_size = regs.msg[2 + que] & 0xffff;

			if (slot_num == 0)
				return -1;

			if (slot_size == 0 || slot_size > HSPI_SLOT_SIZE_MAX)
				return -1;

			info->queue.slot[que].num = slot_num;
			info->queue.slot[que].size = slot_size;
			nrc_attached = 1;
		}
#if 0
		printf("[ HSPI_SLOT ]\n");
		printf(" - hdr: %d-byte\n", HSPI_SLOT_HDR_SIZE);
		printf(" - txq: (%u x %u)-byte\n",
							info->queue.slot[HSPI_TXQ].num,
							info->queue.slot[HSPI_TXQ].size);
		printf(" - rxq: (%u x %u)-byte\n",
							info->queue.slot[HSPI_RXQ].num,
							info->queue.slot[HSPI_RXQ].size);
		printf("\r\n");
#endif
		return 0;
	}

	printf("No ATCMD HSPI firmware\n");
	nrc_attached = 0;
	return -1;
}

void chk_hspi_ready_update() {
	if( hspi_ready(&g_hspi_info) == -1) {
		printf("HSPI not ready\n");
		return 0;
	}
	if( hspi_status_update() != 0) {
		printf("HSPI status update failed\n");
		return 0;
	}
}


static int hspi_open ()
{
	memset(&g_hspi_info, 0, sizeof(hspi_info_t));
	//memcpy(&g_hspi_info.ops, ops, sizeof(hspi_ops_t));

	g_hspi_info.active = ~0;
	//sleep(1);
	if (hspi_ready(&g_hspi_info) != 0 || hspi_status_update() != 0)
	{
		//memset(&g_hspi_info, 0, sizeof(hspi_info_t));
		// return -1;
		printf("Error getting HSPI status\n");
	}

	return 0;
}

static void hspi_close (void)
{
	memset(&g_hspi_info, 0, sizeof(hspi_info_t));
}


int hspi_eirq_enable (int mode, int enable)
{
	hspi_eirq_t eirq;


	if( (flags & FLAGS_IRQ_ENABLE) == 0) enable = 0;

/*	_hspi_log("mode(0x%X), enable(0x%X)\n", mode & 0x3, enable & 0xf); */

	eirq.mode.active = !!(mode & HSPI_EIRQ_HIGH);
	eirq.mode.trigger = !!(mode & HSPI_EIRQ_EDGE);
	eirq.mode.io_enable = 1;

	eirq.enable.txq = !!(enable & HSPI_EIRQ_TXQ);
	eirq.enable.rxq = !!(enable & HSPI_EIRQ_RXQ);
	eirq.enable.ready = !!(enable & HSPI_EIRQ_READY);
	eirq.enable.sleep = !!(enable & HSPI_EIRQ_SLEEP);

	if (hspi_reg_write(HSPI_REG_EIRQ_MODE, eirq.val[0]) != 0)
		return -1;

	if (hspi_reg_write(HSPI_REG_EIRQ_ENABLE, eirq.val[1]) != 0)
		return -1;

#ifdef CONFIG_HSPI_REG_PRINT
	memset(&eirq, 0, sizeof(eirq));

	if (hspi_regs_read_eirq(&eirq) == 0)
		hspi_regs_print_eirq(&eirq);
#endif	
	Printf("HSPI-EIRQ:%d\n", enable);
	return 0;
}

/**********************************************************************************************/

int nrc_hspi_open ()
{
	return hspi_open();
}

void nrc_hspi_close (void)
{
	hspi_close();
}

int nrc_hspi_read (char *buf, int len)
{
	return hspi_read(buf, len);
}

int nrc_hspi_write (char *buf, int len)
{
	return hspi_write(buf, len);
}

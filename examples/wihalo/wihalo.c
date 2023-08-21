#include <stdio.h>
#include <nuttx/config.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <nuttx/spi/spi.h>
#include <nuttx/spi/spi_transfer.h>
#include <semaphore.h>
#include <arch/board/board.h>
#include <arch/chip/pin.h>

#include "nrc-hspi.h"
#include "cli.h"
#include "nrc-atcmd.h"
#include "util.h"
#include "wihalo.h"



// Macros
#define SET_FLAG_BIT(bno,b)             flags = (flags & (~(1<<bno)) ) | ( (!(!b)) <<bno)

// Functions
static int spi_open (void);
static void spi_unlock (void);
static void spi_lock (void);
static int spi_close(void);
static void *spi_recv_thread (void *arg);
static void *monitor_thread (void *arg);
static void *service_thread(void *arg);

int hif_read (char *buf, int len);
int hif_write (char *buf, int len);
// Data
static pthread_mutex_t spi_mutex;
int spi_fd = -1;
static pthread_t rcv_threadid;
static pthread_t monitor_threadid;
static pthread_t service_threadid;
static pthread_t iperf_threadid;
int iperf_started;
static uint8_t quit;

static uint32_t cntr;
uint32_t spi_clock_rate = 16000000;

// Global variables
int wh_atdelay = 0;
uint8_t enable_debug;
int at_timeout = 3;
int wait_response;
sem_t rcv_sem;

uint32_t flags = FLAGS_DEFAULT;
int last_sck_id;
uint8_t nrc_connected, nrc_attached;
uint8_t show_atcmd_output, iperf_abort;

int setup_access_point(void);
int setup_station(void);
int board_gpio_int(uint32_t pin, bool enable);
void read_configuration(void);
uint8_t use_as_service;


#define READY_PIN   31
extern const char *BuildInformation;
extern const char *VersionString;



int cli_main_help(void), cli_main_exit(void), cli_main_regs(void), cli_main_at(void);
int cli_main_atdelay(void), cli_main_debug(void), cli_main_startap(void),cli_main_startsta(void);
int cli_main_connect(void), cli_main_iperf(void),cli_main_iperfend(void), cli_main_mmr(void) ;
int cli_main_cfg(void), cli_main_spi(void), cli_main_listen(void), cli_main_test(void), cli_main_save(void);
int cli_main_stats(void), cli_main_show(void);


CLI_TYPE main_cli_list[] = {
    {"help", "Shows help information", cli_main_help},
    {"exit", "Terminate the NRC test utility", cli_main_exit},
    {"regs", "Read registers and update status", cli_main_regs},
    {"at", "Sends an AT command and receives the response (blocking)", cli_main_at},
    {"atdelay", "Delay for ATCMD in microseconds\n", cli_main_atdelay},
    {"debug", "Enable/Disable debug output", cli_main_debug},
    {"startap", "Starts the access point with default parameters", cli_main_startap},
    {"startsta", "Starts the station with pre-defined parameters", cli_main_startsta},
    {"spi", "Sets SPI communications (clock_rate)", cli_main_spi},
    {"cfg", "Sets configuration parameters", cli_main_cfg},
    {"listen", "listen udp/tcp port", cli_main_listen},
    {"test", "test sck_id length (num-times)", cli_main_test},
    {"connect", "connect tcp/udp (remote-ip remote-port)", cli_main_connect},
    {"iperf","Throughput performance test utility", cli_main_iperf},
    {"iperfend","Terminates iperf procedures", cli_main_iperfend},
    {"mmr","MMR", cli_main_mmr},
    {"save", "save the current configuration to /mnt/spif/wihalo.cfg", cli_main_save },
    {"stats", "show and clear stats", cli_main_stats },
    {NULL}
};



int cli_set_ssid(void), cli_set_frequency(void), cli_set_country(void),cli_set_ip(void), cli_set_flags(void),
    cli_set_dhcp(void), cli_set_security(void), cli_set_show(void), cli_set_bit(void), cli_set_password(void);

CLI_TYPE set_cli_list[] = {
    {"ssid", "Sets SSID", cli_set_ssid},
    {"frequency", "Sets Frequency", cli_set_frequency},
    {"country", "Sets country of operation", cli_set_country},
    {"ip", "Configures IP addresses", cli_set_ip},
    {"dhcp", "Enables/Disables DHCP", cli_set_dhcp},
    {"security", "Sets security configuration\n", cli_set_security},
    {"password", "Sets security password\n", cli_set_password},
    {"show", "View configuration", cli_set_show},
    {"bit", "Configures functionality", cli_set_bit},
    {"flags", "Configures flags bitmap value", cli_set_flags},
    
    {NULL},
};


void main_exit(void) {
    quit = 1;
    printf("Terminating..\n");
    spi_close();
    init_wihalo_threads();
    printf("Done\n");
}

void writeout(char c) {
    fputc(c,stdout);
    fflush(stdout);
}
char cmd_cp[256];

char* readstdin(char *buf, int max) {
    int nb = 0;
    buf[0] = 0;
    while(1) {
        char c = fgetc(stdin);
        if(c=='\r' || (c=='\n')) {
            //puts("\n");
            buf[nb] = 0;
            if(nb>0) return buf;
            return NULL;
        }
        if(c == '/') {
            int l = strlen(cmd_cp);
            if(l && (nb==0) ) {
                memcpy(buf, cmd_cp, strlen(cmd_cp)+1);
                return buf;
            }
        }
        writeout(c);
        buf[nb] = c;
        if(nb < (max-1)) nb++;
    }
}

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PIN_LED0 PIN_I2S1_BCK
#define PIN_LED1 PIN_I2S1_LRCK
#define PIN_LED2 PIN_I2S1_DATA_IN
#define PIN_LED3 PIN_I2S1_DATA_OUT

#define SETUP_PIN_OUTPUT(pin) do{ \
  board_gpio_write(pin, -1); \
  board_gpio_config(pin, 0, false, true, PIN_FLOAT); \
}while(0)


/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: init_leds()
 *
 * Description:
 *   Set up pins of LEDs on the Spresense main board.
 ****************************************************************************/

void init_leds(void)
{
  SETUP_PIN_OUTPUT( PIN_LED0 );
  SETUP_PIN_OUTPUT( PIN_LED1 );
}

int icnt;

static int spi_handler(int irq, FAR void *context, FAR void *arg)
{
    static int x = 0;
    board_gpio_write(PIN_LED1,x);
    x=!x;

    sem_post(&rcv_sem);

    icnt++;
    return OK;
}


/* int wihalo_main(int argc, FAR char *argv[]) */
int main(int argc, FAR char *argv[])
{
    int app_main(int argc, FAR char *argv[]);

#if !WH_APP_MODE    
    return (main_wihalo_main(NULL));
#else
    return (app_main(0,NULL));
#endif

}


void init_wihalo_threads(void) {
    use_as_service = 0;

    if(rcv_threadid) pthread_kill(rcv_threadid, 1);
    if(monitor_threadid) pthread_kill(monitor_threadid, 1);
    if(service_threadid) pthread_kill(service_threadid, 1);
    if(iperf_threadid) pthread_kill(iperf_threadid, 1);


    sleep(1);

    rcv_threadid = 0;
    monitor_threadid = 0;
    service_threadid = 0;
    iperf_threadid = 0;

    
}

int main_wihalo_main(char *argv) {

    init_wihalo_threads();

    printf("Starting WiFi HALO service / utility\n[%s]\n[%s]\n",BuildInformation, VersionString);
    
    read_configuration();
    quit = 0;

    if(spi_open() < 0) {
        printf("Error opening SPI device / HSPI\n");
        return -1;
    }
    
    if(strcmp(argv,"service") == 0) {
        SET_FLAG_BIT(FLAGS_AUTO_START_BIT, 0);
        use_as_service = 1; 
    }
    init_leds();

    board_gpio_config(READY_PIN, 0, 1, 1, 1);
    board_gpio_intconfig(READY_PIN, INT_RISING_EDGE, 0, spi_handler);
    board_gpio_int(READY_PIN, 1);

    nrc_atcmd_log_on();
    wait_response = 1;
    sem_init(&rcv_sem, 0,0);
    pthread_create(&rcv_threadid,NULL, spi_recv_thread, NULL);
    pthread_create(&monitor_threadid,NULL, monitor_thread, NULL);
    

    if(use_as_service) {
        // pthread_create(&service_threadid,NULL, service_thread, NULL);
        WHINFO("WiHalo service initialized\n");
        return;
    }
    
    while(1) {
        int slen;
        memset(cli_in, 0, CLI_BUFSZ);
	    if ( readstdin(cli_in, CLI_BUFSZ) == NULL) {
            puts("\n");
            continue;
        } 
	    slen = strlen(cli_in);
	    strtok((char*) "n", (char*) "n");
        puts("\n");

        strcpy(cmd_cp, cli_in);
	    if(CliProcess(main_cli_list, cli_in, 0) == -1)  {
            strupr(cmd_cp);
            if(memcmp(cmd_cp, "AT", 2) == 0) {
                show_atcmd_output = 1; 
                nrc_atcmd_send_cmd(cmd_cp);
                show_atcmd_output = 0;
            }
        }

        if(quit) break;
    }

    main_exit();
    return 0;
}



static int spi_open ()
{
    int fd;
	if( (fd = open("/dev/spi4",O_RDWR)) < 0) {
        printf("Error opening SPI device\n");
        return -1;
    }
    sleep(1);

	spi_fd = fd;
    if( nrc_hspi_open() == 0) {
        pthread_mutex_init(&spi_mutex, NULL);
        return 0;
    }
	return -1;  
}


static int spi_close ()
{
    nrc_hspi_close();
    if(spi_fd) close(spi_fd);
	return 0;  
}

static void spi_lock (void)
{
	if(pthread_mutex_lock(&spi_mutex) != 0) {
        printf("Mutex lock error\n");
        exit(1);
    }
}

static void spi_unlock (void)
{
	if(pthread_mutex_unlock(&spi_mutex) != 0) {
        printf("Mutex unlock error\n");
        exit(1);
    }
}

int cli_main_help(void){
    CliProcessHelp(main_cli_list);
    return 0;
}

int cli_main_exit(void) {
    quit = 1;
    return 0;
}

int cli_main_regs(void) {
    chk_hspi_ready_update();
    return 0;
}

int cli_main_mmr(void) {
    uint32_t _u32;
    char *t = CliGetNextToken();
    if(t && (sscanf(t,"%x",&_u32)==1)) {
        printf("Value[0x%x]=0x%x\n", _u32, *(uint32_t*)_u32);
    }
    return 0;
}


int cli_main_flags(void) {
    uint32_t _u32;
    char *t = CliGetNextToken();
    if(t && (sscanf(t,"%x",&_u32)==1)) {
        printf("Flags is now 0x%x\n", _u32);
        flags = _u32;
    }
    return 0;
}

int cli_main_save(void) {
    save_configuration();
    return 0;
}

int cli_main_show(void) {
    show_configuration();
    return 0;
}

int cli_main_at() {
    #if 1
    char *atcmd = CliGetNextToken();
    if(atcmd) {
        char *t = cli_get_stream();
        if(t) {
            printf("Stream:%s\n", t);
        }
        strupr(atcmd);
        if(memcmp(atcmd,"AT",2) == 0) { 
            int slen;
            slen = strlen(atcmd);
            show_atcmd_output = 1; putchar('<');
            nrc_atcmd_send_cmd(atcmd);
            show_atcmd_output = 0;
            putchar('>');

            return 0;
        }
        else {
            printf("%s is not an AT command\n", atcmd);
            return 0;
        }
    }
    
    show_atcmd_output = 1;
    nrc_atcmd_send_cmd("AT");
    show_atcmd_output = 0;
    #endif

    return 0;
}

int cli_main_startap()
{
    setup_access_point();
}

int cli_main_startsta()
{
    setup_station();
}

int cli_main_cfg(void)
{
    if(CliProcess(set_cli_list, cli_tok, 1) == -1)  {
        return 1;
    }
    return 0;
}


int cli_main_spi(void)
{
    uint32_t x;
    char *tokn = CliGetNextToken();
    if(tokn) {
        if(sscanf(tokn,"%ld", &x) == 1) {
            spi_clock_rate = x;
            printf("SPI clock rate set to %ld Hz\n", spi_clock_rate);
        }
        // tokn = CliGetNextToken();
        // if(tokn) {
        //     if(sscanf(tokn,"%ld", &x) == 1) {
        //         irq_enable = !(!x);
        //         printf("SPI IRQ mode: %d\n", irq_enable);
        //         return 1;
        //     }

        // }
        return 1;
    }
    return 0;
}

int cli_main_atdelay(void)
{
    char *s = CliGetNextToken();
    if(s) {
        if(sscanf(s, "%ld", &wh_atdelay) != 1) {
            printf("Specify value between 0 1000000 microseconds\n");
            return 1;
        }
        if( (wh_atdelay < 0) || (wh_atdelay > 1000000)) {
            wh_atdelay = 500000;
        }
        printf("ATCMD delay set to %ld microseconds\n", wh_atdelay);
    }
}

cli_main_debug(void) {
    char *s = CliGetNextToken();
    if(s) {
        int temp;
        if(sscanf(s, "%d", &temp) != 1) {
            printf("Bad option, should be 1 or 0\n");
            return 1;
        }
        enable_debug = !(!temp);
        printf("Debug is now\n", (int) enable_debug);
    }
}
int hspi_eirq_enable (int mode, int enable);
int hspi_regs_read_status (hspi_status_t *status);

uint8_t rcv_enable;


void receive_enable() {
    rcv_enable = 1;
}


void receive_disable() {
    rcv_enable = 0;
}

int rcv_cnt;
int idle_cnt;

#define MEM_RX_ALLOC 1024
#define MEM_RX_BLOCKS 128
#define MEM_RX_BUFSIZE (MEM_RX_ALLOC*MEM_RX_BLOCKS)
static char rxbuf[MEM_RX_BUFSIZE];


static void *spi_recv_thread (void *arg)
{
	int ret;
    hspi_eirq_enable ( HSPI_EIRQ_EDGE, HSPI_EIRQ_ALL);
    receive_enable();

	while (1)
	{
        if(flags & FLAGS_IRQ_ENABLE) {
            sem_wait(&rcv_sem);
            rcv_cnt++;
        }

        if(quit) break;

		ret = nrc_hif_read(rxbuf, MEM_RX_BUFSIZE);
		if (ret > 0) {
            continue;
        }
		else if (ret < 0 && ret != -EAGAIN) 
			printf("hif_read(), %s\n", strerror(-ret));

        if((FLAGS_IRQ_ENABLE & flags) == 0) usleep(5);
        cntr++;
        idle_cnt++;

	}
    WHLOG("Exit SPI receive thread\n");
	return NULL;
}

#define resource_lock() if(pthread_mutex_trylock(&spi_mutex)!=0) { \
                            WHLOG("Resource busy in %s, %s\n", __func__, buf); return -1; }

int nrc_hif_read (char *buf, int len)
{
    int ret;
    
    //resource_lock();
    pthread_mutex_lock(&spi_mutex);
	ret = nrc_hspi_read(buf, len);
    if(ret > 0) {
        nrc_atcmd_recv(buf, ret);
    }
	pthread_mutex_unlock(&spi_mutex);
    return ret;
}

int nrc_hif_write (char *buf, int len)
{
    int ret;
    //resource_lock();
    pthread_mutex_lock(&spi_mutex);
	ret = nrc_hspi_write(buf, len);
	pthread_mutex_unlock(&spi_mutex);
	return ret;
}

pthread_mutex_t logsem = PTHREAD_MUTEX_INITIALIZER;

void printer(char *msg, char *b, int len) {
	
	pthread_mutex_lock(&logsem);

	printf("%s\n", msg);

	for (int i = 0; i < len; i += 16) {
		int k;
		int n = (len - i > 16) ? 16 : len - i;
		for (k = 0; k < n; k++) {
			printf("%02hhx ",b[i+k]);
		}
		if (k < 16) {
			for (; k < 16; k++) printf("   ");
		}
		printf(" | ");
		for (int k = 0; k < n; k++) {
			char c = b[i + k];
			if ( (c < 0x20) || (c >= 0x7e)) putchar('.');
			else putchar(c);
		}
		printf("\n");
	}
	printf("\n");
	pthread_mutex_unlock(&logsem);
}


typedef struct halo_params_t_ {
    char        ssid[32];
    float       frequency;
    char        country_code[3];
    char        security[16];
    char        password[64];
    char        ipaddress[20];
    char        netmask[20];
    char        gateway[20];
    int         dhcp_enable;
    int         security_enable;
} halo_params_t;

halo_params_t haloconfig = {"wihaloap", 918.5,"US", "open","passwordhalo","192.168.1.1","255.255.255.0", "192.168.1.1",1,0};


int cli_set_ssid(void)
{
    char *tokn = CliGetNextToken();
    if(tokn) wh_set_ssid(tokn);
    return 1;
}

int cli_set_security(void)
{
    char *tokn = CliGetNextToken();
    if(tokn) wh_set_security(tokn);
    else {
        haloconfig.security_enable = 0;
        printf("Security disabled\n");
    }
    return 1;
}

int cli_set_password(void)
{
    char *tokn = CliGetNextToken();
    if(tokn) wh_set_password(tokn);
    return 1;
}
#include <inttypes.h>

int cli_set_flags(void)
{
    char *tokn = CliGetNextToken();
    if(tokn) {
        uint32_t _flags;
        if( sscanf(tokn,"%lx", &_flags) != 1) {
            printf("Improper input\n");
        } 
        else {
            flags = _flags;
            printf("Flags is now %lx\n", flags);
            return 1;
        }
    }
    return 0;    
}

int cli_set_show(void)
{
    printf("Current Configuration\n");
    printf("%-10s : %u\n", "SPI clock", spi_clock_rate);
    
    printf("%-10s : %s\n", "Country", haloconfig.country_code);
    printf("%-10s : %s\n", "DHCP", haloconfig.dhcp_enable ? "ON" : "OFF");
    printf("%-10s : %s\n", "Security", haloconfig.security_enable ? haloconfig.security : "NONE");
    printf("%-10s : %s\n", "SSID", haloconfig.ssid);
    printf("%-10s : %s,%s,%s\n", "IP Params", haloconfig.ipaddress,haloconfig.netmask, haloconfig.gateway);
    printf("%-10s : %x\n", "Flags", flags);
    printf("%-10s : %f\n", "Channel", haloconfig.frequency);
    printf("%-10s : %d\n", "AT delay", wh_atdelay);
    return 1;   
}


int cli_set_frequency(void){
    float f;

    char *tokn = CliGetNextToken();
    if(tokn && (sscanf(tokn,"%f",&f)==1)) {
        wh_set_frequency(f);
    }
    return 1;
}

int cli_set_country(void)
{
    char *tokn = CliGetNextToken();
    if(tokn) wh_set_country(tokn);
    return 1;

}

int cli_set_ip(void)
{
    char *tokn1, *tokn2, *tokn3;

    tokn1 = CliGetNextToken();
    if(tokn1) {
        tokn2 = CliGetNextToken();
        if(tokn2) {
            tokn3 = CliGetNextToken();

            if( wh_set_ip_params(tokn1, tokn2, tokn3)) {
                return 1;
            }
        }
    }
    return 0;
}

int cli_set_bit(void) {
    char *tok;
    int bitno, bitvalue;

    tok = CliGetNextToken();
    if(tok && (sscanf(tok,"%d",&bitno)==1)) {
        tok = CliGetNextToken();
        if(tok && (sscanf(tok,"%d",&bitvalue)==1)) {
            SET_FLAG_BIT(bitno, bitvalue);
            printf("Flags is now %lx\n", flags);
            return 0;
        }
    }
    return 1;
}

int cli_set_dhcp(void)
{
    int  f;
    char *tokn = CliGetNextToken();
    if(tokn && (sscanf(tokn,"%d",&f)==1)) {
        wh_set_dhcp(f);
    }
    return 1;
}

int wh_set_ssid(char *ssid) {
    if(ssid) {
        strcpy(haloconfig.ssid,ssid);
        printf("Set SSID to %s\n", haloconfig.ssid);
        return 1;
    }
    return 0;
}

int wh_set_frequency(float freq) {
    if(freq) {
        haloconfig.frequency = freq;
        printf("Set frequency to %f Mhz\n", haloconfig.frequency);
        return 1;
    }
    return 0;
}


int wh_set_country(char *country) {
    if(country) {
        strcpy(haloconfig.country_code, country);
        printf("Set country code to %s\n", haloconfig.country_code);
        return 1;
    }
    return 0;
}

int wh_set_security(char *security)
{
    if(security) {
        strcpy(haloconfig.security, security);
        printf("Security is now: %s\n", haloconfig.security);
        if(strcmp(haloconfig.security,"open") == 0) haloconfig.security_enable = 0;
        else haloconfig.security_enable = 0;
        return 1;
    }
    return 0;

}

int wh_set_ip_params(char *ip, char *mask, char *gw) {
    if(ip && mask && gw) {
        strcpy(haloconfig.ipaddress, ip);
        strcpy(haloconfig.netmask, mask);
        strcpy(haloconfig.gateway, gw);
        printf("Set ip params to  to %s,%s,%s\n", haloconfig.ipaddress, haloconfig.netmask, haloconfig.gateway);
        return 1;
    }
    return 0;
}

int wh_set_dhcp(int dhcp) {
    haloconfig.dhcp_enable = !(!dhcp);
    printf("Set DHCP to %s\n", haloconfig.dhcp_enable ? "Enable" : "Disable");
    return 1;
}


int setup_access_point() {

    char atsetup[512];
    int res;
    int at_tm;
    int e = -1;

    at_tm = at_timeout;
    wh_set_loglevel(1);
    do {
        printf("Configuring NRC device\n");
        res = nrc_atcmd_send_cmd("AT+WCOUNTRY=\"%s\"", haloconfig.country_code);
        if(res) break;
        //res = nrc_atcmd_send_cmd("AT+WBSSMAXIDLE=10,3");
        //if(res) break;

        if(haloconfig.security_enable == 0) strcpy(haloconfig.security,"open"); 
        sprintf(atsetup, "AT+WSOFTAP=%f,\"%s\",\"%s\"", haloconfig.frequency,
                haloconfig.ssid,haloconfig.security);
        if(haloconfig.security_enable) sprintf(atsetup+strlen(atsetup),",\"%s\"",haloconfig.password); 
        res = nrc_atcmd_send_cmd(atsetup);
        if(res != 0) break;
        printf("Started the access point\n");

        sprintf(atsetup, "AT+WIPADDR=\"%s\",\"%s\",\"%s\"", haloconfig.ipaddress,haloconfig.netmask,
            haloconfig.gateway);
        res = nrc_atcmd_send_cmd(atsetup);
        if(res != 0) break;
        printf("Setting IP parameters\n");
        if(haloconfig.dhcp_enable) {
            sprintf(atsetup, "AT+WDHCPS");
            res = nrc_atcmd_send_cmd(atsetup);
            if(res != 0) break;
            printf("Started DHCP server\n");
        }

        nrc_atcmd_send_cmd("AT+WSOFTAP?");
        return 0;
    } while(0);

    printf("Error starting the access point\n");
    return e;
}

int setup_station() {

    char atsetup[512];
    int res;
    int e = -1;
    int at_tm = at_timeout;

    printf("Connecting..\n");
    wh_set_loglevel(1);
    do {
        at_timeout = 20;
        //res = nrc_atcmd_send_cmd("ATZ");
        //if(res) break;
        res = nrc_atcmd_send_cmd("AT+WCOUNTRY=\"%s\"", haloconfig.country_code);
        if(res) break;
        res = nrc_atcmd_send_cmd("AT+WDISCONN");
        if(res != 0) break;
        if(haloconfig.dhcp_enable) {
            res = nrc_atcmd_send_cmd("AT+WDHCP=1");
            if(res != 0) break;
        }

        res = nrc_atcmd_send_cmd("AT+WSCAN");
        if(res != 0) break;
        
        sprintf(atsetup, "AT+WCONN=\"%s\",\"%s\",\"%s\"",haloconfig.ssid,haloconfig.security,haloconfig.password);
        res = nrc_atcmd_send_cmd(atsetup);
        if(res != 0) break;
        res = nrc_atcmd_send_cmd("AT+WCONN?");
        if(res != 0) break;
        if(haloconfig.dhcp_enable) {
            res = nrc_atcmd_send_cmd("AT+WDHCP");
            if(res != 0) break;
        }
        e = 0;
        printf("Connected to the access point\n");
        return 0;
    } while(0);
    at_timeout = at_tm;
    printf("Error connecting to the access point\n");
    return e;
}

int wh_open_socket(WH_SOCK_TYPE type, uint16_t port, char *optional_remote_or_null)
{
    char cmd[128];
    int sck;

    if(optional_remote_or_null && type==WH_SOCK_TCP ) {
        sprintf(cmd, "AT+SOPEN=\"%s\",\"%s\",%u",type==WH_SOCK_TCP ? "TCP" : "UDP", optional_remote_or_null, port);
    }
    else {
        sprintf(cmd, "AT+SOPEN=\"%s\",%u",type==WH_SOCK_TCP ? "TCP" : "UDP", port);
    }

    if( nrc_atcmd_send_cmd(cmd) < 0) {
        last_sck_id = -1;
    }
    return last_sck_id;
}

int setup_listen(char *type, int port)
{
    char cmd [128];
    int res;
    WH_SOCK_TYPE wst;

    
    if(strncmp(type,"TCP",3) == 0) {
        wst = WH_SOCK_TCP;
    }
    else if ( strncmp(type,"UDP",3) == 0 ) {
        wst = WH_SOCK_UDP;
    }
    else return -1;
    res = wh_open_socket(wst,(uint16_t) port,NULL);
    if(res >= 0) return 0;
    return -1;
}


int cli_main_listen(void)
{
    int res;
    char scktype[16];
    int port;

    do {
        char *tokn = CliGetNextToken();

        if(!tokn || (sscanf(tokn,"%s",scktype)!=1)) break;
        tokn = CliGetNextToken();
        if(!tokn || (sscanf(tokn,"%d",&port)!=1)) break;
        setup_listen(strupr(scktype), port);
    } while(0);
    return 1;
}

static int setup_connection(char *socktype, char *remote, int port) {
    char cmd[128];
    int res;
    WH_SOCK_TYPE st;

    if(strncmp(socktype,"UDP",3) == 0) {
        st = WH_SOCK_UDP;
    }
    else if(strncmp(socktype,"TCP",3) == 0) {
        st = WH_SOCK_TCP;
    }
    else return 0;

    res = wh_open_socket(st,port,remote);
    return 1;
}

int cli_main_connect(void) {
    char rem_ip[64], type[16];
    int port;
    char *tokn;
    int pass = 0;

    tokn = CliGetNextToken();
    if(!tokn || (sscanf(tokn,"%s",type)!=1)) return 0;
    pass = 1;
    tokn = CliGetNextToken();
    if(tokn && (sscanf(tokn,"%s",rem_ip)==1)) {
        tokn = CliGetNextToken();
        if(!tokn || (sscanf(tokn,"%d",&port)!=1)) pass = 0;
    }
    if(pass) {
        if(setup_connection(strupr(type), rem_ip, port) != 1) {
            printf("Error opening socket\n");
            return 0;
        }
    }
    return 1;
}


int get_ok(void);
char test_char = 'A';

int cli_main_test(void)
{
    int len;
    int sck_id;
    int res;
    int times;
    char cmd[128];
    char data[2048];
    char rip[64];
    int port;
    int tcp = 0;

    char *tokn = CliGetNextToken();

    if(tokn) {
        tokn = strupr(tokn);
        if (strncmp(tokn,"TCP",3)==0) tcp = 1;
        else if (strncmp(tokn,"UDP",3)!=0) return 0;
    } 


    do {
        tokn = CliGetNextToken();
        if(tokn && (sscanf(tokn,"%d",&sck_id)==1)) {

            if(!tcp) {
                tokn = CliGetNextToken();
                if(!tokn || (sscanf(tokn,"%s",rip)!=1)) break;
                tokn = CliGetNextToken();
                if(!tokn || (sscanf(tokn,"%d",&port)!=1)) break;
            }


            tokn = CliGetNextToken();
            if(tokn && (sscanf(tokn,"%d",&len)==1)) {
                tokn = CliGetNextToken();
                if(tokn && (sscanf(tokn,"%d",&times)==1)) {
                    if(times > 2000) times = 2000;
                 }
                 else times = 1;

                if(len > 2047) len = 2047;
                if(len < 1) len = 1;


                WHLOG("Sending data, %d bytes, %d times\n", len, times);
                while(times) {
                    if(tcp) sprintf(cmd,"AT+SSEND=%d,%d",sck_id, len);
                    else sprintf(cmd,"AT+SSEND=%d,\"%s\",%d,%d",sck_id, rip, port, len);

                    wait_response = 0;
                    res = nrc_atcmd_send_cmd(cmd);
                    wait_response = 1;
                    if(res) break;
                    memset(data,test_char,len);
                    if(++test_char == 'Z') test_char = 'A';
                    nrc_atcmd_send_data(data,len);
                    times--;
                    if (get_ok()){
                        if(times) continue;
                    }
                    else {
                        printf("No/Bad response, aborting test\n");
                        break;
                    }
                }
                return 1;
            }        
        }
    } while(0);
    printf("Error sending data\n");
    return 1;
}

int cli_main_stats(void)
{
    printf("Recv cnt: %d\n", rcv_cnt);
    printf("Idle cnt: %d\n", idle_cnt);
    
    rcv_cnt = 0;
    idle_cnt = 0;

}



void* iperf_thread_fn(void *args) 
{
    uint32_t flags_s;
    iperf_started = 1;
    flags_s = flags;
    SET_FLAG_BIT(FLAGS_SHOW_DATA_BIT, 0);
    iperf_main(cli_in);
    flags = flags_s;
    printf("Iperf exit\n");
    iperf_started = 0;
}


int cli_main_iperf(void) 
{
    if(iperf_started) {
        printf("A session is already active\n");
        return 1;
    }
    cli_in[5] = ' ';
    iperf_started = 0;

    pthread_create(&iperf_threadid,NULL, iperf_thread_fn, NULL);
    // while(iperf_started == 0) sleep(1);

    return 1;
}

int cli_main_iperfend(void) 
{
    printf("Requesting termination of iperf thread\n");
    iperf_started = 0;
    return 1;
}

const char *CFGNAME = "/mnt/spif/wihalo.cfg";

int initialize_configuration(void)
{
    int res = 0;
    FILE *c = fopen(CFGNAME, "r");
    if(!c) return 0;
    res = fscanf(c,"%u", &spi_clock_rate);
    res += fscanf(c,"%u", &flags);
    res += fscanf(c,"%s", haloconfig.country_code);
    res += fscanf(c,"%f", &haloconfig.frequency);
    res += fscanf(c,"%s", haloconfig.ssid);
    res += fscanf(c,"%d", &wh_atdelay);

#if 0
    if(res != 6) {
        printf("Corrupted / empty configuration file\nPlease retry\n");
        fclose(c);
        exit(0);
    }
#endif
    fclose(c);

    return (res == 6) ? 1 : 0;
}

int save_configuration(void) {
    FILE *c = fopen(CFGNAME, "w");
    if(!c) {
        printf("Error saving configuration\n");
        return 0;
    }
    fprintf(c,"%d\n", spi_clock_rate);
    fprintf(c,"%u\n", flags);
    fprintf(c,"%s\n", haloconfig.country_code);
    fprintf(c,"%f\n", haloconfig.frequency);
    fprintf(c,"%s\n", haloconfig.ssid);
    fprintf(c,"%d\n", wh_atdelay);

    fclose(c);
    printf("saved default configuration\n");
    return 1;
}

void read_configuration(void)
{
    if( initialize_configuration() == 0) {
        // Set other defaults
        spi_clock_rate = 20000000;
        flags = FLAGS_DEFAULT;
        haloconfig.frequency = 924.5;
        haloconfig.dhcp_enable = 1;
        strcpy(haloconfig.country_code,"JP");
        strcpy(haloconfig.gateway,"192.168.1.1");
        strcpy(haloconfig.ipaddress,"192.168.1.1");
        strcpy(haloconfig.netmask,"255.255.255.0");
        strcpy(haloconfig.security,"open");
        strcpy(haloconfig.password,"password");
        strcpy(haloconfig.ssid,"wihalo-vizmo");
        haloconfig.security_enable = 0;
        wh_atdelay = 0;

        if( save_configuration() == 0) {
            printf("Error reading/writing configuration\n");
            exit(0);
        }
        initialize_configuration();
    }
    printf("Successfully read configuration\n");
}

static void *monitor_thread (void *arg) 
{
    int delay = 5;

    while(1) {
        sleep(delay);
        if(quit) break;

        if(flags & (FLAGS_AUTO_START)) {
            if(!nrc_attached) continue;
            show_atcmd_output = 0;
            nrc_atcmd_send_cmd("AT+WIPADDR?");
            if(nrc_connected) continue;
            if(flags & FLAGS_STN_AP) {
                printf("Starting access point\n");
                setup_access_point();
            }
            else {
                printf("Starting wihalo station\n");
                setup_station();
            }
        }
    }
    printf("Exit monitor thread\n");
    return NULL;
}



/* API implementation*/

int wh_initialize() {
    main_wihalo_main("service");
}

void wh_set_loglevel(int level) 
{
    return nrc_atcmd_send_cmd("AT+SRXLOGLEVEL=%d", level);
}

int wh_start(WH_WIFI_MODE mode) {

    if(mode == WH_ACCESS_POINT) return (setup_access_point());
    else return (setup_station());   
}

void wh_stop() {
    main_exit();
}


int wh_post_command(char *cmd) 
{
    strcpy(cli_in, cmd);

    strtok((char*) "n", (char*) "n");
    puts("\n");
    strcpy(cmd_cp, cli_in);
    if(CliProcess(main_cli_list, cli_in, 0) == -1)  {
        strupr(cmd_cp);
        if(memcmp(cmd_cp, "AT", 2) == 0) {
            show_atcmd_output = 1; 
            nrc_atcmd_send_cmd(cmd_cp);
            show_atcmd_output = 0;
        }
    }

}

void *service_thread(void *arg)
{
    printf("Starting service thread\n");
    while(1) {

        strtok((char*) "n", (char*) "n");
        puts("\n");
        printf("Executing %s\n", cli_in);
        strcpy(cmd_cp, cli_in);
        if(CliProcess(main_cli_list, cli_in, 0) == -1)  {
            strupr(cmd_cp);
            if(memcmp(cmd_cp, "AT", 2) == 0) {
                show_atcmd_output = 1; 
                nrc_atcmd_send_cmd(cmd_cp);
                show_atcmd_output = 0;
            }
        }
    }

    printf("Exit service thread\n");
}

int wh_set_mode(int sta_ap)
{
    if(sta_ap == 1) {
        SET_FLAG_BIT(FLAGS_STN_AP_BIT, 1);
    }
    else if(sta_ap == 0) {
        SET_FLAG_BIT(FLAGS_STN_AP_BIT, 0);
    }
    else return -1;
    return 0;
}

int wh_get_state()
{
    if(nrc_attached) {
        show_atcmd_output = 0;
        nrc_atcmd_send_cmd("AT+WIPADDR?");
        if(nrc_connected) return 1;
    }
    return 0;
}

int wh_socket_listen(WH_SOCK_TYPE type, uint16_t port)
{
    if( setup_listen( (type==WH_SOCK_TCP) ? "TCP" : "UDP", port) == 0) {
        return last_sck_id;
    }
    return -1;
}

int wh_close_socket(int sckid) 
{
    if(sckid >= 0)
        return (nrc_atcmd_send_cmd("AT+SCLOSE=%d", sckid));
    else 
        return (nrc_atcmd_send_cmd("AT+SCLOSE"));
}

void wh_reset() {
    WHLOG("Resetting modem..\n");
    nrc_atcmd_send_cmd("ATZ");
}

int wh_set_password(char *pw)
{
    if(pw) {
        strcpy(haloconfig.password, pw);
        WHLOG("Password is now %s\n", pw);
        return 0;
    }
    return -1;
}

socket_event_callback app_data_callback_fn;


void data_rxd_callback(atcmd_rxd_t *info, uint8_t *data) 
{
    char *rem;
    if(app_data_callback_fn) {
        rem = &info->remote_addr[1];
        info->remote_addr[strlen(info->remote_addr)-1] = 0;
        app_data_callback_fn(0, info->id,0,rem,info->remote_port,data,info->len,0);
    }
}

void event_rxd_callback(enum ATCMD_EVENT event, int argc, char *argv[]) 
{

	int id=-1;
	int err=0;

	switch (event)
	{
		case ATCMD_SEVENT_CONNECT:
            id = atoi(argv[0]);
			break;

		case ATCMD_SEVENT_CLOSE:
        case ATCMD_SEVENT_SEND_IDLE:
            id = atoi(argv[0]);
			break;

		case ATCMD_SEVENT_RECV_ERROR:
			if (argc != 2)	break;

			id = atoi(argv[0]);
			err = atoi(argv[1]);
			break;

		default:
			break;
	}

    if(app_data_callback_fn) {
        WHLOG("Callback with %d %d\n", id, event);
        if(app_data_callback_fn(1, id, event, NULL, 0, NULL, 0, err ) != 1) {
            // WHLOG("Unprocessed event [%d] on sck-id [%d], error=%d\n", event, id, err);
            if(err == -ENOTCONN) {
            }
        }
    }
}

void wh_register_callback(socket_event_callback  data_rxd)
{
	nrc_atcmd_register_callback(ATCMD_CB_RXD, (void*) data_rxd_callback);
	nrc_atcmd_register_callback(ATCMD_CB_EVENT, (void*) event_rxd_callback);
    
    app_data_callback_fn = data_rxd;
}

void wh_unregister_callback(void)
{
	nrc_atcmd_register_callback(ATCMD_CB_RXD, NULL);
    nrc_atcmd_register_callback(ATCMD_CB_EVENT, NULL);
    app_data_callback_fn = NULL;
}

int wh_send_tcp_passmode_data(uint8_t *data, int datalen)
{
    if( nrc_atcmd_send_data(data,datalen) != ATCMD_RET_OK) return -1;
    return 0;
}

int wh_send_tcp_data(int sckid, uint8_t *data, int datalen)
{
    char cmd[128];
    int res;

    if(flags & FLAGS_PASSMODE) {
   		return (wh_send_tcp_passmode_data(data, datalen));
    }

    res = nrc_atcmd_send_cmd("AT+SSEND=%d,%d",sckid, datalen);
    if(res) return -1;
    if( nrc_atcmd_send_data(data,datalen) != ATCMD_RET_OK) return -1;
    return 0;
}


int wh_send_udp_data(int sckid, char *remote, uint16_t port, uint8_t *data, int datalen)
{
    char cmd[128];
    int res;

    res = nrc_atcmd_send_cmd("AT+SSEND=%d,\"%s\",%u,%d",sckid, remote, port, datalen);
    if(res) return -1;
    if( nrc_atcmd_send_data(data,datalen) != ATCMD_RET_OK) return -1;
    return 0;
}

int wh_set_passmode(int ed)
{
    SET_FLAG_BIT(FLAGS_PASSMODE_BIT, !(!ed));
    WHLOG("Passthrough: %s\n", flags&FLAGS_PASSMODE ? "ON" : "OFF");
}


int wh_end_passmode()
{
    sleep(2);
    return ( nrc_atcmd_send_cmd("AT") );
}

int wh_start_passmode(int id)
{
    if(flags & FLAGS_PASSMODE)
        return ( nrc_atcmd_send_cmd("AT+SSEND=%d,0", id) );
    else return 0;
}

static char ips[3][20];


int set_ip_addresses(char *msg)
{
    char s[128];
    char *t;

    int i,n;
    n = strlen(msg);
    strncpy(s,msg+10,n);
    for(i=0;i<n;i++) {
        char ch = s[i];
        if(ch=='\"' || ch==',') ch = ' ';
        s[i] = ch;

    }

    if(sscanf(s,"%s %s %s\n", ips[0], ips[1], ips[2] ) != 3) return -1;
	return 0;
}


/* 
sf update_hw_version 0
*/

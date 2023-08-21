#include <nuttx/config.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include <sys/time.h>

#include <nuttx/video/video.h>
#include "jpeglib.h"

#include "wihalo.h"
#include "camera_bkgd.h"

#define DBUF_LEN	1400

#ifdef WH_APP_MODE

// temporary association for demos
extern uint8_t iperf_abort;
static uint8_t connected, exiting;
static pthread_t srv_thread_id;
static void *server_thread_fn(void *a);
static pthread_t client_thread_id;
static void *client_thread_fn(void *a);
static float sys_time();

static float  start_tv;
char server_ip[40];
uint32_t rx_total;
int cam_sck, srv_sck;
static uint8_t stop_app_thread;

#define IMGSIZE_W (320)
#define IMGSIZE_H (240)
#define PIX_BYTES (2)
#define IMG_BYTES  (IMGSIZE_W * IMGSIZE_H * PIX_BYTES)

struct camera_data_s
{
  uint8_t img[IMG_BYTES];
  char start_str[8]; /* C A M D A T A \0 */
  union {
    int jpgsz;
    char cjpgsz[sizeof(int)];
  }sz;
  int rem_size;
  int is_jpeg;
  uint8_t line[IMGSIZE_W * PIX_BYTES];
};
static struct camera_data_s __attribute__ ((aligned(64))) g_camdata;

#if 0
#define SEND_JPEG
#endif

#ifdef SEND_JPEG
#define START_STR "JPGDATA"
#define FMT_TYPE  V4L2_PIX_FMT_JPEG
#else
#define START_STR "CAMDATA"
#define FMT_TYPE  V4L2_PIX_FMT_RGB565
#endif

#define CAMSTATE_STARTSTR (0)
#define CAMSTATE_SIZE     (1)
#define CAMSTATE_BODY     (2)
static int g_cam_state;

static int g_camfd;

int cam_client_events(int evt_type, int sckid, int event, char *remote, uint16_t port, uint8_t *data, int len, int evt_id );
int server_ip_events(int evt_type, int sckid, int event, char *remote, uint16_t port, uint8_t *data, int len, int evt_id );
void server_thread(void *a);
pthread_t server_thread_id;

// Example application below

void start_camera_server(void);
void start_camera_client(void);
void start_echo_server(void);
void start_echo_client(void);


struct {
	#define MAX_ECHO_LEN	2048
	#define ECHO_PORT		129

	char		remote[32];
	uint16_t	len;
	uint16_t	port;
	uint8_t		data[MAX_ECHO_LEN];

} echo_data;
sem_t echo_sem, echo_resp;
int echo_sck;
static void *echo_client_thread_fn(void *a);
static void *echo_server_thread_fn(void *a);

static int echo_server_events(int evt_type, int sckid, int event, char *remote, uint16_t port, uint8_t *data, int len, int evt_id );
static int echo_client_events(int evt_type, int sckid, int event, char *remote, uint16_t port, uint8_t *data, int len, int evt_id );


static int camera_initialize()
{
  int fd;
  union {
    struct v4l2_requestbuffers req;
    struct v4l2_format fmt;
    struct v4l2_buffer buf;
    enum v4l2_buf_type type;
  } param;

  video_initialize("/dev/video");
  fd = open("/dev/video", 0);

  memset(&param.req, 0, sizeof(struct v4l2_requestbuffers));
  param.req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  param.req.memory = V4L2_MEMORY_USERPTR;
  param.req.count  = 1;
  param.req.mode   = V4L2_BUF_MODE_RING;
  ioctl(fd, VIDIOC_REQBUFS, (unsigned long)&param.req);

  memset(&param.fmt, 0, sizeof(struct v4l2_format));
  param.fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  param.fmt.fmt.pix.width       = IMGSIZE_W;
  param.fmt.fmt.pix.height      = IMGSIZE_H;
  param.fmt.fmt.pix.field       = V4L2_FIELD_ANY;
  param.fmt.fmt.pix.pixelformat = FMT_TYPE;
  ioctl(fd, VIDIOC_S_FMT, (unsigned long)&param.fmt);

  memset(&param.buf, 0, sizeof(struct v4l2_buffer));
  param.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  param.buf.memory = V4L2_MEMORY_USERPTR;
  param.buf.index = 0;
  param.buf.m.userptr = (unsigned long)g_camdata.img;
  param.buf.length = IMG_BYTES;
  ioctl(fd, VIDIOC_QBUF, (unsigned long)&param.buf);

  param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ioctl(fd, VIDIOC_STREAMON, (unsigned long)&param.type);

  return fd;
}

static int stream_camdata(int fd)
{
  int sz;
  int jpg_len;
  char *data = START_STR;
  struct v4l2_buffer buf;

	printf("Starting CAMERA streaming\n");
	if(wh_start_passmode(cam_sck) < 0) {
		printf("Cannot start streaming\n");
		return 0;
	}

  while (1)
    {
      memset(&buf, 0, sizeof(struct v4l2_buffer));
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;
      ioctl(fd, VIDIOC_DQBUF, (unsigned long)&buf);

		  wh_send_tcp_data(cam_sck, data, 8);
#ifdef SEND_JPEG
      g_camdata.sz.jpgsz = buf.bytesused;
      wh_send_tcp_data(cam_sck, &g_camdata.sz.cjpgsz, sizeof(int));
      g_camdata.rem_size = buf.bytesused;
#else
      g_camdata.rem_size = IMG_BYTES;
#endif

      while (g_camdata.rem_size)
        {
          sz = g_camdata.rem_size >= DBUF_LEN ? DBUF_LEN : g_camdata.rem_size;
		      wh_send_tcp_data(cam_sck, &g_camdata.img[IMG_BYTES - g_camdata.rem_size], sz);
          g_camdata.rem_size -= sz;
        }

      ioctl(fd, VIDIOC_QBUF, (unsigned long)&buf);
    }

	wh_end_passmode();
  return 1;
}

int app_main(int argc, FAR char *argv[])
{
	char cmd[256];
	int started = 0;

	exiting = 0;
	wh_initialize();
	wh_set_loglevel(1);
	wh_close_socket(-1);

	srv_sck = cam_sck = echo_sck = -1;

	//printf("STACK: %d\n", CONFIG_EXAMPLES_WHTEST_STACKSIZE);

	while (1) {
		int ch = getchar();
		if (ch != 9) continue; // Type <tab> to access simple command line
		printf("Options: a / s / r / C / A / S / E / e / Q / q / h\n");
		ch = getchar();
		puts("\n");
		fflush(stdout);
		if (ch == 'q') break;
		else if (ch == 'a') {
			if (started) {
				printf("Access point is active\n");
				continue;
			}
			wh_start(WH_ACCESS_POINT);
			if (wh_get_state() == 1)
				started = 1;
		}
		else if (ch == 's') {
			wh_reset();
			wh_start(WH_STATION);
			if (wh_get_state()) {
				started = 1;
			}
		}
		else if (ch == 'r') {
			wh_reset();
			started = 0;
		}
		else if (ch == 'C') {

			printf("Enter command : ");
			fflush(stdout);
			memset(cmd, 0, 256);
			if (readstdin(cmd, 256)) {
				puts("\n");
				wh_post_command(cmd);
			}
		}
		else if (ch == 'A') {
			start_camera_client();
		}
		else if (ch == 'S') {
			start_camera_server();
		}
		else if (ch == 'e') {
			start_echo_client();
		}
		else if (ch == 'E') {
			start_echo_server();
		}
		else if(ch == 'Q') {
			stop_app_thread = 1;
			sleep(1);
			printf("Done\n");
		}
		else if(ch == 'h') {
			printf("Supported comands\n");
			printf("a	- Start access point\n");
			printf("s	- Start station\n");
			printf("C	- Enter a wihalo lib command or ATCMD\n");
			printf("S	- Start a TCP stream server, port 128\n");
			printf("A	- Start a TCP stream client, port 128\n");
			printf("E	- Start a UDP echo server, port %d\n",ECHO_PORT);
			printf("e	- Start a UDP echo client, port %d\n", ECHO_PORT);
			printf("Q	- Terminate camera client / server application\n");
			printf("q	- Quit the wihalo application\n");
		}

		else {
			printf("No function associated with '%c'\n", ch);
		}

	}

	exiting = 1;
	if(srv_thread_id) {
		pthread_kill(srv_thread_id, 1);
	}
	wh_stop();
	printf("Exiting..\n");
	return 0;
}


struct jpg_context
{
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPARRAY buffer;
};
static struct jpg_context g_jpgctx;

static void decode_and_display(void)
{
  int line_pos;
  g_jpgctx.cinfo.err = jpeg_std_error(&g_jpgctx.jerr);
  jpeg_create_decompress(&g_jpgctx.cinfo);
  jpeg_mem_src(&g_jpgctx.cinfo, g_camdata.img, g_camdata.sz.jpgsz);

  jpeg_read_header(&g_jpgctx.cinfo, TRUE);
  printf("image (w,h)  = (%d,%d)\n",
        g_jpgctx.cinfo.image_width, g_jpgctx.cinfo.image_height);

  g_jpgctx.cinfo.out_color_space = JCS_CbYCrY;
  jpeg_start_decompress(&g_jpgctx.cinfo);

  printf("output_width should be the same as 320 : %d\n",
         g_jpgctx.cinfo.output_width);

  g_jpgctx.buffer = (JSAMPARRAY)&g_camdata.line; /* TODO: Is it OK non-use alloc_sarray ? */

  line_pos = 0;
  while (g_jpgctx.cinfo.output_scanline < g_jpgctx.cinfo.output_height)
    {
      /* Decode one line */
      jpeg_read_scanlines(&g_jpgctx.cinfo, g_jpgctx.buffer, 1 /* line */);

      /* Convert YUV422 to RGB565 */
      imageproc_convert_yuv2rgb(g_camdata.line, IMGSIZE_W, 1);

      /* Draw one line */
      nximage_image(g_camdata.line, line_pos, IMGSIZE_W, 1);
      line_pos += IMGSIZE_W;
    }

  jpeg_finish_decompress(&g_jpgctx.cinfo);
  jpeg_destroy_decompress(&g_jpgctx.cinfo);
}


void start_camera_client() {

  /* KOIZUMI:: Client is just received a data. So Client side is LCD display side */

  nximage_initialize(); /* Come from Camera example */

	if (wh_get_state() == 0) {
		printf("Access point is disconnected\n");
		return;
	}
	
	printf("Enter Camera IP address: "); fflush(stdout);
	if(readstdin(server_ip, 40) == NULL ) return NULL;
	puts("\n");
	stop_app_thread = 0;
	if(pthread_create(&client_thread_id, NULL, client_thread_fn, NULL) != 0) {
		printf("Error starting server thread\n");
	}
	return;
}

float sys_time() {
	float f;
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME,&tv);
	f = tv.tv_sec + tv.tv_nsec / 1e9;
	return f;
}
	
void report_client() {

	float x;

	x = sys_time();
	x = x - start_tv;
	x = (rx_total*8)/x; 

	printf("Total: %d bytes / Data Rate: %d bps\r", rx_total, (int) x);
	fflush(stdout);
}

static void *client_thread_fn(void *a)
{
	static uint8_t run = 0;
	if(run) { printf("Already running\n"); return NULL; }

	
	if(cam_sck == -1) {
		cam_sck= wh_open_socket(WH_SOCK_TCP, 128, server_ip);
		if(cam_sck < 0) {
			printf("Error opening connection\n");
			return NULL;
		}
	}

	run = 1;

	connected = 1;

  g_camdata.rem_size = 8;
  g_cam_state = CAMSTATE_STARTSTR;
	wh_register_callback(cam_client_events);

	start_tv = sys_time();

	while(1) {
		if(connected && (exiting==0) && (stop_app_thread==0) ) {
			sleep(1);
			report_client();
			continue;
		}
		wh_close_socket(cam_sck);
		cam_sck = -1;
		connected = 0;
		printf("\n");
		printf("Closing connection\n");
		rx_total = 0;
		break;
	}

	run = 0;
	wh_unregister_callback();

	return NULL;
}

void start_camera_server() {

  /* KOIZUMI:: Server side is just send data. So Serever send Image data from Camera board */

	if (wh_get_state() == 0) {
		printf("WiFi is disconnected\n");
		return;
	}
	stop_app_thread = 0;
	if(pthread_create(&server_thread_id, NULL, server_thread_fn, NULL) != 0) {
		printf("Error starting server thread\n");
	}
	return;
}


static void handle_camera_data(uint8_t *data, int len)
{
  int sz;

  while (len)
    {
      switch (g_cam_state)
        {
          case CAMSTATE_STARTSTR:
            sz = g_camdata.rem_size <= len ? g_camdata.rem_size : len;
            memcpy(&g_camdata.start_str[8 - g_camdata.rem_size], data, sz);
            g_camdata.rem_size -= sz;
            data += sz;
            len -= sz;
            if (g_camdata.rem_size == 0)
              {
                printf("Received START STR [%s]\n", g_camdata.start_str);
                if (strcmp(g_camdata.start_str, "CAMDATA") == 0)
                  {
                    g_cam_state = CAMSTATE_BODY;
                    g_camdata.rem_size = IMG_BYTES;
                    g_camdata.is_jpeg = 0;
                  }
                else if (strcmp(g_camdata.start_str, "JPGDATA") == 0)
                  {
                    g_cam_state = CAMSTATE_SIZE;
                    g_camdata.rem_size = sizeof(int);
                    g_camdata.is_jpeg = 1;
                  }
                else
                  {
                    printf("Error...\n");
                  }
              }

            break;

          case CAMSTATE_SIZE:
            sz = g_camdata.rem_size <= len ? g_camdata.rem_size : len;
            memcpy(&g_camdata.sz.cjpgsz[sizeof(int) - g_camdata.rem_size], data, sz);
            g_camdata.rem_size -= sz;
            data += sz;
            len -= sz;
            if (g_camdata.rem_size == 0)
              {
                printf("JPEG data size:%d\n", g_camdata.sz.jpgsz);
                g_cam_state = CAMSTATE_BODY;
                g_camdata.rem_size = g_camdata.sz.jpgsz;
              }

            break;

          case CAMSTATE_BODY:
            sz = g_camdata.rem_size <= len ? g_camdata.rem_size : len;
            memcpy(&g_camdata.img[IMG_BYTES - g_camdata.rem_size], data, sz);
            g_camdata.rem_size -= sz;
            data += sz;
            len -= sz;
            if (g_camdata.rem_size == 0)
              {
                if (g_camdata.is_jpeg)
                  {
                    printf("Display JPG Image\n");
                    /* decode_and_display(g_camdata.sz.jpgsz); */
                    decode_and_display();
                    g_cam_state = CAMSTATE_STARTSTR;
                    g_camdata.rem_size = 8;
                    memset(g_camdata.start_str, 0, sizeof(g_camdata.start_str));
                  }
                else
                  {
                    printf("Display CAM Image\n");
                    g_cam_state = CAMSTATE_STARTSTR;
                    g_camdata.rem_size = 8;
                    memset(g_camdata.start_str, 0, sizeof(g_camdata.start_str));
                    nximage_draw((void *)g_camdata.img, IMGSIZE_W, IMGSIZE_H);
                  }
              }

            break;
        }
    }
}

int cam_client_events(int evt_type, int sckid, int event, char *remote, uint16_t port, uint8_t *data, int len, int evt_id )
{

  /* KOIZUMI:: Data is comming Camera Display should work here */

	if(evt_type == 0) {
	    if(sckid != cam_sck) return 0;
		if(len == 0) {
			// Error or remote closed session, so close the socket or connection
			connected = 0;
			printf("Closed connection\n");
			return 1;
		}
    handle_camera_data(data, len);
		rx_total += len;
		return 1;
	}
	else if(evt_type == 1) {
		if(sckid != cam_sck) return 0;
		if ( (event == EVENT_TYPE_CLOSED) || (event == EVENT_TYPE_RCVERR) ) {
			connected = 0;
			return 1;					
		}
	}
	return 0;
}



int server_ip_events(int evt_type, int sckid, int event, char *remote, uint16_t port, uint8_t *data, int len, int evt_id )
{

	if(evt_type == 0) {  // Data received
		if(sckid != cam_sck) return 0;
		if(len == 0) {
			// Error or remote closed session, so close the socket or connection
			connected = 0;
		}
		return 1;
	}
	else if(evt_type == 1) {	// Event received
		
		if(event == EVENT_TYPE_CONNECT) {
			if(connected) {
				printf("Cannot accept more connections\n");
				return 1;					
			}
			cam_sck = sckid;			
			connected = 1;
		}
		else if( (event == EVENT_TYPE_CLOSED) || (event == EVENT_TYPE_RCVERR) ) {
			if(sckid != cam_sck) return 0;
			cam_sck = -1;
			connected = 0;
			return 1;					
		}
		else if( event == EVENT_TYPE_SENDIDLE) {
			if(sckid != cam_sck) return 0;
			connected = 0;
			return 1;
		}

	}
	return 0;
}

static int wait_for_connection() 
{
	printf("Waiting for connection\n");
	while(1) {
		if(exiting || stop_app_thread) return 0;
		if(!connected ) {
			sleep(1);
			continue;
		}
		break;
	}
	printf("Conected with remote client\n");
	return 1;
}

static int stream_data(uint8_t *data, int len)
{
	// Returns 1 on error condition and 
	// 0 on error condition to close the server srv_thread

	printf("Starting streaming\n");


	if(wh_start_passmode(cam_sck) < 0) {
		printf("Cannot start streaming\n");
		return 0;
	}

	while(1) {
		if(exiting || stop_app_thread || !connected)  break;
/* KOIZUMI:: Camera data send here */
		memset(data,1,len);

		wh_send_tcp_data(cam_sck, data, len);
	}

	wh_end_passmode();
	return 1;
}


static void *server_thread_fn(void *a) 
{
	uint8_t *data;
	uint8_t db = 'a';

	static uint8_t run = 0;

	if(run) { printf("Already running\n"); return NULL;}
	stop_app_thread = 0;
  g_camfd = camera_initialize();

	do {
#if 0
		data = malloc(DBUF_LEN);
		if(!data) {
			printf("Error memory alloc\n");
			break;
		}
#endif

		if(srv_sck == -1) {
			srv_sck= wh_socket_listen(WH_SOCK_TCP, 128);
			if(srv_sck < 0) {
				printf("Error opening socket\n");
				break;
			}
		}
		run = 1;
		wh_register_callback(server_ip_events);

		printf("Starting server thread\n");

		while(1) {
			if( wait_for_connection() == 0 ) break;
#if 0
			if( stream_data(data, DBUF_LEN) == 0) break;
#else
      if (stream_camdata(g_camfd) == 0) break;
#endif
			if(cam_sck >= 0) {
				wh_close_socket(cam_sck);
				cam_sck = -1;
			}
			if(stop_app_thread || exiting) break;
		}

	} while(0);


	run = 0;
	wh_unregister_callback();
	wh_close_socket(srv_sck);
	srv_sck = -1;
#if 0
	free(data);
#endif
	printf("Closing  server thread\n");
	return NULL;
}

void start_echo_server(void)
{

	if (wh_get_state() == 0) {
		printf("WiFi is disconnected\n");
		return;
	}
	stop_app_thread = 0;
	if(pthread_create(&server_thread_id, NULL, echo_server_thread_fn, NULL) != 0) {
		printf("Error starting echo server thread\n");
	}
	return;
}



int echo_server_events(int evt_type, int sckid, int event, char *remote, uint16_t port, uint8_t *data, int len, int evt_id )
{

	if(evt_type == 0) {  // Data received
		if(sckid != srv_sck) return 0;
		if(len == 0) {
			// Error or remote closed session, so close the socket or connection
			connected = 0;
		}
		else {
			strcpy(echo_data.remote, remote);
			echo_data.port = port;
			echo_data.len = len;
			memcpy(echo_data.data, data, len);
			sem_post(&echo_sem);
		}
		return 1;
	}
	else if(evt_type == 1) {	// Event received
		if( event == EVENT_TYPE_RCVERR ) {
			if(sckid != srv_sck) return 0;
			connected = 0;
			return 1;					
		}

	}
	return 0;
}


static void *echo_server_thread_fn(void *a) 
{
	static uint8_t run = 0;

	if(run) {
		printf("Already running\n");
		return NULL;
	}

	do {
		if(srv_sck == -1) {
			srv_sck= wh_socket_listen(WH_SOCK_UDP, 129);
			if(srv_sck < 0) {
				printf("Error opening socket\n");
				return NULL;
			}
		}
		wh_register_callback(echo_server_events);
		connected = 1;
		sem_init(&echo_sem, 0, 0);
		run = 1;

		printf("Waiting for echo_requests\n");
		run = 1;
		while(1) {
			struct timespec ts;
			int res,i;

			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += 1;

			res = sem_timedwait(&echo_sem, &ts);

			if(exiting || !connected || stop_app_thread) break;
			if(res!=0)  continue;
			wh_send_udp_data(srv_sck, echo_data.remote, echo_data.port, echo_data.data,echo_data.len);
			WHLOG("Sent echo response to %s\n", echo_data.remote);
		}
	} while(0);

	if(srv_sck != -1) wh_close_socket(srv_sck);
	srv_sck = -1;
	sem_destroy(&echo_sem);
	wh_unregister_callback();
	connected = run = 0;
	printf("Closing  echo server thread\n");
}


void start_echo_client(void)
{
	if(connected) {
		printf("Close the running application first\n");
		return;
	}

	if (wh_get_state() == 0) {
		printf("Station disconnected\n");
		return;
	}

	printf("Enter Echo server IP address: "); fflush(stdout);
	if(readstdin(server_ip, 40) == NULL ) return;
	puts("\n");

	if(echo_sck == -1) {
		echo_sck = wh_open_socket(WH_SOCK_UDP, ECHO_PORT, NULL);
		if(echo_sck < 0) {
			printf("Error opening connection\n");
			return;
		}
	}
	connected = 1;
	wh_register_callback(echo_client_events);
	sem_init(&echo_sem, 0, 0);
	sem_init(&echo_resp, 0, 0);
	stop_app_thread = 0;
	if(pthread_create(&client_thread_id, NULL, echo_client_thread_fn, NULL) != 0) {
		printf("Error starting client thread\n");
	}

	printf("\n");

	return;


}


int echo_client_events(int evt_type, int sckid, int event, char *remote, uint16_t port, uint8_t *data, int len, int evt_id )
{

	if(evt_type == 0) {  // Data received
		if(sckid != echo_sck) return 0;
		if(len == 0) {
			// Error or remote closed session, so close the socket or connection
			connected = 0;
		}
		else {
			strcpy(echo_data.remote, remote);
			echo_data.port = port;
			echo_data.len = len;
			memcpy(echo_data.data, data, len);
			sem_post(&echo_resp);
		}
		return 1;
	}
	else if(evt_type == 1) {	// Event received
		if( event == EVENT_TYPE_RCVERR ) {
			if(sckid != echo_sck) return 0;
			connected = 0;
			return 1;					
		}

	}
	return 0;
}

static void *echo_client_thread_fn(void *a) 
{
	uint8_t edata=0;
	int eseq = 0;

	thread_start:
	printf("Sending echo_requests\n");


	while(1) {
		struct timespec ts;
		int res,i;
		float stime;

		memset(echo_data.data, edata++, 1024);
		echo_data.len = 1024;

		if(wh_send_udp_data(echo_sck, server_ip, ECHO_PORT, echo_data.data,echo_data.len) < 0) {
			printf("Error sending data\n");
			break;
		}
		stime = sys_time();
		eseq++;

		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 2;

		res = sem_timedwait(&echo_resp, &ts);
		if(exiting || !connected || stop_app_thread) break;
		if(res!=0)  {
			printf("No response (%d)\n", eseq);
			continue;
		}

		stime = sys_time()-stime;
		printf("Echo response received (%d), Latency: %6.4f secs\n", eseq, stime);
		usleep(50);
	}

	wh_close_socket(echo_sck);
	echo_sck = -1;
	sem_destroy(&echo_resp);
	connected = 0;
	thread_return:
	printf("Closing  echo client thread\n");
	return NULL;
}


#endif

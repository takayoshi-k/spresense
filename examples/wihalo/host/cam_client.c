#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#include "multiwebcam_server.h"

#define PORT 128
#define MARKER_STR "JPGDATA"
#define MARKER_YUV "CAMDATA"

// #define APP_DEBUG
#ifdef APP_DEBUG
# define DUMMYJPG_FNAME "test.jpg"
#endif

#define JPGBUFF_SIZE  (1024 * 1024 * 2)
struct jpeg_dat_s
{
  struct jpeg_dat_s *flink;
  int32_t size;
  char jpgdata[JPGBUFF_SIZE];
};

struct jpeg_select_s
{
  pthread_mutex_t lock;
  pthread_cond_t cond;
  struct jpeg_dat_s container[2];
  struct jpeg_dat_s dummy;
  struct jpeg_dat_s *recorded;
  struct jpeg_dat_s *free;
};

static struct jpeg_select_s g_inst;

static struct jpeg_dat_s *get_receiving_container(void)
{
  struct jpeg_dat_s *ret;

  pthread_mutex_lock(&g_inst.lock);
  ret = g_inst.free;
  if (ret)
    {
      g_inst.free = ret->flink;
      ret->flink = NULL;
    }
  else
    {
      ret = &g_inst.dummy;
    }
  pthread_mutex_unlock(&g_inst.lock);

  return ret;
}

static void set_tail_recorded(struct jpeg_dat_s *jpg)
{
  struct jpeg_dat_s *tmp;

  jpg->flink = NULL;  /* Just in case */
  if (g_inst.recorded)
    {
      for (tmp = g_inst.recorded; tmp->flink; tmp = tmp->flink);
      tmp->flink = jpg;
    }
  else
    {
      g_inst.recorded = jpg;
    }
}

static void set_received(struct jpeg_dat_s *jpg)
{
  pthread_mutex_lock(&g_inst.lock);
  if (jpg != &g_inst.dummy)
    {
      printf("   -> This is real\n");
      set_tail_recorded(jpg);
    }
  pthread_mutex_unlock(&g_inst.lock);
  pthread_cond_signal(&g_inst.cond);
}

static struct jpeg_dat_s *get_recorded_jpeg(void)
{
  struct jpeg_dat_s *ret;

  pthread_mutex_lock(&g_inst.lock);
  while ((ret = g_inst.recorded) == NULL)
    {
      pthread_cond_wait(&g_inst.cond, &g_inst.lock);
    }
  g_inst.recorded = ret->flink;
  ret->flink = NULL;
  pthread_mutex_unlock(&g_inst.lock);

  return ret;
}

static void set_free(struct jpeg_dat_s *jpg)
{
  pthread_mutex_lock(&g_inst.lock);
  jpg->flink = g_inst.free;
  g_inst.free = jpg;
  pthread_mutex_unlock(&g_inst.lock);
  pthread_cond_signal(&g_inst.cond);
}

static char dummy_data[JPGBUFF_SIZE];
static int check_request(int wsock)
{
  int n;
  n = recv(wsock, dummy_data, JPGBUFF_SIZE, 0);
  printf("======== Check request : %d======\n", n);
  printf("%s", dummy_data);
  printf("======== END OF Check request ======\n\n");

  printf("REQ==== %c%c%c%c%c%c%c%c%c%c%c%c\n",
          dummy_data[0], dummy_data[1], dummy_data[2], dummy_data[3],
          dummy_data[4], dummy_data[5], dummy_data[6], dummy_data[7],
          dummy_data[8], dummy_data[9], dummy_data[10], dummy_data[11]);
  if (!strncmp("GET / ", dummy_data, 6))
    {
      return 1;
    }
  else if (!strncmp("GET /video ", dummy_data, 11))
    {
      return 2;
    }

  return 0;
}

#ifdef APP_DEBUG
static bool is_loaded = false;
static void load_dummyjpg(struct jpeg_dat_s *jpg, const char *fname)
{
  FILE *fp = NULL;
  uint32_t bsize;

  if (!is_loaded)
    {
      fp = fopen(fname, "r");

      fseek(fp, 0, SEEK_END);
      bsize = ftell(fp);
      fseek(fp, 0, SEEK_SET);

      jpg->size = bsize;
      fread(jpg->jpgdata, 1, bsize, fp);

      fclose(fp);
      is_loaded = true;
    }
}


static void set_dummydata(struct jpeg_dat_s *dat)
{
  int i;
  dat->size = 10;
  for (i = 0; i < dat->size; i++)
    {
      dat->jpgdata[i] = '0' + i;
    }
}
#endif


static void *jpeg_sender(void *param)
{
  int ret;
  int wsock;
  int rsock;
  struct sockaddr_in client;
  struct jpeg_dat_s *jpg;

  printf("Sender thread\n");
  rsock = multiwebcam_initserver(8080);
  while (1)
    {
      printf("Wait connection\n");
      wsock = multiwebcam_waitconnection(rsock, &client);
      printf("connected\n");
      ret = check_request(wsock);

      switch (ret)
        {
          case 0:
            printf("Send 404\n");
            send_404(wsock);
            close(wsock);
            break;

          case 1:
            printf("Send normal page\n");
            send_normal_page(wsock);
            close(wsock);
            break;

          case 2:
            multiwabcam_sendheader(wsock);
            while (1)
              {
                printf("Getting data\n");
#ifdef APP_DEBUG
                jpg = &g_inst.dummy;
                // set_dummydata(jpg);
                load_dummyjpg(jpg, DUMMYJPG_FNAME);
#else
                jpg = get_recorded_jpeg();
#endif
                printf("Got it data: size = %d\n", jpg->size);
                ret = multiwebcam_sendframe(wsock, (char *)jpg->jpgdata,
                                            (int)jpg->size);
                set_free(jpg);

                if (ret < 0)
                  {
                    close(wsock);
                    wsock = -1;
                    break;
                  }
              }
            break;
        }
    }
}

static pthread_t start_jpegsender(void)
{
  pthread_t thd;
  pthread_create(&thd, NULL, jpeg_sender, NULL);
  return thd;
}

static void init_instance(void)
{
  int i;

  pthread_mutex_init(&g_inst.lock, NULL);
  pthread_cond_init(&g_inst.cond, NULL);
  g_inst.recorded = NULL;
  g_inst.free = NULL;
  for (i = 0; i < 2; i++)
    {
      g_inst.container[i].flink = NULL;
      g_inst.container[i].size = 0;
      set_free(&g_inst.container[i]);
    }

  start_jpegsender();
}

static void shift_marker_data(char *dat, int len, int shift)
{
  memmove(dat, &dat[shift], len - shift);
}

static int receive_sizeddata(int sock, char *buf, int len)
{
  int n;
  int rcved = 0;
  int retry = 10;

  while (rcved < len)
    {
      n = recv(sock, &buf[rcved], len - rcved, 0);
      if (n < 0)
        {
          return -1;
        }
      else if (n == 0)
        {
          retry--;
          if (retry == 0)
            {
              printf("Retry over....\n");
              return -2;
            }
        }
      else
        {
          rcved += n;
        }
    }

  return 0;
}

static void save_jpeg(const char *fname, void *data, int size)
{
  FILE *fp = NULL;
  printf("Saving %s\n", fname);
  fp = fopen(fname, "w");
  fwrite(data, 1, size, fp);
  fclose(fp);
  printf("Done\n");
}

int main(int argc, char *argv[])
{
  int sockfd;
  struct sockaddr_in serv_addr;
  char marker[8];
  int32_t jpgsize;
  int state;
  int ret;
  struct jpeg_dat_s *jpg;
  char *hostip = "192.168.200.10";
  int frame_num;

#define STATE_MARKER        (0)
#define STATE_MARKERSEARCH  (1)
#define STATE_SIZE          (2)
#define STATE_BODY          (3)

  if (argc >= 2) {
    hostip = (char *)argv[1];
  }
  printf("Connecting IP: %s:%d\n", hostip, PORT);

#ifdef APP_DEBUG
  /* For DEBUG */ init_instance(); while (1);
#endif

  // ソケットを作成する
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
      perror("ERROR opening socket");
      exit(1);
  }

  serv_addr.sin_addr.s_addr = inet_addr(hostip);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  // サーバーに接続する
  if (connect(sockfd, (struct sockaddr *)&serv_addr,
              sizeof(serv_addr)) < 0)
    {
      perror("ERROR connecting");
      exit(1);
    }

  init_instance();

  jpg = NULL;
  state = STATE_MARKER;
  frame_num = 0;
  while (1)
    {
      switch (state)
        {
          case STATE_MARKER:
            ret = receive_sizeddata(sockfd, marker, 8);
            if (ret != 0)
              {
                printf("Data transaction ERROR....\n");
                goto finish_app;
              }

            if (!strncmp(marker, MARKER_STR, 8) || !strncmp(marker, MARKER_YUV, 8))
              {
                printf("RcvSync\n");
                state = STATE_SIZE;
              }
            else
              {
                printf("Go search Sync\n");
                state = STATE_MARKERSEARCH;
                shift_marker_data(marker, 8, 1);
              }
            break;

          case STATE_MARKERSEARCH:
            ret = receive_sizeddata(sockfd, &marker[7], 1);
            if (ret != 0)
              {
                printf("Data transaction ERROR....\n");
                goto finish_app;
              }

            if (!strncmp(marker, MARKER_STR, 8) || !strncmp(marker, MARKER_YUV, 8))
              {
                printf("RcvSync\n");
                state = STATE_SIZE;
              }
            else
              {
                shift_marker_data(marker, 8, 1);
              }
            break;

          case STATE_SIZE:
            jpg = get_receiving_container();
            ret = receive_sizeddata(sockfd, (char *)&jpg->size, 4);
            if (ret != 0)
              {
                printf("Data transaction ERROR....\n");
                goto finish_app;
              }

            printf("JPEG size=%d\n", jpg->size);
            state = STATE_BODY;
            break;

          case STATE_BODY:
            ret = receive_sizeddata(sockfd, jpg->jpgdata, jpg->size);
            if (ret != 0)
              {
                printf("Data transaction ERROR....\n");
                goto finish_app;
              }

            frame_num++;
            if (frame_num == 10)
              {
                save_jpeg("rcvd.jpg", jpg->jpgdata, jpg->size);
              }

            printf("Set received\n");
            set_received(jpg);
            jpg = NULL;

            state = STATE_MARKER;
            break;

          default:
            state = STATE_MARKER;
            break;
        }
    }

finish_app:
    close(sockfd);

    return 0;
}

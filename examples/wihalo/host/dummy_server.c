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
#include <signal.h>

#define PORT 128
#define MARKER_STR "JPGDATA"

#define JPGBUFF_SIZE  (1024 * 1024 * 2)
struct jpeg_dat_s
{
  struct jpeg_dat_s *flink;
  int32_t size;
  char jpgdata[JPGBUFF_SIZE];
};

static void load_dummyjpg(struct jpeg_dat_s *jpg, const char *fname)
{
  FILE *fp = NULL;
  uint32_t bsize;

  fp = fopen(fname, "r");

  fseek(fp, 0, SEEK_END);
  bsize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  jpg->size = bsize;
  fread(jpg->jpgdata, 1, bsize, fp);

  fclose(fp);
}

static int initserver(int port_num)
{
  int ret;
  int s_sock;
  struct sockaddr_in addr;
  int32_t yes = 1;

  /* make socket */

  s_sock = socket(AF_INET, SOCK_STREAM, 0);
 
  if (s_sock < 0)
    {
      printf("Error. Cannot make socket\n");
      return -1;
    }

  setsockopt(s_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
    
  /* socket setting */

  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(port_num);
  addr.sin_addr.s_addr = INADDR_ANY;

  /* binding socket */    

  ret = bind(s_sock, (struct sockaddr *)&addr, sizeof(addr));
    
  if (ret < 0)
    {
      printf("Error. Cannot bind socket\n");
      close(s_sock);
      return -1;
    }
 
  /* listen socket */

  listen(s_sock, 1);

  return s_sock;
}

static int waitconnection(int s_sock, struct sockaddr_in *client)
{
  int c_sock;
  socklen_t len = sizeof(struct sockaddr_in);

  /* accept TCP connection from client */

  c_sock = accept(s_sock, (struct sockaddr *)client, &len);

  return c_sock;
}

static int send_binary(int s, const char *data, int len)
{
  int ret;
  int sending_len;
  int sent_len = 0;

  while (sent_len < len)
    {
      sending_len = len - sent_len;
      ret = write(s, data, sending_len);

      if (ret == 0)
        {
          printf("Send 0 byte. Maybe poor reception..\n");
        }

      if (ret < 0)
        {
          return ret;
        }

      sent_len += ret;
      data += ret;
    }

  return len;
}

int main(void)
{
  int ret;
  int ssock;
  int csock;
  struct jpeg_dat_s jpg;
  struct sockaddr_in client;

  load_dummyjpg(&jpg, "test.jpg");
  ssock = initserver(PORT);

  while (1)
    {
      csock = waitconnection(ssock, &client);

      while (1)
        {
          ret = send_binary(csock, MARKER_STR, 8);
          if (ret < 0) break;
          ret = send_binary(csock, (const char *)&jpg.size, sizeof(jpg.size));
          if (ret < 0) break;
          ret = send_binary(csock, jpg.jpgdata, jpg.size);
          if (ret < 0) break;
        }

      close(csock);
    }
}

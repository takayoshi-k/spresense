#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

static char buf[4096];

int main(void)
{
  int sockfd;
  struct hostent *server;
  struct sockaddr_in serv_addr;
  int n;

  // ソケットを作成する
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
      perror("ERROR opening socket");
      exit(1);
  }

  // サーバーのホスト名を取得する
  server = gethostbyname("localhost");
  if (server == NULL) {
      fprintf(stderr, "ERROR, no such host\n");
      exit(0);
  }

  // 接続先アドレスを設定する
  memset((char *)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr,
         server->h_length);
  serv_addr.sin_port = htons(8080);

#if 0
  serv_addr.sin_addr.s_addr = inet_addr(hostip);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
#endif

  // サーバーに接続する
  if (connect(sockfd, (struct sockaddr *)&serv_addr,
              sizeof(serv_addr)) < 0)
    {
      perror("ERROR connecting");
      exit(1);
    }


  while (1)
    {
      n = recv(sockfd, buf, 4096, 0);
      printf("========== Data:%d ==========\n", n);
      buf[n] = '\0';
      printf("%s", buf);
      printf("========== End of Data:%d ==========\n\n", n);
    }

  close(sockfd);
  return 0;
}

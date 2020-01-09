
#include <sdk/config.h>
#include <stdio.h>

#include "lwm2m_lte_connection.h"

extern int lightclient_main(void);

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int lwm2mlight_main(int argc, char *argv[])
#endif
{
  app_lwm2m_connect_to_lte();

  return lightclient_main();
}

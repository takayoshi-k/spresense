
#include <sdk/config.h>
#include <stdio.h>

/* Extern this function from ../lwm2mlight/test_object.c */
extern void updateRecognitionValue(int inst_no, char *data);

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int uprecg_main(int argc, char *argv[])
#endif
{
  if (argc == 2)
  {
    updateRecognitionValue(1, argv[1]);
  }
  else
  {
    printf("Error. Usage : nsh> %s [update string]\n", argv[0]);
  }
  return 0;
}


#include <nuttx/config.h>
#include <stdio.h>

#include "tensorflow/lite/micro/testing/micro_test.h"

extern "C" {

int main(int argc, FAR char *argv[])
{
  printf("Start %s\n", argv[0]);
  return test_main(argc, argv);
}

}



include $(APPDIR)/Make.defs
include $(SDKDIR)/Make.defs

PROGNAME = $(CONFIG_EXAMPLES_WIHALO_PROGNAME)
PRIORITY = $(CONFIG_EXAMPLES_WIHALO_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_WIHALO_STACKSIZE)
MODULE = $(CONFIG_EXAMPLES_WIHALO)

ASRCS =
CSRCS = nrc-hspi.c nrc-atcmd.c cli.c util.c nrc-iperf.c whapp.c build-information.c camera_bkgd.c camera_apis.c
MAINSRC = wihalo.c

include $(APPDIR)/Application.mk

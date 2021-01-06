############################################################################
# externals/tensorflow/Makefile
#
#   Copyright 2021 Sony Semiconductor Solutions Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name of Sony Semiconductor Solutions Corporation nor
#    the names of its contributors may be used to endorse or promote
#    products derived from this software without specific prior written
#    permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################

-include $(TOPDIR)/Make.defs
-include $(SDKDIR)/Make.defs

CUR_DIR=$(shell pwd)

BIN = libtensorflow-microlite$(LIBEXT)

CSRCS =

TENSORFLOW_GITHUB_URL = https://github.com/tensorflow/tensorflow.git
TENSORFLOW_TAG = v2.4.0

TENSORFLOW_DIR = $(CUR_DIR)/tensorflow
TENSORFLOW_MAKEFILE = tensorflow/lite/micro/tools/make/Makefile
TENSORFLOW_TAGS = cmsis-nn
TENSORFLOW_TARGET = spresense
TENSORFLOW_LIBOUT = tensorflow/tensorflow/lite/micro/tools/make/gen/spresense_cortex-m4/lib
SPRESENSE_MAKEDEF = $(TOPDIR)/Make.defs

PATCHES = $(shell find $(CUR_DIR)/patches -name "*.patch")

TF_MAKECMD := make -f $(TENSORFLOW_MAKEFILE) \
											TAGS=$(TENSORFLOW_TAGS) \
											TARGET=$(TENSORFLOW_TARGET) \
											SPRESENSE_DEFS=$(SPRESENSE_MAKEDEF) \
											SPRESENSE_CONFIG_H=$(TOPDIR)/include/nuttx/config.h \
											SPRESENSE_CURDIR=$(CUR_DIR)

context:: copy_tensorflow

clean:: tensorflow_clean

distclean:: delete_tensorflow

tensorflow_clean:
	$(Q) if [ -d $(TENSORFLOW_DIR) ]; then \
		cd $(TENSORFLOW_DIR); $(TF_MAKECMD) clean; \
	fi

download_tensorflow:
	$(Q) if [ ! -d $(TENSORFLOW_DIR) ]; then \
		echo Clone $(TENSORFLOW_GITHUB_URL) tag $(TENSORFLOW_TAG);  \
		git clone $(TENSORFLOW_GITHUB_URL) -b $(TENSORFLOW_TAG) --depth 1; \
		files="$(PATCHES)"; \
		cd $(TENSORFLOW_DIR); \
		for ff in $$files; do git apply $$ff; done; \
		cd $(CUR_DIR); \
		./copy_unmerged_src.sh ; \
	fi

build_tensorflow: download_tensorflow
	$(Q) cd $(TENSORFLOW_DIR); echo Build $(BIN) ;  \
	$(TF_MAKECMD)

copy_tensorflow: build_tensorflow
	$(Q) cp $(TENSORFLOW_LIBOUT)/$(BIN) $(CUR_DIR)

delete_tensorflow:
	$(Q) if [ -d $(TENSORFLOW_DIR) ]; then \
		rm -rf $(TENSORFLOW_DIR); \
	fi

include $(APPDIR)/Application.mk


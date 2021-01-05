# Settings for stm32f4 based platforms

# Evacuate Compiler flags to avoid override them with loading Spresense Config
TMP_CXXFLAGS := $(CXXFLAGS)
TMP_CCLAGS := $(CCFLAGS)

# Load Spresense Config
include $(SPRESENSE_DEFS)

TARGET_ARCH := cortex-m4
TARGET_TOOLCHAIN_PREFIX := arm-none-eabi-

# $(eval $(call add_third_party_download,$(CMSIS_URL),$(CMSIS_MD5),cmsis,patch_cmsis))

PLATFORM_FLAGS = \
  -DGEMMLOWP_ALLOW_SLOW_SCALAR_FALLBACK \
  -DTF_LITE_STATIC_MEMORY \
  -DTF_LITE_MCU_DEBUG_LOG \
  -I$(SPRESENSE_CURDIR)/wrapper_include \
  -DSPRESENSE_CONFIG_H="\"$(SPRESENSE_CONFIG_H)\"" \
  -fmessage-length=0 \
  -fno-exceptions \
  -fno-unwind-tables \
  -ffunction-sections \
  -fdata-sections \
  -funsigned-char \
  -MMD \
  -mcpu=cortex-m4 \
  -mabi=aapcs \
  -mthumb \
  -mfpu=fpv4-sp-d16 \
  -mfloat-abi=hard \
  -Wall \
  -Wextra \
  -Wno-shadow \
  -Wno-vla \
  -Wno-strict-aliasing \
  -Wno-type-limits \
  -Wno-unused-parameter \
  -Wno-missing-field-initializers \
  -Wno-write-strings \
  -Wno-sign-compare \
  -Wunused-function \
  -fno-delete-null-pointer-checks \
  -fomit-frame-pointer \
  -Os

CXXFLAGS := $(TMP_CXXFLAGS) $(PLATFORM_FLAGS) -std=gnu++11 -fno-rtti -fno-use-cxa-atexit
CCFLAGS := $(TMP_CCFLAGS) $(PLATFORM_FLAGS)

BUILD_TYPE := micro

INCLUDES +=  -isystem$(MAKEFILE_DIR)/downloads/cmsis/CMSIS/Core/Include/

THIRD_PARTY_CC_SRCS := \
    $(THIRD_PARTY_CC_SRCS) \
    $(MAKEFILE_DIR)/../../spresense/compiler_specific.cc \

# TODO(b/158324045): Examine why some tests fail here.
EXCLUDED_TESTS := \
  tensorflow/lite/micro/micro_interpreter_test.cc \
  tensorflow/lite/micro/micro_allocator_test.cc \
  tensorflow/lite/micro/memory_helpers_test.cc \
  tensorflow/lite/micro/memory_arena_threshold_test.cc \
  tensorflow/lite/micro/recording_micro_allocator_test.cc \
  tensorflow/lite/micro/kernels/circular_buffer_test.cc
MICROLITE_TEST_SRCS := $(filter-out $(EXCLUDED_TESTS), $(MICROLITE_TEST_SRCS))

EXCLUDED_EXAMPLE_TESTS := \
  tensorflow/lite/micro/examples/magic_wand/Makefile.inc \
  tensorflow/lite/micro/examples/micro_speech/Makefile.inc \
  tensorflow/lite/micro/examples/person_detection_experimental/Makefile.inc \
  tensorflow/lite/micro/examples/image_recognition_experimental/Makefile.inc
MICRO_LITE_EXAMPLE_TESTS := $(filter-out $(EXCLUDED_EXAMPLE_TESTS), $(MICRO_LITE_EXAMPLE_TESTS))

# These are microcontroller-specific rules for converting the ELF output
# of the linker into a binary image that can be loaded directly.
OBJCOPY := $(TARGET_TOOLCHAIN_PREFIX)objcopy

$(BINDIR)/%.bin: $(BINDIR)/%
	@mkdir -p $(dir $@)
	$(OBJCOPY) $< $@ -O binary


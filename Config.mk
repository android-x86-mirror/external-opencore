ifneq ($(strip $(EXTERNAL_OPENCORE_CONFIG_ONCE)),true)
  # This is the first attempt to include this file
  EXTERNAL_OPENCORE_CONFIG_ONCE := true

  PV_TOP := $(my-dir)

  PV_CFLAGS := -Wno-non-virtual-dtor -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DUSE_CML2_CONFIG

  ifeq ($(PV_WERROR),1)
    PV_CFLAGS += -Werror
  endif
  ifeq ($(TARGET_ARCH),arm)
    ifeq (,armv4t)
      PV_CFLAGS += -DPV_ARM_GCC_V4
    else
      PV_CFLAGS += -DPV_ARM_GCC_V5
    endif
  endif

  FORMAT := android

  PV_COPY_HEADERS_TO := libpv

  PV_CFLAGS_MINUS_VISIBILITY := $(PV_CFLAGS)
  PV_CFLAGS += -fvisibility=hidden

  PV_INCLUDES := \
	$(PV_TOP)/android \
	$(PV_TOP)/../sqlite/dist \
	$(PV_TOP)/../../frameworks/base/core/jni \
	$(JNI_H_INCLUDE) \
	$(PV_TOP)/extern_libs_v2/khronos/openmax/include \
	$(PV_TOP)/engines/common/include \
	$(PV_TOP)/engines/player/config/android \
	$(PV_TOP)/engines/player/include \
	$(PV_TOP)/nodes/pvmediaoutputnode/include \
	$(PV_TOP)/nodes/pvdownloadmanagernode/config/opencore \
	$(PV_TOP)/pvmi/pvmf/include \
	$(PV_TOP)/fileformats/mp4/parser/config/opencore \
	$(PV_TOP)/oscl/oscl/config/android \
	$(PV_TOP)/oscl/oscl/config/shared \
	$(PV_TOP)/engines/author/include \
	$(PV_TOP)/android/drm/oma1/src \
	$(PV_TOP)/build_config/opencore_dynamic \
	$(TARGET_OUT_HEADERS)/$(PV_COPY_HEADERS_TO)

  # Stash these values for the next include of this file
  OPENCORE.PV_TOP := $(PV_TOP)
  OPENCORE.PV_CFLAGS := $(PV_CFLAGS)
  OPENCORE.PV_CFLAGS_MINUS_VISIBILITY := $(PV_CFLAGS_MINUS_VISIBILITY)
  OPENCORE.FORMAT := $(FORMAT)
  OPENCORE.PV_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)
  OPENCORE.PV_INCLUDES := $(PV_INCLUDES)
else
  # This file has already been included by someone, so we can
  # use the precomputed values.
  PV_TOP := $(OPENCORE.PV_TOP)
  PV_CFLAGS := $(OPENCORE.PV_CFLAGS)
  PV_CFLAGS := $(OPENCORE.PV_CFLAGS_MINUS_VISIBILITY)
  FORMAT := $(OPENCORE.FORMAT)
  PV_COPY_HEADERS_TO := $(OPENCORE.PV_COPY_HEADERS_TO)
  PV_INCLUDES := $(OPENCORE.PV_INCLUDES)
endif

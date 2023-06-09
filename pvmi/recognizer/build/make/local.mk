# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

LOCAL_DISABLE_COMPILE_WARNINGS_AS_ERRORS := 1

TARGET := pvmfrecognizer


SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := pvmf_recognizer_registry.cpp \
	pvmf_recognizer_registry_impl.cpp

HDRS := pvmf_recognizer_registry.h \
	pvmf_recognizer_types.h \
    pvmf_recognizer_plugin.h

include $(MK)/library.mk




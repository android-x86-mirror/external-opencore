# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvpvxparser










SRCDIR := ../../src
INCSRCDIR := ../../include

SRCS := pvpvxparser.cpp

HDRS := pvpvxparser.h



include $(MK)/library.mk

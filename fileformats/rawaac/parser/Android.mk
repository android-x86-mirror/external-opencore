LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	src/aacfileparser.cpp \
 	src/aacfileio.cpp


LOCAL_MODULE := libpvaacparser

LOCAL_CFLAGS :=   $(PV_CFLAGS)


LOCAL_STATIC_LIBRARIES := 

LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
	$(PV_TOP)/fileformats/rawaac/parser/src \
 	$(PV_TOP)/fileformats/rawaac/parser/include \
 	$(PV_INCLUDES)

LOCAL_COPY_HEADERS_TO := $(PV_COPY_HEADERS_TO)

LOCAL_COPY_HEADERS := \
	include/aacfileparser.h \
 	include/aacfileio.h

include $(BUILD_STATIC_LIBRARY)

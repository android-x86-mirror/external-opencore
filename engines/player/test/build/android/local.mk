# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

TARGET := pvplayer_engine_test



# Temporarily ignore these warnings in the player test code until they can be
# fixed
CXXFLAGS += $(DISABLE_STRICT_ALIASING_WARNINGS)

XCPPFLAGS += -DBUILD_OMX_DEC_NODE 


XINCDIRS +=  ../../../../common/include  ../../../../../pvmi/media_io/pvmiofileoutput/include  ../../../../../nodes/pvmediaoutputnode/include  ../../../include  ../../../../../nodes/common/include  ../../../../../extern_libs_v2/khronos/openmax/include

# This makefile is used by opencore config. So, only the android configuration is required
XINCDIRS +=  ../../config/android





SRCDIR := ../../src
INCSRCDIR := ../../src

SRCS := test_pv_player_engine.cpp \
        test_pv_player_engine_testset_mio_file.cpp \
        test_pv_player_engine_testset1.cpp \
        test_pv_player_engine_testset5.cpp \
        test_pv_player_engine_testset6.cpp \
        test_pv_player_engine_testset7.cpp \
        test_pv_player_engine_testset8.cpp \
        test_pv_player_engine_testset9.cpp \
        test_pv_player_engine_testset10.cpp \
        test_pv_player_engine_testset11.cpp \
        test_pv_player_engine_testset12.cpp \
        test_pv_player_engine_testset13.cpp \
        test_pv_player_engine_testset_cpmdlapassthru.cpp


LIBS := unit_test opencore_player opencore_common opencore_net_support

ifneq ($(ARCHITECTURE),win32)
  SYSLIBS = -lpthread -ldl
endif

include $(MK)/prog.mk

PE_TEST_DIR = ${BUILD_ROOT}/pe_test
PE_TARGET = pvplayer_engine_test

setup_pe_test::
	@echo "[make] setting up for testing..."
	$(quiet) ${RM} -r ${PE_TEST_DIR}
	$(quiet) ${MKDIR} ${PE_TEST_DIR}
	$(quiet) ${RM} -f $(PE_TEST_DIR)/failed_tests.txt

setup_pe_test_linux:: setup_pe_test
	@echo "[make] setting up for linux testing..."
	$(quiet) $(CP) $(SRC_ROOT)/tools_v2/build/package/opencore/elem/common/pvplayer.cfg $(PE_TEST_DIR)
	$(quiet) $(CP) -r $(SRC_ROOT)/engines/player/test/data/* $(PE_TEST_DIR)

setup_pe_test_android:: setup_pe_test
	@echo "[make] setting up for android testing..."

run_pe_test:: $(REALTARGET) default setup_pe_test_linux 
	@echo "[make] running..."
	$(quiet) export LD_LIBRARY_PATH=${BUILD_ROOT}/installed_lib/${HOST_ARCH}; cd $(PE_TEST_DIR) && ${BUILD_ROOT}/bin/${HOST_ARCH}/$(PE_TARGET) $(TEST_ARGS) $(SOURCE_ARGS)

ifeq ($(strip $(EMULATOR_TEST)),1)
    ifeq ($(strip $(ADB)),)
        ADB = adb
    endif
    TEST_CMD = $(quiet) $(ADB) shell 'cd /sdcard; pvplayer_engine_test -test $@ $@ $(TEST_ARGS) $(TEST_ARGS_FROM_FILE) $(SOURCE_ARGS)' >  $(PE_TEST_DIR)/test_output_$@.txt 2>&1; grep 'Failures: 0' $(PE_TEST_DIR)/test_output_$@.txt || echo "$(ADB) shell 'cd /sdcard; pvplayer_engine_test -test $@ $@ $(TEST_ARGS) $(TEST_ARGS_FROM_FILE) $(SOURCE_ARGS)'" >> $(PE_TEST_DIR)/failed_tests.txt
    REALTARGET = 
    SETUP_TEST = setup_pe_test_android
else
    TEST_CMD = $(quiet) export LD_LIBRARY_PATH=${BUILD_ROOT}/installed_lib/${HOST_ARCH}; cd $(PE_TEST_DIR) && ${BUILD_ROOT}/bin/${HOST_ARCH}/$(PE_TARGET) -test $@ $@ $(TEST_ARGS) $(TEST_ARGS_FROM_FILE) $(SOURCE_ARGS) >> $(PE_TEST_DIR)/test_output_$@.txt 2>&1 || echo "$(PE_TARGET) -test $@ $@ $(TEST_ARGS) $(TEST_ARGS_FROM_FILE) $(SOURCE_ARGS)" >> $(PE_TEST_DIR)/failed_tests.txt
    SETUP_TEST = setup_pe_test_linux
endif

ifneq ($(strip $(PARALLEL_TESTS_FILE)),)
    PARALLEL_TESTS = $(shell cat $(PARALLEL_TESTS_FILE) | cut -d' ' -f1)
else 
    ifneq ($(strip $(PARALLEL_TEST_RANGE)),)
        PARALLEL_TESTS = $(shell seq $(PARALLEL_TEST_RANGE))
    endif
endif
run_pe_parallel_test:: $(REALTARGET) $(PARALLEL_TESTS)
	echo "[make] concatenating results..."
	@for test_num in $(PARALLEL_TESTS);\
	do\
		cat $(PE_TEST_DIR)/test_output_$${test_num}.txt;\
	done
	@if [ -e $(PE_TEST_DIR)/failed_tests.txt ]; then\
		echo "[make] The following player engine tests failed:";\
		cat $(PE_TEST_DIR)/failed_tests.txt;\
        exit 1;\
	fi

$(PARALLEL_TESTS): TEST_ARGS_FROM_FILE = $(shell if [ -n "$(PARALLEL_TESTS_FILE)" ]; then grep "^$@ " $(PARALLEL_TESTS_FILE) | cut -d' ' -s -f2-; fi)
$(PARALLEL_TESTS):: $(SETUP_TEST)
	@echo "[make] running test #$@..."
	@echo "$(PE_TARGET) -test $@ $@ $(TEST_ARGS) $(TEST_ARGS_FROM_FILE) $(SOURCE_ARGS)" > $(PE_TEST_DIR)/test_output_$@.txt
	-$(TEST_CMD)
	@echo "[make] completed test #$@."

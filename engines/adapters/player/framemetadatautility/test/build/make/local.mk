# Get the current local path as the first operation
LOCAL_PATH := $(call get_makefile_dir)

# Clear out the variables used in the local makefiles
include $(MK)/clear.mk

# DISABLE TREATING WARNINGS AS ERRORS
# THIS SHOULD BE FIXED UP ASAP
# see linux_arm warnings at a minimum
LOCAL_DISABLE_COMPILE_WARNINGS_AS_ERRORS := 1

TARGET := pv_frame_metadata_utility_test

XCPPFLAGS := -DBUILD_OMX_DEC_NODE

XINCDIRS := ../../config/common ../../../../../../../extern_libs_v2/khronos/openmax/include

SRCDIR := ../../src
INCSRCDIR := ../../src

SRCS := test_pv_frame_metadata_utility.cpp \
        test_pv_frame_metadata_utility_testset1.cpp \
        test_pv_frame_metadata_utility_testset_januscpm.cpp

LIBS :=  pvframemetadatautility \
        colorconvert \
        pvplayer_engine \
        pvfileoutputnode \
        pvpvxparser \
        pvmp3ffparsernode \
        mp3decnode \
        pvdownloadmanagernode \
        pvdtcp_mbds_lib \
        pvmp4ffparsernode \
		pvflvffparsernode \
        cpm \
        pvoma1passthruplugin \
        pvaacffparsernode \
        pvwavffparsernode \
        pvstillimagenode \
        pvmp4ff \
		pvflvff \
        pvmp3ff \
        pvwav \
        pvaacparser \
        getactualaacconfig \
        pvomxvideodecnode \
        pvomxaudiodecnode \
        pvomxbasedecnode \
        m4v_config \
        pvvideodecnode \
        pvavcdecnode \
        gsmamrdecnode \
        aacdecnode \
        pvstreamingmanagernode \
        pvrtspbroadcastwithpvrsm \
        pvmshttpstreamingmanager \
        pvfileplaybackwithpvrsm \
        pvrtspunicaststreamingmanager \
        pvrtsptunicaststreamingmanager \
        pvrtsptunicastrmstreamingmanager \
        pvrtspunicastwithpvrsm \
		pvsmpvrfspcommon \
        pvsmfspcommon \
        pvjitterbuffernode \
        pvjitterbufferasf \
        pvjitterbufferrtp \
        pvjitterbuffer \
        pvmedialayernode \
        rtprtcp \
        pvrtsp_cli_eng_node \
        pvrtsp_cli_eng_real_cloaking_node \
        pvrtsp_cli_eng_playlist_node \
        protocolenginenode \
        pv_http_parcom \
		pv_rtmp_parcom \
        pvsocketnode \
        pvrtppacketsourcenode \
        rtppayloadparser \
        realpayloadparser \
        asfpayloadparser \
        pvpvrnode \
        pvpvr \
        pvpvrff \
        pvmf \
        pvsdpparser \
        pv_rtsp_parcom \
        pvwmadecnode \
        omx_common_lib \
        pv_config_parser \
        omx_mp3_component_lib \
        pvmp3 \
        omx_avc_component_lib \
        pvavcdecoder \
        pv_avc_common_lib \
        omx_amr_component_lib \
        pvdecoder_gsmamr \
        pv_amr_nb_common_lib \
        pvamrwbdecoder \
        omx_m4v_component_lib \
        pvmp4decoder \
        omx_wmv_component_lib \
        omx_aac_component_lib \
        pv_aac_dec \
        pvmiofileoutput \
        pvmimeutils \
        pvmediaoutputnode \
        pvmediadatastruct \
        pvamrffparsernode \
        pvgsmamrparser \
        pvlatmpayloadparser \
        rdt_parser \
        pvwmvdecnode \
        g726decnode \
        pvg726decoder \
        wmvdecoder \
        omx_wma_component_lib \
        wmadecoder \
        wmavoicedecoder \
        pvasfffparsernode \
        pvasfff \
        pvjanusplugin \
        pvmfrecognizer \
        pvasfffrecognizer \
        pvmp4ffrecognizer \
		pvflvffrecognizer \
        mp4recognizer_utility \
		flvrecognizer_utility \
        pvmp3ffrecognizer \
        pvrmffrecognizer \
        pvoma1ffrecognizer \
		pvaacffrecognizer \
		pvwavffrecognizer \
        pvfileparserutils \
        omx_rv_component_lib \
        pvrvdecnode \
        rvdecoder \
        pvra8decnode \
        pvra8decoder \
        pvrmffparsernode \
        pvrmffparser \
        pvrmff \
        pvrmffrecognizer_utility \
        realaudio_deinterleaver \
        pvid3parcom \
        pvgendatastruct \
        pvwmdrm \
        csprng\
        pvcrypto \
        pventropysrc\
        pvlogger \
        osclregcli \
        osclregserv \
        osclio \
        osclproc \
        osclutil \
        osclmemory \
        osclerror \
        osclbase \
        omx_baseclass_lib \
        pvomx_proxy_lib \
        omx_queue_lib \
        threadsafe_callback_ao \
        packetsources_default \
        unit_test \
        asfrecognizer_utility \
        scsp \
        pvdivxffrecognizer \
        pvdivxffparsernode \
        pv_divxfile_parser \
        pvamrffrecognizer \
        divxrecognizer_utility \
        pvplsffrecognizer

SYSLIBS += $(SYS_THREAD_LIB)

TEST_ARGS := -all

SOURCE_ARGS := -source ../../../../../../../extern_libs_v2/wmdrmmd/samples/MD220x176_96_384s_clear_wmv.asf 


include $(MK)/prog.mk


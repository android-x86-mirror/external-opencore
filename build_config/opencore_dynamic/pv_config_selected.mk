#
# Automatically generated, don't edit
#

#
# PV Code Base Configuration System
#

#
# Menu for selecting supported features
#
module_support=y
PV_HAS_SHOUTCAST_SUPPORT_ENABLED=y


#
# Menu for configuring runtime loadable modules
#
pv_so=n
pvsplit_so=y
download_support=y
fasttrack_download_support=n
mp4local_support=y
divxlocal_support=n
rmff_support=n
flvlocal_support=n
asflocal_support=n
janus_support=n
oma1_passthru_support=y
wmdrmoemsettings_support=n
pvdivxoemsettings_support=n
divxdrm_support=n
mtp_db_support=n
pvdb_config_support=n
mtp_config_support=n
usb_transport_support=n
dtcp_support=n


#
# Menu for configuring Baselibs
#
csprng_lib=n
pvcrypto_lib=n
pventropysrc_lib=n
pvgendatastruct_lib=m
pvmediadatastruct_lib=m
pvmimeutils_lib=m
threadsafe_callback_ao_lib=m
pvthreadmessaging_lib=m


#
# Menu for configuring File Formats
#
pvasfff_lib=n
pvmp3ff_lib=m
pvmp4ffcomposer_lib=n
pvmp4ffcomposeropencore_lib=m
pvmp4ff_lib=n
pvmp4ffopencore_lib=m
mp4recognizer_utility_lib=m
pvaacparser_lib=m
pvgsmamrparser_lib=m
pvrmff_lib=n
pvrmffparser_lib=n
pvfileparserutils_lib=m
pvid3parcom_lib=m
pvpvxparser_lib=m
pvwav_lib=m
pvasxparser_lib=n
pvavifileparser_lib=m
pvpvrff_lib=n
asfrecognizer_utility_lib=n
pv_divxfile_parser_lib=n
scsp_lib=m
divxrecognizer_utility_lib=n
pvflvff_lib=n
pvrmffrecognizer_utility_lib=n
flvrecognizer_utility_lib=n


#
# Menu for configuring Codecs
#

#
# Menu for configuring OMX Support
#
omx_mastercore_lib=m
MAX_NUMBER_OF_OMX_CORES=10
MAX_NUMBER_OF_OMX_COMPONENTS=50
pv_omx=y
omx_avc_component_lib=m
omx_common_lib=m
omx_m4v_component_lib=m
omx_queue_lib=m
omx_wmv_component_lib=n
omx_rv_component_lib=n
pvomx_proxy_lib=m
omx_aac_component_lib=m
omx_amr_component_lib=m
omx_mp3_component_lib=m
omx_wma_component_lib=n
omx_ra_component_lib=n
omx_amrenc_component_lib=m
omx_m4venc_component_lib=m
omx_avcenc_component_lib=m
omx_aacenc_component_lib=n
omx_baseclass_lib=m


#
# Menu for configuring audio codecs
#
pv_aac_dec_lib=m
pv_aac_enc_lib=n
getactualaacconfig_lib=m
pv_amr_nb_common_lib=m
pvdecoder_gsmamr_lib=m
pvencoder_gsmamr_lib=m
pvamrwbdecoder_lib=m
gsm_amr_headers_lib=m
pvmp3_lib=m
pvra8decoder_lib=n
wmadecoder_lib=n
useoldwmadecoder=n
wma_audio_support=m

#
# WMA Audio library build configurations (see build_configuration.doc)
#
wma_build_cfg_std=n
wma_build_cfg_stdprostereolbrv1=n
wma_build_cfg_stdprostereolbr=n
wma_build_cfg_stdprolbr=n
wma_build_cfg_lsl=n
wma_build_cfg_all=y

wmavoicedecoder_lib=n
wma_voice_support=m
wma_common_hdrs_lib=n
pvg726decoder_lib=n


#
# Menu for configuring video codecs
#
pv_avc_common_lib=m
pvavcdecoder_lib=m
wmvdecoder_lib=n
pvmp4decoder_lib=m
M4VDEC_FLV_SUPPORT=n
rvdecoder_lib=n
pvm4vencoder_lib=m
pvavch264enc_lib=m


#
# Menu for configuring codecs utilities
#
m4v_config_lib=m
pv_config_parser_lib=m
colorconvert_lib=m

#
# Choose the YUYV format
#
UY0VY1=y
Y1VY0U=n
Y0VY1U=n




#
# Menu for configuring Nodes
#

#
# Menu for configuring Streaming Plugins
#
mshttp_support=n
rtspunicast_support=m
rtsptunicast_support=m
rtsptunicastwithreal_support=n
broadcastpvr_support=n
rtspunicastpvr_support=n
fileplaybackpvr_support=n


#
# Menu for configuring Download
#
pvdownloadmanagernode_lib=m
pvdtcp_mbds_lib=n

#
# Menu for configuring downloadmanager features
#
PVMF_DOWNLOADMANAGER_SUPPORT_PPB=y
PVMF_MEMORYBUFFERDATASTREAM_DTCP_PPB=n
PVMF_DOWNLOADMANAGER_MIN_TCP_BUFFERS_FOR_PPB=39
PVMF_DOWNLOADMANAGER_CACHE_SIZE_FOR_SC_IN_SECONDS=6
PVMF_DOWNLOADMANAGER_MAX_BITRATE_FOR_SC=128



#
# Menu for adding NON-OMX Nodes
#
aacdecnode_lib=n
mp3decnode_lib=n
pvavcdecnode_lib=n
pvwmadecnode_lib=n
pvwmvdecnode_lib=n
gsmamrdecnode_lib=n
pvvideodecnode_lib=n
pvrvdecnode_lib=n
pvvideoencnode_lib=n
pvamrencnode_lib=n
pvavcencnode_lib=n


#
# Menu for configuring ProtocolEngine
#
pvprotocolenginenode_segments_lib=m
pvprotocolenginenode_lib=n
pvprotocolenginenode_pdl_plugin_lib=m
pvprotocolenginenode_ps_plugin_lib=m
pvprotocolenginenode_ftdl_plugin_lib=n
pvprotocolenginenode_wmhttpstreaming_plugin_lib=n
pvprotocolenginenode_shoutcast_plugin_lib=m

#
# Menu for configuring protocol engine node plugin registry
#
BUILD_PROGRESSIVE_DOWNLOAD_PLUGIN=y
BUILD_PROGRESSIVE_STREAMING_PLUGIN=y
BUILD_FASTTRACK_DOWNLOAD_PLUGIN=n
BUILD_WMHTTPSTREAMING_PLUGIN=n
BUILD_SHOUTCAST_PLUGIN=y


pvfileoutputnode_lib=m
pvmediaoutputnode_lib=m
pvsocketnode_lib=m
pvwavffparsernode_lib=m
pvomxencnode_lib=m
pvomxaudiodecnode_lib=m
pvomxbasedecnode_lib=m
pvomxvideodecnode_lib=m
pvaacffparsernode_lib=m
pvamrffparsernode_lib=m
pvasfffparsernode_lib=n
pvmp3ffparsernode_lib=m
pvmp4ffparsernode_lib=n
pvmp4ffparsernodeopencore_lib=m
pvra8decnode_lib=n
pvrmffparsernode_lib=n
pvrtppacketsourcenode_lib=n
nodes_common_headers_lib=m
pvmediainputnode_lib=m
pvmp4ffcomposernode_lib=n
pvmp4ffcomposernodeopencore_lib=m
pvpvr_lib=n
pvpvrnode_lib=n
pvcommsionode_lib=m
pvclientserversocketnode_lib=m
pvloopbacknode_lib=m
pvvideoparsernode_lib=m
pvdummyinputnode_lib=m
pvdummyoutputnode_lib=m
pvdivxffparsernode_lib=n
pvstillimagenode_lib=n
g726decnode_lib=n
pvflvffparsernode_lib=n


#
# Menu for configuring Oscl
#
build_oscl=m
unit_test_lib=y


#
# Menu for configuring Protocols
#

#
# Menu for configuring Value Adds for 2way
#
twoway_value_add_config=y
PV_2WAY_VALUE_ADD_NONE=y

pv_http_parcom_lib=m
pv_http_retriever_lib=n
pvlatmpayloadparser_lib=m
rdt_parser_lib=n
pv_rtsp_parcom_lib=m
rtppayloadparser_lib=m

#
# Menu for rtppayload parser plugins
#
rfc_2429=y
rfc_3016=y
rfc_3267=y
rfc_3640=y
rfc_3984=y

asf_payload_lib=n
realmedia_payload_lib=n
rtprtcp_lib=m
pv324m_lib=m
pv_rtmp_parcom_lib=n
pv324m_common_headers_lib=m
pvgeneraltools_lib=m


#
# Menu for configuring Pvmi
#

#
# Menu for configuring Recognizers
#
pvmfrecognizer_lib=m
pvaacffrecognizer_lib=m
pvamrffrecognizer_lib=m
pvoma1ffrecognizer_lib=n
pvasfffrecognizer_lib=n
pvmp3ffrecognizer_lib=m
pvmp4ffrecognizer_lib=m
pvwavffrecognizer_lib=m
pvrmffrecognizer_lib=n
pvdivxffrecognizer_lib=n
pvplsffrecognizer_lib=m
pvflvffrecognizer_lib=n


#
# Menu for configuring Content Policy Manager
#
cpm_lib=m
oma1_passthru_plugin_lib=m
pvjanusplugin_lib=n
cpm_headers_lib=m
pvoma1lockstream_lib=n
pvplayreadyplugin_lib=n
pvplayreadyutility_lib=n
pvdivxdrmplugin_lib=n
pvdtcpplugin_lib=n


#
# Menu for configuring Media IO
#
pvmiofileinput_lib=m
pvmiofileoutput_lib=m
pvmioaviwavfileinput_lib=m
pvmio_comm_loopback_lib=m


#
# Menu for configuring PacketSources
#
packetsources_default_lib=n

#
# Menu for configuring PacketSource Plugins
#
optimized_bcast_ps_support=n
standard_bcast_ps_support=n


pvmf_lib=m
realaudio_deinterleaver_lib=n
pvdbmanager_lib=n
pvdb_recovery_utility_lib=n


#
# Menu for configuring Engines
#

#
# Menu for configuring Player
#
pvplayer_engine_lib=m

#
# Menu for player engine tunables
#
PVPLAYERENGINE_CONFIG_SKIPTOREQUESTEDPOS_DEF=y
PVPLAYERENGINE_CONFIG_SYNCMARGIN_EARLY_DEF=-200
PVPLAYERENGINE_CONFIG_SYNCMARGIN_LATE_DEF=200
VIDEO_DEC_NODE_LOW_PRIORITY=y
PVPLAYERENGINE_SUPPORT_DTCP=y


#
# Menu for configuring player registry
#
BUILD_OMX_VIDEO_DEC_NODE=y
BUILD_OMX_AUDIO_DEC_NODE=y
BUILD_VIDEO_DEC_NODE=n
BUILD_AVC_DEC_NODE=n
BUILD_WMV_DEC_NODE=n
BUILD_RV_DEC_NODE=n
BUILD_WMA_DEC_NODE=n
BUILD_G726_DEC_NODE=n
BUILD_GSMAMR_DEC_NODE=n
BUILD_AAC_DEC_NODE=n
BUILD_MP3_DEC_NODE=n
BUILD_RA8_DEC_NODE=n
BUILD_MP4_FF_PARSER_NODE=n
BUILD_AMR_FF_PARSER_NODE=y
BUILD_AAC_FF_PARSER_NODE=y
BUILD_MP3_FF_PARSER_NODE=y
BUILD_WAV_FF_PARSER_NODE=y
BUILD_ASF_FF_PARSER_NODE=n
BUILD_RM_FF_PARSER_NODE=n
BUILD_STREAMING_MANAGER_NODE=n
BUILD_DOWNLOAD_MANAGER_NODE=n
BUILD_STILL_IMAGE_NODE=n
BUILD_MP4_FF_REC=n
BUILD_ASF_FF_REC=n
BUILD_OMA1_FF_REC=n
BUILD_AAC_FF_REC=y
BUILD_RM_FF_REC=n
BUILD_MP3_FF_REC=y
BUILD_WAV_FF_REC=y
BUILD_AMR_FF_REC=y
BUILD_DIVX_FF_PARSER_NODE=n
BUILD_DIVX_FF_REC=n
BUILD_PLS_FF_REC=y
BUILD_FLV_FF_PARSER_NODE=n
BUILD_FLV_FF_REC=n



#
# Menu for configuring Author
#
pvauthorengine_lib=m


#
# Menu for configuring pv2way
#
pv2wayengine_lib=m
pv2waysample_lib=m

engines_common_headers_lib=m
pvframemetadatautility_lib=m
pvmetadata_engine_lib=m


#
# Menu for configuring Extern_libs
#
pvmtp_engine_lib=n
pvmtpip_engine_lib=n
pviptransport_lib=n
pvsqlite_lib=n

#
# Menu for configuring Janus wmdrm
#
pvwmdrm_lib=n


#
# Menu for configuring PlayReady wmdrm
#
pvwmdrmmd_lib=n

pvdivxdrm_lib=n
pvdivxdrm_oem_settings_lib=n


#
# That's all, folks!

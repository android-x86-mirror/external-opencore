/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
#ifndef PVMF_MP3FFPARSER_NODE_H_INCLUDED
#define PVMF_MP3FFPARSER_NODE_H_INCLUDED

#ifndef PVMF_NODE_INTERFACE_IMPL_H_INCLUDED
#include "pvmf_node_interface_impl.h"
#endif
#ifndef PVMF_META_DATA_EXTENSION_H_INCLUDED
#include "pvmf_meta_data_extension.h"
#endif
#ifndef PVMF_MEMPOOL_H_INCLUDED
#include "pvmf_mempool.h"
#endif
#ifndef PVMF_RESIZABLE_SIMPLE_MEDIAMSG_H_INCLUDED
#include "pvmf_resizable_simple_mediamsg.h"
#endif
#ifndef PVMF_MP3DEC_DEFS_H_INCLUDED
#include "pvmf_mp3ffparser_defs.h"
#endif
#ifndef PVMF_FFPARSERNODE_PORT_H_INCLUDED
#include "pvmf_mp3ffparser_outport.h"
#endif


#if PV_HAS_SHOUTCAST_SUPPORT_ENABLED // include scsp only when SHOUTCAST support is enabled
#ifndef PVMF_SHOUTCAST_STREAM_PARSER_H_INCLUDED
#include "pvmf_shoutcast_stream_parser.h"
#endif
#endif

#ifndef PVMF_DOWNLOAD_PROGRESS_EXTENSION_H
#include "pvmf_download_progress_interface.h"
#endif
#ifndef IMP3FF_H_INCLUDED
#include "imp3ff.h"  // Includes for the core file format mp3 parser library
#endif
#ifndef PVMF_DURATION_INFOMESSAGE_H_INCLUDED
#include "pvmf_duration_infomessage.h"
#endif
#ifndef PVMF_METADATA_INFOMESSAGE_H_INCLUDED
#include "pvmf_metadata_infomessage.h"
#endif
#ifndef OSCL_MIME_STRING_UTILS_H
#include "pv_mime_string_utils.h"
#endif
#ifndef __MEDIA_CLOCK_CONVERTER_H
#include "media_clock_converter.h"
#endif
#ifndef CPM_H_INCLUDED
#include "cpm.h"
#endif
#ifndef PVMF_CPMPLUGIN_LICENSE_INTERFACE_H_INCLUDED
#include "pvmf_cpmplugin_license_interface.h"
#endif
#ifndef PVMF_LOCAL_DATA_SOURCE_H_INCLUDED
#include "pvmf_local_data_source.h"
#endif
#ifndef PVMF_SOURCE_CONTEXT_DATA_H_INCLUDED
#include "pvmf_source_context_data.h"
#endif
#ifndef PVMF_DATA_SOURCE_INIT_EXTENSION_H_INCLUDED
#include "pvmf_data_source_init_extension.h"
#endif
#ifndef PVMF_TRACK_SELECTION_EXTENSION_H_INCLUDED
#include "pvmf_track_selection_extension.h"
#endif
#ifndef PVMF_DATA_SOURCE_PLAYBACK_CONTROL_H_INCLUDED
#include "pvmf_data_source_playback_control.h"
#endif
#ifndef PVMF_FORMAT_PROGDOWNLOAD_SUPPORT_EXTENSION_H_INCLUDED
#include "pvmf_format_progdownload_support_extension.h"
#endif
#ifndef PVMI_DATASTREAMUSER_INTERFACE_H_INCLUDED
#include "pvmi_datastreamuser_interface.h"
#endif

class PVMFSubNodeContainerBaseMp3
{
    public:
        PVMFSubNodeContainerBaseMp3()
        {
            iCmdState = EIdle;
            iCancelCmdState = EIdle;
            iContainer = NULL;
        }
        virtual ~PVMFSubNodeContainerBaseMp3()
        {
        }
        enum NodeType {ECPM};
        enum CmdType
        {
            //CPM commands
            ECPMCleanup = 0
            , ECPMInit
            , ECPMOpenSession
            , ECPMRegisterContent
            , ECPMGetLicenseInterface
            , ECPMApproveUsage
            , ECPMCheckUsage
            , ECPMUsageComplete
            , ECPMCloseSession
            , ECPMReset
        };

        void Construct(NodeType n, PVMFMP3FFParserNode* c)
        {
            iType = n;
            iContainer = c;
        }

        virtual void Cleanup() = 0;
        virtual PVMFStatus IssueCommand(int32) = 0;
        virtual bool CancelPendingCommand() = 0;

        void CommandDone(PVMFStatus, PVInterface*, OsclAny*);
        void CancelCommandDone(PVMFStatus, PVInterface*, OsclAny*);

        bool CmdPending()
        {
            return iCancelCmdState != EIdle
                   || iCmdState != EIdle;
        }

    protected:
        PVMFMP3FFParserNode*iContainer;
        NodeType iType;

        //Command processing .
        PVMFCommandId iCmdId;
        enum CmdState
        {
            EIdle, //no command
            EBusy //command issued to the sub-node, completion pending.
        };
        CmdState iCmdState;
        int32 iCmd;
        PVMFCommandId iCancelCmdId;
        CmdState iCancelCmdState;
};

class PVMFCPMContainerMp3: public PVMFSubNodeContainerBaseMp3, public PVMFCPMStatusObserver
{
    public:
        PVMFCPMContainerMp3()
        {
            iRequestedUsage.key = NULL;
            iApprovedUsage.key = NULL;
            iAuthorizationDataKvp.key = NULL;
            iCPMContentAccessFactory = NULL;
            iCPM = NULL;
        }

        ~PVMFCPMContainerMp3()
        {
            Cleanup();
        }

        // From PVMFCPMStatusObserver
        OSCL_IMPORT_REF void CPMCommandCompleted(const PVMFCmdResp& aResponse) ;

        //from PVMFSubNodeContainerBase
        PVMFStatus IssueCommand(int32);
        bool CancelPendingCommand();
        void Cleanup();

        //CPM session ID
        PVMFSessionId iSessionId;

        //CPM object
        PVMFCPM* iCPM;

        //CPM data
        PVMFCPMContentType iCPMContentType;
        PVMFCPMPluginAccessInterfaceFactory* iCPMContentAccessFactory;
        PvmiKvp iRequestedUsage;
        PvmiKvp iApprovedUsage;
        PvmiKvp iAuthorizationDataKvp;
        PVMFCPMUsageID iUsageID;
        PVMFCPMPluginLicenseInterface* iCPMLicenseInterface;
        PVInterface* iCPMLicenseInterfacePVI;
        PVMFMetadataExtensionInterface* iCPMMetaDataExtensionInterface;

        PVMFStatus CreateUsageKeys();
        void CleanUsageKeys();
        PVMFStatus CheckApprovedUsage();
        PVMFCommandId GetCPMLicenseInterface();
        bool GetCPMMetaDataExtensionInterface();
};


class PVMP3FFNodeTrackPortInfo
{
    public:
        enum TrackState
        {
            TRACKSTATE_UNINITIALIZED,
            TRACKSTATE_INITIALIZED,
            TRACKSTATE_TRANSMITTING_GETDATA,
            TRACKSTATE_TRANSMITTING_SENDDATA,
            TRACKSTATE_TRANSMITTING_SENDBOS,
            TRACKSTATE_SEND_ENDOFTRACK,
            TRACKSTATE_TRACKDATAPOOLEMPTY,
            TRACKSTATE_MEDIADATAPOOLEMPTY,
            TRACKSTATE_DESTFULL,
            TRACKSTATE_SOURCEEMPTY,
            TRACKSTATE_ENDOFTRACK,
            TRACKSTATE_DOWNLOAD_AUTOPAUSE,
            TRACKSTATE_ERROR
        };

        PVMP3FFNodeTrackPortInfo()
        {

            iClockConverter = NULL;
            iPort = NULL;
            iState = TRACKSTATE_UNINITIALIZED;
            iTrackDataMemoryPool = NULL;
            iMediaDataImplAlloc = NULL;
            iMediaDataMemPool = NULL;
            timestamp_offset = 0;
            iSeqNum = 0;
            iSendBOS = false;
            iFirstFrame = false;
        }

        PVMP3FFNodeTrackPortInfo(const PVMP3FFNodeTrackPortInfo& aSrc)
        {
            iPort = aSrc.iPort;
            iClockConverter = aSrc.iClockConverter;
            iMediaData = aSrc.iMediaData;
            iState = aSrc.iState;
            iTrackDataMemoryPool = aSrc.iTrackDataMemoryPool;
            iMediaDataImplAlloc = aSrc.iMediaDataImplAlloc;
            iMediaDataMemPool = aSrc.iMediaDataMemPool;
            timestamp_offset = aSrc.timestamp_offset;
            iSeqNum = aSrc.iSeqNum;
            iSendBOS = aSrc.iSendBOS;
            iFirstFrame = aSrc.iFirstFrame;
        }

        ~PVMP3FFNodeTrackPortInfo()
        {
        }

        // Output port to send the data downstream
        PVMFMP3FFParserPort* iPort;

        // Shared memory pointer holding the currently retrieved track data
        PVMFSharedMediaDataPtr iMediaData;

        // Current state of this track
        TrackState iState;

        // Output buffer memory pool
        OsclMemPoolResizableAllocator *iTrackDataMemoryPool;

        // Allocator for simple media data buffer impl
        PVMFResizableSimpleMediaMsgAlloc *iMediaDataImplAlloc;

        // Memory pool for simple media data
        PVMFMemPoolFixedChunkAllocator *iMediaDataMemPool;

        // Converter to convert from track timescale to milliseconds
        MediaClockConverter* iClockConverter;


        PVMFFormatType iFormatType;
        uint32 iBitrate;
        int32 timestamp_offset;

        // Sequence number
        uint32 iSeqNum;
        //bos flag
        bool iSendBOS;
        // Random access point idenfier
        bool iFirstFrame;
};  // end class PVMP3FFNodeTrackPortInfo

/*
* The class PVMp3DurationCalculator is the external interface for calculating
* the clip duration as a background AO.
**/
class PVMp3DurationCalculator : public OsclTimerObject
{
    public:
        PVMp3DurationCalculator(int32 aPriority, IMpeg3File* aMP3File, PVMFNodeInterface* aNode, bool aScanEnabled = true);
        ~PVMp3DurationCalculator();
        void Run();
        void ScheduleAO();
    private:
        PVFile* iFile;
        bool iScanComplete;
        bool iScanEnabled;
        MP3ErrorType iErrorCode;
        IMpeg3File* iMP3File;
        PVMFNodeInterface* iNode;
        uint32 totalticks;
};

/*
* The class PVMFMP3FFParserNode is the external interface for using the node.
* Move this to a separate interface file and all the stuff above can go in a
* private header in the src directory.
* */

class PVMFMP3FFParserNode : public PVMFNodeInterfaceImpl,
        public PVMFDataSourceInitializationExtensionInterface,
        public PVMFTrackSelectionExtensionInterface,
        public PVMFMetadataExtensionInterface,
        public OsclMemPoolFixedChunkAllocatorObserver,
        public PVMFFormatProgDownloadSupportInterface,
        public PvmiDataStreamObserver,
        public PVMIDatastreamuserInterface,
        public OsclMemPoolResizableAllocatorObserver,
        public PvmfDataSourcePlaybackControlInterface
#if PV_HAS_SHOUTCAST_SUPPORT_ENABLED
        , public PVMFMetadataUpdatesObserver
#endif
{
    public:
        PVMFMP3FFParserNode(int32 aPriority);
        ~PVMFMP3FFParserNode();

        PVMFStatus QueryInterfaceSync(PVMFSessionId aSession,
                                      const PVUuid& aUuid,
                                      PVInterface*& aInterfacePtr);

        //From PVMFDataSourceInitializationExtensionInterface
        void addRef();
        void removeRef();
        bool queryInterface(const PVUuid& uuid, PVInterface *& iface);

        PVMFStatus SetSourceInitializationData(OSCL_wString& aSourceURL, PVMFFormatType& aSourceFormat, OsclAny* aSourceData, PVMFFormatTypeDRMInfo aType = PVMF_FORMAT_TYPE_CONNECT_DRM_INFO_UNKNOWN);
        PVMFStatus SetClientPlayBackClock(PVMFMediaClock* aClientClock);
        PVMFStatus SetEstimatedServerClock(PVMFMediaClock* aClientClock);

        //From PVMFTrackSelectionExtensionInterface
        PVMFStatus GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo);
        PVMFStatus SelectTracks(PVMFMediaPresentationInfo& aInfo);

#if PV_HAS_SHOUTCAST_SUPPORT_ENABLED
        //From PVMFMetadataUpdatesObserver
        void MetadataUpdated(uint32 aMetadataSize);
#endif
        // From PVMFMetadataExtensionInterface
        uint32 GetNumMetadataKeys(char* aQueryKeyString = NULL);
        uint32 GetNumMetadataValues(PVMFMetadataList& aKeyList);
        PVMFCommandId GetNodeMetadataKeys(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList,
                                          uint32 aStartingKeyIndex, int32 aMaxValueEntries, char* aQueryKeyString = NULL, const OsclAny* aContextData = NULL);
        PVMFCommandId GetNodeMetadataValues(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList,
                                            Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 aStartingValueIndex, int32 aMaxValueEntries, const OsclAny* aContextData = NULL);
        PVMFStatus ReleaseNodeMetadataKeys(PVMFMetadataList& aKeyList, uint32 aStartingKeyIndex, uint32 aEndKeyIndex);
        PVMFStatus ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 aStartingValueIndex, uint32 aEndValueIndex);

        // From PvmfDataSourcePlaybackControlInterface
        virtual PVMFCommandId SetDataSourcePosition(PVMFSessionId aSessionId,
                PVMFTimestamp aTargetNPT,
                PVMFTimestamp& aActualNPT,
                PVMFTimestamp& aActualMediaDataTS,
                bool aSeekToSyncPoint = true,
                uint32 aStreamID = 0,
                OsclAny* aContext = NULL);

        virtual PVMFCommandId QueryDataSourcePosition(PVMFSessionId aSessionId,
                PVMFTimestamp aTargetNPT,
                PVMFTimestamp& aActualNPT,
                bool aSeekToSyncPoint = true,
                OsclAny* aContext = NULL);

        virtual PVMFCommandId QueryDataSourcePosition(PVMFSessionId aSessionId,
                PVMFTimestamp aTargetNPT,
                PVMFTimestamp& aSeekPointBeforeTargetNPT,
                PVMFTimestamp& aSeekPointAfterTargetNPT,
                OsclAny* aContext = NULL,
                bool aSeekToSyncPoint = true);

        virtual PVMFCommandId SetDataSourceRate(PVMFSessionId aSessionId,
                                                int32 aRate,
                                                PVMFTimebase* aTimebase = NULL,
                                                OsclAny* aContext = NULL);

        // From PVMFFormatProgDownloadSupportInterface
        int32 convertSizeToTime(uint32 fileSize, uint32& aNPTInMS);
        bool setProtocolInfo(Oscl_Vector<PvmiKvp*, OsclMemAllocator>& aInfoKvpVec);
        void setFileSize(const uint32 aFileSize);
        void setDownloadProgressInterface(PVMFDownloadProgressInterface* download_progress);
        void playResumeNotification(bool aDownloadComplete);
        void notifyDownloadComplete()
        {
            playResumeNotification(true);
        };

        // From PVMIDatastreamuserInterface
        void PassDatastreamFactory(PVMFDataStreamFactory& aFactory,
                                   int32 aFactoryTag,
                                   const PvmfMimeString* aFactoryConfig = NULL);

        void PassDatastreamReadCapacityObserver(PVMFDataStreamReadCapacityObserver* aObserver);


    protected:
        PVMFCPMContainerMp3 iCPMContainer;

    private:
        void Push(PVMFSubNodeContainerBaseMp3&, PVMFSubNodeContainerBaseMp3::CmdType);

        PVMFStatus CheckForMP3HeaderAvailability();
        PVMFStatus GetFileOffsetForAutoResume(uint32& aOffset, PVMP3FFNodeTrackPortInfo* aTrackPortInfo);

#if PV_HAS_SHOUTCAST_SUPPORT_ENABLED
        PVMFStatus ParseShoutcastMetadata(char* aMetadataBuf, uint32 aMetadataSize, Oscl_Vector<PvmiKvp, OsclMemAllocator>& aKvpVector);
#endif

        void Construct();

        //from OsclActiveObject
        virtual void Run();

        // Port processing
        PVMFStatus ProcessOutgoingMsg(PVMFPortInterface* aPort);
        bool HandleOutgoingQueueReady(PVMFPortInterface* aPortInterface);

        //Command processing
        void CompleteInit(PVMFStatus aStatus);

        //Command dispatchers
        PVMFStatus HandleExtensionAPICommands();
        PVMFStatus CancelCurrentCommand();

        //Command handlers for PVMFNodeInterface APIs.
        PVMFStatus DoReset();
        PVMFStatus DoQueryUuid();
        PVMFStatus DoQueryInterface();
        PVMFStatus DoRequestPort(PVMFPortInterface*&);
        PVMFStatus DoReleasePort();
        PVMFStatus DoInit();
        PVMFStatus DoPrepare();
        PVMFStatus DoStart();
        PVMFStatus DoStop();
        PVMFStatus DoFlush();
        PVMFStatus DoGetNodeMetadataKeys();
        PVMFStatus DoGetNodeMetadataValues();
        PVMFStatus DoSetDataSourceRate();
        PVMFStatus DoSetDataSourcePosition();
        PVMFStatus DoQueryDataSourcePosition();

        bool HandleTrackState();
        bool RetrieveTrackData(PVMP3FFNodeTrackPortInfo& aTrackPortInfo);
        bool SendTrackData(PVMP3FFNodeTrackPortInfo& aTrackPortInfo);

        int32 FindTrackID(PVMFFormatType aFormatType);
        PVMFStatus ParseFile();
        bool CreateFormatSpecificInfo(uint32 numChannels, uint32 samplingRate);

        void ResetTrack();
        void ReleaseTrack();
        void RemoveAllCommands();
        void CleanupFileSource();

        //from PVMFPortActivityHandler
        void HandlePortActivity(const PVMFPortActivity& aActivity);

        // From OsclMemPoolFixedChunkAllocatorObserver
        void freechunkavailable(OsclAny*);
        // From OsclMemPoolResizableAllocatorObserver
        void freeblockavailable(OsclAny*);

        // From PvmiDataStreamObserver
        void DataStreamCommandCompleted(const PVMFCmdResp& aResponse);
        void DataStreamInformationalEvent(const PVMFAsyncEvent& aEvent);
        void DataStreamErrorEvent(const PVMFAsyncEvent& aEvent);
        PVMFStatus CreateMP3FileObject(MP3ErrorType &aSuccess, PVMFCPMPluginAccessInterfaceFactory*aCPM);
        PVMFStatus SetupParserObject();
        PVMFStatus PushBackCPMMetadataKeys(PVMFMetadataList *&aKeyListPtr, uint32 aLcv);

// private member variables

#if PV_HAS_SHOUTCAST_SUPPORT_ENABLED
        // shoutcast related
        int32 iClipByteRate;
        int32 iMetadataBufSize;
        uint32 iMetadataSize;
        int32 iMetadataInterval;
        Oscl_Vector<PvmiKvp, OsclMemAllocator> iMetadataVector;

        PVMFShoutcastStreamParserFactory* iSCSPFactory;
        PVMFShoutcastStreamParser* iSCSP;
        uint8 *iMetadataBuf;
#endif

        // All one port is required as we support only one track
        PVMFMP3FFParserPort* iOutPort;

        // Parse status
        bool iParseStatus;

        OSCL_wHeapString<OsclMemAllocator> iSourceURL;
        bool iSourceURLSet;
        PVMFFormatType iSourceFormat;
        PVMFSourceContextData iSourceContextData;
        bool iSourceContextDataValid;
        OsclFileHandle* iFileHandle;
        PVMFLocalDataSource iCPMSourceData;
        Oscl_FileServer iFileServer;
        IMpeg3File* iMP3File;
        uint32 iConfigOk;
        int iMaxFrameSize;
        PVMP3FFNodeTrackPortInfo iTrack; //The track that this node is streaming. Current assumption is one track supported per node.
        uint32 iMP3FormatBitrate;
        bool iFileSizeRecvd;
        uint32 iFileSize;
        // Logger
        PVLogger *iLogger;

        // Channel sample info stored in a OsclRefCounterMemFrag
        OsclMemAllocDestructDealloc<uint8> iDecodeFormatSpecificInfoAlloc;
        OsclRefCounterMemFrag iDecodeFormatSpecificInfo;
        bool iSendDecodeFormatSpecificInfo;

        /* These vars are used for the prog. download to auto pause*/
        OsclSharedPtr<PVMFMediaClock> iDownloadProgressClock;
        PVMFDownloadProgressInterface* iDownloadProgressInterface;
        bool iAutoPaused;
        bool iDownloadComplete;
        PvmiDataStreamCommandId iRequestReadCapacityNotificationID;
        uint32 iMP3MetaDataSize;

        // Data Stream vars
        PVMIDataStreamSyncInterface* iDataStreamInterface;
        PVMFDataStreamFactory* iDataStreamFactory;
        PVMFDataStreamReadCapacityObserver* iDataStreamReadCapacityObserver;
        PvmiDataStreamSession iDataStreamSessionID;

        //metadata related
        uint32 iMP3ParserNodeMetadataValueCount;
        Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> iCPMMetadataKeys;
        PVMFStatus CompleteGetMetadataKeys();

        //for CPM
        bool oWaitingOnLicense;
        PVMFCommandId iCPMGetMetaDataKeysCmdId;
        PVMFCommandId iCPMGetMetaDataValuesCmdId;
        PVMFCommandId iCPMUsageCompleteCmdId;
        PVMFCommandId iCPMCloseSessionCmdId;
        PVMFCommandId iCPMResetCmdId;
        bool iUseCPMPluginRegistry;
        class SubNodeCmd
        {
            public:
                PVMFSubNodeContainerBaseMp3* iSubNodeContainer;
                PVMFSubNodeContainerBaseMp3::CmdType iCmd;
        };
        Oscl_Vector<SubNodeCmd, OsclMemAllocator> iSubNodeCmdVec;

        void GetCPMMetaDataKeys();

        PVMp3DurationCalculator* iDurationCalcAO;
        bool iCheckForMP3HeaderDuringInit;

        friend class PVMFSubNodeContainerBaseMp3;
        friend class PVMFCPMContainerMp3;
        friend class PVMFMP3FFParserPort;
        friend class PVMp3DurationCalculator;
};

#endif // PVMF_MP3FFPARSER_NODE_H_INCLUDED


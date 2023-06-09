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
#include "pvmf_downloadmanager_node.h"
#include "pvmf_download_data_source.h"
#include "pvmf_protocol_engine_factory.h"
#include "pvmf_socket_factory.h"
#include "pvmf_socket_node.h"
#include "pvmi_datastreamuser_interface.h"
#include "pvpvxparser.h"
#include "pv_mime_string_utils.h"
#include "pvmi_kvp_util.h"

//Log levels for node commands
#define CMD_LOG_LEVEL PVLOGMSG_INFO
//Log levels for subnode commands.
#define SUB_CMD_LOG_LEVEL PVLOGMSG_INFO

#define LOGSUBCMD(x) PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, SUB_CMD_LOG_LEVEL, x)
#define GETNODESTR (iType==EFormatParser)?"Parser":((iType==EProtocolEngine)?"ProtEngine":"SockNode")

///////////////////////////////////////////////////////////////////////////////
//
// Capability and config interface related constants and definitions
//   - based on pv_player_engine.h
//
///////////////////////////////////////////////////////////////////////////////

static const DownloadManagerKeyStringData DownloadManagerConfig_BaseKeys[] =
{
    {"user-agent", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_WCHARPTR},
    {"http-version", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_UINT32},
    {"http-timeout", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_UINT32},
    {"download-progress-info", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_CHARPTR},
    {"protocol-extension-header", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_CHARPTR},
    {"num-redirect-attempts", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_UINT32},
    {"http-header-request-disabled", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_BOOL},
    {"max-tcp-recv-buffer-size-download", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_UINT32},
    {"max-tcp-recv-buffer-count-download", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_UINT32}
};

static const uint DownloadManagerConfig_NumBaseKeys =
    (sizeof(DownloadManagerConfig_BaseKeys) /
     sizeof(DownloadManagerKeyStringData));

enum BaseKeys_IndexMapType
{
    BASEKEY_SESSION_CONTROLLER_USER_AGENT = 0,
    BASEKEY_SESSION_CONTROLLER_HTTP_VERSION,
    BASEKEY_SESSION_CONTROLLER_HTTP_TIMEOUT,
    BASEKEY_SESSION_CONTROLLER_DOWNLOAD_PROGRESS_INFO,
    BASEKEY_SESSION_CONTROLLER_PROTOCOL_EXTENSION_HEADER,
    BASEKEY_SESSION_CONTROLLER_NUM_REDIRECT_ATTEMPTS,
    BASEKEY_SESSION_CONTROLLER_NUM_HTTP_HEADER_REQUEST_DISABLED,
    BASEKEY_MAX_TCP_RECV_BUFFER_SIZE,
    BASEKEY_MAX_TCP_RECV_BUFFER_COUNT
};



PVMFDownloadManagerNode::PVMFDownloadManagerNode(int32 aPriority)
        : PVMFNodeInterfaceImpl(aPriority, "PVMFDownloadManagerNode")
{
    int32 err;
    OSCL_TRY(err, ConstructL(););
    if (err != OsclErrNone)
    {
        //if a leave happened, cleanup and re-throw the error
        iNodeCapability.iInputFormatCapability.clear();
        iNodeCapability.iOutputFormatCapability.clear();
        OSCL_CLEANUP_BASE_CLASS(PVMFNodeInterfaceImpl);
        OSCL_LEAVE(err);
    }

    iDNodeUuids.clear();
    iDNodeUuidCount = 0;
}

void PVMFDownloadManagerNode::ConstructL()
{
    iDebugMode = false;
    iSourceFormat = PVMF_MIME_FORMAT_UNKNOWN;
    iMimeType = PVMF_MIME_FORMAT_UNKNOWN;
    iSourceData = NULL;
    iPlayBackClock = NULL;
    iClockNotificationsInf = NULL;

    iNoPETrackSelect = false;
    iMovieAtomComplete = false;
    iParserInitAfterMovieAtom = false;
    iParserPrepareAfterMovieAtom = false;

    iParserInit = false;
    iDataReady = false;
    iDownloadComplete = false;
    iRecognizerError = false;

    iInitFailedLicenseRequired = false;

    iProtocolEngineNodePort = NULL;
    iSocketNodePort = NULL;
    iPlayerNodeRegistry = NULL;

    //create the sub-node command queue.  Use a reserve to avoid dynamic memory failure later.
    //Max depth is the max number of sub-node commands for any one node command. Init command may take up to 15
    iSubNodeCmdVec.reserve(15);


    //create node containers.
    //@TODO this will create unused node containers.  think about
    //optimizing it.
    iFormatParserNode.Construct(PVMFDownloadManagerSubNodeContainerBase::EFormatParser, this);
    iProtocolEngineNode.Construct(PVMFDownloadManagerSubNodeContainerBase::EProtocolEngine, this);
    iSocketNode.Construct(PVMFDownloadManagerSubNodeContainerBase::ESocket, this);
    iRecognizerNode.Construct(PVMFDownloadManagerSubNodeContainerBase::ERecognizer, this);
    iCPMNode.Construct(PVMFDownloadManagerSubNodeContainerBase::ECPM, this);

    //Set the node capability data.
    iNodeCapability.iCanSupportMultipleInputPorts = false;
    iNodeCapability.iCanSupportMultipleOutputPorts = true;
    iNodeCapability.iHasMaxNumberOfPorts = true;
    iNodeCapability.iMaxNumberOfPorts = 6;
    iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_MPEG4FF);
    iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_ASFFF);
    iNodeCapability.iInputFormatCapability.push_back(PVMF_MIME_RMFF);
    iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_AMR_IETF);
    iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_MPEG4_AUDIO);
    iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_M4V);
    iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_H2631998);
    iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_H2632000);
    iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_REAL_VIDEO);
    iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_WMV);
    iNodeCapability.iOutputFormatCapability.push_back(PVMF_MIME_DIVXFF);

    iFileBufferDatastreamFactory = NULL;
#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
    iMemoryBufferDatastreamFactory = NULL;
#if(PV_HAS_SHOUTCAST_SUPPORT_ENABLED)
    iPLSSessionContextData = NULL;
#endif//PV_HAS_SHOUTCAST_SUPPORT_ENABLED
#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB

    iDownloadFileName = NULL;
    iContentTypeMIMEString = NULL;

    iProtocolEngineNode.iNode = PVMFProtocolEngineNodeFactory::CreatePVMFProtocolEngineNode(OsclActiveObject::EPriorityNominal);
    OsclError::LeaveIfNull(iProtocolEngineNode.iNode);
    iProtocolEngineNode.Connect();

    iSocketNode.iNode = PVMFSocketNodeFactory::CreatePVMFSocketNode(OsclActiveObject::EPriorityNominal);
    OsclError::LeaveIfNull(iSocketNode.iNode);
    iSocketNode.Connect();
}

PVMFDownloadManagerNode::~PVMFDownloadManagerNode()
{
    if (iPlayBackClock != NULL)
    {
        if (iClockNotificationsInf != NULL)
        {
            iClockNotificationsInf->RemoveClockStateObserver(*this);
            iPlayBackClock->DestroyMediaClockNotificationsInterface(iClockNotificationsInf);
        }
    }

    Cancel();
    if (IsAdded())
        RemoveFromScheduler();

    //if any sub-node commands are outstanding, there will be
    //a crash when they callback-- so panic here instead.

    if (iFormatParserNode.CmdPending() ||
            iProtocolEngineNode.CmdPending() ||
            iSocketNode.CmdPending() ||
            iRecognizerNode.CmdPending() ||
            iCPMNode.CmdPending())
    {
        OSCL_ASSERT(0);
    }

    //this is to ensure that there are no more callbacks from PE node to parser node,
    //in case parser node had some outstanding request resume notifications
    if (iProtocolEngineNode.DownloadProgress() != NULL)
    {
        (iProtocolEngineNode.DownloadProgress())->setFormatDownloadSupportInterface(NULL);
    }

    //make sure the subnodes got cleaned up
    iFormatParserNode.Cleanup();
    iProtocolEngineNode.Cleanup();
    iSocketNode.Cleanup();
    iRecognizerNode.Cleanup();
    iCPMNode.Cleanup();

    //delete the subnodes
    if (iFormatParserNode.iNode)
    {
        iDNodeUuidCount--;

        bool release_status = false;
        int32 leavecode = 0;
        OSCL_TRY(leavecode, release_status = iPlayerNodeRegistry->ReleaseNode(iDNodeUuids[iDNodeUuidCount], iFormatParserNode.iNode));
        //ignore errors.
        iDNodeUuids.clear();
    }

    if (iProtocolEngineNode.iNode)
        PVMFProtocolEngineNodeFactory::DeletePVMFProtocolEngineNode(iProtocolEngineNode.iNode);

    if (iSocketNode.iNode)
        PVMFSocketNodeFactory::DeletePVMFSocketNode(iSocketNode.iNode);

    // delete the data stream factory (This has to come after deleting anybody who uses it, like the protocol engine node or the parser node.)
    if (iFileBufferDatastreamFactory)
    {
        PVMF_BASE_NODE_DELETE(iFileBufferDatastreamFactory);
        iFileBufferDatastreamFactory = NULL;
    }
#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
    if (iMemoryBufferDatastreamFactory)
    {
        PVMF_BASE_NODE_DELETE(iMemoryBufferDatastreamFactory);
        iMemoryBufferDatastreamFactory = NULL;
    }
#if(PV_HAS_SHOUTCAST_SUPPORT_ENABLED)
    if (iPLSSessionContextData != NULL)
    {
        OSCL_DELETE(iPLSSessionContextData);
        iPLSSessionContextData = NULL;
    }
#endif//PV_HAS_SHOUTCAST_SUPPORT_ENABLED
#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB

}

PVMFStatus PVMFDownloadManagerNode::ThreadLogon()
{
    // logon the node
    PVMFStatus status = PVMFNodeInterfaceImpl::ThreadLogon();

    if (status != PVMFSuccess)
        return status;

    iLogger = PVLogger::GetLoggerObject("pvdownloadmanagernode");

    //logon the sub-nodes.
    if (iProtocolEngineNode.iNode)
        iProtocolEngineNode.iNode->ThreadLogon();

    if (iSocketNode.iNode)
        iSocketNode.iNode->ThreadLogon();

    return PVMFSuccess;
}


//Public API From node interface.
PVMFStatus PVMFDownloadManagerNode::ThreadLogoff()
{
    iLogger = NULL;

    //logoff the sub-nodes.
    if (iFormatParserNode.iNode)
        iFormatParserNode.iNode->ThreadLogoff();
    if (iProtocolEngineNode.iNode)
        iProtocolEngineNode.iNode->ThreadLogoff();
    if (iSocketNode.iNode)
        iSocketNode.iNode->ThreadLogoff();

    // logoff the node
    PVMFStatus status = PVMFNodeInterfaceImpl::ThreadLogoff();

    if (status != PVMFSuccess)
        return status;

    return PVMFSuccess;
}

//Public API From node interface.
PVMFPortIter* PVMFDownloadManagerNode::GetPorts(const PVMFPortFilter* aFilter)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, CMD_LOG_LEVEL, (0, "PVMFDownloadManagerNode::GetPorts() called"));

    if (iFormatParserNode.iNode)
        return iFormatParserNode.iNode->GetPorts(aFilter);
    return NULL;
}

//public API from PVInterface
void PVMFDownloadManagerNode::addRef()
{
    ++iExtensionRefCount;
}

//public API from PVInterface
void PVMFDownloadManagerNode::removeRef()
{
    --iExtensionRefCount;
}

//public API from PVInterface
bool PVMFDownloadManagerNode::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::queryInterface() In"));

    if (uuid == PVMF_TRACK_SELECTION_INTERFACE_UUID)
    {
        PVMFTrackSelectionExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFTrackSelectionExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMF_DATA_SOURCE_INIT_INTERFACE_UUID)
    {
        PVMFDataSourceInitializationExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFDataSourceInitializationExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == KPVMFMetadataExtensionUuid)
    {
        PVMFMetadataExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFMetadataExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMF_DATA_SOURCE_NODE_REGISRTY_INIT_INTERFACE_UUID)
    {
        PVMFDataSourceNodeRegistryInitInterface* myInterface =
            OSCL_STATIC_CAST(PVMFDataSourceNodeRegistryInitInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PvmfDataSourcePlaybackControlUuid)
    {
        PvmfDataSourcePlaybackControlInterface* myInterface = OSCL_STATIC_CAST(PvmfDataSourcePlaybackControlInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMFCPMPluginLicenseInterfaceUuid)
    {
        iface = OSCL_STATIC_CAST(PVInterface*, iFormatParserNode.iLicenseInterface);
    }
    else
    {
        return false;
    }

    // PVMFCPMPluginLicenseInterface does not belong to this node
    if (uuid != PVMFCPMPluginLicenseInterfaceUuid)
    {
        ++iExtensionRefCount;
    }
    return true;
}


//public API from data source initialization interface
PVMFStatus PVMFDownloadManagerNode::SetSourceInitializationData(OSCL_wString& aSourceURL, PVMFFormatType& aSourceFormat, OsclAny* aSourceData, PVMFFormatTypeDRMInfo aType)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::SetSourceInitializationData() called"));

#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
#if(PV_HAS_SHOUTCAST_SUPPORT_ENABLED)
    if (aSourceFormat == PVMF_MIME_PLSFF)
    {
        // parse the PLS file
        // fill in iSourceURL, iSourceFormat and iSourceData
        PVMFStatus status = ParsePLSFile(aSourceURL);
        if (status != PVMFSuccess)
        {
            return status;
        }
    }
    else
    {
#endif //PV_HAS_SHOUTCAST_SUPPORT_ENABLED
#endif //PVMF_DOWNLOADMANAGER_SUPPORT_PPB
        iSourceURL = aSourceURL;
        iSourceFormat = aSourceFormat;
        iSourceData = aSourceData;

#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
#if(PV_HAS_SHOUTCAST_SUPPORT_ENABLED)
    }
#endif //PV_HAS_SHOUTCAST_SUPPORT_ENABLED
#endif //PVMF_DOWNLOADMANAGER_SUPPORT_PPB

    // Pass the source info directly to the protocol engine node.
    if (!iProtocolEngineNode.DataSourceInit())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                        "PVMFDownloadManagerNode:SetSourceInitializationData() Can't find datasourceinit interface in protocol engine subnode container."));
        return PVMFFailure; //no source init interface.
    }
    PVMFStatus status = (iProtocolEngineNode.DataSourceInit())->SetSourceInitializationData(iSourceURL, iSourceFormat, iSourceData);
    if (status != PVMFSuccess)
        return status;

    if (!iProtocolEngineNode.ProtocolEngineExtension())
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                        "PVMFDownloadManagerNode:SetSourceInitializationData() Can't get ProtocolEngineExtension interface from protocol subnode container."));
        return PVMFFailure; //no ProtocolNodeExtension interface.
    }

    bool socketConfigOK = (iProtocolEngineNode.ProtocolEngineExtension())->GetSocketConfig(iServerAddr);
    if (!socketConfigOK)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                        "PVMFDownloadManagerNode: SetSourceInitializationData() Call to GetSocketConfig() on protocol engine node returned failure."));
        return PVMFErrProcessing;
    }

    if (aSourceFormat == PVMF_MIME_DATA_SOURCE_HTTP_URL ||
            aSourceFormat == PVMF_MIME_DATA_SOURCE_RTMP_STREAMING_URL ||
            aSourceFormat == PVMF_MIME_DATA_SOURCE_DTCP_URL)
    {
        if (!iSourceData)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                            "PVMFDownloadManagerNode:SetSourceInitializationData() Missing source data"));
            return PVMFErrArgument;
        }
        PVInterface* pvinterface = (PVInterface*)iSourceData;
        PVUuid uuid(PVMF_DOWNLOAD_DATASOURCE_HTTP_UUID);
        PVInterface* temp = NULL;
        if (pvinterface->queryInterface(uuid, temp))
        {
            PVMFDownloadDataSourceHTTP* data = OSCL_STATIC_CAST(PVMFDownloadDataSourceHTTP*, temp);
            //extract the download file name from the opaque data.
            iDownloadFileName = data->iDownloadFileName;

            //extract the playback mode
            switch (data->iPlaybackControl)
            {
                case PVMFDownloadDataSourceHTTP::ENoPlayback:
                    iPlaybackMode = EDownloadOnly;
                    break;
                case PVMFDownloadDataSourceHTTP::EAfterDownload:
                    iPlaybackMode = EDownloadThenPlay;
                    break;
                case PVMFDownloadDataSourceHTTP::ENoSaveToFile:
#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
                    iPlaybackMode = EPlaybackOnly;
                    break;
#else
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                                    "PVMFDownloadManagerNode:SetSourceInitializationData() NoSaveToFile is not supported!"));
                    return PVMFErrArgument;//unsupported mode.
#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB
                case PVMFDownloadDataSourceHTTP::EAsap:
                default:
                    iPlaybackMode = EPlayAsap;
                    break;
            }
        }
        else
        {
            PVUuid uuid(PVMF_SOURCE_CONTEXT_DATA_DOWNLOAD_HTTP_UUID);
            temp = NULL;
            if (pvinterface->queryInterface(uuid, temp))
            {
                PVMFSourceContextDataDownloadHTTP* data = OSCL_STATIC_CAST(PVMFSourceContextDataDownloadHTTP*, temp);
                //extract the download file name from the opaque data.
                iDownloadFileName = data->iDownloadFileName;

                //extract the playback mode
                switch (data->iPlaybackControl)
                {
                    case PVMFSourceContextDataDownloadHTTP::ENoPlayback:
                        iPlaybackMode = EDownloadOnly;
                        break;
                    case PVMFSourceContextDataDownloadHTTP::EAfterDownload:
                        iPlaybackMode = EDownloadThenPlay;
                        break;
                    case PVMFSourceContextDataDownloadHTTP::ENoSaveToFile:
#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
                        iPlaybackMode = EPlaybackOnly;
                        break;
#else
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                                        "PVMFDownloadManagerNode:SetSourceInitializationData() NoSaveToFile is not supported!"));
                        return PVMFErrArgument;//unsupported mode.
#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB
                    case PVMFSourceContextDataDownloadHTTP::EAsap:
                    default:
                        iPlaybackMode = EPlayAsap;
                        break;
                }
            }
            else
            {//invalid source data
                return PVMFErrArgument;
            }
        }
    }
    else if (iSourceFormat == PVMF_MIME_DATA_SOURCE_PVX_FILE)
    {
        if (!iSourceData)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                            "PVMFDownloadManagerNode:SetSourceInitializationData() Missing source data"));
            return PVMFErrArgument;
        }
        PVInterface* pvinterface = (PVInterface*)iSourceData;
        PVUuid uuid(PVMF_DOWNLOAD_DATASOURCE_PVX_UUID);
        PVInterface* temp = NULL;
        if (pvinterface->queryInterface(uuid, temp))
        {
            PVMFDownloadDataSourcePVX* data = OSCL_STATIC_CAST(PVMFDownloadDataSourcePVX*, temp);
            iDownloadFileName = data->iDownloadFileName;
            //get the playback mode from the PVX info
            switch (data->iPvxInfo.iPlaybackControl)
            {
                case CPVXInfo::ENoPlayback:
                    iPlaybackMode = EDownloadOnly;
                    break;
                case CPVXInfo::EAfterDownload:
                    iPlaybackMode = EDownloadThenPlay;
                    break;
                case CPVXInfo::EAsap:
                default:
                    iPlaybackMode = EPlayAsap;
                    break;
            }
        }
        else
        {
            PVUuid uuid(PVMF_SOURCE_CONTEXT_DATA_DOWNLOAD_PVX_UUID);

            if (pvinterface->queryInterface(uuid, temp))
            {
                PVMFSourceContextDataDownloadPVX* data = OSCL_STATIC_CAST(PVMFSourceContextDataDownloadPVX*, temp);
                iDownloadFileName = data->iDownloadFileName;
                if (!data->iPvxInfo)
                {//invalid source data
                    return PVMFErrArgument;
                }
                //get the playback mode from the PVX info
                switch (data->iPvxInfo->iPlaybackControl)
                {
                    case CPVXInfo::ENoPlayback:
                        iPlaybackMode = EDownloadOnly;
                        break;
                    case CPVXInfo::EAfterDownload:
                        iPlaybackMode = EDownloadThenPlay;
                        break;
                    case CPVXInfo::EAsap:
                    default:
                        iPlaybackMode = EPlayAsap;
                        break;
                }
            }
            else
            {//invalid source data
                return PVMFErrArgument;
            }
        }
    }
    else if (iSourceFormat == PVMF_MIME_DATA_SOURCE_SHOUTCAST_URL)
    {
        if (!iSourceData)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                            "PVMFDownloadManagerNode:SetSourceInitializationData() Missing source data"));
            return PVMFErrArgument;
        }
        PVInterface* pvinterface = (PVInterface*)iSourceData;
        PVUuid uuid(PVMF_DOWNLOAD_DATASOURCE_HTTP_UUID);
        PVInterface* temp = NULL;
        if (pvinterface->queryInterface(uuid, temp))
        {
            PVMFDownloadDataSourceHTTP* data = OSCL_STATIC_CAST(PVMFDownloadDataSourceHTTP*, temp);

            //extract the download file name from the opaque data.
            iDownloadFileName = data->iDownloadFileName;

            //extract the playback mode
            switch (data->iPlaybackControl)
            {
                case PVMFDownloadDataSourceHTTP::ENoSaveToFile:

#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
                    iPlaybackMode = EPlaybackOnly;
                    break;
#else
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                                    "PVMFDownloadManagerNode:SetSourceInitializationData() NoSaveToFile is not supported!"));
                    return PVMFErrArgument;//unsupported mode.
#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB

                default:

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                                    "PVMFDownloadManagerNode:SetSourceInitializationData() Only NoSaveToFile mode is supported for PVMF_MIME_DATA_SOURCE_SHOUTCAST_URL!"));
                    return PVMFErrArgument;//unsupported mode.
                    break;
            }
        }
        else
        {
            PVUuid uuid(PVMF_SOURCE_CONTEXT_DATA_DOWNLOAD_HTTP_UUID);
            if (pvinterface->queryInterface(uuid, temp))
            {
                PVMFSourceContextDataDownloadHTTP* data = OSCL_STATIC_CAST(PVMFSourceContextDataDownloadHTTP*, temp);
                //extract the download file name from the opaque data.
                iDownloadFileName = data->iDownloadFileName;

                //extract the playback mode
                switch (data->iPlaybackControl)
                {
                    case PVMFSourceContextDataDownloadHTTP::ENoSaveToFile:

#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
                        iPlaybackMode = EPlaybackOnly;
                        break;
#else
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                                        "PVMFDownloadManagerNode:SetSourceInitializationData() NoSaveToFile is not supported!"));
                        return PVMFErrArgument;//unsupported mode.
#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB

                    default:
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                                        "PVMFDownloadManagerNode:SetSourceInitializationData() Only NoSaveToFile mode is supported for PVMF_MIME_DATA_SOURCE_SHOUTCAST_URL!"));
                        return PVMFErrArgument;//unsupported mode.
                        break;
                }
            }
            else
            {   //invalid source data
                return PVMFErrArgument;
            }
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                        "PVMFDownloadManagerNode:SetSourceInitializationData() Unsupported source type"));
        return PVMFErrArgument;
    }

#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
    int32 leavecode;
    //Configure the MBDS
    if (iPlaybackMode == EPlaybackOnly)
    {
        // make sure we have enough TCP buffers for PPB and shoutcast
        if (iSourceFormat == PVMF_MIME_DATA_SOURCE_SHOUTCAST_URL)
        {
            // calculate MBDS cache size in bytes
            // max bitrate in bytes per second * cache size in secs
            uint32 bitRate = PVMF_DOWNLOADMANAGER_MAX_BITRATE_FOR_SC * 1000 / 8;
            uint32 cacheSize = bitRate * PVMF_DOWNLOADMANAGER_CACHE_SIZE_FOR_SC_IN_SECONDS;

            if (iSocketNode.iNode)
            {
                // TCP buffer size for shoutcast is 1564 (1500 data + 64 overhead)
                // add 1 second margin
                PVMFStatus status = ((PVMFSocketNode*)iSocketNode.iNode)->SetMaxTCPRecvBufferCount((cacheSize + bitRate) / PVMF_DOWNLOADMANAGER_TCP_BUFFER_SIZE_FOR_SC);
                if (PVMFSuccess != status)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                                    "PVMFDownloadManagerNode:SetSourceInitializationData() SetMaxTCPRecvBufferCount(%d) failed",
                                    (cacheSize + bitRate) / PVMF_DOWNLOADMANAGER_TCP_BUFFER_SIZE_FOR_SC));
                    return status;
                }

                status = ((PVMFSocketNode*)iSocketNode.iNode)->SetMaxTCPRecvBufferSize(PVMF_DOWNLOADMANAGER_TCP_BUFFER_SIZE_FOR_SC + PVMF_DOWNLOADMANAGER_TCP_BUFFER_OVERHEAD);
                if (PVMFSuccess != status)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                                    "PVMFDownloadManagerNode:SetSourceInitializationData() SetMaxTCPRecvBufferSize(%d) failed",
                                    PVMF_DOWNLOADMANAGER_TCP_BUFFER_SIZE_FOR_SC + PVMF_DOWNLOADMANAGER_TCP_BUFFER_OVERHEAD));
                    return status;
                }
            }

            // Use Memory Buffer Data Stream for progressive playback and Shoutcast
            OSCL_TRY(leavecode, iMemoryBufferDatastreamFactory = PVMF_BASE_NODE_NEW(PVMFMemoryBufferDataStream, (iSourceFormat, cacheSize)));
            OSCL_FIRST_CATCH_ANY(leavecode, return PVMFFailure);
        }
        else
        {
            uint32 bufSize = PVMF_DOWNLOADMANAGER_TCP_BUFFER_SIZE_FOR_PPB;
            uint32 aNumBuf = PVMF_DOWNLOADMANAGER_MIN_TCP_BUFFERS_FOR_PPB;
            if (iSourceFormat == PVMF_MIME_DATA_SOURCE_RTMP_STREAMING_URL) aNumBuf <<= 2; // for RTMP streaming, default 8 buffers has problem that needs to be fixed. For now, just use large number of buffers.

            if (iSocketNode.iNode)
            {
                PVMFStatus status = ((PVMFSocketNode*)iSocketNode.iNode)->SetMaxTCPRecvBufferCount(aNumBuf);
                if (PVMFSuccess != status)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                                    "PVMFDownloadManagerNode:SetSourceInitializationData() SetMaxTCPRecvBufferCount(%d) failed",
                                    aNumBuf));
                    return status;
                }

                // get buffer size
                ((PVMFSocketNode*)iSocketNode.iNode)->GetMaxTCPRecvBufferSize(bufSize);
            }

            // MBDS cache size calculation
            // TCP buffer size is 64000 (the default), assume worst case that the average packet size is 250 bytes
            // Packet overhead is 64 bytes per packet
            // 8 buffers will yield a cache of 305500, 13 buffers will yield a cache of 560500
            uint32 totalPoolSizeMinusTwoBuffers = (aNumBuf - PVMF_DOWNLOADMANAGER_TCP_BUFFER_NOT_AVAILABLE) * bufSize;
            uint32 numPacketsToFitInPool = totalPoolSizeMinusTwoBuffers / (PVMF_DOWNLOADMANAGER_TCP_AVG_SMALL_PACKET_SIZE + PVMF_DOWNLOADMANAGER_TCP_BUFFER_OVERHEAD);
            uint32 maxDataMinusOverheadInPool = numPacketsToFitInPool * PVMF_DOWNLOADMANAGER_TCP_AVG_SMALL_PACKET_SIZE;

            // Use Memory Buffer Data Stream for progressive playback and Shoutcast
            OSCL_TRY(leavecode, iMemoryBufferDatastreamFactory = PVMF_BASE_NODE_NEW(PVMFMemoryBufferDataStream, (iSourceFormat, maxDataMinusOverheadInPool)));
            OSCL_FIRST_CATCH_ANY(leavecode, return PVMFFailure);

            //Check URL for DTCP-IP
            if (iSourceFormat == PVMF_MIME_DATA_SOURCE_DTCP_URL)
            {
                iCPMNode.Cleanup();
                //Create CPM instance.
                OSCL_ASSERT(iCPMNode.iCPM == NULL);
                iCPMNode.iCPM = PVMFCPMFactory::CreateContentPolicyManager(*this);
                if (!iCPMNode.iCPM)
                    return PVMFErrNoMemory;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                                "PVMFDownloadManagerNode:SetSourceInitializationData() Create CPM for DTCP"));
                //thread logon may leave if there are no plugins - we cannot DTCP sessions without DTCP CPM plugin
                int32 err;
                OSCL_TRY(err, iCPMNode.iCPM->ThreadLogon(););
                OSCL_FIRST_CATCH_ANY(err,
                                     iCPMNode.iCPM->ThreadLogoff();
                                     iCPMNode.Cleanup();
                                     return PVMFErrNotSupported;
                                    );
            }
        }

        OSCL_ASSERT(iMemoryBufferDatastreamFactory != NULL);

        iReadFactory  = iMemoryBufferDatastreamFactory->GetReadDataStreamFactoryPtr();
        iWriteFactory = iMemoryBufferDatastreamFactory->GetWriteDataStreamFactoryPtr();
    }
    else
#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB
    {
        // Now that we have the download file name, we can instantiate the file buffer data stream object
        // Create the filebuffer data stream factory
        iFileBufferDatastreamFactory = PVMF_BASE_NODE_NEW(PVMFFileBufferDataStream, (iDownloadFileName));
        OSCL_ASSERT(iFileBufferDatastreamFactory != NULL);
        iReadFactory  = iFileBufferDatastreamFactory->GetReadDataStreamFactoryPtr();
        iWriteFactory = iFileBufferDatastreamFactory->GetWriteDataStreamFactoryPtr();
    }

    return PVMFSuccess;
}

//public API from data source initialization interface
PVMFStatus PVMFDownloadManagerNode::SetClientPlayBackClock(PVMFMediaClock* aClientClock)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::SetClientPlayBackClock() called"));

    iPlayBackClock = aClientClock;
    if (iPlayBackClock)
    {
        iPlayBackClock->ConstructMediaClockNotificationsInterface(iClockNotificationsInf, *this);
    }

    if (iClockNotificationsInf != NULL)
    {
        iClockNotificationsInf->SetClockStateObserver(*this);
    }

    //pass the source info directly to the download node.
    if (NULL == iProtocolEngineNode.DataSourceInit())
        return PVMFFailure;//no source init interface.

    PVMFStatus status = (iProtocolEngineNode.DataSourceInit())->SetClientPlayBackClock(aClientClock);

    return status;
}

//public API from data source initialization interface
PVMFStatus PVMFDownloadManagerNode::SetEstimatedServerClock(PVMFMediaClock*)
{
    //not needed for download.
    return PVMFErrNotSupported;
}

PVMFDownloadManagerSubNodeContainer& PVMFDownloadManagerNode::TrackSelectNode()
{
    //Decide which sub-node is supporting track selection.
    if (iSourceFormat == PVMF_MIME_DATA_SOURCE_PVX_FILE)
    {
        //for pvx file, the PE node may or may not do track selection.
        //the final decision isn't available until PE node prepare is done
        //and we've queried for the TS interface, at which point the
        //iNoPETrackSelect may be set.
        if (iNoPETrackSelect)
            return iFormatParserNode;

        //if download is already complete, such as after a stop, then
        //the parser node will do track selection.
        if (iDownloadComplete && iPlaybackMode != EDownloadOnly)
            return iFormatParserNode;

        return iProtocolEngineNode;
    }
    else
    {
        //for 3gpp & shoutcast, parser does track selection.
        return iFormatParserNode;
    }
}

//Public API From track selection interface.
PVMFStatus PVMFDownloadManagerNode::GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo)
{
    if (TrackSelectNode().TrackSelection())
        return (TrackSelectNode().TrackSelection())->GetMediaPresentationInfo(aInfo);
    else
        return PVMFFailure; //no track selection interface!
}

//Public API From track selection interface.
PVMFStatus PVMFDownloadManagerNode::SelectTracks(PVMFMediaPresentationInfo& aInfo)
{
    if (TrackSelectNode().TrackSelection())
        return (TrackSelectNode().TrackSelection())->SelectTracks(aInfo);
    else
        return PVMFFailure;//no track selection interface!
}


uint32 PVMFDownloadManagerNode::GetNumMetadataKeys(char* query_key)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::GetNumMetadataKeys() called"));
    if (iFormatParserNode.Metadata())
    {
        return (iFormatParserNode.Metadata())->GetNumMetadataKeys(query_key);
    }
    return 0;
}

uint32 PVMFDownloadManagerNode::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::GetNumMetadataValues() called"));
    if (iFormatParserNode.Metadata())
    {
        return (iFormatParserNode.Metadata())->GetNumMetadataValues(aKeyList);
    }
    return 0;
}


PVMFCommandId PVMFDownloadManagerNode::GetNodeMetadataKeys(PVMFSessionId aSessionId,
        PVMFMetadataList& aKeyList,
        uint32 starting_index,
        int32 max_entries, char* query_key,
        const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::GetNodeMetadataKeys() called"));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_GETNODEMETADATAKEYS, aKeyList, starting_index, max_entries, query_key, aContext);
    return QueueCommandL(cmd);
}


PVMFCommandId PVMFDownloadManagerNode::GetNodeMetadataValues(PVMFSessionId aSessionId,
        PVMFMetadataList& aKeyList,
        Oscl_Vector < PvmiKvp,
        OsclMemAllocator > & aValueList,
        uint32 starting_index,
        int32 max_entries,
        const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::GetNodeMetadataValue() called"));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_GETNODEMETADATAVALUES, aKeyList, aValueList, starting_index, max_entries, aContext);
    return QueueCommandL(cmd);
}

// From PVMFMetadataExtensionInterface
PVMFStatus PVMFDownloadManagerNode::ReleaseNodeMetadataKeys(PVMFMetadataList& keys,
        uint32 start, uint32 end)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::ReleaseNodeMetadataKeys() called"));
    if (iFormatParserNode.Metadata())
    {
        return iFormatParserNode.Metadata()->ReleaseNodeMetadataKeys(keys, start, end);
    }
    return PVMFFailure;
}

// From PVMFMetadataExtensionInterface
PVMFStatus PVMFDownloadManagerNode::ReleaseNodeMetadataValues(Oscl_Vector < PvmiKvp,
        OsclMemAllocator > & aValueList,
        uint32 start, uint32 end)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::ReleaseNodeMetadataValues() called"));

    if (iFormatParserNode.Metadata())
    {
        return iFormatParserNode.Metadata()->ReleaseNodeMetadataValues(aValueList, start, end);
    }
    return PVMFFailure;
}

//public API from data source playback interface
PVMFCommandId PVMFDownloadManagerNode::SetDataSourcePosition(PVMFSessionId aSessionId,
        PVMFTimestamp aTargetNPT,
        PVMFTimestamp& aActualNPT,
        PVMFTimestamp& aActualMediaDataTS,
        bool aSeekToSyncPoint,
        uint32 aStreamID, OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFDownloadManagerNode::SetDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION, aTargetNPT, &aActualNPT,
                                   &aActualMediaDataTS, aSeekToSyncPoint, aStreamID, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFDownloadManagerNode::QueryDataSourcePosition(PVMFSessionId aSessionId,
        PVMFTimestamp aTargetNPT,
        PVMFTimestamp& aSeekPointBeforeTargetNPT,
        PVMFTimestamp& aSeekPointAfterTargetNPT,
        OsclAny* aContextData, bool aSeekToSyncPoint)
{
    OSCL_UNUSED_ARG(aSeekPointAfterTargetNPT);
    // Implemented to complete interface file definition
    // Not tested on logical plane
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFDownloadManagerNode::QueryDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x", aTargetNPT,
                     aContextData, aSeekToSyncPoint));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, &aSeekPointBeforeTargetNPT,
                                   aSeekToSyncPoint, aContextData);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFDownloadManagerNode::QueryDataSourcePosition(PVMFSessionId aSessionId,
        PVMFTimestamp aTargetNPT,
        PVMFTimestamp& aActualNPT,
        bool aSeekToSyncPoint,
        OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFDownloadManagerNode::QueryDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, &aActualNPT,
                                   aSeekToSyncPoint, aContext);
    return QueueCommandL(cmd);
}


PVMFCommandId PVMFDownloadManagerNode::SetDataSourceRate(PVMFSessionId aSessionId, int32 aRate,
        PVMFTimebase* aTimebase, OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFDownloadManagerNode::SetDataSourceRate: aRate=%d", aRate));

    PVMFNodeCommand cmd;
    cmd.PVMFNodeCommand::Construct(aSessionId, PVMF_GENERIC_NODE_SET_DATASOURCE_RATE, aRate, aTimebase, aContext);
    return QueueCommandL(cmd);
}

PVMFStatus PVMFDownloadManagerNode::SetPlayerNodeRegistry(PVPlayerNodeRegistryInterface* aRegistry)
{
    iPlayerNodeRegistry = aRegistry;
    return PVMFSuccess;
}

void PVMFDownloadManagerNode::Run()
{
    //Process async node commands.
    if (!iInputCommands.empty())
        ProcessCommand();

    //Issue commands to the sub-nodes.
    if (!iProtocolEngineNode.CmdPending()
            && !iFormatParserNode.CmdPending()
            && !iSocketNode.CmdPending()
            && !iRecognizerNode.CmdPending()
            && !iCPMNode.CmdPending()
            && !iSubNodeCmdVec.empty())
    {
        PVMFStatus status = iSubNodeCmdVec.front().iNC->IssueCommand(iSubNodeCmdVec.front().iCmd);
        if (status != PVMFPending)
            iSubNodeCmdVec.front().iNC->CommandDone(status, NULL, NULL);
    }
}


void PVMFDownloadManagerNode::CommandComplete(PVMFNodeCommand& aCmd, PVMFStatus aStatus,
        PVInterface*aExtMsg, OsclAny* aEventData)
{

    //if the command failed or was cancelled there may be un-processed sub-node commands, so clear the vector now.
    if (!iSubNodeCmdVec.empty())
        iSubNodeCmdVec.clear();

    //We may need to wait on the movie atom before the node cmd can complete.
    //This is a good place to catch that condition and suppress the node
    //cmd completion.
    if (iParserInitAfterMovieAtom
            || iParserPrepareAfterMovieAtom)
    {
        if (aStatus == PVMFSuccess)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFDownloadManagerNode::CommandComplete() Blocking Command Completion until Movie Atom Downloaded."));
            return;//keep waiting on movie atom complete.
        }
        else
        {
            //if command failed or was cancelled then clear any movie atom wait
            //flags.
            iParserInitAfterMovieAtom = false;
            iParserPrepareAfterMovieAtom = false;
        }
    }

    // Base node's CommandComplete() notifies the observer(s)
    PVMFNodeInterfaceImpl::CommandComplete(aCmd, aStatus,
                                           aExtMsg, aEventData, NULL, NULL);
    //re-schedule if there are more commands and node isn't logged off
    if (!iInputCommands.empty())
        Reschedule();
}

void PVMFDownloadManagerNode::CPMCommandCompleted(const PVMFCmdResp& aResponse)
{
    if (iCPMNode.iCPM)
    {
        iCPMNode.CPMCommandCompleted(aResponse);
    }
}


void PVMFDownloadManagerNode::ReportInfoEvent(PVMFAsyncEvent &aEvent)
{
    //Report a node info event

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::ReportInfoEvent() In Type %d Data %d ExtMsg %d",
                    aEvent.GetEventType(), aEvent.GetEventData(), aEvent.GetEventExtensionInterface()));

    // let base node report a Info event
    PVMFNodeInterfaceImpl::ReportInfoEvent(aEvent);

    //For download-then-play mode, generate data ready event when buffering
    //is complete.  We will have suppressed the real initial data ready
    //event from PE node in this case.
    if (aEvent.GetEventType() == PVMFInfoBufferingComplete
            && iPlaybackMode == PVMFDownloadManagerNode::EDownloadThenPlay
            && !iDataReady)
    {
        GenerateDataReadyEvent();
    }
    else if (aEvent.GetEventType() == PVMFInfoContentType)
    {
        // copy and save MIME string for recognizer to use as hint
        iContentTypeMIMEString = (char *)aEvent.GetEventData();
    }
}

void PVMFDownloadManagerNode::GenerateDataReadyEvent()
{
    PVMFAsyncEvent info(PVMFInfoEvent, PVMFInfoDataReady, NULL, NULL);
    ReportInfoEvent(info);
    iDataReady = true;
}

bool PVMFDownloadManagerNode::FilterPlaybackEventsFromSubNodes(const PVMFAsyncEvent& aEvent)
{
    switch (aEvent.GetEventType())
    {
        case PVMFInfoUnderflow:
            //filter any underflow that happens before data ready
            if (!iDataReady)
                return true;
            else
                iDataReady = false;
            break;
        case PVMFInfoDataReady:
            //filter any data ready that happens before download complete
            //in dl-then-play mode
            if (iPlaybackMode == EDownloadThenPlay
                    && !iDownloadComplete)
            {
                return true;
            }
            //filter any data ready in dl-only mode, though I don't
            //think it's possible.
            if (iPlaybackMode == EDownloadOnly)
                return true;

            iDataReady = true;

            break;
        case PVMFInfoRemoteSourceNotification:
            //we get this event for "not pseudostreamable" for both PVX
            //and 3gpp.  Only pass it up for 3gpp.
            if (iSourceFormat != PVMF_MIME_DATA_SOURCE_HTTP_URL)
                return true;
            break;
        default:
            break;
    }
    return false;
}

PVMFStatus PVMFDownloadManagerNode::DoQueryUuid()
{
    //Start executing a node command

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoQueryUuid() In"));

    OSCL_String* mimetype;
    Oscl_Vector<PVUuid, OsclMemAllocator> *uuidvec;
    bool exactmatch;
    iCurrentCommand.PVMFNodeCommandBase::Parse(mimetype, uuidvec, exactmatch);

    // @TODO Add MIME string matching
    // For now just return all available extension interface UUID
    uuidvec->push_back(PVMF_TRACK_SELECTION_INTERFACE_UUID);
    uuidvec->push_back(PVMF_DATA_SOURCE_INIT_INTERFACE_UUID);
    uuidvec->push_back(KPVMFMetadataExtensionUuid);
    uuidvec->push_back(PvmfDataSourcePlaybackControlUuid);
    uuidvec->push_back(PVMI_CAPABILITY_AND_CONFIG_PVUUID);

    return PVMFSuccess;
}


PVMFStatus PVMFDownloadManagerNode::DoQueryInterface()
{
    //Start executing a node command

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoQueryInterface() In"));

    PVUuid* uuid;
    PVInterface** ptr;
    iCurrentCommand.PVMFNodeCommandBase::Parse(uuid, ptr);

    if (queryInterface(*uuid, *ptr))
    {
        //Schedule further queries on sub-nodes...
        return ScheduleSubNodeCommands();
    }
    else
    {
        //interface not supported
        *ptr = NULL;
        return PVMFFailure;
    }
}

PVMFStatus PVMFDownloadManagerNode::DoRequestPort(PVMFPortInterface*& aPort)
{
    //Start executing a node command

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoRequestPort() In"));

    return ScheduleSubNodeCommands();
}

PVMFStatus PVMFDownloadManagerNode::DoReleasePort()
{
    //Start executing a node command

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoReleasePort() In"));

    return ScheduleSubNodeCommands();
}

PVMFStatus PVMFDownloadManagerNode::DoInit()
{
    //Start executing a node command

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoInitNode() In"));

    return ScheduleSubNodeCommands();
}

PVMFStatus PVMFDownloadManagerNode::DoPrepare()
{
    //Start executing a node command

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoPrepareNode() In"));

    return ScheduleSubNodeCommands();
}

PVMFStatus PVMFDownloadManagerNode::DoStart()
{
    //Start executing a node command

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoStartNode() In"));

    return ScheduleSubNodeCommands();
}

PVMFStatus PVMFDownloadManagerNode::DoStop()
{
    //Start executing a node command

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoStopNode() In"));

    return ScheduleSubNodeCommands();
}

PVMFStatus PVMFDownloadManagerNode::DoFlush()
{
    //Start executing a node command

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoFlushNode() In"));

    return ScheduleSubNodeCommands();
}

PVMFStatus PVMFDownloadManagerNode::DoPause()
{
    //Start executing a node command

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoPauseNode() In"));

    return ScheduleSubNodeCommands();
}

PVMFStatus PVMFDownloadManagerNode::DoReset()
{
    //remove the clock observer
    if (iPlayBackClock != NULL)
    {
        if (iClockNotificationsInf != NULL)
        {
            iClockNotificationsInf->RemoveClockStateObserver(*this);
            iPlayBackClock->DestroyMediaClockNotificationsInterface(iClockNotificationsInf);
            iClockNotificationsInf = NULL;
        }
    }

    //Start executing a node command
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::DoResetNode() In"));

    //Reset the sub-nodes first.
    return ScheduleSubNodeCommands();
}

void PVMFDownloadManagerNode::ContinueInitAfterTrackSelectDecision()
{
    //this is called during the Init sequence, once we have enough information
    //to make a definite track select decision.

    //See whether we need to stop to allow track selection on the download server.
    //If it's download-only, we don't offer this option, since the download must
    //be started in the Init.
    if (iPlaybackMode != EDownloadOnly
            && TrackSelectNode().iType == PVMFDownloadManagerSubNodeContainerBase::EProtocolEngine)
    {
        //stop the Init sequence here so we can do track selection on the download
        //server.
        ;
    }
    else
    {
        //else download-only, or there's no track selection available from the
        //PE node.  Continue the Init or Prepare sequence.
        ContinueFromDownloadTrackSelectionPoint();
    }
}

void PVMFDownloadManagerNode::ContinueFromDownloadTrackSelectionPoint()
{
    //Continue the Init or Prepare sequence, stopping at parser init.

    //start the download.
    Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EStart);

    //initiate file recognize & parse, unless this is download-only mode.
    if (iPlaybackMode != EDownloadOnly)
    {
        //do recognizer sequence if needed.
        if (iSourceFormat == PVMF_MIME_DATA_SOURCE_PVX_FILE)
        {
            //PXV is always assumed to be MP4
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,
                            "PVMFDownloadManagerNode::ContinueFromDownloadTrackSelectionPoint Setting format to MP4"));
            iMimeType = PVMF_MIME_MPEG4FF;
        }
        else if (iSourceFormat == PVMF_MIME_DATA_SOURCE_RTMP_STREAMING_URL)
        {
            //RTMP streaming is always assumed to be FLV parser node
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,
                            "PVMFDownloadManagerNode::ContinueFromDownloadTrackSelectionPoint Setting format to FLV"));
            iMimeType = PVMF_MIME_FLVFF;
        }
        else
        {
            //for other source formats, use the recognizer to determine the format.
            Push(iRecognizerNode, PVMFDownloadManagerSubNodeContainerBase::ERecognizerStart);
            Push(iRecognizerNode, PVMFDownloadManagerSubNodeContainerBase::ERecognizerClose);
        }

        //create parser
        Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EParserCreate);

        // Send commands to the parser node to query these extension interfaces.
        Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EQueryDataSourceInit);
        Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EQueryTrackSelection);
        Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EQueryMetadata);
        Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EQueryDatastreamUser);
        Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EQueryDataSourcePlayback);
        Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EQueryFFProgDownload);
        Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::ESetFFProgDownloadSupport);

        //if this is PVX, we need to wait on movie atom before we can
        //init parser.
        if (iSourceFormat == PVMF_MIME_DATA_SOURCE_PVX_FILE)
        {
            if (iMovieAtomComplete || iDownloadComplete)
            {
                iParserInit = true;
                Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EInit);
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,
                                "PVMFDownloadManagerNode::ContinueFromDownloadTrackSelectionPoint Setting flag to Init Parser after Movie Atom Downloaded"));
                //set this flag to trigger parser init when movie atom is done.
                iParserInitAfterMovieAtom = true;
            }
        }
        else
        {
            //for other formats, go ahead and init parser.  Init will block until
            //receiving movie atom.
            iParserInit = true;
            Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EInit);
        }
    }
}

//Called when movie atom is received, or when download is complete
//but movie atom was never received.
void PVMFDownloadManagerNode::ContinueAfterMovieAtom()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFDownloadManagerNode::ContinueAfterMovieAtom() "));

    if (!iMovieAtomComplete)
    {
        iMovieAtomComplete = true;
        //see whether we need to continue with parser init
        if (iParserInitAfterMovieAtom)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFDownloadManagerNode::ContinueAfterMovieAtom() Continuing to Parser Init"));
            iParserInitAfterMovieAtom = false;
            iParserInit = true;
            Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EInit);
            Reschedule();
        }
        //see whether we need to continue with parser prepare
        if (iParserPrepareAfterMovieAtom)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFDownloadManagerNode::ContinueAfterMovieAtom() Continuing to Parser Prepare"));
            iParserPrepareAfterMovieAtom = false;
            Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EPrepare);
            Reschedule();
        }
    }
}

PVMFNodeInterface* PVMFDownloadManagerNode::CreateParser()
{
    if (!(iMimeType == PVMF_MIME_FORMAT_UNKNOWN))
    {
        PVMFNodeInterface *iSourceNode = NULL;
        PVMFFormatType outputFormatType = PVMF_MIME_FORMAT_UNKNOWN;
        iFmt = iMimeType.get_str();
        PVMFStatus status =
            iPlayerNodeRegistry->QueryRegistry(iFmt, outputFormatType, iDNodeUuids);
        if ((status == PVMFSuccess) && (iDNodeUuids.size() > 0))
        {
            int32 leavecode = 0;
            OSCL_TRY(leavecode, iSourceNode = iPlayerNodeRegistry->CreateNode(iDNodeUuids[iDNodeUuidCount]));
            OSCL_FIRST_CATCH_ANY(leavecode, return NULL);
            iDNodeUuidCount++;
            return iSourceNode;
        }
    }
    return NULL;
}

PVMFStatus PVMFDownloadManagerNode::ScheduleSubNodeCommands()
{
    //given the node command ID, create the sub-node command vector, initiate the processing and return the node command status.

    OSCL_ASSERT(iSubNodeCmdVec.empty());

    //Create the vector of all the commands in the sequence.
    switch (iCurrentCommand.iCmd)
    {
        case PVMF_GENERIC_NODE_QUERYINTERFACE:
        {
            //When we get here we've already called queryInterface on this node
            //for the interface.  This code schedules any additional sub-node commands
            //that are needed to support the interface.

            //extract uuid from Node command...
            PVUuid*aUuid;
            PVInterface**aInterface;
            iCurrentCommand.PVMFNodeCommandBase::Parse(aUuid, aInterface);
            OSCL_ASSERT(aUuid != NULL);

            if (*aUuid == PVMF_DATA_SOURCE_INIT_INTERFACE_UUID)
            {
                //To support data source init interface we need a bunch of sub-node interfaces.
                Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EQueryProtocolEngine);
                Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EQueryDatastreamUser);
                Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EQueryDataSourceInit);
                Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EQueryDownloadProgress);
            }
            //else nothing else needed for other interfaces.
        }
        break;

        case PVMF_GENERIC_NODE_INIT:
            //check for second "Init" command after a license acquire.
            if (iInitFailedLicenseRequired)
            {
                iParserInit = true;
                Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EInit);
            }
            else
            {
                //reset any prior download/playback event
                iDownloadComplete = false;
                iParserInit = false;
                iDataReady = false;
                iMovieAtomComplete = false;
                iParserInitAfterMovieAtom = false;
                iParserPrepareAfterMovieAtom = false;
                iRecognizerError = false;
                iInitFailedLicenseRequired = false;

                //reset any prior track select decisions.
                iFormatParserNode.iTrackSelection = NULL;
                iProtocolEngineNode.iTrackSelection = NULL;
                iNoPETrackSelect = false;

                //reset any prior recognizer decisions.
                iMimeType = PVMF_MIME_FORMAT_UNKNOWN;


                // This is for DTCP Mode
                if (iCPMNode.iCPM)
                {
                    Push(iCPMNode, PVMFDownloadManagerSubNodeContainerBase::ECPMInit);
                    Push(iCPMNode, PVMFDownloadManagerSubNodeContainerBase::ECPMOpenSession);
                    Push(iCPMNode, PVMFDownloadManagerSubNodeContainerBase::ECPMRegisterContent);
                    Push(iCPMNode, PVMFDownloadManagerSubNodeContainerBase::ECPMApproveUsage);
                    Push(iCPMNode, PVMFDownloadManagerSubNodeContainerBase::ECPMSetDecryptInterface);
                }

                // Send the INIT command to the protocol engine node, followed by the Socket Node.
                Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EInit);
                Push(iSocketNode, PVMFDownloadManagerSubNodeContainerBase::EInit);
                // Issue the port request to the Protocol Engine Node and the socket node
                // NOTE: The request for the socket node's port must come first, followed by the protocol node,
                // because the code to connect the two ports in CommandDone() will do so when the protocol node's port is returned.
                Push(iSocketNode, PVMFDownloadManagerSubNodeContainerBase::ERequestPort);
                Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::ERequestPort);
                // The two ports will be connected in CommandDone, when the 2nd port request completes.
                // After the ports are connected, the datastream factory is passed to the protocol engine node.
                Push(iSocketNode, PVMFDownloadManagerSubNodeContainerBase::EPrepare);
                Push(iSocketNode, PVMFDownloadManagerSubNodeContainerBase::EStart);
                Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EPrepare);

                if (TrackSelectNode().iType == PVMFDownloadManagerSubNodeContainerBase::EFormatParser)
                {
                    //parser is doing track selection, there's no question
                    ContinueInitAfterTrackSelectDecision();
                }
                else
                {
                    //PE node may be doing track selection, but to be sure, we need
                    //to wait until it is prepared, then request the track selection interface.
                    iProtocolEngineNode.iTrackSelection = NULL;
                    Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EQueryTrackSelection);
                    //once this command is complete, we will call ContinueInitAfterTrackSelectDecision()
                }
            }
            break;

        case PVMF_GENERIC_NODE_PREPARE:
            //if protocol engine node did track selection, then we need to continue
            //to the file parse stage here.  Otherwise it was already done in the Init.
            if (TrackSelectNode().iType == PVMFDownloadManagerSubNodeContainerBase::EProtocolEngine)
            {
                ContinueFromDownloadTrackSelectionPoint();
            }
            //if we initiated file parse sequence already, then go ahead and prepare
            //the parser node.
            if (iParserInit)
            {
                Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EPrepare);
            }
            //if we're waiting on movie atom to init parser, then set a flag so we'll
            //also do the parser prepare when it arrives.
            else if (iParserInitAfterMovieAtom)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0,
                                "PVMFDownloadManagerNode::ScheduleSubNodeCommands Setting flag to Prepare Parser after Movie Atom Downloaded"));
                iParserPrepareAfterMovieAtom = true;
            }
            break;

        case PVMF_GENERIC_NODE_REQUESTPORT:
            //if file isn't parsed (as in download-only), then fail command
            if (!iFormatParserNode.iNode)
                return PVMFErrNotSupported;
            Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::ERequestPort);
            break;

        case PVMF_GENERIC_NODE_RELEASEPORT:
            //if file isn't parsed (as in download-only), then fail command
            if (!iFormatParserNode.iNode)
                return PVMFErrNotSupported;
            Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EReleasePort);
            break;

        case PVMF_GENERIC_NODE_START:
            //Re-start socket node & PE node in case they were stopped by a prior
            //stop command.
            if (iSocketNode.iNode->GetState() == EPVMFNodePrepared)
                Push(iSocketNode, PVMFDownloadManagerSubNodeContainerBase::EStart);
            if (iProtocolEngineNode.iNode->GetState() == EPVMFNodePrepared)
                Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EStart);
            //Start or re-start parser node (unless download-only)
            if (iFormatParserNode.iNode)
                Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EStart);
            break;

        case PVMF_GENERIC_NODE_STOP:
            iDataReady = false;
            //Stop parser (unless download-only)
            if (iFormatParserNode.iNode)
                Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EStop);
            //Stop PE node & socket node.
            Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EStop);
            Push(iSocketNode, PVMFDownloadManagerSubNodeContainerBase::EStop);
            break;

        case PVMF_GENERIC_NODE_FLUSH:
            if (iFormatParserNode.iNode)
                Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EFlush);
            break;

        case PVMF_GENERIC_NODE_PAUSE:
            //note: pause/resume download is not supported.
            if (iFormatParserNode.iNode)
                Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EPause);
            break;

        case PVMF_GENERIC_NODE_RESET:
            //Stop socket node if needed.
            if (iSocketNode.iNode->GetState() == EPVMFNodeStarted
                    || iSocketNode.iNode->GetState() == EPVMFNodePaused)
            {
                Push(iSocketNode, PVMFDownloadManagerSubNodeContainerBase::EStop);
            }
            //Stop PE node if needed.
            if (iProtocolEngineNode.iNode->GetState() == EPVMFNodeStarted
                    || iProtocolEngineNode.iNode->GetState() == EPVMFNodePaused)
            {
                Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EStop);
            }
            //Reset & cleanup all nodes.
            if (iFormatParserNode.iNode)
                Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EReset);
            Push(iSocketNode, PVMFDownloadManagerSubNodeContainerBase::EReset);
            Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::EReset);
            if (iCPMNode.iCPM)
            {
                Push(iCPMNode, PVMFDownloadManagerSubNodeContainerBase::ECPMUsageComplete);
                Push(iCPMNode, PVMFDownloadManagerSubNodeContainerBase::ECPMReset);
            }
            Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::ECleanup);
            Push(iProtocolEngineNode, PVMFDownloadManagerSubNodeContainerBase::ECleanup);
            Push(iSocketNode, PVMFDownloadManagerSubNodeContainerBase::ECleanup);
            Push(iCPMNode, PVMFDownloadManagerSubNodeContainerBase::ECleanup);
            break;

        case PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION:
            //if file isn't parsed (as in download-only), then fail command
            if (!iFormatParserNode.iNode)
                return PVMFErrNotSupported;
            Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::ESetDataSourcePosition);
            break;

        case PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION:
            //if file isn't parsed (as in download-only), then fail command
            if (!iFormatParserNode.iNode)
                return PVMFErrNotSupported;
            Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EQueryDataSourcePosition);
            break;

        case PVMF_GENERIC_NODE_GETNODEMETADATAKEYS:
            //if file isn't parsed (as in download-only), then fail command
            if (!iFormatParserNode.iNode)
                return PVMFErrNotSupported;
            Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EGetMetadataKey);
            break;

        case PVMF_GENERIC_NODE_GETNODEMETADATAVALUES:
            //if file isn't parsed (as in download-only), then fail command
            if (!iFormatParserNode.iNode)
                return PVMFErrNotSupported;
            Push(iFormatParserNode, PVMFDownloadManagerSubNodeContainerBase::EGetMetadataValue);
            break;

        default:
            OSCL_ASSERT(false);
            break;
    }

    if (iSubNodeCmdVec.empty())
    {
        //in a few cases there's nothing needed and no new commands
        //were issued-- so succeed here.
        return PVMFSuccess;
    }
    else
    {
        //Wakeup the node to start issuing the sub-node commands.
        Reschedule();

        //the node command is pending.
        return PVMFPending;
    }
}

void PVMFDownloadManagerNode::Push(PVMFDownloadManagerSubNodeContainerBase& n, PVMFDownloadManagerSubNodeContainerBase::CmdType c)
{
    //push a sub-node command onto the cmd vector
    CmdElem elem;
    elem.iCmd = c;
    elem.iNC = &n;
    iSubNodeCmdVec.push_back(elem);
}

//
// PVMFDownloadManagerSubNodeContainer Implementation.
//

PVMFDownloadManagerSubNodeContainerBase::PVMFDownloadManagerSubNodeContainerBase()
{
    iCmdState = EIdle;
    iCancelCmdState = EIdle;
}

void PVMFDownloadManagerSubNodeContainerBase::Construct(NodeType t, PVMFDownloadManagerNode* c)
{
    iContainer = c;
    iType = t;
}

void PVMFDownloadManagerSubNodeContainer::Cleanup()
{
    //release all the queried interfaces.
    if (iDataSourceInit)
    {
        iDataSourceInit->removeRef();
        iDataSourceInit = NULL;
    }
    if (iProtocolEngineExtensionInt)
    {
        iProtocolEngineExtensionInt->removeRef();
        iProtocolEngineExtensionInt = NULL;
    }
    if (iDatastreamUser)
    {
        iDatastreamUser->removeRef();
        iDatastreamUser = NULL;
    }
    if (iTrackSelection)
    {
        iTrackSelection->removeRef();
        iTrackSelection = NULL;
    }
    if (iMetadata)
    {
        iMetadata->removeRef();
        iMetadata = NULL;
    }
    if (iDataSourcePlayback)
    {
        iDataSourcePlayback->removeRef();
        iDataSourcePlayback = NULL;
    }
    if (iFormatProgDownloadSupport)
    {
        iFormatProgDownloadSupport->removeRef();
        iFormatProgDownloadSupport = NULL;
    }
    if (iDownloadProgress)
    {
        iDownloadProgress->removeRef();
        iDownloadProgress = NULL;
    }
    //the node instance is cleaned up elsewhere.
}

void PVMFDownloadManagerRecognizerContainer::Cleanup()
{
    // Nothing to do here 'til recognizer is integrated
}

void PVMFDownloadManagerSubNodeContainer::Connect()
{
    //Issue connect command to the sub-node.

    //This container class is the observer.
    PVMFNodeSessionInfo info(this //cmd
                             , this, NULL //info
                             , this, NULL); //err
    if (iNode)
        iSessionId = iNode->Connect(info);
}

void PVMFDownloadManagerCPMContainer::Cleanup()
{
    //cleanup CPM object.
    if (iCPM)
    {
        iCPM->ThreadLogoff();
        PVMFCPMFactory::DestroyContentPolicyManager(iCPM);
        iCPM = NULL;
    }
}

PVMFStatus PVMFDownloadManagerSubNodeContainer::IssueCommand(int32 aCmd)
{
    //Issue a command to the sub-node.
    //Return the sub-node completion status-- either pending, success, or failure.

    LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s () In", GETNODESTR));

    OSCL_ASSERT(!CmdPending());

    //find the current node command since we may need its parameters.

    OSCL_ASSERT(iContainer->iCurrentCommand.iCmd != PVMF_GENERIC_NODE_COMMAND_INVALID);
    PVMFNodeCommand* nodeCmd = &iContainer->iCurrentCommand;

    //save the sub-node command code
    iCmd = aCmd;

    switch (aCmd)
    {
        case ECleanup:
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling Cleanup", GETNODESTR));
            Cleanup();
            return PVMFSuccess;

        case EParserCreate:
            iNode = iContainer->CreateParser();
            if (iNode)
            {
                Connect();
                iNode->ThreadLogon();
                return PVMFSuccess;
            }
            return PVMFErrCorrupt;

        case EQueryDataSourceInit:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling QueryInterface(data source init)", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->QueryInterface(iSessionId, PVMF_DATA_SOURCE_INIT_INTERFACE_UUID, iDataSourceInit);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EQueryProtocolEngine:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling QueryInterface(ProtocolEngine)", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->QueryInterface(iSessionId, KPVMFProtocolEngineNodeExtensionUuid, iProtocolEngineExtensionInt);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EQueryDatastreamUser:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling QueryInterface(DatastreamUser)", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->QueryInterface(iSessionId, PVMIDatastreamuserInterfaceUuid, iDatastreamUser);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EQueryTrackSelection:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling QueryInterface(track selection)", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->QueryInterface(iSessionId, PVMF_TRACK_SELECTION_INTERFACE_UUID, iTrackSelection);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EQueryMetadata:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling QueryInterface(metadata)", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->QueryInterface(iSessionId, KPVMFMetadataExtensionUuid, iMetadata);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EQueryDataSourcePlayback:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling QueryInterface(datasourcePB)", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->QueryInterface(iSessionId, PvmfDataSourcePlaybackControlUuid, iDataSourcePlayback);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EInit:
            OSCL_ASSERT(iNode != NULL);
            if (iType == EFormatParser)
            {
                // For this command, which gets pushed to the format parser node, we set the source init and also
                // set the datstream factory
                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling SetSourceInitializationData", GETNODESTR));

                if (!DataSourceInit())
                    return PVMFFailure; //no source init interface?
                if (!DatastreamUser())
                    return PVMFFailure; //no datastreamuser interface?

                //Pass data to the parser node.
                if (iContainer->iInitFailedLicenseRequired)
                {
                    ;//do nothing-- data was already set on the first init call.
                }
                else
                {
                    //Pass source data
                    if (iContainer->iSourceFormat == PVMF_MIME_DATA_SOURCE_PVX_FILE)
                    {
                        // let the parser know this is PVX format.
                        PVMFFormatType fmt = PVMF_MIME_DATA_SOURCE_PVX_FILE;
                        (DataSourceInit())->SetSourceInitializationData(iContainer->iDownloadFileName
                                , fmt
                                , (OsclAny*)&iContainer->iLocalDataSource);
                    }
                    else if (iContainer->iSourceFormat == PVMF_MIME_DATA_SOURCE_SHOUTCAST_URL)
                    {
                        // let the parser node know that it is playing from a shoutcast stream
                        (DataSourceInit())->SetSourceInitializationData(iContainer->iDownloadFileName
                                , iContainer->iSourceFormat
                                , (OsclAny*)iContainer->iSourceData);
                    }
                    else if (iContainer->iSourceFormat == PVMF_MIME_DATA_SOURCE_DTCP_URL)
                    {
                        // let the parser know this is DTCP format.
                        (DataSourceInit())->SetSourceInitializationData(iContainer->iDownloadFileName
                                , iContainer->iFmt
                                , (OsclAny*)iContainer->iSourceData
                                , PVMF_FORMAT_TYPE_CONNECT_UNPROTECTED);
                    }
                    else
                    {
                        // pass the recognized format to the parser.
                        (DataSourceInit())->SetSourceInitializationData(iContainer->iDownloadFileName
                                , iContainer->iFmt
                                , (OsclAny*)iContainer->iSourceData);
                    }

                    //Pass datastream data.
                    (DatastreamUser())->PassDatastreamFactory(*(iContainer->iReadFactory), (int32)0);
                    PVMFFileBufferDataStreamWriteDataStreamFactoryImpl* wdsfactory =
                        OSCL_STATIC_CAST(PVMFFileBufferDataStreamWriteDataStreamFactoryImpl*, iContainer->iWriteFactory);
                    int32 leavecode = 0;
                    OSCL_TRY(leavecode,
                             PVMFDataStreamReadCapacityObserver* obs =
                                 OSCL_STATIC_CAST(PVMFDataStreamReadCapacityObserver*, wdsfactory);
                             (DatastreamUser())->PassDatastreamReadCapacityObserver(obs));
                    OSCL_FIRST_CATCH_ANY(leavecode,
                                         LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s PassDatastreamReadCapacityObserver not supported", GETNODESTR));
                                        );
                }

                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling Init ", GETNODESTR));
                iCmdState = EBusy;
                iCmdId = iNode->Init(iSessionId);
                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
                return PVMFPending;
            }
            else
            {
                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling Init ", GETNODESTR));
                iCmdState = EBusy;
                iCmdId = iNode->Init(iSessionId);
                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
                return PVMFPending;
            }

        case ERequestPort:
            OSCL_ASSERT(iNode != NULL);
            // The parameters to RequestPort vary depending on which node we're getting a port from, so we switch on it.
            switch (iType)
            {
                case EProtocolEngine:
                    LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling RequestPort ", GETNODESTR));
                    iCmdState = EBusy;
                    // For protocol engine port request, we don't need port tag or config info because it's the only port we ask it for.
                    iCmdId = iNode->RequestPort(iSessionId, (int32)0);
                    LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
                    return PVMFPending;

                case ESocket:
                    LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling RequestPort with port config %s", GETNODESTR, iContainer->iServerAddr.get_cstr()));
                    iCmdState = EBusy;
                    //append a mimestring to the port for socket node logging
                    iContainer->iServerAddr += ";mime=download";
                    iCmdId = iNode->RequestPort(iSessionId, PVMF_SOCKET_NODE_PORT_TYPE_PASSTHRU, &iContainer->iServerAddr);
                    LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
                    return PVMFPending;

                case EFormatParser:
                    //extract params from current Node command.
                    OSCL_ASSERT(nodeCmd->iCmd == PVMF_GENERIC_NODE_REQUESTPORT);
                    {
                        int32 aPortTag;
                        OSCL_String*aMimetype;
                        nodeCmd->PVMFNodeCommandBase::Parse(aPortTag, aMimetype);

                        LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling RequestPort ", GETNODESTR));
                        iCmdState = EBusy;
                        iCmdId = iNode->RequestPort(iSessionId, aPortTag, aMimetype);
                    }
                    LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
                    return PVMFPending;

                default:
                    OSCL_ASSERT(false);
                    return PVMFFailure;
            }

        case EReleasePort:
            OSCL_ASSERT(iNode != NULL);
            {
                //extract params from current Node command.
                OSCL_ASSERT(nodeCmd->iCmd == PVMF_GENERIC_NODE_RELEASEPORT);
                PVMFPortInterface *port;
                nodeCmd->PVMFNodeCommandBase::Parse(port);
                OSCL_ASSERT(port != NULL);

                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling ReleasePort", GETNODESTR));
                iCmdState = EBusy;
                iCmdId = iNode->ReleasePort(iSessionId, *port);
                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
                return PVMFPending;
            }

        case EPrepare:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling Prepare", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->Prepare(iSessionId);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EStop:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling Stop", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->Stop(iSessionId);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EStart:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling Start", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->Start(iSessionId);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EPause:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling Pause", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->Pause(iSessionId);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EFlush:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling Flush", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->Flush(iSessionId);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EReset:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling Reset", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->Reset(iSessionId);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EGetMetadataKey:
            OSCL_ASSERT(iNode != NULL);
            {
                if (!Metadata())
                    return PVMFErrNotSupported;//no interface!

                //extract params from current Node command.
                OSCL_ASSERT(nodeCmd->iCmd == PVMF_GENERIC_NODE_GETNODEMETADATAKEYS);

                PVMFMetadataList* aKeyList;
                uint32 starting_index;
                int32 max_entries;
                char* query_key;

                nodeCmd->Parse(aKeyList, starting_index, max_entries, query_key);
                OSCL_ASSERT(aKeyList != NULL);
                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling GetNodeMetadataKeys", GETNODESTR));
                iCmdState = EBusy;
                iCmdId = (Metadata())->GetNodeMetadataKeys(iSessionId, *aKeyList, starting_index, max_entries, query_key, NULL);

                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
                return PVMFPending;
            }


        case EGetMetadataValue:
            OSCL_ASSERT(iNode != NULL);
            {
                if (!Metadata())
                    return PVMFErrNotSupported;//no interface!

                //extract params from current Node command.
                OSCL_ASSERT(nodeCmd->iCmd == PVMF_GENERIC_NODE_GETNODEMETADATAVALUES);
                PVMFMetadataList* aKeyList;
                Oscl_Vector<PvmiKvp, OsclMemAllocator>* aValueList;
                uint32 starting_index;
                int32 max_entries;
                nodeCmd->Parse(aKeyList, aValueList, starting_index, max_entries);
                OSCL_ASSERT(aKeyList != NULL);
                OSCL_ASSERT(aValueList != NULL);

                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling GetNodeMetadataValues", GETNODESTR));
                iCmdState = EBusy;
                iCmdId = (Metadata())->GetNodeMetadataValues(iSessionId, *aKeyList, *aValueList, starting_index, max_entries, NULL);
                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
                return PVMFPending;
            }

        case EQueryFFProgDownload:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling QueryInterface (format prog dl)", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->QueryInterface(iSessionId, PVMF_FF_PROGDOWNLOAD_SUPPORT_INTERFACE_UUID, iFormatProgDownloadSupport);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case EQueryDownloadProgress:
            OSCL_ASSERT(iNode != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling QueryInterface (dl prog)", GETNODESTR));
            iCmdState = EBusy;
            iCmdId = iNode->QueryInterface(iSessionId, PVMF_DOWNLOAD_PROGRESS_INTERFACE_UUID, iDownloadProgress);
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
            return PVMFPending;

        case ESetFFProgDownloadSupport:
            OSCL_ASSERT(iNode != NULL);

            if (!DownloadProgress() || !iContainer->iFormatParserNode.FormatProgDownloadSupport())
                return PVMFErrNotSupported;//no interface!

            //pass parser node format prog download interface to the protocol node.
            LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling setFormatDownloadSupportInterface", GETNODESTR));
            (DownloadProgress())->setFormatDownloadSupportInterface(iContainer->iFormatParserNode.FormatProgDownloadSupport());
            return PVMFSuccess;

        case ESetDataSourcePosition:
            OSCL_ASSERT(iNode != NULL);
            {
                if (!DataSourcePlayback())
                    return PVMFErrNotSupported;//no interface!

                //extract params from current Node command.
                OSCL_ASSERT(nodeCmd->iCmd == PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION);
                PVMFTimestamp aTargetNPT;
                PVMFTimestamp* aActualNPT;
                PVMFTimestamp* aActualMediaDataTS;
                uint32 streamID = 0;
                bool aJump;
                nodeCmd->Parse(aTargetNPT, aActualNPT, aActualMediaDataTS, aJump, streamID);
                OSCL_ASSERT(aActualNPT != NULL);
                OSCL_ASSERT(aActualMediaDataTS != NULL);

                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling SetDataSourcePosition", GETNODESTR));
                iCmdState = EBusy;
                iCmdId = (DataSourcePlayback())->SetDataSourcePosition(iSessionId, aTargetNPT, *aActualNPT, *aActualMediaDataTS, aJump, streamID);
                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
                return PVMFPending;
            }

        case EQueryDataSourcePosition:
            OSCL_ASSERT(iNode != NULL);
            {
                if (!DataSourcePlayback())
                    return PVMFErrNotSupported;//no interface!

                //extract params from current Node command.
                OSCL_ASSERT(nodeCmd->iCmd == PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION);
                PVMFTimestamp aTargetNPT;
                PVMFTimestamp* aActualNPT;
                bool aJump;
                nodeCmd->Parse(aTargetNPT, aActualNPT, aJump);
                OSCL_ASSERT(aActualNPT != NULL);

                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s Calling QueryDataSourcePosition", GETNODESTR));
                iCmdState = EBusy;
                iCmdId = (DataSourcePlayback())->QueryDataSourcePosition(iSessionId, aTargetNPT, *aActualNPT, aJump);
                LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::IssueCommand %s CmdId %d ", GETNODESTR, iCmdId));
                return PVMFPending;
            }

        default:
            OSCL_ASSERT(false);
            return PVMFFailure;
    }
}

bool PVMFDownloadManagerCPMContainer::GetCPMContentAccessFactory()
{
    LOGSUBCMD((0, "PVMFDownloadManagerCPMContainer::GetCPMContentAccessFactory() In"));

    PVMFStatus status = iCPM->GetContentAccessFactory(iSessionId,
                        iCPMContentAccessFactory);
    if (status != PVMFSuccess)
    {
        return false;
    }
    return true;
}

PVMFStatus PVMFDownloadManagerCPMContainer::IssueCommand(int32 aCmd)
{
    LOGSUBCMD((0, "PVMFDownloadManagerCPMContainer::IssueCommand In"));

    OSCL_ASSERT(!CmdPending());

    //find the current node command since we may need its parameters.
    OSCL_ASSERT(iContainer->iCurrentCommand.iCmd != PVMF_GENERIC_NODE_COMMAND_INVALID);

    //save the sub-node command code
    iCmd = aCmd;

    LOGSUBCMD((0, "PVMFDownloadManagerCPMContainer::IssueCommand iCmd = %d", iCmd));

    switch (aCmd)
    {
        case ECleanup:
        {
            LOGSUBCMD((0, "PVMFDownloadManagerCPMContainer::IssueCommand Calling Cleanup"));
            Cleanup();
            return PVMFSuccess;
        }
        case ECPMInit:
        {
            OSCL_ASSERT(iCPM != NULL);
            iCmdState = EBusy;
            iCmdId = iCPM->Init();
            return PVMFPending;
        }
        case ECPMOpenSession:
        {
            OSCL_ASSERT(iCPM != NULL);
            iCmdState = EBusy;
            iCmdId = iCPM->OpenSession(iSessionId);
            return PVMFPending;
        }
        case ECPMRegisterContent:
        {
            OSCL_ASSERT(iCPM != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerCPMContainer::IssueCommand Calling RegisterContent"));
            iCmdState = EBusy;
            iCmdId = iCPM->RegisterContent(iSessionId, iContainer->iSourceURL, iContainer->iSourceFormat, iContainer->iSourceData);
            return PVMFPending;//wait on CPMCommandCompleted
        }
        case ECPMApproveUsage:
        {
            OSCL_ASSERT(iCPM != NULL);
            iCmdState = EBusy;
            iCmdId = iCPM->ApproveUsage(iSessionId,
                                        iRequestedUsage,
                                        iApprovedUsage,
                                        iAuthorizationDataKvp,
                                        iUsageID);
            return PVMFPending;
        }
        case ECPMSetDecryptInterface:
        {
            OSCL_ASSERT(iCPM != NULL);
            if (GetCPMContentAccessFactory())
            {
                PVUuid uuid = PVMFCPMPluginDecryptionInterfaceUuid;
                PVInterface* intf =
                    iCPMContentAccessFactory->CreatePVMFCPMPluginAccessInterface(uuid);
                PVMFCPMPluginAccessInterface* interimPtr =
                    OSCL_STATIC_CAST(PVMFCPMPluginAccessInterface*, intf);
                iDecryptionInterface = OSCL_STATIC_CAST(PVMFCPMPluginAccessUnitDecryptionInterface*, interimPtr);

                // SetDecryptionInterface for DTCP-IP Lib
                iContainer->iWriteFactory->SetDecryptionInterface(iDecryptionInterface);
                return PVMFSuccess;
            }
            return PVMFFailure;
        }
        case ECPMUsageComplete:
        {
            OSCL_ASSERT(iCPM != NULL);
            LOGSUBCMD((0, "PVMFDownloadManagerCPMContainer::IssueCommand Calling UsageComplete"));
            iCmdState = EBusy;
            iCmdId = iCPM->UsageComplete(iSessionId, iUsageID);
            return PVMFPending;//wait on CPMCommandCompleted
        }
        case ECPMReset:
        {
            OSCL_ASSERT(iCPM != NULL);
            if (iDecryptionInterface != NULL)
            {
                iDecryptionInterface->Reset();
                /* Remove the decrpytion interface */
                PVUuid uuid = PVMFCPMPluginDecryptionInterfaceUuid;
                iCPMContentAccessFactory->DestroyPVMFCPMPluginAccessInterface(uuid, iDecryptionInterface);
                iDecryptionInterface = NULL;
            }
            iCmdState = EBusy;
            iCmdId = iCPM->Reset();
            return PVMFPending;
        }
        default:
            OSCL_ASSERT(false);
            return PVMFFailure;
    }
}

PVMFStatus PVMFDownloadManagerRecognizerContainer::IssueCommand(int32 aCmd)
{
    //Issue a command to the Recognizer.
    //Return the completion status-- either pending, success, or failure.

    LOGSUBCMD((0, "PVMFDownloadManagerRecognizerContainer::IssueCommand In"));

    OSCL_ASSERT(!CmdPending());

    //save the sub-node command code
    iCmd = aCmd;

    switch (aCmd)
    {
        case ERecognizerStart:
        {
            PVMFStatus status = PVMFRecognizerRegistry::OpenSession(iRecognizerSessionId, (*this));
            if (status == PVMFSuccess)
            {
                //Issue the asynchronous command to the recognizer.
                iCmdState = EBusy;
                iCmdId = PVMFRecognizerRegistry::Recognize(iRecognizerSessionId,
                         *(iContainer->iReadFactory),
                         NULL,
                         iRecognizerResultVec);
                LOGSUBCMD((0, "PVMFDownloadManagerRecognizerContainer::IssueCommand Recognize Pending Cmd ID %d", iCmdId));
                return PVMFPending;
                //wait on the RecognizerCommandCompleted callback.
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_ERR, (0,
                                "PVMFDownloadManagerRecognizerContainer::IssueCommand Open Session Failed, status %d", status));
            }
            return status;
        }
        // break;   This statement was removed to avoid compiler warning for Unreachable Code

        case ERecognizerClose:
            //close the recognizer session.
        {
            PVMFStatus status = PVMFRecognizerRegistry::CloseSession(iRecognizerSessionId);
            if (status != PVMFSuccess)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_ERR, (0,
                                "PVMFDownloadManagerRecognizerContainer::IssueCommand CloseSession status %d", status));
            }
            return status;
        }

        default:
            LOGSUBCMD((0, "PVMFDownloadManagerRecognizerContainer::IssueCommand Error, Unknown Recognizer Command!"));
            OSCL_ASSERT(false);//unknown command type for recognizer.
            return PVMFFailure;
    }
}

//this is the callback from the Recognizer::Recognize command.
void PVMFDownloadManagerRecognizerContainer::RecognizerCommandCompleted(const PVMFCmdResp& aResponse)
{
    if (aResponse.GetCmdId() == iCmdId
            && iCmdState == EBusy)
    {
        //save the result.
        if (aResponse.GetCmdStatus() == PVMFSuccess
                && iRecognizerResultVec.size() > 0)
        {
            // if there is only 1, use it
            // if more than 1, check the confidence level
            if (1 == iRecognizerResultVec.size())
            {
                iContainer->iMimeType = iRecognizerResultVec[0].iRecognizedFormat;
            }
            else
            {
                // if certain, use it
                // if possible, keep looking
                bool found = false;
                for (uint32 i = 0; i < iRecognizerResultVec.size(); i++)
                {
                    if (PVMFRecognizerConfidenceCertain == iRecognizerResultVec[i].iRecognitionConfidence)
                    {
                        found = true;
                        iContainer->iMimeType = iRecognizerResultVec[i].iRecognizedFormat;
                        break;
                    }
                }

                // if Content-Type may not be known, just use the first result
                if (!found && (0 != iContainer->iContentTypeMIMEString.get_size()))
                {
                    // no certain, all possibles
                    // compare with the Content-Type hint, which is in IANA MIME string format
                    // these are file formats, does not include streaming formats
                    // @TODO: need to add the following to "pvmi/pvmf/include/pvmf_format_type.h"
                    //
                    // MP4 + 3GPP = "video/3gpp", "video/mp4", "audio/3gpp", "audio/mp4", "video/3gpp-tt"
                    // AMR = "audio/amr", "audio/amr-wb"
                    // AAC = "audio/aac", "audio/x-aac", "audio/aacp"
                    // MP3 = "audio/mpeg"
                    // WM + ASF = "video/x-ms-wmv", "video/x-ms-wm", "video/x-ms-asf","audio/x-ms-wma",
                    // RM = "video/vnd.rn-realvideo", "audio/vnd.rn-realaudio"
                    // WAV = "audio/wav", "audio/x-wav", "audio/wave"
                    //
                    // the recognizer plugins may return PV proprietary format, X-...
                    // in "pvmi/pvmf/include/pvmf_format_type.h"
                    // #define PVMF_MIME_MPEG4FF               "video/MP4"
                    // #define PVMF_MIME_AMRFF                 "X-AMR-FF"
                    // #define PVMF_MIME_AACFF                 "X-AAC-FF"
                    // #define PVMF_MIME_MP3FF                 "X-MP3-FF"
                    // #define PVMF_MIME_WAVFF                 "X-WAV-FF"
                    // #define PVMF_MIME_ASFFF                 "x-pvmf/mux/asf"
                    // #define PVMF_MIME_RMFF                  "x-pvmf/mux/rm"

                    // need case insensitive compares
                    const char* mimeStr = iContainer->iContentTypeMIMEString.get_cstr();

                    for (uint32 i = 0; !found && i < iRecognizerResultVec.size(); i++)
                    {
                        const char* recognizedStr = iRecognizerResultVec[i].iRecognizedFormat.get_cstr();
                        if (0 == oscl_CIstrcmp(recognizedStr, PVMF_MIME_MPEG4FF))
                        {
                            if ((0 == oscl_CIstrcmp(mimeStr, "video/3gpp"))    ||
                                    (0 == oscl_CIstrcmp(mimeStr, "video/mp4"))     ||
                                    (0 == oscl_CIstrcmp(mimeStr, "audio/3gpp"))    ||
                                    (0 == oscl_CIstrcmp(mimeStr, "audio/mp4"))     ||
                                    (0 == oscl_CIstrcmp(mimeStr, "video/3gpp-tt")))
                            {
                                found = true;
                                iContainer->iMimeType = iRecognizerResultVec[i].iRecognizedFormat;
                            }
                        }
                        else if (0 == oscl_CIstrcmp(recognizedStr, PVMF_MIME_MP3FF))
                        {
                            if ((0 == oscl_CIstrcmp(mimeStr, "audio/mpeg")))
                            {
                                found = true;
                                iContainer->iMimeType = iRecognizerResultVec[i].iRecognizedFormat;
                            }
                        }
                        else if (0 == oscl_CIstrcmp(recognizedStr, PVMF_MIME_AACFF))
                        {
                            if ((0 == oscl_CIstrcmp(mimeStr, "audio/aac"))   ||
                                    (0 == oscl_CIstrcmp(mimeStr, "audio/x-aac")) ||
                                    (0 == oscl_CIstrcmp(mimeStr, "audio/aacp")))
                            {
                                found = true;
                                iContainer->iMimeType = iRecognizerResultVec[i].iRecognizedFormat;
                            }
                        }
                        else if (0 == oscl_CIstrcmp(recognizedStr, PVMF_MIME_AMRFF))
                        {
                            if ((0 == oscl_CIstrcmp(mimeStr, "audio/amr"))  ||
                                    (0 == oscl_CIstrcmp(mimeStr, "audio/amr-wb")))
                            {
                                found = true;
                                iContainer->iMimeType = iRecognizerResultVec[i].iRecognizedFormat;
                            }
                        }
                        else if (0 == oscl_CIstrcmp(recognizedStr, PVMF_MIME_ASFFF))
                        {
                            if ((0 == oscl_CIstrcmp(mimeStr, "video/x-ms-wmv"))  ||
                                    (0 == oscl_CIstrcmp(mimeStr, "video/x-ms-wm"))   ||
                                    (0 == oscl_CIstrcmp(mimeStr, "video/x-ms-asf"))  ||
                                    (0 == oscl_CIstrcmp(mimeStr, "audio/x-ms-wma")))
                            {
                                found = true;
                                iContainer->iMimeType = iRecognizerResultVec[i].iRecognizedFormat;
                            }
                        }
                        else if (0 == oscl_CIstrcmp(recognizedStr, PVMF_MIME_RMFF))
                        {
                            if ((0 == oscl_CIstrcmp(mimeStr, "video/vnd.rn-realvideo"))  ||
                                    (0 == oscl_CIstrcmp(mimeStr, "audio/vnd.rn-realaudio")))
                            {
                                found = true;
                                iContainer->iMimeType = iRecognizerResultVec[i].iRecognizedFormat;
                            }
                        }
                        else if ((0 == oscl_CIstrcmp(recognizedStr, PVMF_MIME_WAVFF)))
                        {
                            if ((0 == oscl_CIstrcmp(mimeStr, "audio/wav"))    ||
                                    (0 == oscl_CIstrcmp(mimeStr, "audio/wave"))   ||
                                    (0 == oscl_CIstrcmp(mimeStr, "audio/x-wav")))
                            {
                                found = true;
                                iContainer->iMimeType = iRecognizerResultVec[i].iRecognizedFormat;
                            }
                        }
                        else
                        {
                            // some new format that this component does not know about
                            // we'll use it
                            found = true;
                            iContainer->iMimeType = iRecognizerResultVec[i].iRecognizedFormat;
                        }
                    }
                }

                // if still no match found
                // need to wait for more data and run the recognizer again, will implement this later
                // just use the first one for now
                if (!found)
                {
                    // @TODO - implement the recognizer loop later
                    iContainer->iMimeType = iRecognizerResultVec[0].iRecognizedFormat;
                }
            }
        }

        CommandDone(aResponse.GetCmdStatus(), aResponse.GetEventExtensionInterface(), aResponse.GetEventData());

        //catch completion of cancel for recognizer commands
        //since there's no cancel to the recognizer module, the cancel
        //is done whenever the current recognizer command is done.
        if (iCancelCmdState != EIdle)
        {
            CancelCommandDone(PVMFSuccess, NULL, NULL);
        }
    }
    else
    {
        OSCL_ASSERT(false);//unexpected response.
    }
}

//from PVMFNodeErrorEventObserver
void PVMFDownloadManagerSubNodeContainer::HandleNodeErrorEvent(const PVMFAsyncEvent& aEvent)
{
    //A sub-node is reporting an event.

    //print events
    switch (iType)
    {
        case EFormatParser:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_ERR, (0,
                            "PVMFDownloadManagerSubNodeContainer::HandleNodeErrorEvent Parser Node Error Event %d", aEvent.GetEventType()));
            break;
        case EProtocolEngine:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_ERR, (0,
                            "PVMFDownloadManagerSubNodeContainer::HandleNodeErrorEvent ProtocolEngine Node Error Event %d", aEvent.GetEventType()));
            break;
        case ESocket:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_ERR, (0,
                            "PVMFDownloadManagerSubNodeContainer::HandleNodeErrorEvent Socket Node Error Event %d", aEvent.GetEventType()));
            if (iContainer->iDownloadComplete)
                return; // Suppress socket node error, if the download is already complete.
            break;
        default:
            OSCL_ASSERT(false);
            break;
    }

    //duplicate any PVMF Error events from either node.
    if (IsPVMFErrCode(aEvent.GetEventType()))
        iContainer->ReportErrorEvent(aEvent.GetEventType(), aEvent.GetEventData(), aEvent.GetEventExtensionInterface());
}

#include "pvmf_protocol_engine_node_events.h"

//from PVMFNodeInfoEventObserver
void PVMFDownloadManagerSubNodeContainer::HandleNodeInformationalEvent(const PVMFAsyncEvent& aEvent)
{
    //A sub-node is reporting an event.

    //detect sub-node error states.
    if (aEvent.GetEventType() == PVMFInfoStateChanged
            && iNode->GetState() == EPVMFNodeError)
    {
        iContainer->SetState(EPVMFNodeError);
    }

    //detect important status events.
    if (iType == EProtocolEngine)
    {
        switch (aEvent.GetEventType())
        {
            case PVMFInfoBufferingComplete:
                iContainer->iDownloadComplete = true;
                iContainer->NotifyDownloadComplete();
                //not sure whether this is possible, but just in case download
                //completes before movie atom notice, go ahead and do anything
                //that was waiting on movie atom.
                if (!iContainer->iMovieAtomComplete)
                    iContainer->ContinueAfterMovieAtom();
                break;
            case PVMFPROTOCOLENGINE_INFO_MovieAtomCompleted:
                //we may be waiting on this event to continue Parser init.
                if (!iContainer->iMovieAtomComplete)
                    iContainer->ContinueAfterMovieAtom();
                if (iContainer->iDebugMode)
                {
                    iContainer->ReportInfoEvent((PVMFAsyncEvent&)aEvent);
                }
                break;
            default:
                break;
        }
    }

    //filter out events that we don't want to pass up to observer
    bool filter = false;
    if (iType == ESocket)
    {
        switch (aEvent.GetEventType())
        {
            case PVMFInfoRemoteSourceNotification:  //To let the socket node events propagate to pvengine
                filter = false;
                break;
            default:
                filter = true;
        }
    }
    else
    {
        switch (aEvent.GetEventType())
        {
            case PVMFInfoStateChanged:
                filter = true;//always ignore
                break;
            case PVMFInfoPortDeleted:
            case PVMFInfoPortCreated:
            case PVMFInfoPortConnected:
            case PVMFInfoPortDisconnected:
                if (iType != EFormatParser)
                    filter = true;//ignore port events unless from format parser
                break;
            case PVMFInfoUnderflow:
            case PVMFInfoDataReady:
            case PVMFInfoRemoteSourceNotification:
                //apply some filtering to these
                if (iContainer->FilterPlaybackEventsFromSubNodes(aEvent))
                    filter = true;
                break;
            default:
                break;
        }
    }

    //duplicate all remaining PVMFInfo events.
    if (!filter
            && IsPVMFInfoCode(aEvent.GetEventType()))
    {
        iContainer->ReportInfoEvent((PVMFAsyncEvent&)aEvent);
    }

    //just print and ignore other events
    switch (iType)
    {
        case EFormatParser:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_DEBUG, (0,
                            "PVMFDownloadManagerSubNodeContainer::HandleNodeInfoEvent Parser Node Info Event %d", aEvent.GetEventType()));
            break;
        case EProtocolEngine:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_DEBUG, (0,
                            "PVMFDownloadManagerSubNodeContainer::HandleNodeInfoEvent ProtocolEngine Node Info Event %d", aEvent.GetEventType()));
            break;
        case ESocket:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_DEBUG, (0,
                            "PVMFDownloadManagerSubNodeContainer::HandleNodeInfoEvent Socket Node Info Event %d", aEvent.GetEventType()));
            break;

        default:
            OSCL_ASSERT(false);
            break;
    }
}

bool PVMFDownloadManagerSubNodeContainer::CancelPendingCommand()
{
    //initiate sub-node command cancel, return True if cancel initiated.

    if (iCmdState != EBusy)
        return false;//nothing to cancel

    iCancelCmdState = EBusy;

    if (iNode)
    {
        LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::CancelPendingCommand Calling Cancel"));
        iCancelCmdId = iNode->CancelCommand(iSessionId, iCmdId, NULL);
        LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::CancelPendingCommand CmdId %d", iCancelCmdId));
    }

    return true; //cancel initiated
}

bool PVMFDownloadManagerCPMContainer::CancelPendingCommand()
{
    //initiate sub-node command cancel, return True if cancel initiated.
    if (iCmdState != EBusy)
        return false;//nothing to cancel

    iCancelCmdState = EBusy;

    //no cancel available-- just wait on current command to complete.
    return true; //cancel initiated
}

bool PVMFDownloadManagerRecognizerContainer::CancelPendingCommand()
{
    //initiate sub-node command cancel, return True if cancel initiated.

    if (iCmdState != EBusy)
        return false;//nothing to cancel

    iCancelCmdState = EBusy;

    LOGSUBCMD((0, "PVMFDownloadManagerRecognizerContainer::CancelPendingCommand Calling Cancel"));
    iCancelCmdId = PVMFRecognizerRegistry::CancelCommand(iRecognizerSessionId, iCmdId, NULL);
    LOGSUBCMD((0, "PVMFDownloadManagerRecognizerContainer::CancelPendingCommand CmdId %d", iCancelCmdId));

    return true; //cancel initiated
}

void PVMFDownloadManagerSubNodeContainerBase::CommandDone(PVMFStatus aStatus, PVInterface*aExtMsg, OsclAny*aEventData)
{
    //a sub-node command is done-- process the result.

    OSCL_ASSERT(aStatus != PVMFPending);

    //pop the sub-node command vector.
    OSCL_ASSERT(!iContainer->iSubNodeCmdVec.empty());
    iContainer->iSubNodeCmdVec.erase(&iContainer->iSubNodeCmdVec.front());

    iCmdState = EIdle;

    PVMFStatus status = aStatus;

    // Set "Init Failed License Required" flag with the results of parser Init.
    // Also, obtain the license interface synchronously
    if (iType == EFormatParser && iCmd == EInit)
    {
        iContainer->iInitFailedLicenseRequired = (status == PVMFErrDrmLicenseNotFound || status == PVMFErrDrmLicenseExpired);
        iContainer->iFormatParserNode.iNode->QueryInterfaceSync(iContainer->iFormatParserNode.iSessionId, PVMFCPMPluginLicenseInterfaceUuid, iContainer->iFormatParserNode.iLicenseInterface);
    }

    // Watch for the request port command completion from the protocol node, because we need to save the port pointer
    if (iType == EProtocolEngine && iCmd == ERequestPort && status == PVMFSuccess)
    {
        iContainer->iProtocolEngineNodePort = (PVMFPortInterface*)aEventData;
        // If both ports are non-null, connect them.
        if (iContainer->iSocketNodePort && iContainer->iProtocolEngineNodePort)
        {
            iContainer->iSocketNodePort->Connect(iContainer->iProtocolEngineNodePort);

            // The ports are connected, so now we pass the datastream factory to the protocol node via the extension interface, if it's available.
            if (iContainer->iProtocolEngineNode.iDatastreamUser)
            {
                ((PVMIDatastreamuserInterface*)iContainer->iProtocolEngineNode.iDatastreamUser)->PassDatastreamFactory(*(iContainer->iWriteFactory), (int32)0);
            }
        }
    }

    // Watch for the request port command completion from the socket node, because we need to save the port pointer
    if (iType == ESocket && iCmd == ERequestPort && status == PVMFSuccess)
    {
        iContainer->iSocketNodePort = (PVMFPortInterface*)aEventData;
    }

    // Watch for the query track selection interface completion from the protocol engine node.
    if (iType == EProtocolEngine && iCmd == EQueryTrackSelection)
    {
        //see whether we got the TS interface from PE node.
        iContainer->iNoPETrackSelect = (status != PVMFSuccess || iContainer->iProtocolEngineNode.iTrackSelection == NULL);
        //ignore cmd failure so it won't terminate the Init sequence
        if (status != PVMFSuccess)
            status = PVMFSuccess;
        //Continue the Init sequence now that we have the track select decision.
        iContainer->ContinueInitAfterTrackSelectDecision();
    }

    // Watch for recognizer start failure
    if (iType == ERecognizer && iCmd == ERecognizerStart && aStatus != PVMFSuccess)
    {
        iContainer->iRecognizerError = true;
        //save the error code to report after recognizer close.
        iContainer->iRecognizerStartStatus = status;
        //purge everything from the subnode command vector except the recognizer
        //close command
        iContainer->iSubNodeCmdVec.clear();
        iContainer->Push(iContainer->iRecognizerNode, PVMFDownloadManagerSubNodeContainerBase::ERecognizerClose);
        //set status to "success" so that we'll continue with processing
        status = PVMFSuccess;
    }
    // Watch for recognizer close completion after a start failure
    else if (iContainer->iRecognizerError)
    {
        OSCL_ASSERT(iCmd == ERecognizerClose);
        iContainer->iRecognizerError = false;
        //restore the original error code from the recognizer start.
        status = iContainer->iRecognizerStartStatus;
    }

    //Check whether the node command is being cancelled.
    if (iCancelCmdState != EIdle)
    {
        if (!iContainer->iSubNodeCmdVec.empty())
        {
            //even if this command succeeded, we want to report
            //the node command status as cancelled since some sub-node
            //commands were not yet issued.
            status = PVMFErrCancelled;
            //go into an error state since it's not clear
            //how to recover from a partially completed command.
            iContainer->SetState(EPVMFNodeError);
        }
    }

    //figure out the next step in the sequence...
    //A node command is done when either all sub-node commands are
    //done or when one fails.
    if (status == PVMFSuccess
            && !iContainer->iSubNodeCmdVec.empty())
    {
        //The node needs to issue the next sub-node command.
        iContainer->Reschedule();
    }
    else
    {
        //node command is done.
        OSCL_ASSERT(iContainer->iCurrentCommand.iCmd != PVMF_GENERIC_NODE_COMMAND_INVALID);
        iContainer->CommandComplete(iContainer->iCurrentCommand, status, aExtMsg, aEventData);
    }
}

void PVMFDownloadManagerSubNodeContainerBase::CancelCommandDone(PVMFStatus aStatus, PVInterface*aExtMsg, OsclAny*aEventData)
{
    OSCL_UNUSED_ARG(aExtMsg);
    OSCL_UNUSED_ARG(aEventData);
    //a sub-node cancel command is done-- process the result.

    OSCL_ASSERT(aStatus != PVMFPending);

    iCancelCmdState = EIdle;
    //print and ignore any failed sub-node cancel commands.
    if (aStatus != PVMFSuccess)
    {
        switch (iType)
        {
            case EFormatParser:
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_DEBUG, (0,
                                "PVMFDownloadManagerSubNodeContainer::CancelCommandDone Parser Node Cancel failed"));
                break;
            case EProtocolEngine:
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_DEBUG, (0,
                                "PVMFDownloadManagerSubNodeContainer::CancelCommandDone ProtocolEngine Node Cancel failed"));
                break;
            case ESocket:
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_DEBUG, (0,
                                "PVMFDownloadManagerSubNodeContainer::CancelCommandDone Socket Node Cancel failed"));
                break;
            default:
                OSCL_ASSERT(false);
                break;
        }
    }

    //Node cancel command is now done.
    OSCL_ASSERT(iContainer->iCancelCommand.iCmd != PVMF_GENERIC_NODE_COMMAND_INVALID);
    iContainer->CommandComplete(iContainer->iCancelCommand, aStatus, NULL, NULL);
}

// From PVMFCPMStatusObserver  -- this is the callback from the CPM object.
OSCL_EXPORT_REF void PVMFDownloadManagerCPMContainer::CPMCommandCompleted(const PVMFCmdResp& aResponse)
{
    //A command to the CPM node is complete

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFDownloadManagerSubNodeContainer::CPMCommandCompleted "));

    if (aResponse.GetCmdId() == iCmdId
            && iCmdState == EBusy)
    {
        CommandDone(aResponse.GetCmdStatus(), aResponse.GetEventExtensionInterface(), aResponse.GetEventData());

        //catch completion of cancel for CPM commands
        //since there's no cancel to the CPM module, the cancel
        //is done whenever the current CPM command is done.
        if (iCancelCmdState != EIdle)
        {
            CancelCommandDone(PVMFSuccess, NULL, NULL);
        }
    }
    else
    {
        OSCL_ASSERT(false);//unexpected response.
    }
}

//from PVMFNodeCmdStatusObserver
void PVMFDownloadManagerSubNodeContainer::NodeCommandCompleted(const PVMFCmdResp& aResponse)
{
    //A command to a sub-node is complete
    LOGSUBCMD((0, "PVMFDownloadManagerSubNodeContainer::NodeCommandCompleted %s () In CmdId %d Status %d", GETNODESTR, aResponse.GetCmdId(), aResponse.GetCmdStatus()));

    if (aResponse.GetCmdStatus() != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iContainer->iLogger, PVLOGMSG_DEBUG, (0, "PVMFDownloadManagerSubNodeContainer::NodeCommandCompleted Failure! %d", aResponse.GetCmdStatus()));
    }

    if (aResponse.GetCmdId() == iCmdId
            && iCmdState == EBusy)
    {
        //Process normal command response.
        CommandDone(aResponse.GetCmdStatus(), aResponse.GetEventExtensionInterface(), aResponse.GetEventData());
    }
    else if (aResponse.GetCmdId() == iCancelCmdId
             && iCancelCmdState == EBusy)
    {
        //Process node cancel command response
        CancelCommandDone(aResponse.GetCmdStatus(), aResponse.GetEventExtensionInterface(), aResponse.GetEventData());
    }
    else
    {
        OSCL_ASSERT(false);//unexpected response.
    }
}

//From capability and config interface
PVMFStatus PVMFDownloadManagerNode::getParametersSync(PvmiMIOSession aSession,
        PvmiKeyType aIdentifier,
        PvmiKvp*& aParameters,
        int& aNumParamElements,
        PvmiCapabilityContext aContext)
{
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aContext);
    // Initialize the output parameters
    aNumParamElements = 0;
    aParameters = NULL;

    // Count the number of components and parameters in the key
    int compcount = pv_mime_string_compcnt(aIdentifier);
    // Retrieve the first component from the key string
    char* compstr = NULL;
    pv_mime_string_extract_type(0, aIdentifier, compstr);

    if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf")) < 0) || compcount < 2)
    {
        // First component should be "x-pvmf" and there must
        // be at least two components to go past x-pvmf
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFDownloadManagerNode::getParametersSync() Invalid key string"));
        return PVMFErrArgument;
    }

    // Retrieve the second component from the key string
    pv_mime_string_extract_type(1, aIdentifier, compstr);

    // Check if it is key string for Download manager
    if (pv_mime_strcmp(compstr, _STRLIT_CHAR("net")) < 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFDownloadManagerNode::getParametersSync() Unsupported key"));
        return PVMFFailure;
    }

    if (compcount == 2)
    {
        // Since key is "x-pvmf/net" return all
        // nodes available at this level. Ignore attribute
        // since capability is only allowed

        // Allocate memory for the KVP list
        aParameters = (PvmiKvp*)oscl_malloc(DownloadManagerConfig_NumBaseKeys * sizeof(PvmiKvp));
        if (aParameters == NULL)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFDownloadManagerNode::getParametersSync() Memory allocation for KVP failed"));
            return PVMFErrNoMemory;
        }
        oscl_memset(aParameters, 0, DownloadManagerConfig_NumBaseKeys*sizeof(PvmiKvp));
        // Allocate memory for the key strings in each KVP
        PvmiKeyType memblock = (PvmiKeyType)oscl_malloc(DownloadManagerConfig_NumBaseKeys * DLMCONFIG_KEYSTRING_SIZE * sizeof(char));
        if (memblock == NULL)
        {
            oscl_free(aParameters);
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFDownloadManagerNode::getParametersSync() Memory allocation for key string failed"));
            return PVMFErrNoMemory;
        }
        oscl_strset(memblock, 0, DownloadManagerConfig_NumBaseKeys*DLMCONFIG_KEYSTRING_SIZE*sizeof(char));
        // Assign the key string buffer to each KVP
        uint32 j;
        for (j = 0; j < DownloadManagerConfig_NumBaseKeys; ++j)
        {
            aParameters[j].key = memblock + (j * DLMCONFIG_KEYSTRING_SIZE);
        }
        // Copy the requested info
        for (j = 0; j < DownloadManagerConfig_NumBaseKeys; ++j)
        {
            oscl_strncat(aParameters[j].key, _STRLIT_CHAR("x-pvmf/net/"), 17);
            oscl_strncat(aParameters[j].key, DownloadManagerConfig_BaseKeys[j].iString, oscl_strlen(DownloadManagerConfig_BaseKeys[j].iString));
            oscl_strncat(aParameters[j].key, _STRLIT_CHAR(";type="), 6);
            switch (DownloadManagerConfig_BaseKeys[j].iType)
            {
                case PVMI_KVPTYPE_AGGREGATE:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPTYPE_AGGREGATE_STRING), oscl_strlen(PVMI_KVPTYPE_AGGREGATE_STRING));
                    break;

                case PVMI_KVPTYPE_POINTER:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPTYPE_POINTER_STRING), oscl_strlen(PVMI_KVPTYPE_POINTER_STRING));
                    break;

                case PVMI_KVPTYPE_VALUE:
                default:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPTYPE_VALUE_STRING), oscl_strlen(PVMI_KVPTYPE_VALUE_STRING));
                    break;
            }
            oscl_strncat(aParameters[j].key, _STRLIT_CHAR(";valtype="), 9);
            switch (DownloadManagerConfig_BaseKeys[j].iValueType)
            {
                case PVMI_KVPVALTYPE_RANGE_INT32:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_RANGE_INT32_STRING), oscl_strlen(PVMI_KVPVALTYPE_RANGE_INT32_STRING));
                    break;

                case PVMI_KVPVALTYPE_KSV:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_KSV_STRING), oscl_strlen(PVMI_KVPVALTYPE_KSV_STRING));
                    break;

                case PVMI_KVPVALTYPE_CHARPTR:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_CHARPTR_STRING), oscl_strlen(PVMI_KVPVALTYPE_CHARPTR_STRING));
                    break;

                case PVMI_KVPVALTYPE_WCHARPTR:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_WCHARPTR_STRING), oscl_strlen(PVMI_KVPVALTYPE_WCHARPTR_STRING));
                    break;

                case PVMI_KVPVALTYPE_BOOL:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_BOOL_STRING), oscl_strlen(PVMI_KVPVALTYPE_BOOL_STRING));
                    break;

                case PVMI_KVPVALTYPE_UINT32:
                default:
                    oscl_strncat(aParameters[j].key, _STRLIT_CHAR(PVMI_KVPVALTYPE_UINT32_STRING), oscl_strlen(PVMI_KVPVALTYPE_UINT32_STRING));
                    break;
            }
            aParameters[j].key[DLMCONFIG_KEYSTRING_SIZE-1] = 0;
        }
        aNumParamElements = DownloadManagerConfig_NumBaseKeys;
    }
    else if (compcount == 3)
    {
        pv_mime_string_extract_type(2, aIdentifier, compstr);

        // Determine what is requested
        PvmiKvpAttr reqattr = GetAttrTypeFromKeyString(aIdentifier);
        if (reqattr == PVMI_KVPATTR_UNKNOWN)
        {
            reqattr = PVMI_KVPATTR_CUR;
        }
        uint i;
        for (i = 0; i < DownloadManagerConfig_NumBaseKeys; i++)
        {
            if (pv_mime_strcmp(compstr, (char*)(DownloadManagerConfig_BaseKeys[i].iString)) >= 0)
            {
                break;
            }
        }

        if (i == DownloadManagerConfig_NumBaseKeys)
        {
            // no match found
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFDownloadManagerNode::getParametersSync() Unsupported key"));
            return PVMFErrNoMemory;
        }
    }
    else
    {
        oscl_free(aParameters);
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFDownloadManagerNode::getParametersSync() Unsupported key"));
        return PVMFErrNoMemory;
    }

    return PVMFSuccess;
}


PVMFStatus PVMFDownloadManagerNode::releaseParameters(PvmiMIOSession aSession,
        PvmiKvp* aParameters, int num_elements)
{
    OSCL_UNUSED_ARG(aSession);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFDownloadManagerNode::releaseParameters() In"));

    if (aParameters == NULL || num_elements < 1)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFDownloadManagerNode::releaseParameters() KVP list is NULL or number of elements is 0"));
        return PVMFErrArgument;
    }

    // Count the number of components and parameters in the key
    int compcount = pv_mime_string_compcnt(aParameters[0].key);
    // Retrieve the first component from the key string
    char* compstr = NULL;
    pv_mime_string_extract_type(0, aParameters[0].key, compstr);

    if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf")) < 0) || compcount < 2)
    {
        // First component should be "x-pvmf" and there must
        // be at least two components to go past x-pvmf
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFDownloadManagerNode::releaseParameters() Unsupported key"));
        return PVMFErrArgument;
    }

    // Retrieve the second component from the key string
    pv_mime_string_extract_type(1, aParameters[0].key, compstr);

    // Assume all the parameters come from the same component so the base components are the same
    if (pv_mime_strcmp(compstr, _STRLIT_CHAR("net")) >= 0)
    {
        // Go through each KVP and release memory for value if allocated from heap
        for (int32 i = 0; i < num_elements; ++i)
        {
            // Next check if it is a value type that allocated memory
            PvmiKvpType kvptype = GetTypeFromKeyString(aParameters[i].key);
            if (kvptype == PVMI_KVPTYPE_VALUE || kvptype == PVMI_KVPTYPE_UNKNOWN)
            {
                PvmiKvpValueType keyvaltype = GetValTypeFromKeyString(aParameters[i].key);
                if (keyvaltype == PVMI_KVPVALTYPE_UNKNOWN)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFDownloadManagerNode::releaseParameters() Valtype not specified in key string"));
                    return PVMFErrArgument;
                }

                if (keyvaltype == PVMI_KVPVALTYPE_CHARPTR && aParameters[i].value.pChar_value != NULL)
                {
                    oscl_free(aParameters[i].value.pChar_value);
                    aParameters[i].value.pChar_value = NULL;
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_WCHARPTR && aParameters[i].value.pWChar_value != NULL)
                {
                    oscl_free(aParameters[i].value.pWChar_value);
                    aParameters[i].value.pWChar_value = NULL;
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_CHARPTR && aParameters[i].value.pChar_value != NULL)
                {
                    oscl_free(aParameters[i].value.pChar_value);
                    aParameters[i].value.pChar_value = NULL;
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_KSV && aParameters[i].value.key_specific_value != NULL)
                {
                    oscl_free(aParameters[i].value.key_specific_value);
                    aParameters[i].value.key_specific_value = NULL;
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_RANGE_INT32 && aParameters[i].value.key_specific_value != NULL)
                {
                    range_int32* ri32 = (range_int32*)aParameters[i].value.key_specific_value;
                    aParameters[i].value.key_specific_value = NULL;
                    oscl_free(ri32);
                }
                else if (keyvaltype == PVMI_KVPVALTYPE_RANGE_UINT32 && aParameters[i].value.key_specific_value != NULL)
                {
                    range_uint32* rui32 = (range_uint32*)aParameters[i].value.key_specific_value;
                    aParameters[i].value.key_specific_value = NULL;
                    oscl_free(rui32);
                }
            }
        }

        oscl_free(aParameters[0].key);

        // Free memory for the parameter list
        oscl_free(aParameters);
        aParameters = NULL;
    }
    else
    {
        // Unknown key string
        return PVMFErrArgument;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFDownloadManagerNode::releaseParameters() Out"));
    return PVMFSuccess;
}

void PVMFDownloadManagerNode::setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
        int num_elements, PvmiKvp * & aRet_kvp)
{
    OSCL_UNUSED_ARG(aSession);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFDownloadManagerNode::setParametersSync() In"));

    aRet_kvp = NULL;

    // Go through each parameter
    for (int paramind = 0; paramind < num_elements; ++paramind)
    {
        // Count the number of components and parameters in the key
        int compcount = pv_mime_string_compcnt(aParameters[paramind].key);

        // Retrieve the first component from the key string
        char* compstr = NULL;
        pv_mime_string_extract_type(0, aParameters[paramind].key, compstr);

        if ((pv_mime_strcmp(compstr, _STRLIT_CHAR("x-pvmf")) < 0) || compcount < 2)
        {
            // First component should be "x-pvmf" and there must
            // be at least two components to go past x-pvmf
            aRet_kvp = &aParameters[paramind];
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFDownloadManagerNode::setParametersSync() Unsupported key"));
            return;
        }

        // Retrieve the second component from the key string
        pv_mime_string_extract_type(1, aParameters[paramind].key, compstr);

        // First check if it is key string for the Download manager
        if (pv_mime_strcmp(compstr, _STRLIT_CHAR("net")) >= 0)
        {
            if (compcount == 3)
            {
                pv_mime_string_extract_type(2, aParameters[paramind].key, compstr);
                uint i;
                for (i = 0; i < DownloadManagerConfig_NumBaseKeys; i++)
                {
                    if (pv_mime_strcmp(compstr, (char*)(DownloadManagerConfig_BaseKeys[i].iString)) >= 0)
                    {
                        break;
                    }
                }

                if (DownloadManagerConfig_NumBaseKeys == i)
                {
                    // invalid third component
                    aRet_kvp = &aParameters[paramind];
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "PVMFDownloadManagerNode::setParametersSync() Unsupported key"));
                    return;
                }

                // Verify and set the passed-in setting
                switch (i)
                {
                    case BASEKEY_SESSION_CONTROLLER_USER_AGENT:
                    {
                        if (IsDownloadExtensionHeaderValid(*aParameters))
                        {
                            //setting KVP string for download when mode applied for download or not applied at all.
                            OSCL_wHeapString<OsclMemAllocator> userAgent;
                            userAgent = aParameters[paramind].value.pWChar_value;
                            (iProtocolEngineNode.ProtocolEngineExtension())->SetUserAgent(userAgent, true);
                        }
                    }
                    break;
                    case BASEKEY_SESSION_CONTROLLER_HTTP_VERSION:
                    {
                        uint32 httpVersion;
                        httpVersion = aParameters[paramind].value.uint32_value;
                        (iProtocolEngineNode.ProtocolEngineExtension())->SetHttpVersion(httpVersion);

                    }
                    break;
                    case BASEKEY_SESSION_CONTROLLER_HTTP_TIMEOUT:
                    {
                        uint32 httpTimeout;
                        httpTimeout = aParameters[paramind].value.uint32_value;
                        (iProtocolEngineNode.ProtocolEngineExtension())->SetNetworkTimeout(httpTimeout);
                    }
                    break;
                    case BASEKEY_SESSION_CONTROLLER_DOWNLOAD_PROGRESS_INFO:
                    {
                        OSCL_HeapString<OsclMemAllocator> downloadProgressInfo;
                        downloadProgressInfo = aParameters[paramind].value.pChar_value;
                        DownloadProgressMode aMode = DownloadProgressMode_TimeBased;
                        if (IsByteBasedDownloadProgress(downloadProgressInfo)) aMode = DownloadProgressMode_ByteBased;
                        (iProtocolEngineNode.ProtocolEngineExtension())->SetDownloadProgressMode(aMode);
                    }
                    break;
                    case BASEKEY_SESSION_CONTROLLER_PROTOCOL_EXTENSION_HEADER:
                    {
                        if (IsDownloadExtensionHeaderValid(aParameters[paramind]))
                        {
                            OSCL_HeapString<OsclMemAllocator> extensionHeaderKey;
                            OSCL_HeapString<OsclMemAllocator> extensionHeaderValue;
                            HttpMethod httpMethod = HTTP_GET;
                            bool aPurgeOnRedirect = false;
                            if (GetHttpExtensionHeaderParams(aParameters[paramind],
                                                             extensionHeaderKey,
                                                             extensionHeaderValue,
                                                             httpMethod,
                                                             aPurgeOnRedirect))
                            {
                                (iProtocolEngineNode.ProtocolEngineExtension())->SetHttpExtensionHeaderField(extensionHeaderKey,
                                        extensionHeaderValue,
                                        httpMethod,
                                        aPurgeOnRedirect);
                            }
                        }

                    }
                    break;

                    case BASEKEY_SESSION_CONTROLLER_NUM_REDIRECT_ATTEMPTS:
                    {
                        if (IsDownloadExtensionHeaderValid(*aParameters))
                        {
                            //setting KVP string for download when mode applied for download or not applied at all.
                            uint32 numRedirects = aParameters[paramind].value.uint32_value;
                            (iProtocolEngineNode.ProtocolEngineExtension())->SetNumRedirectTrials(numRedirects);
                        }
                    }
                    break;

                    case BASEKEY_SESSION_CONTROLLER_NUM_HTTP_HEADER_REQUEST_DISABLED:
                    {
                        bool httpHeaderRequestDisabled = aParameters[paramind].value.bool_value;
                        (iProtocolEngineNode.ProtocolEngineExtension())->DisableHttpHeadRequest(httpHeaderRequestDisabled);
                    }
                    break;

                    case BASEKEY_MAX_TCP_RECV_BUFFER_SIZE:
                    {
                        uint32 size = aParameters[paramind].value.uint32_value;
                        PVMFSocketNode* socketNode =
                            (PVMFSocketNode*)(iSocketNode.iNode);
                        if (socketNode != NULL)
                        {
                            socketNode->SetMaxTCPRecvBufferSize(size);
                        }
                    }
                    break;

                    case BASEKEY_MAX_TCP_RECV_BUFFER_COUNT:
                    {
                        uint32 size = aParameters[paramind].value.uint32_value;
                        PVMFSocketNode* socketNode =
                            (PVMFSocketNode*)(iSocketNode.iNode);
                        if (socketNode != NULL)
                        {
                            socketNode->SetMaxTCPRecvBufferCount(size);
                        }
                    }
                    break;

                    default:
                    {
                        aRet_kvp = &aParameters[paramind];
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "PVMFDownloadManagerNode::setParametersSync() Setting "
                                         "parameter %d failed", paramind));
                    }
                    break;
                }
            }
            else
            {
                // Do not support more than 3 components right now
                aRet_kvp = &aParameters[paramind];
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "PVMFDownloadManagerNode::setParametersSync() Unsupported key"));
                return;
            }
        }
        else
        {
            // Unknown key string
            aRet_kvp = &aParameters[paramind];
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "PVMFDownloadManagerNode::setParametersSync() Unsupported key"));
            return;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFDownloadManagerNode::setParametersSync() Out"));
}

bool PVMFDownloadManagerNode::IsByteBasedDownloadProgress(OSCL_String &aDownloadProgressInfo)
{
    if (aDownloadProgressInfo.get_size() < 4) return false; // 4 => byte
    char *ptr = (char*)aDownloadProgressInfo.get_cstr();
    uint32 len = aDownloadProgressInfo.get_size();

    while (!(((ptr[0]  | OSCL_ASCII_CASE_MAGIC_BIT) == 'b') &&
             ((ptr[1]  | OSCL_ASCII_CASE_MAGIC_BIT) == 'y') &&
             ((ptr[2]  | OSCL_ASCII_CASE_MAGIC_BIT) == 't') &&
             ((ptr[3]  | OSCL_ASCII_CASE_MAGIC_BIT) == 'e')) &&
            len >= 4)
    {
        ptr++;
        len--;
    }
    if (len < 4) return false;

    return true; // find case-insentive string "byte"
}

bool PVMFDownloadManagerNode::GetHttpExtensionHeaderParams(PvmiKvp &aParameter,
        OSCL_String &extensionHeaderKey,
        OSCL_String &extensionHeaderValue,
        HttpMethod  &httpMethod,
        bool &aPurgeOnRedirect)
{
    // check if the extension header is meant for download
    if (!IsHttpExtensionHeaderValid(aParameter)) return false;

    // get aPurgeOnRedirect
    aPurgeOnRedirect = false;
    OSCL_StackString<32> purgeOnRedirect(_STRLIT_CHAR("purge-on-redirect"));
    if (oscl_strstr(aParameter.key, purgeOnRedirect.get_cstr()) != NULL)
    {
        aPurgeOnRedirect = true;
    }

    // get key, value and http method of protocol extension header
    // the string value needs to be structured as follows: "key=app-feature-tag;value=xyz"
    char* extensionHeader = aParameter.value.pChar_value;
    if (!extensionHeader) return false;

    // (1) extract the key
    OSCL_StackString<8> keyTag(_STRLIT_CHAR("key="));

    OSCL_StackString<8> valueTag(_STRLIT_CHAR("value="));
    char *keyStart = OSCL_CONST_CAST(char*, oscl_strstr(extensionHeader, keyTag.get_cstr()));
    if (!keyStart) return false;

    keyStart += keyTag.get_size();
    char *keyEnd = OSCL_CONST_CAST(char*, oscl_strstr(extensionHeader, valueTag.get_cstr()));
    if (!keyEnd) return false;
    uint32 keyLen = getItemLen(keyStart, keyEnd);
    if (keyLen == 0) return false;
    extensionHeaderKey = OSCL_HeapString<OsclMemAllocator> (keyStart, keyLen);

    // (2) extract the value
    char* valueStart = keyEnd;
    valueStart += valueTag.get_size();

    OSCL_StackString<8> methodTag(_STRLIT_CHAR("method="));
    char* valueEnd = OSCL_CONST_CAST(char*, oscl_strstr(valueStart, methodTag.get_cstr()));
    if (!valueEnd) valueEnd = extensionHeader + aParameter.capacity;
    uint32 valueLen = getItemLen(valueStart, valueEnd);
    extensionHeaderValue = OSCL_HeapString<OsclMemAllocator> (valueStart, valueLen);

    // (3) check for optional method
    const char *methodStart = oscl_strstr(extensionHeader, methodTag.get_cstr());
    if (!methodStart)
    {
        httpMethod = HTTP_GET;
        return true;
    }
    methodStart += methodTag.get_size();

    OSCL_StackString<8> methodHttpGet(_STRLIT_CHAR("GET"));
    OSCL_StackString<8> methodHttpHead(_STRLIT_CHAR("HEAD"));
    OSCL_StackString<8> methodHttpPost(_STRLIT_CHAR("POST"));

    const char* methodGet = oscl_strstr(methodStart, methodHttpGet.get_cstr());
    const char* methodHead = oscl_strstr(methodStart, methodHttpHead.get_cstr());
    const char* methodPost = oscl_strstr(methodStart, methodHttpPost.get_cstr());

    httpMethod = HTTP_GET;
    if (methodPost != NULL) httpMethod = HTTP_POST;
    if (methodGet  != NULL) httpMethod = HTTP_GET;
    if (methodHead != NULL) httpMethod = HTTP_HEAD;
    if ((methodGet != NULL) && (methodHead != NULL)) httpMethod = HTTP_ALLMETHOD;

    return true;
}

bool PVMFDownloadManagerNode::IsHttpExtensionHeaderValid(PvmiKvp &aParameter)
{
    OSCL_StackString<32> downloadMode(_STRLIT_CHAR("mode=download"));
    OSCL_StackString<32> streamingMode(_STRLIT_CHAR("mode=streaming"));

    bool isDownloadMode  = (oscl_strstr(aParameter.key, downloadMode.get_cstr())  != NULL);
    bool isStreamingMode = (oscl_strstr(aParameter.key, streamingMode.get_cstr()) != NULL);

    // streaming mode only would fail, download mode specified or not specified will be viewed as true
    if (isStreamingMode && !isDownloadMode) return false;

    return true;
}

// remove the ending ';', ',' or ' ' and calulate value length
uint32 PVMFDownloadManagerNode::getItemLen(char *ptrItemStart, char *ptrItemEnd)
{
    char *ptr = ptrItemEnd - 1;
    uint32 itemLen = ptr - ptrItemStart;
    for (uint32 i = 0; i < itemLen; i++)
    {
        if (*ptr == ';' || *ptr == ',' || *ptr == ' ') --ptr;
        else break;
    }
    itemLen = ptr - ptrItemStart + 1;
    return itemLen;
}


bool PVMFDownloadManagerNode::IsDownloadExtensionHeaderValid(PvmiKvp &aParameter)
{
    OSCL_StackString<32> downloadMode(_STRLIT_CHAR("mode=download"));
    OSCL_StackString<32> streamingMode(_STRLIT_CHAR("mode=streaming"));
    OSCL_StackString<32> dlaMode(_STRLIT_CHAR("mode=dla"));

    bool isDownloadMode  = (oscl_strstr(aParameter.key, downloadMode.get_cstr())  != NULL);
    bool isStreamingMode = (oscl_strstr(aParameter.key, streamingMode.get_cstr()) != NULL);
    bool isDlaMode = (oscl_strstr(aParameter.key, dlaMode.get_cstr()) != NULL);


    // streaming mode only would fail, download mode specified or not specified will be viewed as true
    if (isStreamingMode && !isDownloadMode) return false;

    // dla mode only would fail, download mode specified or not specified will be viewed as true
    if (isDlaMode && !isDownloadMode) return false;

    return true;
}

void PVMFDownloadManagerNode::NotificationsInterfaceDestroyed()
{
    iClockNotificationsInf = NULL;
}

void PVMFDownloadManagerNode::ClockStateUpdated()
{
    if (!iDataReady)
    {
        // Don't let anyone start the clock while the source node is in underflow
        if (iPlayBackClock != NULL)
        {
            if (iPlayBackClock->GetState() == PVMFMediaClock::RUNNING)
            {
                iPlayBackClock->Pause();
            }
        }
    }
}

// From PVMFNodeInterfaceImpl
PVMFStatus PVMFDownloadManagerNode::HandleExtensionAPICommands()
{
    PVMFStatus status = PVMFErrNotSupported;
    switch (iCurrentCommand.iCmd)
    {
            // Based on the command under processing, corresponding action
            // in ScheduleSubNodeCommands() is executed.

        case PVMF_GENERIC_NODE_SET_DATASOURCE_POSITION:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "processing SetDataSourcePosition in ScheduleSubNodeCommands()"));
            status = ScheduleSubNodeCommands();
            break;

        case PVMF_GENERIC_NODE_QUERY_DATASOURCE_POSITION:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "processing QueryDataSourcePosition in ScheduleSubNodeCommands()"));
            status = ScheduleSubNodeCommands();
            break;

        case PVMF_GENERIC_NODE_GETNODEMETADATAKEYS:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "processing GetNodeMetadataKey in ScheduleSubNodeCommands()"));
            status = ScheduleSubNodeCommands();
            break;

        case PVMF_GENERIC_NODE_GETNODEMETADATAVALUES:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "processing GetNodeMetadataValue in ScheduleSubNodeCommands()"));
            status = ScheduleSubNodeCommands();
            break;

        case PVMF_GENERIC_NODE_SET_DATASOURCE_RATE:
            status = PVMFErrNotSupported;
            break;

        default:
            // unknown command type, assert false
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFDownloadManagerNode::HandleExtensionAPICommands() - Unknown Cmd type %d; Cmd Id %d", iCurrentCommand.iCmd, iCurrentCommand.iId));
            OSCL_ASSERT(false);
            break;
    }
    return status;
}

PVMFStatus PVMFDownloadManagerNode::CancelCurrentCommand()
{
    if (iFormatParserNode.CancelPendingCommand() ||
            iProtocolEngineNode.CancelPendingCommand() ||
            iSocketNode.CancelPendingCommand() ||
            iRecognizerNode.CancelPendingCommand() ||
            iCPMNode.CancelPendingCommand())
    {
        return PVMFPending;//wait on sub-node cancel to complete.
    }

    CommandComplete(iCurrentCommand, PVMFErrCancelled, NULL, NULL);
    return PVMFSuccess;
}

#if(PVMF_DOWNLOADMANAGER_SUPPORT_PPB)
#if(PV_HAS_SHOUTCAST_SUPPORT_ENABLED)

// if the a URL is successfully retrieved from the PLS file,
//   iSourceURL will contain the URL,
//   iSourceFormat will be set to PVMF_MIME_DATA_SOURCE_SHOUTCAST_URL,
//   iSourceData will point to a new PVMFSourceContextData, created for the PE node
PVMFStatus PVMFDownloadManagerNode::ParsePLSFile(OSCL_wString& aPLSFile)
{
    // have a Shoutcast playlist file
    // need to parse it and extract the first station URL
    PVPLSFFParser* plsParser = OSCL_NEW(PVPLSFFParser, ());
    if (plsParser == NULL)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                        "PVMFDownloadManagerNode:ParsePLSFile() Can't instantiate PVPLSFFParser"));
        return PVMFFailure;
    }

    PVPLSParserReturnCode retCode = plsParser->ParseFile(aPLSFile);
    if (retCode != PVPLSFF_PARSER_OK)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                        "PVMFDownloadManagerNode:ParsePLSFile() PVPLSFFParser::ParseFile failed %d", retCode));
        OSCL_DELETE(plsParser);
        return PVMFFailure;
    }

    retCode = plsParser->GetFileInfo(iPLSFileInfo);
    if (retCode != PVPLSFF_PARSER_OK)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                        "PVMFDownloadManagerNode:ParsePLSFile() PVPLSFFParser::GetFileInfo failed %d", retCode));

        OSCL_DELETE(plsParser);
        return PVMFFailure;
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                        "PVMFDownloadManagerNode:ParsePLSFile() PVPLSFFParser::GetFileInfo  %d", iPLSFileInfo.numOfEntries, iPLSFileInfo.versionNum));

        if ((iPLSFileInfo.numOfEntries <= 0) || (iPLSFileInfo.versionNum != 2))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                            "PVMFDownloadManagerNode:ParsePLSFile() PVPLSFFParser::GetFileInfo error"));
            OSCL_DELETE(plsParser);
            return PVMFFailure;
        }
    }

    // get first entry
    retCode = plsParser->GetEntry(iPLSEntry, 1);
    OSCL_DELETE(plsParser);

    if (retCode != PVPLSFF_PARSER_OK)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR, (0,
                        "PVMFDownloadManagerNode:SetSourceInitializationData() PVPLSFFParser::GetEntry %d failed %d", 1, retCode));
        return PVMFFailure;
    }

    // set Shoutcast URL
    iSourceURL = iPLSEntry.iUrl->get_str();
    // set the source format to PVMF_MIME_DATA_SOURCE_SHOUTCAST_URL
    iSourceFormat = PVMF_MIME_DATA_SOURCE_SHOUTCAST_URL;
    // create aSourceData
    if (iPLSSessionContextData != NULL)
    {
        OSCL_DELETE(iPLSSessionContextData);
        iPLSSessionContextData = NULL;
    }
    iPLSSessionContextData = OSCL_NEW(PVMFSourceContextData, ());
    iPLSSessionContextData->EnableCommonSourceContext();
    iPLSSessionContextData->EnableDownloadHTTPSourceContext();
    iPLSSessionContextData->DownloadHTTPData()->bIsNewSession = true;
    iPLSSessionContextData->DownloadHTTPData()->iConfigFileName = NULL;
    iPLSSessionContextData->DownloadHTTPData()->iDownloadFileName = NULL;
    iPLSSessionContextData->DownloadHTTPData()->iMaxFileSize = 0xFFFFFFFF;
    iPLSSessionContextData->DownloadHTTPData()->iProxyName = _STRLIT_CHAR("");
    iPLSSessionContextData->DownloadHTTPData()->iProxyPort = 0;
    iPLSSessionContextData->DownloadHTTPData()->iPlaybackControl = PVMFSourceContextDataDownloadHTTP::ENoSaveToFile;

    iSourceData = iPLSSessionContextData;

    return PVMFSuccess;
}

#endif//PV_HAS_SHOUTCAST_SUPPORT_ENABLED
#endif//PVMF_DOWNLOADMANAGER_SUPPORT_PPB

/*
 * Copyright (c) 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "audio_input"
#include <utils/Log.h>
#include <utils/threads.h>

#include "oscl_base.h"
#include "android_audio_input.h"
#include "pv_mime_string_utils.h"
#include "oscl_dll.h"

#include <media/AudioRecord.h>
#include <sys/prctl.h>

#ifndef PVMF_BASIC_ERRORINFOMESSAGE_H_INCLUDED
#include "pvmf_basic_errorinfomessage.h"
#endif
#ifndef PVMF_MEDIA_INPUT_NODE_FACTORY_H_INCLUDED
#include "pvmf_media_input_node_factory.h"  // For the PvmfMediaInputNodeUuid
#endif

using namespace android;

// TODO: get buffer size from AudioFlinger
static const int kBufferSize = 2048;

// Define entry point for this DLL
OSCL_DLL_ENTRY_POINT_DEFAULT()

// At the start of a recording we are going to mute the first 300ms in
// order to eliminate recording of the videorecorder signaltone.
static const int32 AUTO_RAMP_START_MS = 300;

// After the initial mute we're going to linearly ramp up the volume
// over the next 300ms.
static const int32 AUTO_RAMP_DURATION_MS = 300;

////////////////////////////////////////////////////////////////////////////
AndroidAudioInput::AndroidAudioInput(uint32 audioSource)
    : OsclTimerObject(OsclActiveObject::EPriorityNominal, "AndroidAudioInput"),
    iCmdIdCounter(0),
    iPeer(NULL),
    iThreadLoggedOn(false),
    iAudioNumChannels(DEFAULT_AUDIO_NUMBER_OF_CHANNELS),
    iAudioSamplingRate(DEFAULT_AUDIO_SAMPLING_RATE),
    iAudioSource(audioSource),
    iDataEventCounter(0),
    iWriteCompleteAO(NULL),
    iTimeStamp(0),
    iMediaBufferMemPool(NULL),
    iState(STATE_IDLE),
    iMaxAmplitude(0),
    iTrackMaxAmplitude(false),
    iAudioThreadStarted(false)
{
    LOGV("AndroidAudioInput constructor %p", this);
    // semaphore used to communicate between this  mio and the audio output thread
    iAudioThreadSem = new OsclSemaphore();
    iAudioThreadSem->Create(0);
    iAudioThreadTermSem = new OsclSemaphore();
    iAudioThreadTermSem->Create(0);

    iAudioThreadStartLock = new Mutex();
    iAudioThreadStartCV = new Condition();

    //locks to access the queues by this mio and by the audio output thread
    iWriteResponseQueueLock.Create();
    iOSSRequestQueueLock.Create();


    {
        iAudioFormat=PVMF_MIME_FORMAT_UNKNOWN;
        iExitAudioThread=false;

        iCommandCounter=0;
        iCommandResponseQueue.reserve(5);
        iWriteResponseQueue.reserve(5);
        iOSSRequestQueue.reserve(5);
        iObserver=NULL;
        iWriteBusy=false;
        iWriteBusySeqNum=0;
    }
}

////////////////////////////////////////////////////////////////////////////
AndroidAudioInput::~AndroidAudioInput()
{
    LOGV("AndroidAudioInput destructor %p", this);

    if(iWriteCompleteAO)
    {
        OSCL_DELETE(iWriteCompleteAO);
        iWriteCompleteAO = NULL;
    }

    if(iMediaBufferMemPool)
    {
        OSCL_DELETE(iMediaBufferMemPool);
        iMediaBufferMemPool = NULL;
    }

    iWriteResponseQueueLock.Close();
    iOSSRequestQueueLock.Close();
    iAudioThreadSem->Close();
    delete iAudioThreadSem;
    iAudioThreadTermSem->Close();
    delete iAudioThreadTermSem;

    delete iAudioThreadStartLock;
    delete iAudioThreadStartCV;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus AndroidAudioInput::connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver)
{
    LOGV("connect");

    if(!aObserver)
    {
        LOGV("aObserver is NULL");
        return PVMFFailure;
    }

    int32 err = 0;
    OSCL_TRY(err, iObservers.push_back(aObserver));
    OSCL_FIRST_CATCH_ANY(err, return PVMFErrNoMemory);
    aSession = (PvmiMIOSession)(iObservers.size() - 1); // Session ID is the index of observer in the vector
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus AndroidAudioInput::disconnect(PvmiMIOSession aSession)
{
    LOGV("disconnect");
    uint32 index = (uint32)aSession;
    if(index >= iObservers.size())
    {
        // Invalid session ID
        return PVMFFailure;
    }

    iObservers.erase(iObservers.begin()+index);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PvmiMediaTransfer* AndroidAudioInput::createMediaTransfer(PvmiMIOSession& aSession,
        PvmiKvp* read_formats,
        int32 read_flags,
        PvmiKvp* write_formats,
        int32 write_flags)
{
    LOGV("createMediaTransfer %p", this);
    OSCL_UNUSED_ARG(read_formats);
    OSCL_UNUSED_ARG(read_flags);
    OSCL_UNUSED_ARG(write_formats);
    OSCL_UNUSED_ARG(write_flags);

    uint32 index = (uint32)aSession;
    if(index >= iObservers.size())
    {
        LOGV("Invalid sessions ID: index %d, size%d", index, iObservers.size());
        // Invalid session ID
        OSCL_LEAVE(OsclErrArgument);
        return NULL;
    }

    iWriteCompleteAO = OSCL_NEW(AndroidAudioInputThreadSafeCallbackAO,(this,5));

    return (PvmiMediaTransfer*)this;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::deleteMediaTransfer(PvmiMIOSession& aSession,
        PvmiMediaTransfer* media_transfer)
{
    LOGV("deleteMediaTransfer %p", this);
    uint32 index = (uint32)aSession;
    if(!media_transfer || index >= iObservers.size())
    {
        LOGV("Invalid sessions ID: index %d, size%d", index, iObservers.size());
        // Invalid session ID
        OSCL_LEAVE(OsclErrArgument);
    }
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::QueryUUID(const PvmfMimeString& aMimeType,
        Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids,
        bool aExactUuidsOnly,
        const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aMimeType);
    OSCL_UNUSED_ARG(aExactUuidsOnly);

    int32 err = 0;
    OSCL_TRY(err, aUuids.push_back(PVMI_CAPABILITY_AND_CONFIG_PVUUID););
    OSCL_FIRST_CATCH_ANY(err, OSCL_LEAVE(OsclErrNoMemory););

    return AddCmdToQueue(AI_CMD_QUERY_UUID, aContext);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::QueryInterface(const PVUuid& aUuid,
        PVInterface*& aInterfacePtr,
        const OsclAny* aContext)
{
    if(aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*,this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else
    {
        aInterfacePtr = NULL;
    }

    return AddCmdToQueue(AI_CMD_QUERY_INTERFACE, aContext, (OsclAny*)&aInterfacePtr);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::Init(const OsclAny* aContext)
{
    LOGV("Init");
    if(iState != STATE_IDLE)
    {
        LOGV("Invalid state");
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(AI_CMD_INIT, aContext);
}


////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::Start(const OsclAny* aContext)
{
    LOGV("Start");
    if(iState != STATE_INITIALIZED && iState != STATE_PAUSED)
    {
        LOGV("Invalid state");
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(AI_CMD_START, aContext);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::Pause(const OsclAny* aContext)
{
    LOGV("Pause");
    if(iState != STATE_STARTED)
    {
        LOGV("Invalid state");
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(AI_CMD_PAUSE, aContext);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::Flush(const OsclAny* aContext)
{
    LOGV("Flush");
    if(iState != STATE_STARTED || iState != STATE_PAUSED)
    {
        LOGV("Invalid state");
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(AI_CMD_FLUSH, aContext);
}

OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::Reset(const OsclAny* aContext)
{
    LOGV("Reset");
    return AddCmdToQueue(AI_CMD_RESET, aContext);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::DiscardData(PVMFTimestamp aTimestamp, const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    OSCL_UNUSED_ARG(aTimestamp);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::DiscardData(const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}


////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::Stop(const OsclAny* aContext)
{
    LOGV("Stop %p", this);
    if(iState != STATE_STARTED && iState != STATE_PAUSED)
    {
        LOGV("Invalid state");
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(AI_CMD_STOP, aContext);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::ThreadLogon()
{
    LOGV("ThreadLogon %p", this);
    if(!iThreadLoggedOn)
    {
        AddToScheduler();
        iThreadLoggedOn = true;
    }
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::ThreadLogoff()
{
    LOGV("ThreadLogoff");
    if(iThreadLoggedOn)
    {
        RemoveFromScheduler();
        iThreadLoggedOn = false;
    }
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::CancelAllCommands( const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::CancelCommand( PVMFCommandId aCmdId, const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aCmdId);
    OSCL_UNUSED_ARG(aContext);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::setPeer(PvmiMediaTransfer* aPeer)
{
    LOGV("setPeer");
    if(iPeer && aPeer)
    {
        OSCL_LEAVE(OsclErrGeneral);
        return;
    }

    iPeer = aPeer;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::useMemoryAllocators(OsclMemAllocator* write_alloc)
{
    OSCL_UNUSED_ARG(write_alloc);
    OSCL_LEAVE(OsclErrNotSupported);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::writeAsync(uint8 aFormatType, int32 aFormatIndex,
        uint8* aData, uint32 aDataLen,
        const PvmiMediaXferHeader& data_header_info,
        OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aFormatType);
    OSCL_UNUSED_ARG(aFormatIndex);
    OSCL_UNUSED_ARG(aData);
    OSCL_UNUSED_ARG(aDataLen);
    OSCL_UNUSED_ARG(data_header_info);
    OSCL_UNUSED_ARG(aContext);
    // This is an active data source. writeAsync is not supported.
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::writeComplete(PVMFStatus aStatus, PVMFCommandId write_cmd_id,
        OsclAny* aContext)
{
    LOGV("writeComplete(%d, %p)", write_cmd_id, aContext);

    if (aContext)
    {
        // Is this a callback to complete an info/error event?
        PVMFAsyncEvent* asyncEvent = OSCL_STATIC_CAST(PVMFAsyncEvent*, aContext);
        if ((asyncEvent->IsA() == PVMFInfoEvent) || (asyncEvent->IsA() == PVMFErrorEvent))
        {
            // This is the callback... cleanup the allocated memory
            PVInterface* extInterface = asyncEvent->GetEventExtensionInterface();
            if (extInterface)
            {
                PVMFBasicErrorInfoMessage* eventMsg = OSCL_STATIC_CAST(PVMFBasicErrorInfoMessage*, extInterface);
                eventMsg->removeRef();
            }
            OSCL_DELETE(asyncEvent);
        }
    }

    if(aStatus != PVMFSuccess)
    {
        return;
    }

    if(iSentMediaData.empty())
    {
        // Error: Nothing to complete
        return;
    }

    Oscl_Vector<AndroidAudioInputMediaData, OsclMemAllocator>::iterator it;
    for(it = iSentMediaData.begin(); it != iSentMediaData.end(); it++)
    {
        if( it->iId == write_cmd_id )
        {
            iMediaBufferMemPool->deallocate(it->iData);
            iSentMediaData.erase(it);
            //check mem callback
            AddDataEventToQueue(0);

            //LOGE("writeComplete SentMediaQ out\n");
            return;
        }
    }

    // Error: unmatching ID. They should be in sequence
    //LOGE("writeComplete ERROR SentMediaQ[0].iId %d write_cmd_id %d\n", iSentMediaData[0].iId, write_cmd_id);
}

void AndroidAudioInput::SendEvent(PVMFEventCategory aCategory,
                                  PVMFStatus aStatus,
                                  OsclAny* aEventData,
                                  int32* aEventCode)
{
    LOGE("AndroidAudioInput::SendEvent: aCategory=%d, aStatus=%d, aEventData=%p, aEventCode=%p",
                     aCategory, aStatus, aEventData, aEventCode);

    PvmiMediaXferHeader mediaXferHeader;
    oscl_memset(&mediaXferHeader, '\0', sizeof(mediaXferHeader));

    if (!iPeer)
    {
        LOGE("AndroidAudioInput::SendEvent: Error - iPeer not availbale");
        return;
    }

    int32 formatIndex = 0;
    switch (aCategory)
    {
        case PVMFErrorEvent:
            formatIndex = PVMI_MEDIAXFER_FMT_INDEX_ERROR_EVENT;
            break;

        case PVMFInfoEvent:
            formatIndex = PVMI_MEDIAXFER_FMT_INDEX_INFO_EVENT;
            break;

        default:
            LOGE("AndroidAudioInput::SendEvent: Error - Unsupported event category");
            return;
            break;
    }

    PVMFBasicErrorInfoMessage* eventMsg = NULL;
    if (aEventCode)
    {
        // Set the MediaInputNode UUID
        PVUuid uuid = PvmfMediaInputNodeUuid;

        eventMsg = OSCL_NEW(PVMFBasicErrorInfoMessage,
                            (*aEventCode, uuid, NULL));
    }

    PVMFAsyncEvent* asyncEvent = OSCL_NEW(PVMFAsyncEvent, (aCategory,
                                           aStatus,
                                           NULL,
                                           OSCL_STATIC_CAST(PVInterface *, eventMsg),
                                           aEventData,
                                           NULL,
                                           0));

    if (!asyncEvent)
    {
        LOGE("AndroidAudioInput::SendEvent: Error - PVMFAsyncEvent allocation failed");
        return;
    }

    int32 err = 0;
    OSCL_TRY(err,
             iPeer->writeAsync(PVMI_MEDIAXFER_FMT_TYPE_NOTIFICATION,
                               formatIndex,
                               (uint8*)asyncEvent,
                               sizeof(PVMFAsyncEvent),
                               mediaXferHeader,
                               /* Pass asyncEvent as context for deallocate in writeComplete*/
                               (OsclAny*)asyncEvent);
            );

    if (err != OsclErrNone)
    {
        LOGE("AndroidAudioInput::SendEvent: Error - writeAsync failed. err=%x", err);
        return;
    }
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::readAsync(uint8* data, uint32 max_data_len,
        OsclAny* aContext, int32* formats, uint16 num_formats)
{
    OSCL_UNUSED_ARG(data);
    OSCL_UNUSED_ARG(max_data_len);
    OSCL_UNUSED_ARG(aContext);
    OSCL_UNUSED_ARG(formats);
    OSCL_UNUSED_ARG(num_formats);
    // This is an active data source. readAsync is not supported.
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::readComplete(PVMFStatus aStatus, PVMFCommandId read_cmd_id,
        int32 format_index, const PvmiMediaXferHeader& data_header_info,
        OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aStatus);
    OSCL_UNUSED_ARG(read_cmd_id);
    OSCL_UNUSED_ARG(format_index);
    OSCL_UNUSED_ARG(data_header_info);
    OSCL_UNUSED_ARG(aContext);
    // This is an active data source. readComplete is not supported.
    OSCL_LEAVE(OsclErrNotSupported);
    return;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::statusUpdate(uint32 status_flags)
{
    LOGV("statusUpdate");
    if (status_flags != PVMI_MEDIAXFER_STATUS_WRITE)
    {
        OSCL_LEAVE(OsclErrNotSupported);
    }
}


////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::cancelCommand(PVMFCommandId aCmdId)
{
    OSCL_UNUSED_ARG(aCmdId);
    // This cancel command ( with a small "c" in cancel ) is for the media transfer interface.
    // implementation is similar to the cancel command of the media I/O interface.
    OSCL_LEAVE(OsclErrNotSupported);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::cancelAllCommands()
{
    OSCL_LEAVE(OsclErrNotSupported);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver)
{
    OSCL_UNUSED_ARG(aObserver);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus AndroidAudioInput::getParametersSync(PvmiMIOSession session,
        PvmiKeyType identifier,
        PvmiKvp*& parameters,
        int& num_parameter_elements,
        PvmiCapabilityContext context)
{
    LOGV("getParametersSync(%s)", identifier);
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);

    parameters = NULL;
    num_parameter_elements = 0;
    PVMFStatus status = PVMFFailure;

    if( pv_mime_strcmp(identifier, OUTPUT_FORMATS_CAP_QUERY) == 0 ||
            pv_mime_strcmp(identifier, OUTPUT_FORMATS_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        status = AllocateKvp(parameters, (PvmiKeyType)OUTPUT_FORMATS_VALTYPE, num_parameter_elements);
        if(status != PVMFSuccess)
        {
            LOGE("AndroidAudioInput::getParametersSync() OUTPUT_FORMATS_VALTYPE AllocateKvp failed");
            return status;
        }

        parameters[0].value.pChar_value = (char*)PVMF_MIME_PCM16;
    }
    else if(pv_mime_strcmp(identifier, OUTPUT_TIMESCALE_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        status = AllocateKvp(parameters, (PvmiKeyType)OUTPUT_TIMESCALE_CUR_VALUE, num_parameter_elements);
        if(status != PVMFSuccess)
        {
            LOGE("AndroidAudioInput::getParametersSync() OUTPUT_TIMESCALE_CUR_VALUE AllocateKvp failed");
            return status;
        }

        // XXX is it okay to hardcode this as the timescale?
        parameters[0].value.uint32_value = 1000;
    }
    else if (pv_mime_strcmp(identifier, AUDIO_OUTPUT_SAMPLING_RATE_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        status = AllocateKvp(parameters, (PvmiKeyType)AUDIO_OUTPUT_SAMPLING_RATE_CUR_QUERY, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOGE("AndroidAudioInput::getParametersSync() AUDIO_OUTPUT_SAMPLING_RATE_CUR_QUERY AllocateKvp failed");
            return status;
        }

        parameters[0].value.uint32_value = iAudioSamplingRate;
    }
    else if (pv_mime_strcmp(identifier, AUDIO_OUTPUT_NUM_CHANNELS_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        status = AllocateKvp(parameters, (PvmiKeyType)AUDIO_OUTPUT_NUM_CHANNELS_CUR_QUERY, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOGE("AndroidAudioInput::getParametersSync() AUDIO_OUTPUT_NUM_CHANNELS_CUR_QUERY AllocateKvp failed");
            return status;
        }

        parameters[0].value.uint32_value = iAudioNumChannels;
    }

    return status;
}

    ////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus AndroidAudioInput::releaseParameters(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(num_elements);

    if(parameters)
    {
        iAlloc.deallocate((OsclAny*)parameters);
        return PVMFSuccess;
    }
    else
    {
        return PVMFFailure;
    }
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::createContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::setContextParameters(PvmiMIOSession session,
        PvmiCapabilityContext& context,
        PvmiKvp* parameters, int num_parameter_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_parameter_elements);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::DeleteContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF void AndroidAudioInput::setParametersSync(PvmiMIOSession session, PvmiKvp* parameters,
        int num_elements, PvmiKvp*& ret_kvp)
{
    LOGV("setParametersSync");
    OSCL_UNUSED_ARG(session);
    PVMFStatus status = PVMFSuccess;
    ret_kvp = NULL;

    for(int32 i = 0; i < num_elements; i++)
    {
        status = VerifyAndSetParameter(&(parameters[i]), true);
        if(status != PVMFSuccess)
        {
            LOGV("VerifyAndSetParameter failed");
            ret_kvp = &(parameters[i]);
            OSCL_LEAVE(OsclErrArgument);
        }
    }
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFCommandId AndroidAudioInput::setParametersAsync(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements,
        PvmiKvp*& ret_kvp,
        OsclAny* context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_elements);
    OSCL_UNUSED_ARG(ret_kvp);
    OSCL_UNUSED_ARG(context);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF uint32 AndroidAudioInput::getCapabilityMetric (PvmiMIOSession session)
{
    OSCL_UNUSED_ARG(session);
    return 0;
}

////////////////////////////////////////////////////////////////////////////
OSCL_EXPORT_REF PVMFStatus AndroidAudioInput::verifyParametersSync(PvmiMIOSession session,
        PvmiKvp* parameters, int num_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_elements);
    return PVMFErrNotSupported;
}

////////////////////////////////////////////////////////////////////////////
bool AndroidAudioInput::setAudioSamplingRate(int32 iSamplingRate)
{
    LOGV("AndroidAudioInput::setAudioSamplingRate( %d )", iSamplingRate);

    if (iSamplingRate == 0)
    {
        // Setting sampling rate to zero will cause a crash
        LOGV("AndroidAudioInput::setAudioSamplingRate() invalid sampling rate.  Return false.");
        return false;
    }

    iAudioSamplingRate = iSamplingRate;
    LOGV("AndroidAudioInput::setAudioSamplingRate() iAudioSamplingRate %d set", iAudioSamplingRate);
    return true;
}
////////////////////////////////////////////////////////////////////////////
bool AndroidAudioInput::setAudioNumChannels(int32 iNumChannels)
{
    LOGV("AndroidAudioInput::setAudioNumChannels( %d )", iNumChannels);

    iAudioNumChannels = iNumChannels;
    LOGV("AndroidAudioInput::setAudioNumChannels() iAudioNumChannels %d set", iAudioNumChannels);
    return true;
}

////////////////////////////////////////////////////////////////////////////
//                            Private methods
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInput::Run()
{
    if(!iCmdQueue.empty())
    {
        AndroidAudioInputCmd cmd = iCmdQueue[0];
        iCmdQueue.erase(iCmdQueue.begin());

        switch(cmd.iType)
        {

        case AI_CMD_INIT:
            DoRequestCompleted(cmd, DoInit());
            break;

        case AI_CMD_START:
            DoRequestCompleted(cmd, DoStart());
            break;

        case AI_CMD_PAUSE:
            DoRequestCompleted(cmd, DoPause());
            break;

        case AI_CMD_FLUSH:
            DoRequestCompleted(cmd, DoFlush());
            break;

        case AI_CMD_RESET:
            DoRequestCompleted(cmd, DoReset());
            break;

        case AI_CMD_STOP:
            DoRequestCompleted(cmd, DoStop());
            break;

        case AI_DATA_WRITE_EVENT:
            DoRead();
            break;

        case AI_CMD_QUERY_UUID:
        case AI_CMD_QUERY_INTERFACE:
            DoRequestCompleted(cmd, PVMFSuccess);
            break;

        case AI_CMD_CANCEL_ALL_COMMANDS:
        case AI_CMD_CANCEL_COMMAND:
            DoRequestCompleted(cmd, PVMFFailure);
            break;

        default:
            break;
        }
    }

    if(!iCmdQueue.empty())
    {
        // Run again if there are more things to process
        RunIfNotReady();
    }

    if(iState == STATE_STARTED)
    {
        SendMicData();
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInput::AddCmdToQueue(AndroidAudioInputCmdType aType,
        const OsclAny* aContext, OsclAny* aData1)
{
    if(aType == AI_DATA_WRITE_EVENT)
        OSCL_LEAVE(OsclErrArgument);

    AndroidAudioInputCmd cmd;
    cmd.iType = aType;
    cmd.iContext = OSCL_STATIC_CAST(OsclAny*, aContext);
    cmd.iData1 = aData1;
    cmd.iId = iCmdIdCounter;
    ++iCmdIdCounter;
    iCmdQueue.push_back(cmd);
    RunIfNotReady();
    return cmd.iId;
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInput::AddDataEventToQueue(uint32 aMicroSecondsToEvent)
{
    AndroidAudioInputCmd cmd;
    cmd.iType = AI_DATA_WRITE_EVENT;
    iCmdQueue.push_back(cmd);
    RunIfNotReady(aMicroSecondsToEvent);
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInput::DoRequestCompleted(const AndroidAudioInputCmd& aCmd, PVMFStatus aStatus, OsclAny* aEventData)
{
    LOGV("DoRequestCompleted(%d, %d) this %p", aCmd.iId, aStatus, this);
    PVMFCmdResp response(aCmd.iId, aCmd.iContext, aStatus, aEventData);

    for(uint32 i = 0; i < iObservers.size(); i++)
        iObservers[i]->RequestCompleted(response);
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInput::DoInit()
{
    LOGV("DoInit");

    //calculate time for a buffer to fill
    iMicroSecondsPerDataEvent = (int32)((1000000/iAudioSamplingRate) / iAudioNumChannels);
    LOGV("AndroidAudioInput::DoInit() iMicroSecondsPerDataEvent %d", iMicroSecondsPerDataEvent);

    iDataEventCounter = 0;

    // Create memory pool for the media data, using the maximum frame size found earlier
    int32 err = 0;
    OSCL_TRY(err,
        if(iMediaBufferMemPool)
        {
            OSCL_DELETE(iMediaBufferMemPool);
            iMediaBufferMemPool = NULL;
        }
        iMediaBufferMemPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (4));
        if(!iMediaBufferMemPool) {
            LOGV("AndroidAudioInput::DoInit() unable to create memory pool.  Return PVMFErrNoMemory.");
            OSCL_LEAVE(OsclErrNoMemory);
        }
    );
    OSCL_FIRST_CATCH_ANY(err, return PVMFErrNoMemory);

    iState = STATE_INITIALIZED;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInput::DoStart()
{
    LOGV("DoStart");

    iAudioThreadStartLock->lock();
    iAudioThreadStarted = false;

    OsclThread AudioInput_Thread;
    OsclProcStatus::eOsclProcError ret = AudioInput_Thread.Create(
            (TOsclThreadFuncPtr)start_audin_thread_func, 0,
            (TOsclThreadFuncArg)this, Start_on_creation);

    if ( OsclProcStatus::SUCCESS_ERROR != ret)
    { // thread creation failed
        LOGV("Failed to create thread");
        iAudioThreadStartLock->unlock();
        return PVMFFailure;
    }

    // wait for thread to set up AudioRecord
    while (!iAudioThreadStarted)
        iAudioThreadStartCV->wait(*iAudioThreadStartLock);

    status_t startResult = iAudioThreadStartResult;
    iAudioThreadStartLock->unlock();

    if (startResult != NO_ERROR)
        return PVMFFailure; // thread failed to set up AudioRecord

    iState = STATE_STARTED;

    AddDataEventToQueue(0);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
int AndroidAudioInput::start_audin_thread_func(TOsclThreadFuncArg arg)
{
    prctl(PR_SET_NAME, (unsigned long) "audio in", 0, 0, 0);
    sp<AndroidAudioInput> obj =  (AndroidAudioInput *)arg;
    return obj->audin_thread_func();
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInput::DoPause()
{
    LOGV("DoPause");
    iExitAudioThread = true;
    iState = STATE_PAUSED;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInput::DoReset()
{
    LOGV("DoReset");
    iExitAudioThread = true;
    iDataEventCounter = 0;
    if(iAudioThreadStarted ){
    iAudioThreadSem->Signal();
    iAudioThreadTermSem->Wait();
    iAudioThreadStarted = false;
    }
    while(!iCmdQueue.empty())
    {
        AndroidAudioInputCmd cmd = iCmdQueue[0];
        iCmdQueue.erase(iCmdQueue.begin());
    }
    Cancel();
    while(!iOSSRequestQueue.empty())
    {
        uint8* data = iOSSRequestQueue[0];
        iMediaBufferMemPool->deallocate(data);
        iOSSRequestQueue.erase(&iOSSRequestQueue[0]);
    }
    while(!iWriteResponseQueue.empty())
    {
        MicData micdata = iWriteResponseQueue[0];
        iMediaBufferMemPool->deallocate(micdata.iData);
        iWriteResponseQueue.erase(&iWriteResponseQueue[0]);
    }
    iState = STATE_IDLE;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInput::DoFlush()
{
    // This method should stop capturing media data but continue to send captured
    // media data that is already in buffer and then go to stopped state.
    // However, in this case of file input we do not have such a buffer for
    // captured data, so this behaves the same way as stop.
    LOGV("DoFlush");
    return DoStop();
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInput::DoStop()
{
    LOGV("DoStop");
    iExitAudioThread = true;
    iDataEventCounter = 0;
    iState = STATE_STOPPED;
    if(iAudioThreadStarted ){
    iAudioThreadSem->Signal();
    iAudioThreadTermSem->Wait();
    iAudioThreadStarted = false;
    }
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInput::DoRead()
{
    LOGV("DoRead");
    // Just copy from PVMFFileInputNode::HandleEventPortActivity.  The only difference
    // is that data buffer is allocated by calling iMediaBufferMemPool->allocate(bytesToRead)
    // and there's no need to wrap it in a PVMFSharedMediaDataPtr.  Also, you'll need to
    // keep track of the data pointer and the write command id received from peer->writeAsync
    // and put it in the iSentMediaData queue

    if(iState != STATE_STARTED)
    {
        LOGV("not started");
        return PVMFSuccess;
    }
    if (NULL == iPeer)
    {
        LOGV("iPeer Null");
        return PVMFSuccess;
    } 

    uint32 timeStamp = 0;
    uint32 writeAsyncID = 0;

    ++iDataEventCounter;

    iOSSRequestQueueLock.Lock();
    //allocate mem for the audio capture thread
    // Create new media data buffer
    int32 error = 0;
    uint8* data = NULL;
    OSCL_TRY(error,
            data = (uint8*)iMediaBufferMemPool->allocate(kBufferSize);
            );
    if (error != OsclErrNone || !data )
    {
        LOGV("no data available");
        iOSSRequestQueueLock.Unlock();

        //TBD wait for mem callback
        return PVMFSuccess;
    }
    else
    {
        iOSSRequestQueue.push_back(data);
        iOSSRequestQueueLock.Unlock();
        iAudioThreadSem->Signal();
    }

    // Queue the next data event
    AddDataEventToQueue(iMicroSecondsPerDataEvent);

    return PVMFSuccess;
}

void AndroidAudioInput::RampVolume(
    int32 timeInFrames,
    int32 kAutoRampDurationFrames,
    void *_data,
    size_t numBytes) const {
    int16 *data = (int16 *)_data;

    // Apply the ramp to the entire buffer or to the end of the ramp duration
    // whichever comes first.
    int32 kStopFrame = timeInFrames + numBytes / sizeof(int16);
    if (kStopFrame > kAutoRampDurationFrames) {
        kStopFrame = kAutoRampDurationFrames;
    }

    const int32 kShift = 14;
    int32 fixedMultiplier =
        (timeInFrames << kShift) / kAutoRampDurationFrames;

    if (iAudioNumChannels == 1) {
        while (timeInFrames < kStopFrame) {
            data[0] = (data[0] * fixedMultiplier) >> kShift;
            ++data;
            ++timeInFrames;

            if ((timeInFrames & 3) == 0) {
                // Update the multiplier every 4 frames.

                fixedMultiplier =
                    (timeInFrames << kShift) / kAutoRampDurationFrames;
            }
        }
    } else {
        LOG_ALWAYS_FATAL_IF(
                iAudioNumChannels != 2,
                "We only support mono or stereo audio data here.");

        // Stereo
        while (timeInFrames < kStopFrame) {
            data[0] = (data[0] * fixedMultiplier) >> kShift;
            data[1] = (data[1] * fixedMultiplier) >> kShift;
            data += 2;
            ++timeInFrames;

            if ((timeInFrames & 3) == 0) {
                // Update the multiplier every 4 frames.

                fixedMultiplier =
                    (timeInFrames << kShift) / kAutoRampDurationFrames;
            }
        }
    }
}
////////////////////////////////////////////////////////////////////////////
int AndroidAudioInput::audin_thread_func() {
    // setup audio record session

    iAudioThreadStartLock->lock();

    LOGV("create AudioRecord %p", this);
    AudioRecord
            * record = new AudioRecord(
                    iAudioSource, iAudioSamplingRate,
                    android::AudioSystem::PCM_16_BIT,
                    iAudioNumChannels,
                    4*kBufferSize/iAudioNumChannels/sizeof(int16));
    LOGV("AudioRecord created %p, this %p", record, this);

    status_t res = record->initCheck();
    if (res == NO_ERROR)
        res = record->start();

    iAudioThreadStartResult = res;
    iAudioThreadStarted = true;

    iAudioThreadStartCV->signal();
    iAudioThreadStartLock->unlock();

    if (res == NO_ERROR) {

        // We are going to ramp up the volume from 0 to full at the
        // start of recording.
        int64_t numFramesRecorded = 0;

        const int32 kAutoRampStartFrames =
            AUTO_RAMP_START_MS * iAudioSamplingRate / 1000;

        const int32 kAutoRampDurationFrames =
            AUTO_RAMP_DURATION_MS * iAudioSamplingRate / 1000;

        while (!iExitAudioThread) {
            iOSSRequestQueueLock.Lock();
            if (iOSSRequestQueue.empty()) {
                iOSSRequestQueueLock.Unlock();
                iAudioThreadSem->Wait();
                continue;
            }
            uint8* data = iOSSRequestQueue[0];
            iOSSRequestQueue.erase(&iOSSRequestQueue[0]);
            iOSSRequestQueueLock.Unlock();

            int numOfBytes = record->read(data, kBufferSize);
            //LOGV("read %d bytes", numOfBytes);
            if (numOfBytes <= 0)
                break;

            if (numFramesRecorded < kAutoRampStartFrames) {
                // Start with silence...
                memset(data, 0, numOfBytes);
            } else {
                // Then ramp up the volume...
                int64_t delta = numFramesRecorded - kAutoRampStartFrames;
                if (delta < kAutoRampDurationFrames) {
                    RampVolume(
                            delta, kAutoRampDurationFrames, data, numOfBytes);
                }
            }

            if (iTrackMaxAmplitude) {
                int16 *p = (int16*) data;
                for (int i = numOfBytes >> 1; i > 0; --i) {
                    int16 v = *p++;
                    if (v < 0) {
                        v = -v;
                    }
                    if (v > iMaxAmplitude) {
                        iMaxAmplitude = v;
                    }
                }
            }

            int32 dataFrames = numOfBytes / sizeof(int16) / iAudioNumChannels;
            numFramesRecorded += dataFrames;
            int dataDuration = dataFrames * 1000 / iAudioSamplingRate; //ms

            MicData micdata(data, numOfBytes, iTimeStamp,
                    dataDuration);
            iWriteResponseQueueLock.Lock();
            iWriteResponseQueue.push_back(micdata);
            iWriteResponseQueueLock.Unlock();

            iTimeStamp += dataDuration;
            // Queue the next data event
            OsclAny* P = NULL;
            iWriteCompleteAO->ReceiveEvent(P);
        }

        LOGV("record->stop %p, this %p", record, this);
        record->stop();
    }

    LOGV("delete record %p, this %p", record, this);
    delete record;
    iAudioThreadTermSem->Signal();
    return 0;
}

void AndroidAudioInput::SendMicData(void)
{
    //LOGE("SendMicData in\n");
    //ASSUMPTION: the output queue is always available. no wait
    if(iState != STATE_STARTED)
    {
        LOGV("not started");
        return;
    }
    if (NULL == iPeer)
    {    
        LOGV("iPeer Null");
        return;
    }
    iWriteResponseQueueLock.Lock();
    //LOGE("SendMicData QSize %d\n", iWriteResponseQueue.size());
    if(iWriteResponseQueue.empty())
    {
        iWriteResponseQueueLock.Unlock();
        return;
    }
    MicData &data = iWriteResponseQueue[0];
    // send data to Peer & store the id
    PvmiMediaXferHeader data_hdr;
    data_hdr.seq_num=iDataEventCounter-1;
    data_hdr.timestamp = data.iTimestamp;
    data_hdr.flags=0;
    data_hdr.duration = data.iDuration;
    data_hdr.stream_id=0;
    int32 err = 0;
    PVMFCommandId writeAsyncID = 0;
    OSCL_TRY(err,
             writeAsyncID = iPeer->writeAsync(PVMI_MEDIAXFER_FMT_TYPE_DATA, 0, data.iData, data.iDataLen, data_hdr);
            );
    OSCL_FIRST_CATCH_ANY(err,
             // send data failed, data sent out in wrong state.
             LOGE("send data failed");
             iWriteResponseQueueLock.Unlock();
             return;
             );

    // Save the id and data pointer on iSentMediaData queue for writeComplete call
    AndroidAudioInputMediaData sentData;
    sentData.iId = writeAsyncID;
    sentData.iData = data.iData;

    iWriteResponseQueue.erase(&iWriteResponseQueue[0]);
    iWriteResponseQueueLock.Unlock();

    iSentMediaData.push_back(sentData);
    //LOGE("SendMicData out SentQ size %d\n", iSentMediaData.size());
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInput::AllocateKvp(PvmiKvp*& aKvp, PvmiKeyType aKey, int32 aNumParams)
{
    uint8* buf = NULL;
    uint32 keyLen = oscl_strlen(aKey) + 1;
    int32 err = 0;

    OSCL_TRY(err,
            buf = (uint8*)iAlloc.allocate(aNumParams * (sizeof(PvmiKvp) + keyLen));
            if(!buf)
            OSCL_LEAVE(OsclErrNoMemory);
            );
    OSCL_FIRST_CATCH_ANY(err, LOGV("allocation error"); return PVMFErrNoMemory;);

    int32 i = 0;
    PvmiKvp* curKvp = aKvp = new (buf) PvmiKvp;
    buf += sizeof(PvmiKvp);
    for(i = 1; i < aNumParams; i++)
    {
        curKvp += i;
        curKvp = new (buf) PvmiKvp;
        buf += sizeof(PvmiKvp);
    }

    for(i = 0; i < aNumParams; i++)
    {
        aKvp[i].key = (char*)buf;
        oscl_strncpy(aKvp[i].key, aKey, keyLen);
        buf += keyLen;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInput::VerifyAndSetParameter(PvmiKvp* aKvp, bool aSetParam)
{
    if(!aKvp)
    {
        return PVMFFailure;
    }

    if(pv_mime_strcmp(aKvp->key, OUTPUT_FORMATS_VALTYPE) == 0)
    {
        if(pv_mime_strcmp(aKvp->value.pChar_value, PVMF_MIME_PCM16) == 0)
        {
            return PVMFSuccess;
        }
        else
        {
            LOGV("unsupported format");
            return PVMFFailure;
        }
    }

    LOGV("unsupported parameter");
    return PVMFFailure;
}

int AndroidAudioInput::maxAmplitude()
{
    // first call to this function activates tracking
    if (!iTrackMaxAmplitude) {
        iTrackMaxAmplitude = true;
    }

    int result = iMaxAmplitude;
    iMaxAmplitude = 0;
    return result;
}

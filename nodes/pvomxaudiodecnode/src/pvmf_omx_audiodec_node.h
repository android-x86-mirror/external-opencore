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
#ifndef PVMF_OMX_AUDIODEC_NODE_H_INCLUDED
#define PVMF_OMX_AUDIODEC_NODE_H_INCLUDED

#ifndef PVMF_OMX_BASEDEC_NODE_H_INCLUDED
#include "pvmf_omx_basedec_node.h"
#endif

#ifndef PVMF_OMX_AUDIODEC_PORT_H_INCLUDED
#include "pvmf_omx_basedec_port.h"
#endif

#ifndef _LATMPAYLOADPARSER_H_
#include "latmpayloadparser.h"
#endif

#define PVMFOMXAUDIODECNODE_NUM_CMD_IN_POOL 8
#define PVOMXAUDIODEC_DEFAULT_SAMPLINGRATE 48000
#define PVOMXAUDIODEC_DEFAULT_OUTPUTPCM_TIME 200
#define PVOMXAUDIODEC_AMRNB_SAMPLES_PER_FRAME 160
#define PVOMXAUDIODEC_AMRWB_SAMPLES_PER_FRAME 320
#define PVOMXAUDIODEC_MP3_DEFAULT_SAMPLES_PER_FRAME 1152
#define PVOMXAUDIO_MAX_SUPPORTED_FORMAT 31

// Key string info at the base level ("x-pvmf/audio/decoder")
#define PVOMXAUDIODECNODECONFIG_BASE_NUMKEYS 6
const PVOMXBaseDecNodeKeyStringData PVOMXAudioDecNodeConfigBaseKeys[PVOMXAUDIODECNODECONFIG_BASE_NUMKEYS] =
{
    {"silenceinsertion_enable", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_BOOL},
    {"aac_he_v1_enable", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_BOOL},
    {"aac_he_v2_enable", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_BOOL},
    {"format-type", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_CHARPTR}
};

// Key string info at render ("x-pvmf/audio/render")
#define PVOMXAUDIODECNODECONFIG_RENDER_NUMKEYS 2
const PVOMXBaseDecNodeKeyStringData PVOMXAudioDecNodeConfigRenderKeys[PVOMXAUDIODECNODECONFIG_RENDER_NUMKEYS] =
{
    {"sampling_rate", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_UINT32},
    {"channels", PVMI_KVPTYPE_VALUE, PVMI_KVPVALTYPE_UINT32}
};

//Mimetypes for the custom interface
#define PVMF_OMX_AUDIO_DEC_NODE_MIMETYPE "pvxxx/OMXAudioDecNode"
#define PVMF_BASEMIMETYPE "pvxxx"

class PVMFOMXAudioDecNode: public PVMFOMXBaseDecNode
{
    public:
        PVMFOMXAudioDecNode(int32 aPriority);
        ~PVMFOMXAudioDecNode();

        //From PVMFMetadataExtensionInterface
        uint32 GetNumMetadataKeys(char* query_key = NULL);
        uint32 GetNumMetadataValues(PVMFMetadataList& aKeyList);

        // for WMA params
        bool VerifyParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements);

        PVMFStatus DoCapConfigGetParametersSync(PvmiKeyType aIdentifier, PvmiKvp*& aParameters, int& aNumParamElements, PvmiCapabilityContext aContext);
        PVMFStatus DoCapConfigReleaseParameters(PvmiKvp* aParameters, int aNumElements);

    private:

        PVMFStatus DoQueryUuid();
        PVMFStatus DoRequestPort(PVMFPortInterface*&);
        PVMFStatus DoGetNodeMetadataKey();
        PVMFStatus DoGetNodeMetadataValue();
        bool ProcessIncomingMsg(PVMFPortInterface* aPort);
        PVMFStatus HandlePortReEnable();

        bool InitDecoder(PVMFSharedMediaDataPtr&);

        bool NegotiateComponentParameters(OMX_PTR aOutputParameters);
        bool GetSetCodecSpecificInfo();
        bool QueueOutputBuffer(OsclSharedPtr<PVMFMediaDataImpl> &mediadataimplout, uint32 aDataLen);

        // Capability And Config
        PVMFStatus DoCapConfigVerifyParameters(PvmiKvp* aParameters, int aNumElements);
        void DoCapConfigSetParameters(PvmiKvp* aParameters, int aNumElements, PvmiKvp* &aRetKVP);

        // latm parser for AAC - LATM
        PVMFStatus CreateLATMParser(void);
        PVMFStatus DeleteLATMParser(void);

        bool ReleaseAllPorts();

        bool CreateAACConfigDataFromASF(uint8 *inptr, uint32 inlen, uint8 *outptr, uint32 &outlen);

        OMX_AUDIO_CODINGTYPE iOMXAudioCompressionFormat;

        // Audio parameters
        // the output buffer size is calculated from the parameters below
        uint32 iPCMSamplingRate;        // typically 8,16,22.05,32,44.1, 48 khz
        uint32 iNumberOfAudioChannels;  // can be 1 or 2
        uint32 iSamplesPerFrame;        // number of samples per 1 frame of data (if known) per channel
        uint32 iNumBytesPerFrame;       // depends on number of samples/channel and number of channels
        uint32 iMilliSecPerFrame;       //


        // LATM parser for AAC
        PV_LATM_Parser *iLATMParser;
        uint8 *iLATMConfigBuffer;
        uint32 iLATMConfigBufferSize;
};
#endif // PVMF_OMXAUDIODEC_NODE_H_INCLUDED


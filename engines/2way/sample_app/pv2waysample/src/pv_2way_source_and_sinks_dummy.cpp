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
#include "pv_2way_source_and_sinks_dummy.h"
#include "pv_2way_sink.h"
#include "pv_2way_source.h"
#include "pv_media_output_node_factory.h"
#include "pvmf_media_input_node_factory.h"

OSCL_EXPORT_REF PV2WayDummySourceAndSinks::PV2WayDummySourceAndSinks(PV2Way324InitInfo& aSdkInitInfo) :
        PV2WaySourceAndSinksBase(aSdkInitInfo)
{
}

OSCL_EXPORT_REF PV2WayDummySourceAndSinks::~PV2WayDummySourceAndSinks()
{
    Cleanup();
}

OSCL_EXPORT_REF int PV2WayDummySourceAndSinks::AddPreferredCodec(TPVDirection aDir,
        PV2WayMediaType aMediaType,
        LipSyncDummyMIOSettings& aSettings)
{
    PV2WayMIO* mio = GetMIO(aDir, aMediaType);
    if (mio)

    {
        mio->AddCodec(aSettings);
        return 0;
    }
    OutputInfo(PVLOGMSG_ERR, "PV2WaySourceAndSinksBase::AddPreferredCodec: Error!  No MIO of given dir, type");
    return -1;
}


OSCL_EXPORT_REF PVMFNodeInterface* PV2WayDummySourceAndSinks::CreateMIONode(CodecSpecifier* aformat,
        TPVDirection adir)
{
    PVMFNodeInterface* mioNode = NULL;
    PVMFFormatType format = aformat->GetFormat();
    if (aformat->GetType() == EPVDummyMIO)
    {
        DummyMIOCodecSpecifier* temp = OSCL_REINTERPRET_CAST(DummyMIOCodecSpecifier*, aformat);
        LipSyncDummyMIOSettings fileSettings = temp->GetSpecifierType();
        if (adir == INCOMING)
        {
            if (format.isAudio())
            {
                mioNode = iDummyMioAudioOutputFactory.Create(fileSettings);
            }
            else if (format.isVideo())
            {
                mioNode = iDummyMioVideoOutputFactory.Create(fileSettings);
            }
        }
        else if (adir == OUTGOING)
        {
            if (format.isAudio())
            {
                mioNode = iDummyMioAudioInputFactory.Create(fileSettings);
            }
            else if (format.isVideo())
            {
                mioNode = iDummyMioVideoInputFactory.Create(fileSettings);
            }
        }
    }
    return mioNode;
}

OSCL_EXPORT_REF void PV2WayDummySourceAndSinks::DeleteMIONode(CodecSpecifier* aformat,
        TPVDirection adir,
        PVMFNodeInterface** aMioNode)
{
    if (!aformat)
    {
        if (aMioNode)
        {
            OSCL_DELETE(*aMioNode);
            *aMioNode = NULL;
        }
        return;
    }
    PVMFFormatType format = aformat->GetFormat();
    if (aformat->GetType() == EPVDummyMIO)
    {
        if (adir == INCOMING)
        {
            if (format.isAudio())
            {
                iDummyMioAudioOutputFactory.Delete(aMioNode);
            }
            else if (format.isVideo())
            {
                iDummyMioVideoOutputFactory.Delete(aMioNode);
            }
        }
        else if (adir == OUTGOING)
        {
            if (format.isAudio())
            {
                iDummyMioAudioInputFactory.Delete(aMioNode);
            }
            else if (format.isVideo())
            {
                iDummyMioVideoInputFactory.Delete(aMioNode);
            }
        }
    }
    *aMioNode = NULL;
}




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
#include "pvmf_return_codes.h"

#ifdef CONSIDER
#error "CONSIDER already defined!"
#endif
#define CONSIDER(val) case val: return #val

OSCL_EXPORT_REF const char *PVMFStatusToString(const PVMFStatus status)
{
    switch (status)
    {
            CONSIDER(PVMFSuccess);
            CONSIDER(PVMFPending);
            CONSIDER(PVMFNotSet);
            CONSIDER(PVMFFailure);
            CONSIDER(PVMFErrCancelled);
            CONSIDER(PVMFErrNoMemory);
            CONSIDER(PVMFErrNotSupported);
            CONSIDER(PVMFErrArgument);
            CONSIDER(PVMFErrBadHandle);
            CONSIDER(PVMFErrAlreadyExists);
            CONSIDER(PVMFErrBusy);
            CONSIDER(PVMFErrNotReady);
            CONSIDER(PVMFErrCorrupt);
            CONSIDER(PVMFErrTimeout);
            CONSIDER(PVMFErrOverflow);
            CONSIDER(PVMFErrUnderflow);
            CONSIDER(PVMFErrInvalidState);
            CONSIDER(PVMFErrNoResources);
            CONSIDER(PVMFErrResourceConfiguration);
            CONSIDER(PVMFErrResource);
            CONSIDER(PVMFErrProcessing);
            CONSIDER(PVMFErrPortProcessing);
            CONSIDER(PVMFErrAccessDenied);
            CONSIDER(PVMFErrContentTooLarge);
            CONSIDER(PVMFErrMaxReached);
            CONSIDER(PVMFLowDiskSpace);
            CONSIDER(PVMFErrHTTPAuthenticationRequired);
            CONSIDER(PVMFErrCallbackHasBecomeInvalid);
            CONSIDER(PVMFErrCallbackClockStopped);
            CONSIDER(PVMFErrReleaseMetadataValueNotDone);
            CONSIDER(PVMFErrRedirect);
            CONSIDER(PVMFErrNotImplemented);
            CONSIDER(PVMFErrDrmLicenseNotFound);
            CONSIDER(PVMFErrDrmLicenseExpired);
            CONSIDER(PVMFErrDrmLicenseNotYetValid);
            CONSIDER(PVMFErrDrmInsufficientRights);
            CONSIDER(PVMFErrDrmOutputProtectionLevel);
            CONSIDER(PVMFErrDrmClockRollback);
            CONSIDER(PVMFErrDrmClockError);
            CONSIDER(PVMFErrDrmLicenseStoreCorrupt);
            CONSIDER(PVMFErrDrmLicenseStoreInvalid);
            CONSIDER(PVMFErrDrmLicenseStoreAccess);
            CONSIDER(PVMFErrDrmDeviceDataAccess);
            CONSIDER(PVMFErrDrmNetworkError);
            CONSIDER(PVMFErrDrmDeviceIDUnavailable);
            CONSIDER(PVMFErrDrmDeviceDataMismatch);
            CONSIDER(PVMFErrDrmCryptoError);
            CONSIDER(PVMFErrDrmLicenseNotFoundPreviewAvailable);
            CONSIDER(PVMFErrDrmServerError);
            CONSIDER(PVMFErrDrmDomainRequired);
            CONSIDER(PVMFErrDrmDomainRenewRequired);
            CONSIDER(PVMFErrDrmDomainNotAMember);
            CONSIDER(PVMFErrDrmOperationFailed);
            CONSIDER(PVMFErrContentInvalidForProgressivePlayback);
            CONSIDER(PVMFInfoPortCreated);
            CONSIDER(PVMFInfoPortDeleted);
            CONSIDER(PVMFInfoPortConnected);
            CONSIDER(PVMFInfoPortDisconnected);
            CONSIDER(PVMFInfoOverflow);
            CONSIDER(PVMFInfoUnderflow);
            CONSIDER(PVMFInfoProcessingFailure);
            CONSIDER(PVMFInfoEndOfData);
            CONSIDER(PVMFInfoBufferCreated);
            CONSIDER(PVMFInfoBufferingStart);
            CONSIDER(PVMFInfoBufferingStatus);
            CONSIDER(PVMFInfoBufferingComplete);
            CONSIDER(PVMFInfoDataReady);
            CONSIDER(PVMFInfoPositionStatus);
            CONSIDER(PVMFInfoStateChanged);
            CONSIDER(PVMFInfoDataDiscarded);
            CONSIDER(PVMFInfoErrorHandlingStart);
            CONSIDER(PVMFInfoErrorHandlingComplete);
            CONSIDER(PVMFInfoRemoteSourceNotification);
            CONSIDER(PVMFInfoLicenseAcquisitionStarted);
            CONSIDER(PVMFInfoContentLength);
            CONSIDER(PVMFInfoContentTruncated);
            CONSIDER(PVMFInfoSourceFormatNotSupported);
            CONSIDER(PVMFInfoPlayListClipTransition);
            CONSIDER(PVMFInfoContentType);
            CONSIDER(PVMFInfoTrackDisable);
            CONSIDER(PVMFInfoUnexpectedData);
            CONSIDER(PVMFInfoSessionDisconnect);
            CONSIDER(PVMFInfoStartOfData);
            CONSIDER(PVMFInfoReportObserverRecieved);
            CONSIDER(PVMFInfoMetadataAvailable);
            CONSIDER(PVMFInfoDurationAvailable);
            CONSIDER(PVMFInfoChangePlaybackPositionNotSupported);
            CONSIDER(PVMFInfoPoorlyInterleavedContent);
            CONSIDER(PVMFInfoActualPlaybackPosition);
            CONSIDER(PVMFInfoLiveBufferEmpty);
            CONSIDER(PVMFInfoPlayListSwitch);
            CONSIDER(PVMFMIOConfigurationComplete);
            CONSIDER(PVMFInfoVideoTrackFallingBehind);
            CONSIDER(PVMFInfoSourceOverflow);
            CONSIDER(PVMFInfoShoutcastMediaDataLength);
            CONSIDER(PVMFInfoShoutcastClipBitrate);
            CONSIDER(PVMFInfoIsShoutcastSesssion);
        default:
            return "UNKNOWN PVMFStatus";
    }
}

#undef CONSIDER

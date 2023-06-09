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
#ifndef LIPSYNC_MIO_OBSERVER_H_INCLUDED
#define LIPSYNC_MIO_OBSERVER_H_INCLUDED
#endif
// An observer to the MIO which would be implemented by the test case in order
// to receive updates for some specific tests...
class LipSyncDummyMIOObserver
{
    public:
        /*
         * Signals an update in the status of the MIO.
         */
        virtual void MIOFramesTSUpdate(bool aIsAudio, uint32 aTS) = 0;
};

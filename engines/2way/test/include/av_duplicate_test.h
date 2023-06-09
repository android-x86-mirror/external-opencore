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
#ifndef AV_DUPLICATE_TEST_H_INCLUDED
#define AV_DUPLICATE_TEST_H_INCLUDED

#include "test_base.h"


class av_duplicate_test : public test_base
{
    public:
        av_duplicate_test(bool aUseProxy,
                          int aMaxRuns,
                          uint32 aTimeConnection = TEST_DURATION,
                          uint32 aMaxTestDuration = MAX_TEST_DURATION) :
                test_base(aUseProxy, aTimeConnection, aMaxTestDuration, aMaxRuns)
        {
            iTestName = _STRLIT_CHAR("a/v duplicate");
        };

        ~av_duplicate_test()
        {
        }


        void DoCancel();

        void HandleInformationalEventL(const CPVCmnAsyncInfoEvent& aEvent);

        void CommandCompletedL(const CPVCmnCmdResp& aResponse);

        virtual void TimeoutOccurred(int32 timerID, int32 timeoutInfo);

    private:
        void start_duplicates_if_ready();
        bool start_async_test();
};


#endif



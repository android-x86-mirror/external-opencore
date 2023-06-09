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
#ifndef CONNECT_TEST_H_INCLUDED
#define CONNECT_TEST_H_INCLUDED

#include "test_base.h"


class connect_test : public test_base
{
    public:
        connect_test(bool aUseProxy, int aMaxRuns,
                     uint32 aTimeConnection = TEST_DURATION,
                     uint32 aMaxTestDuration = MAX_TEST_DURATION,
                     bool runTimerTest = false) :
                test_base(aUseProxy, aTimeConnection, aMaxTestDuration, aMaxRuns),
                iRunTimerTest(runTimerTest),
                iMP4H263EncoderInterface(NULL)
        {
            if (iRunTimerTest)
            {
                iTestName = _STRLIT_CHAR("timer configuration and encoder extension IF");
            }
            else
            {
                iTestName = _STRLIT_CHAR("connect");
            }
        };

        ~connect_test()
        {
        }


        void DoCancel();

    private:
        virtual void ConnectSucceeded();
        virtual void InitSucceeded();
        virtual void EncoderIFSucceeded();
        virtual void EncoderIFFailed();
        virtual void ConnectFailed();
        virtual void ConnectCancelled();
        virtual void DisCmdSucceeded();
        virtual void DisCmdFailed();

        bool iRunTimerTest;
        PVInterface *iMP4H263EncoderInterface;
};


#endif



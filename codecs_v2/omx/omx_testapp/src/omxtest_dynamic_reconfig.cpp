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
#include "omxdectest_basic_test.h"
#include "oscl_mem.h"

#define TEMP_OUTPUT_SIZE 20000

bool OmxDecTestPortReconfig::WriteOutput(OMX_U8* aOutBuff, OMX_U32 aSize)
{
    OMX_ERRORTYPE Err;


    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                    (0, "OmxDecTestPortReconfig - Verifying whether port settings have been changed and the corresponding callback has come"));

    /* A check to ensure that output is written to the buffer only in two conditions:
     * 1) port settings have been changed & callback has been received
     * 2) Output buffer & port settings are of correct size, so that no callback is required.
     */
    if (OMX_TRUE == iIsVideoFormat)
    {
        if ((OMX_FALSE == iPortSettingsFlag) && (OMX_FALSE == iTestCompleteFlag))
        {
            INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);
            iParamPort.nPortIndex = iOutputPortIndex;

            Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPortReconfig::Run() - GetParameter called for ParamPortDefinition on port %d", iOutputPortIndex));

            VideoOMXConfigParserOutputs *pOutputParameters;
            pOutputParameters = (VideoOMXConfigParserOutputs*) iOutputParameters;

            if ((OMX_ErrorNone != Err) || (pOutputParameters->width != iParamPort.format.video.nFrameWidth)
                    || (pOutputParameters->height != iParamPort.format.video.nFrameHeight))
            {
                iTestStatus = OMX_FALSE;
                iState = StateError;
                RunIfNotReady();

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestPortReconfig::Run() - Error, Changed Port settings detected, no callback found!"));

                return OMX_FALSE;
            }

            iTestCompleteFlag = OMX_TRUE;
        }
    }

    OMX_U32 BytesWritten = fwrite(aOutBuff, sizeof(OMX_U8), aSize, ipOutputFile);
    return (BytesWritten == aSize);
}


/*
 * Active Object class's Run () function
 * Control all the states of AO & sends API's to the component
 */

void OmxDecTestPortReconfig::Run()
{
    switch (iState)
    {
        case StateUnLoaded:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateUnLoaded IN"));
            OMX_ERRORTYPE Err;
            OMX_BOOL Status;

            if (!iCallbacks->initCallbacks())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestPortReconfig::Run() - ERROR initCallbacks failed, OUT"));
                StopOnError();
                break;
            }

            iDynamicPortReconfigTest = OMX_TRUE;

            ipAppPriv = (AppPrivateType*) oscl_malloc(sizeof(AppPrivateType));
            CHECK_MEM(ipAppPriv, "Component_Handle");
            ipAppPriv->Handle = NULL;

            if (0 == oscl_strcmp(iFormat, "H264") || 0 == oscl_strcmp(iFormat, "M4V")
                    || 0 == oscl_strcmp(iFormat, "WMV") || 0 == oscl_strcmp(iFormat, "H263") || 0 == oscl_strcmp(iFormat, "RV"))
            {
                iIsVideoFormat = OMX_TRUE;
            }

            //Allocate bitstream buffer for AVC component
            if (0 == oscl_strcmp(iFormat, "H264"))
            {
                ipAVCBSO = OSCL_NEW(AVCBitstreamObject, (ipInputFile));
                CHECK_MEM(ipAVCBSO, "Bitstream_Buffer");
            }

            //Allocate bitstream buffer for MPEG4/H263 component
            if (0 == oscl_strcmp(iFormat, "M4V") || 0 == oscl_strcmp(iFormat, "H263"))
            {
                ipBitstreamBuffer = (OMX_U8*) oscl_malloc(BIT_BUFF_SIZE);
                CHECK_MEM(ipBitstreamBuffer, "Bitstream_Buffer")
                ipBitstreamBufferPointer = ipBitstreamBuffer;
            }

            //Allocate bitstream buffer for MP3 component
            if (0 == oscl_strcmp(iFormat, "MP3"))
            {
                ipMp3Bitstream = OSCL_NEW(Mp3BitstreamObject, (ipInputFile));
                CHECK_MEM(ipMp3Bitstream, "Bitstream_Buffer");
            }

            //This should be the first call to the component to load it.
            Err = OMX_MasterInit();
            CHECK_ERROR(Err, "OMX_MasterInit");
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestPortReconfig::Run() - OMX_MasterInit done"));

            Status = PrepareComponent();

            if (OMX_FALSE == Status)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestPortReconfig::Run() Error while loading component OUT"));
                iState = StateError;

                if (iInputParameters.inPtr)
                {
                    oscl_free(iInputParameters.inPtr);
                    iInputParameters.inPtr = NULL;
                }

                RunIfNotReady();
                break;
            }

#if PROXY_INTERFACE
            ipThreadSafeHandlerEventHandler = OSCL_NEW(OmxEventHandlerThreadSafeCallbackAO, (this, EVENT_HANDLER_QUEUE_DEPTH, "EventHandlerAO"));
            ipThreadSafeHandlerEmptyBufferDone = OSCL_NEW(OmxEmptyBufferDoneThreadSafeCallbackAO, (this, iInBufferCount, "EmptyBufferDoneAO"));
            ipThreadSafeHandlerFillBufferDone = OSCL_NEW(OmxFillBufferDoneThreadSafeCallbackAO, (this, iOutBufferCount, "FillBufferDoneAO"));

            if ((NULL == ipThreadSafeHandlerEventHandler) ||
                    (NULL == ipThreadSafeHandlerEmptyBufferDone) ||
                    (NULL == ipThreadSafeHandlerFillBufferDone))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxComponentDecTest::Run() - Error ThreadSafe Callback Handler initialization failed, OUT"));

                iState = StateUnLoaded;
                OsclExecScheduler* sched = OsclExecScheduler::Current();
                sched->StopScheduler();
            }
#endif

            if (StateError != iState)
            {
                iState = StateLoaded;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateUnLoaded OUT, moving to next state"));
            }

            RunIfNotReady();
        }
        break;

        case StateLoaded:
        {
            OMX_ERRORTYPE Err;
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateLoaded IN"));
            // allocate memory for ipInBuffer
            ipInBuffer = (OMX_BUFFERHEADERTYPE**) oscl_malloc(sizeof(OMX_BUFFERHEADERTYPE*) * iInBufferCount);
            CHECK_MEM(ipInBuffer, "InputBufferHeader");

            ipInputAvail = (OMX_BOOL*) oscl_malloc(sizeof(OMX_BOOL) * iInBufferCount);
            CHECK_MEM(ipInputAvail, "InputBufferFlag");

            /* Initialize all the buffers to NULL */
            for (ii = 0; ii < iInBufferCount; ii++)
            {
                ipInBuffer[ii] = NULL;
            }

            //allocate memory for output buffer
            ipOutBuffer = (OMX_BUFFERHEADERTYPE**) oscl_malloc(sizeof(OMX_BUFFERHEADERTYPE*) * iOutBufferCount);
            CHECK_MEM(ipOutBuffer, "OutputBuffer");

            ipOutReleased = (OMX_BOOL*) oscl_malloc(sizeof(OMX_BOOL) * iOutBufferCount);
            CHECK_MEM(ipOutReleased, "OutputBufferFlag");

            /* Initialize all the buffers to NULL */
            for (ii = 0; ii < iOutBufferCount; ii++)
            {
                ipOutBuffer[ii] = NULL;
            }

            Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
            CHECK_ERROR(Err, "SendCommand Loaded->Idle");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPortReconfig::Run() - Sent State Transition Command from Loaded->Idle"));
            iPendingCommands = 1;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPortReconfig::Run() - Allocating %d input and %d output buffers", iInBufferCount, iOutBufferCount));
            //These calls are required because the control of in & out buffer should be with the testapp.
            for (ii = 0; ii < iInBufferCount; ii++)
            {
                Err = OMX_AllocateBuffer(ipAppPriv->Handle, &ipInBuffer[ii], iInputPortIndex, NULL, iInBufferSize);
                CHECK_ERROR(Err, "AllocateBuffer_Input");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPortReconfig::Run() - Called AllocateBuffer for buffer index %d on port %d", ii, iInputPortIndex));
                ipInputAvail[ii] = OMX_TRUE;
                ipInBuffer[ii]->nInputPortIndex = iInputPortIndex;
            }

            if (StateError == iState)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestPortReconfig::Run() - AllocateBuffer Error, StateLoaded OUT"));
                RunIfNotReady();
                break;
            }

            for (ii = 0; ii < iOutBufferCount; ii++)
            {
                Err = OMX_AllocateBuffer(ipAppPriv->Handle, &ipOutBuffer[ii], iOutputPortIndex, NULL, iOutBufferSize);
                CHECK_ERROR(Err, "AllocateBuffer_Output");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPortReconfig::Run() - Called AllocateBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                ipOutReleased[ii] = OMX_TRUE;

                ipOutBuffer[ii]->nOutputPortIndex = iOutputPortIndex;
                ipOutBuffer[ii]->nInputPortIndex = 0;
            }
            if (StateError == iState)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestPortReconfig::Run() - AllocateBuffer Error, StateLoaded OUT"));
                RunIfNotReady();
                break;
            }
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateLoaded OUT, Moving to next state"));
        }
        break;

        case StateIdle:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateIdle IN"));
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            Err = OMX_FillThisBuffer(ipAppPriv->Handle, ipOutBuffer[0]);
            CHECK_ERROR(Err, "FillThisBuffer");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPortReconfig::Run() - FillThisBuffer command called for initiating dynamic port reconfiguration"));

            ipOutReleased[0] = OMX_FALSE;

            Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
            CHECK_ERROR(Err, "SendCommand Idle->Executing");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPortReconfig::Run() - Sent State Transition Command from Idle->Executing"));
            iPendingCommands = 1;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateIdle OUT"));
        }
        break;

        case StateDecodeHeader:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "OmxDecTestPortReconfig::Run() - StateDecodeHeader IN, Sending configuration input buffers to the component to start dynamic port reconfiguration"));

            if (!iFlagDecodeHeader)
            {
                (*this.*pGetInputFrame)();
                //For AAC and AMR component, send one more frame apart from the config frame, so that we can receive the callback
                if (0 == oscl_strcmp(iFormat, "AAC") || 0 == oscl_strcmp(iFormat, "AMR"))
                {
                    (*this.*pGetInputFrame)();
                }

                iFlagDecodeHeader = OMX_TRUE;

                //Proceed to executing state and if Port settings changed callback comes,
                //then test the dynamic port reconfiguration
                iState = StateExecuting;

                RunIfNotReady();
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateDecodeHeader OUT"));

        }
        break;

        case StateDisablePort:
        {
            OMX_ERRORTYPE Err = OMX_ErrorNone;
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateDisablePort IN"));

            if (!iDisableRun)
            {
                if (!iFlagDisablePort)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxDecTestPortReconfig::Run() - Port Settings Changed callback arrived"));

                    iPortSettingsFlag = OMX_TRUE;

                    Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandPortDisable, iOutputPortIndex, NULL);
                    CHECK_ERROR(Err, "SendCommand_PortDisable");

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxDecTestPortReconfig::Run() - Sent Command for OMX_CommandPortDisable on port %d as a part of dynamic port reconfiguration", iOutputPortIndex));

                    iPendingCommands = 1;
                    iFlagDisablePort = OMX_TRUE;

                    RunIfNotReady();
                }
                else
                {
                    //Wait for the buffers to be returned on output port before freeing them
                    //This wait is required because of the queueing of the FillBufferDone Callbacks

                    for (ii = 0; ii < iOutBufferCount; ii++)
                    {
                        if (OMX_FALSE == ipOutReleased[ii])
                        {
                            break;
                        }
                    }

                    if (ii != iOutBufferCount)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestPortReconfig::Run() - Not all the output buffers returned by component yet, wait for it"));
                        RunIfNotReady();
                        break;
                    }

                    for (ii = 0; ii < iOutBufferCount; ii++)
                    {
                        if (ipOutBuffer[ii])
                        {
                            Err = OMX_FreeBuffer(ipAppPriv->Handle, iOutputPortIndex, ipOutBuffer[ii]);
                            CHECK_ERROR(Err, "FreeBuffer_Output_DynamicReconfig");
                            ipOutBuffer[ii] = NULL;

                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                            (0, "OmxDecTestPortReconfig::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                        }
                    }

                    if (ipOutBuffer)
                    {
                        oscl_free(ipOutBuffer);
                        ipOutBuffer = NULL;
                    }

                    if (ipOutReleased)
                    {
                        oscl_free(ipOutReleased);
                        ipOutReleased = NULL;
                    }

                    if (StateError == iState)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "OmxDecTestPortReconfig::Run() - Error occured in this state, StateDisablePort OUT"));
                        RunIfNotReady();
                        break;
                    }
                    iDisableRun = OMX_TRUE;
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateDisablePort OUT"));
        }
        break;

        case StateDynamicReconfig:
        {
            OMX_BOOL Status = OMX_TRUE;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateDynamicReconfig IN"));

            Status = HandlePortReEnable();
            if (OMX_FALSE == Status)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestPortReconfig::Run() - Error occured in this state, StateDynamicReconfig OUT"));
                iState = StateError;
                RunIfNotReady();
                break;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateDynamicReconfig OUT"));
        }
        break;

        case StateExecuting:
        {
            OMX_U32 Index;
            OMX_BOOL MoreOutput;
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateExecuting IN"));

            /* Check whether we need to proceed further to verify the testcase.
             * If the callback has come, we need not proceed to dynamic port reconfiguration,
             * stop the component now
             */
            if (OMX_TRUE == iPortSettingsFlag)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPortReconfig::Run() - Dynamic reconfiguration sucessful, proceed to stopping the component now"));

                iState = StateStopping;
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPortReconfig::Run() - Port Settings Changed callback has not come yet, processing some buffers while waiting for it"));

                //Continue processing till either the callback comes or processing is complete
                //Send in some output buffers

                MoreOutput = OMX_TRUE;
                while (MoreOutput)
                {
                    Index = 0;
                    while (OMX_FALSE == ipOutReleased[Index] && Index < iOutBufferCount)
                    {
                        Index++;
                    }

                    if (Index != iOutBufferCount)
                    {
                        //This call is being made only once per frame
                        Err = OMX_FillThisBuffer(ipAppPriv->Handle, ipOutBuffer[Index]);
                        CHECK_ERROR(Err, "FillThisBuffer");

                        //Make this flag OMX_TRUE till u receive the callback for output buffer free
                        ipOutReleased[Index] = OMX_FALSE;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestPortReconfig::Run() - FillThisBuffer command called for output buffer index %d", Index));
                    }
                    else
                    {
                        MoreOutput = OMX_FALSE;
                    }
                }


                if (!iStopProcessingInput || (OMX_ErrorInsufficientResources == iStatusExecuting))
                {
                    // find available input buffer
                    Index = 0;
                    while (OMX_FALSE == ipInputAvail[Index] && Index < iInBufferCount)
                    {
                        Index++;
                    }

                    if (Index != iInBufferCount)
                    {
                        iStatusExecuting = (*this.*pGetInputFrame)();
                    }
                }
                else
                {
                    // Processing stopped and no callback came, Stop the test case here and mark failure

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "OmxDecTestPortReconfig::Run() - Processing got stopped before the callback, Stop the test case"));

                    iState = StateStopping;
                }
            }

            RunIfNotReady();
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateExecuting OUT"));
        }


        break;

        case StateStopping:
        {
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateStopping IN"));

            //stop execution by state transition to Idle state.
            if (!iFlagStopping)
            {
                Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
                CHECK_ERROR(Err, "SendCommand Executing->Idle");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPortReconfig::Run() - Sent State Transition Command from Executing->Idle"));

                iPendingCommands = 1;
                iFlagStopping = OMX_TRUE;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateStopping OUT"));
        }
        break;

        case StateCleanUp:
        {
            OMX_U32 ii;
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateCleanUp IN"));

            //Added a check here to verify whether all the ip/op buffers are returned back by the component or not
            //in case of Executing->Idle state transition

            if (OMX_FALSE == VerifyAllBuffersReturned())
            {
                // not all buffers have been returned yet, reschedule
                RunIfNotReady();
                break;
            }

            //Destroy the component by state transition to Loaded state
            Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
            CHECK_ERROR(Err, "SendCommand Idle->Loaded");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPortReconfig::Run() - Sent State Transition Command from Idle->Loaded"));

            iPendingCommands = 1;

            if (ipInBuffer)
            {
                for (ii = 0; ii < iInBufferCount; ii++)
                {
                    if (ipInBuffer[ii])
                    {
                        Err = OMX_FreeBuffer(ipAppPriv->Handle, iInputPortIndex, ipInBuffer[ii]);
                        CHECK_ERROR(Err, "FreeBuffer_Input");
                        ipInBuffer[ii] = NULL;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestPortReconfig::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iInputPortIndex));
                    }
                }

                oscl_free(ipInBuffer);
                ipInBuffer = NULL;
            }

            if (ipInputAvail)
            {
                oscl_free(ipInputAvail);
                ipInputAvail = NULL;
            }


            if (ipOutBuffer)
            {
                for (ii = 0; ii < iOutBufferCount; ii++)
                {
                    if (ipOutBuffer[ii])
                    {
                        Err = OMX_FreeBuffer(ipAppPriv->Handle, iOutputPortIndex, ipOutBuffer[ii]);
                        CHECK_ERROR(Err, "FreeBuffer_Output");
                        ipOutBuffer[ii] = NULL;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestPortReconfig::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                    }
                }
                oscl_free(ipOutBuffer);
                ipOutBuffer = NULL;
            }

            if (ipOutReleased)
            {
                oscl_free(ipOutReleased);
                ipOutReleased = NULL;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateCleanUp OUT"));
        }

        break;


        /********* FREE THE HANDLE & CLOSE FILES FOR THE COMPONENT ********/
        case StateStop:
        {
            OMX_U8 TestName[] = "DYNAMIC_PORT_RECONFIG";
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateStop IN"));

            if (ipAppPriv)
            {
                if (ipAppPriv->Handle)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxDecTestPortReconfig::Run() - Free the Component Handle"));

                    Err = OMX_MasterFreeHandle(ipAppPriv->Handle);
                    if (OMX_ErrorNone != Err)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestPortReconfig::Run() - FreeHandle Error"));
                        iTestStatus = OMX_FALSE;
                    }
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPortReconfig::Run() - De-initialize the omx component"));

            Err = OMX_MasterDeinit();
            if (OMX_ErrorNone != Err)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestPortReconfig::Run() - OMX_MasterDeinit Error"));
                iTestStatus = OMX_FALSE;
            }

            if (0 == oscl_strcmp(iFormat, "H264"))
            {
                if (ipAVCBSO)
                {
                    OSCL_DELETE(ipAVCBSO);
                    ipAVCBSO = NULL;
                }
            }

            if (0 == oscl_strcmp(iFormat, "M4V") || 0 == oscl_strcmp(iFormat, "H263"))
            {
                if (ipBitstreamBuffer)
                {
                    oscl_free(ipBitstreamBufferPointer);
                    ipBitstreamBuffer = NULL;
                    ipBitstreamBufferPointer = NULL;
                }
            }

            if (0 == oscl_strcmp(iFormat, "MP3"))
            {
                if (ipMp3Bitstream)
                {
                    OSCL_DELETE(ipMp3Bitstream);
                    ipMp3Bitstream = NULL;
                }
            }

            if (iOutputParameters)
            {
                oscl_free(iOutputParameters);
                iOutputParameters = NULL;
            }

            if (ipAppPriv)
            {
                oscl_free(ipAppPriv);
                ipAppPriv = NULL;
            }

#if PROXY_INTERFACE
            if (ipThreadSafeHandlerEventHandler)
            {
                OSCL_DELETE(ipThreadSafeHandlerEventHandler);
                ipThreadSafeHandlerEventHandler = NULL;
            }

            if (ipThreadSafeHandlerEmptyBufferDone)
            {
                OSCL_DELETE(ipThreadSafeHandlerEmptyBufferDone);
                ipThreadSafeHandlerEmptyBufferDone = NULL;
            }

            if (ipThreadSafeHandlerFillBufferDone)
            {
                OSCL_DELETE(ipThreadSafeHandlerFillBufferDone);
                ipThreadSafeHandlerFillBufferDone = NULL;
            }
#endif

            if (OMX_FALSE == iTestStatus)
            {
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Fail \n", TestName);
#endif

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "OmxDecTestPortReconfig::Run() - %s : Fail", TestName));
            }
            else
            {
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Success \n", TestName);
#endif

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "OmxDecTestPortReconfig::Run() - %s : Success", TestName));
            }

            iState = StateUnLoaded;
            OsclExecScheduler* sched = OsclExecScheduler::Current();
            sched->StopScheduler();
        }
        break;

        case StateError:
        {
            //Do all the cleanup's and exit from here
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateError IN"));

            iTestStatus = OMX_FALSE;

            if (ipInBuffer)
            {
                for (ii = 0; ii < iInBufferCount; ii++)
                {
                    if (ipInBuffer[ii])
                    {
                        OMX_FreeBuffer(ipAppPriv->Handle, iInputPortIndex, ipInBuffer[ii]);
                        ipInBuffer[ii] = NULL;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestPortReconfig::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iInputPortIndex));
                    }
                }
                oscl_free(ipInBuffer);
                ipInBuffer = NULL;
            }

            if (ipInputAvail)
            {
                oscl_free(ipInputAvail);
                ipInputAvail = NULL;
            }

            if (ipOutBuffer)
            {
                for (ii = 0; ii < iOutBufferCount; ii++)
                {
                    if (ipOutBuffer[ii])
                    {
                        OMX_FreeBuffer(ipAppPriv->Handle, iOutputPortIndex, ipOutBuffer[ii]);
                        ipOutBuffer[ii] = NULL;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestPortReconfig::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                    }
                }
                oscl_free(ipOutBuffer);
                ipOutBuffer = NULL;
            }

            if (ipOutReleased)
            {
                oscl_free(ipOutReleased);
                ipOutReleased = NULL;
            }

            iState = StateStop;
            RunIfNotReady();

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPortReconfig::Run() - StateError OUT"));

        }
        break;

        default:
        {
            break;
        }
    }
    return ;
}

#include "message_buffering.hh"
#include <menabrea/test/params_parser.hh>
#include <menabrea/log.h>
#include <menabrea/cores.h>
#include <menabrea/messaging.h>

/* TODO: Expose platform's MESSAGE_BUFFER_SIZE value */
static constexpr const u32 INTERNAL_BUFFER_SIZE = 16;
static constexpr const TMessageId TEST_MESSAGE_ID = 0xCAFE;
struct TestMessageBufferingParams {
    u32 MessageOverload;
};

static TWorkerId s_workerId = WORKER_ID_INVALID;
/* Worker deployed only to the shared core - use a static variable */
static u32 s_messagesReceived = 0;

static void WorkerBody(TMessage message);

u32 TestMessageBuffering::GetParamsSize(void) {

    return sizeof(TestMessageBufferingParams);
}

int TestMessageBuffering::ParseParams(char * paramsIn, void * paramsOut) {

    ParamsParser::StructLayout paramsLayout;
    paramsLayout["overload"] = ParamsParser::StructField(offsetof(TestMessageBufferingParams, MessageOverload), sizeof(u32), ParamsParser::FieldType::U32);

    return ParamsParser::Parse(paramsIn, paramsOut, std::move(paramsLayout));
}

int TestMessageBuffering::StartTest(void * args) {

    /* Initialize the message counter */
    s_messagesReceived = 0;

    TestMessageBufferingParams * params = static_cast<TestMessageBufferingParams *>(args);
    s_workerId = DeploySimpleWorker("OverloadedConsumer", WORKER_ID_INVALID, GetSharedCoreMask(), WorkerBody);
    if (s_workerId == WORKER_ID_INVALID) {

        LogPrint(ELogSeverityLevel_Error, "Failed to deploy the test worker");
        return -1;
    }

    LogPrint(ELogSeverityLevel_Info, "Testing message buffering. Sending %d message(s) plus extra %d before the worker deploys fully...", \
        INTERNAL_BUFFER_SIZE, params->MessageOverload);


    u32 totalMessageCount = INTERNAL_BUFFER_SIZE + params->MessageOverload;
    for (u32 i = 0; i < totalMessageCount; i++) {

        TMessage message = CreateMessage(TEST_MESSAGE_ID, 0);
        if (unlikely(message == MESSAGE_INVALID)) {

            LogPrint(ELogSeverityLevel_Info, "Failed to create the test message number %d", i);
            return -1;
        }

        SendMessage(message, s_workerId);
    }

    return 0;
}

void TestMessageBuffering::StopTest(void) {

    if (s_workerId != WORKER_ID_INVALID) {

        /* Clean up after a failed test */
        TerminateWorker(s_workerId);
    }
}

static void WorkerBody(TMessage message) {

    if (likely(GetMessageId(message) == TEST_MESSAGE_ID)) {

        s_messagesReceived++;
        DestroyMessage(message);
        if (s_messagesReceived == INTERNAL_BUFFER_SIZE) {

            /* Report success to the test framework */
            TestCase::ReportTestResult(TestCase::Result::Success);
            /* Note that the worker is deployed only on the shared core so we do
             * not risk a race condition where the StopTest callback tries to
             * terminate us again by reading obsolete s_workerId */
            s_workerId = WORKER_ID_INVALID;
            /* Terminate self */
            TerminateWorker(WORKER_ID_INVALID);
        }

    } else {

        LogPrint(ELogSeverityLevel_Warning, "Worker 0x%x received unexpected message 0x%x from 0x%x", \
            GetOwnWorkerId(), GetMessageId(message), GetMessageSender(message));
        DestroyMessage(message);
    }
}

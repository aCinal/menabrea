#include <messaging/setup.h>
#include <menabrea/exception.h>
#include <menabrea/log.h>

void MessagingInit(SMessagingConfig * config) {

    /* Create custom event pool */
    AssertTrue(MESSAGING_EVENT_POOL == em_pool_create("messaging_pool", MESSAGING_EVENT_POOL, &config->PoolConfig));
    LogPrint(ELogSeverityLevel_Info, "Successfully created event pool %" PRI_POOL " for messaging framework's use", \
        MESSAGING_EVENT_POOL);

    MessagingNetworkInit(&config->NetworkingConfig);
}

void MessagingTeardown(void) {

    MessagingNetworkTeardown();

    LogPrint(ELogSeverityLevel_Info, "Deleting the message pool...");
    /* Delete the event pool */
    AssertTrue(EM_OK ==  em_pool_delete(MESSAGING_EVENT_POOL));
}

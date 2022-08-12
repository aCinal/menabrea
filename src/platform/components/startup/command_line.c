#include <startup/command_line.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

static void SetDefaults(SStartupParams * params);
static void SetDefaultPoolConfig(SPoolConfig * poolConfig);
static void ParsePoolConfig(const char * optarg, SPoolConfig * config);
static void ParseSubpoolConfig(const char * token, SSubpoolConfig * config);
static void PrintPoolConfig(const SPoolConfig * config);

SStartupParams * ParseCommandLine(int argc, char **argv) {

    SStartupParams * params = malloc(sizeof(SStartupParams));
    AssertTrue(params != NULL);

    SetDefaults(params);

    const struct option longOptions[] = {
        { "defaultPoolConfig", required_argument, NULL, 0},
        { "messagingPoolConfig", required_argument, NULL, 0 },
        { "nodeId", required_argument, NULL, 0 },
        { "netIf", required_argument, NULL, 0 },
        { 0, 0, 0, 0 }
    };
    int optionIndex;

    while (-1 != getopt_long(argc, argv, "", longOptions, &optionIndex)) {

        char * endptr;

        /* Find the corresponding long option - in each case assert
         * that the order in the longOptions structure was not
         * changed by a pedantic developer */
        switch (optionIndex) {
        case 0:
            AssertTrue(0 == strcmp("defaultPoolConfig", longOptions[optionIndex].name));
            LogPrint(ELogSeverityLevel_Debug, "Parsing the default pool config...");
            ParsePoolConfig(optarg, &params->DefaultPoolConfig);
            PrintPoolConfig(&params->DefaultPoolConfig);
            break;

        case 1:
            AssertTrue(0 == strcmp("messagingPoolConfig", longOptions[optionIndex].name));
            LogPrint(ELogSeverityLevel_Debug, "Parsing the messaging pool config...");
            ParsePoolConfig(optarg, &params->MessagingPoolConfig);
            PrintPoolConfig(&params->MessagingPoolConfig);
            break;

        case 2:
            AssertTrue(0 == strcmp("nodeId", longOptions[optionIndex].name));
            LogPrint(ELogSeverityLevel_Debug, "Parsing node ID...");
            params->NodeId = strtol(optarg, &endptr, 0);
            /* Assert a number was parsed */
            AssertTrue(endptr != optarg);
            AssertTrue(params->NodeId <= MAX_NODE_ID);
            LogPrint(ELogSeverityLevel_Debug, "Node ID set to %d", \
                params->NodeId);
            break;

        case 3:
            AssertTrue(0 == strcmp("netIf", longOptions[optionIndex].name));
            LogPrint(ELogSeverityLevel_Debug, "Parsing network interface...");
            /* Copy the name and ensure proper NULL-termination (see strncpy manpage) */
            (void) strncpy(params->NetworkInterface, optarg, sizeof(params->NetworkInterface) - 1);
            params->NetworkInterface[sizeof(params->NetworkInterface) - 1] = '\0';
            LogPrint(ELogSeverityLevel_Debug, "Network interface set to '%s'", \
                params->NetworkInterface);
            break;

        default:
            /* Should never get here */
            RaiseException(EExceptionFatality_Fatal, 0, \
                "getopt_long() returned unexpected optionIndex: %d", optionIndex);
            break;
        }
    }

    /* Assert node ID has been set - we cannot rely on
     * any defaults here */
    AssertTrue(params->NodeId != WORKER_ID_INVALID);

    return params;
}

void ReleaseStartupParams(SStartupParams * params) {

    free(params);
}

static void SetDefaults(SStartupParams * params) {

    /* Configure the event pools */
    SetDefaultPoolConfig(&params->DefaultPoolConfig);
    SetDefaultPoolConfig(&params->MessagingPoolConfig);

    params->NodeId = WORKER_ID_INVALID;

    (void) strcpy(params->NetworkInterface, "eth0");
}

static void SetDefaultPoolConfig(SPoolConfig * poolConfig) {

    poolConfig->Subpools[0].BufferSize = 256;
    poolConfig->Subpools[0].NumOfBuffers = 16384;
    poolConfig->Subpools[0].CacheSize = 64;

    poolConfig->Subpools[1].BufferSize = 512;
    poolConfig->Subpools[1].NumOfBuffers = 1024;
    poolConfig->Subpools[1].CacheSize = 32;

    poolConfig->Subpools[2].BufferSize = 1024;
    poolConfig->Subpools[2].NumOfBuffers = 1024;
    poolConfig->Subpools[2].CacheSize = 16;

    poolConfig->Subpools[3].BufferSize = 2048;
    poolConfig->Subpools[3].NumOfBuffers = 1024;
    poolConfig->Subpools[3].CacheSize = 8;
}

static void ParsePoolConfig(const char * optarg, SPoolConfig * config) {

    /* Pool config command-line parameter should have the
     * format: <SubpoolCount>,<subpool[0].size>:<subpool[0].num>:
     * <subpool[0].cache_size>,<subpool[1].size>:... etc.*/

    size_t optargLen = strlen(optarg);
    /* Make a copy of the optarg string on the heap to safely tokenize */
    char * copy = malloc(optargLen + 1);
    AssertTrue(copy != NULL);
    (void) strcpy(copy, optarg);

    /* Parse the number of subpools */
    char * saveptr = NULL;

    char * token = strtok_r(copy, ",", &saveptr);
    /* First token will never be NULL, but make an
     * assertion about it nonetheless */
    AssertTrue(token != NULL);

    char * endptr;
    /* Parse the first token as the number of subpools -
     * enforce base 10 */
    config->SubpoolCount = strtol(token, &endptr, 10);
    /* Assert no other characters present in the token
     * except the number of subpools */
    AssertTrue(*endptr == '\0');
    AssertTrue(config->SubpoolCount <= EM_MAX_SUBPOOLS);

    for (int i = 0; i < config->SubpoolCount; i++) {

        /* Get the next token */
        token = strtok_r(NULL, ",", &saveptr);
        /* Assert consistency with the declared subpool
         * count */
        AssertTrue(token != NULL);
        /* Parse the token as a subpool config */
        ParseSubpoolConfig(token, &config->Subpools[i]);
    }

    /* Do one more call to strtok_r and assert no trailing
     * tokens are present */
    token = strtok_r(NULL, ",", &saveptr);
    AssertTrue(token == NULL);

    free(copy);
}

static void ParseSubpoolConfig(const char * token, SSubpoolConfig * config) {

    size_t tokenLen = strlen(token);
    /* Allocate a copy on the heap to not interfere with the
     * operations of the strtok_r calls by our caller (i.e. one
     * level up in the callstack) */
    char * copy = malloc(tokenLen + 1);
    AssertTrue(copy != NULL);
    (void) strcpy(copy, token);

    char * saveptr = NULL;
    char * endptr;

    /* Note that subpool config fields are separated by colons */
    char * field = strtok_r(copy, ":", &saveptr);
    /* Parse the field - enforce base 10 and at each step
     * make an assertion about the format */
    config->BufferSize = strtol(field, &endptr, 10);
    AssertTrue(*endptr == '\0');
    field = strtok_r(NULL, ":", &saveptr);
    AssertTrue(field != NULL);
    config->NumOfBuffers = strtol(field, &endptr, 10);
    AssertTrue(*endptr == '\0');
    field = strtok_r(NULL, ":", &saveptr);
    AssertTrue(field != NULL);
    config->CacheSize = strtol(field, &endptr, 10);
    AssertTrue(*endptr == '\0');
    /* Do one more call to strtok_r and assert that no more
     * fields are present */
    field = strtok_r(NULL, ":", &saveptr);
    AssertTrue(field == NULL);

    /* Free the temporary buffer */
    free(copy);
}

static void PrintPoolConfig(const SPoolConfig * config) {

    LogPrint(ELogSeverityLevel_Debug, "Number of subpools: %d", config->SubpoolCount);
    for (int i = 0; i < config->SubpoolCount; i++) {

        LogPrint(ELogSeverityLevel_Debug, "    subpool %d:", i);
        LogPrint(ELogSeverityLevel_Debug, "        size: %d", config->Subpools[i].BufferSize);
        LogPrint(ELogSeverityLevel_Debug, "        num: %d", config->Subpools[i].NumOfBuffers);
        LogPrint(ELogSeverityLevel_Debug, "        cache_size: %d", config->Subpools[i].CacheSize);
    }
}

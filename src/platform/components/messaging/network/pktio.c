#include <messaging/network/pktio.h>
#include <messaging/network/translation.h>
#include <menabrea/log.h>
#include <menabrea/exception.h>
#include <event_machine.h>
#include <event_machine/platform/event_machine_odp_ext.h>

static em_pool_t CreatePacketPool(u32 bufCount);
static odp_pktio_t CreatePktioDevice(const char * ifName, odp_pool_t odpPool);
static void CreatePktioQueues(odp_pktio_t pktio);

static odp_pktio_t s_pktio;
static odp_pktin_queue_t s_pktinQueue;
static odp_queue_t s_pktoutQueue;

#define PKTIO_POOL_BUF_SIZE   MAX_ETH_PACKET_SIZE

void PktioInit(const char * ifName, u32 bufCount) {

    /* Create a packet pool */
    em_pool_t emPool = CreatePacketPool(bufCount);

    /* Convert to ODP pool */
    odp_pool_t odpPool;
    AssertTrue(1 == em_odp_pool2odp(emPool, &odpPool, 1));

    /* Create a pktio device */
    s_pktio = CreatePktioDevice(ifName, odpPool);

    /* Create input and output queues */
    CreatePktioQueues(s_pktio);

    /* Start the PKTIO */
    AssertTrue(0 == odp_pktio_start(s_pktio));

    /* Print out configuration */
    odp_pktio_print(s_pktio);
}

void PktioTeardown(void) {

    /* Stop the pktio device */
    AssertTrue(0 == odp_pktio_stop(s_pktio));
    AssertTrue(0 == odp_pktio_close(s_pktio));

    /* Delete the packet pool */
    AssertTrue(EM_OK == em_pool_delete(NETWORKING_PACKET_POOL));
}

odp_queue_t GetPktoutQueue(void) {

    return s_pktoutQueue;
}

odp_pktin_queue_t GetPktinQueue(void) {

    return s_pktinQueue;
}

static em_pool_t CreatePacketPool(u32 bufCount) {

    LogPrint(ELogSeverityLevel_Info, "Creating pktio packet pool with %d buffers...", bufCount);

    odp_pool_capability_t odpPoolCapa;
    /* Query ODP pool capabilities */
    AssertTrue(0 == odp_pool_capability(&odpPoolCapa));

    em_pool_cfg_t emPoolConfig;
    /* Create the packet pool as EM pool */
    em_pool_cfg_init(&emPoolConfig);
    emPoolConfig.event_type = EM_EVENT_TYPE_PACKET;
    emPoolConfig.num_subpools = 1;
    emPoolConfig.subpool[0].size = PKTIO_POOL_BUF_SIZE;
    emPoolConfig.subpool[0].num = bufCount;
    /* Use max thread-local cache to speed up pktio allocations */
    emPoolConfig.subpool[0].cache_size = odpPoolCapa.pkt.max_cache_size;

    em_pool_t emPool = em_pool_create("pktio_pool", NETWORKING_PACKET_POOL, &emPoolConfig);
    AssertTrue(emPool != EM_POOL_UNDEF);
    return emPool;
}

static odp_pktio_t CreatePktioDevice(const char * ifName, odp_pool_t odpPool) {

    odp_pktio_param_t pktioParams;
    odp_pktio_param_init(&pktioParams);
    /* Direct mode for RX - we will poll the device directly via
     * EM callback */
    pktioParams.in_mode = ODP_PKTIN_MODE_DIRECT;
    /* Queue mode for TX to preserve packet order if needed */
    pktioParams.out_mode = ODP_PKTOUT_MODE_QUEUE;

    LogPrint(ELogSeverityLevel_Info, "Creating a pktio device '%s'...", ifName);

    /* Create a pktio device */
    odp_pktio_t pktio = odp_pktio_open(ifName, odpPool, &pktioParams);
    AssertTrue(pktio != ODP_PKTIO_INVALID);

    odp_pktio_info_t pktioInfo;
    /* Query device information */
    AssertTrue(0 == odp_pktio_info(pktio, &pktioInfo));

    LogPrint(ELogSeverityLevel_Debug, \
        "Successfully created pktio device %" PRIu64 " (index: %d, device: '%s', driver: '%s')", \
        odp_pktio_to_u64(pktio), odp_pktio_index(pktio), ifName, pktioInfo.drv_name);

    /* Configure the pktio */
    odp_pktio_config_t pktioConfig;
    odp_pktio_config_init(&pktioConfig);
    /* Do not parse any headers */
    pktioConfig.parser.layer = ODP_PROTO_LAYER_NONE;
    AssertTrue(0 == odp_pktio_config(pktio, &pktioConfig));

    return pktio;
}

static void CreatePktioQueues(odp_pktio_t pktio) {

    /* Configure input and output queues */
    odp_pktin_queue_param_t pktinQueueParams;
    odp_pktin_queue_param_init(&pktinQueueParams);
    pktinQueueParams.num_queues = 1;
    pktinQueueParams.op_mode = ODP_PKTIO_OP_MT;
    AssertTrue(0 == odp_pktin_queue_config(pktio, &pktinQueueParams));

    odp_pktout_queue_param_t pktoutQueueParams;
    odp_pktout_queue_param_init(&pktoutQueueParams);
    pktoutQueueParams.num_queues = 1;
    pktoutQueueParams.op_mode = ODP_PKTIO_OP_MT;
    AssertTrue(0 == odp_pktout_queue_config(pktio, &pktoutQueueParams));

    /* Create input and output queues */
    AssertTrue(1 == odp_pktin_queue(pktio, &s_pktinQueue, 1));
    AssertTrue(1 == odp_pktout_event_queue(pktio, &s_pktoutQueue, 1));
}

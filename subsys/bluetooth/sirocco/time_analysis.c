/*
 */
#include <zephyr/bluetooth/srcc_time_analysis.h>
#include <zephyr/sys_clock.h>
#include <nrfx.h>
#include <nrfx_timer.h>
#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(srccanalysis, CONFIG_LOG_DEFAULT_LEVEL);


static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(3);

static void init_time_analysis(void)
{
    nrfx_err_t err;

    nrfx_timer_config_t config = {
        .frequency = NRF_TIMER_BASE_FREQUENCY_GET(timer.p_reg), // 16M Hz
        .mode = NRF_TIMER_MODE_TIMER,
        .bit_width = NRF_TIMER_BIT_WIDTH_32,
        .interrupt_priority = NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY,
        .p_context = NULL
    };

    err = nrfx_timer_init(&timer, &config, NULL);
    NRFX_ASSERT(err == NRFX_SUCCESS);

	//nrfx_timer_disable(&timer);
    nrfx_timer_clear(&timer);

	nrfx_timer_enable(&timer);
}

void init_srcc_analysis(void)
{
    init_time_analysis();
}


uint32_t srcc_analysis_capture_cycles(void)
{
    return nrfx_timer_capture(&timer, NRF_TIMER_CC_CHANNEL3);
}

static uint32_t calculate_elapsed_time(uint32_t start, uint32_t end) {
    if (end >= start) {
        return end - start;
    } else {
        // timer has overflowed
        return end + (0xffffffffUL - start) + 1;
    }
}


#if defined(CONFIG_SRCC_ISR_LATENCY)

struct srcc_timestamp {
    uint32_t start;
    uint32_t end;
};

#define MAX_TIMESTAMP 6000
static struct srcc_timestamp conn_rx_ts[MAX_TIMESTAMP];
static struct srcc_timestamp conn_tx_ts[MAX_TIMESTAMP];
static struct srcc_timestamp scan_rx_ts[MAX_TIMESTAMP];
static struct srcc_timestamp adv_rx_ts[MAX_TIMESTAMP];
static struct srcc_timestamp adv_tx_ts[MAX_TIMESTAMP];
static int conn_rx_ts_index = 0;
static int conn_tx_ts_index = 0;
static int scan_rx_ts_index = 0;
static int adv_rx_ts_index = 0;
static int adv_tx_ts_index = 0;


void analyze_timestamps(struct srcc_timestamp timestamp[], int index, char *name)
{
    uint32_t latency = 0;
    uint32_t min_latency = 0xffffffffUL;
    uint32_t max_latency = 0;
    uint64_t total_latency = 0;

    for (int i = 0; i < index; i++) {
        if (timestamp[i].end > timestamp[i].start) {
            latency = timestamp[i].end - timestamp[i].start;
        } else {
            // timer has overflowed
            latency = timestamp[i].end + (0xffffffffUL - timestamp[i].start) + 1;
        }

        min_latency = (latency < min_latency) ? latency : min_latency;
        max_latency = (latency > max_latency) ? latency : max_latency;
        total_latency += latency;
    }

    //double avg_latency = (index > 0) ? (double)total_latency / index : 0.0;
    
    LOG_INF("[%s] (cycles) Average: %lld/%d min: %d max: %d",
            name,
            total_latency,
            index,
            min_latency,
            max_latency);

    const uint32_t clock_freq_mhz = 64;
    LOG_INF("[%s] (us) Average: %lld/%d min: %d max: %d", 
            name,
            total_latency / clock_freq_mhz,
            index,
            min_latency / clock_freq_mhz,
            max_latency / clock_freq_mhz);
}

void start_timestamp_conn_rx_isr(void)
{
    conn_rx_ts[conn_rx_ts_index].start = capture_cycles();
}

void start_timestamp_conn_tx_isr(void)
{
    conn_tx_ts[conn_tx_ts_index].start = capture_cycles();
}

void start_timestamp_scan_rx_isr(void)
{
    scan_rx_ts[scan_rx_ts_index].start = capture_cycles();
}

void start_timestamp_adv_rx_isr(void)
{
    adv_rx_ts[adv_rx_ts_index].start = capture_cycles();
}

void start_timestamp_adv_tx_isr(void)
{
    conn_tx_ts[conn_tx_ts_index].start = capture_cycles();
}


void stop_timestamp_conn_rx_isr(void)
{
    conn_rx_ts[conn_rx_ts_index].end = capture_cycles();
    conn_rx_ts_index++;
    if (conn_rx_ts_index >= MAX_TIMESTAMP) {
        analyze_timestamps(conn_rx_ts, conn_rx_ts_index, "conn_rx");
        conn_rx_ts_index = 0;
    }
}

void stop_timestamp_conn_tx_isr(void)
{
    conn_tx_ts[conn_tx_ts_index].end = capture_cycles();
    conn_tx_ts_index++;
    if (conn_tx_ts_index >= MAX_TIMESTAMP) {
        analyze_timestamps(conn_tx_ts, conn_tx_ts_index, "conn_tx");
        conn_tx_ts_index = 0;
    }
}

void stop_timestamp_scan_rx_isr(void)
{
    scan_rx_ts[scan_rx_ts_index].end = capture_cycles();
    scan_rx_ts_index++;
    if (scan_rx_ts_index >= MAX_TIMESTAMP) {
        analyze_timestamps(scan_rx_ts, scan_rx_ts_index, "scan_rx");
        scan_rx_ts_index = 0;
    }
}

void stop_timestamp_adv_rx_isr(void)
{
    adv_rx_ts[adv_rx_ts_index].end = capture_cycles();
    adv_rx_ts_index++;
    if (adv_rx_ts_index >= MAX_TIMESTAMP) {
        analyze_timestamps(adv_rx_ts, adv_rx_ts_index, "adv_rx");
        adv_rx_ts_index = 0;
    }
}

void stop_timestamp_adv_tx_isr(void)
{
    adv_tx_ts[adv_tx_ts_index].end = capture_cycles();
    adv_tx_ts_index++;
    if (adv_tx_ts_index >= MAX_TIMESTAMP) {
        analyze_timestamps(adv_tx_ts, adv_tx_ts_index, "adv_tx");
        adv_tx_ts_index = 0;
    }
}
#endif /* CONFIG_SRCC_ISR_LATENCY */

/* To measure FIFO latency and throughput */
#if defined(CONFIG_SRCC_FIFO_LATENCY)

struct fifo_metrics {
    uint32_t enqueue_time;
    uint32_t dequeue_time;
    uint32_t queue_length;
};

struct fifo_analysis {
    uint32_t min_latency;
    uint32_t max_latency;
    uint64_t avg_latency;
    uint32_t max_queue_length;
    uint32_t total_messages;
    uint32_t dropped_messages;
};

#define MAX_METRICS 6000
static struct fifo_metrics fifo_metrics[MAX_METRICS];
static atomic_t fifo_metrics_count = 0;
static atomic_t current_queue_length = 0;
static atomic_t dropped_messages = 0;


void record_enqueue_metric(void)
{
    int index = atomic_inc(&fifo_metrics_count);

    if (index < MAX_METRICS) {
        fifo_metrics[index].enqueue_time = capture_cycles();
        fifo_metrics[index].queue_length = atomic_inc(&current_queue_length);
    } else {
        atomic_inc(&dropped_messages);
    }
}

void record_dequeue_metric(void)
{
    int index = atomic_get(&fifo_metrics_count) - 1;

    if (index >= 0 && index < MAX_METRICS) {
        fifo_metrics[index].dequeue_time = capture_cycles();
        atomic_dec(&current_queue_length);
    }
}

void reset_fifo_metrics(void) {
    atomic_set(&fifo_metrics_count, 0);
    atomic_set(&current_queue_length, 0);
    atomic_set(&dropped_messages, 0);
}


void analyze_fifo_metrics(void)
{
    struct fifo_analysis analysis = {
        .min_latency = 0xffffffffUL,
        .max_latency = 0,
        .avg_latency = 0,
        .max_queue_length = 0,
        .total_messages = 0,
        .dropped_messages = atomic_get(&dropped_messages)
    };

    uint64_t total_latency = 0;
    int count = atomic_get(&fifo_metrics_count);
    count = (count < MAX_METRICS) ? count : MAX_METRICS;

    for (int i = 0; i < count; i++) {
        uint32_t latency = calculate_elapsed_time(
            fifo_metrics[i].enqueue_time,
            fifo_metrics[i].dequeue_time
        );

        // Update min/max latency
        analysis.min_latency = (latency < analysis.min_latency) ? latency : analysis.min_latency;
        analysis.max_latency = (latency > analysis.max_latency) ? latency : analysis.max_latency;
        total_latency += latency;

        // Update max queue length
        analysis.max_queue_length = MAX(
            analysis.max_queue_length,
            fifo_metrics[i].queue_length
        );
    }

    // Calculate average latency
    if (count > 0) {
        analysis.avg_latency = total_latency / count;
    }
    analysis.total_messages = count;

    // Print analysis results
    const uint32_t clock_freq_mhz = 64;
    LOG_INF("FIFO Analysis Results:");
    LOG_INF("  Total messages processed: %u", analysis.total_messages);
    LOG_INF("  Dropped messages: %u", analysis.dropped_messages);
    LOG_INF("  Max queue length: %u", analysis.max_queue_length);
    LOG_INF("  Avg latency: %llu cycles (%llu us)",
            analysis.avg_latency, analysis.avg_latency / clock_freq_mhz);
    LOG_INF("  Min latency: %u cycles (%u us)",
            analysis.min_latency, analysis.min_latency / clock_freq_mhz);
    LOG_INF("  Max latency: %u cycles (%u us)",
             analysis.max_latency, analysis.max_latency / clock_freq_mhz);
}

static void srcc_fifo_analysis_loop(void *, void *, void *)
{
    /* Sleep and print measures. */
    while (1) {
        //k_msleep(10000);    // 10sec
        //k_msleep(60000);    // 1min
        k_msleep(300000);   // 5min
        analyze_fifo_metrics();
        reset_fifo_metrics();
    }
}

#define SRCC_ANALYSIS_THREAD_STACK_SIZE 4096
#define SRCC_ANALYSIS_THREAD_PRIORITY 5
K_THREAD_DEFINE(fifo_analysis_id,
				SRCC_ANALYSIS_THREAD_STACK_SIZE,
				srcc_fifo_analysis_loop,
				NULL,
				NULL,
				NULL,
				SRCC_ANALYSIS_THREAD_PRIORITY,
				0,
				0);
#endif /* CONFIG_SRCC_FIFO_LATENCY */



#if defined(CONFIG_SRCC_E2E_LATENCY)

struct e2e_metrics {
    uint32_t start;
    uint32_t end;
};

struct e2e_analysis {
    uint32_t min_latency;
    uint32_t max_latency;
    uint64_t avg_latency;
    uint32_t total_packets;
};

#define MAX_E2E_METRICS 6100
static struct e2e_metrics e2e_conn_rx[MAX_E2E_METRICS];
static atomic_t e2e_conn_rx_index = 0;

void srcc_e2e_conn_rx(uint32_t e2e_start_cycles)
{
    struct e2e_metrics metrics;

    metrics.end = srcc_analysis_capture_cycles();
    metrics.start = e2e_start_cycles;

    if (e2e_conn_rx_index < MAX_E2E_METRICS) {
        e2e_conn_rx[e2e_conn_rx_index] = metrics;
        atomic_inc(&e2e_conn_rx_index);
    }
}

static void reset_e2e_metrics(void)
{
    memset(&e2e_conn_rx, 0, MAX_E2E_METRICS);
    atomic_set(&e2e_conn_rx_index, 0);
}

static void analyze_e2e_metrics(void)
{
    struct e2e_analysis conn_rx_analysis = {
        .min_latency = 0xffffffffUL,
        .max_latency = 0,
        .avg_latency = 0,
        .total_packets = 0,
    };
    uint64_t total_latency = 0;
    int count = atomic_get(&e2e_conn_rx_index);
    count = (count < MAX_E2E_METRICS) ? count : MAX_E2E_METRICS;

    for (int i = 0; i < count; i++) {
        uint32_t latency = calculate_elapsed_time(
            e2e_conn_rx[i].start,
            e2e_conn_rx[i].end
        );

        /* Update min/max latency */
        conn_rx_analysis.min_latency = (latency < conn_rx_analysis.min_latency) ? latency : conn_rx_analysis.min_latency;
        conn_rx_analysis.max_latency = (latency > conn_rx_analysis.max_latency) ? latency : conn_rx_analysis.max_latency;
        total_latency += latency;
    }

    /* Calculate average latency */
    if (count > 0) {
        conn_rx_analysis.avg_latency = total_latency / count;

    } else {
        conn_rx_analysis.min_latency = 0;
    }
    conn_rx_analysis.total_packets = count;

    /* Print analysis results */
    const uint32_t clock_freq_mhz = 64;
    LOG_INF("E2E Analysis Results:");
    LOG_INF("  For conn_rx:");
    LOG_INF("    Total packets processed: %u", conn_rx_analysis.total_packets);
    LOG_INF("    Avg latency: %llu cycles (%llu us)",
            conn_rx_analysis.avg_latency, conn_rx_analysis.avg_latency / clock_freq_mhz);
    LOG_INF("    Min latency: %u cycles (%u us)",
            conn_rx_analysis.min_latency, conn_rx_analysis.min_latency / clock_freq_mhz);
    LOG_INF("    Max latency: %u cycles (%u us)",
             conn_rx_analysis.max_latency, conn_rx_analysis.max_latency / clock_freq_mhz);
}


static void srcc_e2e_analysis_loop(void *, void *, void *)
{
    /* Sleep and print measures. */
    while (1) {
        //k_msleep(10000);    // 10sec
        //k_msleep(60000);    // 1min
        k_msleep(300000);   // 5min
        analyze_e2e_metrics();
        reset_e2e_metrics();
    }
}

#define SRCC_E2E_THREAD_STACK_SIZE 4096
#define SRCC_E2E_THREAD_PRIORITY 5
K_THREAD_DEFINE(e2e_analysis_id,
				SRCC_E2E_THREAD_STACK_SIZE,
				srcc_e2e_analysis_loop,
				NULL,
				NULL,
				NULL,
				SRCC_E2E_THREAD_PRIORITY,
				0,
				0);
#endif /* CONFIG_SRCC_E2E_LATENCY */
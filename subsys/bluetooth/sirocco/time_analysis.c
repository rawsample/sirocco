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


/* */

static uint32_t capture_cycles(void)
{
    return nrfx_timer_capture(&timer, NRF_TIMER_CC_CHANNEL3);
}


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


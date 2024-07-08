/*
 */
//#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>

#include<zephyr/bluetooth/sirocco.h>

#include "../scan.h"


LOG_MODULE_DECLARE(sirocco, CONFIG_BT_SRCC_LOG_LEVEL);

#define OASIS_GATTACKER_SUSPICIOUS_THRESHOLD 50

#define MAX_ADV_DELAY 300    /* in ms */
//#define MAX_ADV_DELAY_CYCLES (MAX_ADV_DELAY * CYCLES_PER_SEC)

static uint8_t periph_addr[BDADDR_SIZE] = {0xaa, 0xaa, 0xef, 0xbe, 0xad, 0xde};


void init_oasis_gattacker_data(struct oasis_gattacker_data *data)
{
    data->threshold = 0;
    data->last_channel = 0;
    ring_buf_init(&data->adv_intervals.rb, RBUFF_SIZE, data->adv_intervals.buf);
}

void clean_oasis_gattacker_data(struct oasis_gattacker_data *data)
{
    /* Nothing to do */
}


static int estimate_threshold(struct oasis_gattacker_data *gattacker_data,
                              struct srcc_scan_metric *metric, uint32_t adv_interval)
{
    uint32_t min_interval;
    uint32_t buf[RBUFF_SIZE/sizeof(uint32_t)];
#if (CONFIG_BT_SRCC_LOG_LEVEL >= 4)
    char print_buf[(RBUFF_SIZE/sizeof(uint32_t) * 11) + 1];    // each uint32_t can be up to 10 digits + 1 space + 1 null terminator
    char *pbuf_ptr = print_buf;
#endif /* CONFIG_BT_SRCC_LOG_LEVEL >= 4 */
    uint32_t ret, err;

    ret = ring_buf_space_get(&(gattacker_data->adv_intervals.rb));

    /* Delete the oldest data if the buffer is full */
    if (ret < sizeof(uint32_t)) {
        err = ring_buf_get(&gattacker_data->adv_intervals.rb,
                     NULL, sizeof(uint32_t));
    }

    /* Add the new interval to the ring buffer */
    ring_buf_put(&gattacker_data->adv_intervals.rb,
                 (uint8_t *)&adv_interval, sizeof(uint32_t));

    /* Buffer is not full, leave because it is the _learning phase_ */
    if (ret > 0) {
        return 0;
    }

    /* Calculate the min of the window */
    ret = ring_buf_peek(&gattacker_data->adv_intervals.rb,
                        (uint8_t *)buf, RBUFF_SIZE);
    if (ret != RBUFF_SIZE) {
        printk("Failed to peek the ring buffer\n");
        return -1;
    }

    min_interval = buf[0];
    for (int i=1; i < (RBUFF_SIZE/sizeof(uint32_t)); i++) {
        if (buf[i] < min_interval) {
            min_interval = buf[i];
        }
    }
    LOG_DBG("min_interval = %u ms", min_interval);

    /* Print buffer content */
#if (CONFIG_BT_SRCC_LOG_LEVEL >= 4)
    for (int i=0; i < (RBUFF_SIZE/sizeof(uint32_t)); i++) {
        //pbuf_ptr += sprintf(pbuf_ptr, "%u ", buf[i]);
        int written = sprintf(pbuf_ptr, "%u ", buf[i]);
        pbuf_ptr += written;
    }
    *pbuf_ptr = '\0';
#endif /* CONFIG_BT_SRCC_LOG_LEVEL >= 4 */
    LOG_DBG("buf: %s", print_buf);

    /* Compute the detection threshold */
    if (gattacker_data->threshold == 0
        || min_interval >  MAX_ADV_DELAY) {

        gattacker_data->threshold = MAX_ADV_DELAY;
        /*
        if (min_interval > MAX_ADV_DELAY_CYCLES) {
            data->oasis_gattacker_data.threshold = min_interval - MAX_ADV_DELAY_CYCLES;
        } else {
            data->oasis_gattacker_data.threshold = MAX_ADV_DELAY_CYCLES;
        }
        */
        LOG_DBG("%02X:%02X:%02X:%02X:%02X:%02X - set threshold to %u ms",
            metric->adv_ind.addr[5],
            metric->adv_ind.addr[4],
            metric->adv_ind.addr[3],
            metric->adv_ind.addr[2],
            metric->adv_ind.addr[1],
            metric->adv_ind.addr[0],
            gattacker_data->threshold);
    }

    return 1;
}

static void do_detection(uint64_t address, struct oasis_gattacker_data *gattacker_data,
                         struct srcc_scan_metric *metric, uint32_t adv_interval)
{
  if (adv_interval < gattacker_data->threshold) {
      gattacker_data->suspicious++;

      if (gattacker_data->suspicious > OASIS_GATTACKER_SUSPICIOUS_THRESHOLD) {
          srcc_alert(GATTACKER, "Suspicious activity detected from %x", address);
      }

  /* Reset the suspicious activity counter */
  } else if (gattacker_data->suspicious) {
      gattacker_data->suspicious = 0;
  }
}


void srcc_detect_oasis_gattacker(uint64_t address, struct scan_data *scan_data,
                                 struct srcc_scan_metric *metric)
{
    uint32_t adv_interval;
    struct oasis_gattacker_data *gattacker_data;

    /* Look only ADV_IND */
    if (metric->type != 0b0000) {
        return;
    }

    /* Select the ADV channel
     * The GATTacker detection module works only on the same channel
     * because of the estimated ADV interval.
     */
    gattacker_data = &(scan_data->channel[metric->channel]);
    /*
    if (data->oasis_gattacker_data.last_channel != metric->channel) {
        data->oasis_gattacker_data.last_channel = metric->channel;
        data->oasis_gattacker_data.threshold = 0;
        return;
    }
    */

    //adv_interval = srcc_timing_capture_ms();
    //printk("current timestamp %u ms\n", adv_interval);

    /* Calculare the interval between two ADV_IND in cycles */
    adv_interval = metric->timestamp - gattacker_data->previous_adv_timestamp;
    LOG_DBG("chan %d - adv_interval %u - timestamp %u - previous %u",
            metric->channel, adv_interval, metric->timestamp, gattacker_data->previous_adv_timestamp);
    LOG_DBG("%02X:%02X:%02X:%02X:%02X:%02X - chan %d - %u ms",
           metric->adv_ind.addr[5],
           metric->adv_ind.addr[4],
           metric->adv_ind.addr[3],
           metric->adv_ind.addr[2],
           metric->adv_ind.addr[1],
           metric->adv_ind.addr[0],
           metric->channel,
           adv_interval);

    /* Estimate the threshold */
    if (estimate_threshold(gattacker_data, metric, adv_interval) >= 0) {

        /* Detect MitM */
        do_detection(address, gattacker_data, metric, adv_interval);
    }

    /* Save the current metric if it is an ADV_IND */
    //memcpy(&data->previous_metric, metric, sizeof(struct srcc_scan_metric));
    gattacker_data->previous_adv_timestamp = metric->timestamp;
}

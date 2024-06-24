/*
 */
//#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>

#include<zephyr/bluetooth/sirocco.h>

#include "../scan.h"


#define MAX_ADV_DELAY 300    /* in ms */
//#define MAX_ADV_DELAY_CYCLES (MAX_ADV_DELAY * CYCLES_PER_SEC)



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


static int estimate_threshold(struct scan_data *data,
                              struct srcc_scan_metric *metric, uint32_t adv_interval)
{
    uint32_t min_interval;
    uint32_t buf[RBUFF_SIZE/sizeof(uint32_t)];
    uint32_t ret, err;

    ret = ring_buf_space_get(&(data->oasis_gattacker_data.adv_intervals.rb));
    //printk("ring_buf_space_get = %u\n", ret);

    /* Delete the oldest data if the buffer is full */
    if (ret < sizeof(uint32_t)) {
        err = ring_buf_get(&data->oasis_gattacker_data.adv_intervals.rb,
                     NULL, sizeof(uint32_t));
    }

    /* Add the new interval to the ring buffer */
    ring_buf_put(&data->oasis_gattacker_data.adv_intervals.rb,
                 (uint8_t *)&adv_interval, sizeof(uint32_t));

    /* Buffer is not full and therefore not considered as initialized */
    if (ret > 0) {
        return 0;
    }

    /* Calculate the min of the window */
    ret = ring_buf_peek(&data->oasis_gattacker_data.adv_intervals.rb,
                        (uint8_t *)buf, RBUFF_SIZE);
    //printk("ring_buf_peek = %u\n", ret);
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

    printk("[*] min_interval: %u ms - ", min_interval);
    if (min_interval < MAX_ADV_DELAY) {
        printk("buf: ");
        for (int i=0; i < (RBUFF_SIZE/sizeof(uint32_t)); i++) {
            printk("%u ", buf[i]);
        }
        printk("\n");
    }

    /* Compute the detection threshold */
    if (data->oasis_gattacker_data.threshold == 0
        || min_interval >  MAX_ADV_DELAY) {

        data->oasis_gattacker_data.threshold = MAX_ADV_DELAY;
        /*
        if (min_interval > MAX_ADV_DELAY_CYCLES) {
            data->oasis_gattacker_data.threshold = min_interval - MAX_ADV_DELAY_CYCLES;
        } else {
            data->oasis_gattacker_data.threshold = MAX_ADV_DELAY_CYCLES;
        }
        */
        //printk("Set threshold to %u\n", data->oasis_gattacker_data.threshold);
        printk("[>>>] %02x:%02x:%02x:%02x:%02x:%02x - set threshold to %u ms\n",
            metric->adv_ind.addr[5],
            metric->adv_ind.addr[4],
            metric->adv_ind.addr[3],
            metric->adv_ind.addr[2],
            metric->adv_ind.addr[1],
            metric->adv_ind.addr[0],
            data->oasis_gattacker_data.threshold);
    }

    return 1;
}

static void do_detection(uint64_t address, struct scan_data *data,
                         struct srcc_scan_metric *metric, uint32_t adv_interval)
{
  if (adv_interval < data->oasis_gattacker_data.threshold) {
      data->oasis_gattacker_data.suspicious++;

      if (data->oasis_gattacker_data.suspicious > 50) {
          srcc_alert(GATTACKER, "Suspicious activity detected from %s", address);
      }

  /* Reset the suspicious activity counter */
  } else if (data->oasis_gattacker_data.suspicious) {
      data->oasis_gattacker_data.suspicious = 0;
  }
}


void srcc_detect_oasis_gattacker(uint64_t address, struct scan_data *data,
                                 struct srcc_scan_metric *metric)
{
    uint32_t adv_interval;

    //__ASSERT(0, "To implement");
    //printk("[SIROCCO] Oasis GATTacker module\n");

    /* Look only ADV_IND */
    if (metric->type != 0b0000) {
        return;
    }

    /* Select the ADV channel
     * The GATTacker detection module works only on the same channel
     * because of the estimated ADV interval.
     */
    if (data->oasis_gattacker_data.last_channel != metric->channel) {
        data->oasis_gattacker_data.last_channel = metric->channel;
        data->oasis_gattacker_data.threshold = 0;
        return;
    }

    //adv_interval = srcc_timing_capture_ms();
    //printk("current timestamp %u ms\n", adv_interval);

    /* Calculare the interval between two ADV_IND in cycles */
    adv_interval = metric->timestamp - data->previous_adv_timestamp;
    //printk("[*] adv_interval %u - timestamp %u - previous %u\n", adv_interval, metric->timestamp, data->previous_adv_timestamp);
    /*
    */
    printk("[*] %02x:%02x:%02x:%02x:%02x:%02x - %u ms\n",
           metric->adv_ind.addr[5],
           metric->adv_ind.addr[4],
           metric->adv_ind.addr[3],
           metric->adv_ind.addr[2],
           metric->adv_ind.addr[1],
           metric->adv_ind.addr[0],
           adv_interval);

    /* Estimate the threshold */
    if (estimate_threshold(data, metric, adv_interval) >= 0) {

        /* Detect MitM */
        do_detection(address, data, metric, adv_interval);
    }

    /* Save the current metric if it is an ADV_IND */
    //memcpy(&data->previous_metric, metric, sizeof(struct srcc_scan_metric));
    data->previous_adv_timestamp = metric->timestamp;
}

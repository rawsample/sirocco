/*
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/hash_map.h>

#include<zephyr/bluetooth/sirocco.h>



LOG_MODULE_DECLARE(sirocco, CONFIG_BT_SRCC_LOG_LEVEL);


struct injectable_data {
    uint32_t previous_conn_timestamp;
};

SYS_HASHMAP_DEFINE_STATIC(injectable_hmap);


void srcc_detect_injectable(struct srcc_conn_metric *conn_metric)
{
    float conn_interval;
    float interval_ms;
    struct injectable_data *data = NULL;
    uint64_t access_addr = 0;
    uint64_t value;
    bool ret;


    memcpy(&access_addr, conn_metric->access_addr, sizeof(uint32_t));

    ret = sys_hashmap_get(&injectable_hmap, access_addr, &value);
    if (ret) {
        data = (struct injectable_data *)((uint32_t)value);

    } else {
        /* Initialize new structure */
        data = k_malloc(sizeof(struct injectable_data));
        if (data == NULL) {
            printk("Fail to allocate memory for injectable :(\n");
            return;
        }

        data->previous_conn_timestamp = 0;
        ret = sys_hashmap_insert(&injectable_hmap, access_addr, ((uint32_t)data) | 0ULL, NULL);
        if (!ret) {
            printk("Couldn't insert previous_timestamp in injectable_hmap\n");
            return;
        }
    }

    conn_interval = abs(conn_metric->timestamp - data->previous_conn_timestamp);
    interval_ms = conn_metric->interval * 1.25;
    LOG_DBG("(TSP) %d - (PREV TST) %d = %d.%06d <? (lll_interval) %d.%06d",
            conn_metric->timestamp, data->previous_conn_timestamp,
            (int)conn_interval, (int)((conn_interval - (int)conn_interval) * 1000000),
            (int)interval_ms, (int)((interval_ms - (int)interval_ms) * 1000000));

    /* The threshold is the one set during the connection negotiation. */
    if (conn_interval < conn_metric->interval) {
        srcc_alert(INJECTABLE, srcc_timing_capture_ms(), conn_metric->access_addr);
    }

    data->previous_conn_timestamp = conn_metric->timestamp;
}

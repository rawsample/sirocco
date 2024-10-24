/*
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/hash_map.h>

#include<zephyr/bluetooth/sirocco.h>



LOG_MODULE_DECLARE(sirocco, CONFIG_BT_SRCC_LOG_LEVEL);


struct injectable_data {
    uint32_t previous_conn_timestamp;
    uint32_t previous_conn_event_counter;
};

SYS_HASHMAP_DEFINE_STATIC(injectable_hmap);


void srcc_detect_injectable(struct srcc_conn_metric *conn_metric)
{
    float conn_interval;
    //float interval_ms;
    float interval_us;
    struct injectable_data *data = NULL;
    uint64_t access_addr = 0;
    uint64_t value;
    bool ret;


    memcpy(&access_addr, conn_metric->access_addr, sizeof(uint32_t));

    ret = sys_hashmap_get(&injectable_hmap, access_addr, &value);
    if (ret) {
        //data = (struct injectable_data *)((uint32_t)value);
        data = (struct injectable_data *)(value);

    } else {
        /* Initialize new structure */
        data = k_malloc(sizeof(struct injectable_data));
        if (data == NULL) {
            LOG_ERR("Fail to allocate memory for injectable :(");
            return;
        }

        data->previous_conn_timestamp = 0;
        data->previous_conn_event_counter = 0;
        //ret = sys_hashmap_insert(&injectable_hmap, access_addr, ((uint32_t)data) | 0ULL, NULL);
        ret = sys_hashmap_insert(&injectable_hmap, access_addr, data, NULL);
        if (!ret) {
            LOG_ERR("Couldn't insert previous_timestamp in injectable_hmap");
            return;
        }
    }

    if (data->previous_conn_event_counter != conn_metric->conn_event_number){
        conn_interval = abs(conn_metric->timestamp - data->previous_conn_timestamp);
        //interval_ms = conn_metric->interval * 1.25;
        interval_us = conn_metric->interval * 1250.;

        

        /* The threshold is the one set during the connection negotiation. */
        // if (conn_interval < conn_metric->interval) { // && conn_interval>1.) {
        if (conn_interval < interval_us) {
            LOG_DBG("INJECTABLE (TSP) %d - (PREV TST) %d = %d.%06d <? (lll_interval) %d.%06d",
                conn_metric->timestamp, data->previous_conn_timestamp,
                (int)conn_interval, (int)((conn_interval - (int)conn_interval) * 1000000),
                (int)interval_us, (int)((interval_us - (int)interval_us) * 1000000));
            printk("Injectable detected\n");
            srcc_alert(INJECTABLE, srcc_timing_capture_ms(), conn_metric->access_addr);
        }

        data->previous_conn_timestamp = conn_metric->timestamp;
        data->previous_conn_event_counter = conn_metric->conn_event_number;
    }
}

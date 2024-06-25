/*
 */
#include <zephyr/kernel.h>
#include <zephyr/sys/hash_map.h>

#include<zephyr/bluetooth/sirocco.h>



/* Number of wrong packets before raising an alert */
#define BTLEJACK_THRESHOLD 3

struct btlejack_data {
  uint32_t invalid_counter;
};

SYS_HASHMAP_DEFINE_STATIC(btlejack_hmap);


static void do_detection(struct btlejack_data *data, struct srcc_conn_metric *conn_metric)
{
    if (!conn_metric->crc_is_valid || conn_metric->packet_lost_counter > 0) {
        data->invalid_counter++;
    }

    if (data->invalid_counter > BTLEJACK_THRESHOLD) {
        srcc_alert(BTLEJACK, "%02X:%02X:%02X:%02X",
                   conn_metric->access_addr[3], conn_metric->access_addr[2],
                   conn_metric->access_addr[1], conn_metric->access_addr[0]);
    }

    if (conn_metric->crc_is_valid || conn_metric->packet_lost_counter == 0) {
        data->invalid_counter = 0;
    }
}


/* */
void srcc_detect_btlejack(struct srcc_conn_metric *conn_metric)
{
    struct btlejack_data *data = NULL;
    uint64_t value;
    uint64_t access_addr = 0;
    bool ret;

    memcpy(&access_addr, conn_metric->access_addr, sizeof(uint32_t));

    ret = sys_hashmap_get(&btlejack_hmap, access_addr, &value);
    if (!ret) {

        /* Initialize new counter */
        data = k_malloc(sizeof(struct btlejack_data));
        if (data == NULL) {
            printk("Fail to allocate memory for btlejack :(\n");
            return;
        }

        data->invalid_counter = 0;
        ret = sys_hashmap_insert(&btlejack_hmap, access_addr, ((uint32_t)data) | 0ULL, NULL);
        if (!ret) {
            printk("Couldn't insert counter in btlejack_hmap\n");
            return;
        }
    }
    data = (struct btlejack_data *)((uint32_t)value);

    do_detection(data, conn_metric);
}

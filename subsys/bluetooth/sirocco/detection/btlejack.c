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


static void do_detection(struct btlejack_data *data, struct srcc_metric *metric)
{
    if (!(metric->type && 0x1)) {
        /* only RX packets*/
        if (!metric->pkt.crc_is_valid || metric->conn.packet_lost_counter > 0) {
            data->invalid_counter++;
        }
    }

    if (data->invalid_counter > BTLEJACK_THRESHOLD) {
        srcc_alert(BTLEJACK, "%x", (uint32_t) metric->conn.access_address);
    }


    if (!(metric->type && 0x1)) {
        /* only RX packets*/
        if (metric->pkt.crc_is_valid || metric->conn.packet_lost_counter == 0) {
            data->invalid_counter = 0;
        }
    }
}

static void central_detection(struct srcc_metric *metric)
{
    struct btlejack_data *data = NULL;
    uint64_t access_addr = 0;
    bool ret;

    memcpy(&access_addr, metric->conn.access_address, sizeof(uint32_t));

    ret = sys_hashmap_get(&btlejack_hmap, access_addr, data);
    if (!ret) {

        /* Initialize new counter */
        data = k_malloc(sizeof(struct btlejack_data));
        if (data == NULL) {
            printk("Fail to allocate memory for btlejack :(\n");
            return;
        }

        data->invalid_counter = 0;

        ret = sys_hashmap_insert(&btlejack_hmap, access_addr, data, NULL);
        if (!ret) {
            printk("Couldn't insert counter in btlejack_hmap\n");
            return;
        }
    }

    do_detection(data, metric);
}


/* */
void srcc_detect_btlejack(struct srcc_metric *metric)
{
    printk("[SIROCCO] BTLEJack module\n");

    switch (metric->type) {
        case CONN_RX:
        case CONN_TX:
            /* continue */
            break;

        case SCAN_RX:
        case SCAN_TX:
        default:
            /* nothing to do on these types of events */
            return;
    }

    switch (metric->local_dev.gap_role) {
        case CENTRAL:
            central_detection(metric);
            break;

        case PERIPHERAL:
        case ADVERTISER:
        case SCANNER:
        default:
            __ASSERT(0, "BTLEJack detection module not implemented for peripheral, advertiser and scanner");
    }
}

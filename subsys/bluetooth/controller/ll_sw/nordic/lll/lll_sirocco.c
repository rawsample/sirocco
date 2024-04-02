/*
 */
#include "hal/cpu.h"
#include "hal/ccm.h"

#include "util/util.h"
#include "util/memq.h"
#include "util/dbuf.h"

#include "pdu_df.h"
#include "pdu_vendor.h"
#include "pdu.h"

#include "lll.h"
#include "lll_df_types.h"
#include "lll_conn.h"

#include "lll_sirocco.h"

#include <zephyr/bluetooth/sirocco.h>



/* 
 * Careful, if time spent in ISR is too much,
 * an assert fault is triggered on lll_conn.c:354
 */
void lll_srcc_conn_rx(uint8_t crc_ok, uint32_t rssi_value,
    struct lll_conn *lll, struct pdu_data *pdu_data)
{
    struct srcc_ble_conn *conn;
    struct srcc_ble_pkt *pkt;
    struct metric_item *item;
    
    /* XXX: Doing a malloc here for each RX packet may not be the best... */
    item = k_malloc(sizeof(struct metric_item));
    if (item == NULL) {
        printk("Failed to allocate memory from the heap for item :(\n");
        return;
    }
    memset(item, 0, sizeof(struct metric_item));

    conn = &(item->metric.conn);
    memcpy(conn->access_address, &lll->access_addr, 4*sizeof(uint8_t));
    memcpy(conn->crc_init, &lll->crc_init, 4*sizeof(uint8_t));
    memcpy(conn->channel_map, &lll->data_chan_map, 5*sizeof(uint8_t));
    conn->hop_interval = lll->interval;
    conn->rx_counter += 1;
    conn->packet_lost_counter = 0;  // Reset packet lost counter

    pkt = &(item->metric.pkt);
    pkt->timestamp = k_cycle_get_32();
    pkt->crc_is_valid = crc_ok;
    pkt->rssi = rssi_value;
    pkt->len = (pdu_data->len < 255) ? pdu_data->len : 255;
    pkt->header.ll_id = pdu_data->ll_id;
    pkt->header.nesn = pdu_data->nesn;
    pkt->header.sn = pdu_data->sn;
    pkt->header.md = pdu_data->md;
    pkt->header.rfu = pdu_data->rfu;
    memcpy(pkt->payload, pdu_data->lldata, pkt->len);

    /* Callbacks */
    srcc_notify_conn_rx(item);
}

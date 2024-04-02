/*
 */
#include "hal/cpu.h"
#include "hal/ccm.h"
#include "hal/radio.h"

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



static void copy_pdu_data_to_pkt(struct srcc_ble_pkt *pkt, struct pdu_data *pdu_data)
{
    pkt->len = (pdu_data->len < 255) ? pdu_data->len : 255;
    pkt->header.ll_id = pdu_data->ll_id;
    pkt->header.nesn = pdu_data->nesn;
    pkt->header.sn = pdu_data->sn;
    pkt->header.md = pdu_data->md;
    pkt->header.rfu = pdu_data->rfu;
    memcpy(pkt->payload, pdu_data->lldata, pkt->len);
}

static void copy_lll_to_conn(struct srcc_ble_conn *conn, struct lll_conn *lll)
{
    memcpy(conn->access_address, &lll->access_addr, 4*sizeof(uint8_t));
    memcpy(conn->crc_init, &lll->crc_init, 4*sizeof(uint8_t));
    memcpy(conn->channel_map, &lll->data_chan_map, 5*sizeof(uint8_t));
    conn->hop_interval = lll->interval;
}


/* 
 * Careful, if time spent in ISR is too much,
 * an assert fault is triggered on lll_conn.c:354
 */
void lll_srcc_conn_rx(struct lll_conn *lll, uint8_t crc_ok, uint32_t rssi_value)
{
    struct srcc_ble_conn *conn;
    struct srcc_ble_pkt *pkt;
    struct metric_item *item;
    struct pdu_data *pdu_data_rx;
    uint32_t timestamp;

    timestamp = k_cycle_get_32();
    
    /* XXX: Doing a malloc here for each RX packet may not be the best... */
    item = k_malloc(sizeof(struct metric_item));
    if (item == NULL) {
        printk("Failed to allocate memory from the heap for item :(\n");
        return;
    }
    memset(item, 0, sizeof(struct metric_item));

    pdu_data_rx = radio_pkt_get();

    conn = &(item->metric.conn);
    copy_lll_to_conn(conn, lll);
    conn->rx_counter += 1;
    conn->packet_lost_counter = 0;  // Reset packet lost counter

    pkt = &(item->metric.pkt);
    pkt->timestamp = timestamp;
    copy_pdu_data_to_pkt(pkt, pdu_data_rx);
    pkt->crc_is_valid = crc_ok;
    pkt->rssi = rssi_value;

    /* Callbacks */
    srcc_notify_conn_rx(item);
}

void lll_srcc_conn_tx(struct lll_conn *lll)
{
    struct srcc_ble_conn *conn;
    struct srcc_ble_pkt *pkt;
    struct metric_item *item;
    struct pdu_data *pdu_data_tx;
    uint32_t timestamp;

    timestamp = k_cycle_get_32();

    item = k_malloc(sizeof(struct metric_item));
    if (item == NULL) {
        printk("Failed to allocate memory from the heap for item :(\n");
        return;
    }
    memset(item, 0, sizeof(struct metric_item));

    pdu_data_tx = radio_pkt_get();

    conn = &(item->metric.conn);
    copy_lll_to_conn(conn, lll);
    conn->tx_counter += 1;
    conn->packet_lost_counter = 0;

    pkt = &(item->metric.pkt);
    pkt->timestamp = timestamp;
    copy_pdu_data_to_pkt(pkt, pdu_data_tx);
    pkt->crc_is_valid = 0;
    pkt->rssi = radio_rssi_get();

    /* Callbacks */
    srcc_notify_conn_tx(item);
}
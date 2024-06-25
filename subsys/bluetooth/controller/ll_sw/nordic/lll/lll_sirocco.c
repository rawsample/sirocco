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
#include "lll_scan.h"
#include "lll_conn.h"
#include "lll_adv_types.h"
#include "lll_adv.h"

#include "lll_sirocco.h"

#include <zephyr/bluetooth/sirocco.h>



static uint32_t scan_rx_count = 0;



/* 
 * Careful, spending too much time in ISR results in an assert fault
 * triggered on lll_conn.c:354
 */
void lll_srcc_conn_rx(struct lll_conn *lll, uint8_t crc_ok, uint32_t rssi_value)
{
    struct srcc_conn_item *item;
    struct pdu_data *pdu_data;
    uint32_t timestamp;

    //printk("SCAN RX COUNT = %d\n", scan_rx_count);
    //printk("metric_item size = %d bytes", sizeof(struct metric_item));

    timestamp = srcc_timing_capture_ms();

    pdu_data = radio_pkt_get();

    item = srcc_malloc_conn_item();
    if (item == NULL) {
        printk("Failed to allocate memory from the heap for conn rx item :(\n");
        return;
    }

    item->metric.timestamp = timestamp;
    item->metric.rssi = radio_rssi_get();
    item->metric.crc_is_valid = crc_ok;
#if defined(CONFIG_BT_SRCC_BTLEJACK)
    item->metric.packet_lost_counter = 0;
#endif
    memcpy(&item->metric.access_addr, &lll->access_addr, 4*sizeof(uint8_t));
    item->metric.len = pdu_data->len;
    item->metric.ll_id = pdu_data->ll_id;

#if defined(CONFIG_BT_SRCC_KNOB)
    item->metric.payload = k_malloc(pdu_data->len);
    if (item != NULL) {
        memcpy(item->metric.payload, pdu_data->lldata, pdu_data->len);
    } else {
        printk("Failed to allocate memory for pdu_data\n");
    }
#else
    item->metric.payload = NULL;
#endif

    /* Callbacks */
    srcc_notify_conn_rx(item);
}

void lll_srcc_conn_tx(struct lll_conn *lll)
{
    struct srcc_conn_item *item;
    struct pdu_data *pdu_data;
    uint32_t timestamp;

    timestamp = srcc_timing_capture_ms();

    pdu_data = radio_pkt_get();

    item = srcc_malloc_conn_item();
    if (item == NULL) {
        printk("Failed to allocate memory from the heap for conn tx item :(\n");
        return;
    }

    item->metric.timestamp = timestamp;
    item->metric.rssi = radio_rssi_get();
    item->metric.crc_is_valid = 0xFF;
#if defined(CONFIG_BT_SRCC_BTLEJACK)
    item->metric.packet_lost_counter++;
#endif
    memcpy(&item->metric.access_addr, &lll->access_addr, 4*sizeof(uint8_t));
    item->metric.len = pdu_data->len;
    item->metric.ll_id = pdu_data->ll_id;
    item->metric.payload = NULL;

    /* Callbacks */
    srcc_notify_conn_tx(item);
}

void lll_srcc_scan_rx(struct lll_scan *lll, uint8_t crc_ok, uint32_t rssi_value)
{
    struct srcc_scan_item *item;
    struct pdu_adv *pdu_adv;
    uint32_t timestamp;

    //printk("SCAN RX\n");
    scan_rx_count++;
    //return;

    timestamp = srcc_timing_capture_ms();

    /* Check if it is one of the type we monitor */
    pdu_adv = radio_pkt_get();

    /* Select only SCAN packets */
    switch (pdu_adv->type) {
        case 0b0000:    /* ADV_IND */
        case 0b0001:    /* ADV_DIRECT_IND */
        case 0b0011:    /* SCAN_REQ */
        case 0b0100:    /* SCAN_RESP */
            /* continue */
            break;

        default:
            /* */
            return;
    }

    item = srcc_malloc_scan_item();
    if (item == NULL) {
        printk("Failed to allocate memory from the heap for scan rx item :(\n");
        return;
    }

#if defined(CONFIG_BT_CENTRAL)
    memcpy(&item->metric.adv_addr, lll->adv_addr, BDADDR_SIZE * sizeof(uint8_t));
#endif
    item->metric.timestamp = timestamp;
    item->metric.rssi = radio_rssi_get();
    item->metric.channel = lll->chan;
    item->metric.crc_is_valid = crc_ok;
    item->metric.type = pdu_adv->type;
    item->metric.len = pdu_adv->len;
    item->metric.interval = lll->interval;
    item->metric.ticks_window = lll->ticks_window;

    switch (pdu_adv->type) {
        case 0b0000:    /* ADV_IND */
            memcpy(&item->metric.adv_ind, &pdu_adv->adv_ind,
                   sizeof(struct pdu_adv_adv_ind));
            break;
        case 0b0001:    /* ADV_DIRECT_IND */
            memcpy(&item->metric.direct_ind, &pdu_adv->direct_ind,
                   sizeof(struct pdu_adv_adv_ind));
            break;
        case 0b0011:    /* SCAN_REQ */
            memcpy(&item->metric.scan_req, &pdu_adv->scan_req,
                   sizeof(struct pdu_adv_scan_req));
            break;
        case 0b0100:    /* SCAN_RESP */
            memcpy(&item->metric.scan_rsp, &pdu_adv->scan_rsp,
                   sizeof(struct pdu_adv_scan_rsp));
            break;
        default:
            break;
    }

    /* Callbacks */
    srcc_notify_scan_rx(item);
}


void lll_srcc_adv_rx(struct lll_adv *lll, uint8_t crc_ok, uint32_t rssi_value)
{
    struct srcc_adv_item *item;
    struct pdu_adv *pdu_adv;
    uint32_t timestamp;

    timestamp = srcc_timing_capture_ms();

    /* Check if it is one of the type we monitor */
    pdu_adv = radio_pkt_get();

    //printk("ADV RX: 0x%x\n", pdu_adv->type);

    /* Note: We do not consider secondary advertising physical channel here. */
    switch (pdu_adv->type) {
        case 0b0101:    /* CONNECT_IND */
            /* continue */
            break;

        default:
            /* */
            return;
    }

    item = srcc_malloc_adv_item();
    if (item == NULL) {
        printk("Failed to allocate memory from the heap for adv rx item :(\n");
        return;
    }

    item->metric.timestamp = timestamp;
    item->metric.rssi = radio_rssi_get();
    item->metric.crc_is_valid = crc_ok;
    item->metric.type = pdu_adv->type;
    item->metric.len = pdu_adv->len;

    switch (pdu_adv->type) {
        case 0b0101:    /* CONNECT_IND */
            memcpy(&item->metric.connect_ind, &pdu_adv->connect_ind,
                   sizeof(struct pdu_adv_connect_ind));
        default:
            break;
    }

    /* Callbacks */
    srcc_notify_adv_rx(item);
}

void lll_srcc_adv_tx(struct lll_adv *lll)
{
    /* ADV_IND are sent too quickly for the FIFO to drain. */
    return;

    /* Dead code: */
    struct srcc_adv_item *item;
    struct pdu_adv *pdu_adv;
    uint32_t timestamp;

    timestamp = srcc_timing_capture_ms();


    /* Check if it is one of the type we monitor */
    pdu_adv = radio_pkt_get();

    /* Note: We do not consider secondary advertising physical channel here. */
    switch (pdu_adv->type) {
        case 0b0000:    /* ADV_IND */
        case 0b0001:    /* ADV_DIRECT_IND */
            /* continue */
            break;

        default:
            /* */
            return;
    }

    item = srcc_malloc_adv_item();
    if (item == NULL) {
        printk("Failed to allocate memory from the heap for adv tx item :(\n");
        return;
    }

    item->metric.timestamp = timestamp;
    item->metric.rssi = radio_rssi_get();
    item->metric.crc_is_valid = 0;
    item->metric.type = pdu_adv->type;
    item->metric.len = pdu_adv->len;

    switch (pdu_adv->type) {
        case 0b0000:    /* ADV_IND */
            memcpy(&item->metric.adv_ind, &pdu_adv->adv_ind,
                   sizeof(struct pdu_adv_adv_ind));
            break;
        case 0b0001:    /* ADV_DIRECT_IND */
            memcpy(&item->metric.direct_ind, &pdu_adv->direct_ind,
                   sizeof(struct pdu_adv_adv_ind));
            break;

        default:
            break;
    }

    /* Callbacks */
    srcc_notify_adv_tx(item);
}
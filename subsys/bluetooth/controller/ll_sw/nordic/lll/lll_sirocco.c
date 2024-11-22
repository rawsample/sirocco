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

#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/sirocco.h>


LOG_MODULE_DECLARE(sirocco, CONFIG_BT_SRCC_LOG_LEVEL);

void log_pdu_adv_type(char *isr, uint8_t type);


/* 
 * Careful, spending too much time in ISR results in an assert fault
 * triggered on lll_conn.c:354
 */
void lll_srcc_conn_rx(struct lll_conn *lll, uint8_t crc_ok, uint32_t rssi_value)
{
    struct srcc_conn_item *item;
    struct pdu_data *pdu_data;
    uint32_t timestamp;

    timestamp = srcc_timing_capture_ms();

    pdu_data = radio_pkt_get();

    item = srcc_malloc_conn_item();
    if (item == NULL) {
        LOG_ERR("Failed to allocate memory from the heap for conn rx item :(");
        return;
    }

    item->metric.timestamp = timestamp;
    item->metric.rssi = radio_rssi_get();
    item->metric.crc_is_valid = crc_ok;
#if defined(CONFIG_BT_SRCC_INJECTABLE)
    item->metric.interval = lll->interval;
#endif
#if defined(CONFIG_BT_SRCC_BTLEJACK)
    item->metric.packet_lost_counter = 0;
#endif
    memcpy(&item->metric.access_addr, &lll->access_addr, 4*sizeof(uint8_t));
    item->metric.len = pdu_data->len;
    item->metric.ll_id = pdu_data->ll_id;

#if defined(CONFIG_BT_SRCC_KNOB)
    item->metric.payload = k_malloc(pdu_data->len);
    if (item->metric.payload != NULL) {
        memcpy(item->metric.payload, pdu_data->lldata, pdu_data->len);
    } else {
        LOG_ERR("Failed to allocate memory for pdu_data");
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
        LOG_ERR("Failed to allocate memory from the heap for conn tx item :(");
        return;
    }

    item->metric.timestamp = timestamp;
    item->metric.rssi = radio_rssi_get();
    item->metric.crc_is_valid = 0xFF;
#if defined(CONFIG_BT_SRCC_INJECTABLE)
    item->metric.interval = lll->interval;
#endif
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

    timestamp = srcc_timing_capture_ms();

    /* Check if it is one of the type we monitor */
    pdu_adv = radio_pkt_get();
    //log_pdu_adv_type("scan_rx\0", pdu_adv->type);

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
        LOG_ERR("Failed to allocate memory from the heap for scan rx item :(");
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
    //log_pdu_adv_type("adv_rx\0", pdu_adv->type);

    //LOG_DBG("ADV RX: 0x%x", pdu_adv->type);

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
        LOG_ERR("Failed to allocate memory from the heap for adv rx item :(");
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
    //log_pdu_adv_type("adv_tx\0", pdu_adv->type);

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
        LOG_ERR("Failed to allocate memory from the heap for adv tx item :(");
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





void log_pdu_adv_type(char *isr, uint8_t type)
{
    char name[16];

    switch (type) {
        case 0b0000:    /* ADV_IND */
            memcpy(name, "ADV_IND\0", 8);
            break;
        case 0b0001:    /* ADV_DIRECT_IND */
            memcpy(name, "ADV_DIRECT_IND\0", 14);
            break;
        case 0b0010:    /* ADV_NONCONN_IND */
            memcpy(name, "ADV_NONCONN_IND\0", 16);
            break;
        case 0b0011:    /* SCAN_REQ & AUX_SCAN_REQ */
            memcpy(name, "SCAN_REQ\0", 9);
            break;
        case 0b0100:    /* SCAN_RSP */
            memcpy(name, "SCAN_RSP\0", 9);
            break;
        case 0b0101:    /* CONNECT_IND & AUX_CONNECT_IND */
            memcpy(name, "CONNECT_IND\0", 12);
            break;
        case 0b0110:    /* ADV_SCAN_IND */
            memcpy(name, "ADV_SCAN_IND\0", 13);
            break;
        case 0b0111:    /* ADV_EXT_IND & ... */
            memcpy(name, "ADV_EXT_IND\0", 12);
            break;
        case 0b1000:    /* AUX_CONNECT_RSP */
            memcpy(name, "AUX_CONNECT_RSP\0", 16);
            break;
        default:
            memcpy(name, "N/A\0", 4);
            break;
    }

    LOG_INF("%s: %s", isr, name);
}
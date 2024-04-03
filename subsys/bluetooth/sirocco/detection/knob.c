/*
 *
 */
#include <zephyr/bluetooth/sirocco.h>


static void do_detection(struct srcc_metric *metric)
{
    struct srcc_ble_pkt *pkt;

    pkt = &metric->pkt;

    if (
        pkt->len >= 11 &&
        pkt->header.ll_id == 0x2 &&     // is a L2CAP message
        pkt->payload[2] == 0x06 && pkt->payload[3] == 0x00 &&   // CID = 6 <=> Security Manager
        pkt->payload[4] == 0x01 &&      // is a pariing request
        pkt->payload[8] <= 10
    ) {
        srcc_alert(KNOB, "0x%02x", pkt->payload[8]);
    }
}

/* */
void srcc_detect_knob(struct srcc_metric *metric)
{
    printk("[SIROCCO] KNOB module\n");

    switch(metric->type) {
        case CONN_RX:
            do_detection(metric);
            break;

        case CONN_TX:
        case SCAN_RX:
        case SCAN_TX:
        default:
            /* nothing to do on these types of events */
    }
}

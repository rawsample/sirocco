/*
 *
 */
#include <zephyr/bluetooth/sirocco.h>


/* */
void srcc_detect_knob(struct srcc_conn_metric *conn_metric)
{
    /* If the packet is a L2CAP message */
    if (conn_metric->len >= 11 && conn_metric->ll_id == 0x2) {
        /* If we successfully copied its content */
        if (conn_metric->payload != NULL) {
            /* If */
            if (
                conn_metric->payload[2] == 0x06 && conn_metric->payload[3] == 0x00 && // CID = 6 <=> Security Manager
                conn_metric->payload[4] == 0x01 && // is a pairing request
                conn_metric->payload[8] <= 10
               ) {
                srcc_alert(KNOB, "%02X:%02X:%02X:%02X",
                        conn_metric->access_addr[3], conn_metric->access_addr[2],
                        conn_metric->access_addr[1], conn_metric->access_addr[0]);
            }
        }
    }
}

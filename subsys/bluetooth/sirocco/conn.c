/*
 *
 */
#include <zephyr/bluetooth/sirocco.h>



void printk_conn_metric(struct srcc_conn_metric *conn_metric)
{
    printk("[SIROCCO] CONN: %u %02x:%02x:%02x:%02x 0x%x %hu %hu\n",
           conn_metric->timestamp,
           conn_metric->access_addr[0],
           conn_metric->access_addr[1],
           conn_metric->access_addr[2],
           conn_metric->access_addr[3],
           conn_metric->len,
           conn_metric->crc_is_valid,
           conn_metric->rssi
    );
}



void run_conn_tx_detection(struct srcc_conn_metric *conn_metric)
{
    // For now, only print something
    //printk_conn_metric(conn_metric);

    /* Call detection modules here */
}

void run_conn_rx_detection(struct srcc_conn_metric *conn_metric)
{
    // For now, only print something
    //printk_conn_metric(conn_metric);

    /* Call detection modules here */
#if defined(CONFIG_BT_SRCC_BTLEJACK)
    srcc_detect_btlejack(conn_metric);
#endif /* CONFIG_BT_SRCC_BTLEJACK */

#if defined(CONFIG_BT_SRCC_INJECTABLE)
    srcc_detect_injectable(conn_metric);
#endif /* CONFIG_BT_SRCC_INJECTABLE */

#if defined(CONFIG_BT_SRCC_KNOB)
    srcc_detect_knob(conn_metric);
#endif /* CONFIG_BT_SRCC_KNOB */

#if defined(CONFIG_SRCC_E2E_LATENCY)
    srcc_e2e_conn_rx(conn_metric->e2e_start_cycles);
#endif /* CONFIG_SRCC_E2E_LATENCY */
}

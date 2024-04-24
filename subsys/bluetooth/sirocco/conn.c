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



void run_conn_detection(struct srcc_conn_metric *conn_metric)
{
    // For now, only print something
    printk_conn_metric(conn_metric);
}
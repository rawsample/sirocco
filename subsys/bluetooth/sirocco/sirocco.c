/*
 * The Sirocco Bluetooth Intrusion Detection System (IDS).
 */

#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>

#include <zephyr/bluetooth/sirocco.h>




K_FIFO_DEFINE(rx_fifo);
//K_FIFO_DEFINE(tx_fifo);
//K_FIFO_DEFINE(scan_fifo);

//struct metric_item rx_metric;



/* Main thread running detection algorithms
 ****************************************************************************/

#define SRCC_THREAD_STACK_SIZE 1024
#define SRCC_THREAD_PRIORITY 5

static void printk_conn(struct srcc_ble_conn *conn)
{
    printk("struct src_ble_conn = { "
           "\n\taccess_address = %02x:%02x:%02x:%02x, "
           "\n\tcrc_init = %02x:%02x:%02x, "
           "\n\tchannel_map = %02x %02x %02x %02x %02x, "
           "\n\thop_interval = %u, "
           "\n\tpacket_lost_count = %u, "
           "\n\ttx_counter = %u, "
           "\n\trx_counter = %u, "
           "\n}\n",
           conn->access_address[0], conn->access_address[1],
           conn->access_address[2], conn->access_address[3],
           conn->crc_init[0], conn->crc_init[1], conn->crc_init[2],
           conn->channel_map[0], conn->channel_map[1],
           conn->channel_map[2], conn->channel_map[3],
           conn->channel_map[4],
           conn->hop_interval,
           conn->packet_lost_counter,
           conn->tx_counter, conn->rx_counter
    );
}

static void printk_pkt(struct srcc_ble_pkt *pkt)
{
    printk("struct srcc_ble_packet = { "
           "\n\ttimestamp = %u, "
           "\n\tcrc_is_valid = %hu, "
           "\n\trssi = %hu, "
           "\n\tlen = %hhu, "
           "\n\theader = { "
           "\n\t\tlld_id = 0x%x, "
           "\n\t\tnesn = 0x%x, "
           "\n\t\tsn = 0x%x, "
           "\n\t\tmd = 0x%x, "
           "\n\t\trfu = 0x%x, "
           "\n\t}, "
           "\n\tpayload = ",
           pkt->timestamp, pkt->crc_is_valid, pkt->rssi, pkt->len, 
           pkt->header.ll_id & 0x3,
           pkt->header.nesn & 0x1,
           pkt->header.sn & 0x1,
           pkt->header.md & 0x1,
           pkt->header.rfu & 0x7
    );
    for (int i=0; i<pkt->len && i<255; i++) {
        printk("%02x ", pkt->payload[i]);
    }
    printk("\n}\n");
}

/* Main loop */
static void sirocco_main_loop(void *, void *, void *)
{
    struct metric_item *item;
	struct srcc_metric *metric;

	while(1) {
		//printk("[SIROCCO] main loop\n");

        item = k_fifo_get(&rx_fifo, K_FOREVER);
        metric = &item->metric;
        //printk("metric @ %08x\n", metric);

        /*
        printk_conn(&metric->conn);
        printk_pkt(&metric->pkt);
        */

        if (metric->pkt.len != 0 ) {
            printk_conn(&metric->conn);
            printk_pkt(&metric->pkt);
            printk("[SIROCCO] New Packet len = %d ", metric->pkt.len);
            printk("payload: ");
            for (int i=0; i<metric->pkt.len && i<255; i++) {
                printk("%02x", metric->pkt.payload[i]);
            }
            printk("\n");
        }

        k_free(item);

		//k_sleep(K_SECONDS(1));
        //k_sleep(K_MSEC(1000));
	}
}

K_THREAD_DEFINE(sirocco_id,
				SRCC_THREAD_STACK_SIZE,
				sirocco_main_loop,
				NULL,
				NULL,
				NULL,
				SRCC_THREAD_PRIORITY,
				0,
				0);


/* Callback mechanism 
 ****************************************************************************/

static struct srcc_cb *callback_list;


void srcc_cb_register(struct srcc_cb *cb)
{
	cb->_next = callback_list;
	callback_list = cb;
}

/*
void srcc_notify_conn_init(void)
{
    return;
}
*/

void srcc_notify_conn_rx(struct metric_item *item)
{
    //printk("[ISR RX] sirocco\n");

    k_fifo_put(&rx_fifo, item);

    /* Callbacks
    for (struct srcc_cb *cb = callback_list; cb; cb = cb->_next) {
        if(cb->conn_rx) {
            cb->conn_rx(metric);
        }
    }
    */
}

/*
void srcc_notify_conn_tx(void)
{
    // TODO: Gather the metrics

    for (struct srcc_cb *cb = callback_list; cb; cb = cb->_next) {
        if(cb->conn_tx) {
            cb->conn_tx();
        }
    }
}

void srcc_notify_scan_rx(void)
{
    return;
}
*/


/*
 ****************************************************************************/

void srcc_init(void)
{
    return;    
}

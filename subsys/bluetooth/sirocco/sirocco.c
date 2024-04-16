/*
 * The Sirocco Bluetooth Intrusion Detection System (IDS).
 */

#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>

#include <zephyr/bluetooth/sirocco.h>




K_FIFO_DEFINE(conn_rx_fifo);
K_FIFO_DEFINE(conn_tx_fifo);
K_FIFO_DEFINE(scan_rx_fifo);

struct k_poll_event events[3] = {
    K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_FIFO_DATA_AVAILABLE,
                                    K_POLL_MODE_NOTIFY_ONLY,
                                    &conn_rx_fifo, 0),
    K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_FIFO_DATA_AVAILABLE,
                                    K_POLL_MODE_NOTIFY_ONLY,
                                    &conn_tx_fifo, 0),
    K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_FIFO_DATA_AVAILABLE,
                                    K_POLL_MODE_NOTIFY_ONLY,
                                    &scan_rx_fifo, 0),
};


/* Main thread running detection algorithms
 ****************************************************************************/

#define SRCC_THREAD_STACK_SIZE 1024
#define SRCC_THREAD_PRIORITY 5

/*
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
*/
void printk_conn_metric(struct srcc_conn_metric *conn_metric)
{
    printk("[SIROCCO] CONN: %d %02x:%02x:%02x:%02x 0x%x %hu %hu\n",
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

void printk_scan_metric(struct srcc_scan_metric *scan_metric)
{
    printk("[SIROCCO] SCAN RX: %d %02x:%02x:%02x:%02x:%02x:%02x 0x%x 0x%x %hu %hu %hu %d\n",
           scan_metric->timestamp,
           scan_metric->adv_addr[5],
           scan_metric->adv_addr[4],
           scan_metric->adv_addr[3],
           scan_metric->adv_addr[2],
           scan_metric->adv_addr[1],
           scan_metric->adv_addr[0],
           scan_metric->type,
           scan_metric->len,
           scan_metric->crc_is_valid,
           scan_metric->rssi,
           scan_metric->interval,
           scan_metric->ticks_window
    );
}

/*
static void run_detection_modules(struct srcc_metric *metric)
{
#if defined(CONFIG_BT_SRCC_BTLEJACK)
    srcc_detect_btlejack(metric);
#endif
#if defined(CONFIG_BT_SRCC_BTLEJUICE)
    srcc_detect_btlejuice(metric);
#endif
#if defined(CONFIG_BT_SRCC_GATTACKER)
    srcc_detect_gattacker(metric);
#endif
#if defined(CONFIG_BT_SRCC_INJECTABLE)
    srcc_detect_injectable(metric);
#endif
#if defined(CONFIG_BT_SRCC_JAMMING)
    srcc_detect_jamming(metric);
#endif
#if defined(CONFIG_BT_SRCC_KNOB)
    srcc_detect_knob(metric);
#endif
}
*/

static void run_conn_detection(struct srcc_conn_metric *conn_metric)
{
    // For now, only print something
    printk_conn_metric(conn_metric);
}

static void run_scan_detection(struct srcc_scan_metric *scan_metric)
{
    // For now, only print something
    printk_scan_metric(scan_metric);
}


/* Main loop */
static void sirocco_main_loop(void *, void *, void *)
{
    int ret;
    struct srcc_scan_item *scan_item;
    struct srcc_conn_item *conn_item;

    printk("[SIROCCO] sizeof(scan_metric) = %d bytes\n"
           "[SIROCCO] sizeof(conn_metric) = %d bytes\n",
           sizeof(struct srcc_scan_metric), sizeof(struct srcc_conn_metric));

	while(1) {
		//printk("[SIROCCO] main loop\n");

        ret = k_poll(events, 3, K_FOREVER);
        if (ret < 0) {
            printk("[SIROCCO] An error occured while polling fifo events: %d", ret);
        }

        if (events[0].state == K_POLL_STATE_FIFO_DATA_AVAILABLE) {
            conn_item = k_fifo_get(&conn_rx_fifo, K_FOREVER);
            run_conn_detection(&conn_item->metric);
            srcc_free_conn_item(conn_item);

        } else if (events[1].state == K_POLL_STATE_FIFO_DATA_AVAILABLE) {
            conn_item = k_fifo_get(&conn_tx_fifo, K_FOREVER);
            run_conn_detection(&conn_item->metric);
            srcc_free_conn_item(conn_item);

        } else if (events[2].state == K_POLL_STATE_FIFO_DATA_AVAILABLE) {
            scan_item = k_fifo_get(&scan_rx_fifo, K_FOREVER);
            run_scan_detection(&scan_item->metric);
            srcc_free_scan_item(scan_item);
        }

        events[0].state = K_POLL_STATE_NOT_READY;
        events[1].state = K_POLL_STATE_NOT_READY;
        events[2].state = K_POLL_STATE_NOT_READY;

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

void srcc_notify_conn_rx(struct srcc_conn_item *item)
{
    //printk("[RX ISR] sirocco\n");

    k_fifo_put(&conn_rx_fifo, item);

    /* Callbacks
    for (struct srcc_cb *cb = callback_list; cb; cb = cb->_next) {
        if(cb->conn_rx) {
            cb->conn_rx(item);
        }
    }
    */
}

void srcc_notify_conn_tx(struct srcc_conn_item *item)
{
    //printk("[TX ISR] sirocco\n");

    k_fifo_put(&conn_tx_fifo, item);

    /* Callbacks
    for (struct srcc_cb *cb = callback_list; cb; cb = cb->_next) {
        if(cb->conn_tx) {
            cb->conn_tx(item);
        }
    }
    */
}

void srcc_notify_scan_rx(struct srcc_scan_item *item)
{
    //printk("[TX ISR] sirocco\n");

    k_fifo_put(&scan_rx_fifo, item);

    /* Callbacks
    for (struct srcc_cb *cb = callback_list; cb; cb = cb->_next) {
        if(cb->scan_rx) {
            cb->scan_rx(item);
        }
    }
    */
}


/*
 ****************************************************************************/

void srcc_init(void)
{
    uint32_t ret;

    /* Create memory pools for passing metric to main thread */
    ret = srcc_init_conn_alloc(SRCC_MALLOC_COUNT_ITEMS);
    if (ret < 0) {
        return;
    }
    ret = srcc_init_scan_alloc(SRCC_MALLOC_COUNT_ITEMS);
    if (ret < 0) {
        return;
    }

    return;    
}

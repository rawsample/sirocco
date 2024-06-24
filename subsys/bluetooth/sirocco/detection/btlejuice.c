/*
 * Do not take into account if the peripheral connects to multiple centrals at the same time.
 */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/sirocco.h>



/* Modify these to adjust the scanning speed. */
#define BTLEJUICE_SCAN_INTERVAL    0x0010
#define BTLEJUICE_SCAN_WINDOW      0x0010

static bool is_scanning = false;


/* Called from scan messages */
void srcc_detect_btlejuice(struct srcc_scan_metric *scan_metric)
{
    return;

	bt_addr_le_t local_addrs[CONFIG_BT_ID_MAX];
    int count = 1;
    int res = 0;

    /* Get current local addresses */
	bt_id_get(local_addrs, &count);

    for (int i=0; i<count; i++) {
        res = memcmp(&local_addrs[i].a, scan_metric->adv_addr, BDADDR_SIZE*sizeof(uint8_t));
        if (res == 0) {
            printk(">>> [SIROCCO] BTLEJuice attack detected !!!\n");
            srcc_alert(BTLEJUICE, "%x", (uint32_t) scan_metric->adv_addr);
        }
    }
}

/* It is also possible to do the detection with the already existing callback mechanism 
static void sirocco_btlejuice_cb(const bt_addr_le_t *remote_addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
	char laddr_str[BT_ADDR_LE_STR_LEN];
	char raddr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_t local_addrs[CONFIG_BT_ID_MAX];
	size_t count = 1;
    int res = 0;

	bt_id_get(local_addrs, &count);

    for (int i=0; i<count; i++) {
        // Note: the conversion & comparison could be done more straight forward.
        bt_addr_le_to_str(&local_addrs[i], laddr_str, BT_ADDR_LE_STR_LEN);
        bt_addr_le_to_str(remote_addr, raddr_str, BT_ADDR_LE_STR_LEN);

        res = bt_addr_cmp(&remote_addr->a, &local_addrs[i].a);
        if (res == 0) {
            printk(">>> [SIROCCO] BTLEJuice attack detected !!!\n");
            //srcc_alert(BTLEJUICE, "%x", );
        }
    }
}
*/


/* Trigger on receiving a CONNECT_IND */
static void srcc_start_btlejuice_bg_scan(void)
{
    int err;
    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_PASSIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = BTLEJUICE_SCAN_INTERVAL,
        .window     = BTLEJUICE_SCAN_WINDOW,
    };

    /* Start scanning */
	err = bt_le_scan_start(&scan_param, NULL);
	//err = bt_le_scan_start(&scan_param, sirocco_btlejuice_cb);
	if (err) {
		printk("Starting scanning failed (err %d)\n", err);
		return;
	}

    is_scanning = true;
    printk("Sirocco BTLEJuice module started\n");

    return;
}

/* Trigger on starting advertising */
static void srcc_stop_btlejuice_bg_scan(void)
{
    int err;

    /* Stop scanning */
	err = bt_le_scan_stop();
	if (err) {
		printk("Stopping scanning failed (err %d)\n", err);
		return;
	}

    is_scanning = false;
    printk("Sirocco BTLEJuice module stopped\n");

    return;
}


/* Let's use the Zephyr bluetooth's stack callbacks to manage transitions with connections */
void srcc_btlejuice_conn_cb(struct bt_conn *conn, uint8_t err)
{
    if (err == 0) {
        srcc_start_btlejuice_bg_scan();
    } else {
        printk("BTLEJuice module not started (err %d)\n", err);
    }
}

void srcc_btlejuice_disconn_cb(struct bt_conn *conn, uint8_t reason)
{
    srcc_stop_btlejuice_bg_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = srcc_btlejuice_conn_cb,
	.disconnected = srcc_btlejuice_disconn_cb,
};

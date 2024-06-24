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


/* Trigger on receiving a CONNECT_IND */
void srcc_start_btlejuice_bg_scan(struct srcc_adv_metric *adv_metric)
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
	if (err) {
		printk("Starting scanning failed (err %d)\n", err);
		return;
	}

    is_scanning = true;
    printk("Sirocco BTLEJuice module started\n");

    return;
}

/* Trigger on starting advertising */
void srcc_stop_btlejuice_bg_scan(void)
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


BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = srcc_start_btlejuice_bg_scan,
	.disconnected = srcc_stop_btlejuice_bg_scan,
};

/*
 * Do not take into account if the peripheral connects to multiple centrals at the same time.
 */
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/sirocco.h>


LOG_MODULE_DECLARE(sirocco, CONFIG_BT_SRCC_LOG_LEVEL);


/* Modify these to adjust the scanning speed. */
#define BTLEJUICE_SCAN_INTERVAL    0x0010
#define BTLEJUICE_SCAN_WINDOW      0x0010

bool is_btlejuice_scanning = false;


static void srcc_btlejuice_cb(const bt_addr_le_t *remote_addr, int8_t rssi,
                                 uint8_t adv_type, struct net_buf_simple *buf)
{
	bt_addr_le_t local_addrs[CONFIG_BT_ID_MAX];
	size_t count = 1;
    int res = 0;

    /* Get current local addresses */
	bt_id_get(local_addrs, &count);

    for (int i=0; i<count; i++) {

        res = memcmp(&local_addrs[i].a.val, &remote_addr->a.val, BDADDR_SIZE*sizeof(uint8_t));
        if (res == 0) {
            LOG_DBG(">>> BTLEJuice attack detected !!!");
            srcc_alert(BTLEJUICE, srcc_timing_capture_ms(), (uint8_t *) remote_addr->a.val);
        }
    }
}


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
	err = bt_le_scan_start(&scan_param, srcc_btlejuice_cb);
	if (err) {
		LOG_ERR("Starting scanning failed (err %d)", err);
		return;
	}

    is_btlejuice_scanning = true;
    LOG_DBG("Sirocco BTLEJuice module started");

    return;
}

/* Trigger on starting advertising */
static void srcc_stop_btlejuice_bg_scan(void)
{
    int err;

    /* Stop scanning */
	err = bt_le_scan_stop();
	if (err) {
		LOG_ERR("Stopping scanning failed (err %d)", err);
		return;
	}

    is_btlejuice_scanning = false;
    LOG_DBG("Sirocco BTLEJuice module stopped");

    return;
}


/* Let's use the Zephyr bluetooth's stack callbacks to manage transitions with connections */
void srcc_btlejuice_conn_cb(struct bt_conn *conn, uint8_t err)
{
    if (err == 0) {
        srcc_start_btlejuice_bg_scan();
    } else {
        LOG_ERR("BTLEJuice module not started (err %d)", err);
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

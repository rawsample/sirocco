/*
 */
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/sirocco.h>


LOG_MODULE_DECLARE(sirocco, CONFIG_BT_SRCC_LOG_LEVEL);



K_MSGQ_DEFINE(alert_msgq, sizeof(struct alert_t), 32, 1);


/* Thread aggregating alerts
 *****************************************************************************/

#define SRCC_ALERT_THREAD_STACK_SIZE 1024
#define SRCC_ALERT_THREAD_PRIORITY 5

static void log_alert(struct alert_t *alert);
static void broadcast_alert(struct alert_t *alert);


/* Callback mechanism
 ****************************************************************************/
static struct srcc_alert_cb *callback_list;

void srcc_alert_register_cb(struct srcc_alert_cb *cb)
{
	cb->_next = callback_list;
	callback_list = cb;
}


/* Main loop */
static void sirocco_alert_loop(void *, void *, void *)
{
    struct alert_t alert;
    int ret;

    while (1) {
        ret = k_msgq_get(&alert_msgq, &alert, K_FOREVER);
        if (ret == 0) {
            /* Alerts can be logged, sent to the controller app via the callback mechanism,
               broadcasted as an advertisement, sent by HCI, etc. */
            log_alert(&alert);

            for (struct srcc_alert_cb *cb = callback_list; cb; cb = cb->_next) {
                if(cb->alert_cb) {
                    cb->alert_cb(&alert);
                }
            }

            broadcast_alert(&alert);

        } else if (ret == -ENOMSG) {
            // do nothing
        }
    }
}

K_THREAD_DEFINE(sirocco_alert_id,
				SRCC_ALERT_THREAD_STACK_SIZE,
				sirocco_alert_loop,
				NULL,
				NULL,
				NULL,
				SRCC_ALERT_THREAD_PRIORITY,
				0,
				0);


static void log_alert(struct alert_t *alert)
{
    char buffer[256];

    const char *prefix = "";
    switch (alert->nb) {
        case NO_ALERT:      prefix = " "; break;
        case BTLEJACK:      prefix = "BTLJack: "; break;
        case BTLEJUICE:     prefix = "BTLEJuice: "; break;
        case GATTACKER:     prefix = "GATTacker: "; break;
        case INJECTABLE:    prefix = "InjectaBLE: "; break;
        case JAMMING:       prefix = "Jamming: "; break;
        case KNOB:          prefix = "KNOB: "; break;
    }
    snprintf(buffer, sizeof(buffer), "%s", prefix);
    snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer),
             "%d", alert->timestamp);

    switch (alert->nb) {
        /* access address */
        case BTLEJACK:
        case INJECTABLE:
        case KNOB:
            snprintf(buffer + strlen(buffer),
                     sizeof(buffer) - strlen(buffer),
                     " -- %02X:%02X:%02X:%02X",
                     alert->access_addr[3], alert->access_addr[2],
                     alert->access_addr[1], alert->access_addr[0]);
            break;

        /* advertisement address */
        case NO_ALERT:
        case BTLEJUICE:
        case GATTACKER:
            snprintf(buffer + strlen(buffer),
                     sizeof(buffer) - strlen(buffer),
                     " -- %02X:%02X:%02X:%02X:%02X:%02X",
                     alert->adv_addr[5], alert->adv_addr[4],
                     alert->adv_addr[3], alert->adv_addr[2],
                     alert->adv_addr[1], alert->adv_addr[0]);
            break;

        default:
            break;
    }

    LOG_INF("[ALERT] %s", buffer);
}

#define ADV_SIROCCO_ID 0xABC

/* Broadcast the alert for 5sec. */
static void broadcast_alert(struct alert_t *alert)
{
    int ret;
    size_t ad_data_size = 12;
    uint8_t ad_data[14] = {
        BT_LE_AD_NO_BREDR,
        (ADV_SIROCCO_ID & 0xFF),
        ((ADV_SIROCCO_ID >> 8) & 0xFF),
        alert->nb,
        (alert->timestamp & 0xFF),
        ((alert->timestamp >> 8) & 0xFF),
        ((alert->timestamp >> 16) & 0xFF),
        ((alert->timestamp >> 24) & 0xFF)
    };

    switch (alert->nb) {
        /* access address */
        case BTLEJACK:
        case INJECTABLE:
        case KNOB:
            memcpy(&ad_data[8], alert->access_addr, 4);
            ad_data_size = 12;
            break;

        /* advertisement address */
        case NO_ALERT:
        case BTLEJUICE:
        case GATTACKER:
            memcpy(&ad_data[8], alert->adv_addr, BDADDR_SIZE);
            ad_data_size = 14;
            break;

        default:
            return;
    }

    struct bt_data ad[] = {
        BT_DATA(BT_DATA_FLAGS, &ad_data[0], 1),
        BT_DATA(BT_DATA_MANUFACTURER_DATA, ad_data + 1, ad_data_size - 1)
    };

    ret = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (ret) {
        printk("Advertising failed to start (err %d)\n", ret);
        return;
    }

    k_msleep(5000);

    ret = bt_le_adv_stop();
    if (ret) {
        printk("Advertising failed to stop (err %d)\n", ret);
    }
}


/*****************************************************************************/

void srcc_alert(enum alert_num nb, uint32_t timestamp, uint8_t addr[])
{
    struct alert_t alert;

    /* Create the alert object. */
    memset(&alert, 0, sizeof(struct alert_t));
    alert.nb = nb;
    alert.timestamp = timestamp;
    switch (nb) {
        /* access address */
        case BTLEJACK:
        case INJECTABLE:
        case KNOB:
            memcpy(&alert.access_addr, addr, 4);
            break;

        /* advertisement address */
        case NO_ALERT:
        case BTLEJUICE:
        case GATTACKER:
            memcpy(&alert.access_addr, addr, BDADDR_SIZE);
            break;

        default:
            break;
    }

    /* Send it to the alert thread via message queue. */
    while (k_msgq_put(&alert_msgq, &alert, K_NO_WAIT) != 0) {
        /* message queue is full, purge old data & retry */
        k_msgq_purge(&alert_msgq);
    }
}

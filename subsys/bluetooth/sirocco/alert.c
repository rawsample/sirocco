/*
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/sirocco.h>


LOG_MODULE_DECLARE(sirocco, CONFIG_BT_SRCC_LOG_LEVEL);



K_MSGQ_DEFINE(alert_msgq, sizeof(struct alert_t), 32, 1);


/* Thread aggregating alerts
 *****************************************************************************/

#define SRCC_ALERT_THREAD_STACK_SIZE 1024
#define SRCC_ALERT_THREAD_PRIORITY 5

static void log_alert(struct alert_t *alert);


/* Callback mechanism
 ****************************************************************************/
static struct alert_cb *callback_list;

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
            /* Alerts can be logged, send by HCI, etc. */
            log_alert(&alert);

            for (struct srcc_alert_cb *cb = callback_list; cb; cb = cb->_next) {
                if(cb->alert_cb) {
                    cb->alert_cb(&alert);
                }
            }

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

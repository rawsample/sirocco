/*
 *
 */
#include <inttypes.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/hash_map.h>

#include <zephyr/bluetooth/sirocco.h>

#include "adv.h"




void printk_adv_metric(enum event_type event, struct srcc_adv_metric *adv_metric)
{
    uint8_t  addr[BDADDR_SIZE];

    switch (adv_metric->type) {
        case 0b0000:    /* ADV_IND */
            memcpy(&addr, adv_metric->adv_ind.addr,
                   sizeof(struct pdu_adv_adv_ind));
            break;
        
        case 0b0001:    /* ADV_DIRECT_IND */
            memcpy(&addr, adv_metric->adv_ind.addr,
                   sizeof(struct pdu_adv_adv_ind));
        
        case 0b0101:    /* CONNECT_IND */
            memcpy(&addr, adv_metric->connect_ind.adv_addr,
                    BDADDR_SIZE* sizeof(uint8_t));
            break;

        default:
            /* */
            return;
    }

    printk("[SIROCCO] ADV ");
    switch (event) {
        case ADV_TX: 
            printk("TX");
            break;
        case ADV_RX: 
            printk("RX");
            break;
        default:
            break;
    }
    printk(": %u 0x%x %02x:%02x:%02x:%02x 0x%x %hu %hu\n",
           adv_metric->timestamp,
           adv_metric->type,
           addr[0],
           addr[1],
           addr[2],
           addr[3],
           adv_metric->len,
           adv_metric->crc_is_valid,
           adv_metric->rssi
    );
    printk("\n");
}


void run_adv_rx_detection(struct srcc_adv_metric *adv_metric)
{
    printk_adv_metric(ADV_RX, adv_metric);

    /* Call detection modules here */
#if defined(CONFIG_BT_SRCC_BTLEJUICE) && defined(CONFIG_BT_PERIPHERAL)
    srcc_start_btlejuice_bg_scan(adv_metric);
#endif  /* CONFIG_BT_SRCC_BTLEJUICE */

}

void run_adv_tx_detection(struct srcc_adv_metric *adv_metric)
{
    printk_adv_metric(ADV_TX, adv_metric);

    /* Call detection modules here */
}
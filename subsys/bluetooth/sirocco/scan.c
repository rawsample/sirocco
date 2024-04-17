/*
 *
 * Handle received advertising and scan request packets.
 */
#include <inttypes.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/hash_map.h>

#include <zephyr/bluetooth/sirocco.h>


SYS_HASHMAP_DEFINE_STATIC(scan_hmap);



/* Printk functions */

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



#define BUFFER_SIZE 512

void printk_scan_pdu(struct srcc_scan_metric *scan_metric)
{
    /* Following is the structure of a Link Layer PDU:
     *
     * +-----------------+--------------------------------------------------------+
     * |  Access Address |                   PDU ( 2 - 255 bytes)                 |
     * +-----------------+--------------------------------------------------------+
     * |       ---       |              Header                          | Payload |
     * +-----------------+----------------------------------------------+---------+
     * |       ---       | PDU type | RFU | ChSel | TxAdd | RxAdd | Len |   ---   |
     * +-----------------+----------------------------------------------+---------+
     */
    size_t len;
    uint8_t buffer[512];
    int offset;

    /* access address */
    //snprintf(buffer, BUFFER_SIZE, "| %02x:%02x:%02x:%02x |");
    offset = snprintf(buffer, BUFFER_SIZE, "| --:--:--:-- |");

    /* header and payload */
    switch (scan_metric->type) {
        case 0:    /* ADV_IND */
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                               " ADV_IND | - | - | - | - | %02d |",
                               scan_metric->len);
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                               " %02X:%02X:%02X:%02X:%02X:%02X | ",
                               scan_metric->adv_ind.addr[5],
                               scan_metric->adv_ind.addr[4],
                               scan_metric->adv_ind.addr[3],
                               scan_metric->adv_ind.addr[2],
                               scan_metric->adv_ind.addr[1],
                               scan_metric->adv_ind.addr[0]
            );
            for (int i=0; i<scan_metric->len; i++) {
                offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                                   "%02x", scan_metric->adv_ind.data[i]);
            }
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset, " |");
            break;

        case 1:    /* ADV_DIRECT_IND */
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                               " ADV_DIRECT_IND | - | - | - | - | %02d |",
                     scan_metric->len);
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                     " %02X:%02X:%02X:%02X:%02X:%02X | %02X:%02X:%02X:%02X:%02X:%02X |",
                     scan_metric->direct_ind.adv_addr[5],
                     scan_metric->direct_ind.adv_addr[4],
                     scan_metric->direct_ind.adv_addr[3],
                     scan_metric->direct_ind.adv_addr[2],
                     scan_metric->direct_ind.adv_addr[1],
                     scan_metric->direct_ind.adv_addr[0],
                     scan_metric->direct_ind.tgt_addr[5],
                     scan_metric->direct_ind.tgt_addr[4],
                     scan_metric->direct_ind.tgt_addr[3],
                     scan_metric->direct_ind.tgt_addr[2],
                     scan_metric->direct_ind.tgt_addr[1],
                     scan_metric->direct_ind.tgt_addr[0]
            );
            break;

        case 3:    /* SCAN_REQ */
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                               " SCAN_REQ | - | - | - | - | %02d |",
                     scan_metric->len);
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                     " %02X:%02X:%02X:%02X:%02X:%02X | %02X:%02X:%02X:%02X:%02X:%02X |",
                     scan_metric->scan_req.scan_addr[5],
                     scan_metric->scan_req.scan_addr[4],
                     scan_metric->scan_req.scan_addr[3],
                     scan_metric->scan_req.scan_addr[2],
                     scan_metric->scan_req.scan_addr[1],
                     scan_metric->scan_req.scan_addr[0],
                     scan_metric->scan_req.adv_addr[5],
                     scan_metric->scan_req.adv_addr[4],
                     scan_metric->scan_req.adv_addr[3],
                     scan_metric->scan_req.adv_addr[2],
                     scan_metric->scan_req.adv_addr[1],
                     scan_metric->scan_req.adv_addr[0]
            );
            break;

        case 4:    /* SCAN_RESP */
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                               " SCAN_RESP | - | - | - | - | %02d |",
                               scan_metric->len);
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                               " %02X:%02X:%02X:%02X:%02X:%02X | ",
                               scan_metric->scan_rsp.addr[5],
                               scan_metric->scan_rsp.addr[4],
                               scan_metric->scan_rsp.addr[3],
                               scan_metric->scan_rsp.addr[2],
                               scan_metric->scan_rsp.addr[1],
                               scan_metric->scan_rsp.addr[0]
            );
            for (int i=0; i<scan_metric->len; i++) {
                offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                                   "%02x", scan_metric->scan_rsp.data[i]);
            }
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset, " |");
            break;

        default:
            offset += snprintf(buffer + offset, BUFFER_SIZE - offset,
                               " %02d | - | - | - | - | %02d | ... |",
                               scan_metric->type, scan_metric->len);
            break;
    }

    len = strlen(buffer);
    printf("[SIROCCO] SCAN PDU (%zu): %s\n", len, buffer);
}


void run_scan_detection(struct srcc_scan_metric *scan_metric)
{
    // For now, only print something
    //printk_scan_metric(scan_metric);
    //printk_scan_pdu(scan_metric);

    uint64_t addr = 0;
    uint64_t counter = 0;


    switch (scan_metric->type) {
        case 0b0000:    /* ADV_IND */
            memcpy(&addr, &(scan_metric->adv_ind.addr), BDADDR_SIZE*sizeof(uint8_t));
            break;

        case 0b0100:    /* SCAN_RESP */
            memcpy(&addr, &(scan_metric->scan_rsp.addr), BDADDR_SIZE*sizeof(uint8_t));
            break;

        default:
            break;
    }

    if (sys_hashmap_get(&scan_hmap, addr, &counter)) {
        counter++;
    }

    sys_hashmap_insert(&scan_hmap, addr, counter, NULL);

    printk("[SIROCCO] SCAN RX: %" PRIx64 " -- %llu\n", addr, counter);

}
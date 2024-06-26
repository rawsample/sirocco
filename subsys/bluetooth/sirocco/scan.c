/*
 *
 * Handle received advertising and scan request packets.
 */
#include <inttypes.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/hash_map.h>

#include <zephyr/bluetooth/sirocco.h>

#include "timing.h"
#include "scan.h"


#define TIMEOUT_MSEC 30000   /* in ms */
//#define TIMEOUT_CYCLES (TIMEOUT_SEC * CYCLES_PER_SEC)

//SYS_HASHMAP_DEFINE_STATIC(scan_hmap);
#define SCAN_MAX_ENTRY 32   /* Note: If > 255, update struct timeout_data below */
SYS_HASHMAP_SC_DEFINE_STATIC_ADVANCED(scan_hmap,
                                      sys_hash32,
                                      SYS_HASHMAP_DEFAULT_ALLOCATOR,
                                      SYS_HASHMAP_CONFIG(SCAN_MAX_ENTRY, SYS_HASHMAP_DEFAULT_LOAD_FACTOR)
);

static uint32_t debug_scan_data_counter = 0;


/* Printk functions */
#define BUFFER_SIZE 512

void printk_scan_metric(struct srcc_scan_metric *scan_metric)
{
    printk("[SIROCCO] SCAN RX: %u %02x:%02x:%02x:%02x:%02x:%02x 0x%x 0x%x %hu %hu %hu %d\n",
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

// TODO: make it independant from struct srcc_scan_metric
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
    uint8_t buffer[BUFFER_SIZE];
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


static struct scan_data *malloc_scan_data(void)
{
    struct scan_data *scan_data;

    scan_data = k_calloc(1, sizeof(struct scan_data));
    if (scan_data == NULL) {
        printk("Failed to allocate memory for scan_data :( count = %d\n",
                debug_scan_data_counter);
        return NULL;
    }
    debug_scan_data_counter++;

#if defined(CONFIG_BT_SRCC_OASIS_GATTACKER)
    init_oasis_gattacker_data(&scan_data->oasis_gattacker_data);
#endif  /* CONFIG_BT_SRCC_OASIS_GATTACKER */

    return scan_data;
}

static void free_scan_data(struct scan_data *scan_data)
{
#if defined(CONFIG_BT_SRCC_OASIS_GATTACKER)
    clean_oasis_gattacker_data(&scan_data->oasis_gattacker_data);
#endif  /* CONFIG_BT_SRCC_OASIS_GATTACKER */

    k_free(scan_data);
    debug_scan_data_counter--;
}


struct timeout_data {
    uint32_t threshold; /* in ms */
    uint64_t keys[5];   /* Let's not delete more than 5 entries at a time */
    uint8_t len;
};

static void remove_timeout_callback(uint64_t key, uint64_t value, void *cookie)
{
    struct timeout_data *timeout = (struct timeout_data *)cookie;
    struct scan_data *scan_data = (struct scan_data *)((uint32_t)value);

    //printk("Cookie->threshold is %u\n", timeout->threshold);
    //printk("(%#llx -> %#llx) ", key, value);

    /* Let's not delete more than 5 entries at a time */
    if (timeout->len > 10) {
        return;
    }

    if (scan_data->previous_timestamp < timeout->threshold) {
        timeout->keys[timeout->len] = key;
        timeout->len++;
    }
}

/* Remove all entries where previous_metric.timestamp is older than TIMEOUT */
static int remove_timeout_entries()
{
    struct timeout_data timeout;
    struct scan_data *scan_data;
    uint64_t ptr;
    bool success;
    int count = 0;

    timeout.len = 0;
    timeout.threshold = srcc_timing_capture_ms() - TIMEOUT_MSEC;

    //printk("Threshold is %u\n", timeout.threshold);

    sys_hashmap_foreach(&scan_hmap, remove_timeout_callback, &timeout);

    for (int i=0; i<timeout.len; i++) {
        success = sys_hashmap_remove(&scan_hmap, timeout.keys[i], &ptr);
        if (success) {
            scan_data = (struct scan_data *)((uint32_t)ptr);
            //printk("Removed from hashmap %llx and freeing %p\n", timeout.keys[i], scan_data);
            free_scan_data(scan_data);
            count++;
        } else {
            printk("Failed to remove from hashmap 0x%#llx\n", ptr);
        }
    }

    return count;
}


void run_scan_rx_detection(struct srcc_scan_metric *scan_metric)
{
    // For now, only print something
    //printk_scan_metric(scan_metric);
    //printk_scan_pdu(scan_metric);

    uint64_t addr = 0;
    struct scan_data *scan_data;
    uint64_t hvalue;
    bool success;


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

    if (sys_hashmap_get(&scan_hmap, addr, &hvalue)) {

        scan_data = (struct scan_data *)((uint32_t)hvalue);

    } else {
        /* Init new scan_data */
        scan_data = malloc_scan_data();
        if (scan_data == NULL) {
            //printk("Failed to allocated scan_data...try freeing old entries...\n");
            remove_timeout_entries();
            //printk("try again...\n");
            scan_data = malloc_scan_data();
            if (scan_data == NULL) {
                //printk("...no sucess, leaving!\n");
                return;
            }
            //printk("...yes!\n");
        }
        //printk("Allocated scan_data %p\n", scan_data);

        success = sys_hashmap_insert(&scan_hmap, addr, ((uint32_t)scan_data) | 0ULL, NULL);
        if (!success) {
            //printk("Failed to insert into scan_hmap...try freeing...\n");
            remove_timeout_entries();
            success = sys_hashmap_insert(&scan_hmap, addr, ((uint32_t)scan_data) | 0ULL, NULL);
            if (!success) {
                //printk("...no sucess, leaving!\n");
                free_scan_data(scan_data);
                return;
            }
            //printk("good, continuing!\n");
        }
    }

    scan_data->counter++;

    /*
    printk("[SIROCCO] SCAN RX: %" PRIx64 " -- %llu"
           " %hu : %u | %u : %u"
           "\n",
           addr, scan_data->counter,
           scan_metric->interval, adv_interval,
           scan_metric->timestamp, scan_data->previous_metric.timestamp
    );
    */

    /* Call detection modules here */
//#if defined(CONFIG_BT_SRCC_BTLEJUICE) && defined(CONFIG_BT_PERIPHERAL)
//    srcc_detect_btlejuice(scan_metric);
//#endif  /* CONFIG_BT_SRCC_BTLEJUICE && CONFIG_BT_PERIPHERAL */

#if defined(CONFIG_BT_SRCC_OASIS_GATTACKER)
    srcc_detect_oasis_gattacker(addr, scan_data, scan_metric);
#endif  /* CONFIG_BT_SRCC_OASIS_GATTACKER */


    /* Clean up routines */
    scan_data->previous_timestamp = scan_metric->timestamp;
    remove_timeout_entries();
    //printk("Hashmap size: %u\n", sys_hashmap_size(&scan_hmap));
}
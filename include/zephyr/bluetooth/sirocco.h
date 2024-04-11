/*
 * The Sirocco Bluetooth Intrusion Detection System (IDS).
 */

#ifndef SRCC_H
#define SRCC_H

#include <zephyr/bluetooth/addr.h>



#define SRCC_MALLOC_COUNT_ITEMS 20


/* Packet */
//enum adv_type {
//  ADV_IND = 0,
//  ADV_DIRECT_IND = 1,
//  ADV_NONCONN_IND = 2,
//  SCAN_REQ = 3,
//  SCAN_RSP = 4,
//  CONNECT_REQ = 5,
//  ADV_SCAN_IND = 6
//};

//struct adv_ind {
//  uint8_t adv_address[6];
//  uint8_t *data;
//};
//
//struct srcc_ble_header {
//  uint8_t ll_id:2;
//  uint8_t nesn:1;
//  uint8_t sn:1;
//  uint8_t md:1;
///*
//#if defined(CONFIG_BT_CTLR_DF_CONN_CTE_TX) || defined(CONFIG_BT_CTLR_DF_CONN_CTE_RX)
//	uint8_t cp:1;
//	uint8_t rfu:2;
//#else
//*/
//	uint8_t rfu:3;
///*
//#endif
//*/
//};

/*
struct __aligned(4) srcc_ble_pkt {
  uint32_t timestamp;
  uint16_t crc_is_valid;
  uint16_t rssi;
  uint8_t len;
  struct srcc_ble_header header;
  uint8_t payload[255];
};
*/

/* Connection */
//struct __aligned(4) srcc_ble_conn {
//  uint8_t access_address[4];
//  uint8_t crc_init[3];
//  uint8_t channel_map[5]; // voir PDU_CHANNEL_MAP_SIZE
//  uint16_t hop_interval;
//  /* Count the number of packet lost between a ISR TX and ISR RX */
//  uint16_t packet_lost_counter;
//  uint16_t tx_counter;
//  uint16_t rx_counter;
//};

/* Advertise
struct __aligned(4) srcc_ble_adv {
	uint8_t  adv_addr[BDADDR_SIZE];
	uint8_t  adv_addr_type:1;
	uint8_t  init_addr[BDADDR_SIZE];
	uint16_t interval;
	uint32_t ticks_window;
};
*/

/* Device 
enum srcc_gap_role {
  ADVERTISER = 0x0,
  PERIPHERAL = 0x1,
  SCANNER    = 0x2,
  CENTRAL    = 0x3
};

struct srcc_local_dev {
  enum srcc_gap_role gap_role;
  bt_addr_le_t address;
};

struct srcc_remote_dev {
  enum srcc_gap_role gap_role;
  bt_addr_le_t address;
  //#ifdef SCAN_ENABLED
  //uint16_t adv_interval;
  //#endif
  //#ifdef CONNECTION_ENABLED
  //uint32_t connection_interval;
  //#endif
};
*/

/* Event type
 * - first bit indicates RX / TX
 * - second bit indicates CONN / SCAN
 */
enum event_type {
    CONN_RX  = 0x0,
    CONN_TX  = 0x1,
    SCAN_RX  = 0x2,
    SCAN_TX  = 0x3,
};

/* Metrics
struct __aligned(4) srcc_metric {
    enum event_type type;
    struct srcc_ble_pkt pkt;
    struct srcc_ble_conn conn;
    struct srcc_ble_adv adv;
    struct srcc_local_dev local_dev;
    struct srcc_remote_dev remote_dev;
};

struct __aligned(4) metric_item {
    void *fifo_reserved;
    struct srcc_metric metric;
};
*/


/* SCAN metrics */

/* Ugly hack to check if pdu.h has been included or we should defines struct */
#ifndef BDADDR_SIZE
/* Taken from controller/ll_sw/pdu.h */
#define BDADDR_SIZE   6
#define PDU_AC_LEG_DATA_SIZE_MAX   31

struct pdu_adv_scan_req {
    uint8_t scan_addr[BDADDR_SIZE];
    uint8_t adv_addr[BDADDR_SIZE];
} __packed;

struct pdu_adv_scan_rsp {
    uint8_t addr[BDADDR_SIZE];
    uint8_t data[PDU_AC_LEG_DATA_SIZE_MAX];
} __packed;

struct pdu_adv_adv_ind {
    uint8_t addr[BDADDR_SIZE];
    uint8_t data[PDU_AC_LEG_DATA_SIZE_MAX];
} __packed;

struct pdu_adv_direct_ind {
    uint8_t adv_addr[BDADDR_SIZE];
    uint8_t tgt_addr[BDADDR_SIZE];
} __packed;

#endif  /* BDADDR_SIZE */

struct __aligned(4) srcc_scan_metric {

    /* Local Device (lll_scan) */
    //uint8_t  adv_addr[BDADDR_SIZE];
    //uint16_t interval;                // scanning interval used by the scanner
    //uint32_t ticks_window;            // similar

    /* Meta data */
    uint32_t timestamp;
    uint16_t rssi;

    /* Packet */
    uint16_t crc_is_valid;
    /* Header */
    uint8_t type:4;
    //uint8_t rfu:1;
    //uint8_t chan_sel:1;
    //uint8_t tx_addr:1;
    //uint8_t rx_addr:1;
    uint8_t len;

    /* Payload */
    union {
        struct pdu_adv_scan_req scan_req;
        struct pdu_adv_scan_rsp scan_rsp;
        struct pdu_adv_adv_ind adv_ind;
        struct pdu_adv_direct_ind direct_ind;
    } __packed;
} __packed;

struct __aligned(4) srcc_scan_item {
  void *fifo_reserved;
  struct srcc_scan_metric metric;
} __packed;


/* CONN metrics */

struct __aligned(4) srcc_conn_metric {

    /* Local device (lll_conn) */
    uint8_t access_addr[4];

    /* Meta data */
    uint32_t timestamp;
    uint16_t rssi;

    /* Packet */
    uint16_t crc_is_valid;
    /* Header */
    //uint8_t ll_id:2;
    //uint8_t nesn:1;
    //uint8_t sn:1;
    //uint8_t md:1;
    //uint8_t rfu:3;
    uint8_t len;

    /* Payload */
    //uint8_t payload[]
} __packed;

struct __aligned(4) srcc_conn_item {
  void *fifo_reserved;
  struct srcc_conn_metric metric;
} __packed;


/* API exposed to applications */
/* Callback definitions */
/* Be carefull everything happens in ISR context
 */
struct srcc_cb {
  //void (*conn_init)(void);
  //void (*conn_delete)(void);
  void (*conn_rx)(struct srcc_metric *);
  void (*conn_tx)(struct srcc_metric *);
  void (*scan_rx)(struct srcc_metric *);
  //void (*scan_tx)(void);
  struct srcc_cb *_next;
};

void srcc_init(void);
void srcc_cb_register(struct srcc_cb *cb);



/* Reserved usage, should not be called by applications */
//void srcc_notify_conn_init(void);
//void srcc_notify_conn_delete(void);
void srcc_notify_conn_rx(struct srcc_conn_item *item);
void srcc_notify_conn_tx(struct srcc_conn_item *item);
void srcc_notify_scan_rx(struct srcc_scan_item *item);


int srcc_init_conn_alloc(uint32_t count);
int srcc_init_scan_alloc(uint32_t count);
void srcc_clean_conn_alloc(void);
void srcc_clean_scan_alloc(void);
void *srcc_malloc_conn_item(void);
void *srcc_malloc_scan_item(void);
void srcc_free_conn_item(void *ptr);
void srcc_free_scan_item(void *ptr);


/* Detection modules */

enum alert_num {
  NO_ALERT    = 0x0,
  BTLEJACK    = 0x1,
  BTLEJUICE   = 0x2,
  GATTACKER   = 0x3,
  INJECTABLE  = 0x4,
  JAMMING     = 0x5,
  KNOB        = 0x6,
};

void srcc_alert(enum alert_num nb, const char *fmt, ...);

#if defined(CONFIG_BT_SRCC_BTLEJACK)
void srcc_detect_btlejack(struct srcc_metric *metric);
#endif
#if defined(CONFIG_BT_SRCC_BTLEJUICE)
void srcc_detect_btlejuice(struct srcc_metric *metric);
#endif
#if defined(CONFIG_BT_SRCC_GATTACKER)
void srcc_detect_gattacker(struct srcc_metric *metric);
#endif
#if defined(CONFIG_BT_SRCC_INJECTABLE)
void srcc_detect_injectable(struct srcc_metric *metric);
#endif
#if defined(CONFIG_BT_SRCC_JAMMING)
void srcc_detect_jamming(struct srcc_metric *metric);
#endif
#if defined(CONFIG_BT_SRCC_KNOB)
void srcc_detect_knob(struct srcc_metric *metric);
#endif

#endif
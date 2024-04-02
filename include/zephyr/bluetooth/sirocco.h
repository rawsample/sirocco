/*
 * The Sirocco Bluetooth Intrusion Detection System (IDS).
 */

#ifndef SRCC_H
#define SRCC_H

#include <zephyr/bluetooth/addr.h>



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

struct adv_ind {
  uint8_t adv_address[6];
  uint8_t *data;
};

struct srcc_ble_header {
  uint8_t ll_id:2;
  uint8_t nesn:1;
  uint8_t sn:1;
  uint8_t md:1;
/*
#if defined(CONFIG_BT_CTLR_DF_CONN_CTE_TX) || defined(CONFIG_BT_CTLR_DF_CONN_CTE_RX)
	uint8_t cp:1;
	uint8_t rfu:2;
#else
*/
	uint8_t rfu:3;
/*
#endif
*/
};

struct __aligned(4) srcc_ble_pkt {
  uint32_t timestamp;
  uint16_t crc_is_valid;
  uint16_t rssi;
  uint8_t len;
  struct srcc_ble_header header;
  uint8_t payload[255];
};

/* Connection */
struct __aligned(4) srcc_ble_conn {
  uint8_t access_address[4];
  uint8_t crc_init[3];
  uint8_t channel_map[5]; // voir PDU_CHANNEL_MAP_SIZE
  uint16_t hop_interval;
  /* Count the number of packet lost between a ISR TX and ISR RX */
  uint16_t packet_lost_counter;
  uint16_t tx_counter;
  uint16_t rx_counter;
};

/* Device */
enum srcc_gap_role {
  ADVERTISER = 0,
  PERIPHERAL = 1,
  SCANNER = 2,
  CENTRAL = 3
};

struct srcc_local_dev {
  enum srcc_gap_role gap_role;
  bt_addr_le_t address;
};

struct srcc_remote_dev {
  enum srcc_gap_role gap_role;
  bt_addr_le_t address;
  //#ifdef SCAN_ENABLED
  //uint32_t advertisements_interval;
  //#endif
  //#ifdef CONNECTION_ENABLED
  //uint32_t connection_interval;
  //#endif
};

/* Metrics */
struct __aligned(4) srcc_metric {
    struct srcc_ble_pkt pkt;
    struct srcc_ble_conn conn;
    //struct srcc_local_dev local_dev;
    //struct srcc_remote_dev remote_dev;
};

struct __aligned(4) metric_item {
    void *fifo_reserved;
    struct srcc_metric metric;
};



/* API exposed to applications */
/* Callback definitions */
/* Be carefull everything happens in ISR context
 */
struct srcc_cb {
  //void (*conn_init)(void);
  //void (*conn_delete)(void);
  void (*conn_rx)(struct srcc_metric *);
  //void (*conn_tx)(void);
  //void (*scan_rx)(void);
  //void (*scan_tx)(void);
  struct srcc_cb *_next;
};

void srcc_init(void);
void srcc_cb_register(struct srcc_cb *cb);



/* Reserved usage, should not be called by applications */
//void srcc_notify_conn_init(void);
//void srcc_notify_conn_delete(void);
void srcc_notify_conn_rx(struct metric_item *item);
//void srcc_notify_conn_tx(void);
//void srcc_notify_scan_rx(void);


#endif
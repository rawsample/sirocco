/*
 */
#ifndef LLL_SRCC_H
#define LLL_SRCC_H

void lll_srcc_conn_rx(struct lll_conn *lll, uint8_t crc_ok, uint32_t rssi_value);
void lll_srcc_conn_tx(struct lll_conn *lll);

void lll_srcc_scan_rx(struct lll_scan *lll, uint8_t crc_ok, uint32_t rssi_value);

#endif
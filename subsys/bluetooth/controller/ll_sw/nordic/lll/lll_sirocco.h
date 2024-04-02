/*
 */
#ifndef LLL_SRCC_H
#define LLL_SRCC_H

void lll_srcc_conn_rx(uint8_t crc_ok, uint32_t rssi_value,
    struct lll_conn *lll, struct pdu_data *pdu_data);

#endif
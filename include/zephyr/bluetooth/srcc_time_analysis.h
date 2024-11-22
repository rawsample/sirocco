/*
 * Define a set of structure and function to measure
  * and analyse latency introduced by Sirocco.
 */
#include <zephyr/kernel.h>



void init_srcc_analysis(void);

/* Measure the time spent within the ISR for packet reception and emission. */
void start_timestamp_conn_rx_isr(void);
void start_timestamp_conn_tx_isr(void);
void start_timestamp_scan_rx_isr(void);
void start_timestamp_adv_rx_isr(void);
void start_timestamp_adv_tx_isr(void);
void stop_timestamp_conn_rx_isr(void);
void stop_timestamp_conn_tx_isr(void);
void stop_timestamp_scan_rx_isr(void);
void stop_timestamp_adv_rx_isr(void);
void stop_timestamp_adv_tx_isr(void);

/* Measure the FIFOs latency and throughput */
void record_dequeue_metric(void);
void record_enqueue_metric(void);
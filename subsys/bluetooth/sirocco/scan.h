#include <zephyr/kernel.h>
#include <zephyr/sys/hash_map.h>
#include <zephyr/sys/ring_buffer.h>

#include <zephyr/bluetooth/sirocco.h>



#if defined(CONFIG_BT_SRCC_OASIS_GATTACKER)
#define RBUFF_SIZE 128

struct adv_intervals {
  struct ring_buf rb;
  uint8_t buf[RBUFF_SIZE];
};

struct oasis_gattacker_data {
    struct adv_intervals adv_intervals;
    uint32_t threshold;
    uint8_t last_channel;
    uint8_t suspicious;
};
#endif  /* CONFIG_BT_SRCC_OASIS_GATTACKER */



struct scan_data {
    uint64_t counter;
    uint32_t previous_timestamp;
#if defined(CONFIG_BT_SRCC_OASIS_GATTACKER)
    struct oasis_gattacker_data oasis_gattacker_data;
    //struct srcc_scan_metric previous_adv_metric;
    uint32_t previous_adv_timestamp;
#endif  /* CONFIG_BT_SRCC_OASIS_GATTACKER */
};



#if defined(CONFIG_BT_SRCC_OASIS_GATTACKER)
void srcc_detect_oasis_gattacker(uint64_t address, struct scan_data *data, struct srcc_scan_metric *metric);
void init_oasis_gattacker_data(struct oasis_gattacker_data *data);
void clean_oasis_gattacker_data(struct oasis_gattacker_data *data);
#endif  /* CONFIG_BT_SRCC_OASIS_GATTACKER */

#if defined(CONFIG_BT_SRCC_BTLEJUICE) && defined(CONFIG_BT_PERIPHERAL)
void srcc_detect_btlejuice(struct srcc_scan_metric *scan_metric);
#endif  /* CONFIG_BT_SRCC_BTLEJUICE */
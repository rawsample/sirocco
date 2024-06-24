#include <zephyr/bluetooth/sirocco.h>


#if defined(CONFIG_BT_SRCC_BTLEJUICE) && defined(CONFIG_BT_PERIPHERAL)
void srcc_start_btlejuice_bg_scan(struct srcc_adv_metric *adv_metric);
void srcc_stop_btlejuice_bg_scan(void);
#endif  /* CONFIG_BT_SRCC_BTLEJUICE */
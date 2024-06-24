/*
 */

//#include <stdarg.h>
#include <zephyr/bluetooth/sirocco.h>


void srcc_alert(enum alert_num nb, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

    char buffer[256];

    const char *prefix = "";
    switch (nb) {
        case NO_ALERT:      prefix = " "; break;
        case BTLEJACK:      prefix = "BTLJack: "; break;
        case BTLEJUICE:     prefix = "BTLEJuice: "; break;
        case GATTACKER:     prefix = "GATTacker: "; break;
        case INJECTABLE:    prefix = "InjectaBLE: "; break;
        case JAMMING:       prefix = "Jamming: "; break;
        case KNOB:          prefix = "KNOB: "; break;
    }
    snprintf(buffer, sizeof(buffer), "%s", prefix);
    vsnprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), fmt, ap);

    printk("[SIROCCO >ALERT<] %s\n", buffer);

	va_end(ap);
}
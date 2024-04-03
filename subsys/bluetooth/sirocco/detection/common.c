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
        case NO_ALERT:      prefix = ": ";
        case BTLEJACK:      prefix = "BTLJack: ";
        case BTLEJUICE:     prefix = "BTLEJuice: ";
        case GATTACKER:     prefix = "GATTacker: ";
        case INJECTABLE:    prefix = "InjectaBLE: ";
        case JAMMING:       prefix = "Jamming: ";
        case KNOB:          prefix = "KNOB: ";
    }
    snprintf(buffer, sizeof(buffer), "%s", prefix);
    vsnprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), fmt, ap);

    printk("[SIROCCO ALERT] %s\n", buffer);

	va_end(ap);
}
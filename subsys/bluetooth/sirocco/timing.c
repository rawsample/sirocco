/*
 *
 */
//#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>
#include <nrfx.h>
#include <nrfx_timer.h>



#define SRCC_NRF2_TIMER 2

static const nrfx_timer_t timer = NRFX_TIMER_INSTANCE(SRCC_NRF2_TIMER);



int srcc_timing_init(void)
{
    nrfx_err_t err;

    nrfx_timer_config_t config = {
        .frequency = NRF_TIMER_BASE_FREQUENCY_GET(timer.p_reg), // 16M Hz
        .mode = NRF_TIMER_MODE_TIMER,
        .bit_width = NRF_TIMER_BIT_WIDTH_32,
        .interrupt_priority = NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY,
        .p_context = NULL
    };

    err = nrfx_timer_init(&timer, &config, NULL);
    NRFX_ASSERT(err == NRFX_SUCCESS);

	//nrfx_timer_disable(&timer);
    nrfx_timer_clear(&timer);

    return 0;
}


void srcc_timing_start(void)
{
	return nrfx_timer_enable(&timer);
}

void srcc_timing_stop(void)
{
	return nrfx_timer_disable(&timer);
}


uint32_t srcc_timing_cycles_to_ms(uint32_t cycles)
{
	return (cycles) / 16000;
}


uint32_t srcc_timing_capture_cycles(void)
{
    return nrfx_timer_capture(&timer, NRF_TIMER_CC_CHANNEL2);
}


/* Note: the overflow is rounded from (2<<32) / 16MHz * 1000 in ms
   This means it should shift of ~0.456 ms every overflow,
   so ~ 146 ms every day.
 */
#define OVERFLOW_MS 268435    // 
static uint32_t overflow_counter = 0;
static uint32_t previous_tsp = 0;

/* Note: the timestamp should overflow every ~49,7 days. */
uint32_t srcc_timing_capture_ms(void)
{
    uint32_t tsp = srcc_timing_cycles_to_ms(srcc_timing_capture_cycles());

    if (tsp < previous_tsp) {
        overflow_counter++;
    }

    previous_tsp = tsp;

    return tsp + overflow_counter * OVERFLOW_MS;
}
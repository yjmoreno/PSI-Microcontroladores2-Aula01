#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(prioridades, LOG_LEVEL_INF);

/* ====================================================
 * ESCOLHA DO CASO: mude o numero, grave e salve o log
 *   1: A prio 5,  B prio 7,  os dois dormem  (codigo original)
 *   2: A prio 7,  B prio 5,  os dois dormem  (prioridades invertidas)
 *   3: A prio 7,  B prio 7,  os dois dormem  (prioridades iguais)
 *   4: A prio 5,  B prio 7,  B SEM k_msleep  (B nunca dorme, prioridade menor)
 *   5: A prio 7,  B prio 5,  B SEM k_msleep  (B nunca dorme, prioridade maior)
 *   6: A prio 5,  B prio -1 (cooperativa)    (A nao consegue interromper B)
 * ==================================================== */
#define CASO 1

#if CASO == 1
#define PRIO_THREAD_A 5
#define PRIO_THREAD_B 7
#define B_DORME       1
#elif CASO == 2
#define PRIO_THREAD_A 7
#define PRIO_THREAD_B 5
#define B_DORME       1
#elif CASO == 3
#define PRIO_THREAD_A 7
#define PRIO_THREAD_B 7
#define B_DORME       1
#elif CASO == 4
#define PRIO_THREAD_A 5
#define PRIO_THREAD_B 7
#define B_DORME       0
#elif CASO == 5
#define PRIO_THREAD_A 7
#define PRIO_THREAD_B 5
#define B_DORME       0
#elif CASO == 6
#define PRIO_THREAD_A 5
#define PRIO_THREAD_B -1
#define B_DORME       1
#endif

#define TEMPO_A_MS  1500  // Thread A dorme
#define TEMPO_B_MS  1000  // Thread B dorme
#define STACK_SIZE  1024  // 512 nao basta com Logging (formatacao usa a pilha)
#define ATRASO_MS   50    // threads comecam depois do main configurar os GPIOs

// --- LEDs e botao via DeviceTree ---
#define LED_A_NODE  DT_ALIAS(led0)                 // LED verde
#define LED_B_NODE  DT_ALIAS(led2)                 // LED vermelho
#define LED_C_NODE  DT_ALIAS(led1)                 // LED azul (ISR)
#define BUTTON_NODE DT_NODELABEL(user_button_0)    // SW0 (PTA16)

static const struct gpio_dt_spec ledA   = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledB   = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);
static const struct gpio_dt_spec ledC   = GPIO_DT_SPEC_GET(LED_C_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static struct gpio_callback button_cb_data;

// ----------------------------------------------------
// THREAD A — tarefa curta
// ----------------------------------------------------
void thread_A(void *p1, void *p2, void *p3)
{
    while (1) {
        uint32_t inicio_ms = k_uptime_get_32();
        uint32_t inicio_cyc = k_cycle_get_32();   // contador do SysTick (48 MHz)

        gpio_pin_set_dt(&ledA, 1);
        for (volatile int i = 0; i < 200000; i++) {}
        gpio_pin_set_dt(&ledA, 0);

        uint32_t dur_us = k_cyc_to_us_floor32(k_cycle_get_32() - inicio_cyc);
        LOG_INF("A: inicio %u ms, fim %u ms, duracao %u us",
                inicio_ms, k_uptime_get_32(), dur_us);

        k_msleep(TEMPO_A_MS);
    }
}

// ----------------------------------------------------
// THREAD B — tarefa longa
// ----------------------------------------------------
void thread_B(void *p1, void *p2, void *p3)
{
    while (1) {
        uint32_t inicio_ms = k_uptime_get_32();
        uint32_t inicio_cyc = k_cycle_get_32();

        gpio_pin_set_dt(&ledB, 1);
        for (volatile int i = 0; i < 10000000; i++) {
            if (i % 2000000 == 0) {
                // tempo intermediario: mostra se B foi interrompida no meio
                LOG_INF("B: %d iteracoes, %u ms desde o inicio",
                        i, k_uptime_get_32() - inicio_ms);
            }
        }
        gpio_pin_set_dt(&ledB, 0);

        uint32_t dur_us = k_cyc_to_us_floor32(k_cycle_get_32() - inicio_cyc);
        LOG_INF("B: inicio %u ms, fim %u ms, duracao %u us",
                inicio_ms, k_uptime_get_32(), dur_us);

        if (B_DORME) {
            k_msleep(TEMPO_B_MS);
        }
    }
}

// ----------------------------------------------------
// ISR do botao — roda acima de qualquer thread
// ----------------------------------------------------
void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    gpio_pin_toggle_dt(&ledC);
    LOG_INF("ISR botao em %u ms (interrompeu a thread %s)",
            k_uptime_get_32(), k_thread_name_get(k_current_get()));
}

K_THREAD_DEFINE(a_tid, STACK_SIZE, thread_A, NULL, NULL, NULL, PRIO_THREAD_A, 0, ATRASO_MS);
K_THREAD_DEFINE(b_tid, STACK_SIZE, thread_B, NULL, NULL, NULL, PRIO_THREAD_B, 0, ATRASO_MS);

int main(void)
{
    if (!gpio_is_ready_dt(&ledA) || !gpio_is_ready_dt(&ledB) ||
        !gpio_is_ready_dt(&ledC) || !gpio_is_ready_dt(&button)) {
        LOG_ERR("GPIO nao esta pronto");
        return 0;
    }

    gpio_pin_configure_dt(&ledA, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledB, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledC, GPIO_OUTPUT_INACTIVE);

    gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    LOG_INF("=== Caso %d: A prio %d, B prio %d, B dorme: %s ===",
            CASO, PRIO_THREAD_A, PRIO_THREAD_B, B_DORME ? "sim" : "nao");
    LOG_INF("Clock do SysTick: %u Hz", sys_clock_hw_cycles_per_sec());

    while (1) {
        k_sleep(K_FOREVER); // main dorme e libera a CPU
    }
    return 0;
}

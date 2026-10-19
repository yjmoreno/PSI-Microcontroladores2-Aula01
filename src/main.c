#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define PERIODO_LED0_MS 1000 /* LED verde */
#define PERIODO_LED1_MS 350  /* LED azul (500 na parte basica) */

/* LEDs definidos no Device Tree da FRDM-KL25Z */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

/* Verifica e configura um LED como saida (apagado) */
static int configurar(const struct gpio_dt_spec *led)
{
    if (!gpio_is_ready_dt(led))
    {
        printk("Erro: LED %s nao esta pronto\n", led->port->name);
        return -1;
    }

    if (gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE) < 0)
    {
        printk("Erro: falha ao configurar o pino %d\n", led->pin);
        return -1;
    }

    return 0;
}

int main(void)
{
    if (configurar(&led0) < 0 || configurar(&led1) < 0)
    {
        return 0;
    }

    printk("LED0 a cada %d ms, LED1 a cada %d ms\n", PERIODO_LED0_MS, PERIODO_LED1_MS);

    /* Proximo instante (em ms desde o boot) em que cada LED deve mudar */
    int64_t proximo_led0 = k_uptime_get() + PERIODO_LED0_MS;
    int64_t proximo_led1 = k_uptime_get() + PERIODO_LED1_MS;

    /* Super-loop: um unico laco controla os dois LEDs, sem threads */
    while (1)
    {
        int64_t agora = k_uptime_get();

        if (agora >= proximo_led0)
        {
            gpio_pin_toggle_dt(&led0);
            proximo_led0 += PERIODO_LED0_MS;
        }

        if (agora >= proximo_led1)
        {
            gpio_pin_toggle_dt(&led1);
            proximo_led1 += PERIODO_LED1_MS;
        }

        k_msleep(1); /* evita ocupar a CPU 100% */
    }

    return 0;
}
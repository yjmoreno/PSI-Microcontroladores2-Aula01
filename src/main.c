#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define PERIODO_LED0_MS 1000 /* LED verde */
#define PERIODO_LED1_MS 350  /* LED azul */

#define STACK_SIZE 1024
#define PRIORIDADE 7

/* LEDs definidos no Device Tree da FRDM-KL25Z */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

/* Configura o LED e fica piscando para sempre com o periodo informado */
static void piscar(const struct gpio_dt_spec *led, int periodo_ms)
{
    if (!gpio_is_ready_dt(led))
    {
        printk("Erro: LED %s nao esta pronto\n", led->port->name);
        return;
    }

    if (gpio_pin_configure_dt(led, GPIO_OUTPUT_INACTIVE) < 0)
    {
        printk("Erro: falha ao configurar o pino %d\n", led->pin);
        return;
    }

    printk("LED no pino %d piscando a cada %d ms\n", led->pin, periodo_ms);

    while (1)
    {
        gpio_pin_toggle_dt(led);
        printk("[%lld ms] LED pino %d trocou\n", k_uptime_get(), led->pin);
        k_msleep(periodo_ms); /* bloqueia so esta thread; a outra continua */
    }
}

/* Uma thread para cada LED */
static void thread_led0(void)
{
    piscar(&led0, PERIODO_LED0_MS);
}

static void thread_led1(void)
{
    piscar(&led1, PERIODO_LED1_MS);
}

K_THREAD_DEFINE(led0_id, STACK_SIZE, thread_led0, NULL, NULL, NULL, PRIORIDADE, 0, 0);
K_THREAD_DEFINE(led1_id, STACK_SIZE, thread_led1, NULL, NULL, NULL, PRIORIDADE, 0, 0);

int main(void)
{
    /* As threads ja iniciam sozinhas; o main nao precisa fazer nada */
    return 0;
}
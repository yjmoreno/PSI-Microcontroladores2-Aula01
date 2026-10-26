#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(estacionamento, LOG_LEVEL_INF);

#define CARROS_POR_CANCELA 1000  /* carros que passam em cada cancela */
#define PROTEGER           0     /* 0 = sem protecao (mostra o erro), 1 = com mutex */

#define STACK_SIZE 1024
#define PRIORIDADE 7             /* mesma prioridade nas duas cancelas */

static const struct gpio_dt_spec led_verde    = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_vermelho = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

/* Recurso compartilhado: quantos carros estao dentro do estacionamento */
static int carros_dentro = 0;

K_MUTEX_DEFINE(trava);
K_SEM_DEFINE(terminou, 0, 2);

/* Atualiza o contador do jeito "ingenuo": le, processa e grava */
static void atualiza_contador(int delta)
{
#if PROTEGER
    k_mutex_lock(&trava, K_FOREVER);
#endif
    int copia = carros_dentro;      /* 1. le o valor atual */
    k_busy_wait(50);                /* 2. tempo de processar o sensor (50 us) */
    carros_dentro = copia + delta;  /* 3. grava o novo valor */
#if PROTEGER
    k_mutex_unlock(&trava);
#endif
}

static void cancela_entrada(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < CARROS_POR_CANCELA; i++) {
        atualiza_contador(+1);
    }
    k_sem_give(&terminou);
}

static void cancela_saida(void *p1, void *p2, void *p3)
{
    for (int i = 0; i < CARROS_POR_CANCELA; i++) {
        atualiza_contador(-1);
    }
    k_sem_give(&terminou);
}

/* As cancelas comecam 100 ms depois do boot, quando o main ja configurou tudo */
K_THREAD_DEFINE(entrada_id, STACK_SIZE, cancela_entrada, NULL, NULL, NULL, PRIORIDADE, 0, 100);
K_THREAD_DEFINE(saida_id,   STACK_SIZE, cancela_saida,   NULL, NULL, NULL, PRIORIDADE, 0, 100);

int main(void)
{
    gpio_pin_configure_dt(&led_verde, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_vermelho, GPIO_OUTPUT_INACTIVE);

    LOG_INF("Protecao com mutex: %s", PROTEGER ? "SIM" : "NAO");
    LOG_INF("Entram %d carros e saem %d. Esperado no final: 0 carros dentro",
            CARROS_POR_CANCELA, CARROS_POR_CANCELA);

    uint32_t inicio = k_uptime_get_32();

    /* espera as duas cancelas terminarem */
    k_sem_take(&terminou, K_FOREVER);
    k_sem_take(&terminou, K_FOREVER);

    LOG_INF("Tempo total: %u ms", k_uptime_get_32() - inicio);
    LOG_INF("Carros dentro no final: %d", carros_dentro);

    if (carros_dentro == 0) {
        gpio_pin_set_dt(&led_verde, 1);
        LOG_INF("Contagem correta");
    } else {
        gpio_pin_set_dt(&led_vermelho, 1);
        LOG_WRN("Contagem errada em %d carros: condicao de corrida!", carros_dentro);
    }
    return 0;
}

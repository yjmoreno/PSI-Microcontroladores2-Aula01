#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <math.h>

/* ====================================================
 * Atividade Acelerometro - Serial (com threads)
 *
 *   aquisicao  (prio 5): le o MMA8451Q a cada 50 ms
 *   integracao (prio 6): calcula velocidade e posicao (regra dos trapezios)
 *   serial     (prio 7): manda ax,ay,az,vx,vy,vz,px,py,pz pela serial
 *
 * As threads conversam por duas filas de mensagens (k_msgq):
 *   aquisicao --fila_acel--> integracao --fila_serial--> serial
 * ==================================================== */

#define PERIODO_MS   50                    /* 20 amostras por segundo */
#define DT           (PERIODO_MS / 1000.0f) /* passo de integracao em segundos */

#define PRIO_AQUISICAO   5   /* menor numero = maior prioridade */
#define PRIO_INTEGRACAO  6
#define PRIO_SERIAL      7

#define STACK_SIZE   1024
#define N_CALIBRA    20      /* 20 amostras = 1 s parado para medir o offset */
#define ZONA_MORTA   0.10f   /* m/s^2: abaixo disso considera aceleracao zero */

/* LED vermelho aceso enquanto calibra (deixe a placa parada!) */
static const struct gpio_dt_spec led_cal = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

/* Acelerometro da placa (alias accel0 -> mma8451q@1d no I2C0) */
static const struct device *const acel = DEVICE_DT_GET(DT_ALIAS(accel0));

/* Uma leitura do sensor */
struct amostra {
    float a[3];          /* ax, ay, az em m/s^2 */
};

/* O que vai para a serial */
struct estado {
    float a[3];          /* aceleracao (ja sem offset) */
    float v[3];          /* velocidade */
    float p[3];          /* posicao */
};

K_MSGQ_DEFINE(fila_acel,   sizeof(struct amostra), 8, 4);
K_MSGQ_DEFINE(fila_serial, sizeof(struct estado),  8, 4);

/* Contadores de amostras perdidas (fila cheia) - so para conferencia */
static uint32_t perdidas_acel;
static uint32_t perdidas_serial;

// ----------------------------------------------------
// THREAD 1 — aquisicao do acelerometro
// ----------------------------------------------------
void thread_aquisicao(void *p1, void *p2, void *p3)
{
    struct sensor_value val[3];
    struct amostra s;
    struct k_timer timer;

    /* timer periodico: o periodo nao "escorrega" mesmo se a leitura demorar */
    k_timer_init(&timer, NULL, NULL);
    k_timer_start(&timer, K_MSEC(PERIODO_MS), K_MSEC(PERIODO_MS));

    while (1) {
        k_timer_status_sync(&timer);           /* dorme ate o proximo tick de 50 ms */

        if (sensor_sample_fetch(acel) < 0) {
            continue;                          /* leitura falhou, tenta no proximo periodo */
        }
        sensor_channel_get(acel, SENSOR_CHAN_ACCEL_XYZ, val);

        for (int i = 0; i < 3; i++) {
            s.a[i] = sensor_value_to_float(&val[i]);
        }

        /* K_NO_WAIT: a aquisicao nunca fica bloqueada esperando as outras threads */
        if (k_msgq_put(&fila_acel, &s, K_NO_WAIT) != 0) {
            perdidas_acel++;
        }
    }
}

// ----------------------------------------------------
// THREAD 2 — integracao numerica (regra dos trapezios)
// ----------------------------------------------------
void thread_integracao(void *p1, void *p2, void *p3)
{
    struct amostra s;
    struct estado e = {0};
    float offset[3] = {0};
    float a_ant[3] = {0};    /* aceleracao da amostra anterior */
    float v_ant[3] = {0};    /* velocidade da amostra anterior */

    /* 1) Calibracao: media das primeiras N_CALIBRA amostras com a placa parada.
     *    Isso tira o offset do sensor e a gravidade (que aparece no eixo z). */
    gpio_pin_set_dt(&led_cal, 1);
    for (int n = 0; n < N_CALIBRA; n++) {
        k_msgq_get(&fila_acel, &s, K_FOREVER);
        for (int i = 0; i < 3; i++) {
            offset[i] += s.a[i] / N_CALIBRA;
        }
    }
    gpio_pin_set_dt(&led_cal, 0);

    /* 2) Integracao: a cada amostra nova
     *    v[k] = v[k-1] + (a[k-1] + a[k]) / 2 * dt
     *    p[k] = p[k-1] + (v[k-1] + v[k]) / 2 * dt   */
    while (1) {
        k_msgq_get(&fila_acel, &s, K_FOREVER);   /* bloqueia ate chegar amostra */

        for (int i = 0; i < 3; i++) {
            float a = s.a[i] - offset[i];
            if (fabsf(a) < ZONA_MORTA) {
                a = 0.0f;                        /* ruido de placa parada */
            }

            e.a[i] = a;
            e.v[i] = v_ant[i] + (a_ant[i] + a) / 2.0f * DT;
            e.p[i] = e.p[i] + (v_ant[i] + e.v[i]) / 2.0f * DT;

            a_ant[i] = a;
            v_ant[i] = e.v[i];
        }

        if (k_msgq_put(&fila_serial, &e, K_NO_WAIT) != 0) {
            perdidas_serial++;
        }
    }
}

// ----------------------------------------------------
// THREAD 3 — envio pela serial (formato CSV)
// ----------------------------------------------------
void thread_serial(void *p1, void *p2, void *p3)
{
    struct estado e;

    printk("# Calibrando: deixe a placa parada por 1 s (LED vermelho aceso)\n");
    printk("ax,ay,az,vx,vy,vz,px,py,pz\n");

    while (1) {
        k_msgq_get(&fila_serial, &e, K_FOREVER);

        printk("%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
               (double)e.a[0], (double)e.a[1], (double)e.a[2],
               (double)e.v[0], (double)e.v[1], (double)e.v[2],
               (double)e.p[0], (double)e.p[1], (double)e.p[2]);
    }
}

K_THREAD_DEFINE(aquisicao_tid,  STACK_SIZE, thread_aquisicao,  NULL, NULL, NULL,
                PRIO_AQUISICAO,  0, 100);
K_THREAD_DEFINE(integracao_tid, STACK_SIZE, thread_integracao, NULL, NULL, NULL,
                PRIO_INTEGRACAO, 0, 100);
K_THREAD_DEFINE(serial_tid,     STACK_SIZE, thread_serial,     NULL, NULL, NULL,
                PRIO_SERIAL,     0, 100);

int main(void)
{
    if (!device_is_ready(acel)) {
        printk("# Erro: acelerometro nao encontrado\n");
        k_thread_abort(aquisicao_tid);
        return 0;
    }

    gpio_pin_configure_dt(&led_cal, GPIO_OUTPUT_INACTIVE);
    return 0;   /* o main termina; as tres threads continuam rodando */
}

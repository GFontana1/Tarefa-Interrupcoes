#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "ws2812.pio.h"

// Definição dos pinos
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define WS2812_PIN 7
#define LED_RGB_RED_PIN 13
#define LED_RGB_GREEN_PIN 12
#define LED_RGB_BLUE_PIN 13

// Definição do número máximo de LEDs
#define MAX_LEDS 25

// Variáveis globais
volatile int num = 0;  // Número na matriz de LEDs
volatile bool botao_a = false;
volatile bool botao_b = false;

// Variáveis para controle dos LEDs WS2812
static int sm = 0;  // Máquina de estado do PIO
static PIO pio = pio0;  // Bloco PIO
static uint32_t grb[MAX_LEDS];  // Array para armazenar as cores dos LEDs

// Inicializa o LED RGB
void init_rgb_led() {
    gpio_init(LED_RGB_RED_PIN);
    gpio_init(LED_RGB_GREEN_PIN);
    gpio_init(LED_RGB_BLUE_PIN);
    gpio_set_dir(LED_RGB_RED_PIN, GPIO_OUT);
    gpio_set_dir(LED_RGB_GREEN_PIN, GPIO_OUT);
    gpio_set_dir(LED_RGB_BLUE_PIN, GPIO_OUT);
}

// Função para piscar o LED vermelho
void blink_red_led() {
    gpio_put(LED_RGB_RED_PIN, 1);
    sleep_ms(100);
    gpio_put(LED_RGB_RED_PIN, 0);
    sleep_ms(100);
}

// Inicializa os botões com debouncing
void init_buttons() {
    gpio_init(BUTTON_A_PIN);
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_pull_up(BUTTON_B_PIN);
}

// Interrompe os botões
void button_isr(uint gpio, uint32_t events) {
    static uint32_t last_time_a = 0;
    static uint32_t last_time_b = 0;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());

    if (gpio == BUTTON_A_PIN && (current_time - last_time_a > 200)) {  // Debouncing de 200ms
        botao_a = true;
        last_time_a = current_time;
    } else if (gpio == BUTTON_B_PIN && (current_time - last_time_b > 200)) {  // Debouncing de 200ms
        botao_b = true;
        last_time_b = current_time;
    }
}

// Função para converter porcentagens de cores RGB para o formato GRB
static void rgb_to_grb(uint32_t porcentColors[][3]) {
    for (int i = 0; i < MAX_LEDS; i++) {
        uint8_t r = porcentColors[i][0] != 0 ? 255 * (porcentColors[i][0] / 100.0) : 0; // Calcula R
        uint8_t g = porcentColors[i][1] != 0 ? 255 * (porcentColors[i][1] / 100.0) : 0; // Calcula G
        uint8_t b = porcentColors[i][2] != 0 ? 255 * (porcentColors[i][2] / 100.0) : 0; // Calcula B
        grb[i] = (g << 16) | (r << 8) | b;  // Combina em formato GRB
    }
}

// Função para inicializar a matriz de LEDs WS2812
void init_ws2812() {
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);
    for (int i = 0; i < MAX_LEDS; i++) {
        pio_sm_put_blocking(pio, sm, 0);  // Inicializa todos os LEDs como apagados
    }
}

// Função para exibir um número na matriz de LEDs WS2812
void display_num(int num) {
    // Verifica se o número está no intervalo válido (0-9)
    if (num < 0 || num > 9) {
        return;  // Número inválido, não faz nada
    }

    // Definição dos números de 0 a 9 em uma matriz 5x5
    const uint32_t matriz_led[10][25] = {
        {0,1,1,1,0, 0,1,0,1,0, 0,1,0,1,0, 0,1,0,1,0, 0,1,1,1,0}, // 0
        {0,1,1,1,0, 0,0,1,0,0, 0,0,1,0,0, 0,1,1,0,0, 0,0,1,0,0}, // 1
        {0,1,1,1,0, 0,1,0,0,0, 0,1,1,1,0, 0,0,0,1,0, 0,1,1,1,0}, // 2
        {0,1,1,1,0, 0,0,0,1,0, 0,1,1,1,0, 0,0,0,1,0, 0,1,1,1,0}, // 3
        {0,1,0,0,0, 0,0,0,1,0, 0,1,1,1,0, 0,1,0,1,0, 0,1,0,1,0}, // 4
        {0,1,1,1,0, 0,0,0,1,0, 0,1,1,1,0, 0,1,0,0,0, 0,1,1,1,0}, // 5
        {0,1,1,1,0, 0,1,0,1,0, 0,1,1,1,0, 0,1,0,0,0, 0,1,1,1,0}, // 6
        {0,0,0,1,0, 0,0,1,0,0, 0,1,0,0,0, 0,0,0,1,0, 0,1,1,1,0}, // 7
        {0,1,1,1,0, 0,1,0,1,0, 0,1,1,1,0, 0,1,0,1,0, 0,1,1,1,0}, // 8
        {0,1,1,1,0, 0,0,0,1,0, 0,1,1,1,0, 0,1,0,1,0, 0,1,1,1,0}  // 9
    };

    // Converte o número para o formato de cores RGB
    uint32_t porcentColors[MAX_LEDS][3] = {0};  // Inicializa todas as cores como 0%
    for (int i = 0; i < MAX_LEDS; i++) {
        if (matriz_led[num][i]) {
            porcentColors[i][0] = 100;  // Vermelho em 100%
        }
    }

    // Converte as cores para GRB e exibe na matriz de LEDs
    rgb_to_grb(porcentColors);
    for (int i = 0; i < MAX_LEDS; i++) {
        pio_sm_put_blocking(pio, sm, grb[i] << 8u);  // Envia a cor para o LED
    }
    sleep_us(10);  // Pulso de reset
}

int main() {
    int cont_a = 0;  // Conta quantas vezes o botão A foi pressionado
    int cont_b = 0;  // Conta quantas vezes o botão B foi pressionado

    stdio_init_all();
    init_rgb_led();
    init_buttons();
    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &button_isr);
    gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &button_isr);
    init_ws2812();

    while (1) {
        blink_red_led(); // Pisca o LED vermelho 5x por segundo

        // Checa se o botão A foi pressionado
        if (botao_a) {
            cont_a++;
            num = (num + 1) % 10;  // Incrementa o número (0-9)
            printf("Botão A pressionado. Número exibido: %d\n", num);
            printf("Quantidade de vezes que o botão A foi pressionado: %d\n\n", cont_a);
            botao_a = false;
            display_num(num);
        }

        // Checa se o botão B foi pressionado
        if (botao_b) {
            cont_b++;
            num = (num - 1 + 10) % 10;  // Decrementa o número (0-9)
            printf("Botão B pressionado. Número exibido: %d\n", num);
            printf("Quantidade de vezes que o botão B foi pressionado: %d\n\n", cont_b);
            botao_b = false;
            display_num(num);
        }
    }

    return 0;
}
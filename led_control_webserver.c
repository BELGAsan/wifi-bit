#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"

// Configurações de Wi-Fi
#define WIFI_SSID "Labirang"
#define WIFI_PASSWORD "1fp1*007"

// Configurações dos botões
#define BUTTON_A 5
#define BUTTON_B 6

// Variáveis para armazenar estado dos botões
volatile bool button_a_pressed = false;
volatile bool button_b_pressed = false;

// Função para ler a temperatura
float read_temperature()
{
    sleep_ms(200);
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    const float conversion_factor = 3.3f / (1 << 12);
    return 27.0f - ((raw_value * conversion_factor) - 0.706f) / 0.001721f;
}


// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Criar mensagem do botão
    char button_message[100];

    if (button_a_pressed)
    {
        snprintf(button_message, sizeof(button_message), "botao a pressionado");
        button_a_pressed = false;
    }
    else if (button_b_pressed)
    {
        snprintf(button_message, sizeof(button_message), "botao b pressionado");
        button_b_pressed = false;
    }
    else
    {
        snprintf(button_message, sizeof(button_message), "pressione um botao\n");
    }

    // Leitura da temperatura
    float temperature = read_temperature();

   

    // Criar resposta HTML
    char html[1024];
    snprintf(html, sizeof(html),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "\r\n"
             "<!DOCTYPE html>\n"
             "<html>\n"
             "<head>\n"
             "<title>Estado dos Botoes</title>\n"
             "<style>\n"
             "body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }\n"
             "h1 { font-size: 64px; margin-bottom: 30px; }\n"
             ".message { font-size: 48px; margin-top: 30px; color: red; }\n"
             ".temperature { font-size: 48px; margin-top: 30px; color: blue; }\n"
             "</style>\n"
             "<script>\n"
             "function atualizarPagina() { location.reload(); }\n"
             "setInterval(atualizarPagina, 1000);\n"
             "</script>\n"
             "</head>\n"
             "<body>\n"
             "<h1>Estado dos Botoes</h1>\n"
             "<p class=\"message\">%s</p>\n"
             "<p class=\"temperature\">Temperatura: %.2f &deg;C</p>\n"
             "</body>\n"
             "</html>\n",
             button_message, temperature);

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    free(request);
    pbuf_free(p);

    return ERR_OK;
}

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Função de interrupção dos botões
void button_callback(uint gpio, uint32_t events)
{
    if (gpio == BUTTON_A)
    {
        button_a_pressed = true;
    }
    else if (gpio == BUTTON_B)
    {
        button_b_pressed = true;
    }
}

// Função principal
int main()
{
    stdio_init_all();

    // Configuração dos botões
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    // Inicializa o ADC para o sensor de temperatura
    adc_init();
    adc_set_temp_sensor_enabled(true);

    // Inicialização do Wi-Fi
    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    printf("Conectado ao Wi-Fi\n");

    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);

    printf("Servidor ouvindo na porta 80\n");

    while (true)
    {
        cyw43_arch_poll();
    }

    cyw43_arch_deinit();
    return 0;
}
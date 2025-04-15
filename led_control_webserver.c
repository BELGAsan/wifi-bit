#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define WIFI_SSID "Labirang"
#define WIFI_PASSWORD "1fp1*007"

// Função para detectar a direção do joystick
const char* get_direcao(uint16_t x, uint16_t y) {
    const int DEADZONE = 1000;
    int centro = 2048;
    int dx = x - centro;
    int dy = y - centro;

    if (abs(dx) < DEADZONE && abs(dy) < DEADZONE) return "Centro";
    if (dx > DEADZONE && abs(dy) < DEADZONE) return "Leste";
    if (dx < -DEADZONE && abs(dy) < DEADZONE) return "Oeste";
    if (dy > DEADZONE && abs(dx) < DEADZONE) return "Norte";
    if (dy < -DEADZONE && abs(dx) < DEADZONE) return "Sul";
    if (dx > DEADZONE && dy > DEADZONE) return "Nordeste";
    if (dx < -DEADZONE && dy > DEADZONE) return "Noroeste";
    if (dx > DEADZONE && dy < -DEADZONE) return "Sudeste";
    if (dx < -DEADZONE && dy < -DEADZONE) return "Sudoeste";

    return "Nao identificado";
}

// Template HTML sem cores
const char *html_template =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=UTF-8\r\n\r\n"
    "<!DOCTYPE html>"
    "<html><head><meta charset=\"UTF-8\">"
    "<script>setTimeout(() => location.reload(), 1000);</script>"
    "<style>"
    "body{font-family:sans-serif;text-align:center;padding-top:40px}"
    "h2{font-size:24px}"
    ".info{font-size:20px;margin-top:10px}"
    ".direcao{font-size:22px;font-weight:bold;margin:20px 0}"
    ".botao{margin:8px;font-size:18px}"
    "</style></head><body>"
    "<h2>Painel de Controle</h2>"
    "<div class=\"info\">Eixo X : %d</div>"
    "<div class=\"info\">Eixo Y : %d</div>"
    "<div class=\"direcao\"> Local: %s</div>"
    "<div class=\"botao\">Botão A : %s</div>"
    "<div class=\"botao\">Botão B : %s</div>"
    "<p style=\"font-size:14px;\">Atualiza automaticamente a cada 1 segundo.</p>"
    "</body></html>";

// Callback de recepção TCP
static err_t on_tcp_receive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    adc_select_input(1); // X = GPIO27
    uint16_t x = adc_read();

    adc_select_input(0); // Y = GPIO26
    uint16_t y = adc_read();

    const char *direcao = get_direcao(x, y);

    bool btn_a = !gpio_get(5);
    bool btn_b = !gpio_get(6);

    char response[1024];
    snprintf(response, sizeof(response), html_template,
             x, y, direcao,
             btn_a ? "Pressionado" : "Nao pressionado",
             btn_b ? "Pressionado" : "Nao pressionado");

    tcp_write(tpcb, response, strlen(response), TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    pbuf_free(p);
    return ERR_OK;
}

// Callback de nova conexão
static err_t on_tcp_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, on_tcp_receive);
    return ERR_OK;
}

int main() {
    stdio_init_all();

    adc_init();
    adc_gpio_init(26); // Y
    adc_gpio_init(27); // X

    gpio_init(5);
    gpio_set_dir(5, GPIO_IN);
    gpio_pull_up(5);

    gpio_init(6);
    gpio_set_dir(6, GPIO_IN);
    gpio_pull_up(6);

    if (cyw43_arch_init()) {
        printf("Erro ao iniciar Wi-Fi\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar no Wi-Fi\n");
        return -1;
    }

    printf("Conectado!\n");
    if (netif_default)
        printf("IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr));

    struct tcp_pcb *server = tcp_new();
    tcp_bind(server, IP_ADDR_ANY, 80);
    server = tcp_listen(server);
    tcp_accept(server, on_tcp_accept);

    while (true)
        cyw43_arch_poll();

    cyw43_arch_deinit();
    return 0;
}

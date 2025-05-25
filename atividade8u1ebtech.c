#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include <string.h>

#define BOTAO_PIN 5
#define LED_PIN 13
#define DEBOUNCE_DELAY_MS 50

int contador_botoes = 0;
bool botao_esta_pressionado = false;
absolute_time_t ultimo_tempo_botao = 0;

#define WIFI_SSID "iPhone de Leonardo"
#define WIFI_PASS "12345678"

char estado_botao_str[50] = "Não pressionado";
char http_response[2048];
char status_response[100];

void atualizar_estado_botao() {
    bool estado_atual = !gpio_get(BOTAO_PIN); // Inverte porque é pull-up
    
    // Verifica se o estado mudou
    if (estado_atual != botao_esta_pressionado) {
        absolute_time_t agora = get_absolute_time();
        int64_t diferenca = absolute_time_diff_us(ultimo_tempo_botao, agora) / 1000;
        
        // Debounce - só considera após o tempo mínimo
        if (diferenca > DEBOUNCE_DELAY_MS) {
            botao_esta_pressionado = estado_atual;
            ultimo_tempo_botao = agora;
            
            if (botao_esta_pressionado) {
                contador_botoes++;
                snprintf(estado_botao_str, sizeof(estado_botao_str), "Pressionado (%d)", contador_botoes);
                printf("Botão pressionado! Total: %d\n", contador_botoes);
            } else {
                snprintf(estado_botao_str, sizeof(estado_botao_str), "Solto");
            }
        }
    }
}

void criar_resposta_http() {
    const char* estado_led = gpio_get(LED_PIN) ? "Ligado" : "Desligado";
    
    snprintf(http_response, sizeof(http_response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n\r\n"
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "  <meta charset=\"UTF-8\">"
        "  <title>Controle Pico W</title>"
        "  <style>"
        "    body { font-family: Arial, sans-serif; margin: 20px; }"
        "    .status { padding: 10px; margin: 5px; border: 1px solid #ddd; border-radius: 5px; }"
        "    .led-on { background-color: #ffdddd; }"
        "    .led-off { background-color: #ddffdd; }"
        "    a { display: inline-block; padding: 8px 15px; margin: 5px; background: #4CAF50; color: white; text-decoration: none; border-radius: 5px; }"
        "    a:hover { background: #45a049; }"
        "  </style>"
        "  <script>"
        "    function atualizarEstados() {"
        "      fetch('/status')"
        "        .then(response => response.text())"
        "        .then(data => {"
        "          const [led, botao, contador] = data.split(',');"
        "          document.getElementById('led-status').textContent = 'LED: ' + (led === '1' ? 'LIGADO' : 'DESLIGADO');"
        "          document.getElementById('led-status').className = 'status ' + (led === '1' ? 'led-on' : 'led-off');"
        "          document.getElementById('botao-status').textContent = 'BOTÃO: ' + botao;"
        "          document.getElementById('contador').textContent = 'CONTAGEM: ' + contador;"
        "        });"
        "    }"
        "    setInterval(atualizarEstados, 300);"
        "    window.onload = atualizarEstados;"
        "  </script>"
        "</head>"
        "<body>"
        "  <h1>Controle Pico W</h1>"
        "  <div>"
        "    <a href=\"/led/on\">Ligar LED</a>"
        "    <a href=\"/led/off\">Desligar LED</a>"
        "  </div>"
        "  <div id=\"led-status\" class=\"status\">LED: %s</div>"
        "  <div id=\"botao-status\" class=\"status\">BOTÃO: %s</div>"
        "  <div id=\"contador\" class=\"status\">CONTAGEM: %d</div>"
        "</body>"
        "</html>",
        estado_led, estado_botao_str, contador_botoes);
}

void criar_resposta_status(struct tcp_pcb *tpcb) {
    snprintf(status_response, sizeof(status_response), "%d,%s,%d", 
             gpio_get(LED_PIN), estado_botao_str, contador_botoes);
    
    char http_status[256];
    snprintf(http_status, sizeof(http_status),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/plain\r\n"
             "Access-Control-Allow-Origin: *\r\n"
             "\r\n"
             "%s", status_response);
    
    tcp_write(tpcb, http_status, strlen(http_status), TCP_WRITE_FLAG_COPY);
}

static err_t http_callback(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *request = (char *)p->payload;

    if (strstr(request, "GET /led/on")) {
        gpio_put(LED_PIN, 1);
        printf("LED ligado\n");
    } else if (strstr(request, "GET /led/off")) {
        gpio_put(LED_PIN, 0);
        printf("LED desligado\n");
    } else if (strstr(request, "GET /status")) {
        criar_resposta_status(tpcb);
        pbuf_free(p);
        return ERR_OK;
    }

    criar_resposta_http();
    tcp_write(tpcb, http_response, strlen(http_response), TCP_WRITE_FLAG_COPY);
    pbuf_free(p);

    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_callback);
    return ERR_OK;
}

static void iniciar_servidor_http(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB\n");
        return;
    }

    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }

    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP iniciado na porta 80\n");
}

int main() {
    stdio_init_all();
    
    // Configuração do hardware
    gpio_init(BOTAO_PIN);
    gpio_set_dir(BOTAO_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_PIN);
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    printf("\n=== Iniciando Pico W ===\n");
    printf("Aguardando conexão WiFi...\n");

    if (cyw43_arch_init()) {
        printf("Falha ao inicializar WiFi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    
    // Conecta ao WiFi
    int tentativas = 0;
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000) != 0) {
        printf("Falha na conexão WiFi, tentativa %d\n", ++tentativas);
        if (tentativas >= 3) {
            printf("Não foi possível conectar ao WiFi\n");
            return 1;
        }
        sleep_ms(2000);
    }

    printf("Conectado ao WiFi!\n");
    printf("Endereço IP: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    
    iniciar_servidor_http();

    while (true) {
        cyw43_arch_poll();
        atualizar_estado_botao();
        sleep_ms(10); // Reduz o consumo de CPU
    }
    
    cyw43_arch_deinit();
    return 0;
}
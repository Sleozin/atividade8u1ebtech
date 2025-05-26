#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "inc/ssd1306.h"




int contador_botoes = 0;
bool botao_esta_pressionado = false;
absolute_time_t ultimo_tempo_botao = 0;
extern struct render_area area;
// === Buffer de vídeo do OLED (tela de 128 x 64) ===
uint8_t ssd[ssd1306_buffer_length];

// === Área de renderização usada por render_on_display() ===
struct render_area area = {
    .start_column = 0,
    .end_column = ssd1306_width - 1,
    .start_page = 0,
    .end_page = ssd1306_n_pages - 1
};



// Porta padrão para servidor HTTP
#define TCP_PORT 80
#define HTTP_GET "GET"
#define HTTP_RESPONSE_HEADERS "HTTP/1.1 %d OK\nContent-Length: %d\nContent-Type: text/html; charset=utf-8\nConnection: close\n\n"

// Pinos usados no sistema embarcado
#define LED_R 13         // LED vermelho
#define BUZZER_A 21      // Buzzer simples (GPIO)
#define BUZZER_B 10      // Buzzer com PWM (sirene)

static int modo_alerta = 0;  // Ativa o alerta (sirene e pisca)
static int modo_ativo = 0;   // Indica se o sistema está ligado (sem alarme)

ssd1306_t oled;         // Instância do display OLED
uint buzzer_slice;      // Slice do PWM usado pelo buzzer

void Update_display(const char *mensagem) {
    ssd1306_clear_display(ssd);
    render_on_display(ssd, &area);  // Garante que a limpeza seja visível
    sleep_ms(20);                   // Pequeno delay opcional

    char* linha1 = "SISTEMA";
    char* linha2 = "ALARME";
    char linha3[30];

    snprintf(linha3, sizeof(linha3), mensagem);

    // Fonte padrão: 6 px por caractere, altura: 8 px
    int x1 = (128 - strlen(linha1) * 6) / 2;
    int x2 = (128 - strlen(linha2) * 6) / 2;
    int x3 = (128 - strlen(linha3) * 6) / 2;

    // Y = linha × altura da fonte (8 px padrão)
    ssd1306_draw_string(ssd, x1, 0, linha1);    // Linha 0 (Y=0)
    // Linha 1 = em branco (Y=8)
    ssd1306_draw_string(ssd, x2, 16, linha2);   // Linha 2 (Y=16)
    // Linha 3 = em branco (Y=24)

    ssd1306_draw_string(ssd, 0, 56, linha3);  // Y = 32 

    render_on_display(ssd, &area);
}

static int generate_html_content(const char *params, char *result, size_t max_len) {
    // Interpreta comandos da URL (?acao=ligar, ?acao=desligar, ?acao=alerta)
    if (params) {
        if (strcmp(params, "acao=ligar") == 0) {
            modo_alerta = 0;
            modo_ativo = 1;
            pwm_set_enabled(buzzer_slice, false);
            gpio_put(BUZZER_A, 0);
            Update_display("Sistema ativo");
        } else if (strcmp(params, "acao=desligar") == 0) {
            modo_alerta = 0;
            modo_ativo = 0;
            pwm_set_enabled(buzzer_slice, false);
            gpio_put(BUZZER_A, 0);
            Update_display("Sistema em repouso");
        } else if (strcmp(params, "acao=alerta") == 0) {
            modo_alerta = 1;
            modo_ativo = 0;
            Update_display("Evacuar");
        }
    }

    // Retorna HTML formatado com estilo moderno
    return snprintf(result, max_len,
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>Controle Alarme</title>"
        "<style>"
        "body{font-family:Arial,sans-serif;text-align:center;margin:0;padding:20px;background:#f5f5f5;}"
        "h1{color:#333;margin-bottom:30px;}"
        ".btn{display:inline-block;padding:12px 25px;margin:10px;border:none;border-radius:5px;"
        "text-decoration:none;font-size:16px;cursor:pointer;transition:background 0.3s;}"
        ".on{background:#4CAF50;color:white;}"
        ".off{background:#f44336;color:white;}"
        ".alert{background:#FF9800;color:white;}"
        ".btn:hover{opacity:0.8;}"
        ".status{padding:15px;margin-top:20px;background:#ddd;border-radius:5px;display:inline-block;}"
        "</style>"
        "</head>"
        "<body>"
        "<h1>Controle do Alarme</h1>"
        "<a href='?acao=ligar' class='btn on'>Ligar</a>"
        "<a href='?acao=desligar' class='btn off'>Desligar</a>"
        "<a href='?acao=alerta' class='btn alert'>Alerta</a>"
        "<div class='status'>Status: %s</div>"
        "</body>"
        "</html>",
        (modo_alerta ? "ALERTA" : (modo_ativo ? "ATIVO" : "INATIVO"))
    );
}

// Manipula requisições HTTP recebidas
err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(pcb);
        return ERR_OK;
    }

    char headers[512] = {0};
    char response[2048] = {0};
    pbuf_copy_partial(p, headers, p->tot_len < sizeof(headers) ? p->tot_len : sizeof(headers) - 1, 0);

    if (strncmp(headers, HTTP_GET, strlen(HTTP_GET)) == 0) {
        char *uri = strchr(headers, ' ');
        if (uri) uri++; else uri = "";
        char *params = strchr(uri, '?');
        if (params) {
            *params++ = 0;
            char *space = strchr(params, ' ');
            if (space) *space = 0;
        }

        int body_len = generate_html_content(params, response, sizeof(response));
        char header_response[256];
        snprintf(header_response, sizeof(header_response), HTTP_RESPONSE_HEADERS, 200, body_len);

        tcp_write(pcb, header_response, strlen(header_response), 0);
        tcp_write(pcb, response, body_len, 0);
    }

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

// Aceita novas conexões TCP (navegadores)
err_t tcp_server_accept(void *arg, struct tcp_pcb *new_pcb, err_t err) {
    tcp_recv(new_pcb, tcp_server_recv);
    return ERR_OK;
}



int main() {
    stdio_init_all();
    sleep_ms(1000);
    i2c_init(i2c1, 400 * 1000);  // <---I2C primeiro
    gpio_set_function(14, GPIO_FUNC_I2C);
    gpio_set_function(15, GPIO_FUNC_I2C);
    gpio_pull_up(14);
    gpio_pull_up(15);
    ssd1306_init();
    calculate_render_area_buffer_length(&area);
    Update_display("---INICIO---");

    // Inicializa GPIOs
    gpio_init(LED_R); gpio_set_dir(LED_R, GPIO_OUT);
    gpio_init(BUZZER_A); gpio_set_dir(BUZZER_A, GPIO_OUT);

    // Configura PWM para o BUZZER_B
    gpio_set_function(BUZZER_B, GPIO_FUNC_PWM);
    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_B);
    pwm_set_clkdiv(buzzer_slice, 4.f);
    pwm_set_wrap(buzzer_slice, 62500);
    pwm_set_gpio_level(BUZZER_B, 31250);
    pwm_set_enabled(buzzer_slice, false);


    // Inicializa Wi-Fi como Access Point
    if (cyw43_arch_init()) return 1;
    const char *ap_name = "rasp_wifi";
    const char *password = "12345678";
    cyw43_arch_enable_ap_mode(ap_name, password, CYW43_AUTH_WPA2_AES_PSK);

    // Configuração de IP local
    ip4_addr_t ip, mask;
    IP4_ADDR(&ip, 192, 168, 4, 1);
    IP4_ADDR(&mask, 255, 255, 255, 0);

    // Inicializa servidor DHCP e DNS Hijack
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &ip, &mask);
    dns_server_t dns_server;
    dns_server_init(&dns_server, &ip);

    // Inicia servidor HTTP
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb || tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT) != ERR_OK) return 1;
    pcb = tcp_listen_with_backlog(pcb, 1);
    tcp_accept(pcb, tcp_server_accept);

    // Loop principal: controla LED e buzzer com base nos modos
    bool led_on = false;
    bool buzzer_toggle = false;

    while (true) {
        if (modo_alerta) {
            gpio_put(LED_R, led_on);
            pwm_set_enabled(buzzer_slice, true);
            pwm_set_wrap(buzzer_slice, buzzer_toggle ? 50000 : 25000);  // alterna tons da sirene
        } else if (modo_ativo) {
            gpio_put(LED_R, led_on);
            pwm_set_enabled(buzzer_slice, false);
        } else {
            gpio_put(LED_R, 0);
            pwm_set_enabled(buzzer_slice, false);
        }

        led_on = !led_on;
        buzzer_toggle = !buzzer_toggle;
        sleep_ms(500);
    }

    return 0;
}
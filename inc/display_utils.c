#include <stdio.h>
#include <stdint.h>

#include "display_utils.h"
#include "big_string_drawer.h"

void mostrar_valor_grande(uint8_t *ssd, float valor, int y) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%+.1foC", valor);
    draw_big_string_aligned_right(ssd, y, buffer);
}

void mostrar_status_grande(uint8_t *ssd, char *valor, int y) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%c", valor);
    draw_big_string_aligned_right(ssd, y, buffer);
}
#ifndef TEXT_H
#define TEXT_H
#include <stdint.h>

#define FONT_W 12
#define FONT_H 24

/* font : 256 caracteres de FONT_W x FONT_H pixels ARGB8888, 0 = transparent ; buffer de pitch buffer_w. */
int draw_text_ss3x3(uint32_t *buffer, int buffer_w, int buffer_h, int x, int y, int char_h,
                    const uint32_t *font, const char *format, ...);

/* Construit une fonte 12x24 a partir de la fonte de debug SDL (ASCII 32..126). A liberer par free(). */
uint32_t *font_build(void);

#endif

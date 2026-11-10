#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "text.h"

/*
 * Fonte de référence :
 *
 *   256 caractères
 *   12 pixels de large
 *   24 pixels de haut
 *
 * Chaque pixel est en ARGB8888.
 * Un pixel égal à 0 est transparent.
 * Tout pixel dont A != 0 est considéré comme présent.
 *
 * Organisation :
 *
 *   font[c * 12 * 24 + y * 12 + x]
 *
 * ou, si on préfère :
 *
 *   uint32_t font[256][24][12];
 */


#define FONT_PIXELS_PER_CHAR (FONT_W * FONT_H)

/*
 * Extraction des composantes ARGB8888.
 *
 * On suppose ici le format logique :
 *
 *   0xAARRGGBB
 */
#define PIXEL_A(p) ((uint8_t)((p) >> 24))
#define PIXEL_R(p) ((uint8_t)((p) >> 16))
#define PIXEL_G(p) ((uint8_t)((p) >>  8))
#define PIXEL_B(p) ((uint8_t)((p) >>  0))

#define ARGB8888(a, r, g, b) \
    (((uint32_t)(a) << 24) | \
     ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) <<  8) | \
     ((uint32_t)(b) <<  0))


/*
 * Lit un pixel de la fonte.
 *
 * cx : code ASCII/Latin-1 du caractère
 * sx : 0..11
 * sy : 0..23
 */
static inline uint32_t
font_pixel(const uint32_t *font, unsigned int cx,
           unsigned int sx, unsigned int sy)
{
    return font[cx * FONT_PIXELS_PER_CHAR + sy * FONT_W + sx];
}


/*
 * Affiche une chaîne dans un buffer ARGB8888.
 *
 * buffer
 *   Buffer de destination.
 *
 * buffer_w / buffer_h
 *   Dimensions du buffer en pixels.
 *
 * x / y
 *   Position du coin supérieur gauche du texte.
 *
 * char_h
 *   Hauteur destination d'un caractère.
 *
 * font
 *   Fonte 256 caractères de 12x24 pixels en ARGB8888.
 *
 * format
 *   Comme pour printf().
 *
 * ...
 *   Arguments du format.
 *
 * Retour :
 *   Nombre de pixels horizontaux occupés par le texte.
 *
 * Remarque :
 *   La largeur d'un caractère est calculée par :
 *
 *       char_w = char_h * 12 / 24
 *              = char_h / 2
 *
 *   La largeur minimale est donc 1 pixel.
 */
int
draw_text_ss3x3(uint32_t       *buffer,
                int             buffer_w,
                int             buffer_h,
                int             x,
                int             y,
                int             char_h,
                const uint32_t *font,
                const char     *format, ...)
{
    if (!buffer || !font || !format)
        return 0;

    /*
     * Avec un ratio 12/24, une largeur >= 1 implique
     * une hauteur >= 2.
     */
    if (char_h < 2)
        char_h = 2;

    /*
     * Conservation du ratio 12:24.
     *
     * On arrondit à l'entier le plus proche.
     * Pour char_h >= 2, char_w >= 1.
     */
    int char_w = (char_h * FONT_W + FONT_H / 2) / FONT_H;

    if (char_w < 1)
        char_w = 1;

    /*
     * Première passe : déterminer la taille nécessaire
     * à la chaîne formatée.
     */
    va_list ap;
    va_start(ap, format);

    va_list ap_copy;
    va_copy(ap_copy, ap);

    int len = vsnprintf(NULL, 0, format, ap_copy);

    va_end(ap_copy);

    if (len < 0) {
        va_end(ap);
        return 0;
    }

    /*
     * +1 pour le '\0'.
     */
    char *text = (char *)malloc((size_t)len + 1);

    if (!text) {
        va_end(ap);
        return 0;
    }

    vsnprintf(text, (size_t)len + 1, format, ap);

    va_end(ap);

    /*
     * Le texte est maintenant disponible sous forme
     * d'une chaîne classique.
     *
     * On traite notamment :
     *   '\n'
     *   '\r'
     *   '\t'
     */
    int pen_x = x;
    int pen_y = y;

    const int line_h = char_h;

    for (int i = 0; text[i] != '\0'; ++i) {

        unsigned char c = (unsigned char)text[i];

        /*
         * Nouvelle ligne.
         */
        if (c == '\n') {
            pen_x = x;
            pen_y += line_h;
            continue;
        }

        /*
         * Retour chariot.
         */
        if (c == '\r') {
            pen_x = x;
            continue;
        }

        /*
         * Tabulation : 4 caractères.
         */
        if (c == '\t') {
            pen_x += char_w * 4;
            continue;
        }

        /*
         * Pour chaque pixel destination du caractère.
         */
        for (int dy = 0; dy < char_h; ++dy) {

            int dst_y = pen_y + dy;

            if (dst_y < 0 || dst_y >= buffer_h)
                continue;

            /*
             * Coordonnée verticale dans la fonte.
             *
             * Le pixel destination représente une portion
             * de l'espace source [0..24[.
             *
             * On ajoute 0.5 afin d'échantillonner au centre
             * des pixels destination.
             */
            float syf =
                ((float)dy + 0.5f) * (float)FONT_H / (float)char_h;

            /*
             * Pour le supersampling, on prend 3 positions
             * autour de ce point.
             */
            for (int dx = 0; dx < char_w; ++dx) {

                int dst_x = pen_x + dx;

                if (dst_x < 0 || dst_x >= buffer_w)
                    continue;

                /*
                 * Centre du pixel destination dans l'espace
                 * de la fonte.
                 */
                float sxf =
                    ((float)dx + 0.5f) *
                    (float)FONT_W / (float)char_w;

                /*
                 * Les 9 échantillons 3x3.
                 *
                 * Le décalage est exprimé en coordonnées
                 * source. On place les samples à ±1/3 pixel
                 * autour du centre.
                 */
                static const float offsets[3] = {
                    -1.0f / 3.0f,
                     0.0f,
                     1.0f / 3.0f
                };

                unsigned int sum_r = 0;
                unsigned int sum_g = 0;
                unsigned int sum_b = 0;
                unsigned int count = 0;

                for (int syi = 0; syi < 3; ++syi) {
                    float sample_y = syf + offsets[syi];

                    int sy = (int)sample_y;

                    if (sy < 0)
                        sy = 0;
                    else if (sy >= FONT_H)
                        sy = FONT_H - 1;

                    for (int sxi = 0; sxi < 3; ++sxi) {
                        float sample_x = sxf + offsets[sxi];

                        int sx = (int)sample_x;

                        if (sx < 0)
                            sx = 0;
                        else if (sx >= FONT_W)
                            sx = FONT_W - 1;

                        uint32_t p = font_pixel(font, c, sx, sy);

                        /*
                         * 0 = transparent.
                         *
                         * Toute autre valeur est considérée
                         * comme un pixel de fonte valide.
                         */
                        if (PIXEL_A(p) != 0) {
                            sum_r += PIXEL_R(p);
                            sum_g += PIXEL_G(p);
                            sum_b += PIXEL_B(p);
                            ++count;
                        }
                    }
                }

                /*
                 * Aucun sample opaque :
                 * on ne touche pas au buffer.
                 */
                if (count == 0)
                    continue;

                /*
                 * Moyenne des samples présents.
                 *
                 * Alpha forcé à 255 :
                 * SDL_PIXELFORMAT_ARGB8888 aime parfois
                 * faire des choses amusantes avec l'alpha.
                 */
                uint8_t r = (uint8_t)(sum_r / count);
                uint8_t g = (uint8_t)(sum_g / count);
                uint8_t b = (uint8_t)(sum_b / count);

                buffer[(size_t)dst_y * (size_t)buffer_w + dst_x] =
                    ARGB8888(255, r, g, b);
            }
        }

        /*
         * Caractère suivant.
         */
        pen_x += char_w;
    }

    /*
     * Largeur horizontale occupée par le texte sur la dernière
     * ligne. Pour un printf-like, ce retour est surtout pratique
     * pour savoir où se trouve le curseur.
     */
    int width = pen_x - x;

    free(text);

    return width;
}



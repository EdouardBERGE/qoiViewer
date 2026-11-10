#include <math.h>
#include <stdint.h>
#include <limits.h>
#include "draw_line.h"
#include "draw_triangle.h"

#define TRI_PI 3.14159265358979323846

/* double -> int arrondi et sature, pour ne jamais deborder meme avec ray ~ INT_MAX */
#if 0
static int round_sat(double v)
{
    if (v <= (double)INT_MIN) return INT_MIN;
    if (v >= (double)INT_MAX) return INT_MAX;
    return (int)lround(v);
}
#endif
void draw_triangle(unsigned int *buffer, unsigned int width, unsigned int height,
                   unsigned int color, int cx, int cy, int ray, double angle)
{
    if (!buffer || width == 0 || height == 0 || ray < 0 || !isfinite(angle))
        return;

    /* Clipping total : le cercle circonscrit est hors du buffer, donc le triangle aussi */
    const int64_t X = cx, Y = cy, R = ray;
    if (X + R < 0 || Y + R < 0 || X - R >= (int64_t)width || Y - R >= (int64_t)height)
        return;

    int px[3], py[3];
    for (int i = 0; i < 3; i++) {
        double a = angle - TRI_PI / 2.0 + i * (2.0 * TRI_PI / 3.0);
        px[i] = round_sat((double)cx + (double)ray * cos(a));
        py[i] = round_sat((double)cy + (double)ray * sin(a));
    }

    /* Le clipping de chaque arete est gere par draw_line */
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        draw_line(buffer, width, height, color, px[i], py[i], px[j], py[j]);
    }
}

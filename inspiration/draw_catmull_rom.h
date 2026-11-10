#ifndef DRAW_CATMULL_ROM_H
#define DRAW_CATMULL_ROM_H

/* Spline de Catmull-Rom (uniforme, courbe ouverte) passant par les nb points
   (x[i], y[i]). Les extremites sont traitees en dupliquant le premier et le
   dernier point. nb == 1 : un pixel ; nb == 2 : un segment. */
void draw_catmull_rom(unsigned int *buffer, unsigned int width, unsigned int height,
                      unsigned int color, int *x, int *y, unsigned int nb);

#endif

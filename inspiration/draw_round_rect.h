#ifndef DRAW_ROUND_RECT_H
#define DRAW_ROUND_RECT_H

/* Contour d'un rectangle a coins arrondis.
   (x,y)        : pixel du coin haut-gauche
   rect_w/rect_h: dimensions en pixels (le rectangle couvre x..x+rect_w-1, y..y+rect_h-1)
   radius       : rayon d'arrondi en pixels. Si radius <= 0, ou si rect_w < 2*radius,
                  ou si rect_h < 2*radius, le rectangle n'est pas arrondi. */
void draw_round_rect(unsigned int *buffer, unsigned int width, unsigned int height,
                     unsigned int color, int x, int y, int rect_w, int rect_h, int radius);

#endif

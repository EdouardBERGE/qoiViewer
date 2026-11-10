#ifndef DRAW_TRIANGLE_H
#define DRAW_TRIANGLE_H

/* Triangle equilateral (contour) inscrit dans le cercle de centre (cx,cy) et de
   rayon ray (distance centre -> sommet).
   angle : rotation en radians. 0 = un sommet pointe vers le haut de l'ecran ;
           les valeurs positives tournent dans le sens horaire a l'ecran (axe y vers le bas). */
void draw_triangle(unsigned int *buffer, unsigned int width, unsigned int height,
                   unsigned int color, int cx, int cy, int ray, double angle);

#endif

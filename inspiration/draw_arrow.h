#ifndef DRAW_ARROW_H
#define DRAW_ARROW_H

/* Fleche : un segment (x1,y1)-(x2,y2) avec, a chaque extremite, une pointe optionnelle
   (head1 : pointe en (x1,y1) ; head2 : pointe en (x2,y2) ; 0 = absente, != 0 = presente).
   La pointe est un "V" ouvert : deux barbes partant de l'extremite, chacune faisant
   30 degres avec le segment (ouverture totale 60 degres).
   adj : longueur du COTE ADJACENT de cet angle, c'est-a-dire la distance mesuree le long
   du segment entre l'extremite et le pied des barbes (en pixels). Chaque barbe mesure
   donc adj / cos(30) ~ 1.155 * adj et s'ecarte llateralement de adj * tan(30) ~ 0.577 * adj.
   adj <= 0 : pas de pointe. Segment de longueur nulle : un seul pixel, sans pointe. */
void draw_arrow(unsigned int *buffer, unsigned int width, unsigned int height,
                unsigned int color, int x1, int y1, int x2, int y2,
                int head1, int head2, int adj);

#endif

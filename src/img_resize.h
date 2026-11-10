#ifndef IMG_RESIZE_H
#define IMG_RESIZE_H

/* Redimensionne un buffer 32 bits (4 canaux de 8 bits) ; resultat alloue par malloc. */
unsigned int *img_resize(const unsigned int *buffer, int width, int height, int owidth, int oheight);

#endif

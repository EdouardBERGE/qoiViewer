#ifndef IMG_SHARPEN_H
#define IMG_SHARPEN_H

int  needsSharpening(unsigned int *buffer, int width, int height);
void applySharpen(unsigned int *buffer, int width, int height, int w);
void applyVerticalSharpen(unsigned int *buffer, int width, int height);
void applyHorizontalSharpen(unsigned int *buffer, int width, int height);

#endif

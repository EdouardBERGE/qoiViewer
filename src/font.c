/* Fonte 12x24 pour draw_text_ss3x3, generee a partir de la fonte 8x8 de SDL_RenderDebugText :
   glyphe etire verticalement (8x16) et centre dans la cellule 12x24. */
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
#include "text.h"

uint32_t *font_build(void)
{
	const int W = 256 * FONT_W;
	uint32_t *font = calloc(256 * FONT_W * FONT_H, sizeof *font);
	SDL_Surface *s = SDL_CreateSurface(W, FONT_H, SDL_PIXELFORMAT_ARGB8888);
	SDL_Renderer *r = s ? SDL_CreateSoftwareRenderer(s) : NULL;

	if (!font || !r) {
		fprintf(stderr, "font_build [%s]\n", SDL_GetError());
		SDL_DestroySurface(s);
		return font;
	}
	SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
	SDL_RenderClear(r);
	SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
	SDL_SetRenderScale(r, 1.0f, 2.0f);
	for (int c = 33; c < 127; c++) {
		char str[2] = { (char)c, 0 };
		SDL_RenderDebugText(r, (float)(c * FONT_W + (FONT_W - 8) / 2), (FONT_H - 16) / 4.0f, str);
	}
	SDL_FlushRenderer(r);

	SDL_LockSurface(s);
	for (int c = 0; c < 256; c++)
		for (int y = 0; y < FONT_H; y++)
			for (int x = 0; x < FONT_W; x++) {
				uint32_t p = ((uint32_t *)((Uint8 *)s->pixels + (size_t)y * s->pitch))[c * FONT_W + x];
				font[(c * FONT_H + y) * FONT_W + x] = (p >> 24) ? p : 0;
			}
	SDL_UnlockSurface(s);
	SDL_DestroyRenderer(r);
	SDL_DestroySurface(s);
	return font;
}

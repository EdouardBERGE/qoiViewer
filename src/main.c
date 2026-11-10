/*
 * qoiviewer - visionneuse d'images QOI (SDL3)
 *
 * Liste les fichiers *.qoi (casse ignoree) du repertoire courant, affiche le premier.
 *   Fleches gauche/haut  : image precedente     Fleches droite/bas : image suivante
 *   ALT+ENTER            : bascule plein ecran  ESC                : quitter
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <SDL3/SDL.h>

#include "qoi.h"
#include "img_resize.h"

#define REPEAT_DELAY    300   /* ms avant le debut de la repetition */
#define REPEAT_INTERVAL 300   /* ms entre deux repetitions          */
#define CHECKER_SIZE    8     /* damier de fond pour la transparence */
#define CHECKER_A       0xFF666666u
#define CHECKER_B       0xFF999999u
#define BACKGROUND      0xFF000000u

typedef struct {
	unsigned int *pixels;   /* ARGB 0xAARRGGBB premultiplie */
	int w, h;
	int has_alpha;
	int error;
} Image;

typedef struct {
	SDL_Window  *win;
	char       **files;
	int          nfiles;
	int          current;
	int          fullscreen;
	Image        img;
	SDL_Surface *view;      /* image mise a l'echelle et composee, prete a blitter */
	int          view_bw, view_bh; /* boite cible ayant servi a construire view */
} Viewer;

/* ------------------------------------------------------------------------- */
/* Tri "naturel" insensible a la casse : img2 < img10                        */
/* ------------------------------------------------------------------------- */
static int natural_cmp(const char *a, const char *b)
{
	while (*a && *b) {
		if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
			while (*a == '0') a++;
			while (*b == '0') b++;
			const char *sa = a, *sb = b;
			while (isdigit((unsigned char)*a)) a++;
			while (isdigit((unsigned char)*b)) b++;
			size_t la = a - sa, lb = b - sb;
			if (la != lb) return la < lb ? -1 : 1;
			int c = strncmp(sa, sb, la);
			if (c) return c;
		} else {
			int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
			if (ca != cb) return ca < cb ? -1 : 1;
			a++; b++;
		}
	}
	return (*a != 0) - (*b != 0);
}

static int qsort_cmp(const void *pa, const void *pb)
{
	return natural_cmp(*(const char * const *)pa, *(const char * const *)pb);
}

static int list_files(Viewer *v)
{
	int count = 0;
	char **glob = SDL_GlobDirectory(".", "*.qoi", SDL_GLOB_CASEINSENSITIVE, &count);
	if (!glob) {
		fprintf(stderr, "Lecture du repertoire impossible [%s]\n", SDL_GetError());
		return 0;
	}
	v->files = malloc((count ? count : 1) * sizeof *v->files);
	v->nfiles = 0;
	for (int i = 0; i < count; i++) {
		SDL_PathInfo info;
		if (SDL_GetPathInfo(glob[i], &info) && info.type == SDL_PATHTYPE_FILE)
			v->files[v->nfiles++] = strdup(glob[i]);
	}
	SDL_free(glob);
	qsort(v->files, v->nfiles, sizeof *v->files, qsort_cmp);
	return v->nfiles;
}

/* ------------------------------------------------------------------------- */
/* Chargement                                                                */
/* ------------------------------------------------------------------------- */
static void free_image(Image *img)
{
	free(img->pixels);
	memset(img, 0, sizeof *img);
}

static void load_image(Image *img, const char *path)
{
	qoi_desc desc;
	unsigned char *rgba;

	free_image(img);
	rgba = qoi_read(path, &desc, 4);
	if (!rgba) {
		fprintf(stderr, "Impossible de decoder [%s]\n", path);
		img->w = 320; img->h = 200; img->error = 1;
		img->pixels = malloc((size_t)img->w * img->h * sizeof *img->pixels);
		for (int i = 0; i < img->w * img->h; i++) img->pixels[i] = 0xFF600000u;
		return;
	}
	img->w = desc.width;
	img->h = desc.height;
	img->pixels = malloc((size_t)img->w * img->h * sizeof *img->pixels);
	const size_t n = (size_t)img->w * img->h;
	for (size_t i = 0; i < n; i++) {
		unsigned int r = rgba[i * 4], g = rgba[i * 4 + 1], b = rgba[i * 4 + 2], a = rgba[i * 4 + 3];
		if (a != 255) {
			img->has_alpha = 1;
			/* premultiplication : le redimensionnement ne bave pas la couleur des pixels transparents */
			r = (r * a + 127) / 255; g = (g * a + 127) / 255; b = (b * a + 127) / 255;
		}
		img->pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
	}
	free(rgba);
}

/* ------------------------------------------------------------------------- */
/* Geometrie                                                                 */
/* ------------------------------------------------------------------------- */

/* Taille d'affichage de l'image dans une boite bw x bh : reduction seulement, ratio conserve. */
static void fit_size(int w, int h, int bw, int bh, int *ow, int *oh)
{
	if (w <= bw && h <= bh) { *ow = w; *oh = h; return; }
	if ((long long)w * bh > (long long)h * bw) {
		*ow = bw;
		*oh = (int)(((long long)h * bw + w / 2) / w);
	} else {
		*oh = bh;
		*ow = (int)(((long long)w * bh + h / 2) / h);
	}
	if (*ow < 1) *ow = 1;
	if (*oh < 1) *oh = 1;
}

/* Zone maximale disponible pour le contenu d'une fenetre decoree. */
static void windowed_box(Viewer *v, SDL_Rect *usable, int *bw, int *bh)
{
	int top = 0, left = 0, bottom = 0, right = 0;
	SDL_DisplayID d = SDL_GetDisplayForWindow(v->win);

	if (!SDL_GetDisplayUsableBounds(d, usable)) {
		usable->x = usable->y = 0; usable->w = 1024; usable->h = 768;
	}
	if (!SDL_GetWindowBordersSize(v->win, &top, &left, &bottom, &right) || (top | left | bottom | right) == 0) {
		top = 32; left = right = bottom = 4; /* estimation si le WM ne dit rien */
	}
	*bw = usable->w - left - right;
	*bh = usable->h - top - bottom;
	if (*bw < 64) *bw = 64;
	if (*bh < 64) *bh = 64;
}

/* ------------------------------------------------------------------------- */
/* Construction de la vue (mise a l'echelle + composition sur damier)        */
/* ------------------------------------------------------------------------- */
static void build_view(Viewer *v, int bw, int bh)
{
	int ow, oh;
	unsigned int *scaled;

	if (v->view && v->view_bw == bw && v->view_bh == bh) return;
	SDL_DestroySurface(v->view);
	v->view = NULL;

	fit_size(v->img.w, v->img.h, bw, bh, &ow, &oh);
	if (ow == v->img.w && oh == v->img.h) {
		scaled = v->img.pixels;
	} else if (!(scaled = img_resize(v->img.pixels, v->img.w, v->img.h, ow, oh))) {
		fprintf(stderr, "Echec du redimensionnement %dx%d -> %dx%d\n", v->img.w, v->img.h, ow, oh);
		scaled = v->img.pixels; ow = v->img.w; oh = v->img.h;
	}

	v->view = SDL_CreateSurface(ow, oh, SDL_PIXELFORMAT_ARGB8888);
	if (v->view) {
		SDL_LockSurface(v->view);
		for (int y = 0; y < oh; y++) {
			unsigned int *dst = (unsigned int *)((Uint8 *)v->view->pixels + (size_t)y * v->view->pitch);
			const unsigned int *src = scaled + (size_t)y * ow;
			if (!v->img.has_alpha) {
				memcpy(dst, src, ow * sizeof *dst);
				continue;
			}
			for (int x = 0; x < ow; x++) {
				const unsigned int p = src[x], a = p >> 24, ia = 255 - a;
				const unsigned int bg = ((x / CHECKER_SIZE + y / CHECKER_SIZE) & 1) ? CHECKER_A : CHECKER_B;
				unsigned int out = 0xFF000000u;
				for (int s = 0; s < 24; s += 8) {
					unsigned int c = ((p >> s) & 255) + (((bg >> s) & 255) * ia + 127) / 255;
					out |= (c > 255 ? 255 : c) << s;
				}
				dst[x] = out;
			}
		}
		SDL_UnlockSurface(v->view);
	}
	if (scaled != v->img.pixels) free(scaled);
	v->view_bw = bw;
	v->view_bh = bh;
}

/* ------------------------------------------------------------------------- */
/* Affichage                                                                 */
/* ------------------------------------------------------------------------- */
static void render(Viewer *v)
{
	SDL_Surface *screen = SDL_GetWindowSurface(v->win);
	int bw, bh;
	SDL_Rect usable;

	if (!screen) return;
	if (v->fullscreen) { bw = screen->w; bh = screen->h; }
	else windowed_box(v, &usable, &bw, &bh);
	build_view(v, bw, bh);

	SDL_FillSurfaceRect(screen, NULL, BACKGROUND);
	if (v->view) {
		SDL_Rect r = { (screen->w - v->view->w) / 2, (screen->h - v->view->h) / 2, v->view->w, v->view->h };
		SDL_BlitSurface(v->view, NULL, screen, &r);
	}
	SDL_UpdateWindowSurface(v->win);
}

static void update_title(Viewer *v)
{
	char title[1024];
	const char *name = v->files[v->current];
	if (v->img.error)
		snprintf(title, sizeof title, "%s [%d/%d] - ERREUR DE DECODAGE", name, v->current + 1, v->nfiles);
	else if (v->view && (v->view->w != v->img.w || v->view->h != v->img.h))
		snprintf(title, sizeof title, "%s [%d/%d] - %dx%d (%d%%)", name, v->current + 1, v->nfiles,
		         v->img.w, v->img.h, (int)((100LL * v->view->w + v->img.w / 2) / v->img.w));
	else
		snprintf(title, sizeof title, "%s [%d/%d] - %dx%d", name, v->current + 1, v->nfiles, v->img.w, v->img.h);
	SDL_SetWindowTitle(v->win, title);
}

/* Ajuste la fenetre a la vue courante et la garde dans la zone utilisable de l'ecran. */
static void fit_window(Viewer *v)
{
	SDL_Rect usable;
	int bw, bh, x, y, top = 0, left = 0, bottom = 0, right = 0;

	if (v->fullscreen) return;
	windowed_box(v, &usable, &bw, &bh);
	build_view(v, bw, bh);
	if (!v->view) return;

	SDL_SetWindowSize(v->win, v->view->w, v->view->h);
	SDL_SyncWindow(v->win);

	SDL_GetWindowBordersSize(v->win, &top, &left, &bottom, &right);
	SDL_GetWindowPosition(v->win, &x, &y);
	const int nx0 = x, ny0 = y;
	if (x + v->view->w + right > usable.x + usable.w) x = usable.x + usable.w - v->view->w - right;
	if (y + v->view->h + bottom > usable.y + usable.h) y = usable.y + usable.h - v->view->h - bottom;
	if (x - left < usable.x) x = usable.x + left;
	if (y - top < usable.y) y = usable.y + top;
	if (x != nx0 || y != ny0) {
		SDL_SetWindowPosition(v->win, x, y);
		SDL_SyncWindow(v->win);
	}
}

static void show(Viewer *v, int index)
{
	v->current = (index % v->nfiles + v->nfiles) % v->nfiles;
	load_image(&v->img, v->files[v->current]);
	SDL_DestroySurface(v->view);
	v->view = NULL;
	fit_window(v);
	render(v);
	update_title(v);
}

static void toggle_fullscreen(Viewer *v)
{
	v->fullscreen = !v->fullscreen;
	if (!SDL_SetWindowFullscreen(v->win, v->fullscreen)) {
		fprintf(stderr, "Bascule plein ecran impossible [%s]\n", SDL_GetError());
		v->fullscreen = !v->fullscreen;
		return;
	}
	SDL_SyncWindow(v->win);
	if (!v->fullscreen) fit_window(v);
	render(v);
	update_title(v);
}

/* ------------------------------------------------------------------------- */
static int nav_dir(SDL_Scancode sc)
{
	switch (sc) {
		case SDL_SCANCODE_LEFT:  case SDL_SCANCODE_UP:   return -1;
		case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_DOWN: return  1;
		default: return 0;
	}
}

int main(int argc, char **argv)
{
	Viewer v;
	SDL_Scancode held = SDL_SCANCODE_UNKNOWN;
	Uint64 next_repeat = 0;
	int running = 1;

	(void)argc; (void)argv;
	memset(&v, 0, sizeof v);

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "SDL_Init error [%s]\n", SDL_GetError());
		return 1;
	}
	if (!list_files(&v)) {
		fprintf(stderr, "Aucun fichier .qoi dans le repertoire courant\n");
		SDL_Quit();
		return 1;
	}
	if (!(v.win = SDL_CreateWindow("qoiviewer", 320, 200, 0))) {
		fprintf(stderr, "SDL_CreateWindow error [%s]\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	show(&v, 0);

	while (running) {
		SDL_Event ev;
		Sint32 timeout = -1;

		if (held != SDL_SCANCODE_UNKNOWN) {
			Uint64 now = SDL_GetTicks();
			timeout = next_repeat > now ? (Sint32)(next_repeat - now) : 0;
		}

		if (SDL_WaitEventTimeout(&ev, timeout)) {
			switch (ev.type) {
				case SDL_EVENT_QUIT:
					running = 0;
					break;
				case SDL_EVENT_KEY_DOWN:
					if (ev.key.repeat) break; /* repetition geree par nos soins */
					if (ev.key.scancode == SDL_SCANCODE_ESCAPE) {
						running = 0;
					} else if ((ev.key.scancode == SDL_SCANCODE_RETURN || ev.key.scancode == SDL_SCANCODE_KP_ENTER)
					           && (ev.key.mod & SDL_KMOD_ALT)) {
						toggle_fullscreen(&v);
					} else if (nav_dir(ev.key.scancode)) {
						held = ev.key.scancode;
						show(&v, v.current + nav_dir(held));
						next_repeat = SDL_GetTicks() + REPEAT_DELAY;
					}
					break;
				case SDL_EVENT_KEY_UP:
					if (ev.key.scancode == held) held = SDL_SCANCODE_UNKNOWN;
					break;
				case SDL_EVENT_WINDOW_FOCUS_LOST:
					held = SDL_SCANCODE_UNKNOWN;
					break;
				case SDL_EVENT_WINDOW_EXPOSED:
				case SDL_EVENT_WINDOW_RESIZED:
				case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
				case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
					render(&v);
					break;
				default:
					break;
			}
		}

		if (held != SDL_SCANCODE_UNKNOWN && SDL_GetTicks() >= next_repeat) {
			show(&v, v.current + nav_dir(held));
			/* compte a partir de la fin du chargement : pas d'accumulation sur les grosses images */
			next_repeat = SDL_GetTicks() + REPEAT_INTERVAL;
		}
	}

	free_image(&v.img);
	SDL_DestroySurface(v.view);
	for (int i = 0; i < v.nfiles; i++) free(v.files[i]);
	free(v.files);
	SDL_DestroyWindow(v.win);
	SDL_Quit();
	return 0;
}

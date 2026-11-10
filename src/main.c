/*
 * qoiviewer - visionneuse d'images QOI (SDL3)
 *
 * Liste les fichiers *.qoi (casse ignoree) du repertoire courant, affiche le premier.
 * Touches : voir help_lines[] (H pour l'afficher).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <SDL3/SDL.h>

#include "qoi.h"
#include "img_resize.h"
#include "text.h"

#define REPEAT_DELAY    300   /* ms avant le debut de la repetition */
#define REPEAT_NAV      40    /* ms entre deux repetitions en navigation (25 images/s) */
#define REPEAT_ZOOM     300   /* ms entre deux repetitions du zoom  */
#define MIN_WIN         200   /* taille minimale de la fenetre (largeur et hauteur) */
#define ZOOM_STEP       1.25
#define ZOOM_MAX        32.0
#define CHECKER_SIZE    8     /* damier de fond pour la transparence */
#define CHECKER_A       0xFF666666u
#define CHECKER_B       0xFF999999u
#define BACKGROUND      0xFF000000u
#define HELP_BG         0xD0202020u

static const char *help_lines[] = {
	"qoiviewer - aide",
	"",
	"Fleche gauche/haut   image precedente",
	"Fleche droite/bas    image suivante",
	"Pave num. +          zoom avant",
	"Pave num. -          zoom arriere",
	"Pave num. *          zoom ajuste",
	"K                    zoom 100% (1:1)",
	"ALT+ENTREE           plein ecran",
	"H                    afficher/masquer l'aide",
	"ESC                  fermer l'aide / quitter",
};
#define HELP_NLINES ((int)(sizeof help_lines / sizeof *help_lines))

typedef struct {
	unsigned int *pixels;   /* ARGB 0xAARRGGBB premultiplie */
	int w, h;
	int has_alpha;
	int error;
} Image;

typedef enum { ACT_NONE, ACT_PREV, ACT_NEXT, ACT_ZOOM_IN, ACT_ZOOM_OUT } Action;

typedef struct {
	SDL_Window  *win;
	char       **files;
	int          nfiles;
	int          current;
	int          fullscreen;
	int          help;
	int          zoom_auto;  /* 1 : ajuste a la fenetre/l'ecran ; 0 : zoom manuel */
	double       zoom;       /* zoom manuel */
	uint32_t    *font;
	Image        img;
	/* vue : partie visible de l'image mise a l'echelle, composee, prete a blitter */
	SDL_Surface *view;
	int          view_x, view_y;            /* position dans la fenetre (peut etre < 0) */
	int          key_sw, key_sh, key_vw, key_vh;
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

/* Taille de l'image ajustee dans une boite bw x bh, ratio conserve. allow_up : agrandir si plus petite. */
static void fit_size(int w, int h, int bw, int bh, int allow_up, int *ow, int *oh)
{
	if (!allow_up && w <= bw && h <= bh) { *ow = w; *oh = h; return; }
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
	if (*bw < MIN_WIN) *bw = MIN_WIN;
	if (*bh < MIN_WIN) *bh = MIN_WIN;
}

/* Taille de l'image entiere une fois mise a l'echelle. bw x bh : boite d'ajustement (zoom auto). */
static void scaled_size(Viewer *v, int bw, int bh, int *sw, int *sh)
{
	if (v->zoom_auto) {
		fit_size(v->img.w, v->img.h, bw, bh, v->fullscreen, sw, sh);
		return;
	}
	*sw = (int)lround(v->img.w * v->zoom);
	*sh = (int)lround(v->img.h * v->zoom);
	if (*sw < 1) *sw = 1;
	if (*sh < 1) *sh = 1;
}

/* Boite d'ajustement pour le mode courant, viewport = taille actuelle de la surface fenetre. */
static void current_box(Viewer *v, int vw, int vh, int *bw, int *bh)
{
	SDL_Rect usable;
	if (v->fullscreen) { *bw = vw; *bh = vh; }
	else windowed_box(v, &usable, bw, bh);
}

static double effective_zoom(Viewer *v)
{
	SDL_Surface *screen = SDL_GetWindowSurface(v->win);
	int bw, bh, sw, sh;
	current_box(v, screen ? screen->w : MIN_WIN, screen ? screen->h : MIN_WIN, &bw, &bh);
	scaled_size(v, bw, bh, &sw, &sh);
	return (double)sw / v->img.w;
}

/* ------------------------------------------------------------------------- */
/* Construction de la vue                                                    */
/* ------------------------------------------------------------------------- */

/* Pour un axe : image de longueur len, mise a l'echelle a s, vue de longueur vlen, centree.
   Donne la tranche source utile [src0, src1[, sa longueur une fois mise a l'echelle (out)
   et sa position dans la vue (dst, negative si l'image deborde : le blit clippe). */
static void axis_crop(int len, int s, int vlen, int *src0, int *src1, int *out, int *dst)
{
	if (s <= vlen) {
		*src0 = 0; *src1 = len; *out = s; *dst = (vlen - s) / 2;
		return;
	}
	const double z = (double)s / len;
	const int o = (s - vlen) / 2;                 /* debut de la zone visible, en coordonnees mises a l'echelle */
	*src0 = (int)floor(o / z) - 2;                /* marge de 2 pixels : bords du bilineaire hors champ */
	*src1 = (int)ceil((o + vlen) / z) + 2;
	if (*src0 < 0) *src0 = 0;
	if (*src1 > len) *src1 = len;
	*out = (int)lround((*src1 - *src0) * z);
	if (*out < 1) *out = 1;
	*dst = (int)lround(*src0 * z) - o;
}

static void build_view(Viewer *v, int vw, int vh)
{
	int bw, bh, sw, sh, x0, x1, y0, y1, ow, oh, dx, dy;
	unsigned int *crop, *scaled;

	current_box(v, vw, vh, &bw, &bh);
	scaled_size(v, bw, bh, &sw, &sh);
	if (v->view && v->key_sw == sw && v->key_sh == sh && v->key_vw == vw && v->key_vh == vh) return;
	SDL_DestroySurface(v->view);
	v->view = NULL;
	v->key_sw = sw; v->key_sh = sh; v->key_vw = vw; v->key_vh = vh;

	axis_crop(v->img.w, sw, vw, &x0, &x1, &ow, &dx);
	axis_crop(v->img.h, sh, vh, &y0, &y1, &oh, &dy);
	const int cw = x1 - x0, ch = y1 - y0;

	/* tranche source */
	if (cw == v->img.w && ch == v->img.h) {
		crop = v->img.pixels;
	} else {
		crop = malloc((size_t)cw * ch * sizeof *crop);
		if (!crop) return;
		for (int y = 0; y < ch; y++)
			memcpy(crop + (size_t)y * cw, v->img.pixels + (size_t)(y0 + y) * v->img.w + x0, cw * sizeof *crop);
	}
	/* mise a l'echelle */
	if (ow == cw && oh == ch) {
		scaled = crop;
	} else if (!(scaled = img_resize(crop, cw, ch, ow, oh))) {
		fprintf(stderr, "Echec du redimensionnement %dx%d -> %dx%d\n", cw, ch, ow, oh);
		scaled = crop; ow = cw; oh = ch;
	}

	v->view = SDL_CreateSurface(ow, oh, SDL_PIXELFORMAT_ARGB8888);
	v->view_x = dx;
	v->view_y = dy;
	if (v->view) {
		SDL_SetSurfaceBlendMode(v->view, SDL_BLENDMODE_NONE);
		SDL_LockSurface(v->view);
		for (int y = 0; y < oh; y++) {
			unsigned int *dst = (unsigned int *)((Uint8 *)v->view->pixels + (size_t)y * v->view->pitch);
			const unsigned int *src = scaled + (size_t)y * ow;
			if (!v->img.has_alpha) {
				memcpy(dst, src, ow * sizeof *dst);
				continue;
			}
			/* composition "over" (premultiplie) sur un damier fixe par rapport a la fenetre */
			const int sy = y + dy + vh; /* + vh : reste positif */
			for (int x = 0; x < ow; x++) {
				const unsigned int p = src[x], ia = 255 - (p >> 24);
				const int sx = x + dx + vw;
				const unsigned int bg = ((sx / CHECKER_SIZE + sy / CHECKER_SIZE) & 1) ? CHECKER_A : CHECKER_B;
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
	if (scaled != crop) free(scaled);
	if (crop != v->img.pixels) free(crop);
}

/* ------------------------------------------------------------------------- */
/* Affichage                                                                 */
/* ------------------------------------------------------------------------- */
#define HELP_MARGIN 8

static int help_maxlen(void)
{
	int maxlen = 0;
	for (int i = 0; i < HELP_NLINES; i++) {
		int l = (int)strlen(help_lines[i]);
		if (l > maxlen) maxlen = l;
	}
	return maxlen;
}

/* Taille du panneau d'aide pour une hauteur de caractere ch. */
static void help_size(int ch, int *pw, int *ph)
{
	*pw = (ch * FONT_W + FONT_H / 2) / FONT_H * help_maxlen() + 2 * HELP_MARGIN;
	*ph = ch * HELP_NLINES + 2 * HELP_MARGIN;
}

static void draw_help(Viewer *v, SDL_Surface *screen)
{
	const int maxlen = help_maxlen(), margin = HELP_MARGIN;
	int pw, ph;
	/* hauteur de caractere : 24 au plus, reduite pour tenir dans la fenetre (largeur = hauteur / 2) */
	int ch = FONT_H;
	if ((ch / 2) * maxlen > screen->w - 4 * margin) ch = 2 * ((screen->w - 4 * margin) / maxlen);
	if (ch * HELP_NLINES > screen->h - 4 * margin) ch = (screen->h - 4 * margin) / HELP_NLINES;
	if (ch < 6) ch = 6;

	help_size(ch, &pw, &ph);
	uint32_t *buf = malloc((size_t)pw * ph * sizeof *buf);
	if (!buf) return;
	for (int i = 0; i < pw * ph; i++) buf[i] = HELP_BG;
	for (int i = 0; i < HELP_NLINES; i++)
		draw_text_ss3x3(buf, pw, ph, margin, margin + i * ch, ch, v->font, "%s", help_lines[i]);

	SDL_Surface *panel = SDL_CreateSurfaceFrom(pw, ph, SDL_PIXELFORMAT_ARGB8888, buf, pw * (int)sizeof *buf);
	if (panel) {
		SDL_Rect r = { (screen->w - pw) / 2, (screen->h - ph) / 2, pw, ph };
		SDL_SetSurfaceBlendMode(panel, SDL_BLENDMODE_BLEND);
		SDL_BlitSurface(panel, NULL, screen, &r);
		SDL_DestroySurface(panel);
	}
	free(buf);
}

static void render(Viewer *v)
{
	SDL_Surface *screen = SDL_GetWindowSurface(v->win);
	if (!screen) return;

	build_view(v, screen->w, screen->h);
	SDL_FillSurfaceRect(screen, NULL, BACKGROUND);
	if (v->view) {
		SDL_Rect r = { v->view_x, v->view_y, v->view->w, v->view->h };
		SDL_BlitSurface(v->view, NULL, screen, &r); /* clipping par SDL si l'image deborde */
	}
	if (v->help && v->font) draw_help(v, screen);
	SDL_UpdateWindowSurface(v->win);
}

static void update_title(Viewer *v)
{
	char title[1024];
	const char *name = v->files[v->current];
	const int zoom = (int)lround(100.0 * effective_zoom(v));

	if (v->img.error)
		snprintf(title, sizeof title, "%s - ERREUR DE DECODAGE [%d/%d]", name, v->current + 1, v->nfiles);
	else if (zoom != 100)
		snprintf(title, sizeof title, "%s - %dx%d - %d%% [%d/%d]", name, v->img.w, v->img.h, zoom,
		         v->current + 1, v->nfiles);
	else
		snprintf(title, sizeof title, "%s - %dx%d [%d/%d]", name, v->img.w, v->img.h, v->current + 1, v->nfiles);
	SDL_SetWindowTitle(v->win, title);
}

/* Fenetre fenetree : taille de l'image mise a l'echelle, bornee a [MIN_WIN, ecran],
   et gardee dans la zone utilisable de l'ecran. */
static void fit_window(Viewer *v)
{
	SDL_Rect usable;
	int bw, bh, sw, sh, x, y, top = 0, left = 0, bottom = 0, right = 0;

	if (v->fullscreen) return;
	windowed_box(v, &usable, &bw, &bh);
	scaled_size(v, bw, bh, &sw, &sh);
	int ww = sw < MIN_WIN ? MIN_WIN : sw;
	int wh = sh < MIN_WIN ? MIN_WIN : sh;
	if (v->help) { /* aide affichee : la fenetre s'agrandit pour la montrer en taille normale */
		int pw, ph;
		help_size(FONT_H, &pw, &ph);
		if (ww < pw + 4 * HELP_MARGIN) ww = pw + 4 * HELP_MARGIN;
		if (wh < ph + 4 * HELP_MARGIN) wh = ph + 4 * HELP_MARGIN;
	}
	if (ww > bw) ww = bw;
	if (wh > bh) wh = bh;

	int cw, chh;
	SDL_GetWindowSize(v->win, &cw, &chh);
	if (cw == ww && chh == wh) return;
	SDL_SetWindowSize(v->win, ww, wh);
	SDL_SyncWindow(v->win);

	SDL_GetWindowBordersSize(v->win, &top, &left, &bottom, &right);
	SDL_GetWindowPosition(v->win, &x, &y);
	const int nx0 = x, ny0 = y;
	if (x + ww + right > usable.x + usable.w) x = usable.x + usable.w - ww - right;
	if (y + wh + bottom > usable.y + usable.h) y = usable.y + usable.h - wh - bottom;
	if (x - left < usable.x) x = usable.x + left;
	if (y - top < usable.y) y = usable.y + top;
	if (x != nx0 || y != ny0) {
		SDL_SetWindowPosition(v->win, x, y);
		SDL_SyncWindow(v->win);
	}
}

static void refresh(Viewer *v)
{
	fit_window(v);
	render(v);
	update_title(v);
}

static void show(Viewer *v, int index)
{
	v->current = (index % v->nfiles + v->nfiles) % v->nfiles;
	load_image(&v->img, v->files[v->current]);
	SDL_DestroySurface(v->view);
	v->view = NULL;
	v->zoom_auto = 1;
	refresh(v);
}

static void set_zoom(Viewer *v, double z)
{
	const int m = v->img.w < v->img.h ? v->img.w : v->img.h;
	const double zmin = 1.0 / m;            /* au moins 1 pixel sur le plus petit cote */
	if (z < zmin) z = zmin;
	if (z > ZOOM_MAX) z = ZOOM_MAX;
	v->zoom_auto = 0;
	v->zoom = z;
	refresh(v);
}

static void zoom_step(Viewer *v, int dir)
{
	const double cur = v->zoom_auto ? effective_zoom(v) : v->zoom;
	double z = dir > 0 ? cur * ZOOM_STEP : cur / ZOOM_STEP;
	/* on ne saute pas par-dessus 100% */
	if ((cur < 0.999 && z > 1.001) || (cur > 1.001 && z < 0.999)) z = 1.0;
	set_zoom(v, z);
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
	v->zoom_auto = 1;
	refresh(v);
}

static void do_action(Viewer *v, Action a)
{
	switch (a) {
		case ACT_PREV:     show(v, v->current - 1); break;
		case ACT_NEXT:     show(v, v->current + 1); break;
		case ACT_ZOOM_IN:  zoom_step(v, 1);  break;
		case ACT_ZOOM_OUT: zoom_step(v, -1); break;
		default: break;
	}
}

/* Actions repetables (touche maintenue). */
static Action key_action(const SDL_KeyboardEvent *k)
{
	switch (k->scancode) {
		case SDL_SCANCODE_LEFT:  case SDL_SCANCODE_UP:   return ACT_PREV;
		case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_DOWN: return ACT_NEXT;
		case SDL_SCANCODE_KP_PLUS:  return ACT_ZOOM_IN;
		case SDL_SCANCODE_KP_MINUS: return ACT_ZOOM_OUT;
		default: return ACT_NONE;
	}
}

/* ------------------------------------------------------------------------- */
int main(int argc, char **argv)
{
	Viewer v;
	SDL_Scancode held = SDL_SCANCODE_UNKNOWN;
	Action held_action = ACT_NONE;
	Uint64 next_repeat = 0;
	int running = 1;

	(void)argc; (void)argv;
	memset(&v, 0, sizeof v);
	v.zoom_auto = 1;

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "SDL_Init error [%s]\n", SDL_GetError());
		return 1;
	}
	if (!list_files(&v)) {
		fprintf(stderr, "Aucun fichier .qoi dans le repertoire courant\n");
		SDL_Quit();
		return 1;
	}
	if (!(v.win = SDL_CreateWindow("qoiviewer", MIN_WIN, MIN_WIN, 0))) {
		fprintf(stderr, "SDL_CreateWindow error [%s]\n", SDL_GetError());
		SDL_Quit();
		return 1;
	}
	v.font = font_build();
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
				case SDL_EVENT_KEY_DOWN: {
					Action a;
					if (ev.key.repeat) break; /* repetition geree par nos soins */
					if (ev.key.scancode == SDL_SCANCODE_ESCAPE) {
						if (v.help) { v.help = 0; refresh(&v); }
						else running = 0;
					} else if ((ev.key.scancode == SDL_SCANCODE_RETURN || ev.key.scancode == SDL_SCANCODE_KP_ENTER)
					           && (ev.key.mod & SDL_KMOD_ALT)) {
						toggle_fullscreen(&v);
					} else if (ev.key.key == SDLK_K) {
						set_zoom(&v, 1.0);
					} else if (ev.key.scancode == SDL_SCANCODE_KP_MULTIPLY) {
						v.zoom_auto = 1;
						refresh(&v);
					} else if (ev.key.key == SDLK_H) {
						v.help = !v.help;
						refresh(&v);
					} else if ((a = key_action(&ev.key)) != ACT_NONE) {
						held = ev.key.scancode;
						held_action = a;
						do_action(&v, a);
						next_repeat = SDL_GetTicks() + REPEAT_DELAY;
					}
					break;
				}
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
			do_action(&v, held_action);
			/* cadence fixe (25 images/s en navigation) ; si le chargement est plus lent que
			   l'intervalle, on enchaine sans accumuler de retard */
			const Uint64 now = SDL_GetTicks();
			next_repeat += (held_action == ACT_PREV || held_action == ACT_NEXT) ? REPEAT_NAV : REPEAT_ZOOM;
			if (next_repeat < now) next_repeat = now;
		}
	}

	free_image(&v.img);
	SDL_DestroySurface(v.view);
	free(v.font);
	for (int i = 0; i < v.nfiles; i++) free(v.files[i]);
	free(v.files);
	SDL_DestroyWindow(v.win);
	SDL_Quit();
	return 0;
}

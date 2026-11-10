#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "img_sharpen.h"
#include "img_resize.h"


/* a + (a - b) par canal, borne a 0..255 : prolonge lineairement le degrade b -> a. */
static inline unsigned int extrapolate(unsigned int a, unsigned int b)
{
    unsigned int r = 0;
    for (int s = 0; s < 32; s += 8) {
        int v = 2 * (int)((a >> s) & 255) - (int)((b >> s) & 255);
        r |= (unsigned int)(v < 0 ? 0 : (v > 255 ? 255 : v)) << s;
    }
    return r;
}

/* Interpolation bilineaire des 4 canaux, poids wx, wy dans [0,256] (somme des 4 poids = 65536). */
static inline unsigned int bilerp(unsigned int p00, unsigned int p10, unsigned int p01, unsigned int p11,
                                  unsigned int wx, unsigned int wy)
{
    const unsigned int w00 = (256 - wx) * (256 - wy), w10 = wx * (256 - wy),
                       w01 = (256 - wx) * wy,         w11 = wx * wy;
    unsigned int r = 0;
    for (int s = 0; s < 32; s += 8) {
        unsigned int c = ((p00 >> s) & 255) * w00 + ((p10 >> s) & 255) * w10 +
                         ((p01 >> s) & 255) * w01 + ((p11 >> s) & 255) * w11;
        r |= ((c + 32768) >> 16) << s;
    }
    return r;
}

/* Moyenne de 4 pixels ARGB, canal par canal, arrondie au plus proche. Les canaux sont
   traites par paires (octets pairs / impairs) dans des voies de 16 bits : pas de debordement. */
#define LANES 0x00FF00FFu
static inline unsigned int avg4(unsigned int a, unsigned int b, unsigned int c, unsigned int d)
{
    unsigned int rb = (a & LANES) + (b & LANES) + (c & LANES) + (d & LANES) + 0x00020002u;
    unsigned int ag = ((a >> 8) & LANES) + ((b >> 8) & LANES) + ((c >> 8) & LANES) + ((d >> 8) & LANES) + 0x00020002u;
    return ((rb >> 2) & LANES) | (((ag >> 2) & LANES) << 8);
}

/* Reduit par 2 en largeur (rx) et/ou en hauteur (ry) : moyenne de boite 2x1, 1x2 ou 2x2.
   Dimension impaire : le dernier pixel est moyenne avec lui-meme. */
static unsigned int *halve(const unsigned int *s, size_t cw, size_t ch, int rx, int ry,
                           size_t *nw, size_t *nh)
{
    *nw = rx ? (cw + 1) / 2 : cw;
    *nh = ry ? (ch + 1) / 2 : ch;
    unsigned int *d = malloc(*nw * *nh * sizeof *d);
    if (!d) return NULL;

    for (size_t y = 0; y < *nh; y++) {
        const size_t ya = ry ? 2 * y : y, yb = ry ? (2 * y + 1 < ch ? 2 * y + 1 : ch - 1) : ya;
        const unsigned int *r0 = s + ya * cw, *r1 = s + yb * cw;
        for (size_t x = 0; x < *nw; x++) {
            const size_t xa = rx ? 2 * x : x, xb = rx ? (2 * x + 1 < cw ? 2 * x + 1 : cw - 1) : xa;
            d[y * *nw + x] = avg4(r0[xa], r0[xb], r1[xa], r1[xb]);
        }
    }
    return d;
}

static inline int64_t ifloor_div(int64_t n, int64_t d)   /* d > 0 */
{
    int64_t q = n / d;
    return (n % d < 0) ? q - 1 : q;
}

/* Noyau bilineaire : src (cw x ch) est la version de l'image d'origine (W x H) reduite d'un
   facteur fx en largeur et fy en hauteur (fx, fy = puissances de 2). Les positions sont
   calculees sur la geometrie d'origine : x = (ox + 0.5) * W / (ow * fx) - 0.5. */
static unsigned int *bilinear(const unsigned int *src, size_t cw, size_t ch,
                              int64_t W, int64_t H, int64_t fx, int64_t fy, size_t ow, size_t oh)
{
    const size_t W2 = cw + 2, H2 = ch + 2;
    if (W2 > SIZE_MAX / H2 / sizeof(unsigned int) || ow > SIZE_MAX / oh / sizeof(unsigned int))
        return NULL;

    unsigned int *work = malloc(W2 * H2 * sizeof *work);
    unsigned int *out  = malloc(ow * oh * sizeof *out);
    uint32_t     *xi   = malloc(ow * sizeof *xi);        /* par colonne de sortie : indice dans le buffer de travail */
    uint16_t     *wxs  = malloc(ow * sizeof *wxs);       /*                         et poids horizontal 0..256        */
    if (!work || !out || !xi || !wxs) {
        free(work); free(out); free(xi); free(wxs);
        return NULL;
    }

    /* --- 1. Image au centre du buffer de travail (cw+2) x (ch+2) --- */
    for (size_t y = 0; y < ch; y++)
        memcpy(work + (y + 1) * W2 + 1, src + y * cw, cw * sizeof *work);

    /* --- 2. Colonnes gauche/droite : prolongation lineaire (colonne 0 + (colonne 0 - colonne 1)) --- */
    for (size_t y = 1; y <= ch; y++) {
        unsigned int *row = work + y * W2;
        row[0]      = extrapolate(row[1], row[cw >= 2 ? 2 : 1]);
        row[cw + 1] = extrapolate(row[cw], row[cw >= 2 ? cw - 1 : cw]);
    }

    /* --- 3. Lignes haut/bas sur toute la largeur, extras compris : les 4 coins en decoulent --- */
    for (size_t x = 0; x < W2; x++) {
        work[x]                 = extrapolate(work[W2 + x],      work[(ch >= 2 ? 2 : 1) * W2 + x]);
        work[(ch + 1) * W2 + x] = extrapolate(work[ch * W2 + x], work[(ch >= 2 ? ch - 1 : ch) * W2 + x]);
    }

    /* --- 4. Positions source des colonnes, en entier exact ---
       numerateur (2*ox+1)*W - ow*fx sur le denominateur 2*ow*fx ; x0 = partie entiere (>= -1),
       poids = partie fractionnaire sur 0..256. x0 + 1 et x0 + 2 sont toujours dans [0, cw+1]. */
    const int64_t denx = 2 * (int64_t)ow * fx, deny = 2 * (int64_t)oh * fy;
    for (size_t ox = 0; ox < ow; ox++) {
        int64_t num = (2 * (int64_t)ox + 1) * W - (int64_t)ow * fx;
        int64_t x0  = ifloor_div(num, denx);
        xi[ox]  = (uint32_t)(x0 + 1);
        wxs[ox] = (uint16_t)(((num - x0 * denx) * 256 + denx / 2) / denx);
    }

    /* --- 5. Interpolation --- */
    for (size_t oy = 0; oy < oh; oy++) {
        int64_t num = (2 * (int64_t)oy + 1) * H - (int64_t)oh * fy;
        int64_t y0  = ifloor_div(num, deny);
        const unsigned int wy = (unsigned int)(((num - y0 * deny) * 256 + deny / 2) / deny);
        const unsigned int *r0 = work + (size_t)(y0 + 1) * W2, *r1 = r0 + W2;
        unsigned int *dst = out + oy * ow;

        for (size_t ox = 0; ox < ow; ox++) {
            const size_t i = xi[ox];
            dst[ox] = bilerp(r0[i], r0[i + 1], r1[i], r1[i + 1], wxs[ox], wy);
        }
    }

    free(work); free(xi); free(wxs);
    return out;
}

unsigned int *img_resize(const unsigned int *buffer, int width, int height, int owidth, int oheight)
{
    if (!buffer || width <= 0 || height <= 0 || owidth <= 0 || oheight <= 0)
        return NULL;

    const size_t ow = (size_t)owidth, oh = (size_t)oheight;
    size_t cw = (size_t)width, ch = (size_t)height;
    int64_t fx = 1, fy = 1;
    unsigned int *cur = NULL;
    unsigned int *owned = NULL;                  /* image intermediaire courante (jamais le buffer de l'appelant) */
    int w=0;

    /* Sortie au plus moitie moins grande qu'une dimension : on la divise par 2 (moyenne de boite),
       autant de fois que necessaire, avant l'interpolation bilineaire (qui ne regarde que 4 pixels). */
    for (;;) {
        const int rx = 2 * ow <= cw, ry = 2 * oh <= ch;
        if (!rx && !ry) break;

        size_t nw, nh;
        unsigned int *next = halve(cur==NULL?buffer:cur, cw, ch, rx, ry, &nw, &nh);
        if (!next) { free(owned); return NULL; }
        free(owned);
        owned = next; cur = next; cw = nw; ch = nh;
        if (rx) fx *= 2;
        if (ry) fy *= 2;
    }

    // sharp a little according to pre-reduction
    if (owidth<=(width>>1) && oheight<=(height>>1)) {
	applySharpen(cur,cw,ch,13);
	w=13;
    } else if (owidth<=(width>>1)) {
	applyHorizontalSharpen(cur,cw,ch);
	w=13;
    } else if (oheight<=(height>>1)) {
	applyVerticalSharpen(cur,cw,ch);
	w=13;
    }
    unsigned int *out = bilinear(cur, cw, ch, width, height, fx, fy, ow, oh);
    if (!w) {
	    // no post-reduction sharpen
	    int ratio;
	    ratio=width/ow;
	    // weight correction according to scale factor
	    w = ratio <= 1.3 ? 26 : ratio >= 1.7 ? 13 : 26 - (int)((ratio - 1.3) / 0.4 * 13 + 0.5);
    }
    applySharpen(out,ow,oh,w);
    free(owned);
    return out;
}

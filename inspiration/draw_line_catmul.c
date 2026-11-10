#include <stdint.h>
#include <stddef.h>
#include "draw_line.h"

/* Les produits intermediaires atteignent ~2^66 pour des int extremes : __int128
   (GCC / Clang) les rend exacts. */
#ifndef __SIZEOF_INT128__
#error "draw_line.c necessite __int128 (GCC ou Clang)"
#endif
typedef __int128 i128;

static inline i128 ceil_div(i128 n, i128 d)   /* d > 0 */
{
    i128 q = n / d, r = n % d;
    return (r > 0) ? q + 1 : q;
}
static inline i128 floor_div(i128 n, i128 d)  /* d > 0 */
{
    i128 q = n / d, r = n % d;
    return (r < 0) ? q - 1 : q;
}

/*
 * Ligne de Bresenham a clipping EXACT : les pixels dessines sont ceux de la ligne
 * non clippee, restreints au buffer (aucune deformation aux bords).
 * Axe majeur (a) parcouru pas a pas ; axe mineur (b) : m(i) = floor((2*db*i + da) / (2*da)).
 */
void draw_line(unsigned int *buffer, unsigned int width, unsigned int height,
               unsigned int color, int x1, int y1, int x2, int y2)
{
    if (!buffer || width == 0 || height == 0)
        return;

    const int64_t W = width, H = height;

    /* --- Detection de clipping total (rejet trivial) --- */
    int xmin_l = x1 < x2 ? x1 : x2, xmax_l = x1 > x2 ? x1 : x2;
    int ymin_l = y1 < y2 ? y1 : y2, ymax_l = y1 > y2 ? y1 : y2;
    if (xmax_l < 0 || ymax_l < 0 || (int64_t)xmin_l >= W || (int64_t)ymin_l >= H)
        return;

    const int64_t X1 = x1, Y1 = y1;
    const int64_t dx = (int64_t)x2 - x1, dy = (int64_t)y2 - y1;
    const int64_t adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    const int64_t sx = dx < 0 ? -1 : 1,   sy = dy < 0 ? -1 : 1;

    /* a = axe majeur, b = axe mineur */
    const int steep = ady > adx;
    const int64_t a1 = steep ? Y1 : X1,  b1 = steep ? X1 : Y1;
    const int64_t da = steep ? ady : adx, db = steep ? adx : ady;
    const int64_t sa = steep ? sy : sx,   sb = steep ? sx : sy;
    const int64_t Amax = (steep ? H : W) - 1, Bmax = (steep ? W : H) - 1;

    /* --- Plage d'indices i dans [0,da] imposee par l'axe majeur --- */
    int64_t i0, i1;
    if (sa > 0) { i0 = -a1;       i1 = Amax - a1; }
    else        { i0 = a1 - Amax; i1 = a1;        }
    if (i0 < 0)  i0 = 0;
    if (i1 > da) i1 = da;
    if (i0 > i1)
        return;

    /* --- Plage imposee par l'axe mineur : m dans [mlo,mhi] --- */
    if (db == 0) {
        if (b1 < 0 || b1 > Bmax)
            return;
    } else {
        int64_t mlo, mhi;
        if (sb > 0) { mlo = -b1;       mhi = Bmax - b1; }
        else        { mlo = b1 - Bmax; mhi = b1;        }
        if (mlo < 0)  mlo = 0;
        if (mhi > db) mhi = db;
        if (mlo > mhi)
            return;
        if (mlo > 0) {   /* m(i) >= mlo  <=>  i >= ceil(da*(2*mlo-1) / (2*db)) */
            i128 lo = ceil_div((i128)da * (2 * (i128)mlo - 1), 2 * (i128)db);
            if (lo > i0) i0 = (int64_t)lo;
        }
        if (mhi < db) {  /* m(i) <= mhi  <=>  i <= floor((2*da*(mhi+1) - da - 1) / (2*db)) */
            i128 hi = floor_div(2 * (i128)da * ((i128)mhi + 1) - da - 1, 2 * (i128)db);
            if (hi < i1) i1 = (int64_t)hi;
        }
        if (i0 > i1)
            return;
    }

    /* --- Etat de Bresenham a l'indice i0, calcule exactement --- */
    int64_t m = 0, r = 0;   /* r = (2*db*i + da) - 2*da*m, dans [0, 2*da) */
    if (da > 0) {
        i128 num = 2 * (i128)db * i0 + da;
        i128 mm  = num / (2 * (i128)da);
        m = (int64_t)mm;
        r = (int64_t)(num - mm * 2 * (i128)da);
    }

    for (int64_t i = i0; ; i++) {
        int64_t a = a1 + sa * i, b = b1 + sb * m;
        int64_t px = steep ? b : a, py = steep ? a : b;
        if ((uint64_t)px < (uint64_t)W && (uint64_t)py < (uint64_t)H)   /* filet de securite */
            buffer[(size_t)py * (size_t)W + (size_t)px] = color;
        if (i == i1)
            break;
        r += 2 * db;
        if (r >= 2 * da) { r -= 2 * da; m++; }
    }
}

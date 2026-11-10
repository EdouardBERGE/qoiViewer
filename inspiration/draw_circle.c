
/* Ecriture avec test de bornes (clipping local). */
static inline void put_clipped(unsigned int *buf, int64_t w, int64_t h,
                               unsigned int color, int64_t x, int64_t y)
{
    if ((uint64_t)x < (uint64_t)w && (uint64_t)y < (uint64_t)h)
        buf[(size_t)y * (size_t)w + (size_t)x] = color;
}

/* Ecriture sans test : uniquement si le cercle est entierement dans le buffer. */
static inline void put_fast(unsigned int *buf, int64_t w, unsigned int color,
                            int64_t x, int64_t y)
{
    buf[(size_t)y * (size_t)w + (size_t)x] = color;
}

void draw_circle(unsigned int *buffer, unsigned int width, unsigned int height,
                 unsigned int color, int cx, int cy, int ray)
{
    if (!buffer || width == 0 || height == 0 || ray < 0)
        return;

    const int64_t W = width, H = height;
    const int64_t X = cx, Y = cy, R = ray;

    /* --- Clipping total : la boite englobante du cercle est hors du buffer --- */
    if (X + R < 0 || Y + R < 0 || X - R >= W || Y - R >= H)
        return;

    /* --- Clipping total bis : le buffer entier est strictement a l'interieur
           du disque (les 4 coins sont a distance < R-1 du centre) : rien a tracer.
           Calcul en double car les carres peuvent depasser 64 bits. --- */
    if (R >= 2) {
        /* ecart max au centre, sur chaque axe, parmi les bords 0 et W-1 (resp. H-1) */
        int64_t ax = X < 0 ? -X : X,  bx = (W - 1 - X) < 0 ? X - (W - 1) : (W - 1 - X);
        int64_t ay = Y < 0 ? -Y : Y,  by = (H - 1 - Y) < 0 ? Y - (H - 1) : (H - 1 - Y);
        double fx = (double)(ax > bx ? ax : bx);
        double fy = (double)(ay > by ? ay : by);
        double lim = (double)(R - 1);
        if (fx * fx + fy * fy < lim * lim)
            return;
    }

    const int fully_inside = (X - R >= 0 && X + R < W && Y - R >= 0 && Y + R < H);

    /* --- Algorithme d'Andres : un octant, symetrie 8 fois, sans trou --- */
    int64_t x = 0, y = R, d = R - 1;

#define PLOT8(PUT)                                  \
    do {                                            \
        PUT(X + x, Y + y); PUT(X + y, Y + x);       \
        PUT(X - x, Y + y); PUT(X - y, Y + x);       \
        PUT(X + x, Y - y); PUT(X + y, Y - x);       \
        PUT(X - x, Y - y); PUT(X - y, Y - x);       \
    } while (0)

#define PUT_FAST(px, py)    put_fast(buffer, W, color, (px), (py))
#define PUT_CLIP(px, py)    put_clipped(buffer, W, H, color, (px), (py))

    while (y >= x) {
        if (fully_inside) PLOT8(PUT_FAST);
        else              PLOT8(PUT_CLIP);

        if (d >= 2 * x) {
            d -= 2 * x + 1;
            x++;
        } else if (d < 2 * (R - y)) {
            d += 2 * y - 1;
            y--;
        } else {
            d += 2 * (y - x - 1);
            y--;
            x++;
        }
    }
#undef PLOT8
#undef PUT_FAST
#undef PUT_CLIP
}

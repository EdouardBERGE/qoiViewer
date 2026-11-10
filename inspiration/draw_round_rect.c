
static inline int sat_int(int64_t v)
{
    return v < INT_MIN ? INT_MIN : (v > INT_MAX ? INT_MAX : (int)v);
}

#if 0
static inline void put_clipped(unsigned int *buf, int64_t w, int64_t h,
                               unsigned int color, int64_t x, int64_t y)
{
    if ((uint64_t)x < (uint64_t)w && (uint64_t)y < (uint64_t)h)
        buf[(size_t)y * (size_t)w + (size_t)x] = color;
}

static inline void put_fast(unsigned int *buf, int64_t w, unsigned int color,
                            int64_t x, int64_t y)
{
    buf[(size_t)y * (size_t)w + (size_t)x] = color;
}
#endif

/* Un quart de cercle (algorithme d'Andres, sans trou) centre en (cx,cy).
   sx,sy = +-1 : quadrant trace (ex. sx=-1, sy=-1 -> haut-gauche). */
static void draw_corner(unsigned int *buf, int64_t W, int64_t H, unsigned int color,
                        int64_t cx, int64_t cy, int64_t r, int sx, int sy)
{
    /* Boite du quadrant : [cx, cx+sx*r] x [cy, cy+sy*r] */
    int64_t x0 = sx < 0 ? cx - r : cx, x1 = sx < 0 ? cx : cx + r;
    int64_t y0 = sy < 0 ? cy - r : cy, y1 = sy < 0 ? cy : cy + r;

    /* Clipping total du coin */
    if (x1 < 0 || y1 < 0 || x0 >= W || y0 >= H)
        return;

    const int inside = (x0 >= 0 && x1 < W && y0 >= 0 && y1 < H);

    int64_t x = 0, y = r, d = r - 1;
    while (y >= x) {
        int64_t ax = cx + sx * x, ay = cy + sy * y;   /* octant 1 */
        int64_t bx = cx + sx * y, by = cy + sy * x;   /* octant 2 (symetrique) */
        if (inside) {
            put_fast(buf, W, color, ax, ay);
            put_fast(buf, W, color, bx, by);
        } else {
            put_clipped(buf, W, H, color, ax, ay);
            put_clipped(buf, W, H, color, bx, by);
        }
        if (d >= 2 * x)                 { d -= 2 * x + 1; x++; }
        else if (d < 2 * (r - y))       { d += 2 * y - 1; y--; }
        else                            { d += 2 * (y - x - 1); y--; x++; }
    }
}

void draw_round_rect(unsigned int *buffer, unsigned int width, unsigned int height,
                     unsigned int color, int x, int y, int rect_w, int rect_h, int radius)
{
    if (!buffer || width == 0 || height == 0 || rect_w <= 0 || rect_h <= 0)
        return;

    const int64_t W = width, H = height;
    const int64_t L = x, T = y;
    const int64_t R = (int64_t)x + rect_w - 1;   /* colonne droite incluse */
    const int64_t B = (int64_t)y + rect_h - 1;   /* ligne basse incluse   */

    /* --- Clipping total : la boite du rectangle est hors du buffer --- */
    if (R < 0 || B < 0 || L >= W || T >= H)
        return;

    /* --- Pas d'arrondi si le rectangle est trop petit --- */
    int64_t r = radius;
    if (r <= 0 || (int64_t)rect_w < 2 * r || (int64_t)rect_h < 2 * r)
        r = 0;

    /* --- Aretes droites (clippees par draw_line) --- */
    draw_line(buffer, width, height, color, sat_int(L + r), sat_int(T), sat_int(R - r), sat_int(T)); /* haut   */
    if (B != T)
        draw_line(buffer, width, height, color, sat_int(L + r), sat_int(B), sat_int(R - r), sat_int(B)); /* bas */
    draw_line(buffer, width, height, color, sat_int(L), sat_int(T + r), sat_int(L), sat_int(B - r)); /* gauche */
    if (R != L)
        draw_line(buffer, width, height, color, sat_int(R), sat_int(T + r), sat_int(R), sat_int(B - r)); /* droite */

    /* --- Coins arrondis --- */
    if (r > 0) {
        draw_corner(buffer, W, H, color, L + r, T + r, r, -1, -1);
        draw_corner(buffer, W, H, color, R - r, T + r, r, +1, -1);
        draw_corner(buffer, W, H, color, L + r, B - r, r, -1, +1);
        draw_corner(buffer, W, H, color, R - r, B - r, r, +1, +1);
    }
}

#define draw_rect(buffer,w,h,c,x,y,rw,rh) draw_round_rect(buffer,w,h,c,x,y,rw,rh,0);


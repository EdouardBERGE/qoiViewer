
#define TAN30   0.57735026918962576451   /* tan(30 deg) : decalage lateral / cote adjacent */
#define INV_COS 1.15470053837925152902   /* 1 / cos(30 deg) : longueur d'une barbe / cote adjacent */

#if 0
static int round_sat(double v)
{
    if (v <= (double)INT_MIN) return INT_MIN;
    if (v >= (double)INT_MAX) return INT_MAX;
    return (int)lround(v);
}
#endif

/* Pointe en (tx,ty). (bx,by) = vecteur unitaire qui part de la pointe et remonte le
   long du segment. Les barbes finissent a adj le long du segment et +/- adj*tan30 de cote. */
static void draw_head(unsigned int *buf, unsigned int w, unsigned int h, unsigned int color,
                      int tx, int ty, double bx, double by, int adj)
{
    const double a = (double)adj, s = a * TAN30;
    const double px = -by, py = bx;                 /* perpendiculaire unitaire */
    const double ex = tx + a * bx, ey = ty + a * by;/* pied des barbes sur le segment */

    draw_line(buf, w, h, color, tx, ty, round_sat(ex + s * px), round_sat(ey + s * py));
    draw_line(buf, w, h, color, tx, ty, round_sat(ex - s * px), round_sat(ey - s * py));
}

void draw_arrow(unsigned int *buffer, unsigned int width, unsigned int height,
                unsigned int color, int x1, int y1, int x2, int y2,
                int head1, int head2, int adj)
{
    if (!buffer || width == 0 || height == 0)
        return;

    const int has_heads = (head1 || head2) && adj > 0;

    /* --- Clipping total : boite des extremites, elargie de la portee max d'une barbe --- */
    const int64_t reach = has_heads ? (int64_t)ceil((double)adj * INV_COS) + 1 : 0;
    const int64_t minx = (int64_t)(x1 < x2 ? x1 : x2) - reach, maxx = (int64_t)(x1 > x2 ? x1 : x2) + reach;
    const int64_t miny = (int64_t)(y1 < y2 ? y1 : y2) - reach, maxy = (int64_t)(y1 > y2 ? y1 : y2) + reach;
    if (maxx < 0 || maxy < 0 || minx >= (int64_t)width || miny >= (int64_t)height)
        return;

    /* --- Segment (clipping exact par draw_line) --- */
    draw_line(buffer, width, height, color, x1, y1, x2, y2);

    if (!has_heads || (x1 == x2 && y1 == y2))
        return;

    /* --- Pointes --- */
    const double dx = (double)x2 - x1, dy = (double)y2 - y1;
    const double len = hypot(dx, dy);
    const double ux = dx / len, uy = dy / len;      /* direction de (x1,y1) vers (x2,y2) */

    if (head2) draw_head(buffer, width, height, color, x2, y2, -ux, -uy, adj);
    if (head1) draw_head(buffer, width, height, color, x1, y1,  ux,  uy, adj);
}

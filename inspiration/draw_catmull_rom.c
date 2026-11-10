
#define FLAT_TOL    0.25   /* ecart max (pixels) entre la courbe et sa corde */
#define MAX_DEPTH   22     /* garde-fou de la subdivision */

typedef struct { double x, y; } pt;

typedef struct {
    unsigned int *buf;
    unsigned int  w, h;
    unsigned int  color;
} ctx;
static int round_sat(double v)
{
    if (v <= (double)INT_MIN) return INT_MIN;
    if (v >= (double)INT_MAX) return INT_MAX;
    return (int)lround(v);
}
static inline pt mid(pt a, pt b) { pt r = { (a.x + b.x) * 0.5, (a.y + b.y) * 0.5 }; return r; }

static inline double dmin4(double a, double b, double c, double d)
{ double m = a < b ? a : b; double n = c < d ? c : d; return m < n ? m : n; }
static inline double dmax4(double a, double b, double c, double d)
{ double m = a > b ? a : b; double n = c > d ? c : d; return m > n ? m : n; }

/* Une courbe de Bezier cubique est contenue dans l'enveloppe convexe de ses 4
   points de controle : si leur boite est hors du buffer, rien n'est visible. */
static int hull_outside(const ctx *c, pt b0, pt b1, pt b2, pt b3)
{
    double minx = dmin4(b0.x, b1.x, b2.x, b3.x), maxx = dmax4(b0.x, b1.x, b2.x, b3.x);
    double miny = dmin4(b0.y, b1.y, b2.y, b3.y), maxy = dmax4(b0.y, b1.y, b2.y, b3.y);
    /* marge de 1 pixel : l'arrondi vers le pixel ne doit jamais etre rate */
    return maxx < -1.0 || maxy < -1.0 || minx > (double)c->w || miny > (double)c->h;
}

static int is_flat(pt b0, pt b1, pt b2, pt b3)
{
    double dx = b3.x - b0.x, dy = b3.y - b0.y;
    double len2 = dx * dx + dy * dy;

    if (len2 < 1e-12) {   /* extremites confondues : plat si les controles sont tout proches */
        double e1 = (b1.x - b0.x) * (b1.x - b0.x) + (b1.y - b0.y) * (b1.y - b0.y);
        double e2 = (b2.x - b0.x) * (b2.x - b0.x) + (b2.y - b0.y) * (b2.y - b0.y);
        return e1 <= FLAT_TOL * FLAT_TOL && e2 <= FLAT_TOL * FLAT_TOL;
    }

    double len = sqrt(len2);
    double d1 = fabs((b1.x - b0.x) * dy - (b1.y - b0.y) * dx);   /* distance * len */
    double d2 = fabs((b2.x - b0.x) * dy - (b2.y - b0.y) * dx);
    if ((d1 + d2) > FLAT_TOL * len)
        return 0;

    /* Les controles ne doivent pas deborder des extremites le long de la corde
       (sinon la courbe fait un aller-retour : cusp ou boucle) */
    double t1 = (b1.x - b0.x) * dx + (b1.y - b0.y) * dy;
    double t2 = (b2.x - b0.x) * dx + (b2.y - b0.y) * dy;
    double lo = -FLAT_TOL * len, hi = len2 + FLAT_TOL * len;
    return t1 >= lo && t1 <= hi && t2 >= lo && t2 <= hi;
}

static void bezier_draw(const ctx *c, pt b0, pt b1, pt b2, pt b3, int depth)
{
    if (hull_outside(c, b0, b1, b2, b3))
        return;                                   /* clipping total du morceau */

    if (depth >= MAX_DEPTH || is_flat(b0, b1, b2, b3)) {
        draw_line(c->buf, c->w, c->h, c->color,   /* clipping local par draw_line */
                  round_sat(b0.x), round_sat(b0.y), round_sat(b3.x), round_sat(b3.y));
        return;
    }

    /* De Casteljau a t = 0.5 */
    pt b01 = mid(b0, b1), b12 = mid(b1, b2), b23 = mid(b2, b3);
    pt b012 = mid(b01, b12), b123 = mid(b12, b23);
    pt m = mid(b012, b123);
    bezier_draw(c, b0, b01, b012, m, depth + 1);
    bezier_draw(c, m, b123, b23, b3, depth + 1);
}

void draw_catmull_rom(unsigned int *buffer, unsigned int width, unsigned int height,
                      unsigned int color, int *x, int *y, unsigned int nb)
{
    if (!buffer || !x || !y || width == 0 || height == 0 || nb == 0)
        return;

    ctx c = { buffer, width, height, color };

    if (nb == 1) {
        draw_line(buffer, width, height, color, x[0], y[0], x[0], y[0]);
        return;
    }

    for (unsigned int i = 0; i + 1 < nb; i++) {
        unsigned int i0 = i ? i - 1 : 0;              /* extremite : point duplique */
        unsigned int i3 = (i + 2 < nb) ? i + 2 : nb - 1;

        pt p0 = { (double)x[i0],    (double)y[i0]    };
        pt p1 = { (double)x[i],     (double)y[i]     };
        pt p2 = { (double)x[i + 1], (double)y[i + 1] };
        pt p3 = { (double)x[i3],    (double)y[i3]    };

        /* Catmull-Rom (tension 0.5) -> Bezier cubique equivalente */
        pt b1 = { p1.x + (p2.x - p0.x) / 6.0, p1.y + (p2.y - p0.y) / 6.0 };
        pt b2 = { p2.x - (p3.x - p1.x) / 6.0, p2.y - (p3.y - p1.y) / 6.0 };

        bezier_draw(&c, p1, b1, b2, p2, 0);
    }
}

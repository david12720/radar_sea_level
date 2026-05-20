#include "bicubic.h"
#include <cmath>

static double cubicWeight(double t)
{
    t = std::abs(t);
    if (t < 1.0) return  1.5*t*t*t - 2.5*t*t + 1.0;
    if (t < 2.0) return -0.5*t*t*t + 2.5*t*t - 4.0*t + 2.0;
    return 0.0;
}

double bicubic_interpolate(const double p[4][4], double tx, double ty)
{
    double result = 0.0;
    for (int ci = 0; ci < 4; ++ci) {
        double wc = cubicWeight(ci - 1 - tx);
        for (int ri = 0; ri < 4; ++ri)
            result += wc * cubicWeight(ri - 1 - ty) * p[ci][ri];
    }
    return result;
}

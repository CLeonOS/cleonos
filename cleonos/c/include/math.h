#ifndef CLEONOS_MATH_H
#define CLEONOS_MATH_H

#ifndef INFINITY
#define INFINITY ((double)(1.0 / 0.0))
#endif

#ifndef HUGE_VAL
#define HUGE_VAL INFINITY
#endif

double floor(double value);
double ceil(double value);
double fmod(double left, double right);
double ldexp(double value, int exponent);
double frexp(double value, int *out_exponent);
double sqrt(double value);
double pow(double base, double exponent);

#endif

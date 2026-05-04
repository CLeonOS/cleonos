#ifndef CLEONOS_LUA_MATH_H
#define CLEONOS_LUA_MATH_H

#define HUGE_VAL (__builtin_huge_val())

double fabs(double value);
double floor(double value);
double ceil(double value);
double fmod(double left, double right);
double ldexp(double value, int exponent);
double frexp(double value, int *out_exponent);
double sqrt(double value);
double pow(double base, double exponent);

#endif

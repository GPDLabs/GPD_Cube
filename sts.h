#ifndef __STS_H__
#define __STS_H__

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define MAX(x,y)             ((x) <  (y)  ? (y)  : (x))
#define MIN(x,y)             ((x) >  (y)  ? (y)  : (x))
#define isNonPositive(x)     ((x) <= 0.e0 ?   1  : 0)
#define isPositive(x)        ((x) >  0.e0 ?   1 : 0)
#define isNegative(x)        ((x) <  0.e0 ?   1 : 0)
#define isGreaterThanOne(x)  ((x) >  1.e0 ?   1 : 0)
#define isZero(x)            ((x) == 0.e0 ?   1 : 0)
#define isOne(x)             ((x) == 1.e0 ?   1 : 0)

#define ALPHA							0.01	/* SIGNIFICANCE LEVEL */
#define MAXNUMOFTEMPLATES				148		/* APERIODIC TEMPLATES: 148=>temp_length=9 */

typedef unsigned char	BitSequence;
extern BitSequence* epsilon;

double cephes_igamc(double a, double x);
double cephes_igam(double a, double x);
double cephes_lgam(double x);
double cephes_p1evl(double x, double* coef, int N);
double cephes_polevl(double x, double* coef, int N);
double cephes_erf(double x);
double cephes_erfc(double x);
double cephes_normal(double x);

int				computeRank(int M, int Q, BitSequence** matrix);
void			perform_elementary_row_operations(int flag, int i, int M, int Q, BitSequence** A);
int				find_unit_element_and_swap(int flag, int i, int M, int Q, BitSequence** A);
int				swap_rows(int i, int index, int Q, BitSequence** A);
int				determine_rank(int m, int M, int Q, BitSequence** A);
BitSequence** create_matrix(int M, int Q);
void			display_matrix(int M, int Q, BitSequence** m);
void			def_matrix(int M, int Q, BitSequence** m, int k);
void			delete_matrix(int M, BitSequence** matrix);

int nist_randomness_evaluate(unsigned char* rnd);

#endif

#include "sts.h"

BitSequence * epsilon = NULL;

double test01Frequency(int n)
{
    double p_value = 0;
    int		i;
    double	f, s_obs, sum, sqrt2 = 1.41421356237309504880;

    sum = 0.0;
    for (i = 0; i < n; i++)
        sum += 2 * (int)epsilon[i] - 1;
    s_obs = fabs(sum) / sqrt(n);
    f = s_obs / sqrt2;
    p_value = erfc(f);
    return p_value;
}

double test02BlockFrequency(int n)
{
    int M = 128;

    int		i, j, N, blockSum;
    double	p_value, sum, pi, v, chi_squared;

    N = n / M; 		/* # OF SUBSTRING BLOCKS      */
    sum = 0.0;

    for (i = 0; i < N; i++) {
        blockSum = 0;
        for (j = 0; j < M; j++)
            blockSum += epsilon[j + i * M];
        pi = (double)blockSum / (double)M;
        v = pi - 0.5;
        sum += v * v;
    }
    chi_squared = 4.0 * M * sum;
    p_value = cephes_igamc(N / 2.0, chi_squared / 2.0);
    return p_value;
}

double test03CumulativeSums(int n)
{
    int		S, sup, inf, z, zrev, k;
    double	sum1, sum2, p_value;

    S = 0;
    sup = 0;
    inf = 0;
    for (k = 0; k < n; k++) {
        epsilon[k] ? S++ : S--;
        if (S > sup)
            sup++;
        if (S < inf)
            inf--;
        z = (sup > -inf) ? sup : -inf;
        zrev = (sup - S > S - inf) ? sup - S : S - inf;
    }

    // forward
    sum1 = 0.0;
    for (k = (-n / z + 1) / 4; k <= (n / z - 1) / 4; k++) {
        sum1 += cephes_normal(((4 * k + 1) * z) / sqrt(n));
        sum1 -= cephes_normal(((4 * k - 1) * z) / sqrt(n));
    }
    sum2 = 0.0;
    for (k = (-n / z - 3) / 4; k <= (n / z - 1) / 4; k++) {
        sum2 += cephes_normal(((4 * k + 3) * z) / sqrt(n));
        sum2 -= cephes_normal(((4 * k + 1) * z) / sqrt(n));
    }

    p_value = 1.0 - sum1 + sum2;

    // backwards
    sum1 = 0.0;
    for (k = (-n / zrev + 1) / 4; k <= (n / zrev - 1) / 4; k++) {
        sum1 += cephes_normal(((4 * k + 1) * zrev) / sqrt(n));
        sum1 -= cephes_normal(((4 * k - 1) * zrev) / sqrt(n));
    }
    sum2 = 0.0;
    for (k = (-n / zrev - 3) / 4; k <= (n / zrev - 1) / 4; k++) {
        sum2 += cephes_normal(((4 * k + 3) * zrev) / sqrt(n));
        sum2 -= cephes_normal(((4 * k + 1) * zrev) / sqrt(n));
    }
    p_value = MIN(p_value, 1.0 - sum1 + sum2);

    return p_value;
}

double test04Runs(int n)
{
    int		S, k;
    double	pi, V, erfc_arg, p_value;

    S = 0;
    for (k = 0; k < n; k++)
        if (epsilon[k])
            S++;
    pi = (double)S / (double)n;

    if (fabs(pi - 0.5) > (2.0 / sqrt(n))) {
        p_value = 0.0;
    }
    else {
        V = 1;
        for (k = 1; k < n; k++)
            if (epsilon[k] != epsilon[k - 1])
                V++;
        erfc_arg = fabs(V - 2.0 * n * pi * (1 - pi)) / (2.0 * pi * (1 - pi) * sqrt(2 * n));
        p_value = erfc(erfc_arg);
    }
    return p_value;
}

double test05LongestRunOfOnes(int n)
{
    double			pval, chi2, pi[7];
    int				run, v_n_obs, N, i, j, K, M, V[7];
    unsigned int	nu[7] = { 0, 0, 0, 0, 0, 0, 0 };

    if (n < 128) {
        return 0.0;
    }
    if (n < 6272) {
        K = 3;
        M = 8;
        V[0] = 1; V[1] = 2; V[2] = 3; V[3] = 4;
        pi[0] = 0.21484375;
        pi[1] = 0.3671875;
        pi[2] = 0.23046875;
        pi[3] = 0.1875;
    }
    else if (n < 750000) {
        K = 5;
        M = 128;
        V[0] = 4; V[1] = 5; V[2] = 6; V[3] = 7; V[4] = 8; V[5] = 9;
        pi[0] = 0.1174035788;
        pi[1] = 0.242955959;
        pi[2] = 0.249363483;
        pi[3] = 0.17517706;
        pi[4] = 0.102701071;
        pi[5] = 0.112398847;
    }
    else {
        K = 6;
        M = 10000;
        V[0] = 10; V[1] = 11; V[2] = 12; V[3] = 13; V[4] = 14; V[5] = 15; V[6] = 16;
        pi[0] = 0.0882;
        pi[1] = 0.2092;
        pi[2] = 0.2483;
        pi[3] = 0.1933;
        pi[4] = 0.1208;
        pi[5] = 0.0675;
        pi[6] = 0.0727;
    }

    N = n / M;
    for (i = 0; i < N; i++) {
        v_n_obs = 0;
        run = 0;
        for (j = 0; j < M; j++) {
            if (epsilon[i * M + j] == 1) {
                run++;
                if (run > v_n_obs)
                    v_n_obs = run;
            }
            else
                run = 0;
        }
        if (v_n_obs < V[0])
            nu[0]++;
        for (j = 0; j <= K; j++) {
            if (v_n_obs == V[j])
                nu[j]++;
        }
        if (v_n_obs > V[K])
            nu[K]++;
    }

    chi2 = 0.0;
    for (i = 0; i <= K; i++)
        chi2 += ((nu[i] - N * pi[i]) * (nu[i] - N * pi[i])) / (N * pi[i]);

    pval = cephes_igamc((double)(K / 2.0), chi2 / 2.0);
    return pval;
}

double test06Rank(int n)
{
    int			N, i, k, r;
    double		p_value, product, chi_squared, arg1, p_32, p_31, p_30, R, F_32, F_31, F_30;
    BitSequence** matrix = create_matrix(32, 32);

    N = n / (32 * 32);
    if (isZero(N)) {
        p_value = 0.00;
    }
    else {
        r = 32;					/* COMPUTE PROBABILITIES */
        product = 1;
        for (i = 0; i <= r - 1; i++)
            product *= ((1.e0 - pow(2, i - 32)) * (1.e0 - pow(2, i - 32))) / (1.e0 - pow(2, i - r));
        p_32 = pow(2, r * (32 + 32 - r) - 32 * 32) * product;

        r = 31;
        product = 1;
        for (i = 0; i <= r - 1; i++)
            product *= ((1.e0 - pow(2, i - 32)) * (1.e0 - pow(2, i - 32))) / (1.e0 - pow(2, i - r));
        p_31 = pow(2, r * (32 + 32 - r) - 32 * 32) * product;

        p_30 = 1 - (p_32 + p_31);

        F_32 = 0;
        F_31 = 0;
        for (k = 0; k < N; k++) {			/* FOR EACH 32x32 MATRIX   */
            def_matrix(32, 32, matrix, k);
#if 0
            display_matrix(32, 32, matrix);
#endif
            R = computeRank(32, 32, matrix);
            if (R == 32)
                F_32++;			/* DETERMINE FREQUENCIES */
            if (R == 31)
                F_31++;
        }
        F_30 = (double)N - (F_32 + F_31);

        chi_squared = (pow(F_32 - N * p_32, 2) / (double)(N * p_32) +
            pow(F_31 - N * p_31, 2) / (double)(N * p_31) +
            pow(F_30 - N * p_30, 2) / (double)(N * p_30));

        arg1 = -chi_squared / 2.e0;

        p_value = exp(arg1);
        for (i = 0; i < 32; i++)				/* DEALLOCATE MATRIX  */
            free(matrix[i]);
        free(matrix);
    }
    return p_value;
}

void  __ogg_fdrffti(int n, double* wsave, int* ifac);
void  __ogg_fdrfftf(int n, double* X, double* wsave, int* ifac);

double test07DiscreteFourierTransform(int n)
{
    double	p_value, upperBound, percentile, N_l, N_o, d, * m = NULL, * X = NULL, * wsave = NULL;
    int		i, count, ifac[15];

    if (((X = (double*)calloc(n, sizeof(double))) == NULL) ||
        ((wsave = (double*)calloc(2 * n, sizeof(double))) == NULL) ||
        ((m = (double*)calloc(n / 2 + 1, sizeof(double))) == NULL)) {
        if (X != NULL)
            free(X);
        if (wsave != NULL)
            free(wsave);
        if (m != NULL)
            free(m);
        return 0;
    }
    for (i = 0; i < n; i++)
        X[i] = 2 * (int)epsilon[i] - 1;

    __ogg_fdrffti(n, wsave, ifac);		/* INITIALIZE WORK ARRAYS */
    __ogg_fdrfftf(n, X, wsave, ifac);	/* APPLY FORWARD FFT */

    m[0] = sqrt(X[0] * X[0]);	    /* COMPUTE MAGNITUDE */

    for (i = 0; i < n / 2; i++)
        m[i + 1] = sqrt(pow(X[2 * i + 1], 2) + pow(X[2 * i + 2], 2));
    count = 0;				       /* CONFIDENCE INTERVAL */
    upperBound = sqrt(2.995732274 * n);
    for (i = 0; i < n / 2; i++)
        if (m[i] < upperBound)
            count++;
    percentile = (double)count / (n / 2) * 100;
    N_l = (double)count;       /* number of peaks less than h = sqrt(3*n) */
    N_o = (double)0.95 * n / 2.0;
    d = (N_l - N_o) / sqrt(n / 4.0 * 0.95 * 0.05);
    p_value = erfc(fabs(d) / sqrt(2.0));

    free(X);
    free(wsave);
    free(m);

    return p_value;
}

int is_peridic(int m, unsigned char data[])
{
    int i;
    int l, n;

    for (l = 1; l < m; l++) {
        n = m / l;
        if (memcmp(data, data + n * l, m - n * l)) {
            continue;
        }
        for (i = 1; i < n; i++) {
            if (memcmp(data, data + i * l, l)) {
                break;
            }
        }
        if (i == n) {
            return 1;
        }
    }
    return 0;
}

int generate_template(int m, int i, unsigned char data[])
{
    int j;
    for (j = 0; j < m; j++) {
        data[j] = (i >> j) & 0x01;
    }
    if (!is_peridic(m, data)) {
        return 1;
    }
    return 0;
}


double test08NonOverlappingTemplateMatchings(int n)
{
    int m = 9;

    int		numOfTemplates[100] = { 0, 0, 2, 4, 6, 12, 20, 40, 74, 148, 284, 568, 1116,
                        2232, 4424, 8848, 17622, 35244, 70340, 140680, 281076, 562152 };
    /*----------------------------------------------------------------------------
    NOTE:  Should additional templates lengths beyond 21 be desired, they must
    first be constructed, saved into files and then the corresponding
    number of nonperiodic templates for that file be stored in the m-th
    position in the numOfTemplates variable.
    ----------------------------------------------------------------------------*/
    unsigned int	W_obs, nu[6], * Wj = NULL;
    FILE* fp = NULL;
    double			sum, chi2, p_value, lambda, pi[6], varWj;
    int				i, j, jj, k, kk, match, SKIP, M, N, K = 5;
    BitSequence* sequence = NULL;

    N = 8;
    M = n / N;

    if ((Wj = (unsigned int*)calloc(N, sizeof(unsigned int))) == NULL) {
        return 0;
    }
    lambda = (M - m + 1) / pow(2, m);
    varWj = M * (1.0 / pow(2.0, m) - (2.0 * m - 1.0) / pow(2.0, 2.0 * m));

    if (((isNegative(lambda)) || (isZero(lambda))) ||
        ((sequence = (BitSequence*)calloc(m, sizeof(BitSequence))) == NULL)) {
        if (sequence != NULL)
            free(sequence);
        free(Wj);
        return 0;
    }

    if (numOfTemplates[m] < MAXNUMOFTEMPLATES)
        SKIP = 1;
    else
        SKIP = (int)(numOfTemplates[m] / MAXNUMOFTEMPLATES);
    numOfTemplates[m] = (int)numOfTemplates[m] / SKIP;

    sum = 0.0;
    for (i = 0; i < 2; i++) {                      /* Compute Probabilities */
        pi[i] = exp(-lambda + i * log(lambda) - cephes_lgam(i + 1));
        sum += pi[i];
    }
    pi[0] = sum;
    for (i = 2; i <= K; i++) {                      /* Compute Probabilities */
        pi[i - 1] = exp(-lambda + i * log(lambda) - cephes_lgam(i + 1));
        sum += pi[i - 1];
    }
    pi[K] = 1 - sum;

    kk = 0;
    p_value = 1.0;
    for (jj = 0; jj < MIN(MAXNUMOFTEMPLATES, numOfTemplates[m]);jj++) {
        sum = 0;

        while (kk < (1 << m)){
            kk++;
            if (generate_template(m, kk, sequence)) {
                break;
            }
        }
        for (k = 0; k <= K; k++)
            nu[k] = 0;
        for (i = 0; i < N; i++) {
            W_obs = 0;
            for (j = 0; j < M - m + 1; j++) {
                match = 1;
                for (k = 0; k < m; k++) {
                    if ((int)sequence[k] != (int)epsilon[i * M + j + k]) {
                        match = 0;
                        break;
                    }
                }
                if (match == 1) {
                    W_obs++;
                    j += m - 1;
                }
            }
            Wj[i] = W_obs;
        }
        sum = 0;
        chi2 = 0.0;                                   /* Compute Chi Square */
        for (i = 0; i < N; i++) {
            chi2 += pow(((double)Wj[i] - lambda) / pow(varWj, 0.5), 2);
        }
        p_value = MIN(p_value, cephes_igamc(N / 2.0, chi2 / 2.0));
    }

    free(sequence);
    free(Wj);
    return p_value;
}

double
Pr(int u, double eta)
{
    int		l;
    double	sum, p;

    if (u == 0)
        p = exp(-eta);
    else {
        sum = 0.0;
        for (l = 1; l <= u; l++)
            sum += exp(-eta - u * log(2) + l * log(eta) - cephes_lgam(l + 1) + cephes_lgam(u) - cephes_lgam(l) - cephes_lgam(u - l + 1));
        p = sum;
    }
    return p;
}


double test09OverlappingTemplateMatchings(int n)
{
    int m = 9;
    int				i, k, match;
    double			W_obs, eta, sum, chi2, p_value, lambda;
    int				M, N, j, K = 5;
    unsigned int	nu[6] = { 0, 0, 0, 0, 0, 0 };
    //double			pi[6] = { 0.143783, 0.139430, 0.137319, 0.124314, 0.106209, 0.348945 };
    double			pi[6] = { 0.364091, 0.185659, 0.139381, 0.100571, 0.0704323, 0.139865 };
    BitSequence* sequence;

    M = 1032;
    N = n / M;

    if ((sequence = (BitSequence*)calloc(m, sizeof(BitSequence))) == NULL) {
        return 0;
    }
    else
        for (i = 0; i < m; i++)
            sequence[i] = 1;

    lambda = (double)(M - m + 1) / pow(2, m);
    eta = lambda / 2.0;
    sum = 0.0;
    for (i = 0; i < K; i++) {			/* Compute Probabilities */
        pi[i] = Pr(i, eta);
        sum += pi[i];
    }
    pi[K] = 1 - sum;

    for (i = 0; i < N; i++) {
        W_obs = 0;
        for (j = 0; j < M - m + 1; j++) {
            match = 1;
            for (k = 0; k < m; k++) {
                if (sequence[k] != epsilon[i * M + j + k])
                    match = 0;
            }
            if (match == 1)
                W_obs++;
        }
        if (W_obs <= 4)
            nu[(int)W_obs]++;
        else
            nu[K]++;
    }
    sum = 0;
    chi2 = 0.0;                                   /* Compute Chi Square */
    for (i = 0; i < K + 1; i++) {
        chi2 += pow((double)nu[i] - (double)N * pi[i], 2) / ((double)N * pi[i]);
        sum += nu[i];
    }
    p_value = cephes_igamc(K / 2.0, chi2 / 2.0);

    free(sequence);
    return p_value;
}

double test10Universal(int n)
{
    int		i, j, p, L, Q, K;
    double	arg, sqrt2, sigma, phi, sum, p_value, c;
    long* T = NULL, decRep;
    double	expected_value[17] = { 0, 0, 0, 0, 0, 0, 5.2177052, 6.1962507, 7.1836656,
                8.1764248, 9.1723243, 10.170032, 11.168765,
                12.168070, 13.167693, 14.167488, 15.167379 };
    double   variance[17] = { 0, 0, 0, 0, 0, 0, 2.954, 3.125, 3.238, 3.311, 3.356, 3.384,
                3.401, 3.410, 3.416, 3.419, 3.421 };

    /* * * * * * * * * ** * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
     * THE FOLLOWING REDEFINES L, SHOULD THE CONDITION:     n >= 1010*2^L*L       *
     * NOT BE MET, FOR THE BLOCK LENGTH L.                                        *
     * * * * * * * * * * ** * * * * * * * * * * * * * * * * * * * * * * * * * * * */
    L = 5;
    if (n >= 387840)     L = 6;
    if (n >= 904960)     L = 7;
    if (n >= 2068480)    L = 8;
    if (n >= 4654080)    L = 9;
    if (n >= 10342400)   L = 10;
    if (n >= 22753280)   L = 11;
    if (n >= 49643520)   L = 12;
    if (n >= 107560960)  L = 13;
    if (n >= 231669760)  L = 14;
    if (n >= 496435200)  L = 15;
    if (n >= 1059061760) L = 16;

    Q = 10 * (int)pow(2, L);
    K = (int)(floor(n / L) - (double)Q);	 		    /* BLOCKS TO TEST */

    p = (int)pow(2, L);
    if ((L < 6) || (L > 16) || ((double)Q < 10 * pow(2, L)) ||
        ((T = (long*)calloc(p, sizeof(long))) == NULL)) {
        if (T != NULL) {
            free(T);
        }
        return 0;
    }

    /* COMPUTE THE EXPECTED:  Formula 16, in Marsaglia's Paper */
    c = 0.7 - 0.8 / (double)L + (4 + 32 / (double)L) * pow(K, -3 / (double)L) / 15;
    sigma = c * sqrt(variance[L] / (double)K);
    sqrt2 = sqrt(2);
    sum = 0.0;
    for (i = 0; i < p; i++)
        T[i] = 0;
    for (i = 1; i <= Q; i++) {		/* INITIALIZE TABLE */
        decRep = 0;
        for (j = 0; j < L; j++)
            decRep += epsilon[(i - 1) * L + j] * (long)pow(2, L - 1 - j);
        T[decRep] = i;
    }
    for (i = Q + 1; i <= Q + K; i++) { 	/* PROCESS BLOCKS */
        decRep = 0;
        for (j = 0; j < L; j++)
            decRep += epsilon[(i - 1) * L + j] * (long)pow(2, L - 1 - j);
        sum += log(i - T[decRep]) / log(2);
        T[decRep] = i;
    }
    phi = (double)(sum / (double)K);

    arg = fabs(phi - expected_value[L]) / (sqrt2 * sigma);
    p_value = erfc(arg);

    free(T);
    return p_value;
}

double test11ApproximateEntropy(int n)
{
    int m = 10;
    int				i, j, k, r, blockSize, seqLength, powLen, index;
    double			sum, numOfBlocks, ApEn[2], apen, chi_squared, p_value;
    unsigned int* P;

    seqLength = n;
    r = 0;

    for (blockSize = m; blockSize <= m + 1; blockSize++) {
        if (blockSize == 0) {
            ApEn[0] = 0.00;
            r++;
        }
        else {
            numOfBlocks = (double)seqLength;
            powLen = (int)pow(2, blockSize + 1) - 1;
            if ((P = (unsigned int*)calloc(powLen, sizeof(unsigned int))) == NULL) {
                return 0;
            }
            for (i = 1; i < powLen - 1; i++)
                P[i] = 0;
            for (i = 0; i < numOfBlocks; i++) { /* COMPUTE FREQUENCY */
                k = 1;
                for (j = 0; j < blockSize; j++) {
                    k <<= 1;
                    if ((int)epsilon[(i + j) % seqLength] == 1)
                        k++;
                }
                P[k - 1]++;
            }
            /* DISPLAY FREQUENCY */
            sum = 0.0;
            index = (int)pow(2, blockSize) - 1;
            for (i = 0; i < (int)pow(2, blockSize); i++) {
                if (P[index] > 0)
                    sum += P[index] * log(P[index] / numOfBlocks);
                index++;
            }
            sum /= numOfBlocks;
            ApEn[r] = sum;
            r++;
            free(P);
        }
    }
    apen = ApEn[0] - ApEn[1];

    chi_squared = 2.0 * seqLength * (log(2) - apen);
    p_value = cephes_igamc(pow(2, m - 1), chi_squared / 2.0);
	
    return p_value;
}

double test12RandomExcursions(int n)
{
    int		b, i, j, k, J, x;
    int		cycleStart, cycleStop, * cycle = NULL, * S_k = NULL;
    int		stateX[8] = { -4, -3, -2, -1, 1, 2, 3, 4 };
    int		counter[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    double	p_value, sum, constraint, nu[6][8];
    double	pi[5][6] = { {0.0000000000, 0.00000000000, 0.00000000000, 0.00000000000, 0.00000000000, 0.0000000000},
                         {0.5000000000, 0.25000000000, 0.12500000000, 0.06250000000, 0.03125000000, 0.0312500000},
                         {0.7500000000, 0.06250000000, 0.04687500000, 0.03515625000, 0.02636718750, 0.0791015625},
                         {0.8333333333, 0.02777777778, 0.02314814815, 0.01929012346, 0.01607510288, 0.0803755143},
                         {0.8750000000, 0.01562500000, 0.01367187500, 0.01196289063, 0.01046752930, 0.0732727051} };

    if (((S_k = (int*)calloc(n, sizeof(int))) == NULL) ||
        ((cycle = (int*)calloc(MAX(1000, n / 100), sizeof(int))) == NULL)) {
        if (S_k != NULL)
            free(S_k);
        if (cycle != NULL)
            free(cycle);
        return 0;
    }

    J = 0; 					/* DETERMINE CYCLES */
    S_k[0] = 2 * (int)epsilon[0] - 1;
    for (i = 1; i < n; i++) {
        S_k[i] = S_k[i - 1] + 2 * epsilon[i] - 1;
        if (S_k[i] == 0) {
            J++;
            if (J > MAX(1000, n / 100)) {
                free(S_k);
                free(cycle);
                return 0;
            }
            cycle[J] = i;
        }
    }
    if (S_k[n - 1] != 0)
        J++;
    cycle[J] = n;

    constraint = MAX(0.005 * pow(n, 0.5), 500);
    if (J >= constraint) {
        cycleStart = 0;
        cycleStop = cycle[1];
        for (k = 0; k < 6; k++)
            for (i = 0; i < 8; i++)
                nu[k][i] = 0.;
        for (j = 1; j <= J; j++) {                           /* FOR EACH CYCLE */
            for (i = 0; i < 8; i++)
                counter[i] = 0;
            for (i = cycleStart; i < cycleStop; i++) {
                if ((S_k[i] >= 1 && S_k[i] <= 4) || (S_k[i] >= -4 && S_k[i] <= -1)) {
                    if (S_k[i] < 0)
                        b = 4;
                    else
                        b = 3;
                    counter[S_k[i] + b]++;
                }
            }
            cycleStart = cycle[j] + 1;
            if (j < J)
                cycleStop = cycle[j + 1];

            for (i = 0; i < 8; i++) {
                if ((counter[i] >= 0) && (counter[i] <= 4))
                    nu[counter[i]][i]++;
                else if (counter[i] >= 5)
                    nu[5][i]++;
            }
        }

        p_value = 1;
        for (i = 0; i < 8; i++) {
            x = stateX[i];
            sum = 0.;
            for (k = 0; k < 6; k++)
                sum += pow(nu[k][i] - J * pi[(int)fabs(x)][k], 2) / (J * pi[(int)fabs(x)][k]);
            p_value = MIN(p_value, cephes_igamc(2.5, sum / 2.0));
        }
    }
    free(S_k);
    free(cycle);
    return p_value;
}

double test13RandomExcursionsVariant(int n)
{
    int		i, p, J, x, constraint, count, * S_k;
    int		stateX[18] = { -9, -8, -7, -6, -5, -4, -3, -2, -1, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    double	p_value;

    if ((S_k = (int*)calloc(n, sizeof(int))) == NULL) {
        return 0;
    }
    J = 0;
    S_k[0] = 2 * (int)epsilon[0] - 1;
    for (i = 1; i < n; i++) {
        S_k[i] = S_k[i - 1] + 2 * epsilon[i] - 1;
        if (S_k[i] == 0)
            J++;
    }
    if (S_k[n - 1] != 0)
        J++;

    constraint = (int)MAX(0.005 * pow(n, 0.5), 500);
    if (J >= constraint) {
        p_value = 1;
        for (p = 0; p <= 17; p++) {
            x = stateX[p];
            count = 0;
            for (i = 0; i < n; i++)
                if (S_k[i] == x)
                    count++;
            p_value = MIN(p_value, erfc(fabs(count - J) / (sqrt(2.0 * J * (4.0 * fabs(x) - 2)))));
        }
    }
    free(S_k);
    return p_value;
}

double
psi2(int m, int n)
{
    int				i, j, k, powLen;
    double			sum, numOfBlocks;
    unsigned int* P;

    if ((m == 0) || (m == -1))
        return 0.0;
    numOfBlocks = n;
    powLen = (int)pow(2, m + 1) - 1;
    if ((P = (unsigned int*)calloc(powLen, sizeof(unsigned int))) == NULL) {
        return 0.0;
    }
    for (i = 1; i < powLen - 1; i++)
        P[i] = 0;	  /* INITIALIZE NODES */
    for (i = 0; i < numOfBlocks; i++) {		 /* COMPUTE FREQUENCY */
        k = 1;
        for (j = 0; j < m; j++) {
            if (epsilon[(i + j) % n] == 0)
                k *= 2;
            else if (epsilon[(i + j) % n] == 1)
                k = 2 * k + 1;
        }
        P[k - 1]++;
    }
    sum = 0.0;
    for (i = (int)pow(2, m) - 1; i < (int)pow(2, m + 1) - 1; i++)
        sum += pow(P[i], 2);
    sum = (sum * pow(2, m) / (double)n) - (double)n;
    free(P);

    return sum;
}

double test14Serial(int n)
{
    int m = 16;

    double	p_value1, p_value2, psim0, psim1, psim2, del1, del2;

    psim0 = psi2(m, n);
    psim1 = psi2(m - 1, n);
    psim2 = psi2(m - 2, n);
    del1 = psim0 - psim1;
    del2 = psim0 - 2.0 * psim1 + psim2;
    p_value1 = cephes_igamc(pow(2, m - 1) / 2, del1 / 2.0);
    p_value2 = cephes_igamc(pow(2, m - 2) / 2, del2 / 2.0);
    return MIN(p_value1, p_value2);
}

double test15LinearComplexity(int n)
{
    int M = 500;
    int       i, ii, j, d, N, L, m, N_, parity, sign, K = 6;
    double    p_value, T_, mean, nu[7], chi2;
    double    pi[7] = { 0.01047, 0.03125, 0.12500, 0.50000, 0.25000, 0.06250, 0.020833 };
    BitSequence* T = NULL, * P = NULL, * B_ = NULL, * C = NULL;

    N = (int)floor(n / M);
    if (((B_ = (BitSequence*)calloc(M, sizeof(BitSequence))) == NULL) ||
        ((C = (BitSequence*)calloc(M, sizeof(BitSequence))) == NULL) ||
        ((P = (BitSequence*)calloc(M, sizeof(BitSequence))) == NULL) ||
        ((T = (BitSequence*)calloc(M, sizeof(BitSequence))) == NULL)) {
        printf("Insufficient Memory for Work Space:: Linear Complexity Test\n");
        if (B_ != NULL)
            free(B_);
        if (C != NULL)
            free(C);
        if (P != NULL)
            free(P);
        if (T != NULL)
            free(T);
        return 0;
    }

    for (i = 0; i < K + 1; i++)
        nu[i] = 0.00;
    for (ii = 0; ii < N; ii++) {
        for (i = 0; i < M; i++) {
            B_[i] = 0;
            C[i] = 0;
            T[i] = 0;
            P[i] = 0;
        }
        L = 0;
        m = -1;
        d = 0;
        C[0] = 1;
        B_[0] = 1;

        /* DETERMINE LINEAR COMPLEXITY */
        N_ = 0;
        while (N_ < M) {
            d = (int)epsilon[ii * M + N_];
            for (i = 1; i <= L; i++)
                d += C[i] * epsilon[ii * M + N_ - i];
            d = d % 2;
            if (d == 1) {
                for (i = 0; i < M; i++) {
                    T[i] = C[i];
                    P[i] = 0;
                }
                for (j = 0; j < M; j++)
                    if (B_[j] == 1)
                        P[j + N_ - m] = 1;
                for (i = 0; i < M; i++)
                    C[i] = (C[i] + P[i]) % 2;
                if (L <= N_ / 2) {
                    L = N_ + 1 - L;
                    m = N_;
                    for (i = 0; i < M; i++)
                        B_[i] = T[i];
                }
            }
            N_++;
        }
        if ((parity = (M + 1) % 2) == 0)
            sign = -1;
        else
            sign = 1;
        mean = M / 2.0 + (9.0 + sign) / 36.0 - 1.0 / pow(2, M) * (M / 3.0 + 2.0 / 9.0);
        if ((parity = M % 2) == 0)
            sign = 1;
        else
            sign = -1;
        T_ = sign * (L - mean) + 2.0 / 9.0;

        if (T_ <= -2.5)
            nu[0]++;
        else if (T_ > -2.5 && T_ <= -1.5)
            nu[1]++;
        else if (T_ > -1.5 && T_ <= -0.5)
            nu[2]++;
        else if (T_ > -0.5 && T_ <= 0.5)
            nu[3]++;
        else if (T_ > 0.5 && T_ <= 1.5)
            nu[4]++;
        else if (T_ > 1.5 && T_ <= 2.5)
            nu[5]++;
        else
            nu[6]++;
    }
    chi2 = 0.00;
    for (i = 0; i < K + 1; i++)
        chi2 += pow(nu[i] - N * pi[i], 2) / (N * pi[i]);
    p_value = cephes_igamc(K / 2.0, chi2 / 2.0);

    free(B_);
    free(P);
    free(C);
    free(T);
    return p_value;
}

int nist_randomness_evaluate(unsigned char *rnd)
{
	int n;
	int i,j,k;
	unsigned char rnd_byte;

	double p_value;
	double (*pf[15])(int) = {
        test01Frequency,
        test02BlockFrequency,
        test03CumulativeSums,
        test04Runs,
        test05LongestRunOfOnes,
        test06Rank,
        test07DiscreteFourierTransform,
        test08NonOverlappingTemplateMatchings,
        test09OverlappingTemplateMatchings,
        test10Universal,
        test11ApproximateEntropy,
        test12RandomExcursions,
        test13RandomExcursionsVariant,
        test14Serial,
        test15LinearComplexity
    };
	char* msg[] = {
        "The Frequency (Monobit) Test Passed.\n",
        "Frequency Test within a Block Passed.\n",
        "The Cumulative Sums (Cusums) Test Passed.\n",
        "The Runs Test Passed.\n",
        "Tests for the Longest-Run-of-Ones in a Block Passed.\n",
        "The Binary Matrix Rank Test Passed.\n",
        "The Discrete Fourier Transform (Spectral) Test Passed.\n",
        "The Non-overlapping Template Matching Test Passed.\n",
        "The Overlapping Template Matching Test Passed.\n",
        "Maurer's \"Universal Statistical\" Test Passed.\n",
        "The Approximate Entropy Test Passed.\n",
        "The Random Excursions Test Passed.\n",
        "The Random Excursions Variant Test Passed.\n",
        "The Serial Test Passed.\n",
		"The Linear Complexity Test Passed.\n"
	};

	n = 1024*1024*8;
	epsilon = (BitSequence*)calloc(n, sizeof(BitSequence));

	if (!epsilon) {
		printf("Failed to allocate memory.\n");
		return 20;
	}
	k = 0;
    for (i = 1; i < n / 8; i++) {
		rnd_byte = rnd[i];
//printf("rnd_byte = %d\n", rnd_byte);
//printf("i = %d\n", i);
		for (j = 0; j < 8; j++) {
			epsilon[k] = (rnd_byte & 0x01);
			rnd_byte >>= 1;
            k++;

        }
	}

    for (i = 2; i < 16; i++) {
        //bypass test08.
        if (i == 8) {
            continue;
        }
        p_value = pf[i - 1](n);
        printf("p_value = %.100f\n", p_value);//输出保留小数点后100位
        if (p_value < ALPHA) {
            free(epsilon);

            return i;
        }
        printf("%s", msg[i - 1]);
    }
	free(epsilon);
	return 0;
}


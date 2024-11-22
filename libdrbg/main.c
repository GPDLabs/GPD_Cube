/*
 *  Copyright (C) 2022 - This file is part of libdrbg project
 *
 *  Author:       Ryad BENADJILA <ryad.benadjila@ssi.gouv.fr>
 *  Contributor:  Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */

#include "ctr_drbg_tests.h"
// #include "drbg_tests/test_vectors/ctr_drbg_tests_cases.h"
#include "drbg.h"
#include "drbg_common.h"

// static inline int self_tests(void)
// {
// 	int ret = -1;


// #ifdef WITH_CTR_DRBG
// 	if (do_ctr_dbrg_self_tests(all_ctr_tests, num_ctr_tests))
// 	{
// 		goto err;
// 	}
// #endif
// 	ret = 0;

// err:
// 	return ret;
// }

int main(int argc, char *argv[])
{
	((void)argc);
	((void)argv);
	{
		drbg_ctx drbg;
		drbg_error ret;
		const unsigned char pers_string[] = "DRBG_PERS";
		unsigned char output[1024] = {0};
		unsigned char entropy[256] = {0};
		unsigned char nonce[256] = {0};
		uint32_t max_len = 0;
		drbg_options opt;
		uint32_t security_strength;

		(void)pers_string;
		(void)output;
		(void)entropy;
		(void)nonce;
		(void)max_len;
		(void)opt;
		(void)security_strength;

		/**/
		DRBG_CTR_OPTIONS_INIT(opt, CTR_DRBG_BC_AES256, true, 0);
		ret = drbg_instantiate_with_user_entropy(&drbg, pers_string, sizeof(pers_string) - 1, entropy, sizeof(entropy), nonce, sizeof(nonce), NULL, true, DRBG_CTR, &opt);
		if (ret != DRBG_OK)
		{
			goto err;
		}
		ret = drbg_get_drbg_strength(&drbg, &security_strength);
		if (ret != DRBG_OK)
		{
			goto err;
		}
		printf("DRBG_CTR instantiated with AES256, actual security strength = %u\n", security_strength);
		max_len = 0;
		ret = drbg_get_max_asked_length(&drbg, &max_len);
		if (ret != DRBG_OK)
		{
			goto err;
		}
		printf("drbg_get_max_asked_length: %u\n", max_len);
		ret = drbg_generate(&drbg, NULL, 0, output, (max_len < sizeof(output)) ? max_len : sizeof(output), true);
		if (ret != DRBG_OK)
		{
			goto err;
		}
		hexdump("output:", (const char *)output, sizeof(output));
		// 打开文件
		FILE *file2 = fopen("QR-drbgaesrandom.txt", "ab");
		if (file2 == NULL)
		{
			perror("Error opening file");
			return -1;
		}

		// 将output数组写入文件
		if (fwrite(output, sizeof(output), 1, file2) != 1)
		{
			perror("Error writing to file");
			fclose(file2);
			return -1;
		}

		// 关闭文件
		fclose(file2);
	}
	return 0;
err:
	return -1;
}

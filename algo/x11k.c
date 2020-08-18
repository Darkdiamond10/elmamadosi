#include "miner.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "sha3/sph_blake.h"
#include "sha3/sph_bmw.h"
#include "sha3/sph_groestl.h"
#include "sha3/sph_jh.h"
#include "sha3/sph_keccak.h"
#include "sha3/sph_skein.h"
#include "sha3/sph_luffa.h"
#include "sha3/sph_cubehash.h"
#include "sha3/sph_shavite.h"
#include "sha3/sph_simd.h"
#include "sha3/sph_echo.h"


const uint64_t GetUint64(const void *data, int pos)
{
	const uint8_t *ptr = data + pos * 8;
	return ((uint64_t)ptr[0]) | \
			((uint64_t)ptr[1]) << 8 | \
			((uint64_t)ptr[2]) << 16 | \
			((uint64_t)ptr[3]) << 24 | \
			((uint64_t)ptr[4]) << 32 | \
			((uint64_t)ptr[5]) << 40 | \
			((uint64_t)ptr[6]) << 48 | \
			((uint64_t)ptr[7]) << 56;
}

void processHash(void *oHash, const void *iHash, int index)
{
	sph_blake512_context ctx_blake;
	sph_bmw512_context ctx_bmw;
	sph_groestl512_context ctx_groestl;
	sph_skein512_context ctx_skein;
	sph_jh512_context ctx_jh;
	sph_keccak512_context ctx_keccak;
	sph_echo512_context	ctx_echo1;
	sph_luffa512_context ctx_luffa1;
	sph_cubehash512_context	ctx_cubehash1;
	sph_shavite512_context ctx_shavite1;
	sph_simd512_context	ctx_simd1;

	//these uint512 in the c++ source of the client are backed by an array of uint32
	uint32_t _ALIGN(64) pHash[16];

	switch(index)
	{
		case 0:
			sph_blake512_init(&ctx_blake);
			sph_blake512 (&ctx_blake, iHash, 80);
			sph_blake512_close (&ctx_blake, pHash);
			break;

		case 1:
			sph_bmw512_init(&ctx_bmw);
			sph_bmw512 (&ctx_bmw, iHash, 64);
			sph_bmw512_close(&ctx_bmw, pHash);
			break;

		case 2:
			sph_groestl512_init(&ctx_groestl);
			sph_groestl512 (&ctx_groestl, iHash, 64);
			sph_groestl512_close(&ctx_groestl, pHash);
			break;

		case 3:
			sph_skein512_init(&ctx_skein);
			sph_skein512 (&ctx_skein, iHash, 64);
			sph_skein512_close (&ctx_skein, pHash);
			break;

		case 4:
			sph_jh512_init(&ctx_jh);
			sph_jh512 (&ctx_jh, iHash, 64);
			sph_jh512_close(&ctx_jh, pHash);
			break;

		case 5:
			sph_keccak512_init(&ctx_keccak);
			sph_keccak512 (&ctx_keccak, iHash, 64);
			sph_keccak512_close(&ctx_keccak, pHash);
			break;

		case 6:
			sph_echo512_init (&ctx_echo1);
			sph_echo512 (&ctx_echo1, iHash, 64);
			sph_echo512_close(&ctx_echo1, pHash);
			break;

		case 7:
			sph_luffa512_init (&ctx_luffa1);
			sph_luffa512 (&ctx_luffa1, iHash, 64);
			sph_luffa512_close (&ctx_luffa1, pHash);
			break;

		case 8:
			sph_cubehash512_init (&ctx_cubehash1);
			sph_cubehash512 (&ctx_cubehash1, iHash, 64);
			sph_cubehash512_close(&ctx_cubehash1, pHash);
			break;

		case 9:
			sph_shavite512_init (&ctx_shavite1);
			sph_shavite512 (&ctx_shavite1, iHash, 64);
			sph_shavite512_close(&ctx_shavite1, pHash);
			break;

		case 10:
			sph_simd512_init (&ctx_simd1);
			sph_simd512 (&ctx_simd1, iHash, 64);
			sph_simd512_close(&ctx_simd1, pHash);
			break;

		default:
			break;
	}

	memcpy(oHash, pHash, 64);
}

void x11khash(void *output, const void *input)
{
	const int HASHX11K_NUMBER_ITERATIONS = 64;

	uint32_t _ALIGN(64) hashA[16], hashB[16];
	// uint64_t hashA, hashB;

	// Iteration 0
	processHash(hashA, input, 0);

	for(int i = 1; i < HASHX11K_NUMBER_ITERATIONS; i++) {
		uint64_t index = GetUint64(hashA, i % 8) % 11;
		processHash(hashB, hashA, index);
		memcpy(hashA, hashB, 64);
	}

	memcpy(output, hashA, 32);
}

int scanhash_x11k(int thr_id, struct work *work, uint32_t max_nonce, uint64_t *hashes_done)
{
	uint32_t _ALIGN(128) hash[8];
	uint32_t _ALIGN(128) endiandata[20];
	uint32_t *pdata = work->data;
	uint32_t *ptarget = work->target;

	const uint32_t Htarg = ptarget[7];
	const uint32_t first_nonce = pdata[19];
	uint32_t nonce = first_nonce;
	volatile uint8_t *restart = &(work_restart[thr_id].restart);

	if (opt_benchmark)
		ptarget[7] = 0x0cff;

	for (int k=0; k < 19; k++)
		be32enc(&endiandata[k], pdata[k]);

	do {
		be32enc(&endiandata[19], nonce);
		x11khash(hash, endiandata);

		if (hash[7] <= Htarg && fulltest(hash, ptarget)) {
			work_set_target_ratio(work, hash);
			pdata[19] = nonce;
			*hashes_done = pdata[19] - first_nonce;
			return 1;
		}
		nonce++;

	} while (nonce < max_nonce && !(*restart));

	pdata[19] = nonce;
	*hashes_done = pdata[19] - first_nonce + 1;
	return 0;
}

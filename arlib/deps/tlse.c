// from https://github.com/eduardsui/tlse
#define TLS_AMALGAMATION
#define _XOPEN_SOURCE 600
#undef DEBUG

/********************************************************************************
 Copyright (c) 2016, Eduard Suica
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
 
 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 
 2. Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or other
 materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 ********************************************************************************/
#ifndef TLSE_C
#define TLSE_C

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#define strcasecmp stricmp
#else
// hton* and ntoh* functions
#include <arpa/inet.h>
#include <unistd.h>
#endif

#ifdef TLS_AMALGAMATION
#include "libtomcrypt.c"
#else
#include <tomcrypt.h>
#endif

#include "tlse.h"

#define TLS_DH_DEFAULT_P            "87A8E61DB4B6663CFFBBD19C651959998CEEF608660DD0F25D2CEED4435E3B00E00DF8F1D61957D4FAF7DF4561B2AA3016C3D91134096FAA3BF4296D830E9A7C209E0C6497517ABD5A8A9D306BCF67ED91F9E6725B4758C022E0B1EF4275BF7B6C5BFC11D45F9088B941F54EB1E59BB8BC39A0BF12307F5C4FDB70C581B23F76B63ACAE1CAA6B7902D52526735488A0EF13C6D9A51BFA4AB3AD8347796524D8EF6A167B5A41825D967E144E5140564251CCACB83E6B486F6B3CA3F7971506026C0B857F689962856DED4010ABD0BE621C3A3960A54E710C375F26375D7014103A4B54330C198AF126116D2276E11715F693877FAD7EF09CADB094AE91E1A1597"
#define TLS_DH_DEFAULT_G            "3FB32C9B73134D0B2E77506660EDBD484CA7B18F21EF205407F4793A1A0BA12510DBC15077BE463FFF4FED4AAC0BB555BE3A6C1B0C6B47B1BC3773BF7E8C6F62901228F8C28CBB18A55AE31341000A650196F931C77A57F2DDF463E5E9EC144B777DE62AAAB8A8628AC376D282D6ED3864E67982428EBC831D14348F6F2F9193B5045AF2767164E1DFC967C1FB3F2E55A4BD1BFFE83B9C80D052B985D182EA0ADB2A3B7313D3FE14C8484B1E052588B9B7D2BBD2DF016199ECD06E1557CD0915B3353BBB64E0EC377FD028370DF92B52C7891428CDC67EB6184B523D1DB246C32F63078490F00EF8D647D148D47954515E2327CFEF98C582664B4C0F6CC41659"
#define __TLS_DHE_KEY_SIZE          2048

#define TLS_MALLOC(size)        malloc(size)
#define TLS_REALLOC(ptr, size)  realloc(ptr, size)
#define TLS_FREE(ptr)           if (ptr) free(ptr)

#ifdef DEBUG
#define DEBUG_PRINT(...)            fprintf(stderr, __VA_ARGS__)
#define DEBUG_DUMP_HEX(buf, len)    {int i; for (i = 0; i < len; i++) { DEBUG_PRINT("%02X ", (unsigned int)(buf)[i]); } }
#define DEBUG_INDEX(fields)         print_index(fields)
#define DEBUG_DUMP(buf, length)     fwrite(buf, 1, length, stderr);
#define DEBUG_DUMP_HEX_LABEL(title, buf, len)    {fprintf(stderr, "%s (%i): ", title, (int)len); DEBUG_DUMP_HEX(buf, len); fprintf(stderr, "\n");}
#else
#define DEBUG_PRINT(...)            { }
#define DEBUG_DUMP_HEX(buf, len)    { }
#define DEBUG_INDEX(fields)         { }
#define DEBUG_DUMP(buf, length)     { }
#define DEBUG_DUMP_HEX_LABEL(title, buf, len) { }
#endif

#ifndef htonll
#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#endif

#ifndef ntohll
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))
#endif

#define TLS_CHANGE_CIPHER       0x14
#define TLS_ALERT               0x15
#define TLS_HANDSHAKE           0x16
#define TLS_APPLICATION_DATA    0x17

#define TLS_SERIALIZED_OBJECT   0xFE

#define __TLS_CLIENT_HELLO_MINSIZE  41
#define __TLS_CLIENT_RANDOM_SIZE    32
#define __TLS_SERVER_RANDOM_SIZE    32
#define __TLS_MAX_SESSION_ID        32
#define __TLS_SHA256_MAC_SIZE       32
#define __TLS_SHA1_MAC_SIZE         20
#define __TLS_SHA384_MAC_SIZE       48
#define __TLS_MAX_MAC_SIZE          __TLS_SHA384_MAC_SIZE
#define __TLS_MAX_KEY_EXPANSION_SIZE 192 // 160
// 512bits (sha256) = 64 bytes
#define __TLS_MAX_HASH_LEN          64
#define __TLS_AES_IV_LENGTH         16
#define __TLS_AES_BLOCK_SIZE        16
#define __TLS_AES_GCM_IV_LENGTH     4
#define __TLS_GCM_TAG_LEN           16
#define __TLS_MAX_TAG_LEN           16
#define __TLS_MIN_FINISHED_OPAQUE_LEN 12

#define __TLS_BLOB_INCREMENT        0xFFF
#define __TLS_ASN1_MAXLEVEL         0xFF

#define __DTLS_COOKIE_SIZE          32

#define __TLS_MAX_SHA_SIZE 48
#define __TLS_V11_HASH_SIZE 36      // 16(md5) + 20(sha1)
#define __TLS_MAX_HASH_SIZE __TLS_MAX_SHA_SIZE
#define __TLS_MAX_RSA_KEY   2048    // 16kbits

#define __TLS_MAX_TLS_APP_SIZE      0x4000
// max 1 second sleep
#define __TLS_MAX_ERROR_SLEEP_uS    1000000
// max 5 seconds context sleep
#define __TLS_MAX_ERROR_IDLE_S      5

#define VERSION_SUPPORTED(version, err)  if ((version != TLS_V12) && (version != TLS_V11) && (version != TLS_V10) && (version != DTLS_V12) && (version != DTLS_V10)) { if ((version == SSL_V30) && (context->connection_status == 0)) { version = TLS_V12; } else { DEBUG_PRINT("UNSUPPORTED TLS VERSION %x\n", (int)version); return err;} }
#define CHECK_SIZE(size, buf_size, err)  if (((int)size > (int)buf_size) || ((int)buf_size < 0)) return err;
#define TLS_IMPORT_CHECK_SIZE(buf_pos, size, buf_size) if (((int)size > (int)buf_size - buf_pos) || ((int)buf_pos > (int)buf_size)) { DEBUG_PRINT("IMPORT ELEMENT SIZE ERROR\n"); tls_destroy_context(context); return NULL; }
#define CHECK_HANDSHAKE_STATE(context, n, limit)  { if (context->hs_messages[n] >= limit) { DEBUG_PRINT("* UNEXPECTED MESSAGE (%i)\n", (int)n); payload_res = TLS_UNEXPECTED_MESSAGE; break; } context->hs_messages[n]++; }

#ifdef TLS_WITH_CHACHA20_POLY1305
#define __TLS_CHACHA20_IV_LENGTH    12

// ChaCha implementation by D. J. Bernstein
// Public domain.

#define CHACHA_MINKEYLEN 	16
#define CHACHA_NONCELEN		8
#define CHACHA_NONCELEN_96	12
#define CHACHA_CTRLEN		8
#define CHACHA_CTRLEN_96	4
#define CHACHA_STATELEN		(CHACHA_NONCELEN+CHACHA_CTRLEN)
#define CHACHA_BLOCKLEN		64

#define POLY1305_MAX_AAD    32
#define POLY1305_KEYLEN		32
#define POLY1305_TAGLEN		16

#define u_int   unsigned int
#define uint8_t unsigned char
#define u_char  unsigned char
#ifndef NULL
#define NULL (void *)0
#endif

struct chacha_ctx {
	u_int input[16];
	uint8_t ks[CHACHA_BLOCKLEN];
	uint8_t unused;
};

static inline void chacha_keysetup(struct chacha_ctx *x, const u_char *k, u_int kbits);
static inline void chacha_ivsetup(struct chacha_ctx *x, const u_char *iv, const u_char *ctr);
static inline void chacha_ivsetup_96bitnonce(struct chacha_ctx *x, const u_char *iv, const u_char *ctr);
static inline void chacha_encrypt_bytes(struct chacha_ctx *x, const u_char *m, u_char *c, u_int bytes);
static inline int poly1305_generate_key(unsigned char *key256, unsigned char *nonce, unsigned int noncelen, unsigned char *poly_key, unsigned int counter);

#define poly1305_block_size 16
#define poly1305_context poly1305_state_internal_t

typedef unsigned char u8;
typedef unsigned int u32;

typedef struct chacha_ctx chacha_ctx;

#define U8C(v) (v##U)
#define U32C(v) (v##U)

#define U8V(v) ((u8)(v) & U8C(0xFF))
#define U32V(v) ((u32)(v) & U32C(0xFFFFFFFF))

#define ROTL32(v, n) \
  (U32V((v) << (n)) | ((v) >> (32 - (n))))

#define U8TO32_LITTLE(p) \
  (((u32)((p)[0])) | \
   ((u32)((p)[1]) <<  8) | \
   ((u32)((p)[2]) << 16) | \
   ((u32)((p)[3]) << 24))

#define U32TO8_LITTLE(p, v) \
  do { \
    (p)[0] = U8V((v)); \
    (p)[1] = U8V((v) >>  8); \
    (p)[2] = U8V((v) >> 16); \
    (p)[3] = U8V((v) >> 24); \
  } while (0)

#define ROTATE(v,c) (ROTL32(v,c))
#define XOR(v,w) ((v) ^ (w))
#define PLUS(v,w) (U32V((v) + (w)))
#define PLUSONE(v) (PLUS((v),1))

#define QUARTERROUND(a,b,c,d) \
  a = PLUS(a,b); d = ROTATE(XOR(d,a),16); \
  c = PLUS(c,d); b = ROTATE(XOR(b,c),12); \
  a = PLUS(a,b); d = ROTATE(XOR(d,a), 8); \
  c = PLUS(c,d); b = ROTATE(XOR(b,c), 7);

static const char sigma[] = "expand 32-byte k";
static const char tau[] = "expand 16-byte k";

static inline void chacha_keysetup(chacha_ctx *x, const u8 *k, u32 kbits) {
	const char *constants;

	x->input[4] = U8TO32_LITTLE(k + 0);
	x->input[5] = U8TO32_LITTLE(k + 4);
	x->input[6] = U8TO32_LITTLE(k + 8);
	x->input[7] = U8TO32_LITTLE(k + 12);
	if (kbits == 256) { /* recommended */
		k += 16;
		constants = sigma;
	} else { /* kbits == 128 */
		constants = tau;
	}
	x->input[8] = U8TO32_LITTLE(k + 0);
	x->input[9] = U8TO32_LITTLE(k + 4);
	x->input[10] = U8TO32_LITTLE(k + 8);
	x->input[11] = U8TO32_LITTLE(k + 12);
	x->input[0] = U8TO32_LITTLE(constants + 0);
	x->input[1] = U8TO32_LITTLE(constants + 4);
	x->input[2] = U8TO32_LITTLE(constants + 8);
	x->input[3] = U8TO32_LITTLE(constants + 12);
}

static inline void chacha_key(chacha_ctx *x, u8 *k) {
	U32TO8_LITTLE(k, x->input[4]);
	U32TO8_LITTLE(k + 4, x->input[5]);
	U32TO8_LITTLE(k + 8, x->input[6]);
	U32TO8_LITTLE(k + 12, x->input[7]);

	U32TO8_LITTLE(k + 16, x->input[8]);
	U32TO8_LITTLE(k + 20, x->input[9]);
	U32TO8_LITTLE(k + 24, x->input[10]);
	U32TO8_LITTLE(k + 28, x->input[11]);
}

static inline void chacha_nonce(chacha_ctx *x, u8 *nonce) {
	U32TO8_LITTLE(nonce + 0, x->input[13]);
	U32TO8_LITTLE(nonce + 4, x->input[14]);
	U32TO8_LITTLE(nonce + 8, x->input[15]);
}

static inline void chacha_ivsetup(chacha_ctx *x, const u8 *iv, const u8 *counter) {
    x->input[12] = counter == NULL ? 0 : U8TO32_LITTLE(counter + 0);
    x->input[13] = counter == NULL ? 0 : U8TO32_LITTLE(counter + 4);
    if (iv) {
        x->input[14] = U8TO32_LITTLE(iv + 0);
        x->input[15] = U8TO32_LITTLE(iv + 4);
    }
}

static inline void chacha_ivsetup_96bitnonce(chacha_ctx *x, const u8 *iv, const u8 *counter) {
    x->input[12] = counter == NULL ? 0 : U8TO32_LITTLE(counter + 0);
    if (iv) {
        x->input[13] = U8TO32_LITTLE(iv + 0);
        x->input[14] = U8TO32_LITTLE(iv + 4);
        x->input[15] = U8TO32_LITTLE(iv + 8);
    }
}

static inline void chacha_ivupdate(chacha_ctx *x, const u8 *iv, const u8 *aad, const u8 *counter) {
    x->input[12] = counter == NULL ? 0 : U8TO32_LITTLE(counter + 0);
    x->input[13] = U8TO32_LITTLE(iv + 0);
    x->input[14] = U8TO32_LITTLE(iv + 4) ^ U8TO32_LITTLE(aad);
    x->input[15] = U8TO32_LITTLE(iv + 8) ^ U8TO32_LITTLE(aad + 4);
}

static inline void chacha_encrypt_bytes(chacha_ctx *x, const u8 *m, u8 *c, u32 bytes) {
	u32 x0, x1, x2, x3, x4, x5, x6, x7;
	u32 x8, x9, x10, x11, x12, x13, x14, x15;
	u32 j0, j1, j2, j3, j4, j5, j6, j7;
	u32 j8, j9, j10, j11, j12, j13, j14, j15;
	u8 *ctarget = NULL;
	u8 tmp[64];
	u_int i;

	if (!bytes)
		return;

	j0 = x->input[0];
	j1 = x->input[1];
	j2 = x->input[2];
	j3 = x->input[3];
	j4 = x->input[4];
	j5 = x->input[5];
	j6 = x->input[6];
	j7 = x->input[7];
	j8 = x->input[8];
	j9 = x->input[9];
	j10 = x->input[10];
	j11 = x->input[11];
	j12 = x->input[12];
	j13 = x->input[13];
	j14 = x->input[14];
	j15 = x->input[15];

	for (;;) {
		if (bytes < 64) {
			for (i = 0; i < bytes; ++i)
				tmp[i] = m[i];
			m = tmp;
			ctarget = c;
			c = tmp;
		}
		x0 = j0;
		x1 = j1;
		x2 = j2;
		x3 = j3;
		x4 = j4;
		x5 = j5;
		x6 = j6;
		x7 = j7;
		x8 = j8;
		x9 = j9;
		x10 = j10;
		x11 = j11;
		x12 = j12;
		x13 = j13;
		x14 = j14;
		x15 = j15;
		for (i = 20; i > 0; i -= 2) {
			QUARTERROUND(x0, x4, x8, x12)
			QUARTERROUND(x1, x5, x9, x13)
			QUARTERROUND(x2, x6, x10, x14)
			QUARTERROUND(x3, x7, x11, x15)
			QUARTERROUND(x0, x5, x10, x15)
			QUARTERROUND(x1, x6, x11, x12)
			QUARTERROUND(x2, x7, x8, x13)
			QUARTERROUND(x3, x4, x9, x14)
		}
		x0 = PLUS(x0, j0);
		x1 = PLUS(x1, j1);
		x2 = PLUS(x2, j2);
		x3 = PLUS(x3, j3);
		x4 = PLUS(x4, j4);
		x5 = PLUS(x5, j5);
		x6 = PLUS(x6, j6);
		x7 = PLUS(x7, j7);
		x8 = PLUS(x8, j8);
		x9 = PLUS(x9, j9);
		x10 = PLUS(x10, j10);
		x11 = PLUS(x11, j11);
		x12 = PLUS(x12, j12);
		x13 = PLUS(x13, j13);
		x14 = PLUS(x14, j14);
		x15 = PLUS(x15, j15);

		if (bytes < 64) {
			U32TO8_LITTLE(x->ks + 0, x0);
			U32TO8_LITTLE(x->ks + 4, x1);
			U32TO8_LITTLE(x->ks + 8, x2);
			U32TO8_LITTLE(x->ks + 12, x3);
			U32TO8_LITTLE(x->ks + 16, x4);
			U32TO8_LITTLE(x->ks + 20, x5);
			U32TO8_LITTLE(x->ks + 24, x6);
			U32TO8_LITTLE(x->ks + 28, x7);
			U32TO8_LITTLE(x->ks + 32, x8);
			U32TO8_LITTLE(x->ks + 36, x9);
			U32TO8_LITTLE(x->ks + 40, x10);
			U32TO8_LITTLE(x->ks + 44, x11);
			U32TO8_LITTLE(x->ks + 48, x12);
			U32TO8_LITTLE(x->ks + 52, x13);
			U32TO8_LITTLE(x->ks + 56, x14);
			U32TO8_LITTLE(x->ks + 60, x15);
		}

		x0 = XOR(x0, U8TO32_LITTLE(m + 0));
		x1 = XOR(x1, U8TO32_LITTLE(m + 4));
		x2 = XOR(x2, U8TO32_LITTLE(m + 8));
		x3 = XOR(x3, U8TO32_LITTLE(m + 12));
		x4 = XOR(x4, U8TO32_LITTLE(m + 16));
		x5 = XOR(x5, U8TO32_LITTLE(m + 20));
		x6 = XOR(x6, U8TO32_LITTLE(m + 24));
		x7 = XOR(x7, U8TO32_LITTLE(m + 28));
		x8 = XOR(x8, U8TO32_LITTLE(m + 32));
		x9 = XOR(x9, U8TO32_LITTLE(m + 36));
		x10 = XOR(x10, U8TO32_LITTLE(m + 40));
		x11 = XOR(x11, U8TO32_LITTLE(m + 44));
		x12 = XOR(x12, U8TO32_LITTLE(m + 48));
		x13 = XOR(x13, U8TO32_LITTLE(m + 52));
		x14 = XOR(x14, U8TO32_LITTLE(m + 56));
		x15 = XOR(x15, U8TO32_LITTLE(m + 60));

		j12 = PLUSONE(j12);
		if (!j12) {
			j13 = PLUSONE(j13);
			/*
			 * Stopping at 2^70 bytes per nonce is the user's
			 * responsibility.
			 */
		}

		U32TO8_LITTLE(c + 0, x0);
		U32TO8_LITTLE(c + 4, x1);
		U32TO8_LITTLE(c + 8, x2);
		U32TO8_LITTLE(c + 12, x3);
		U32TO8_LITTLE(c + 16, x4);
		U32TO8_LITTLE(c + 20, x5);
		U32TO8_LITTLE(c + 24, x6);
		U32TO8_LITTLE(c + 28, x7);
		U32TO8_LITTLE(c + 32, x8);
		U32TO8_LITTLE(c + 36, x9);
		U32TO8_LITTLE(c + 40, x10);
		U32TO8_LITTLE(c + 44, x11);
		U32TO8_LITTLE(c + 48, x12);
		U32TO8_LITTLE(c + 52, x13);
		U32TO8_LITTLE(c + 56, x14);
		U32TO8_LITTLE(c + 60, x15);

		if (bytes <= 64) {
			if (bytes < 64) {
				for (i = 0; i < bytes; ++i)
					ctarget[i] = c[i];
			}
			x->input[12] = j12;
			x->input[13] = j13;
			x->unused = 64 - bytes;
			return;
		}
		bytes -= 64;
		c += 64;
		m += 64;
	}
}

static inline void chacha20_block(chacha_ctx *x, unsigned char *c, int len) {
	u_int i;

    unsigned int state[16];
    for (i = 0; i < 16; i++)
        state[i] = x->input[i];
	for (i = 20; i > 0; i -= 2) {
		QUARTERROUND(state[0], state[4], state[8], state[12])
		QUARTERROUND(state[1], state[5], state[9], state[13])
		QUARTERROUND(state[2], state[6], state[10], state[14])
		QUARTERROUND(state[3], state[7], state[11], state[15])
		QUARTERROUND(state[0], state[5], state[10], state[15])
		QUARTERROUND(state[1], state[6], state[11], state[12])
		QUARTERROUND(state[2], state[7], state[8], state[13])
		QUARTERROUND(state[3], state[4], state[9], state[14])
	}

    for (i = 0; i < 16; i++)
        x->input[i] = PLUS(x->input[i], state[i]);

    for (i = 0; i < len; i += 4) {
        U32TO8_LITTLE(c + i, x->input[i/4]);
    }
}

static inline int poly1305_generate_key(unsigned char *key256, unsigned char *nonce, unsigned int noncelen, unsigned char *poly_key, unsigned int counter) {
    struct chacha_ctx ctx;
    uint64_t ctr;
    memset(&ctx, 0, sizeof(ctx));
    chacha_keysetup(&ctx, key256, 256);
    switch (noncelen) {
        case 8:
            ctr = counter;
            chacha_ivsetup(&ctx, nonce, (unsigned char *)&ctr);
            break;
        case 12:
            chacha_ivsetup_96bitnonce(&ctx, nonce, (unsigned char *)&counter);
            break;
        default:
            return -1;
    }
    chacha20_block(&ctx, poly_key, POLY1305_KEYLEN);
    return 0;
}

/* 17 + sizeof(size_t) + 14*sizeof(unsigned long) */
typedef struct poly1305_state_internal_t {
	unsigned long r[5];
	unsigned long h[5];
	unsigned long pad[4];
	size_t leftover;
	unsigned char buffer[poly1305_block_size];
	unsigned char final;
} poly1305_state_internal_t;

/* interpret four 8 bit unsigned integers as a 32 bit unsigned integer in little endian */
static unsigned long U8TO32(const unsigned char *p) {
	return
		(((unsigned long)(p[0] & 0xff)      ) |
	     ((unsigned long)(p[1] & 0xff) <<  8) |
         ((unsigned long)(p[2] & 0xff) << 16) |
         ((unsigned long)(p[3] & 0xff) << 24));
}

/* store a 32 bit unsigned integer as four 8 bit unsigned integers in little endian */
static void U32TO8(unsigned char *p, unsigned long v) {
	p[0] = (v      ) & 0xff;
	p[1] = (v >>  8) & 0xff;
	p[2] = (v >> 16) & 0xff;
	p[3] = (v >> 24) & 0xff;
}

void poly1305_init(poly1305_context *ctx, const unsigned char key[32]) {
	poly1305_state_internal_t *st = (poly1305_state_internal_t *)ctx;

	/* r &= 0xffffffc0ffffffc0ffffffc0fffffff */
	st->r[0] = (U8TO32(&key[ 0])     ) & 0x3ffffff;
	st->r[1] = (U8TO32(&key[ 3]) >> 2) & 0x3ffff03;
	st->r[2] = (U8TO32(&key[ 6]) >> 4) & 0x3ffc0ff;
	st->r[3] = (U8TO32(&key[ 9]) >> 6) & 0x3f03fff;
	st->r[4] = (U8TO32(&key[12]) >> 8) & 0x00fffff;

	/* h = 0 */
	st->h[0] = 0;
	st->h[1] = 0;
	st->h[2] = 0;
	st->h[3] = 0;
	st->h[4] = 0;

	/* save pad for later */
	st->pad[0] = U8TO32(&key[16]);
	st->pad[1] = U8TO32(&key[20]);
	st->pad[2] = U8TO32(&key[24]);
	st->pad[3] = U8TO32(&key[28]);

	st->leftover = 0;
	st->final = 0;
}

static void poly1305_blocks(poly1305_state_internal_t *st, const unsigned char *m, size_t bytes) {
	const unsigned long hibit = (st->final) ? 0 : (1UL << 24); /* 1 << 128 */
	unsigned long r0,r1,r2,r3,r4;
	unsigned long s1,s2,s3,s4;
	unsigned long h0,h1,h2,h3,h4;
	unsigned long long d0,d1,d2,d3,d4;
	unsigned long c;

	r0 = st->r[0];
	r1 = st->r[1];
	r2 = st->r[2];
	r3 = st->r[3];
	r4 = st->r[4];

	s1 = r1 * 5;
	s2 = r2 * 5;
	s3 = r3 * 5;
	s4 = r4 * 5;

	h0 = st->h[0];
	h1 = st->h[1];
	h2 = st->h[2];
	h3 = st->h[3];
	h4 = st->h[4];

	while (bytes >= poly1305_block_size) {
		/* h += m[i] */
		h0 += (U8TO32(m+ 0)     ) & 0x3ffffff;
		h1 += (U8TO32(m+ 3) >> 2) & 0x3ffffff;
		h2 += (U8TO32(m+ 6) >> 4) & 0x3ffffff;
		h3 += (U8TO32(m+ 9) >> 6) & 0x3ffffff;
		h4 += (U8TO32(m+12) >> 8) | hibit;

		/* h *= r */
		d0 = ((unsigned long long)h0 * r0) + ((unsigned long long)h1 * s4) + ((unsigned long long)h2 * s3) + ((unsigned long long)h3 * s2) + ((unsigned long long)h4 * s1);
		d1 = ((unsigned long long)h0 * r1) + ((unsigned long long)h1 * r0) + ((unsigned long long)h2 * s4) + ((unsigned long long)h3 * s3) + ((unsigned long long)h4 * s2);
		d2 = ((unsigned long long)h0 * r2) + ((unsigned long long)h1 * r1) + ((unsigned long long)h2 * r0) + ((unsigned long long)h3 * s4) + ((unsigned long long)h4 * s3);
		d3 = ((unsigned long long)h0 * r3) + ((unsigned long long)h1 * r2) + ((unsigned long long)h2 * r1) + ((unsigned long long)h3 * r0) + ((unsigned long long)h4 * s4);
		d4 = ((unsigned long long)h0 * r4) + ((unsigned long long)h1 * r3) + ((unsigned long long)h2 * r2) + ((unsigned long long)h3 * r1) + ((unsigned long long)h4 * r0);

		/* (partial) h %= p */
		              c = (unsigned long)(d0 >> 26); h0 = (unsigned long)d0 & 0x3ffffff;
		d1 += c;      c = (unsigned long)(d1 >> 26); h1 = (unsigned long)d1 & 0x3ffffff;
		d2 += c;      c = (unsigned long)(d2 >> 26); h2 = (unsigned long)d2 & 0x3ffffff;
		d3 += c;      c = (unsigned long)(d3 >> 26); h3 = (unsigned long)d3 & 0x3ffffff;
		d4 += c;      c = (unsigned long)(d4 >> 26); h4 = (unsigned long)d4 & 0x3ffffff;
		h0 += c * 5;  c =                (h0 >> 26); h0 =                h0 & 0x3ffffff;
		h1 += c;

		m += poly1305_block_size;
		bytes -= poly1305_block_size;
	}

	st->h[0] = h0;
	st->h[1] = h1;
	st->h[2] = h2;
	st->h[3] = h3;
	st->h[4] = h4;
}

void poly1305_finish(poly1305_context *ctx, unsigned char mac[16]) {
	poly1305_state_internal_t *st = (poly1305_state_internal_t *)ctx;
	unsigned long h0,h1,h2,h3,h4,c;
	unsigned long g0,g1,g2,g3,g4;
	unsigned long long f;
	unsigned long mask;

	/* process the remaining block */
	if (st->leftover) {
		size_t i = st->leftover;
		st->buffer[i++] = 1;
		for (; i < poly1305_block_size; i++)
			st->buffer[i] = 0;
		st->final = 1;
		poly1305_blocks(st, st->buffer, poly1305_block_size);
	}

	/* fully carry h */
	h0 = st->h[0];
	h1 = st->h[1];
	h2 = st->h[2];
	h3 = st->h[3];
	h4 = st->h[4];

	             c = h1 >> 26; h1 = h1 & 0x3ffffff;
	h2 +=     c; c = h2 >> 26; h2 = h2 & 0x3ffffff;
	h3 +=     c; c = h3 >> 26; h3 = h3 & 0x3ffffff;
	h4 +=     c; c = h4 >> 26; h4 = h4 & 0x3ffffff;
	h0 += c * 5; c = h0 >> 26; h0 = h0 & 0x3ffffff;
	h1 +=     c;

	/* compute h + -p */
	g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
	g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
	g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
	g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
	g4 = h4 + c - (1UL << 26);

	/* select h if h < p, or h + -p if h >= p */
	mask = (g4 >> ((sizeof(unsigned long) * 8) - 1)) - 1;
	g0 &= mask;
	g1 &= mask;
	g2 &= mask;
	g3 &= mask;
	g4 &= mask;
	mask = ~mask;
	h0 = (h0 & mask) | g0;
	h1 = (h1 & mask) | g1;
	h2 = (h2 & mask) | g2;
	h3 = (h3 & mask) | g3;
	h4 = (h4 & mask) | g4;

	/* h = h % (2^128) */
	h0 = ((h0      ) | (h1 << 26)) & 0xffffffff;
	h1 = ((h1 >>  6) | (h2 << 20)) & 0xffffffff;
	h2 = ((h2 >> 12) | (h3 << 14)) & 0xffffffff;
	h3 = ((h3 >> 18) | (h4 <<  8)) & 0xffffffff;

	/* mac = (h + pad) % (2^128) */
	f = (unsigned long long)h0 + st->pad[0]            ; h0 = (unsigned long)f;
	f = (unsigned long long)h1 + st->pad[1] + (f >> 32); h1 = (unsigned long)f;
	f = (unsigned long long)h2 + st->pad[2] + (f >> 32); h2 = (unsigned long)f;
	f = (unsigned long long)h3 + st->pad[3] + (f >> 32); h3 = (unsigned long)f;

	U32TO8(mac +  0, h0);
	U32TO8(mac +  4, h1);
	U32TO8(mac +  8, h2);
	U32TO8(mac + 12, h3);

	/* zero out the state */
	st->h[0] = 0;
	st->h[1] = 0;
	st->h[2] = 0;
	st->h[3] = 0;
	st->h[4] = 0;
	st->r[0] = 0;
	st->r[1] = 0;
	st->r[2] = 0;
	st->r[3] = 0;
	st->r[4] = 0;
	st->pad[0] = 0;
	st->pad[1] = 0;
	st->pad[2] = 0;
	st->pad[3] = 0;
}

void poly1305_update(poly1305_context *ctx, const unsigned char *m, size_t bytes) {
	poly1305_state_internal_t *st = (poly1305_state_internal_t *)ctx;
	size_t i;
	/* handle leftover */
	if (st->leftover) {
		size_t want = (poly1305_block_size - st->leftover);
		if (want > bytes)
			want = bytes;
		for (i = 0; i < want; i++)
			st->buffer[st->leftover + i] = m[i];
		bytes -= want;
		m += want;
		st->leftover += want;
		if (st->leftover < poly1305_block_size)
			return;
		poly1305_blocks(st, st->buffer, poly1305_block_size);
		st->leftover = 0;
	}

	/* process full blocks */
	if (bytes >= poly1305_block_size) {
		size_t want = (bytes & ~(poly1305_block_size - 1));
		poly1305_blocks(st, m, want);
		m += want;
		bytes -= want;
	}

	/* store leftover */
	if (bytes) {
		for (i = 0; i < bytes; i++)
			st->buffer[st->leftover + i] = m[i];
		st->leftover += bytes;
	}
}

int poly1305_verify(const unsigned char mac1[16], const unsigned char mac2[16]) {
	size_t i;
	unsigned int dif = 0;
	for (i = 0; i < 16; i++)
		dif |= (mac1[i] ^ mac2[i]);
	dif = (dif - 1) >> ((sizeof(unsigned int) * 8) - 1);
	return (dif & 1);
}

void chacha20_poly1305_key(struct chacha_ctx *ctx, unsigned char *poly1305_key) {
    unsigned char key[32];
    unsigned char nonce[12];
    chacha_key(ctx, key);
    chacha_nonce(ctx, nonce);
    poly1305_generate_key(key, nonce, sizeof(nonce), poly1305_key, 0);
}

int chacha20_poly1305_aead(struct chacha_ctx *ctx,  unsigned char *pt, unsigned int len, unsigned char *aad, unsigned int aad_len, unsigned char *poly_key, unsigned char *out) {
    static unsigned char zeropad[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (aad_len > POLY1305_MAX_AAD)
        return -1;

    unsigned int counter = 1;
    chacha_ivsetup_96bitnonce(ctx, NULL, (unsigned char *)&counter);
    chacha_encrypt_bytes(ctx, pt, out, len);
    
    poly1305_context aead_ctx;
    poly1305_init(&aead_ctx, poly_key);
    poly1305_update(&aead_ctx, aad, aad_len);
    int rem = aad_len % 16;
    if (rem)
        poly1305_update(&aead_ctx, zeropad, 16 - rem);
    poly1305_update(&aead_ctx, out, len);
    rem = len % 16;
    if (rem)
        poly1305_update(&aead_ctx, zeropad, 16 - rem);

    unsigned char trail[16];
    U32TO8(&trail[0], aad_len);
    *(int *)&trail[4] = 0;
    U32TO8(&trail[8], len);
    *(int *)&trail[12] = 0;

    poly1305_update(&aead_ctx, trail, 16);
    poly1305_finish(&aead_ctx, out + len);
    
    return len + POLY1305_TAGLEN;
}
#endif

typedef enum {
    KEA_dhe_dss,
    KEA_dhe_rsa,
    KEA_dh_anon,
    KEA_rsa,
    KEA_dh_dss,
    KEA_dh_rsa,
    KEA_ec_diffie_hellman
} KeyExchangeAlgorithm;

typedef enum {
    rsa_sign = 1,
    dss_sign = 2,
    rsa_fixed_dh = 3,
    dss_fixed_dh = 4,
    rsa_ephemeral_dh_RESERVED = 5,
    dss_ephemeral_dh_RESERVED = 6,
    fortezza_dms_RESERVED = 20,
    ecdsa_sign = 64,
    rsa_fixed_ecdh = 65,
    ecdsa_fixed_ecdh = 66
} TLSClientCertificateType;

typedef enum {
    none = 0,
    md5 = 1,
    sha1 = 2,
    sha224 = 3,
    sha256 = 4,
    sha384 = 5,
    sha512 = 6,
    __md5_sha1 = 255
} TLSHashAlgorithm;

typedef enum {
    anonymous = 0,
    rsa = 1,
    dsa = 2,
    ecdsa = 3
} TLSSignatureAlgorithm;

struct __private_OID_chain {
    void *top;
    unsigned char *oid;
};

struct TLSCertificate {
    unsigned short version;
    unsigned int algorithm;
    unsigned int key_algorithm;
    unsigned int ec_algorithm;
    unsigned char *exponent;
    unsigned int exponent_len;
    unsigned char *pk;
    unsigned int pk_len;
    unsigned char *priv;
    unsigned int priv_len;
    unsigned char *issuer_country;
    unsigned char *issuer_state;
    unsigned char *issuer_location;
    unsigned char *issuer_entity;
    unsigned char *issuer_subject;
    unsigned char *not_before;
    unsigned char *not_after;
    unsigned char *country;
    unsigned char *state;
    unsigned char *location;
    unsigned char *entity;
    unsigned char *subject;
    unsigned char **san;
    unsigned short san_length;
    unsigned char *ocsp;
    unsigned char *serial_number;
    unsigned int serial_len;
    unsigned char *sign_key;
    unsigned int sign_len;
    unsigned char *fingerprint;
    unsigned char *der_bytes;
    unsigned int der_len;
    unsigned char *bytes;
    unsigned int len;
};

typedef struct {
    union {
        symmetric_CBC aes_local;
        gcm_state aes_gcm_local;
#ifdef TLS_WITH_CHACHA20_POLY1305
        chacha_ctx chacha_local;
#endif
    } ctx_local;
    union {
        symmetric_CBC aes_remote;
        gcm_state aes_gcm_remote;
#ifdef TLS_WITH_CHACHA20_POLY1305
        chacha_ctx chacha_remote;
#endif
    } ctx_remote;
    union {
        unsigned char local_mac[__TLS_MAX_MAC_SIZE];
        unsigned char local_aead_iv[__TLS_AES_GCM_IV_LENGTH];
#ifdef TLS_WITH_CHACHA20_POLY1305
        unsigned char local_nonce[__TLS_CHACHA20_IV_LENGTH];
#endif
    } ctx_local_mac;
    union {
        unsigned char remote_aead_iv[__TLS_AES_GCM_IV_LENGTH];
        unsigned char remote_mac[__TLS_MAX_MAC_SIZE];
#ifdef TLS_WITH_CHACHA20_POLY1305
        unsigned char remote_nonce[__TLS_CHACHA20_IV_LENGTH];
#endif
    } ctx_remote_mac;
    unsigned char created;
} TLSCipher;

typedef struct {
    hash_state hash;
#ifdef TLS_LEGACY_SUPPORT
    hash_state hash2;
#endif
    unsigned char created;
} TLSHash;

#ifdef TLS_FORWARD_SECRECY
#define mp_init(a)                           ltc_mp.init(a)
#define mp_init_multi                        ltc_init_multi
#define mp_clear(a)                          ltc_mp.deinit(a)
#define mp_clear_multi                       ltc_deinit_multi
#define mp_count_bits(a)                     ltc_mp.count_bits(a)
#define mp_read_radix(a, b, c)               ltc_mp.read_radix(a, b, c)
#define mp_unsigned_bin_size(a)              ltc_mp.unsigned_size(a)
#define mp_to_unsigned_bin(a, b)             ltc_mp.unsigned_write(a, b)
#define mp_read_unsigned_bin(a, b, c)        ltc_mp.unsigned_read(a, b, c)
#define mp_exptmod(a, b, c, d)               ltc_mp.exptmod(a, b, c, d)
#define mp_add(a, b, c)                      ltc_mp.add(a, b, c)
#define mp_mul(a, b, c)                      ltc_mp.mul(a, b, c)
#define mp_cmp(a, b)                         ltc_mp.compare(a, b)
#define mp_cmp_d(a, b)                       ltc_mp.compare_d(a, b)
#define mp_sqr(a, b)                         ltc_mp.sqr(a, b)
#define mp_mod(a, b, c)                      ltc_mp.mpdiv(a, b, NULL, c)
#define mp_sub(a, b, c)                      ltc_mp.sub(a, b, c)
#define mp_set(a, b)                         ltc_mp.set_int(a, b)

typedef struct {
    void *x;
    void *y;
    void *p;
    void *g;
} DHKey;

struct ECCCurveParameters {
    int size;
    int iana;
    const char *name;
    const char *P;
    const char *A;
    const char *B;
    const char *Gx;
    const char *Gy;
    const char *order;
    ltc_ecc_set_type dp;
};

static struct ECCCurveParameters secp192r1 = {
    24,
    19,
    "secp192r1",
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFF", // P
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFC", // A
    "64210519E59C80E70FA7E9AB72243049FEB8DEECC146B9B1", // B
    "188DA80EB03090F67CBF20EB43A18800F4FF0AFD82FF1012", // Gx
    "07192B95FFC8DA78631011ED6B24CDD573F977A11E794811", // Gy
    "FFFFFFFFFFFFFFFFFFFFFFFF99DEF836146BC9B1B4D22831"  // order (n)
};


static struct ECCCurveParameters secp224r1 = {
    28,
    21,
    "secp224r1",
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF000000000000000000000001", // P
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFE", // A
    "B4050A850C04B3ABF54132565044B0B7D7BFD8BA270B39432355FFB4", // B
    "B70E0CBD6BB4BF7F321390B94A03C1D356C21122343280D6115C1D21", // Gx
    "BD376388B5F723FB4C22DFE6CD4375A05A07476444D5819985007E34", // Gy
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFF16A2E0B8F03E13DD29455C5C2A3D"  // order (n)
};

static struct ECCCurveParameters secp224k1 = {
    28,
    20,
    "secp224k1",
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFE56D", // P
    "00000000000000000000000000000000000000000000000000000000", // A
    "00000000000000000000000000000000000000000000000000000005", // B
    "A1455B334DF099DF30FC28A169A467E9E47075A90F7E650EB6B7A45C", // Gx
    "7E089FED7FBA344282CAFBD6F7E319F7C0B0BD59E2CA4BDB556D61A5", // Gy
    "0000000000000000000000000001DCE8D2EC6184CAF0A971769FB1F7"  // order (n)
};

static struct ECCCurveParameters secp256r1 = {
    32,
    23,
    "secp256r1",
    "FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFF", // P
    "FFFFFFFF00000001000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFC", // A
    "5AC635D8AA3A93E7B3EBBD55769886BC651D06B0CC53B0F63BCE3C3E27D2604B", // B
    "6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296", // Gx
    "4FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5", // Gy
    "FFFFFFFF00000000FFFFFFFFFFFFFFFFBCE6FAADA7179E84F3B9CAC2FC632551"  // order (n)
};

static struct ECCCurveParameters secp256k1 = {
    32,
    22,
    "secp256k1",
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F", // P
    "0000000000000000000000000000000000000000000000000000000000000000", // A
    "0000000000000000000000000000000000000000000000000000000000000007", // B
    "79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798", // Gx
    "483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8", // Gy
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141"  // order (n)
};

static struct ECCCurveParameters secp384r1 = {
    48,
    24,
    "secp384r1",
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFF0000000000000000FFFFFFFF", // P
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFFFF0000000000000000FFFFFFFC", // A
    "B3312FA7E23EE7E4988E056BE3F82D19181D9C6EFE8141120314088F5013875AC656398D8A2ED19D2A85C8EDD3EC2AEF", // B
    "AA87CA22BE8B05378EB1C71EF320AD746E1D3B628BA79B9859F741E082542A385502F25DBF55296C3A545E3872760AB7", // Gx
    "3617DE4A96262C6F5D9E98BF9292DC29F8F41DBD289A147CE9DA3113B5F0B8C00A60B1CE1D7E819D7A431D7C90EA0E5F", // Gy
    "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC7634D81F4372DDF581A0DB248B0A77AECEC196ACCC52973"  // order (n)
};

static struct ECCCurveParameters secp521r1 = {
    66,
    25,
    "secp521r1",
    "01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", // P
    "01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFC", // A
    "0051953EB9618E1C9A1F929A21A0B68540EEA2DA725B99B315F3B8B489918EF109E156193951EC7E937B1652C0BD3BB1BF073573DF883D2C34F1EF451FD46B503F00", // B
    "00C6858E06B70404E9CD9E3ECB662395B4429C648139053FB521F828AF606B4D3DBAA14B5E77EFE75928FE1DC127A2FFA8DE3348B3C1856A429BF97E7E31C2E5BD66", // Gx
    "011839296A789A3BC0045C8A5FB42C7D1BD998F54449579B446817AFBD17273E662C97EE72995EF42640C550B9013FAD0761353C7086A272C24088BE94769FD16650", // Gy
    "01FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFA51868783BF2F966B7FCC0148F709A5D03BB5C9B8899C47AEBB6FB71E91386409"  // order (n)
};

static struct ECCCurveParameters *default_curve = &secp256r1;

void init_curve(struct ECCCurveParameters *curve) {
    curve->dp.size = curve->size;
    curve->dp.name = (char *)curve->name;
    curve->dp.B = (char *)curve->B;
    curve->dp.prime = (char *)curve->P;
    curve->dp.Gx = (char *)curve->Gx;
    curve->dp.Gy = (char *)curve->Gy;
    curve->dp.order = (char *)curve->order;
}

void init_curves() {
    init_curve(&secp192r1);
    init_curve(&secp224r1);
    init_curve(&secp224k1);
    init_curve(&secp256r1);
    init_curve(&secp256k1);
    init_curve(&secp384r1);
    init_curve(&secp521r1);
}
#endif

struct TLSContext {
    unsigned char remote_random[__TLS_CLIENT_RANDOM_SIZE];
    unsigned char local_random[__TLS_SERVER_RANDOM_SIZE];
    unsigned char session[__TLS_MAX_SESSION_ID];
    unsigned char session_size;
    unsigned short cipher;
    unsigned short version;
    unsigned char is_server;
    struct TLSCertificate **certificates;
    struct TLSCertificate *private_key;
#ifdef TLS_ECDSA_SUPPORTED
    struct TLSCertificate *ec_private_key;
#endif
#ifdef TLS_FORWARD_SECRECY
    DHKey *dhe;
    ecc_key *ecc_dhe;
    char *default_dhe_p;
    char *default_dhe_g;
    const struct ECCCurveParameters *curve;
#endif
    struct TLSCertificate **client_certificates;
    unsigned int certificates_count;
    unsigned int client_certificates_count;
    unsigned char *master_key;
    unsigned int master_key_len;
    unsigned char *premaster_key;
    unsigned int premaster_key_len;
    unsigned char cipher_spec_set;
    TLSCipher crypto;
    TLSHash *handshake_hash;
    
    unsigned char *message_buffer;
    unsigned int message_buffer_len;
    uint64_t remote_sequence_number;
    uint64_t local_sequence_number;
    
    unsigned char connection_status;
    unsigned char critical_error;
    unsigned char error_code;
    
    unsigned char *tls_buffer;
    unsigned int tls_buffer_len;
    
    unsigned char *application_buffer;
    unsigned int application_buffer_len;
    unsigned char is_child;
    unsigned char exportable;
    unsigned char *exportable_keys;
    unsigned char exportable_size;
    char *sni;
    unsigned char request_client_certificate;
    unsigned char dtls;
    unsigned short dtls_epoch_local;
    unsigned short dtls_epoch_remote;
    unsigned char *dtls_cookie;
    unsigned char dtls_cookie_len;
    unsigned char dtls_seq;
    unsigned char *cached_handshake;
    unsigned int cached_handshake_len;
    unsigned char client_verified;
    // handshake messages flags
    unsigned char hs_messages[11];
    void *user_data;
    struct TLSCertificate **root_certificates;
    unsigned int root_count;
#ifdef TLS_ACCEPT_SECURE_RENEGOTIATION
    unsigned char *verify_data;
    unsigned char verify_len;
#endif
    char **alpn;
    unsigned char alpn_count;
    char *negotiated_alpn;
    unsigned int sleep_until;
};

struct TLSPacket {
    unsigned char *buf;
    unsigned int len;
    unsigned int size;
    unsigned char broken;
    struct TLSContext *context;
};

#ifdef SSL_COMPATIBLE_INTERFACE

typedef int (*SOCKET_RECV_CALLBACK)(int socket, void *buffer, size_t length, int flags);
typedef int (*SOCKET_SEND_CALLBACK)(int socket, const void *buffer, size_t length, int flags);

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
#endif

static unsigned int version_id[] = {1, 1, 1, 0};
static unsigned int pk_id[] = {1, 1, 7, 0};
static unsigned int serial_id[] = {1, 1, 2, 1, 0};
static unsigned int issurer_id[] = {1, 1, 4, 0};
static unsigned int owner_id[] = {1, 1, 6, 0};
static unsigned int validity_id[] = {1, 1, 5, 0};
static unsigned int algorithm_id[] = {1, 1, 3, 0};
static unsigned int sign_id[] = {1, 3, 2, 1, 0};
static unsigned int priv_id[] = {1, 4, 0};
static unsigned int priv_der_id[] = {1, 3, 1, 0};
static unsigned int ecc_priv_id[] = {1, 2, 0};

static unsigned char country_oid[] = {0x55, 0x04, 0x06, 0x00};
static unsigned char state_oid[] = {0x55, 0x04, 0x08, 0x00};
static unsigned char location_oid[] = {0x55, 0x04, 0x07, 0x00};
static unsigned char entity_oid[] = {0x55, 0x04, 0x0A, 0x00};
static unsigned char subject_oid[] = {0x55, 0x04, 0x03, 0x00};
static unsigned char san_oid[] = {0x55, 0x1D, 0x11, 0x00};
static unsigned char ocsp_oid[] = {0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x00};

static unsigned char TLS_RSA_SIGN_RSA_OID[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01, 0x00};
static unsigned char TLS_RSA_SIGN_MD5_OID[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x04, 0x00};
static unsigned char TLS_RSA_SIGN_SHA1_OID[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x05, 0x00};
static unsigned char TLS_RSA_SIGN_SHA256_OID[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B, 0x00};
static unsigned char TLS_RSA_SIGN_SHA384_OID[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0C, 0x00};
static unsigned char TLS_RSA_SIGN_SHA512_OID[] = {0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0D, 0x00};

// static unsigned char TLS_ECDSA_SIGN_SHA1_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x01, 0x05, 0x00, 0x00};
// static unsigned char TLS_ECDSA_SIGN_SHA224_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x01, 0x05, 0x00, 0x00};
// static unsigned char TLS_ECDSA_SIGN_SHA256_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, 0x05, 0x00, 0x00};
// static unsigned char TLS_ECDSA_SIGN_SHA384_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x03, 0x05, 0x00, 0x00};
// static unsigned char TLS_ECDSA_SIGN_SHA512_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x04, 0x05, 0x00, 0x00};

static unsigned char TLS_EC_PUBLIC_KEY_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, 0x00};

static unsigned char TLS_EC_prime192v1_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x01, 0x00};
static unsigned char TLS_EC_prime192v2_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x02, 0x00};
static unsigned char TLS_EC_prime192v3_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x03, 0x00};
static unsigned char TLS_EC_prime239v1_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x04, 0x00};
static unsigned char TLS_EC_prime239v2_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x05, 0x00};
static unsigned char TLS_EC_prime239v3_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x06, 0x00};
static unsigned char TLS_EC_prime256v1_OID[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x00};

#define TLS_EC_secp256r1_OID    TLS_EC_prime256v1_OID
static unsigned char TLS_EC_secp224r1_OID[] = {0x2B, 0x81, 0x04, 0x00, 0x21, 0x00};
static unsigned char TLS_EC_secp384r1_OID[] = {0x2B, 0x81, 0x04, 0x00, 0x22, 0x00};
static unsigned char TLS_EC_secp521r1_OID[] = {0x2B, 0x81, 0x04, 0x00, 0x23, 0x00};

struct TLSCertificate *asn1_parse(struct TLSContext *context, const unsigned char *buffer, int size, int client_cert);
int __private_tls_update_hash(struct TLSContext *context, const unsigned char *in, unsigned int len);
struct TLSPacket *tls_build_finished(struct TLSContext *context);
unsigned int __private_tls_hmac_message(unsigned char local, struct TLSContext *context, const unsigned char *buf, int buf_len, const unsigned char *buf2, int buf_len2, unsigned char *out, unsigned int outlen, uint64_t remote_sequence_number);
int tls_random(unsigned char *key, int len);
void tls_destroy_packet(struct TLSPacket *packet);
struct TLSPacket *tls_build_hello(struct TLSContext *context);
struct TLSPacket *tls_build_certificate(struct TLSContext *context);
struct TLSPacket *tls_build_done(struct TLSContext *context);
struct TLSPacket *tls_build_alert(struct TLSContext *context, char critical, unsigned char code);
struct TLSPacket *tls_build_change_cipher_spec(struct TLSContext *context);
struct TLSPacket *tls_build_verify_request(struct TLSContext *context);
int __private_tls_crypto_create(struct TLSContext *context, int key_length, int iv_length, unsigned char *localkey, unsigned char *localiv, unsigned char *remotekey, unsigned char *remoteiv);
int __private_tls_get_hash(struct TLSContext *context, unsigned char *hout);
int __private_tls_build_random(struct TLSPacket *packet);
unsigned int __private_tls_mac_length(struct TLSContext *context);
void __private_dtls_handshake_data(struct TLSContext *context, struct TLSPacket *packet, unsigned int dataframe);
#ifdef TLS_FORWARD_SECRECY
void __private_tls_dhe_free(struct TLSContext *context);
void __private_tls_ecc_dhe_free(struct TLSContext *context);
void __private_tls_dh_clear_key(DHKey *key);
#endif

static unsigned char dependecies_loaded = 0;
// not supported
// static unsigned char TLS_DSA_SIGN_SHA1_OID[] = {0x2A, 0x86, 0x52, 0xCE, 0x38, 0x04, 0x03, 0x00};

// base64 stuff
static const char cd64[] = "|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";

void __private_b64_decodeblock(unsigned char in[4], unsigned char out[3]) {
    out[0] = (unsigned char )(in[0] << 2 | in[1] >> 4);
    out[1] = (unsigned char )(in[1] << 4 | in[2] >> 2);
    out[2] = (unsigned char )(((in[2] << 6) & 0xc0) | in[3]);
}

int __private_b64_decode(const char *in_buffer, int in_buffer_size, unsigned char *out_buffer) {
    unsigned char in[4], out[3], v;
    int           i, len;
    
    const char *ptr     = in_buffer;
    char *out_ptr = (char *)out_buffer;
    
    while (ptr <= in_buffer + in_buffer_size) {
        for (len = 0, i = 0; i < 4 && (ptr <= in_buffer + in_buffer_size); i++) {
            v = 0;
            while ((ptr <= in_buffer + in_buffer_size) && v == 0) {
                v = (unsigned char)ptr[0];
                ptr++;
                v = (unsigned char)((v < 43 || v > 122) ? 0 : cd64[v - 43]);
                if (v)
                    v = (unsigned char)((v == '$') ? 0 : v - 61);
            }
            if (ptr <= in_buffer + in_buffer_size) {
                len++;
                if (v)
                    in[i] = (unsigned char)(v - 1);
            } else {
                in[i] = 0;
            }
        }
        if (len) {
            __private_b64_decodeblock(in, out);
            for (i = 0; i < len - 1; i++) {
                out_ptr[0] = out[i];
                out_ptr++;
            }
        }
    }
    return (int)((intptr_t)out_ptr - (intptr_t)out_buffer);
}

void init_dependencies() {
    if (dependecies_loaded)
        return;
    DEBUG_PRINT("Initializing dependencies\n");
    dependecies_loaded = 1;
#ifdef LTM_DESC
    ltc_mp = ltm_desc;
#else
#ifdef TFM_DESC
    ltc_mp = tfm_desc;
#else
#ifdef GMP_DESC
    ltc_mp = gmp_desc;
#endif
#endif
#endif
    register_prng(&sprng_desc);
    register_hash(&sha256_desc);
    register_hash(&sha1_desc);
    register_hash(&sha384_desc);
    register_hash(&sha512_desc);
    register_hash(&md5_desc);
    register_cipher(&aes_desc);
#ifdef TLS_FORWARD_SECRECY
#ifdef TLS_ECDSA_SUPPORTED
    init_curves();
#endif
#endif
}

#ifdef TLS_FORWARD_SECRECY
int __private_tls_dh_shared_secret(DHKey *private_key, DHKey *public_key, unsigned char *out, unsigned long *outlen) {
    void *tmp;
    unsigned long x;
    int err;
    
    if ((!private_key) || (!public_key) || (!out) || (!outlen))
        return TLS_GENERIC_ERROR;
    
    /* compute y^x mod p */
    if ((err = mp_init(&tmp)) != CRYPT_OK)
        return err;
    
    if ((err = mp_exptmod(public_key->y, private_key->x, private_key->p, tmp)) != CRYPT_OK) {
        mp_clear(tmp);
        return err;
    }
    
    x = (unsigned long)mp_unsigned_bin_size(tmp);
    if (*outlen < x) {
        err = CRYPT_BUFFER_OVERFLOW;
        mp_clear(tmp);
        return err;
    }
    
    if ((err = mp_to_unsigned_bin(tmp, out)) != CRYPT_OK) {
        mp_clear(tmp);
        return err;
    }
    *outlen = x;
    mp_clear(tmp);
    return 0;
}

unsigned char *__private_tls_decrypt_dhe(struct TLSContext *context, const unsigned char *buffer, unsigned int len, unsigned int *size, int clear_key) {
    *size = 0;
    if ((!len) || (!context) || (!context->dhe)) {
        DEBUG_PRINT("No private DHE key set");
        return NULL;
    }
    
    unsigned char *out = (unsigned char *)TLS_MALLOC(len);
    unsigned long out_size = len;
    void *Yc = NULL;
    
    if (mp_init(&Yc)) {
        DEBUG_PRINT("ERROR CREATING Yc\n");
        return NULL;
    }
    if (mp_read_unsigned_bin(Yc, (unsigned char *)buffer, len)) {
        DEBUG_PRINT("ERROR LOADING DHE Yc\n");
        mp_clear(Yc);
        return NULL;
    }
    DHKey client_key;
    memset(&client_key, 0, sizeof(DHKey));
    
    client_key.p = context->dhe->p;
    client_key.g = context->dhe->g;
    client_key.y = Yc;
    int err = __private_tls_dh_shared_secret(context->dhe, &client_key, out, &out_size);
    // don't delete p and g
    client_key.p = NULL;
    client_key.g = NULL;
    __private_tls_dh_clear_key(&client_key);
    // not needing the dhe key anymore
    if (clear_key)
        __private_tls_dhe_free(context);
    if (err) {
        DEBUG_PRINT("DHE DECRYPT ERROR %i\n", err);
        TLS_FREE(out);
        return NULL;
    }
    DEBUG_PRINT("OUT_SIZE: %i\n", out_size);
    DEBUG_DUMP_HEX_LABEL("DHE", out, out_size);
    *size = (unsigned int)out_size;
    return out;
}

unsigned char *__private_tls_decrypt_ecc_dhe(struct TLSContext *context, const unsigned char *buffer, unsigned int len, unsigned int *size, int clear_key) {
    *size = 0;
    if ((!len) || (!context) || (!context->ecc_dhe)) {
        DEBUG_PRINT("No private ECC DHE key set");
        return NULL;
    }
    
    const struct ECCCurveParameters *curve;
    if (context->curve)
        curve = context->curve;
    else
        curve = default_curve;
    
    ltc_ecc_set_type *dp = (ltc_ecc_set_type *)&curve->dp;
    
    ecc_key client_key;
    memset(&client_key, 0, sizeof(client_key));
    if (ecc_ansi_x963_import_ex(buffer, len, &client_key, dp)) {
        DEBUG_PRINT("Error importing ECC DHE key\n");
        return NULL;
    }
    unsigned char *out = (unsigned char *)TLS_MALLOC(len);
    unsigned long out_size = len;
    
    int err = ecc_shared_secret(context->ecc_dhe, &client_key, out, &out_size);
    ecc_free(&client_key);
    if (clear_key)
        __private_tls_ecc_dhe_free(context);
    if (err) {
        DEBUG_PRINT("ECC DHE DECRYPT ERROR %i\n", err);
        TLS_FREE(out);
        return NULL;
    }
    DEBUG_PRINT("OUT_SIZE: %i\n", out_size);
    DEBUG_DUMP_HEX_LABEL("ECC DHE", out, out_size);
    *size = (unsigned int)out_size;
    return out;
}
#endif

unsigned char *__private_tls_decrypt_rsa(struct TLSContext *context, const unsigned char *buffer, unsigned int len, unsigned int *size) {
    *size = 0;
    if ((!len) || (!context) || (!context->private_key) || (!context->private_key->der_bytes) || (!context->private_key->der_len)) {
        DEBUG_PRINT("No private key set");
        return NULL;
    }
    init_dependencies();
    rsa_key key;
    int err;
    err = rsa_import(context->private_key->der_bytes, context->private_key->der_len, &key);
    
    if (err) {
        DEBUG_PRINT("Error importing RSA key (code: %i)", err);
        return NULL;
    }
    unsigned char *out = (unsigned char *)TLS_MALLOC(len);
    unsigned long out_size = len;
    int hash_idx = find_hash("sha256");
    int res = 0;
    err = rsa_decrypt_key_ex(buffer, len, out, &out_size, (unsigned char *)"Concept", 7, hash_idx, LTC_LTC_PKCS_1_V1_5, &res, &key);
    rsa_free(&key);
    if ((err) || (!out_size)) {
        DEBUG_PRINT("RSA DECRYPT ERROR\n");
        TLS_FREE(out);
        return NULL;
    }
    *size = (unsigned int)out_size;
    return out;
}

unsigned char *__private_tls_encrypt_rsa(struct TLSContext *context, const unsigned char *buffer, unsigned int len, unsigned int *size) {
    *size = 0;
    if ((!len) || (!context) || (!context->certificates) || (!context->certificates_count) || (!context->certificates[0]) ||
        (!context->certificates[0]->der_bytes) || (!context->certificates[0]->der_len)) {
        DEBUG_PRINT("No certificate set\n");
        return NULL;
    }
    init_dependencies();
    rsa_key key;
    int err;
    err = rsa_import(context->certificates[0]->der_bytes, context->certificates[0]->der_len, &key);
    
    if (err) {
        DEBUG_PRINT("Error importing RSA certificate (code: %i)", err);
        return NULL;
    }
    unsigned long out_size = __TLS_MAX_RSA_KEY;
    unsigned char *out = (unsigned char *)TLS_MALLOC(out_size);
    int hash_idx = find_hash("sha256");
    int prng_idx = find_prng("sprng");
    err = rsa_encrypt_key_ex(buffer, len, out, &out_size, (unsigned char *)"Concept", 7, NULL, prng_idx, hash_idx, LTC_LTC_PKCS_1_V1_5, &key);
    rsa_free(&key);
    if ((err) || (!out_size)) {
        TLS_FREE(out);
        return NULL;
    }
    *size = (unsigned int)out_size;
    return out;
}

#ifdef TLS_LEGACY_SUPPORT
int __private_rsa_verify_hash_md5sha1(const unsigned char *sig, unsigned long siglen, unsigned char *hash, unsigned long hashlen, int *stat, rsa_key *key) {
    unsigned long modulus_bitlen, modulus_bytelen, x;
    int           err;
    unsigned char *tmpbuf = NULL;
    
    if ((hash == NULL) || (sig == NULL) || (stat == NULL) || (key == NULL) || (!siglen) || (!hashlen))
        return TLS_GENERIC_ERROR;
    
    *stat = 0;
    
    modulus_bitlen = mp_count_bits((key->N));
    
    modulus_bytelen = mp_unsigned_bin_size((key->N));
    if (modulus_bytelen != siglen)
        return TLS_GENERIC_ERROR;
    
    tmpbuf = (unsigned char *)TLS_MALLOC(siglen);
    if (!tmpbuf)
        return TLS_GENERIC_ERROR;
    
    x = siglen;
    if ((err = ltc_mp.rsa_me(sig, siglen, tmpbuf, &x, PK_PUBLIC, key)) != CRYPT_OK) {
        TLS_FREE(tmpbuf);
        return err;
    }
    
    if (x != siglen) {
        TLS_FREE(tmpbuf);
        return CRYPT_INVALID_PACKET;
    }
    unsigned long out_len = siglen;
    unsigned char *out = (unsigned char *)TLS_MALLOC(siglen);
    if (!out) {
        TLS_FREE(tmpbuf);
        return TLS_GENERIC_ERROR;
    }
    
    int decoded = 0;
    err = pkcs_1_v1_5_decode(tmpbuf, x, LTC_LTC_PKCS_1_EMSA, modulus_bitlen, out, &out_len, &decoded);
    if (decoded) {
        if (out_len == hashlen) {
            if (!memcmp(out, hash, hashlen))
                *stat = 1;
        }
    }
    
    TLS_FREE(tmpbuf);
    TLS_FREE(out);
    return err;
}
#endif

int __private_tls_verify_rsa(struct TLSContext *context, unsigned int hash_type, const unsigned char *buffer, unsigned int len, const unsigned char *message, unsigned int message_len) {
    init_dependencies();
    rsa_key key;
    int err;
    
    if (context->is_server) {
        if ((!len) || (!context) || (!context->client_certificates) || (!context->client_certificates_count) || (!context->client_certificates[0]) ||
            (!context->client_certificates[0]->der_bytes) || (!context->client_certificates[0]->der_len)) {
            DEBUG_PRINT("No client certificate set\n");
            return TLS_GENERIC_ERROR;
        }
        err = rsa_import(context->client_certificates[0]->der_bytes, context->client_certificates[0]->der_len, &key);
    } else {
        if ((!len) || (!context) || (!context->certificates) || (!context->certificates_count) || (!context->certificates[0]) ||
            (!context->certificates[0]->der_bytes) || (!context->certificates[0]->der_len)) {
            DEBUG_PRINT("No server certificate set\n");
            return TLS_GENERIC_ERROR;
        }
        err = rsa_import(context->certificates[0]->der_bytes, context->certificates[0]->der_len, &key);
    }
    if (err) {
        DEBUG_PRINT("Error importing RSA certificate (code: %i)", err);
        return TLS_GENERIC_ERROR;
    }
    int hash_idx = -1;
    unsigned char hash[__TLS_MAX_HASH_LEN];
    unsigned int hash_len = 0;
    hash_state state;
    switch (hash_type) {
        case md5:
            hash_idx = find_hash("md5");
            err = md5_init(&state);
            if (!err) {
                err = md5_process(&state, message, message_len);
                if (!err)
                    err = md5_done(&state, hash);
            }
            hash_len = 16;
            break;
        case sha1:
            hash_idx = find_hash("sha1");
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash);
            }
            hash_len = 20;
            break;
        case sha256:
            hash_idx = find_hash("sha256");
            err = sha256_init(&state);
            if (!err) {
                err = sha256_process(&state, message, message_len);
                if (!err)
                    err = sha256_done(&state, hash);
            }
            hash_len = 32;
            break;
        case sha384:
            hash_idx = find_hash("sha384");
            err = sha384_init(&state);
            if (!err) {
                err = sha384_process(&state, message, message_len);
                if (!err)
                    err = sha384_done(&state, hash);
            }
            hash_len = 48;
            break;
        case sha512:
            hash_idx = find_hash("sha512");
            err = sha512_init(&state);
            if (!err) {
                err = sha512_process(&state, message, message_len);
                if (!err)
                    err = sha512_done(&state, hash);
            }
            hash_len = 64;
            break;
#ifdef TLS_LEGACY_SUPPORT
        case __md5_sha1:
            hash_idx = find_hash("md5");
            err = md5_init(&state);
            if (!err) {
                err = md5_process(&state, message, message_len);
                if (!err)
                    err = md5_done(&state, hash);
            }
            hash_idx = find_hash("sha1");
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash + 16);
            }
            hash_len = 36;
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash + 16);
            }
            hash_len = 36;
            break;
#endif
    }
    if ((hash_idx < 0) || (err)) {
        DEBUG_PRINT("Unsupported hash type: %i\n", hash_type);
        return TLS_GENERIC_ERROR;
    }
    int rsa_stat = 0;
#ifdef TLS_LEGACY_SUPPORT
    if (hash_type == __md5_sha1)
        err = __private_rsa_verify_hash_md5sha1(buffer, len, hash, hash_len, &rsa_stat, &key);
    else
#endif
        err = rsa_verify_hash_ex(buffer, len, hash, hash_len, LTC_LTC_PKCS_1_V1_5, hash_idx, 0, &rsa_stat, &key);
    rsa_free(&key);
    if (err)
        return 0;
    return rsa_stat;
}

#ifdef TLS_LEGACY_SUPPORT
int __private_rsa_sign_hash_md5sha1(const unsigned char *in, unsigned long inlen, unsigned char *out, unsigned long *outlen, prng_state *prng, int prng_idx, unsigned long saltlen, rsa_key *key) {
    unsigned long modulus_bitlen, modulus_bytelen, x;
    int err;
    
    if ((in == NULL) || (out == NULL) || (outlen == NULL) || (key == NULL))
        return TLS_GENERIC_ERROR;
    
    modulus_bitlen = mp_count_bits((key->N));
    
    modulus_bytelen = mp_unsigned_bin_size((key->N));
    if (modulus_bytelen > *outlen) {
        *outlen = modulus_bytelen;
        return CRYPT_BUFFER_OVERFLOW;
    }
    x = modulus_bytelen;
    err = pkcs_1_v1_5_encode(in, inlen, LTC_LTC_PKCS_1_EMSA, modulus_bitlen, NULL, 0, out, &x);
    if (err != CRYPT_OK)
        return err;
    
    return ltc_mp.rsa_me(out, x, out, outlen, PK_PRIVATE, key);
}
#endif

int __private_tls_sign_rsa(struct TLSContext *context, unsigned int hash_type, const unsigned char *message, unsigned int message_len, unsigned char *out, unsigned long *outlen) {
    if ((!outlen) || (!context) || (!out) || (!outlen) || (!context->private_key) || (!context->private_key->der_bytes) || (!context->private_key->der_len)) {
        DEBUG_PRINT("No private key set");
        return TLS_GENERIC_ERROR;
    }
    init_dependencies();
    rsa_key key;
    int err;
    err = rsa_import(context->private_key->der_bytes, context->private_key->der_len, &key);
    
    if (err) {
        DEBUG_PRINT("Error importing RSA certificate (code: %i)", err);
        return TLS_GENERIC_ERROR;
    }
    int hash_idx = -1;
    unsigned char hash[__TLS_MAX_HASH_LEN];
    unsigned int hash_len = 0;
    hash_state state;
    switch (hash_type) {
        case md5:
            hash_idx = find_hash("md5");
            err = md5_init(&state);
            if (!err) {
                err = md5_process(&state, message, message_len);
                if (!err)
                    err = md5_done(&state, hash);
            }
            hash_len = 16;
            break;
        case sha1:
            hash_idx = find_hash("sha1");
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash);
            }
            hash_len = 20;
            break;
        case sha256:
            hash_idx = find_hash("sha256");
            err = sha256_init(&state);
            if (!err) {
                err = sha256_process(&state, message, message_len);
                if (!err)
                    err = sha256_done(&state, hash);
            }
            hash_len = 32;
            break;
        case sha384:
            hash_idx = find_hash("sha384");
            err = sha384_init(&state);
            if (!err) {
                err = sha384_process(&state, message, message_len);
                if (!err)
                    err = sha384_done(&state, hash);
            }
            hash_len = 48;
            break;
        case sha512:
            hash_idx = find_hash("sha512");
            err = sha512_init(&state);
            if (!err) {
                err = sha512_process(&state, message, message_len);
                if (!err)
                    err = sha512_done(&state, hash);
            }
            hash_len = 64;
            break;
        case __md5_sha1:
            hash_idx = find_hash("md5");
            err = md5_init(&state);
            if (!err) {
                err = md5_process(&state, message, message_len);
                if (!err)
                    err = md5_done(&state, hash);
            }
            hash_idx = find_hash("sha1");
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash + 16);
            }
            hash_len = 36;
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash + 16);
            }
            hash_len = 36;
            break;
    }
#ifdef TLS_LEGACY_SUPPORT
    if (hash_type == __md5_sha1) {
        if (err) {
            DEBUG_PRINT("Unsupported hash type: %i\n", hash_type);
            return TLS_GENERIC_ERROR;
        }
        err = __private_rsa_sign_hash_md5sha1(hash, hash_len, out, outlen, NULL, find_prng("sprng"), 0, &key);
    } else
#endif
    {
        if ((hash_idx < 0) || (err)) {
            DEBUG_PRINT("Unsupported hash type: %i\n", hash_type);
            return TLS_GENERIC_ERROR;
        }
        err = rsa_sign_hash_ex(hash, hash_len, out, outlen, LTC_LTC_PKCS_1_V1_5, NULL, find_prng("sprng"), hash_idx, 0, &key);
    }
    rsa_free(&key);
    if (err)
        return 0;
    
    return 1;
}

#ifdef TLS_ECDSA_SUPPORTED
static int __private_tls_is_point(ecc_key *key) {
    void *prime, *b, *t1, *t2;
    int  err;
    
    if ((err = mp_init_multi(&prime, &b, &t1, &t2, NULL)) != CRYPT_OK) {
        return err;
    }
    
    /* load prime and b */
    if ((err = mp_read_radix(prime, key->dp->prime, 16)) != CRYPT_OK) {
        goto error;
    }
    if ((err = mp_read_radix(b, key->dp->B, 16)) != CRYPT_OK) {
        goto error;
    }
    
    /* compute y^2 */
    if ((err = mp_sqr(key->pubkey.y, t1)) != CRYPT_OK) {
        goto error;
    }
    
    /* compute x^3 */
    if ((err = mp_sqr(key->pubkey.x, t2)) != CRYPT_OK) {
        goto error;
    }
    if ((err = mp_mod(t2, prime, t2)) != CRYPT_OK) {
        goto error;
    }
    if ((err = mp_mul(key->pubkey.x, t2, t2)) != CRYPT_OK) {
        goto error;
    }
    
    /* compute y^2 - x^3 */
    if ((err = mp_sub(t1, t2, t1)) != CRYPT_OK) {
        goto error;
    }
    
    /* compute y^2 - x^3 + 3x */
    if ((err = mp_add(t1, key->pubkey.x, t1)) != CRYPT_OK) {
        goto error;
    }
    if ((err = mp_add(t1, key->pubkey.x, t1)) != CRYPT_OK) {
        goto error;
    }
    if ((err = mp_add(t1, key->pubkey.x, t1)) != CRYPT_OK) {
        goto error;
    }
    if ((err = mp_mod(t1, prime, t1)) != CRYPT_OK) {
        goto error;
    }
    while (mp_cmp_d(t1, 0) == LTC_MP_LT) {
        if ((err = mp_add(t1, prime, t1)) != CRYPT_OK) {
            goto error;
        }
    }
    while (mp_cmp(t1, prime) != LTC_MP_LT) {
        if ((err = mp_sub(t1, prime, t1)) != CRYPT_OK) {
            goto error;
        }
    }
    
    /* compare to b */
    if (mp_cmp(t1, b) != LTC_MP_EQ) {
        err = CRYPT_INVALID_PACKET;
    } else {
        err = CRYPT_OK;
    }
    
error:
    mp_clear_multi(prime, b, t1, t2, NULL);
    return err;
}

int __private_tls_ecc_import_key(const unsigned char *private_key, int private_len, const unsigned char *public_key, int public_len, ecc_key *key, const ltc_ecc_set_type *dp) {
    int           err;
    
    if ((!key) || (!ltc_mp.name))
        return CRYPT_MEM;
        
    key->type = PK_PRIVATE;
    
    if (mp_init_multi(&key->pubkey.x, &key->pubkey.y, &key->pubkey.z, &key->k, NULL) != CRYPT_OK)
        return CRYPT_MEM;
    
    if ((public_len) && (!public_key[0])) {
        public_key++;
        public_len--;
    }
    if ((err = mp_read_unsigned_bin(key->pubkey.x, (unsigned char *)public_key + 1, (public_len - 1) >> 1)) != CRYPT_OK) {
        mp_clear_multi(key->pubkey.x, key->pubkey.y, key->pubkey.z, key->k, NULL);
        return err;
    }
    
    if ((err = mp_read_unsigned_bin(key->pubkey.y, (unsigned char *)public_key + 1 + ((public_len - 1) >> 1), (public_len - 1) >> 1)) != CRYPT_OK) {
        mp_clear_multi(key->pubkey.x, key->pubkey.y, key->pubkey.z, key->k, NULL);
        return err;
    }
    
    if ((err = mp_read_unsigned_bin(key->k, (unsigned char *)private_key, private_len)) != CRYPT_OK) {
        mp_clear_multi(key->pubkey.x, key->pubkey.y, key->pubkey.z, key->k, NULL);
        return err;
    }
    
    key->idx = -1;
    key->dp  = dp;
    
    /* set z */
    if ((err = mp_set(key->pubkey.z, 1)) != CRYPT_OK) {
        mp_clear_multi(key->pubkey.x, key->pubkey.y, key->pubkey.z, key->k, NULL);
        return err;
    }
    
    /* is it a point on the curve?  */
    if ((err = __private_tls_is_point(key)) != CRYPT_OK) {
        DEBUG_PRINT("KEY IS NOT ON CURVE\n");
        mp_clear_multi(key->pubkey.x, key->pubkey.y, key->pubkey.z, key->k, NULL);
        return err;
    }
    
    /* we're good */
    return CRYPT_OK;
}

int __private_tls_sign_ecdsa(struct TLSContext *context, unsigned int hash_type, const unsigned char *message, unsigned int message_len, unsigned char *out, unsigned long *outlen) {
    if ((!outlen) || (!context) || (!out) || (!outlen) || (!context->ec_private_key) ||
        (!context->ec_private_key->priv) || (!context->ec_private_key->priv_len) || (!context->ec_private_key->pk) || (!context->ec_private_key->pk_len)) {
        DEBUG_PRINT("No private ECDSA key set");
        return TLS_GENERIC_ERROR;
    }
    
    const struct ECCCurveParameters *curve = NULL;
    
    switch (context->ec_private_key->ec_algorithm) {
        case 19:
            curve = &secp192r1;
            break;
        case 20:
            curve = &secp224k1;
            break;
        case 21:
            curve = &secp224r1;
            break;
        case 22:
            curve = &secp256k1;
            break;
        case 23:
            curve = &secp256r1;
            break;
        case 24:
            curve = &secp384r1;
            break;
        case 25:
            curve = &secp521r1;
            break;
        default:
            DEBUG_PRINT("UNSUPPORTED CURVE");
            return TLS_GENERIC_ERROR;
    }
    
    if (!curve)
        return TLS_GENERIC_ERROR;
    
    init_dependencies();
    ecc_key key;
    int err;
    
    ltc_ecc_set_type *dp = (ltc_ecc_set_type *)&curve->dp;
    
    // broken ... fix this
    err = __private_tls_ecc_import_key(context->ec_private_key->priv, context->ec_private_key->priv_len, context->ec_private_key->pk, context->ec_private_key->pk_len, &key, dp);
    if (err) {
        DEBUG_PRINT("Error importing ECC certificate (code: %i)", (int)err);
        return TLS_GENERIC_ERROR;
    }
    int hash_idx = -1;
    unsigned char hash[__TLS_MAX_HASH_LEN];
    unsigned int hash_len = 0;
    hash_state state;
    switch (hash_type) {
        case md5:
            hash_idx = find_hash("md5");
            err = md5_init(&state);
            if (!err) {
                err = md5_process(&state, message, message_len);
                if (!err)
                    err = md5_done(&state, hash);
            }
            hash_len = 16;
            break;
        case sha1:
            hash_idx = find_hash("sha1");
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash);
            }
            hash_len = 20;
            break;
        case sha256:
            hash_idx = find_hash("sha256");
            err = sha256_init(&state);
            if (!err) {
                err = sha256_process(&state, message, message_len);
                if (!err)
                    err = sha256_done(&state, hash);
            }
            hash_len = 32;
            break;
        case sha384:
            hash_idx = find_hash("sha384");
            err = sha384_init(&state);
            if (!err) {
                err = sha384_process(&state, message, message_len);
                if (!err)
                    err = sha384_done(&state, hash);
            }
            hash_len = 48;
            break;
        case sha512:
            hash_idx = find_hash("sha512");
            err = sha512_init(&state);
            if (!err) {
                err = sha512_process(&state, message, message_len);
                if (!err)
                    err = sha512_done(&state, hash);
            }
            hash_len = 64;
            break;
        case __md5_sha1:
            hash_idx = find_hash("md5");
            err = md5_init(&state);
            if (!err) {
                err = md5_process(&state, message, message_len);
                if (!err)
                    err = md5_done(&state, hash);
            }
            hash_idx = find_hash("sha1");
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash + 16);
            }
            hash_len = 36;
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash + 16);
            }
            hash_len = 36;
            break;
    }
    
    if ((hash_idx < 0) || (err)) {
        DEBUG_PRINT("Unsupported hash type: %i\n", hash_type);
        return TLS_GENERIC_ERROR;
    }
    // "Let z be the Ln leftmost bits of e, where Ln is the bit length of the group order n."
    if (hash_len > curve->size)
        hash_len = curve->size;
    err = ecc_sign_hash(hash, hash_len, out, outlen, NULL, find_prng("sprng"), &key);
    DEBUG_DUMP_HEX_LABEL("ECC SIGNATURE", out, *outlen);
    ecc_free(&key);
    if (err)
        return 0;
    
    return 1;
}

#ifdef TLS_CLIENT_ECDSA
int __private_tls_ecc_import_pk(const unsigned char *public_key, int public_len, ecc_key *key, const ltc_ecc_set_type *dp) {
    int           err;
    
    if ((!key) || (!ltc_mp.name))
        return CRYPT_MEM;
        
    key->type = PK_PUBLIC;
    
    if (mp_init_multi(&key->pubkey.x, &key->pubkey.y, &key->pubkey.z, &key->k, NULL) != CRYPT_OK)
        return CRYPT_MEM;
    
    if ((public_len) && (!public_key[0])) {
        public_key++;
        public_len--;
    }
    if ((err = mp_read_unsigned_bin(key->pubkey.x, (unsigned char *)public_key + 1, (public_len - 1) >> 1)) != CRYPT_OK) {
        mp_clear_multi(key->pubkey.x, key->pubkey.y, key->pubkey.z, key->k, NULL);
        return err;
    }
    
    if ((err = mp_read_unsigned_bin(key->pubkey.y, (unsigned char *)public_key + 1 + ((public_len - 1) >> 1), (public_len - 1) >> 1)) != CRYPT_OK) {
        mp_clear_multi(key->pubkey.x, key->pubkey.y, key->pubkey.z, key->k, NULL);
        return err;
    }
    
    
    key->idx = -1;
    key->dp  = dp;
    
    /* set z */
    if ((err = mp_set(key->pubkey.z, 1)) != CRYPT_OK) {
        mp_clear_multi(key->pubkey.x, key->pubkey.y, key->pubkey.z, key->k, NULL);
        return err;
    }
    
    /* is it a point on the curve?  */
    if ((err = __private_tls_is_point(key)) != CRYPT_OK) {
        DEBUG_PRINT("KEY IS NOT ON CURVE\n");
        mp_clear_multi(key->pubkey.x, key->pubkey.y, key->pubkey.z, key->k, NULL);
        return err;
    }
    
    /* we're good */
    return CRYPT_OK;
}

int __private_tls_verify_ecdsa(struct TLSContext *context, unsigned int hash_type, const unsigned char *buffer, unsigned int len, const unsigned char *message, unsigned int message_len) {
    init_dependencies();
    ecc_key key;
    int err;
    
    if (context->is_server) {
        if ((!len) || (!context) || (!context->client_certificates) || (!context->client_certificates_count) || (!context->client_certificates[0]) ||
            (!context->client_certificates[0]->pk) || (!context->client_certificates[0]->pk_len) || (!context->curve)) {
            DEBUG_PRINT("No client certificate set\n");
            return TLS_GENERIC_ERROR;
        }
        err = __private_tls_ecc_import_pk(context->client_certificates[0]->pk, context->client_certificates[0]->pk_len, &key, (ltc_ecc_set_type *)&context->curve->dp);
    } else {
        if ((!len) || (!context) || (!context->certificates) || (!context->certificates_count) || (!context->certificates[0]) ||
            (!context->certificates[0]->pk) || (!context->certificates[0]->pk_len) || (!context->curve)) {
            DEBUG_PRINT("No server certificate set\n");
            return TLS_GENERIC_ERROR;
        }
        err = __private_tls_ecc_import_pk(context->certificates[0]->pk, context->certificates[0]->pk_len, &key, (ltc_ecc_set_type *)&context->curve->dp);
    }
    if (err) {
        DEBUG_PRINT("Error importing ECC certificate (code: %i)", err);
        return TLS_GENERIC_ERROR;
    }
    int hash_idx = -1;
    unsigned char hash[__TLS_MAX_HASH_LEN];
    unsigned int hash_len = 0;
    hash_state state;
    switch (hash_type) {
        case md5:
            hash_idx = find_hash("md5");
            err = md5_init(&state);
            if (!err) {
                err = md5_process(&state, message, message_len);
                if (!err)
                    err = md5_done(&state, hash);
            }
            hash_len = 16;
            break;
        case sha1:
            hash_idx = find_hash("sha1");
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash);
            }
            hash_len = 20;
            break;
        case sha256:
            hash_idx = find_hash("sha256");
            err = sha256_init(&state);
            if (!err) {
                err = sha256_process(&state, message, message_len);
                if (!err)
                    err = sha256_done(&state, hash);
            }
            hash_len = 32;
            break;
        case sha384:
            hash_idx = find_hash("sha384");
            err = sha384_init(&state);
            if (!err) {
                err = sha384_process(&state, message, message_len);
                if (!err)
                    err = sha384_done(&state, hash);
            }
            hash_len = 48;
            break;
        case sha512:
            hash_idx = find_hash("sha512");
            err = sha512_init(&state);
            if (!err) {
                err = sha512_process(&state, message, message_len);
                if (!err)
                    err = sha512_done(&state, hash);
            }
            hash_len = 64;
            break;
#ifdef TLS_LEGACY_SUPPORT
        case __md5_sha1:
            hash_idx = find_hash("md5");
            err = md5_init(&state);
            if (!err) {
                err = md5_process(&state, message, message_len);
                if (!err)
                    err = md5_done(&state, hash);
            }
            hash_idx = find_hash("sha1");
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash + 16);
            }
            hash_len = 36;
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash + 16);
            }
            hash_len = 36;
            break;
#endif
    }
    if ((hash_idx < 0) || (err)) {
        DEBUG_PRINT("Unsupported hash type: %i\n", hash_type);
        return TLS_GENERIC_ERROR;
    }
    int ecc_stat = 0;
    err = ecc_verify_hash(buffer, len, hash, hash_len, &ecc_stat, &key);
    ecc_free(&key);
    if (err)
        return 0;
    return ecc_stat;
}
#endif

#endif

unsigned int __private_tls_random_int(int limit) {
    unsigned int res = 0;
    tls_random((unsigned char *)&res, sizeof(int));
    if (limit)
        res %= limit;
    return res;
}

void __private_tls_sleep(unsigned int microseconds) {
#ifdef _WIN32
    Sleep(microseconds/1000);
#else
    struct timespec ts;
    
    ts.tv_sec = (unsigned int) (microseconds / 1000000);
    ts.tv_nsec = (unsigned int) (microseconds % 1000000) * 1000ul;
    
    nanosleep(&ts, NULL);
#endif
}

void __private_random_sleep(struct TLSContext *context, int max_microseconds) {
    if (context)
        context->sleep_until = time(NULL) + __private_tls_random_int(max_microseconds/1000000 * __TLS_MAX_ERROR_IDLE_S);
    else
        __private_tls_sleep(__private_tls_random_int(max_microseconds));
}

void __private_tls_prf_helper(  int hash_idx, unsigned char *output, unsigned int outlen, const unsigned char *secret, const unsigned int secret_len,
                              const unsigned char *label, unsigned int label_len, unsigned char *seed, unsigned int seed_len,
                              unsigned char *seed_b, unsigned int seed_b_len) {
    unsigned char digest_out0[__TLS_MAX_HASH_LEN];
    unsigned char digest_out1[__TLS_MAX_HASH_LEN];
    unsigned long dlen = 32;
    unsigned int i;
    hmac_state hmac;
    
    hmac_init(&hmac, hash_idx, secret, secret_len);
    hmac_process(&hmac, label, label_len);
    
    hmac_process(&hmac, seed, seed_len);
    if ((seed_b) && (seed_b_len))
        hmac_process(&hmac, seed_b, seed_b_len);
    hmac_done(&hmac, digest_out0, &dlen);
    int idx = 0;
    while (outlen) {
        hmac_init(&hmac, hash_idx, secret, secret_len);
        hmac_process(&hmac, digest_out0, dlen);
        hmac_process(&hmac, label, label_len);
        hmac_process(&hmac, seed, seed_len);
        if ((seed_b) && (seed_b_len))
            hmac_process(&hmac, seed_b, seed_b_len);
        hmac_done(&hmac, digest_out1, &dlen);
        
        unsigned int copylen = outlen;
        if (copylen > dlen)
            copylen = dlen;
        
        for (i = 0; i < copylen; i++) {
            output[idx++] ^= digest_out1[i];
            outlen--;
        }
        
        if (!outlen)
            break;
        
        hmac_init(&hmac, hash_idx, secret, secret_len);
        hmac_process(&hmac, digest_out0, dlen);
        hmac_done(&hmac, digest_out0, &dlen);
    }
}

void __private_tls_prf(struct TLSContext *context,
                       unsigned char *output, unsigned int outlen, const unsigned char *secret, const unsigned int secret_len,
                       const unsigned char *label, unsigned int label_len, unsigned char *seed, unsigned int seed_len,
                       unsigned char *seed_b, unsigned int seed_b_len) {
    if ((!secret) || (!secret_len)) {
        DEBUG_PRINT("NULL SECRET\n");
        return;
    }
    if ((context->version != TLS_V12) && (context->version != DTLS_V12)) {
        int md5_hash_idx = find_hash("md5");
        int sha1_hash_idx = find_hash("sha1");
        int half_secret = (secret_len + 1) / 2;
        
        memset(output, 0, outlen);
        __private_tls_prf_helper(md5_hash_idx,  output, outlen, secret, half_secret, label, label_len, seed, seed_len, seed_b, seed_b_len);
        __private_tls_prf_helper(sha1_hash_idx,  output, outlen, secret + (secret_len - half_secret), secret_len - half_secret, label, label_len, seed, seed_len, seed_b, seed_b_len);
    } else {
        // sha256_hmac
        unsigned char digest_out0[__TLS_MAX_HASH_LEN];
        unsigned char digest_out1[__TLS_MAX_HASH_LEN];
        unsigned long dlen = 32;
        int hash_idx;
        unsigned int mac_length = __private_tls_mac_length(context);
        if (mac_length == __TLS_SHA384_MAC_SIZE) {
            hash_idx = find_hash("sha384");
            dlen = mac_length;
        } else
            hash_idx = find_hash("sha256");
        unsigned int i;
        hmac_state hmac;
        
        hmac_init(&hmac, hash_idx, secret, secret_len);
        hmac_process(&hmac, label, label_len);
        
        hmac_process(&hmac, seed, seed_len);
        if ((seed_b) && (seed_b_len))
            hmac_process(&hmac, seed_b, seed_b_len);
        hmac_done(&hmac, digest_out0, &dlen);
        int idx = 0;
        while (outlen) {
            hmac_init(&hmac, hash_idx, secret, secret_len);
            hmac_process(&hmac, digest_out0, dlen);
            hmac_process(&hmac, label, label_len);
            hmac_process(&hmac, seed, seed_len);
            if ((seed_b) && (seed_b_len))
                hmac_process(&hmac, seed_b, seed_b_len);
            hmac_done(&hmac, digest_out1, &dlen);
            
            unsigned int copylen = outlen;
            if (copylen > dlen)
                copylen = (unsigned int)dlen;
            
            for (i = 0; i < copylen; i++) {
                output[idx++] = digest_out1[i];
                outlen--;
            }
            
            if (!outlen)
                break;
            
            hmac_init(&hmac, hash_idx, secret, secret_len);
            hmac_process(&hmac, digest_out0, dlen);
            hmac_done(&hmac, digest_out0, &dlen);
        }
    }
}

int __private_tls_key_length(struct TLSContext *context) {
    switch (context->cipher) {
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
            return 16;
        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
        case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
        case TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
            return 32;
    }
    return 0;
}

int __private_tls_is_aead(struct TLSContext *context) {
    switch (context->cipher) {
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
            return 1;
        case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
        case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
        case TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
            return 2;
    }
    return 0;
}



unsigned int __private_tls_mac_length(struct TLSContext *context) {
    switch (context->cipher) {
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
            return __TLS_SHA1_MAC_SIZE;
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
        case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
        case TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
            return __TLS_SHA256_MAC_SIZE;
        case TLS_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
            return __TLS_SHA384_MAC_SIZE;
    }
    return 0;
}

int __private_tls_expand_key(struct TLSContext *context) {
    unsigned char key[__TLS_MAX_KEY_EXPANSION_SIZE];
    
    if ((!context->master_key) || (!context->master_key_len))
        return 0;
    
    int key_length = __private_tls_key_length(context);
    int mac_length = __private_tls_mac_length(context);
    
    if ((!key_length) || (!mac_length)) {
        DEBUG_PRINT("KEY EXPANSION FAILED, KEY LENGTH: %i, MAC LENGTH: %i\n", key_length, mac_length);
        return 0;
    }
    
    if (context->is_server)
        __private_tls_prf(context, key, sizeof(key), context->master_key, context->master_key_len, (unsigned char *)"key expansion", 13, context->local_random, __TLS_SERVER_RANDOM_SIZE, context->remote_random, __TLS_CLIENT_RANDOM_SIZE);
    else
        __private_tls_prf(context, key, sizeof(key), context->master_key, context->master_key_len, (unsigned char *)"key expansion", 13, context->remote_random, __TLS_SERVER_RANDOM_SIZE, context->local_random, __TLS_CLIENT_RANDOM_SIZE);
    
    DEBUG_DUMP_HEX_LABEL("LOCAL RANDOM ", context->local_random, __TLS_SERVER_RANDOM_SIZE);
    DEBUG_DUMP_HEX_LABEL("REMOTE RANDOM", context->remote_random, __TLS_CLIENT_RANDOM_SIZE);
    DEBUG_PRINT("\n=========== EXPANSION ===========\n");
    DEBUG_DUMP_HEX(key, __TLS_MAX_KEY_EXPANSION_SIZE);
    DEBUG_PRINT("\n");
    
    unsigned char *clientkey = NULL;
    unsigned char *serverkey = NULL;
    unsigned char *clientiv = NULL;
    unsigned char *serveriv = NULL;
    int iv_length = __TLS_AES_IV_LENGTH;
    
    int pos = 0;
    int is_aead = __private_tls_is_aead(context);
#ifdef TLS_WITH_CHACHA20_POLY1305
    if (is_aead == 2) {
        iv_length = __TLS_CHACHA20_IV_LENGTH;
    } else
#endif
    if (is_aead)
        iv_length = __TLS_AES_GCM_IV_LENGTH;
    else {
        if (context->is_server) {
            memcpy(context->crypto.ctx_remote_mac.remote_mac, &key[pos], mac_length);
            pos += mac_length;
            memcpy(context->crypto.ctx_local_mac.local_mac, &key[pos], mac_length);
            pos += mac_length;
        } else {
            memcpy(context->crypto.ctx_local_mac.local_mac, &key[pos], mac_length);
            pos += mac_length;
            memcpy(context->crypto.ctx_remote_mac.remote_mac, &key[pos], mac_length);
            pos += mac_length;
        }
    }
    
    clientkey = &key[pos];
    pos += key_length;
    serverkey = &key[pos];
    pos += key_length;
    clientiv = &key[pos];
    pos += iv_length;
    serveriv = &key[pos];
    pos += iv_length;
    
    DEBUG_PRINT("EXPANSION %i/%i\n", (int)pos, (int)__TLS_MAX_KEY_EXPANSION_SIZE);
    DEBUG_DUMP_HEX_LABEL("CLIENT KEY", clientkey, key_length)
    DEBUG_DUMP_HEX_LABEL("CLIENT IV", clientiv, iv_length)
    DEBUG_DUMP_HEX_LABEL("CLIENT MAC KEY", context->is_server ? context->crypto.ctx_remote_mac.remote_mac : context->crypto.ctx_local_mac.local_mac, mac_length)
    DEBUG_DUMP_HEX_LABEL("SERVER KEY", serverkey, key_length)
    DEBUG_DUMP_HEX_LABEL("SERVER IV", serveriv, iv_length)
    DEBUG_DUMP_HEX_LABEL("SERVER MAC KEY", context->is_server ? context->crypto.ctx_local_mac.local_mac : context->crypto.ctx_remote_mac.remote_mac, mac_length)
    
    if (context->is_server) {
#ifdef TLS_WITH_CHACHA20_POLY1305
        if (is_aead == 2) {
            memcpy(context->crypto.ctx_remote_mac.remote_nonce, clientiv, iv_length);
            memcpy(context->crypto.ctx_local_mac.local_nonce, serveriv, iv_length);
        } else
#endif
        if (is_aead) {
            memcpy(context->crypto.ctx_remote_mac.remote_aead_iv, clientiv, iv_length);
            memcpy(context->crypto.ctx_local_mac.local_aead_iv, serveriv, iv_length);
        }
        if (__private_tls_crypto_create(context, key_length, iv_length, serverkey, serveriv, clientkey, clientiv))
            return 0;
    } else {
#ifdef TLS_WITH_CHACHA20_POLY1305
        if (is_aead == 2) {
            memcpy(context->crypto.ctx_local_mac.local_nonce, clientiv, iv_length);
            memcpy(context->crypto.ctx_remote_mac.remote_nonce, serveriv, iv_length);
        } else
#endif
        if (is_aead) {
            memcpy(context->crypto.ctx_local_mac.local_aead_iv, clientiv, iv_length);
            memcpy(context->crypto.ctx_remote_mac.remote_aead_iv, serveriv, iv_length);
        }
        if (__private_tls_crypto_create(context, key_length, iv_length, clientkey, clientiv, serverkey, serveriv))
            return 0;
    }
    
    if (context->exportable) {
        TLS_FREE(context->exportable_keys);
        context->exportable_keys = (unsigned char *)TLS_MALLOC(key_length * 2);
        if (context->exportable_keys) {
            if (context->is_server) {
                memcpy(context->exportable_keys, serverkey, key_length);
                memcpy(context->exportable_keys + key_length, clientkey, key_length);
            } else {
                memcpy(context->exportable_keys, clientkey, key_length);
                memcpy(context->exportable_keys + key_length, serverkey, key_length);
            }
            context->exportable_size = key_length * 2;
        }
    }
    
    // extract client_mac_key(mac_key_length)
    // extract server_mac_key(mac_key_length)
    // extract client_key(enc_key_length)
    // extract server_key(enc_key_length)
    // extract client_iv(fixed_iv_lengh)
    // extract server_iv(fixed_iv_length)
    return 1;
}

int __private_tls_compute_key(struct TLSContext *context, unsigned int key_len) {
    if ((!context) || (!context->premaster_key) || (!context->premaster_key_len) || (key_len < 48)) {
        DEBUG_PRINT("CANNOT COMPUTE MASTER SECRET\n");
        return 0;
    }
    
    unsigned char master_secret_label[] = "master secret";
#ifdef __TLS_CHECK_PREMASTER_KEY
    if (!tls_cipher_is_ephemeral(context)) {
        unsigned short version = ntohs(*(unsigned short *)context->premaster_key);
        // this check is not true for DHE/ECDHE ciphers
        if (context->version > version) {
            DEBUG_PRINT("Mismatch protocol version 0x(%x)\n", version);
            return 0;
        }
    }
#endif
    TLS_FREE(context->master_key);
    context->master_key_len = 0;
    context->master_key = NULL;
    if ((context->version == TLS_V12) || (context->version == TLS_V11) || (context->version == TLS_V10) ||  (context->version == DTLS_V12) || (context->version == DTLS_V10)) {
        context->master_key = (unsigned char *)TLS_MALLOC(key_len);
        if (!context->master_key)
            return 0;
        context->master_key_len = key_len;
        if (context->is_server) {
            __private_tls_prf(context,
                              context->master_key, context->master_key_len,
                              context->premaster_key, context->premaster_key_len,
                              master_secret_label, 13,
                              context->remote_random, __TLS_CLIENT_RANDOM_SIZE,
                              context->local_random, __TLS_SERVER_RANDOM_SIZE
                              );
        } else {
            __private_tls_prf(context,
                              context->master_key, context->master_key_len,
                              context->premaster_key, context->premaster_key_len,
                              master_secret_label, 13,
                              context->local_random, __TLS_CLIENT_RANDOM_SIZE,
                              context->remote_random, __TLS_SERVER_RANDOM_SIZE
                              );
        }
        TLS_FREE(context->premaster_key);
        context->premaster_key = NULL;
        context->premaster_key_len = 0;
        DEBUG_PRINT("\n=========== Master key ===========\n");
        DEBUG_DUMP_HEX(context->master_key, context->master_key_len);
        DEBUG_PRINT("\n");
        __private_tls_expand_key(context);
        return 1;
    }
    return 0;
}

unsigned char *tls_pem_decode(const unsigned char *data_in, unsigned int input_length, int cert_index, unsigned int *output_len) {
    unsigned int i;
    *output_len = 0;
    int alloc_len = input_length / 4 * 3;
    unsigned char *output = (unsigned char *)TLS_MALLOC(alloc_len);
    if (!output)
        return NULL;
    unsigned int start_at = 0;
    unsigned int idx = 0;
    for (i = 0; i < input_length; i++) {
        if ((data_in[i] == '\n') || (data_in[i] == '\r'))
            continue;
        
        if (data_in[i] != '-')  {
            // read entire line
            while ((i < input_length) && (data_in[i] != '\n'))
                i++;
            continue;
        }
        
        if (data_in[i] == '-') {
            unsigned int end_idx = i;
            //read until end of line
            while ((i < input_length) && (data_in[i] != '\n'))
                i++;
            if (start_at) {
                if (cert_index > 0) {
                    cert_index--;
                    start_at = 0;
                } else {
                    idx = __private_b64_decode((const char *)&data_in[start_at], end_idx - start_at, output);
                    break;
                }
            } else
                start_at = i + 1;
        }
    }
    *output_len = idx;
    if (!idx) {
        TLS_FREE(output);
        return NULL;
    }
    return output;
}

int __is_oid(const unsigned char *oid, const unsigned char *compare_to, int compare_to_len) {
    int i = 0;
    while ((oid[i]) && (i < compare_to_len)) {
        if (oid[i] != compare_to[i])
            return 0;
        
        i++;
    }
    return 1;
}

int __is_oid2(const unsigned char *oid, const unsigned char *compare_to, int compare_to_len, int oid_len) {
    int i = 0;
    if (oid_len < compare_to_len)
        compare_to_len = oid_len;
    while (i < compare_to_len) {
        if (oid[i] != compare_to[i])
            return 0;
        
        i++;
    }
    return 1;
}

struct TLSCertificate *tls_create_certificate() {
    struct TLSCertificate *cert = (struct TLSCertificate *)TLS_MALLOC(sizeof(struct TLSCertificate));
    if (cert)
        memset(cert, 0, sizeof(struct TLSCertificate));
    return cert;
}

int tls_certificate_valid_subject_name(const unsigned char *cert_subject, const char *subject) {
    // no subjects ...
    if (((!cert_subject) || (!cert_subject[0])) && ((!subject) || (!subject[0])))
        return 0;
    
    if ((!subject) || (!subject[0]))
        return bad_certificate;
    
    if ((!cert_subject) || (!cert_subject[0]))
        return bad_certificate;
    
    // exact match
    if (!strcmp((const char *)cert_subject, subject))
        return 0;
    
    const char *wildcard = strchr((const char *)cert_subject, '*');
    if (wildcard) {
        // 6.4.3 (1) The client SHOULD NOT attempt to match a presented identifier in
        // which the wildcard character comprises a label other than the left-most label
        if (!wildcard[1]) {
            // subject is [*]
            // or
            // subject is [something*] .. invalid
            return bad_certificate;
        }
        wildcard++;
        const char *match = strstr(subject, wildcard);
        if ((!match) && (wildcard[0] == '.')) {
            // check *.domain.com agains domain.com
            wildcard++;
            if (!strcasecmp(subject, wildcard))
                return 0;
        }
        if (match) {
            unsigned long offset = (unsigned long)match - (unsigned long)subject;
            if (offset) {
                // check for foo.*.domain.com against *.domain.com (invalid)
                if (memchr(subject, '.', offset))
                    return bad_certificate;
            }
            // check if exact match
            if (!strcasecmp(match, wildcard))
                return 0;
        }
    }
    
    return bad_certificate;
}

int tls_certificate_valid_subject(struct TLSCertificate *cert, const char *subject) {
    int i;
    if (!cert)
        return certificate_unknown;
    int err = tls_certificate_valid_subject_name(cert->subject, subject);
    if ((err) && (cert->san)) {
        for (i = 0; i < cert->san_length; i++) {
            err = tls_certificate_valid_subject_name(cert->san[i], subject);
            if (!err)
                return err;
        }
    }
    return err;
}

int tls_certificate_is_valid(struct TLSCertificate *cert) {
    if (!cert)
        return certificate_unknown;
    if (!cert->not_before)
        return certificate_unknown;
    if (!cert->not_after)
        return certificate_unknown;
    //160224182300Z//
    char current_time[14];
    time_t t = time(NULL);
    struct tm *utct = gmtime(&t);
    if (utct) {
        current_time[0] = 0;
        snprintf(current_time, sizeof(current_time), "%04d%02d%02d%02d%02d%02dZ", 1900 + utct->tm_year, utct->tm_mon + 1, utct->tm_mday, utct->tm_hour, utct->tm_min, utct->tm_sec);
        if (strcasecmp((char *)cert->not_before, current_time) > 0) {
            DEBUG_PRINT("Certificate is not yer valid, now: %s (validity: %s - %s)\n", current_time, cert->not_before, cert->not_after);
            return certificate_expired;
        }
        if (strcasecmp((char *)cert->not_after, current_time) < 0) {
            DEBUG_PRINT("Expired certificate, now: %s (validity: %s - %s)\n", current_time, cert->not_before, cert->not_after);
            return certificate_expired;
        }
        DEBUG_PRINT("Valid certificate, now: %s (validity: %s - %s)\n", current_time, cert->not_before, cert->not_after);
    }
    return 0;
}

void tls_certificate_set_copy(unsigned char **member, const unsigned char *val, int len) {
    if (!member)
        return;
    TLS_FREE(*member);
    if (len) {
        *member = (unsigned char *)TLS_MALLOC(len + 1);
        if (*member) {
            memcpy(*member, val, len);
            (*member)[len] = 0;
        }
    } else
        *member = NULL;
}

void tls_certificate_set_copy_date(unsigned char **member, const unsigned char *val, int len) {
    if (!member)
        return;
    TLS_FREE(*member);
    if (len > 4) {
        *member = (unsigned char *)TLS_MALLOC(len + 3);
        if (*member) {
            if (val[0] == '9') {
                (*member)[0]='1';
                (*member)[1]='9';
            } else {
                (*member)[0]='2';
                (*member)[1]='0';
            }
            memcpy(*member + 2, val, len);
            (*member)[len] = 0;
        }
    } else
        *member = NULL;
}

void tls_certificate_set_key(struct TLSCertificate *cert, const unsigned char *val, int len) {
    if ((!val[0]) && (len % 2)) {
        val++;
        len--;
    }
    tls_certificate_set_copy(&cert->pk, val, len);
    if (cert->pk)
        cert->pk_len = len;
}

void tls_certificate_set_priv(struct TLSCertificate *cert, const unsigned char *val, int len) {
    tls_certificate_set_copy(&cert->priv, val, len);
    if (cert->priv)
        cert->priv_len = len;
}

void tls_certificate_set_sign_key(struct TLSCertificate *cert, const unsigned char *val, int len) {
    if ((!val[0]) && (len % 2)) {
        val++;
        len--;
    }
    tls_certificate_set_copy(&cert->sign_key, val, len);
    if (cert->sign_key)
        cert->sign_len = len;
}

char *tls_certificate_to_string(struct TLSCertificate *cert, char *buffer, int len) {
    unsigned int i;
    if (!buffer)
        return NULL;
    buffer[0] = 0;
    if (cert->version) {
        int res = snprintf(buffer, len, "X.509v%i certificate\n  Issued by: [%s]%s (%s)\n  Issued to: [%s]%s (%s, %s)\n  Subject: %s\n  Validity: %s - %s\n  OCSP: %s\n  Serial number: ",
                           (int)cert->version,
                           cert->issuer_country, cert->issuer_entity, cert->issuer_subject,
                           cert->country, cert->entity, cert->state, cert->location,
                           cert->subject,
                           cert->not_before, cert->not_after,
                           cert->ocsp
                           );
        if (res > 0) {
            for (i = 0; i < cert->serial_len; i++)
                res += snprintf(buffer + res, len - res, "%02x", (int)cert->serial_number[i]);
        }
        if ((cert->san) && (cert->san_length)) {
            res += snprintf(buffer + res, len - res, "\n  Alternative subjects: ");
            for (i = 0; i < cert->san_length; i++) {
                if (i)
                    res += snprintf(buffer + res, len - res, ", %s", cert->san[i]);
                else
                    res += snprintf(buffer + res, len - res, "%s", cert->san[i]);
            }
        }
        res += snprintf(buffer + res, len - res, "\n  Key (%i bits, ", cert->pk_len * 8);
        if (res > 0) {
            switch (cert->key_algorithm) {
                case TLS_RSA_SIGN_RSA:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_RSA");
                    break;
                case TLS_RSA_SIGN_MD5:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_MD5");
                    break;
                case TLS_RSA_SIGN_SHA1:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_SHA1");
                    break;
                case TLS_RSA_SIGN_SHA256:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_SHA256");
                    break;
                case TLS_RSA_SIGN_SHA384:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_SHA384");
                    break;
                case TLS_RSA_SIGN_SHA512:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_SHA512");
                    break;
                case TLS_EC_PUBLIC_KEY:
                    res += snprintf(buffer + res, len - res, "EC_PUBLIC_KEY");
                    break;
                default:
                    res += snprintf(buffer + res, len - res, "not supported (%i)", (int)cert->key_algorithm);
            }
        }
        if ((res > 0) && (cert->ec_algorithm)) {
            switch (cert->ec_algorithm) {
                case TLS_EC_prime192v1:
                    res += snprintf(buffer + res, len - res, " prime192v1");
                    break;
                case TLS_EC_prime192v2:
                    res += snprintf(buffer + res, len - res, " prime192v2");
                    break;
                case TLS_EC_prime192v3:
                    res += snprintf(buffer + res, len - res, " prime192v3");
                    break;
                case TLS_EC_prime239v2:
                    res += snprintf(buffer + res, len - res, " prime239v2");
                    break;
                case TLS_EC_secp256r1:
                    res += snprintf(buffer + res, len - res, " EC_secp256r1");
                    break;
                case TLS_EC_secp224r1:
                    res += snprintf(buffer + res, len - res, " EC_secp224r1");
                    break;
                case TLS_EC_secp384r1:
                    res += snprintf(buffer + res, len - res, " EC_secp384r1");
                    break;
                case TLS_EC_secp521r1:
                    res += snprintf(buffer + res, len - res, " EC_secp521r1");
                    break;
                default:
                    res += snprintf(buffer + res, len - res, " unknown(%i)", (int)cert->ec_algorithm);
            }
        }
        res += snprintf(buffer + res, len - res, "):\n");
        if (res > 0) {
            for (i = 0; i < cert->pk_len; i++)
                res += snprintf(buffer + res, len - res, "%02x", (int)cert->pk[i]);
            res += snprintf(buffer + res, len - res, "\n  Signature (%i bits, ", cert->sign_len * 8);
            switch (cert->algorithm) {
                case TLS_RSA_SIGN_RSA:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_RSA):\n");
                    break;
                case TLS_RSA_SIGN_MD5:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_MD5):\n");
                    break;
                case TLS_RSA_SIGN_SHA1:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_SHA1):\n");
                    break;
                case TLS_RSA_SIGN_SHA256:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_SHA256):\n");
                    break;
                case TLS_RSA_SIGN_SHA384:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_SHA384):\n");
                    break;
                case TLS_RSA_SIGN_SHA512:
                    res += snprintf(buffer + res, len - res, "RSA_SIGN_SHA512):\n");
                    break;
                case TLS_EC_PUBLIC_KEY:
                    res += snprintf(buffer + res, len - res, "EC_PUBLIC_KEY):\n");
                    break;
                default:
                    res += snprintf(buffer + res, len - res, "not supported):\n");
            }
            
            for (i = 0; i < cert->sign_len; i++)
                res += snprintf(buffer + res, len - res, "%02x", (int)cert->sign_key[i]);
        }
    } else
    if ((cert->priv) && (cert->priv_len)) {
        int res = snprintf(buffer, len, "X.509 private key\n");
        res += snprintf(buffer + res, len - res, "  Private Key: ");
        if (res > 0) {
            for (i = 0; i < cert->priv_len; i++)
                res += snprintf(buffer + res, len - res, "%02x", (int)cert->priv[i]);
        }
    } else
        snprintf(buffer, len, "Empty ASN1 file");
    return buffer;
}

void tls_certificate_set_exponent(struct TLSCertificate *cert, const unsigned char *val, int len) {
    tls_certificate_set_copy(&cert->exponent, val, len);
    if (cert->exponent)
        cert->exponent_len = len;
}

void tls_certificate_set_serial(struct TLSCertificate *cert, const unsigned char *val, int len) {
    tls_certificate_set_copy(&cert->serial_number, val, len);
    if (cert->serial_number)
        cert->serial_len = len;
}

void tls_certificate_set_algorithm(unsigned int *algorithm, const unsigned char *val, int len) {
    if ((len == 7) && (__is_oid(val, TLS_EC_PUBLIC_KEY_OID, 7))) {
        *algorithm = TLS_EC_PUBLIC_KEY;
        return;
    }
    if (len == 8) {
        if (__is_oid(val, TLS_EC_prime192v1_OID, len)) {
            *algorithm = TLS_EC_prime192v1;
            return;
        }
        if (__is_oid(val, TLS_EC_prime192v2_OID, len)) {
            *algorithm = TLS_EC_prime192v2;
            return;
        }
        if (__is_oid(val, TLS_EC_prime192v3_OID, len)) {
            *algorithm = TLS_EC_prime192v3;
            return;
        }
        if (__is_oid(val, TLS_EC_prime239v1_OID, len)) {
            *algorithm = TLS_EC_prime239v1;
            return;
        }
        if (__is_oid(val, TLS_EC_prime239v2_OID, len)) {
            *algorithm = TLS_EC_prime239v2;
            return;
        }
        if (__is_oid(val, TLS_EC_prime239v3_OID, len)) {
            *algorithm = TLS_EC_prime239v3;
            return;
        }
        if (__is_oid(val, TLS_EC_prime256v1_OID, len)) {
            *algorithm = TLS_EC_prime256v1;
            return;
        }
    }
    if (len == 5) {
        if (__is_oid2(val, TLS_EC_secp224r1_OID, len, sizeof(TLS_EC_secp224r1_OID) - 1)) {
            *algorithm = TLS_EC_secp224r1;
            return;
        }
        if (__is_oid2(val, TLS_EC_secp384r1_OID, len, sizeof(TLS_EC_secp384r1_OID) - 1)) {
            *algorithm = TLS_EC_secp384r1;
            return;
        }
        if (__is_oid2(val, TLS_EC_secp521r1_OID, len, sizeof(TLS_EC_secp521r1_OID) - 1)) {
            *algorithm = TLS_EC_secp521r1;
            return;
        }
    }
    if (len != 9)
        return;
    
    if (__is_oid(val, TLS_RSA_SIGN_SHA256_OID, 9)) {
        *algorithm = TLS_RSA_SIGN_SHA256;
        return;
    }
    
    if (__is_oid(val, TLS_RSA_SIGN_RSA_OID, 9)) {
        *algorithm = TLS_RSA_SIGN_RSA;
        return;
    }
    
    if (__is_oid(val, TLS_RSA_SIGN_SHA1_OID, 9)) {
        *algorithm = TLS_RSA_SIGN_SHA1;
        return;
    }
    
    if (__is_oid(val, TLS_RSA_SIGN_SHA512_OID, 9)) {
        *algorithm = TLS_RSA_SIGN_SHA512;
        return;
    }
    
    if (__is_oid(val, TLS_RSA_SIGN_SHA384_OID, 9)) {
        *algorithm = TLS_RSA_SIGN_SHA384;
        return;
    }
    
    if (__is_oid(val, TLS_RSA_SIGN_MD5_OID, 9)) {
        *algorithm = TLS_RSA_SIGN_MD5;
        return;
    }
}

void tls_destroy_certificate(struct TLSCertificate *cert) {
    if (cert) {
        int i;
        TLS_FREE(cert->exponent);
        TLS_FREE(cert->pk);
        TLS_FREE(cert->issuer_country);
        TLS_FREE(cert->issuer_state);
        TLS_FREE(cert->issuer_location);
        TLS_FREE(cert->issuer_entity);
        TLS_FREE(cert->issuer_subject);
        TLS_FREE(cert->country);
        TLS_FREE(cert->state);
        TLS_FREE(cert->location);
        TLS_FREE(cert->subject);
        for (i = 0; i < cert->san_length; i++) {
            TLS_FREE(cert->san[i]);
        }
        TLS_FREE(cert->san);
        TLS_FREE(cert->ocsp);
        TLS_FREE(cert->serial_number);
        TLS_FREE(cert->entity);
        TLS_FREE(cert->not_before);
        TLS_FREE(cert->not_after);
        TLS_FREE(cert->sign_key);
        TLS_FREE(cert->priv);
        TLS_FREE(cert->der_bytes);
        TLS_FREE(cert->bytes);
        TLS_FREE(cert->fingerprint);
        TLS_FREE(cert);
    }
}

struct TLSPacket *tls_create_packet(struct TLSContext *context, unsigned char type, unsigned short version, int payload_size_hint) {
    struct TLSPacket *packet = (struct TLSPacket *)TLS_MALLOC(sizeof(struct TLSPacket));
    if (!packet)
        return NULL;
    packet->broken = 0;
    if (payload_size_hint > 0)
        packet->size = payload_size_hint + 10;
    else
        packet->size = __TLS_BLOB_INCREMENT;
    packet->buf = (unsigned char *)TLS_MALLOC(packet->size);
    packet->context = context;
    if (!packet->buf) {
        TLS_FREE(packet);
        return NULL;
    }
    if ((context) && (context->dtls))
        packet->len = 13;
    else
        packet->len = 5;
    packet->buf[0] = type;
    *(unsigned short *)&packet->buf[1] = htons(version);
    return packet;
}

void tls_destroy_packet(struct TLSPacket *packet) {
    if (packet) {
        if (packet->buf)
            TLS_FREE(packet->buf);
        TLS_FREE(packet);
    }
}

int __private_tls_crypto_create(struct TLSContext *context, int key_length, int iv_length, unsigned char *localkey, unsigned char *localiv, unsigned char *remotekey, unsigned char *remoteiv) {
    if (context->crypto.created) {
        if (context->crypto.created == 1) {
            cbc_done(&context->crypto.ctx_remote.aes_remote);
            cbc_done(&context->crypto.ctx_local.aes_local);
        } else {
#ifdef TLS_WITH_CHACHA20_POLY1305
            if (context->crypto.created == 2) {
#endif
            unsigned char dummy_buffer[32];
            unsigned long tag_len = 0;
            gcm_done(&context->crypto.ctx_remote.aes_gcm_remote, dummy_buffer, &tag_len);
            gcm_done(&context->crypto.ctx_local.aes_gcm_local, dummy_buffer, &tag_len);
#ifdef TLS_WITH_CHACHA20_POLY1305
            }
#endif
        }
        context->crypto.created = 0;
    }
    init_dependencies();
    int is_aead = __private_tls_is_aead(context);
    int cipherID = find_cipher("aes");
    DEBUG_PRINT("Using cipher ID: %x\n", (int)context->cipher);
#ifdef TLS_WITH_CHACHA20_POLY1305
    if (is_aead == 2) {
        unsigned int counter = 1;

        chacha_keysetup(&context->crypto.ctx_local.chacha_local, localkey, key_length * 8);
        chacha_ivsetup_96bitnonce(&context->crypto.ctx_local.chacha_local, localiv, (unsigned char *)&counter);

        chacha_keysetup(&context->crypto.ctx_remote.chacha_remote, remotekey, key_length * 8);
        chacha_ivsetup_96bitnonce(&context->crypto.ctx_remote.chacha_remote, remoteiv, (unsigned char *)&counter);

        context->crypto.created = 3;
    } else
#endif
    if (is_aead) {
        int res1 = gcm_init(&context->crypto.ctx_local.aes_gcm_local, cipherID, localkey, key_length);
        int res2 = gcm_init(&context->crypto.ctx_remote.aes_gcm_remote, cipherID, remotekey, key_length);
        
        if ((res1) || (res2))
            return TLS_GENERIC_ERROR;
        context->crypto.created = 2;
    } else {
        int res1 = cbc_start(cipherID, localiv, localkey, key_length, 0, &context->crypto.ctx_local.aes_local);
        int res2 = cbc_start(cipherID, remoteiv, remotekey, key_length, 0, &context->crypto.ctx_remote.aes_remote);
        
        if ((res1) || (res2))
            return TLS_GENERIC_ERROR;
        context->crypto.created = 1;
    }
    return 0;
}

int __private_tls_crypto_encrypt(struct TLSContext *context, unsigned char *buf, unsigned char *ct, unsigned int len) {
    if (context->crypto.created == 1)
        return cbc_encrypt(buf, ct, len, &context->crypto.ctx_local.aes_local);

    memset(ct, 0, len);
    return TLS_GENERIC_ERROR;
}

int __private_tls_crypto_decrypt(struct TLSContext *context, unsigned char *buf, unsigned char *pt, unsigned int len) {
    if (context->crypto.created == 1)
        return cbc_decrypt(buf, pt, len, &context->crypto.ctx_remote.aes_remote);
    
    memset(pt, 0, len);
    return TLS_GENERIC_ERROR;
}

void __private_tls_crypto_done(struct TLSContext *context) {
    unsigned char dummy_buffer[32];
    unsigned long tag_len = 0;
    switch (context->crypto.created) {
        case 1:
            cbc_done(&context->crypto.ctx_remote.aes_remote);
            cbc_done(&context->crypto.ctx_local.aes_local);
            break;
        case 2:
            gcm_done(&context->crypto.ctx_remote.aes_gcm_remote, dummy_buffer, &tag_len);
            gcm_done(&context->crypto.ctx_local.aes_gcm_local, dummy_buffer, &tag_len);
            break;
    }
    context->crypto.created = 0;
}

void tls_packet_update(struct TLSPacket *packet) {
    if ((packet) && (!packet->broken)) {
        unsigned int header_size = 5;
        if ((packet->context) && (packet->context->dtls)) {
            header_size = 13;
            *(unsigned short *)&packet->buf[3] = htons(packet->context->dtls_epoch_local);
            uint64_t sequence_number = packet->context->local_sequence_number;
            packet->buf[5] = sequence_number / 0x10000000000LL;
            sequence_number %= 0x10000000000LL;
            packet->buf[6] = sequence_number / 0x100000000LL;
            sequence_number %= 0x100000000LL;
            packet->buf[7] = sequence_number / 0x1000000;
            sequence_number %= 0x1000000;
            packet->buf[8] = sequence_number / 0x10000;
            sequence_number %= 0x10000;
            packet->buf[9] = sequence_number / 0x100;
            sequence_number %= 0x100;
            packet->buf[10] = sequence_number;

            *(unsigned short *)&packet->buf[11] = htons(packet->len - header_size);
        } else
            *(unsigned short *)&packet->buf[3] = htons(packet->len - header_size);
        if (packet->context) {
            if (packet->buf[0] != TLS_CHANGE_CIPHER)  {
                if ((packet->buf[0] == TLS_HANDSHAKE) && (packet->len > header_size)) {
                    unsigned char handshake_type = packet->buf[header_size];
                    if ((handshake_type != 0x00) && (handshake_type != 0x03))
                        __private_tls_update_hash(packet->context, packet->buf + header_size, packet->len - header_size);
                }
                
                if ((packet->context->cipher_spec_set) && (packet->context->crypto.created)) {
                    int block_size = __TLS_AES_BLOCK_SIZE;
                    int mac_size = 0;
                    unsigned int length = 0;
                    unsigned char padding = 0;
                    unsigned int pt_length = packet->len - header_size;
                    
                    if (packet->context->crypto.created == 1) {
                        mac_size = __private_tls_mac_length(packet->context);
#ifdef TLS_LEGACY_SUPPORT
                        if (packet->context->version == TLS_V10)
                            length = packet->len - header_size + mac_size;
                        else
#endif
                            length = packet->len - header_size + __TLS_AES_IV_LENGTH + mac_size;
                        padding = block_size - length % block_size;
                        length += padding;
#ifdef TLS_WITH_CHACHA20_POLY1305
                    } else
                    if (packet->context->crypto.created == 3) {
                        mac_size = POLY1305_TAGLEN;
                        length = packet->len - header_size + mac_size;
#endif
                    } else {
                        mac_size = __TLS_GCM_TAG_LEN;
                        length = packet->len - header_size + 8 + mac_size;
                    }
                    
                    
                    if (packet->context->crypto.created == 1) {
                        unsigned char *buf = (unsigned char *)TLS_MALLOC(length);
                        if (buf) {
                            unsigned char *ct = (unsigned char *)TLS_MALLOC(length + header_size);
                            if (ct) {
                                unsigned int buf_pos = 0;
                                memcpy(ct, packet->buf, header_size - 2);
                                *(unsigned short *)&ct[header_size - 2] = htons(length);
#ifdef TLS_LEGACY_SUPPORT
                                if (packet->context->version != TLS_V10)
#endif
                                {
                                    tls_random(buf, __TLS_AES_IV_LENGTH);
                                    buf_pos += __TLS_AES_IV_LENGTH;
                                }
                                // copy payload
                                memcpy(buf + buf_pos, packet->buf + header_size, packet->len - header_size);
                                buf_pos += packet->len - header_size;
                                if (packet->context->dtls) {
                                    unsigned char temp_buf[5];
                                    memcpy(temp_buf, packet->buf, 3);
                                    *(unsigned short *)&temp_buf[3] = *(unsigned short *)&packet->buf[header_size - 2];
                                    uint64_t dtls_sequence_number = ntohll(*(uint64_t *)&packet->buf[3]);
                                    __private_tls_hmac_message(1, packet->context, temp_buf, 5, packet->buf + header_size, packet->len  - header_size, buf + buf_pos, mac_size, dtls_sequence_number);
                                } else
                                    __private_tls_hmac_message(1, packet->context, packet->buf, packet->len, NULL, 0, buf + buf_pos, mac_size, 0);
                                buf_pos += mac_size;
                                
                                memset(buf + buf_pos, padding - 1, padding);
                                buf_pos += padding;
                                
                                //DEBUG_DUMP_HEX_LABEL("PT BUFFER", buf, length);
                                __private_tls_crypto_encrypt(packet->context, buf, ct + header_size, length);
                                TLS_FREE(packet->buf);
                                packet->buf = ct;
                                packet->len = length + header_size;
                                packet->size = packet->len;
                            } else {
                                // invalidate packet
                                memset(packet->buf, 0, packet->len);
                            }
                            TLS_FREE(buf);
                        } else {
                            // invalidate packet
                            memset(packet->buf, 0, packet->len);
                        }
                    } else
#ifdef TLS_WITH_CHACHA20_POLY1305
                    if (packet->context->crypto.created >= 2) {
#else
                    if (packet->context->crypto.created == 2) {
#endif
                        int ct_size = length + header_size + 12 + __TLS_MAX_TAG_LEN;
                        unsigned char *ct = (unsigned char *)TLS_MALLOC(ct_size);
                        if (ct) {
                            memset(ct, 0, ct_size);
                            // AEAD
                            // sequence number (8 bytes)
                            // content type (1 byte)
                            // version (2 bytes)
                            // length (2 bytes)
                            unsigned char aad[13];
                            if (packet->context->dtls)
                                *((uint64_t *)aad) = *(uint64_t *)&packet->buf[3];
                            else
                                *((uint64_t *)aad) = htonll(packet->context->local_sequence_number);
                            aad[8] = packet->buf[0];
                            aad[9] = packet->buf[1];
                            aad[10] = packet->buf[2];
                            *((unsigned short *)&aad[11]) = htons(packet->len - header_size);

                            int ct_pos = header_size;
#ifdef TLS_WITH_CHACHA20_POLY1305
                            if (packet->context->crypto.created == 3) {
                                unsigned int counter = 1;
                                unsigned char poly1305_key[POLY1305_KEYLEN];
				                chacha_ivupdate(&packet->context->crypto.ctx_local.chacha_local, packet->context->crypto.ctx_local_mac.local_aead_iv, aad, (u8 *)&counter);
                                chacha20_poly1305_key(&packet->context->crypto.ctx_local.chacha_local, poly1305_key);
                                ct_pos += chacha20_poly1305_aead(&packet->context->crypto.ctx_local.chacha_local, packet->buf + header_size, pt_length, aad, sizeof(aad), poly1305_key, ct + ct_pos);
                            } else {
#endif
                                unsigned char iv[12];
                                memcpy(iv, packet->context->crypto.ctx_local_mac.local_aead_iv, __TLS_AES_GCM_IV_LENGTH);
                                tls_random(iv + __TLS_AES_GCM_IV_LENGTH, 8);
                                memcpy(ct + ct_pos, iv + __TLS_AES_GCM_IV_LENGTH, 8);
                                ct_pos += 8;

                                gcm_reset(&packet->context->crypto.ctx_local.aes_gcm_local);
                                gcm_add_iv(&packet->context->crypto.ctx_local.aes_gcm_local, iv, 12);
                                gcm_add_aad(&packet->context->crypto.ctx_local.aes_gcm_local, aad, sizeof(aad));
                                
                                gcm_process(&packet->context->crypto.ctx_local.aes_gcm_local, packet->buf + header_size, pt_length, ct + ct_pos, GCM_ENCRYPT);
                                ct_pos += pt_length;
                                
                                unsigned long taglen = __TLS_GCM_TAG_LEN;
                                gcm_done(&packet->context->crypto.ctx_local.aes_gcm_local, ct + ct_pos, &taglen);
                                ct_pos += taglen;
#ifdef TLS_WITH_CHACHA20_POLY1305
                            }
#endif
                            
                            memcpy(ct, packet->buf, header_size - 2);
                            *(unsigned short *)&ct[header_size - 2] = htons(ct_pos - header_size);
                            TLS_FREE(packet->buf);
                            packet->buf = ct;
                            packet->len = ct_pos;
                            packet->size = ct_pos;
                        } else {
                            // invalidate packet
                            memset(packet->buf, 0, packet->len);
                        }
                    } else {
                        // invalidate packet (never reached)
                        memset(packet->buf, 0, packet->len);
                    }
                }
            } else
                packet->context->dtls_epoch_local++;
            packet->context->local_sequence_number++;
        }
    }
}

int tls_packet_append(struct TLSPacket *packet, unsigned char *buf, unsigned int len) {
    if ((!packet) || (packet->broken))
        return -1;
    
    if (!len)
        return 0;
    
    unsigned int new_len = packet->len + len;
    
    if (new_len > packet->size) {
        packet->size = (new_len / __TLS_BLOB_INCREMENT + 1) * __TLS_BLOB_INCREMENT;
        packet->buf = (unsigned char *)TLS_REALLOC(packet->buf, packet->size);
        if (!packet->buf) {
            packet->size = 0;
            packet->len = 0;
            packet->broken = 1;
            return -1;
        }
    }
    memcpy(packet->buf + packet->len, buf, len);
    packet->len = new_len;
    return new_len;
}

int tls_packet_uint8(struct TLSPacket *packet, unsigned char i) {
    return tls_packet_append(packet, &i, 1);
}

int tls_packet_uint16(struct TLSPacket *packet, unsigned short i) {
    unsigned short ni = htons(i);
    return tls_packet_append(packet, (unsigned char *)&ni, 2);
}

int tls_packet_uint32(struct TLSPacket *packet, unsigned int i) {
    unsigned int ni = htonl(i);
    return tls_packet_append(packet, (unsigned char *)&ni, 4);
}

int tls_packet_uint24(struct TLSPacket *packet, unsigned int i) {
    unsigned char buf[3];
    buf[0] = i / 0x10000;
    i %= 0x10000;
    buf[1] = i / 0x100;
    i %= 0x100;
    buf[2] = i;
    
    return tls_packet_append(packet, buf, 3);
}

int tls_random(unsigned char *key, int len) {
#ifdef __APPLE__
    for (int i = 0; i < len; i++) {
        unsigned int v = arc4random() % 0x100;
        key[i] = (char)v;
    }
    return 1;
#else
#ifdef _WIN32
    HCRYPTPROV hProvider = 0;
    if (CryptAcquireContext(&hProvider, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        if (CryptGenRandom(hProvider, len, (BYTE *)key)) {
            CryptReleaseContext(hProvider, 0);
            return 1;
        }
        CryptReleaseContext(hProvider, 0);
    }
#else
    FILE *fp = fopen("/dev/urandom", "r");
    if (fp) {
        int key_len = fread(key, 1, len, fp);
        fclose(fp);
        if (key_len == len)
            return 1;
    }
#endif
#endif
    return 0;
}

TLSHash *__private_tls_ensure_hash(struct TLSContext *context) {
    TLSHash *hash = context->handshake_hash;
    if (!hash) {
        hash = (TLSHash *)TLS_MALLOC(sizeof(TLSHash));
        if (hash)
            memset(hash, 0, sizeof(TLSHash));
        context->handshake_hash = hash;
    }
    return hash;
}

void __private_tls_destroy_hash(struct TLSContext *context) {
    if (context) {
        TLS_FREE(context->handshake_hash);
        context->handshake_hash = NULL;
    }
}

void __private_tls_create_hash(struct TLSContext *context) {
    if (!context)
        return;
    
    TLSHash *hash = __private_tls_ensure_hash(context);
    if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
        int hash_size = __private_tls_mac_length(context);
        if (hash->created) {
            unsigned char temp[__TLS_MAX_SHA_SIZE];
            if (hash_size == __TLS_SHA384_MAC_SIZE)
                sha384_done(&hash->hash, temp);
            else
                sha256_done(&hash->hash, temp);
        }
        if (hash_size == __TLS_SHA384_MAC_SIZE)
            sha384_init(&hash->hash);
        else
            sha256_init(&hash->hash);
        hash->created = 1;
    } else {
#ifdef TLS_LEGACY_SUPPORT
        // TLS_V11
        if (hash->created) {
            unsigned char temp[__TLS_V11_HASH_SIZE];
            md5_done(&hash->hash, temp);
            sha1_done(&hash->hash2, temp);
        }
        md5_init(&hash->hash);
        sha1_init(&hash->hash2);
        hash->created = 1;
#endif
    }
}

int __private_tls_update_hash(struct TLSContext *context, const unsigned char *in, unsigned int len) {
    if (!context)
        return 0;
    TLSHash *hash = __private_tls_ensure_hash(context);
    if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
        if (!hash->created) {
            __private_tls_create_hash(context);
#ifdef TLS_LEGACY_SUPPORT
            // cache first hello in case of protocol downgrade
            if ((!context->is_server) && (!context->cached_handshake) && (!context->request_client_certificate) && (len)) {
                context->cached_handshake = (unsigned char *)TLS_MALLOC(len);
                if (context->cached_handshake) {
                    memcpy(context->cached_handshake, in, len);
                    context->cached_handshake_len = len;
                }
            }
#endif
        }
        int hash_size = __private_tls_mac_length(context);
        if (hash_size == __TLS_SHA384_MAC_SIZE) {
            sha384_process(&hash->hash, in, len);
        } else {
            sha256_process(&hash->hash, in, len);
            hash_size = __TLS_SHA256_MAC_SIZE;
        }
    } else {
#ifdef TLS_LEGACY_SUPPORT
        if (!hash->created)
            __private_tls_create_hash(context);
        md5_process(&hash->hash, in, len);
        sha1_process(&hash->hash2, in, len);
#endif
    }
    if ((context->request_client_certificate) && (len)) {
        // cache all messages for verification
        int new_len = context->cached_handshake_len + len;
        context->cached_handshake = (unsigned char *)TLS_REALLOC(context->cached_handshake, new_len);
        if (context->cached_handshake) {
            memcpy(context->cached_handshake + context->cached_handshake_len, in, len);
            context->cached_handshake_len = new_len;
        } else
            context->cached_handshake_len = 0;
    }
    return 0;
}

#ifdef TLS_LEGACY_SUPPORT
int __private_tls_change_hash_type(struct TLSContext *context) {
    if (!context)
        return 0;
    TLSHash *hash = __private_tls_ensure_hash(context);
    if ((hash) && (hash->created) && (context->cached_handshake) && (context->cached_handshake_len)) {
        __private_tls_destroy_hash(context);
        int res = __private_tls_update_hash(context, context->cached_handshake, context->cached_handshake_len);
        TLS_FREE(context->cached_handshake);
        context->cached_handshake = NULL;
        context->cached_handshake_len = 0;
        return res;
    }
    return 0;
}
#endif

int __private_tls_done_hash(struct TLSContext *context, unsigned char *hout) {
    if (!context)
        return 0;
    
    TLSHash *hash = __private_tls_ensure_hash(context);
    if (!hash->created)
        return 0;
    
    int hash_size = 0;
    if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
        unsigned char temp[__TLS_MAX_SHA_SIZE];
        if (!hout)
            hout = temp;
        //__TLS_HASH_DONE(&hash->hash, hout);
        hash_size = __private_tls_mac_length(context);
        if (hash_size == __TLS_SHA384_MAC_SIZE)
            sha384_done(&hash->hash, hout);
        else {
            sha256_done(&hash->hash, hout);
            hash_size = __TLS_SHA256_MAC_SIZE;
        }
    } else {
#ifdef TLS_LEGACY_SUPPORT
        // TLS_V11
        unsigned char temp[__TLS_V11_HASH_SIZE];
        if (!hout)
            hout = temp;
        md5_done(&hash->hash, hout);
        sha1_done(&hash->hash2, hout + 16);
        hash_size = __TLS_V11_HASH_SIZE;
#endif
    }
    hash->created = 0;
    if (context->cached_handshake) {
        // not needed anymore
        TLS_FREE(context->cached_handshake);
        context->cached_handshake = NULL;
        context->cached_handshake_len = 0;
    }
    return hash_size;
}

int __private_tls_get_hash(struct TLSContext *context, unsigned char *hout) {
    if (!context)
        return 0;
    
    TLSHash *hash = __private_tls_ensure_hash(context);
    if (!hash->created)
        return 0;
    
    int hash_size = 0;
    if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
        hash_size = __private_tls_mac_length(context);
        hash_state prec;
        memcpy(&prec, &hash->hash, sizeof(hash_state));
        if (hash_size == __TLS_SHA384_MAC_SIZE)
            sha384_done(&hash->hash, hout);
        else {
            hash_size = __TLS_SHA256_MAC_SIZE;
            sha256_done(&hash->hash, hout);
        }
        memcpy(&hash->hash, &prec, sizeof(hash_state));
    } else {
#ifdef TLS_LEGACY_SUPPORT
        // TLS_V11
        hash_state prec;
        
        memcpy(&prec, &hash->hash, sizeof(hash_state));
        md5_done(&hash->hash, hout);
        memcpy(&hash->hash, &prec, sizeof(hash_state));
        
        memcpy(&prec, &hash->hash2, sizeof(hash_state));
        sha1_done(&hash->hash2, hout + 16);
        memcpy(&hash->hash2, &prec, sizeof(hash_state));
        
        hash_size = __TLS_V11_HASH_SIZE;
#endif
    }
    return hash_size;
}

int __private_tls_write_packet(struct TLSPacket *packet) {
    if (!packet)
        return -1;
    struct TLSContext *context = packet->context;
    if (!context)
        return -1;
    
    if (context->tls_buffer) {
        int len = context->tls_buffer_len + packet->len;
        context->tls_buffer = (unsigned char *)TLS_REALLOC(context->tls_buffer, len);
        if (!context->tls_buffer) {
            context->tls_buffer_len = 0;
            return -1;
        }
        memcpy(context->tls_buffer + context->tls_buffer_len, packet->buf, packet->len);
        context->tls_buffer_len = len;
        int written = packet->len;
        tls_destroy_packet(packet);
        return written;
    }
    context->tls_buffer_len = packet->len;
    context->tls_buffer = packet->buf;
    packet->buf = NULL;
    packet->len = 0;
    packet->size = 0;
    tls_destroy_packet(packet);
    return context->tls_buffer_len;
}

int __private_tls_write_app_data(struct TLSContext *context, const unsigned char *buf, unsigned int buf_len) {
    if (!context)
        return -1;
    if ((!buf) || (!buf_len))
        return 0;
    
    int len = context->application_buffer_len + buf_len;
    context->application_buffer = (unsigned char *)TLS_REALLOC(context->application_buffer, len);
    if (!context->application_buffer) {
        context->application_buffer_len = 0;
        return -1;
    }
    memcpy(context->application_buffer + context->application_buffer_len, buf, buf_len);
    context->application_buffer_len = len;
    return buf_len;
}

const unsigned char *tls_get_write_buffer(struct TLSContext *context, unsigned int *outlen) {
    if (!outlen)
        return NULL;
    if (!context) {
        *outlen = 0;
        return NULL;
    }
    // check if any error
    if (context->sleep_until) {
        if (context->sleep_until < time(NULL)) {
            *outlen = 0;
            return NULL;
        }
        context->sleep_until = 0;
    }
    *outlen = context->tls_buffer_len;
    return context->tls_buffer;
}

const unsigned char *tls_get_message(struct TLSContext *context, unsigned int *outlen, int offset) {
    if (!outlen)
        return NULL;
    if ((!context) || (!context->tls_buffer)) {
        *outlen = 0;
        return NULL;
    }

    if (offset >= context->tls_buffer_len) {
        *outlen = 0;
        return NULL;
    }
    // check if any error
    if (context->sleep_until) {
        if (context->sleep_until < time(NULL)) {
            *outlen = 0;
            return NULL;
        }
        context->sleep_until = 0;
    }
    unsigned char *tls_buffer = &context->tls_buffer[offset];
    unsigned int tls_buffer_len = context->tls_buffer_len - offset;
    int len = 0;
    if (context->dtls) {
        if (tls_buffer_len < 13) {
            *outlen = 0;
            return NULL;
        }

        len = ntohs(*(unsigned short *)&tls_buffer[11]) + 13;
    } else {
        if (tls_buffer_len < 5) {
            *outlen = 0;
            return NULL;
        }
        len = ntohs(*(unsigned short *)&tls_buffer[3]) + 5;
    }
    if (len > tls_buffer_len) {
        *outlen = 0;
        return NULL;
    }

    *outlen = len;
    return tls_buffer;
}

void tls_buffer_clear(struct TLSContext *context) {
    if ((context) && (context->tls_buffer)) {
        TLS_FREE(context->tls_buffer);
        context->tls_buffer = NULL;
        context->tls_buffer_len = 0;
    }
}

int tls_established(struct TLSContext *context) {
    if (context) {
        if (context->critical_error)
            return -1;
        
        if (context->connection_status == 0xFF)
            return 1;
    }
    return 0;
}

void tls_read_clear(struct TLSContext *context) {
    if ((context) && (context->application_buffer)) {
        TLS_FREE(context->application_buffer);
        context->application_buffer = NULL;
        context->application_buffer_len = 0;
    }
}

int tls_read(struct TLSContext *context, unsigned char *buf, unsigned int size) {
    if (!context)
        return -1;
    if ((context->application_buffer) && (context->application_buffer_len)) {
        if (context->application_buffer_len < size)
            size = context->application_buffer_len;
        
        memcpy(buf, context->application_buffer, size);
        if (context->application_buffer_len == size) {
            TLS_FREE(context->application_buffer);
            context->application_buffer = NULL;
            context->application_buffer_len = 0;
            return size;
        }
        context->application_buffer_len -= size;
        memmove(context->application_buffer, context->application_buffer + size, context->application_buffer_len);
        return size;
    }
    return 0;
}

struct TLSContext *tls_create_context(unsigned char is_server, unsigned short version) {
    struct TLSContext *context = (struct TLSContext *)TLS_MALLOC(sizeof(struct TLSContext));
    if (context) {
        memset(context, 0, sizeof(struct TLSContext));
        context->is_server = is_server;
        if ((version == DTLS_V12) || (version == DTLS_V10))
            context->dtls = 1;
        context->version = version;
    }
    return context;
}

#ifdef TLS_FORWARD_SECRECY
const struct ECCCurveParameters *tls_set_curve(struct TLSContext *context, const struct ECCCurveParameters *curve) {
    if (!context->is_server)
        return NULL;
    const struct ECCCurveParameters *old_curve = context->curve;
    context->curve = curve;
    return old_curve;
}
#endif

struct TLSContext *tls_accept(struct TLSContext *context) {
    if ((!context) || (!context->is_server))
        return NULL;
    
    struct TLSContext *child = (struct TLSContext *)TLS_MALLOC(sizeof(struct TLSContext));
    if (child) {
        memset(child, 0, sizeof(struct TLSContext));
        child->is_server = 1;
        child->is_child = 1;
        child->dtls = context->dtls;
        child->version = context->version;
        child->certificates = context->certificates;
        child->certificates_count = context->certificates_count;
        child->private_key = context->private_key;
#ifdef TLS_ECDSA_SUPPORTED
        child->ec_private_key = context->ec_private_key;
#endif
        child->exportable = context->exportable;
        child->root_certificates = context->root_certificates;
        child->root_count = context->root_count;
#ifdef TLS_FORWARD_SECRECY
        child->default_dhe_p = context->default_dhe_p;
        child->default_dhe_g = context->default_dhe_g;
        child->curve = context->curve;
#endif
        child->alpn = context->alpn;
        child->alpn_count = context->alpn_count;
    }
    return child;
}

#ifdef TLS_FORWARD_SECRECY
void __private_tls_dhe_free(struct TLSContext *context) {
    if (context->dhe) {
        __private_tls_dh_clear_key(context->dhe);
        TLS_FREE(context->dhe);
        context->dhe = NULL;
    }
}

void __private_tls_dhe_create(struct TLSContext *context) {
    __private_tls_dhe_free(context);
    context->dhe = (DHKey *)TLS_MALLOC(sizeof(DHKey));
    if (context->dhe)
        memset(context->dhe, 0, sizeof(DHKey));
}

void __private_tls_ecc_dhe_free(struct TLSContext *context) {
    if (context->ecc_dhe) {
        ecc_free(context->ecc_dhe);
        TLS_FREE(context->ecc_dhe);
        context->ecc_dhe = NULL;
    }
}

void __private_tls_ecc_dhe_create(struct TLSContext *context) {
    __private_tls_ecc_dhe_free(context);
    context->ecc_dhe = (ecc_key *)TLS_MALLOC(sizeof(ecc_key));
    memset(context->ecc_dhe, 0, sizeof(ecc_key));
}

int tls_set_default_dhe_pg(struct TLSContext *context, const char *p_hex_str, const char *g_hex_str) {
    if ((!context) || (context->is_child) || (!context->is_server) || (!p_hex_str) || (!g_hex_str))
        return 0;
    
    TLS_FREE(context->default_dhe_p);
    TLS_FREE(context->default_dhe_g);
    
    context->default_dhe_p = NULL;
    context->default_dhe_g = NULL;
    
    int p_len = strlen(p_hex_str);
    int g_len = strlen(g_hex_str);
    if ((p_len <= 0) || (g_len <= 0))
        return 0;
    context->default_dhe_p = (char *)TLS_MALLOC(p_len + 1);
    if (!context->default_dhe_p)
        return 0;
    context->default_dhe_g = (char *)TLS_MALLOC(g_len + 1);
    if (!context->default_dhe_g)
        return 0;
    
    memcpy(context->default_dhe_p, p_hex_str, p_len);
    context->default_dhe_p[p_len] = 0;
    
    memcpy(context->default_dhe_g, g_hex_str, g_len);
    context->default_dhe_g[g_len] = 0;
    return 1;
}
#endif

const char *tls_alpn(struct TLSContext *context) {
    if (!context)
        return NULL;
    return context->negotiated_alpn;
}

int tls_add_alpn(struct TLSContext *context, const char *alpn) {
    if ((!context) || (!alpn) || (!alpn[0]) || ((context->is_server) && (context->is_child)))
        return TLS_GENERIC_ERROR;
    int len = strlen(alpn);
    if (tls_alpn_contains(context, alpn, len))
        return 0;
    context->alpn = (char **)TLS_REALLOC(context->alpn, (context->alpn_count + 1) * sizeof(char *));
    if (!context->alpn) {
        context->alpn_count = 0;
        return TLS_NO_MEMORY;
    }
    char *alpn_ref = (char *)TLS_MALLOC(len+1);
    context->alpn[context->alpn_count] = alpn_ref;
    if (alpn_ref) {
        memcpy(alpn_ref, alpn, len);
        alpn_ref[len] = 0;
        context->alpn_count++;
    } else
        return TLS_NO_MEMORY;
    return 0;
}

int tls_alpn_contains(struct TLSContext *context, const char *alpn, unsigned char alpn_size) {
    if ((!context) || (!alpn) || (!alpn_size))
        return 0;

    if (context->alpn) {
        int i;
        for (i = 0; i < context->alpn_count; i++) {
            const char *alpn_local = context->alpn[i];
            if (alpn_local) {
                int len = strlen(alpn_local);
                if (alpn_size == len) {
                    if (!memcmp(alpn_local, alpn, alpn_size))
                        return 1;
                }
            }
        }
    }
    return 0;
}

void tls_destroy_context(struct TLSContext *context) {
    unsigned int i;
    if (!context)
        return;
    if (!context->is_child) {
        if (context->certificates) {
            for (i = 0; i < context->certificates_count; i++)
                tls_destroy_certificate(context->certificates[i]);
        }
        if (context->root_certificates) {
            for (i = 0; i < context->root_count; i++)
                tls_destroy_certificate(context->root_certificates[i]);
        }
        if (context->private_key)
            tls_destroy_certificate(context->private_key);
#ifdef TLS_ECDSA_SUPPORTED
        if (context->ec_private_key)
            tls_destroy_certificate(context->ec_private_key);
#endif
        TLS_FREE(context->certificates);
#ifdef TLS_FORWARD_SECRECY
        TLS_FREE(context->default_dhe_p);
        TLS_FREE(context->default_dhe_g);
#endif
        if (context->alpn) {
            for (i = 0; i < context->alpn_count; i++)
                TLS_FREE(context->alpn[i]);
            TLS_FREE(context->alpn);
        }
    }
    if (context->client_certificates) {
        for (i = 0; i < context->client_certificates_count; i++)
            tls_destroy_certificate(context->client_certificates[i]);
        TLS_FREE(context->client_certificates);
    }
    TLS_FREE(context->master_key);
    TLS_FREE(context->premaster_key);
    if (context->crypto.created)
        __private_tls_crypto_done(context);
    TLS_FREE(context->message_buffer);
    __private_tls_done_hash(context, NULL);
    __private_tls_destroy_hash(context);
    TLS_FREE(context->tls_buffer);
    TLS_FREE(context->application_buffer);
    // zero out the keys before free
    if ((context->exportable_keys) && (context->exportable_size))
        memset(context->exportable_keys, 0, context->exportable_size);
    TLS_FREE(context->exportable_keys);
    TLS_FREE(context->sni);
    TLS_FREE(context->dtls_cookie);
    TLS_FREE(context->cached_handshake);
#ifdef TLS_FORWARD_SECRECY
    __private_tls_dhe_free(context);
    __private_tls_ecc_dhe_free(context);
#endif
#ifdef TLS_ACCEPT_SECURE_RENEGOTIATION
    TLS_FREE(context->verify_data);
#endif
    if (context->client_certificates) {
        for (i = 0; i < context->client_certificates_count; i++)
            tls_destroy_certificate(context->client_certificates[i]);
        TLS_FREE(context->client_certificates);
    }
    TLS_FREE(context->negotiated_alpn);
    TLS_FREE(context);
}

#ifdef TLS_ACCEPT_SECURE_RENEGOTIATION
void __private_tls_reset_context(struct TLSContext *context) {
    unsigned int i;
    if (!context)
        return;
    if (!context->is_child) {
        if (context->certificates) {
            for (i = 0; i < context->certificates_count; i++)
                tls_destroy_certificate(context->certificates[i]);
        }
        context->certificates = NULL;
        if (context->private_key) {
            tls_destroy_certificate(context->private_key);
            context->private_key = NULL;
        }
#ifdef TLS_ECDSA_SUPPORTED
        if (context->ec_private_key) {
            tls_destroy_certificate(context->ec_private_key);
            context->ec_private_key = NULL;
        }
#endif
        TLS_FREE(context->certificates);
        context->certificates = NULL;
#ifdef TLS_FORWARD_SECRECY
        TLS_FREE(context->default_dhe_p);
        TLS_FREE(context->default_dhe_g);
        context->default_dhe_p = NULL;
        context->default_dhe_g = NULL;
#endif
    }
    if (context->client_certificates) {
        for (i = 0; i < context->client_certificates_count; i++)
            tls_destroy_certificate(context->client_certificates[i]);
        TLS_FREE(context->client_certificates);
    }
    context->client_certificates = NULL;
    TLS_FREE(context->master_key);
    context->master_key = NULL;
    TLS_FREE(context->premaster_key);
    context->premaster_key = NULL;
    if (context->crypto.created)
        __private_tls_crypto_done(context);
    __private_tls_done_hash(context, NULL);
    __private_tls_destroy_hash(context);
    TLS_FREE(context->application_buffer);
    context->application_buffer = NULL;
    // zero out the keys before free
    if ((context->exportable_keys) && (context->exportable_size))
        memset(context->exportable_keys, 0, context->exportable_size);
    TLS_FREE(context->exportable_keys);
    context->exportable_keys =  NULL;
    TLS_FREE(context->sni);
    context->sni = NULL;
    TLS_FREE(context->dtls_cookie);
    context->dtls_cookie = NULL;
    TLS_FREE(context->cached_handshake);
    context->cached_handshake = NULL;
    context->connection_status = 0;
#ifdef TLS_FORWARD_SECRECY
    __private_tls_dhe_free(context);
    __private_tls_ecc_dhe_free(context);
#endif
}
#endif

int tls_cipher_supported(struct TLSContext *context, unsigned short cipher) {
    if (!context)
        return 0;
    switch (cipher) {
#ifdef TLS_FORWARD_SECRECY
#ifdef TLS_ECDSA_SUPPORTED
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
#ifdef TLS_CLIENT_ECDSA
            if ((context) && (((context->certificates) && (context->certificates_count) && (context->ec_private_key)) || (!context->is_server)))
#else
            if ((context) && (context->certificates) && (context->certificates_count) && (context->ec_private_key))
#endif
                return 1;
            return 0;
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
#ifdef TLS_WITH_CHACHA20_POLY1305
        case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
#endif
            if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
#ifdef TLS_CLIENT_ECDSA
                if ((context) && (((context->certificates) && (context->certificates_count) && (context->ec_private_key)) || (!context->is_server)))
#else
                if ((context) && (context->certificates) && (context->certificates_count) && (context->ec_private_key))
#endif
                    return 1;
            }
            return 0;
#endif
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
#endif
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA:
            return 1;
#ifdef TLS_FORWARD_SECRECY
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
#ifdef TLS_WITH_CHACHA20_POLY1305
        case TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
        case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
#endif
#endif
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_RSA_WITH_AES_256_GCM_SHA384:
            if ((context->version == TLS_V12) || (context->version == DTLS_V12))
                return 1;
            return 0;
    }
    return 0;
}

int tls_cipher_is_fs(struct TLSContext *context, unsigned short cipher) {
    if (!context)
        return 0;
    switch (cipher) {
#ifdef TLS_ECDSA_SUPPORTED
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
#ifdef TLS_WITH_CHACHA20_POLY1305
        case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
#endif
            if ((context) && (context->certificates) && (context->certificates_count) && (context->ec_private_key))
                return 1;
            return 0;
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
            if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
                if ((context) && (context->certificates) && (context->certificates_count) && (context->ec_private_key))
                    return 1;
            }
            return 0;
#endif
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
            return 1;
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
#ifdef TLS_WITH_CHACHA20_POLY1305
        case TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
        case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
#endif
            if ((context->version == TLS_V12) || (context->version == DTLS_V12))
                return 1;
            break;
    }
    return 0;
}

int tls_choose_cipher(struct TLSContext *context, const unsigned char *buf, int buf_len, int *scsv_set) {
    int i;
    if (scsv_set)
        *scsv_set = 0;
    if (!context)
        return 0;
    int selected_cipher = TLS_NO_COMMON_CIPHER;
#ifdef TLS_FORWARD_SECRECY
    for (i = 0; i < buf_len; i+=2) {
        unsigned short cipher = ntohs(*(unsigned short *)&buf[i]);
        if (tls_cipher_is_fs(context, cipher)) {
            selected_cipher = cipher;
            break;
        }
    }
#endif
    for (i = 0; i < buf_len; i+=2) {
        unsigned short cipher = ntohs(*(unsigned short *)&buf[i]);
        if (cipher == TLS_FALLBACK_SCSV) {
            if (scsv_set)
                *scsv_set = 1;
            if (selected_cipher != TLS_NO_COMMON_CIPHER)
                break;
        } else
        if ((selected_cipher == TLS_NO_COMMON_CIPHER) && (tls_cipher_supported(context, cipher)))
            selected_cipher = cipher;
    }
    return selected_cipher;
}

int tls_cipher_is_ephemeral(struct TLSContext *context) {
    if (context) {
        switch (context->cipher) {
            case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
            case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
            case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
            case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
            case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
            case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
            case TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
                return 1;
            case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
            case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
            case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
            case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
            case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
            case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
            case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
            case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
            case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
            case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
            case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
            case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
            case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
                return 2;
        }
    }
    return 0;
}

const char *tls_cipher_name(struct TLSContext *context) {
    if (context) {
        switch (context->cipher) {
            case TLS_RSA_WITH_AES_128_CBC_SHA:
                return "RSA-AES128CBC-SHA";
            case TLS_RSA_WITH_AES_256_CBC_SHA:
                return "RSA-AES256CBC-SHA";
            case TLS_RSA_WITH_AES_128_CBC_SHA256:
                return "RSA-AES128CBC-SHA256";
            case TLS_RSA_WITH_AES_256_CBC_SHA256:
                return "RSA-AES256CBC-SHA256";
            case TLS_RSA_WITH_AES_128_GCM_SHA256:
                return "RSA-AES128GCM-SHA256";
            case TLS_RSA_WITH_AES_256_GCM_SHA384:
                return "RSA-AES256GCM-SHA384";
            case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
                return "DHE-RSA-AES128CBC-SHA";
            case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
                return "DHE-RSA-AES256CBC-SHA";
            case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
                return "DHE-RSA-AES128CBC-SHA256";
            case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
                return "DHE-RSA-AES256CBC-SHA256";
            case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
                return "DHE-RSA-AES128GCM-SHA256";
            case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
                return "DHE-RSA-AES256GCM-SHA384";
            case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
                return "ECDHE-RSA-AES128CBC-SHA";
            case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
                return "ECDHE-RSA-AES256CBC-SHA";
            case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256:
                return "ECDHE-RSA-AES128CBC-SHA256";
            case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
                return "ECDHE-RSA-AES128GCM-SHA256";
            case TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384:
                return "ECDHE-RSA-AES256GCM-SHA384";
            case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
                return "ECDHE-ECDSA-AES128CBC-SHA";
            case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
                return "ECDHE-ECDSA-AES256CBC-SHA";
            case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
                return "ECDHE-ECDSA-AES128CBC-SHA256";
            case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
                return "ECDHE-ECDSA-AES256CBC-SHA384";
            case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
                return "ECDHE-ECDSA-AES128GCM-SHA256";
            case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
                return "ECDHE-ECDSA-AES256GCM-SHA384";
            case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
                return "ECDHE-RSA-CHACHA20-POLY1305-SHA256";
            case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
                return "ECDHE-ECDSA-CHACHA20-POLY1305-SHA256";
            case TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
                return "ECDHE-DHE-CHACHA20-POLY1305-SHA256";
        }
    }
    return "UNKNOWN";
}

#ifdef TLS_FORWARD_SECRECY
int __private_tls_dh_export_pqY(unsigned char *pbuf, unsigned long *plen, unsigned char *gbuf, unsigned long *glen, unsigned char *Ybuf, unsigned long *Ylen, DHKey *key) {
    unsigned long len;
    int err;
    
    if ((pbuf  == NULL) || (plen  == NULL) || (gbuf == NULL) || (glen == NULL) || (Ybuf == NULL) || (Ylen == NULL) || (key == NULL))
        return TLS_GENERIC_ERROR;
    
    len = mp_unsigned_bin_size(key->y);
    if (len > *Ylen)
        return TLS_GENERIC_ERROR;
    
    if ((err = mp_to_unsigned_bin(key->y, Ybuf)) != CRYPT_OK)
        return err;
    
    *Ylen = len;
    
    len = mp_unsigned_bin_size(key->p);
    if (len > *plen)
        return TLS_GENERIC_ERROR;
    
    if ((err = mp_to_unsigned_bin(key->p, pbuf)) != CRYPT_OK)
        return err;
    
    *plen = len;
    
    len = mp_unsigned_bin_size(key->g);
    if (len > *glen)
        return TLS_GENERIC_ERROR;
    
    if ((err = mp_to_unsigned_bin(key->g, gbuf)) != CRYPT_OK)
        return err;
    
    *glen = len;
    
    return 0;
}

void __private_tls_dh_clear_key(DHKey *key) {
    mp_clear_multi(key->g, key->p, key->x, key->y, NULL);
    key->g = NULL;
    key->p = NULL;
    key->x = NULL;
    key->y = NULL;
}

int __private_tls_dh_make_key(int keysize, DHKey *key, const char *pbuf, const char *gbuf, int pbuf_len, int gbuf_len) {
    unsigned char *buf;
    int err;
    if (!key)
        return TLS_GENERIC_ERROR;
    
    static prng_state prng;
    int wprng = find_prng("sprng");
    if ((err = prng_is_valid(wprng)) != CRYPT_OK)
        return err;
    
    buf = (unsigned char *)TLS_MALLOC(keysize);
    if (!buf)
        return TLS_NO_MEMORY;
    
    if (rng_make_prng(keysize, wprng, &prng, NULL) != CRYPT_OK) {
        TLS_FREE(buf);
        return TLS_GENERIC_ERROR;
    }
    
    if (prng_descriptor[wprng].read(buf, keysize, &prng) != (unsigned long)keysize) {
        TLS_FREE(buf);
        return TLS_GENERIC_ERROR;
    }
    
    if ((err = mp_init_multi(&key->g, &key->p, &key->x, &key->y, NULL)) != CRYPT_OK) {
        TLS_FREE(buf);
        
        return TLS_GENERIC_ERROR;
    }
    
    if (gbuf_len <= 0) {
        if ((err = mp_read_radix(key->g, gbuf, 16)) != CRYPT_OK) {
            TLS_FREE(buf);
            __private_tls_dh_clear_key(key);
            return TLS_GENERIC_ERROR;
        }
    } else {
        if ((err = mp_read_unsigned_bin(key->g, (unsigned char *)gbuf, gbuf_len)) != CRYPT_OK) {
            TLS_FREE(buf);
            __private_tls_dh_clear_key(key);
            return TLS_GENERIC_ERROR;
        }
    }
    
    if (pbuf_len <= 0) {
        if ((err = mp_read_radix(key->p, pbuf, 16)) != CRYPT_OK) {
            TLS_FREE(buf);
            __private_tls_dh_clear_key(key);
            return TLS_GENERIC_ERROR;
        }
    } else {
        if ((err = mp_read_unsigned_bin(key->p, (unsigned char *)pbuf, pbuf_len)) != CRYPT_OK) {
            TLS_FREE(buf);
            __private_tls_dh_clear_key(key);
            return TLS_GENERIC_ERROR;
        }
    }
    
    if ((err = mp_read_unsigned_bin(key->x, buf, keysize)) != CRYPT_OK) {
        TLS_FREE(buf);
        __private_tls_dh_clear_key(key);
        return TLS_GENERIC_ERROR;
    }
    
    if ((err = mp_exptmod(key->g, key->x, key->p, key->y)) != CRYPT_OK) {
        TLS_FREE(buf);
        __private_tls_dh_clear_key(key);
        return TLS_GENERIC_ERROR;
    }
    
    TLS_FREE(buf);
    return 0;
}
#endif

int tls_is_ecdsa(struct TLSContext *context) {
    if (!context)
        return 0;
    switch (context->cipher) {
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
        case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384:
        case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
        case TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384:
#ifdef TLS_WITH_CHACHA20_POLY1305
        case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
#endif
            return 1;
    }
    return 0;
}

struct TLSPacket *tls_build_client_key_exchange(struct TLSContext *context) {
    if (context->is_server) {
        DEBUG_PRINT("CANNOT BUILD CLIENT KEY EXCHANGE MESSAGE FOR SERVERS\n");
        return NULL;
    }
    
    struct TLSPacket *packet = tls_create_packet(context, TLS_HANDSHAKE, context->version, 0);
    tls_packet_uint8(packet, 0x10);
#ifdef TLS_FORWARD_SECRECY
    int ephemeral = tls_cipher_is_ephemeral(context);
    if ((ephemeral) && (context->premaster_key) && (context->premaster_key_len)) {
        if (ephemeral == 1) {
            unsigned char dh_Ys[0xFFF];
            unsigned char dh_p[0xFFF];
            unsigned char dh_g[0xFFF];
            unsigned long dh_p_len = sizeof(dh_p);
            unsigned long dh_g_len = sizeof(dh_g);
            unsigned long dh_Ys_len = sizeof(dh_Ys);
            
            if (__private_tls_dh_export_pqY(dh_p, &dh_p_len, dh_g, &dh_g_len, dh_Ys, &dh_Ys_len, context->dhe)) {
                DEBUG_PRINT("ERROR EXPORTING DHE KEY %x\n", context->dhe);
                TLS_FREE(packet);
                __private_tls_dhe_free(context);
                return NULL;
            }
            __private_tls_dhe_free(context);
            DEBUG_DUMP_HEX_LABEL("Yc", dh_Ys, dh_Ys_len);
            tls_packet_uint24(packet, dh_Ys_len + 2);
            if (context->dtls)
                __private_dtls_handshake_data(context, packet, dh_Ys_len + 2);
            tls_packet_uint16(packet, dh_Ys_len);
            tls_packet_append(packet, dh_Ys, dh_Ys_len);
        } else
        if (context->ecc_dhe) {
            unsigned char out[__TLS_MAX_RSA_KEY];
            unsigned long out_len = __TLS_MAX_RSA_KEY;
            
            if (ecc_ansi_x963_export(context->ecc_dhe, out, &out_len)) {
                DEBUG_PRINT("Error exporting ECC key\n");
                TLS_FREE(packet);
                return NULL;
            }
            __private_tls_ecc_dhe_free(context);
            tls_packet_uint24(packet, out_len + 1);
            if (context->dtls) {
                __private_dtls_handshake_data(context, packet, out_len + 1);
                context->dtls_seq++;
            }
            tls_packet_uint8(packet, out_len);
            tls_packet_append(packet, out, out_len);
        }
        __private_tls_compute_key(context, 48);
    } else
#endif
    __private_tls_build_random(packet);
    context->connection_status = 2;
    tls_packet_update(packet);
    return packet;
}

void __private_dtls_handshake_data(struct TLSContext *context, struct TLSPacket *packet, unsigned int framelength) {
    // message seq
    tls_packet_uint16(packet, context->dtls_seq);
    // fragment offset
    tls_packet_uint24(packet, 0);
    // fragment length
    tls_packet_uint24(packet, framelength);
}

void __private_dtls_handshake_copyframesize(struct TLSContext *context, struct TLSPacket *packet) {
    packet->buf[22] = packet->buf[14];
    packet->buf[23] = packet->buf[15];
    packet->buf[24] = packet->buf[16];
}

struct TLSPacket *tls_build_server_key_exchange(struct TLSContext *context, int method) {
    if (!context->is_server) {
        DEBUG_PRINT("CANNOT BUILD SERVER KEY EXCHANGE MESSAGE FOR CLIENTS\n");
        return NULL;
    }
    
    struct TLSPacket *packet = tls_create_packet(context, TLS_HANDSHAKE, context->version, 0);
    tls_packet_uint8(packet, 0x0C);
    unsigned char dummy[3];
    tls_packet_append(packet, dummy, 3);
    if (context->dtls)
        __private_dtls_handshake_data(context, packet, 0);
    int start_len = packet->len;
#ifdef TLS_FORWARD_SECRECY
    if (method == KEA_dhe_rsa) {
        init_dependencies();
        __private_tls_dhe_create(context);
        
        const char *default_dhe_p = context->default_dhe_p;
        const char *default_dhe_g = context->default_dhe_g;
        int key_size;
        if ((!default_dhe_p) || (!default_dhe_g)) {
            default_dhe_p = TLS_DH_DEFAULT_P;
            default_dhe_g = TLS_DH_DEFAULT_G;
            key_size = __TLS_DHE_KEY_SIZE / 8;
        } else {
            if (default_dhe_p)
                key_size = strlen(default_dhe_p);
            else
                key_size = strlen(default_dhe_g);
        }
        if (__private_tls_dh_make_key(key_size, context->dhe, default_dhe_p, default_dhe_g, 0, 0)) {
            DEBUG_PRINT("ERROR CREATING DHE KEY\n");
            TLS_FREE(packet);
            TLS_FREE(context->dhe);
            context->dhe = NULL;
            return NULL;
        }
        
        unsigned char dh_Ys[0xFFF];
        unsigned char dh_p[0xFFF];
        unsigned char dh_g[0xFFF];
        unsigned long dh_p_len = sizeof(dh_p);
        unsigned long dh_g_len = sizeof(dh_g);
        unsigned long dh_Ys_len = sizeof(dh_Ys);
        
        if (__private_tls_dh_export_pqY(dh_p, &dh_p_len, dh_g, &dh_g_len, dh_Ys, &dh_Ys_len, context->dhe)) {
            DEBUG_PRINT("ERROR EXPORTING DHE KEY\n");
            TLS_FREE(packet);
            return NULL;
        }
        
        DEBUG_PRINT("LEN: %i (%i, %i)\n", dh_Ys_len, dh_p_len, dh_g_len);
        DEBUG_DUMP_HEX_LABEL("DHE PK", dh_Ys, dh_Ys_len);
        DEBUG_DUMP_HEX_LABEL("DHE P", dh_p, dh_p_len);
        DEBUG_DUMP_HEX_LABEL("DHE G", dh_g, dh_g_len);
        
        tls_packet_uint16(packet, dh_p_len);
        tls_packet_append(packet, dh_p, dh_p_len);
        
        tls_packet_uint16(packet, dh_g_len);
        tls_packet_append(packet, dh_g, dh_g_len);
        
        tls_packet_uint16(packet, dh_Ys_len);
        tls_packet_append(packet, dh_Ys, dh_Ys_len);
        //dh_p
        //dh_g
        //dh_Ys
    } else
    if (method == KEA_ec_diffie_hellman) {
        // 3 = named curve
        if (!context->curve)
            context->curve = default_curve;
        tls_packet_uint8(packet, 3);
        tls_packet_uint16(packet, context->curve->iana);
        init_dependencies();
        __private_tls_ecc_dhe_create(context);
        
        ltc_ecc_set_type *dp = (ltc_ecc_set_type *)&context->curve->dp;
        
        if (ecc_make_key_ex(NULL, find_prng("sprng"), context->ecc_dhe, dp)) {
            TLS_FREE(context->ecc_dhe);
            context->ecc_dhe = NULL;
            DEBUG_PRINT("Error generatic ECC key\n");
            TLS_FREE(packet);
            return NULL;
        }
        unsigned char out[__TLS_MAX_RSA_KEY];
        unsigned long out_len = __TLS_MAX_RSA_KEY;
        if (ecc_ansi_x963_export(context->ecc_dhe, out, &out_len)) {
            DEBUG_PRINT("Error exporting ECC key\n");
            TLS_FREE(packet);
            return NULL;
        }
        tls_packet_uint8(packet, out_len);
        tls_packet_append(packet, out, out_len);
    } else
#endif
    {
        TLS_FREE(packet);
        DEBUG_PRINT("Unsupported ephemeral method: %i\n", method);
        return NULL;
    }
    
    // signature
    unsigned int params_len = packet->len - start_len;
    unsigned int message_len = params_len + __TLS_CLIENT_RANDOM_SIZE + __TLS_SERVER_RANDOM_SIZE;
    unsigned char *message = (unsigned char *)TLS_MALLOC(message_len);
    if (message) {
        unsigned char out[__TLS_MAX_RSA_KEY];
        unsigned long out_len = __TLS_MAX_RSA_KEY;
        
        int hash_algorithm;
        if ((context->version != TLS_V12) && (context->version != DTLS_V12)) {
            hash_algorithm = __md5_sha1;
        } else {
            if ((context->version == TLS_V12) || (context->version == DTLS_V12))
                hash_algorithm = sha256;
            else
                hash_algorithm = sha1;
            
#ifdef TLS_ECDSA_SUPPORTED
            if (tls_is_ecdsa(context)) {
                if ((context->version == TLS_V12) || (context->version == DTLS_V12))
                    hash_algorithm = sha512;
                tls_packet_uint8(packet, hash_algorithm);
                tls_packet_uint8(packet, ecdsa);
            } else
#endif
            {
                tls_packet_uint8(packet, hash_algorithm);
                tls_packet_uint8(packet, rsa_sign);
            }
        }
        
        memcpy(message, context->remote_random, __TLS_CLIENT_RANDOM_SIZE);
        memcpy(message + __TLS_CLIENT_RANDOM_SIZE, context->local_random, __TLS_SERVER_RANDOM_SIZE);
        memcpy(message + __TLS_CLIENT_RANDOM_SIZE + __TLS_SERVER_RANDOM_SIZE, packet->buf + start_len, params_len);
#ifdef TLS_ECDSA_SUPPORTED
        if (tls_is_ecdsa(context)) {
            if (__private_tls_sign_ecdsa(context, hash_algorithm, message, message_len, out, &out_len) == 1) {
                DEBUG_PRINT("Signing OK! (ECDSA, length %i)\n", out_len);
                tls_packet_uint16(packet, out_len);
                tls_packet_append(packet, out, out_len);
            }
        } else
#endif
        if (__private_tls_sign_rsa(context, hash_algorithm, message, message_len, out, &out_len) == 1) {
            DEBUG_PRINT("Signing OK! (length %i)\n", out_len);
            tls_packet_uint16(packet, out_len);
            tls_packet_append(packet, out, out_len);
        }
        TLS_FREE(message);
    }
    if ((!packet->broken) && (packet->buf)) {
        int remaining = packet->len - start_len;
        int payload_pos = 6;
        if (context->dtls)
            payload_pos = 14;
        packet->buf[payload_pos] = remaining / 0x10000;
        remaining %= 0x10000;
        packet->buf[payload_pos + 1] = remaining / 0x100;
        remaining %= 0x100;
        packet->buf[payload_pos + 2] = remaining;
        if (context->dtls) {
            __private_dtls_handshake_copyframesize(context, packet);
            context->dtls_seq++;
        }
    }
    tls_packet_update(packet);
    return packet;
}

void __private_tls_set_session_id(struct TLSContext *context) {
    if (tls_random(context->session, __TLS_MAX_SESSION_ID))
        context->session_size = __TLS_MAX_SESSION_ID;
    else
        context->session_size = 0;
}

struct TLSPacket *tls_build_hello(struct TLSContext *context) {
    if (!tls_random(context->local_random, __TLS_SERVER_RANDOM_SIZE))
        return NULL;
    
    unsigned short packet_version = context->version;
    unsigned short version = context->version;
    struct TLSPacket *packet = tls_create_packet(context, TLS_HANDSHAKE, packet_version, 0);
    if (packet) {
        // hello
        if (context->is_server)
            tls_packet_uint8(packet, 0x02);
        else
            tls_packet_uint8(packet, 0x01);
        unsigned char dummy[3];
        tls_packet_append(packet, dummy, 3);

        if (context->dtls)
            __private_dtls_handshake_data(context, packet, 0);

        int start_len = packet->len;
        tls_packet_uint16(packet, version);
        if (context->is_server)
            tls_packet_append(packet, context->local_random, __TLS_SERVER_RANDOM_SIZE);
        else
            tls_packet_append(packet, context->local_random, __TLS_CLIENT_RANDOM_SIZE);

#ifdef IGNORE_SESSION_ID
        // session size
        tls_packet_uint8(packet, 0);
#else
        __private_tls_set_session_id(context);
        // session size
        tls_packet_uint8(packet, context->session_size);
        if (context->session_size)
            tls_packet_append(packet, context->session, context->session_size);
#endif

        int extension_len = 0;
        int alpn_len = 0;
        int alpn_negotiated_len = 0;
        int i;
        if ((context->is_server) && (context->negotiated_alpn)) {
            alpn_negotiated_len = strlen(context->negotiated_alpn);
            alpn_len = alpn_negotiated_len + 1;
            extension_len += alpn_len + 6;
        } else
        if ((!context->is_server) && (context->alpn_count)) {
            for (i = 0; i < context->alpn_count;i++) {
                if (context->alpn[i]) {
                    int len = strlen(context->alpn[i]);
                    if (len)
                        alpn_len += len + 1;
                }
            }
            if (alpn_len)
                extension_len += alpn_len + 6;
        }

        // ciphers
        if (context->is_server) {
            // fallback ... this should never happen
            if (!context->cipher)
                context->cipher = TLS_RSA_WITH_AES_128_CBC_SHA;
            
            tls_packet_uint16(packet, context->cipher);
            // no compression
            tls_packet_uint8(packet, 0);
#ifndef STRICT_TLS
            if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
                // extensions size
                tls_packet_uint16(packet, 5 + extension_len);
                // secure renegotation
                // advertise it, but refuse renegotiation
                tls_packet_uint16(packet, 0xff01);
#ifdef TLS_ACCEPT_SECURE_RENEGOTIATION
                // a little defensive
                if ((context->verify_len) && (!context->verify_data))
                        context->verify_len = 0;
                tls_packet_uint16(packet, context->verify_len + 1);
                tls_packet_uint8(packet, context->verify_len);
                if (context->verify_len)
                    tls_packet_append(packet, (unsigned char *)context->verify_data, context->verify_len);
#else
                tls_packet_uint16(packet, 1);
                tls_packet_uint8(packet, 0);
#endif
                if (alpn_len) {
                    tls_packet_uint16(packet, 0x10);
                    tls_packet_uint16(packet, alpn_len + 2);
                    tls_packet_uint16(packet, alpn_len);

                    tls_packet_uint8(packet, alpn_negotiated_len);
                    tls_packet_append(packet, (unsigned char *)context->negotiated_alpn, alpn_negotiated_len);
                }
            }
#endif
        } else {
            if (context->dtls) {
                tls_packet_uint8(packet, context->dtls_cookie_len);
                if (context->dtls_cookie_len)
                    tls_packet_append(packet, context->dtls_cookie, context->dtls_cookie_len);
            }

#ifndef STRICT_TLS
            if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
#endif
#ifdef TLS_FORWARD_SECRECY
#ifdef TLS_CLIENT_ECDHE
#ifdef TLS_WITH_CHACHA20_POLY1305
    #ifdef TLS_CLIENT_ECDSA
                tls_packet_uint16(packet, 42);
    #ifdef TLS_PREFER_CHACHA20
                tls_packet_uint16(packet, TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
    #endif
                tls_packet_uint16(packet, TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
    #ifndef TLS_PREFER_CHACHA20
                tls_packet_uint16(packet, TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256);
    #endif
                tls_packet_uint16(packet, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256);
                tls_packet_uint16(packet, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA);
                tls_packet_uint16(packet, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
    #else
                // sizeof ciphers (16 ciphers * 2 bytes)
                tls_packet_uint16(packet, 32);
    #endif
#else
    #ifdef TLS_CLIENT_ECDSA
                tls_packet_uint16(packet, 36);
                tls_packet_uint16(packet, TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
                tls_packet_uint16(packet, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256);
                tls_packet_uint16(packet, TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA);
                tls_packet_uint16(packet, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
    #else
                // sizeof ciphers (14 ciphers * 2 bytes)
                tls_packet_uint16(packet, 28);
    #endif
#endif
#ifdef TLS_WITH_CHACHA20_POLY1305
                #ifdef TLS_PREFER_CHACHA20
                    tls_packet_uint16(packet, TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
                #endif
#endif
                tls_packet_uint16(packet, TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256);
                tls_packet_uint16(packet, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
                tls_packet_uint16(packet, TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);
                tls_packet_uint16(packet, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256);
#ifdef TLS_WITH_CHACHA20_POLY1305
                #ifndef TLS_PREFER_CHACHA20
                    tls_packet_uint16(packet, TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
                #endif
#endif
#else
#ifdef TLS_WITH_CHACHA20_POLY1305
                // sizeof ciphers (11 ciphers * 2 bytes)
                tls_packet_uint16(packet, 22);
#else
                // sizeof ciphers (10 ciphers * 2 bytes)
                tls_packet_uint16(packet, 20);
#endif
#endif
                // not yet supported, because the first message sent (this one)
                // is already hashed by the client with sha256 (sha384 not yet supported client-side)
                // but is fully suported server-side
                // tls_packet_uint16(packet, TLS_DHE_RSA_WITH_AES_256_GCM_SHA384);
                tls_packet_uint16(packet, TLS_DHE_RSA_WITH_AES_128_GCM_SHA256);
                tls_packet_uint16(packet, TLS_DHE_RSA_WITH_AES_256_CBC_SHA256);
                tls_packet_uint16(packet, TLS_DHE_RSA_WITH_AES_128_CBC_SHA256);
                tls_packet_uint16(packet, TLS_DHE_RSA_WITH_AES_256_CBC_SHA);
                tls_packet_uint16(packet, TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
#ifdef TLS_WITH_CHACHA20_POLY1305
                tls_packet_uint16(packet, TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256);
#endif
#else
                tls_packet_uint16(packet, 10);
#endif
                // tls_packet_uint16(packet, TLS_RSA_WITH_AES_256_GCM_SHA384);
                tls_packet_uint16(packet, TLS_RSA_WITH_AES_128_GCM_SHA256);
                tls_packet_uint16(packet, TLS_RSA_WITH_AES_256_CBC_SHA256);
                tls_packet_uint16(packet, TLS_RSA_WITH_AES_128_CBC_SHA256);
                tls_packet_uint16(packet, TLS_RSA_WITH_AES_256_CBC_SHA);
                tls_packet_uint16(packet, TLS_RSA_WITH_AES_128_CBC_SHA);
#ifndef STRICT_TLS
            } else {
#ifdef TLS_FORWARD_SECRECY
#ifdef TLS_CLIENT_ECDHE
                tls_packet_uint16(packet, 14);
                tls_packet_uint16(packet, TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA);
                tls_packet_uint16(packet, TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA);
#else
                tls_packet_uint16(packet, 10);
#endif
                tls_packet_uint16(packet, TLS_DHE_RSA_WITH_AES_256_CBC_SHA);
                tls_packet_uint16(packet, TLS_DHE_RSA_WITH_AES_256_CBC_SHA);
                tls_packet_uint16(packet, TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
#else
                tls_packet_uint16(packet, 4);
#endif
                tls_packet_uint16(packet, TLS_RSA_WITH_AES_256_CBC_SHA);
                tls_packet_uint16(packet, TLS_RSA_WITH_AES_128_CBC_SHA);
            }
#endif
            // compression
            tls_packet_uint8(packet, 1);
            // no compression
            tls_packet_uint8(packet, 0);
            if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
                int sni_len = 0;
                if (context->sni)
                    sni_len = strlen(context->sni);
                
#ifdef TLS_CLIENT_ECDHE
                extension_len += 12;
#endif
                if (sni_len)
                    extension_len += sni_len + 9;
                
                tls_packet_uint16(packet, extension_len);
                
                if (sni_len) {
                    // sni extension
                    tls_packet_uint16(packet, 0x00);
                    // sni extension len
                    tls_packet_uint16(packet, sni_len + 5);
                    // sni len
                    tls_packet_uint16(packet, sni_len + 3);
                    // sni type
                    tls_packet_uint8(packet, 0);
                    // sni host len
                    tls_packet_uint16(packet, sni_len);
                    tls_packet_append(packet, (unsigned char *)context->sni, sni_len);
                }
#ifdef TLS_FORWARD_SECRECY
#ifdef TLS_CLIENT_ECDHE
                // supported groups
                tls_packet_uint16(packet, 0x0A);
                tls_packet_uint16(packet, 8);
                // 3 curves x 2 bytes
                tls_packet_uint16(packet, 6);
                tls_packet_uint16(packet, secp256r1.iana);
                tls_packet_uint16(packet, secp384r1.iana);
                tls_packet_uint16(packet, secp224r1.iana);
#endif
#endif
                if (alpn_len) {
                    tls_packet_uint16(packet, 0x10);
                    tls_packet_uint16(packet, alpn_len + 2);
                    tls_packet_uint16(packet, alpn_len);

                    for (i = 0; i < context->alpn_count;i++) {
                        if (context->alpn[i]) {
                            int len = strlen(context->alpn[i]);
                            if (len) {
                                tls_packet_uint8(packet, len);
                                tls_packet_append(packet, (unsigned char *)context->alpn[i], len);
                            }
                        }
                    }
                }
            }
        }
        
        if ((!packet->broken) && (packet->buf)) {
            int remaining = packet->len - start_len;
            int payload_pos = 6;
            if (context->dtls)
                payload_pos = 14;
            packet->buf[payload_pos] = remaining / 0x10000;
            remaining %= 0x10000;
            packet->buf[payload_pos + 1] = remaining / 0x100;
            remaining %= 0x100;
            packet->buf[payload_pos + 2] = remaining;
            if (context->dtls) {
                __private_dtls_handshake_copyframesize(context, packet);
                context->dtls_seq++;
            }
        }
        tls_packet_update(packet);
    }
    return packet;
}

struct TLSPacket *tls_certificate_request(struct TLSContext *context) {
    if ((!context) || (!context->is_server))
        return NULL;
    
    unsigned short packet_version = context->version;
    struct TLSPacket *packet = tls_create_packet(context, TLS_HANDSHAKE, packet_version, 0);
    if (packet) {
        // certificate request
        tls_packet_uint8(packet, 0x0D);
        unsigned char dummy[3];
        tls_packet_append(packet, dummy, 3);
        if (context->dtls)
            __private_dtls_handshake_data(context, packet, 0);
        int start_len = packet->len;
        tls_packet_uint8(packet, 1);
        tls_packet_uint8(packet, rsa_sign);
        if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
            // 10 pairs or 2 bytes
            tls_packet_uint16(packet, 10);
            tls_packet_uint8(packet, sha256);
            tls_packet_uint8(packet, rsa);
            tls_packet_uint8(packet, sha1);
            tls_packet_uint8(packet, rsa);
            tls_packet_uint8(packet, sha384);
            tls_packet_uint8(packet, rsa);
            tls_packet_uint8(packet, sha512);
            tls_packet_uint8(packet, rsa);
            tls_packet_uint8(packet, md5);
            tls_packet_uint8(packet, rsa);
        }
        // no DistinguishedName yet
        tls_packet_uint16(packet, 0);
        if ((!packet->broken) && (packet->buf)) {
            int remaining = packet->len - start_len;
            int payload_pos = 6;
            if (context->dtls)
                payload_pos = 14;
            packet->buf[payload_pos] = remaining / 0x10000;
            remaining %= 0x10000;
            packet->buf[payload_pos + 1] = remaining / 0x100;
            remaining %= 0x100;
            packet->buf[payload_pos + 2] = remaining;

            if (context->dtls) {
                __private_dtls_handshake_copyframesize(context, packet);
                context->dtls_seq++;
            }
        }
        tls_packet_update(packet);
    }
    return packet;
}

struct TLSPacket *tls_build_verify_request(struct TLSContext *context) {
    if ((!context->is_server) || (!context->dtls))
        return NULL;
    
    if ((!context->dtls_cookie) || (!context->dtls_cookie_len)) {
        context->dtls_cookie = (unsigned char *)TLS_MALLOC(__DTLS_COOKIE_SIZE);
        if (!context->dtls_cookie)
            return NULL;
        
        if (!tls_random(context->dtls_cookie, __DTLS_COOKIE_SIZE)) {
            TLS_FREE(context->dtls_cookie);
            context->dtls_cookie = NULL;
            return NULL;
        }
        context->dtls_cookie_len = __DTLS_COOKIE_SIZE;
    }

    unsigned short packet_version = context->version;
    struct TLSPacket *packet = tls_create_packet(context, TLS_HANDSHAKE, packet_version, 0);
    if (packet) {
        // verify request
        tls_packet_uint8(packet, 0x03);
        // 24-bit length
        tls_packet_uint24(packet, context->dtls_cookie_len + 3);
        // 16-bit message_sequence
        tls_packet_uint16(packet, 0);
        // 24-bit fragment_offset
        tls_packet_uint24(packet, 0);
        // 24-bit fragment_offset
        tls_packet_uint24(packet, context->dtls_cookie_len + 3);
        // server_version
        tls_packet_uint16(packet, context->version);
        tls_packet_uint8(packet, context->dtls_cookie_len);
        tls_packet_append(packet, context->dtls_cookie, context->dtls_cookie_len);
        tls_packet_update(packet);
    }
    return packet;
}

int __private_dtls_check_packet(const unsigned char *buf, int buf_len) {
    CHECK_SIZE(11, buf_len, TLS_NEED_MORE_DATA)

    unsigned int bytes_to_follow = buf[0] * 0x10000 + buf[1] * 0x100 + buf[2];
    // not used: unsigned short message_seq = ntohs(*(unsigned short *)&buf[3]);
    unsigned int fragment_offset = buf[5] * 0x10000 + buf[6] * 0x100 + buf[7];
    unsigned int fragment_length = buf[8] * 0x10000 + buf[9] * 0x100 + buf[10];

    if ((fragment_offset) || (fragment_length != bytes_to_follow)) {
        DEBUG_PRINT("FRAGMENTED PACKETS NOT SUPPORTED\n");
        return TLS_FEATURE_NOT_SUPPORTED;
    }
    return bytes_to_follow;
}

void __private_dtls_reset(struct TLSContext *context) {
    context->dtls_epoch_local = 0;
    context->dtls_epoch_remote = 0;
    context->dtls_seq = 0;
    __private_tls_destroy_hash(context);
    context->connection_status = 0;
}

int tls_parse_verify_request(struct TLSContext *context, const unsigned char *buf, int buf_len, unsigned int *write_packets) {
    *write_packets = 0;
    if ((context->connection_status != 0) || (!context->dtls)) {
        DEBUG_PRINT("UNEXPECTED VERIFY REQUEST MESSAGE\n");
        return TLS_UNEXPECTED_MESSAGE;
    }
    int res = 11;
    int bytes_to_follow = __private_dtls_check_packet(buf, buf_len);
    if (bytes_to_follow < 0)
        return bytes_to_follow;

    CHECK_SIZE(bytes_to_follow, buf_len - res, TLS_NEED_MORE_DATA)
    // not used: unsigned short version = ntohs(*(unsigned short *)&buf[res]);
    res += 2;
    unsigned char len = buf[res];
    res++;
    TLS_FREE(context->dtls_cookie);
    context->dtls_cookie_len = 0;
    if (len) {
        CHECK_SIZE(len, buf_len - res, TLS_NEED_MORE_DATA)
        context->dtls_cookie = (unsigned char *)TLS_MALLOC(len);
        if (!context->dtls_cookie)
            return TLS_NO_MEMORY;
        context->dtls_cookie_len = len;
        memcpy(context->dtls_cookie, &buf[res], len);
        res += len;
        *write_packets = 4;
    }

    // reset context
    __private_dtls_reset(context);
    return res;
}

void __private_dtls_reset_cookie(struct TLSContext *context) {
    TLS_FREE(context->dtls_cookie);
    context->dtls_cookie = NULL;
    context->dtls_cookie_len = 0;
}

int tls_parse_hello(struct TLSContext *context, const unsigned char *buf, int buf_len, unsigned int *write_packets, unsigned int *dtls_verified) {
    *write_packets = 0;
    *dtls_verified = 0;
    if (context->connection_status != 0) {
        // ignore multiple hello on dtls
        if (context->dtls) {
            DEBUG_PRINT("RETRANSMITTED HELLO MESSAGE RECEIVED\n");
            return 1;
        }
        DEBUG_PRINT("UNEXPECTED HELLO MESSAGE\n");
        return TLS_UNEXPECTED_MESSAGE;
    }
    
    int res = 0;
    int downgraded = 0;
    int hello_min_size = context->dtls ? __TLS_CLIENT_HELLO_MINSIZE + 8 : __TLS_CLIENT_HELLO_MINSIZE;
    CHECK_SIZE(hello_min_size, buf_len, TLS_NEED_MORE_DATA)
    // big endian
    unsigned int bytes_to_follow = buf[0] * 0x10000 + buf[1] * 0x100 + buf[2];
    res += 3;
    if (context->dtls) {
        int dtls_check = __private_dtls_check_packet(buf, buf_len);
        if (dtls_check < 0)
            return dtls_check;
        // 16 bit message seq + 24 bit fragment offset + 24 bit fragment length
        res += 8;
    }
    CHECK_SIZE(bytes_to_follow, buf_len - res, TLS_NEED_MORE_DATA)
    
    CHECK_SIZE(2, buf_len - res, TLS_NEED_MORE_DATA)
    unsigned short version = ntohs(*(unsigned short *)&buf[res]);
    
    res += 2;
    VERSION_SUPPORTED(version, TLS_NOT_SAFE)
    DEBUG_PRINT("VERSION REQUIRED BY REMOTE %x, VERSION NOW %x\n", (int)version, (int)context->version);
#ifdef TLS_LEGACY_SUPPORT
    // when no legacy support, don't downgrade
#ifndef TLS_FORCE_LOCAL_VERSION
    // downgrade ?
    if (context->dtls) {
        // for dlts, newer version has lower id (1.0 = FEFF, 1.2 = FEFD)
        if (context->version < version)
            downgraded = 1;
    } else {
        if (context->version > version)
            downgraded = 1;
    }
    if (downgraded) {
        context->version = version;
        if (!context->is_server)
            __private_tls_change_hash_type(context);
    }
#endif
#endif
    memcpy(context->remote_random, &buf[res], __TLS_CLIENT_RANDOM_SIZE);
    res += __TLS_CLIENT_RANDOM_SIZE;
    
    unsigned char session_len = buf[res++];
    CHECK_SIZE(session_len, buf_len - res, TLS_NEED_MORE_DATA)
    if ((session_len) && (session_len <= __TLS_MAX_SESSION_ID)) {
        memcpy(context->session, &buf[res], session_len);
        context->session_size = session_len;
    } else
        context->session_size = 0;

    res += session_len;

    if (context->is_server) {
        if (context->dtls) {
            CHECK_SIZE(1, buf_len - res, TLS_NEED_MORE_DATA)
            unsigned char tls_cookie_len = buf[res++];
            if (tls_cookie_len) {
                CHECK_SIZE(tls_cookie_len, buf_len - res, TLS_NEED_MORE_DATA)
                if ((context->dtls_cookie_len != tls_cookie_len) || (!context->dtls_cookie)) {
                    *dtls_verified = 2;
                    __private_dtls_reset_cookie(context);
                    DEBUG_PRINT("INVALID DTLS COOKIE\n");
                    return TLS_BROKEN_PACKET;
                }
                if (memcmp(context->dtls_cookie, &buf[res], tls_cookie_len)) {
                    *dtls_verified = 3;
                    __private_dtls_reset_cookie(context);
                    DEBUG_PRINT("MISMATCH DTLS COOKIE\n");
                    return TLS_BROKEN_PACKET;
                }
                __private_dtls_reset_cookie(context);
                context->dtls_seq++;
                *dtls_verified = 1;
                res += tls_cookie_len;
            } else {
                *write_packets = 2;
                return buf_len;
            }
        }
        CHECK_SIZE(2, buf_len - res, TLS_NEED_MORE_DATA)
        unsigned short cipher_len = ntohs(*(unsigned short *)&buf[res]);
        res += 2;
        CHECK_SIZE(cipher_len, buf_len - res, TLS_NEED_MORE_DATA)
        // faster than cipher_len % 2
        if (cipher_len & 1)
            return TLS_BROKEN_PACKET;
        
        int scsv_set = 0;
        int cipher = tls_choose_cipher(context, &buf[res], cipher_len, &scsv_set);
        if (cipher < 0) {
            DEBUG_PRINT("NO COMMON CIPHERS\n");
            return cipher;
        }
        if ((downgraded) && (scsv_set)) {
            DEBUG_PRINT("NO DOWNGRADE (SCSV SET)\n");
            __private_tls_write_packet(tls_build_alert(context, 1, inappropriate_fallback));
            context->critical_error = 1;
            return TLS_NOT_SAFE;
        }
        context->cipher = cipher;
        res += cipher_len;
        
        CHECK_SIZE(1, buf_len - res, TLS_NEED_MORE_DATA)
        unsigned char compression_list_size = buf[res++];
        CHECK_SIZE(compression_list_size, buf_len - res, TLS_NEED_MORE_DATA)
        
        // no compression support
        res += compression_list_size;
    } else {
        CHECK_SIZE(2, buf_len - res, TLS_NEED_MORE_DATA)
        unsigned short cipher = ntohs(*(unsigned short *)&buf[res]);
        res += 2;
        context->cipher = cipher;
        if (!tls_cipher_supported(context, cipher)) {
            context->cipher = 0;
            DEBUG_PRINT("NO CIPHER SUPPORTED\n");
            return TLS_NO_COMMON_CIPHER;
        }
        DEBUG_PRINT("CIPHER: %s\n", tls_cipher_name(context));
        CHECK_SIZE(1, buf_len - res, TLS_NEED_MORE_DATA)
        unsigned char compression = buf[res++];
        if (compression != 0) {
            DEBUG_PRINT("COMPRESSION NOT SUPPORTED\n");
            return TLS_COMPRESSION_NOT_SUPPORTED;
        }
    }
    
    if (res > 0) {
        if (context->is_server)
            *write_packets = 2;
        context->connection_status = 1;
    }
    
    
    unsigned short extensions_size = 0;
    if (res > 2) {
        extensions_size = ntohs(*(unsigned short *)&buf[res]);
        res += 2;
    }
    // ignore extensions for now
    while (buf_len - res >= 4) {
        // have extensions
        unsigned short extension_type = ntohs(*(unsigned short *)&buf[res]);
        res += 2;
        unsigned short extension_len = ntohs(*(unsigned short *)&buf[res]);
        res += 2;
        DEBUG_PRINT("Extension: 0x0%x (%i), len: %i\n", (int)extension_type, (int)extension_type, (int)extension_len);
        if (extension_len) {
            // SNI extension
            CHECK_SIZE(extension_len, buf_len - res, TLS_NEED_MORE_DATA)
            if (extension_type == 0x00) {
                // unsigned short sni_len = ntohs(*(unsigned short *)&buf[res]);
                // unsigned char sni_type = buf[res + 2];
                unsigned short sni_host_len = ntohs(*(unsigned short *)&buf[res + 3]);
                CHECK_SIZE(sni_host_len, buf_len - res - 5, TLS_NEED_MORE_DATA)
                if (sni_host_len) {
                    TLS_FREE(context->sni);
                    context->sni = (char *)TLS_MALLOC(sni_host_len + 1);
                    if (context->sni) {
                        memcpy(context->sni, &buf[res + 5], sni_host_len);
                        context->sni[sni_host_len] = 0;
                        DEBUG_PRINT("SNI HOST INDICATOR: [%s]\n", context->sni);
                    }
                }
            } else
#ifdef TLS_FORWARD_SECRECY
            if (extension_type == 0x0A) {
                // supported groups
                if (buf_len - res > 2) {
                    unsigned short group_len = ntohs(*(unsigned short *)&buf[res]);
                    if (buf_len - res >= group_len + 2) {
                        DEBUG_DUMP_HEX_LABEL("SUPPORTED GROUPS", &buf[res + 2], group_len);
                        int i;
                        int selected = 0;
                        for (i = 0; i < group_len; i += 2) {
                            unsigned short iana_n = ntohs(*(unsigned short *)&buf[res + 2 + i]);
                            switch (iana_n) {
                                case 23:
                                    context->curve = &secp256r1;
                                    selected = 1;
                                    break;
                                case 24:
                                    context->curve = &secp384r1;
                                    selected = 1;
                                    break;
                                // do not use it anymore
                                // case 25:
                                //    context->curve = &secp521r1;
                                //    selected = 1;
                                //    break;
                            }
                            if (selected) {
                                DEBUG_PRINT("SELECTED CURVE %s\n", context->curve->name);
                                break;
                            }
                        }
                    }
                }
            } else
#endif
            if ((extension_type == 0x10) && (context->alpn) && (context->alpn_count)) {
                if (buf_len - res > 2) {
                    unsigned short alpn_len = ntohs(*(unsigned short *)&buf[res]);
                    if ((alpn_len) && (alpn_len <= extension_len - 2)) {
                        unsigned char *alpn = (unsigned char *)&buf[res + 2];
                        int alpn_pos = 0;
                        while (alpn_pos < alpn_len) {
                            unsigned char alpn_size = alpn[alpn_pos++];
                            if (alpn_size + alpn_pos >= extension_len)
                                break;
                            if ((alpn_size) && (tls_alpn_contains(context, (char *)&alpn[alpn_pos], alpn_size))) {
                                TLS_FREE(context->negotiated_alpn);
                                context->negotiated_alpn = (char *)TLS_MALLOC(alpn_size + 1);
                                if (context->negotiated_alpn) {
                                    memcpy(context->negotiated_alpn, &alpn[alpn_pos], alpn_size);
                                    context->negotiated_alpn[alpn_size] = 0;
                                    DEBUG_PRINT("NEGOTIATED ALPN: %s\n", context->negotiated_alpn);
                                }
                                break;
                            }
                            alpn_pos += alpn_size;
                            // ServerHello contains just one alpn
                            if (!context->is_server)
                                break;
                        }
                    }
                }
            } else
            if (extension_type == 0x0D) {
                // supported signatures
                DEBUG_DUMP_HEX_LABEL("SUPPORTED SIGNATURES", &buf[res], extension_len);
            } else
            if (extension_type == 0x0B) {
                // supported point formats
                DEBUG_DUMP_HEX_LABEL("SUPPORTED POINT FORMATS", &buf[res], extension_len);
            }
            res += extension_len;
        }
    }
    if (buf_len != res)
        return TLS_NEED_MORE_DATA;
    return res;
}

int tls_parse_certificate(struct TLSContext *context, const unsigned char *buf, int buf_len, int is_client) {
    int res = 0;
    CHECK_SIZE(3, buf_len, TLS_NEED_MORE_DATA)
    unsigned int size_of_all_certificates = buf[0] * 0x10000 + buf[1] * 0x100 + buf[2];
    
    if (size_of_all_certificates <= 4)
        return 3 + size_of_all_certificates;
    res += 3;

    if (context->dtls) {
        int dtls_check = __private_dtls_check_packet(buf, buf_len);
        if (dtls_check < 0)
            return dtls_check;
        res += 8;
    }

    CHECK_SIZE(size_of_all_certificates, buf_len - res, TLS_NEED_MORE_DATA);
    int size = size_of_all_certificates;
    
    int idx = 0;
    int valid_certificate = 0;
    while (size > 0) {
        idx++;
        CHECK_SIZE(3, buf_len - res, TLS_NEED_MORE_DATA);
        unsigned int certificate_size = buf[res] * 0x10000 + buf[res + 1] * 0x100 + buf[res + 2];
        res += 3;
        CHECK_SIZE(certificate_size, buf_len - res, TLS_NEED_MORE_DATA)
        // load chain
        int certificates_in_chain = 0;
        int res2 = res;
        unsigned int remaining = certificate_size;
        do {
            if (remaining <= 3)
                break;
            certificates_in_chain++;
            unsigned int certificate_size2 = buf[res2] * 0x10000 + buf[res2 + 1] * 0x100 + buf[res2 + 2];
            res2 += 3;
            remaining -= 3;
            if (certificate_size2 > remaining) {
                DEBUG_PRINT("Invalid certificate size (%i from %i bytes remaining)\n", certificate_size2, remaining);
                break;
            }
            remaining -= certificate_size2;
            
            struct TLSCertificate *cert = asn1_parse(context, &buf[res2], certificate_size2, is_client);
            if (cert) {
                if (certificate_size2) {
                    cert->bytes = (unsigned char *)TLS_MALLOC(certificate_size2);
                    if (cert->bytes) {
                        cert->len = certificate_size2;
                        memcpy(cert->bytes, &buf[res2], certificate_size2);
                    }
                }
                // valid certificate
                if (is_client) {
                    valid_certificate = 1;
                    context->client_certificates = (struct TLSCertificate **)TLS_REALLOC(context->client_certificates, (context->client_certificates_count + 1) * sizeof(struct TLSCertificate));
                    context->client_certificates[context->client_certificates_count] = cert;
                    context->client_certificates_count++;
                } else {
                    context->certificates = (struct TLSCertificate **)TLS_REALLOC(context->certificates, (context->certificates_count + 1) * sizeof(struct TLSCertificate));
                    context->certificates[context->certificates_count] = cert;
                    context->certificates_count++;
                    if ((cert->pk) || (cert->priv))
                        valid_certificate = 1;
                }
            }
            res2 += certificate_size2;
        } while (remaining > 0);
        if (remaining)
            DEBUG_PRINT("Extra %i bytes after certificate\n", remaining);
        size -= certificate_size + 3;
        res += certificate_size;
    }
    if (!valid_certificate)
        return TLS_UNSUPPORTED_CERTIFICATE;
    if (res != buf_len) {
        DEBUG_PRINT("Warning: %i bytes read from %i byte buffer", (int)res, (int)buf_len);
    }
    return res;
}

int __private_tls_parse_dh(struct TLSContext *context, const unsigned char *buf, int buf_len, const unsigned char **out, int *out_size) {
    int res = 0;
    *out = NULL;
    *out_size = 0;
    CHECK_SIZE(2, buf_len, TLS_NEED_MORE_DATA)
    unsigned short size = ntohs(*(unsigned short *)buf);
    res += 2;
    CHECK_SIZE(size, buf_len - res, TLS_NEED_MORE_DATA)
    DEBUG_DUMP_HEX(&buf[res], size);
    *out = &buf[res];
    *out_size = size;
    res += size;
    return res;
}

int __private_tls_parse_random(struct TLSContext *context, const unsigned char *buf, int buf_len) {
    int res = 0;
    int ephemeral = tls_cipher_is_ephemeral(context);
    unsigned short size;
    if (ephemeral == 2) {
        CHECK_SIZE(1, buf_len, TLS_NEED_MORE_DATA)
        size = buf[0];
        res += 1;
    } else {
        CHECK_SIZE(2, buf_len, TLS_NEED_MORE_DATA)
        size = ntohs(*(unsigned short *)buf);
        res += 2;
    }
    
    CHECK_SIZE(size, buf_len - res, TLS_NEED_MORE_DATA)
    unsigned int out_len = 0;
    unsigned char *random = NULL;
    switch (ephemeral) {
#ifdef TLS_FORWARD_SECRECY
        case 1:
            random = __private_tls_decrypt_dhe(context, &buf[res], size, &out_len, 1);
            break;
        case 2:
            random = __private_tls_decrypt_ecc_dhe(context, &buf[res], size, &out_len, 1);
            break;
#endif
        default:
            random = __private_tls_decrypt_rsa(context, &buf[res], size, &out_len);
    }
    
    if ((random) && (out_len > 2)) {
        // *(unsigned short *)&random[0] = htons(context->version);
        DEBUG_DUMP_HEX_LABEL("PRE MASTER KEY", random, out_len);
        TLS_FREE(context->premaster_key);
        context->premaster_key = random;
        context->premaster_key_len = out_len;
        __private_tls_compute_key(context, 48);
    } else {
        TLS_FREE(random);
        return 0;
    }
    res += size;
    return res;
}

int __private_tls_build_random(struct TLSPacket *packet) {
    int res = 0;
    unsigned char rand_bytes[48];
    int bytes = 48;
    if (!tls_random(rand_bytes, bytes))
        return TLS_GENERIC_ERROR;
    
    *(unsigned short *)&rand_bytes[0] = htons(packet->context->version);
    //DEBUG_DUMP_HEX_LABEL("PREMASTER KEY", rand_bytes, bytes);
    
    TLS_FREE(packet->context->premaster_key);
    packet->context->premaster_key = (unsigned char *)TLS_MALLOC(bytes);
    if (!packet->context->premaster_key)
        return TLS_NO_MEMORY;
    
    packet->context->premaster_key_len = bytes;
    memcpy(packet->context->premaster_key, rand_bytes, packet->context->premaster_key_len);
    
    unsigned int out_len;
    unsigned char *random = __private_tls_encrypt_rsa(packet->context, packet->context->premaster_key, packet->context->premaster_key_len, &out_len);
    
    __private_tls_compute_key(packet->context, bytes);
    if ((random) && (out_len > 2)) {
        tls_packet_uint24(packet, out_len + 2);
        if (packet->context->dtls)
            __private_dtls_handshake_data(packet->context, packet, out_len + 2);
        tls_packet_uint16(packet, out_len);
        tls_packet_append(packet, random, out_len);
    } else
        res = TLS_GENERIC_ERROR;
    TLS_FREE(random);
    if (res)
        return res;
    
    return out_len + 2;
}

const unsigned char *__private_tls_parse_signature(struct TLSContext *context, const unsigned char *buf, int buf_len, int *hash_algorithm, int *sign_algorithm, int *sig_size, int *offset) {
    int res = 0;
    CHECK_SIZE(2, buf_len, NULL)
    *hash_algorithm = __md5_sha1;
    *sign_algorithm = rsa_sign;
    *sig_size = 0;
    if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
        *hash_algorithm = buf[res];
        res++;
        *sign_algorithm = buf[res];
        res++;
    }
    unsigned short size = ntohs(*(unsigned short *)&buf[res]);
    res += 2;
    CHECK_SIZE(size, buf_len - res, NULL)
    DEBUG_DUMP_HEX(&buf[res], size);
    *sig_size = size;
    *offset = res + size;
    return &buf[res];
}

int tls_parse_server_key_exchange(struct TLSContext *context, const unsigned char *buf, int buf_len) {
    int res = 0;
    int dh_res = 0;
    CHECK_SIZE(3, buf_len, TLS_NEED_MORE_DATA)
    unsigned int size = buf[0] * 0x10000 + buf[1] * 0x100 + buf[2];
    res += 3;
    if (context->dtls) {
        int dtls_check = __private_dtls_check_packet(buf, buf_len);
        if (dtls_check < 0)
            return dtls_check;
        res += 8;
    }
    const unsigned char *packet_ref = buf + res;
    CHECK_SIZE(size, buf_len - res, TLS_NEED_MORE_DATA);
    
    if (!size)
        return res;
    
    unsigned char has_ds_params = 0;
#ifdef TLS_FORWARD_SECRECY
    const struct ECCCurveParameters *curve = NULL;
    const unsigned char *pk_key = NULL;
    unsigned int key_size = 0;
    int ephemeral = tls_cipher_is_ephemeral(context);
    if (ephemeral) {
        if (ephemeral == 1) {
            has_ds_params = 1;
        } else {
            if (buf[res++] != 3) {
                // named curve
                // any other method is not supported
                return 0;
            }
            CHECK_SIZE(3, buf_len - res, TLS_NEED_MORE_DATA);
            int iana_n = ntohs(*(unsigned short *)&buf[res]);
            res += 2;
            key_size = buf[res];
            res++;
            CHECK_SIZE(key_size, buf_len - res, TLS_NEED_MORE_DATA);
            DEBUG_PRINT("IANA CURVE NUMBER: %i\n", iana_n);
            switch (iana_n) {
                case 19:
                    curve = &secp192r1;
                    break;
                case 20:
                    curve = &secp224k1;
                    break;
                case 21:
                    curve = &secp224r1;
                    break;
                case 22:
                    curve = &secp256k1;
                    break;
                case 23:
                    curve = &secp256r1;
                    break;
                case 24:
                    curve = &secp384r1;
                    break;
                case 25:
                    curve = &secp521r1;
                    break;
                default:
                    DEBUG_PRINT("UNSUPPORTED CURVE");
                    return TLS_GENERIC_ERROR;
            }
            pk_key = &buf[res];
            res += key_size;
            context->curve = curve;
        }
    }
#endif
    const unsigned char *dh_p = NULL;
    int dh_p_len = 0;
    const unsigned char *dh_g = NULL;
    int dh_g_len = 0;
    const unsigned char *dh_Ys = NULL;
    int dh_Ys_len = 0;
    if (has_ds_params) {
        DEBUG_PRINT("          dh_p: ");
        dh_res = __private_tls_parse_dh(context, &buf[res], buf_len - res, &dh_p, &dh_p_len);
        if (dh_res <= 0)
            return TLS_BROKEN_PACKET;
        res += dh_res;
        DEBUG_PRINT("\n");
        
        DEBUG_PRINT("          dh_q: ");
        dh_res = __private_tls_parse_dh(context, &buf[res], buf_len - res, &dh_g, &dh_g_len);
        if (dh_res <= 0)
            return TLS_BROKEN_PACKET;
        res += dh_res;
        DEBUG_PRINT("\n");
        
        DEBUG_PRINT("          dh_Ys: ");
        dh_res = __private_tls_parse_dh(context, &buf[res], buf_len - res, &dh_Ys, &dh_Ys_len);
        if (dh_res <= 0)
            return TLS_BROKEN_PACKET;
        res += dh_res;
        DEBUG_PRINT("\n");
    }
    int sign_size;
    int hash_algorithm;
    int sign_algorithm;
    int packet_size = res - 3;
    if (context->dtls)
        packet_size -= 8;
    int offset = 0;
    DEBUG_PRINT("          SIGNATURE (%i/%i/%i): ", packet_size, dh_res, key_size);
    const unsigned char *signature = __private_tls_parse_signature(context, &buf[res], buf_len - res, &hash_algorithm, &sign_algorithm, &sign_size, &offset);
    DEBUG_PRINT("\n");
    if ((sign_size <= 0) || (!signature))
        return TLS_BROKEN_PACKET;
    res += offset;
    // check signature
    unsigned int message_len = packet_size + __TLS_CLIENT_RANDOM_SIZE + __TLS_SERVER_RANDOM_SIZE;
    unsigned char *message = (unsigned char *)TLS_MALLOC(message_len);
    if (message) {        
        memcpy(message, context->local_random, __TLS_CLIENT_RANDOM_SIZE);
        memcpy(message + __TLS_CLIENT_RANDOM_SIZE, context->remote_random, __TLS_SERVER_RANDOM_SIZE);
        memcpy(message + __TLS_CLIENT_RANDOM_SIZE + __TLS_SERVER_RANDOM_SIZE, packet_ref, packet_size);
#ifdef TLS_CLIENT_ECDSA
        if (tls_is_ecdsa(context)) {
            if (__private_tls_verify_ecdsa(context, hash_algorithm, signature, sign_size, message, message_len) != 1) {
                DEBUG_PRINT("ECC Server signature FAILED!\n");
                TLS_FREE(message);
                return TLS_BROKEN_PACKET;
            }
        } else 
#endif
        {
            if (__private_tls_verify_rsa(context, hash_algorithm, signature, sign_size, message, message_len) != 1) {
                DEBUG_PRINT("Server signature FAILED!\n");
                TLS_FREE(message);
                return TLS_BROKEN_PACKET;
            }
        }
        TLS_FREE(message);
    }
    
    if (buf_len - res) {
        DEBUG_PRINT("EXTRA %i BYTES AT THE END OF MESSAGE\n", buf_len - res);
        DEBUG_DUMP_HEX(&buf[res], buf_len - res);
        DEBUG_PRINT("\n");
    }
#ifdef TLS_FORWARD_SECRECY
    if (ephemeral == 1) {
        __private_tls_dhe_create(context);
        DEBUG_DUMP_HEX_LABEL("DHP", dh_p, dh_p_len);
        DEBUG_DUMP_HEX_LABEL("DHG", dh_g, dh_g_len);
        int dhe_key_size = dh_p_len;
        if (dh_g_len > dh_p_len)
            dhe_key_size = dh_g_len;
        if (__private_tls_dh_make_key(dhe_key_size, context->dhe, (const char *)dh_p, (const char *)dh_g, dh_p_len, dh_g_len)) {
            DEBUG_PRINT("ERROR CREATING DHE KEY\n");
            TLS_FREE(context->dhe);
            context->dhe = NULL;
            return TLS_GENERIC_ERROR;
        }
        
        unsigned int dh_key_size = 0;
        unsigned char *key = __private_tls_decrypt_dhe(context, dh_Ys, dh_Ys_len, &dh_key_size, 0);
        DEBUG_DUMP_HEX_LABEL("DH COMMON SECRET", key, dh_key_size);
        if ((key) && (dh_key_size)) {
            TLS_FREE(context->premaster_key);
            context->premaster_key = key;
            context->premaster_key_len = dh_key_size;
        }
    } else
    if ((ephemeral == 2) && (curve) && (pk_key) && (key_size)) {
        init_dependencies();
        __private_tls_ecc_dhe_create(context);
        
        ltc_ecc_set_type *dp = (ltc_ecc_set_type *)&curve->dp;
        if (ecc_make_key_ex(NULL, find_prng("sprng"), context->ecc_dhe, dp)) {
            TLS_FREE(context->ecc_dhe);
            context->ecc_dhe = NULL;
            DEBUG_PRINT("Error generatic ECC key\n");
            return TLS_GENERIC_ERROR;
        }
        
        TLS_FREE(context->premaster_key);
        context->premaster_key_len = 0;
        
        unsigned int out_len = 0;
        context->premaster_key = __private_tls_decrypt_ecc_dhe(context, pk_key, key_size, &out_len, 0);
        if (context->premaster_key)
            context->premaster_key_len = out_len;
    }
#endif
    return res;
}

int tls_parse_client_key_exchange(struct TLSContext *context, const unsigned char *buf, int buf_len) {
    if (context->connection_status != 1) {
        DEBUG_PRINT("UNEXPECTED CLIENT KEY EXCHANGE MESSAGE (connections status: %i)\n", (int)context->connection_status);
        return TLS_UNEXPECTED_MESSAGE;
    }

    int res = 0;
    int dh_res = 0;
    CHECK_SIZE(3, buf_len, TLS_NEED_MORE_DATA)
    
    unsigned int size = buf[0] * 0x10000 + buf[1] * 0x100 + buf[2];
    res += 3;
    if (context->dtls) {
        int dtls_check = __private_dtls_check_packet(buf, buf_len);
        if (dtls_check < 0)
            return dtls_check;
        res += 8;
    }

    CHECK_SIZE(size, buf_len - res, TLS_NEED_MORE_DATA);

    if (!size)
        return res;
    
    dh_res = __private_tls_parse_random(context, &buf[res], size);
    if (dh_res <= 0) {
        DEBUG_PRINT("broken key\n");
        return TLS_BROKEN_PACKET;
    }
    DEBUG_PRINT("\n");
    
    res += size;
    context->connection_status = 2;
    return res;
}

int tls_parse_server_hello_done(struct TLSContext *context, const unsigned char *buf, int buf_len) {
    int res = 0;
    CHECK_SIZE(3, buf_len, TLS_NEED_MORE_DATA)
    
    unsigned int size = buf[0] * 0x10000 + buf[1] * 0x100 + buf[2];
    res += 3;
    if (context->dtls) {
        int dtls_check = __private_dtls_check_packet(buf, buf_len);
        if (dtls_check < 0)
            return dtls_check;
        res += 8;
    }
    
    CHECK_SIZE(size, buf_len - res, TLS_NEED_MORE_DATA);
    
    res += size;
    return res;
}

int tls_parse_finished(struct TLSContext *context, const unsigned char *buf, int buf_len, unsigned int *write_packets) {
    if ((context->connection_status < 2) || (context->connection_status == 0xFF))  {
        DEBUG_PRINT("UNEXPECTED HELLO MESSAGE\n");
        return TLS_UNEXPECTED_MESSAGE;
    }
    
    int res = 0;
    *write_packets = 0;
    CHECK_SIZE(3, buf_len, TLS_NEED_MORE_DATA)
    
    unsigned int size = buf[0] * 0x10000 + buf[1] * 0x100 + buf[2];
    res += 3;
    if (context->dtls) {
        int dtls_check = __private_dtls_check_packet(buf, buf_len);
        if (dtls_check < 0)
            return dtls_check;
        res += 8;
    }
    
    if (size < __TLS_MIN_FINISHED_OPAQUE_LEN) {
        DEBUG_PRINT("Invalid finished pachet size: %i\n", size);
        return TLS_BROKEN_PACKET;
    }
    
    CHECK_SIZE(size, buf_len - res, TLS_NEED_MORE_DATA);
    
    // verify
    unsigned char *out = (unsigned char *)TLS_MALLOC(size);
    if (!out) {
        DEBUG_PRINT("Error in TLS_MALLOC (%i bytes)\n", (int)size);
        return TLS_NO_MEMORY;
    }
    
    unsigned char hash[__TLS_MAX_SHA_SIZE];
    unsigned int hash_len = __private_tls_get_hash(context, hash);
    // server verifies client's message
    if (context->is_server)
        __private_tls_prf(context, out, size, context->master_key, context->master_key_len, (unsigned char *)"client finished", 15, hash, hash_len, NULL, 0);
    else
        __private_tls_prf(context, out, size, context->master_key, context->master_key_len, (unsigned char *)"server finished", 15, hash, hash_len, NULL, 0);
    
    //unsigned char hash2[__TLS_HASH_SIZE];
    //hash_len = __private_tls_get_hash(context, hash2);
    //int x = memcmp(hash, hash2, __TLS_HASH_SIZE);
    //DEBUG_PRINT("MEMCMP RESULT: %i\n", x);
    if (memcmp(out, &buf[res], size)) {
        TLS_FREE(out);
        DEBUG_PRINT("Finished validation error (sequence number, local: %i, remote: %i)\n", (int)context->local_sequence_number, (int)context->remote_sequence_number);
        DEBUG_DUMP_HEX_LABEL("FINISHED OPAQUE", &buf[res], size);
        DEBUG_DUMP_HEX_LABEL("VERIFY", out, size);
        return TLS_NOT_VERIFIED;
    }
    TLS_FREE(out);
    if (context->is_server)
        *write_packets = 3;
    else
        context->connection_status = 0xFF;
#ifdef TLS_ACCEPT_SECURE_RENEGOTIATION
    if (size) {
        if (context->is_server) {
            TLS_FREE(context->verify_data);
            context->verify_data = (unsigned char *)TLS_MALLOC(size);
            if (context->verify_data) {
                memcpy(context->verify_data, out, size);
                context->verify_len = size;
            }
        } else {
            // concatenate client verify and server verify
            context->verify_data = (unsigned char *)TLS_REALLOC(context->verify_data, size);
            if (context->verify_data) {
                memcpy(context->verify_data + context->verify_len, out, size);
                context->verify_len += size;
            } else
                context->verify_len = 0;
        }
    }
#endif
    res += size;
    return res;
}

int tls_parse_verify(struct TLSContext *context, const unsigned char *buf, int buf_len) {
    CHECK_SIZE(7, buf_len, TLS_BAD_CERTIFICATE)
    unsigned int bytes_to_follow = buf[0] * 0x10000 + buf[1] * 0x100 + buf[2];
    CHECK_SIZE(bytes_to_follow, buf_len - 3, TLS_BAD_CERTIFICATE)
    int res = -1;

    if ((context->version == TLS_V12) || (context->version == DTLS_V12)) {
        unsigned int hash = buf[3];
        unsigned int algorithm = buf[4];
        if (algorithm != rsa)
            return TLS_UNSUPPORTED_CERTIFICATE;
        unsigned short size = ntohs(*(unsigned short *)&buf[5]);
        CHECK_SIZE(size, bytes_to_follow - 4, TLS_BAD_CERTIFICATE)
        DEBUG_PRINT("ALGORITHM %i/%i (%i)\n", hash, algorithm, (int)size);
        DEBUG_DUMP_HEX_LABEL("VERIFY", &buf[7], bytes_to_follow - 7);
        
        res = __private_tls_verify_rsa(context, hash, &buf[7], size, context->cached_handshake, context->cached_handshake_len);
    } else {
#ifdef TLS_LEGACY_SUPPORT
        unsigned short size = ntohs(*(unsigned short *)&buf[3]);
        CHECK_SIZE(size, bytes_to_follow - 2, TLS_BAD_CERTIFICATE)
        res = __private_tls_verify_rsa(context, md5, &buf[5], size, context->cached_handshake, context->cached_handshake_len);
#endif
    }
    if (context->cached_handshake) {
        // not needed anymore
        TLS_FREE(context->cached_handshake);
        context->cached_handshake = NULL;
        context->cached_handshake_len = 0;
    }
    if (res == 1) {
        DEBUG_PRINT("Signature OK\n");
        context->client_verified = 1;
    } else {
        DEBUG_PRINT("Signature FAILED\n");
        context->client_verified = 0;
    }
    return 1;
}

int tls_parse_payload(struct TLSContext *context, const unsigned char *buf, int buf_len, tls_validation_function certificate_verify) {
    int orig_len = buf_len;
    if (context->connection_status == 0xFF) {
#ifndef TLS_ACCEPT_SECURE_RENEGOTIATION
        // renegotiation disabled (emit warning alert)
        __private_tls_write_packet(tls_build_alert(context, 0, no_renegotiation));
        return 1;
#endif
    }

    while ((buf_len >= 4) && (!context->critical_error)) {
        int payload_res = 0;
        unsigned char update_hash = 1;
        CHECK_SIZE(1, buf_len, TLS_NEED_MORE_DATA)
        unsigned char type = buf[0];
        unsigned int write_packets = 0;
        unsigned int dtls_cookie_verified = 0;
        int certificate_verify_alert = no_error;
        unsigned int payload_size = buf[1] * 0x10000 + buf[2] * 0x100 + buf[3] + 3;
        if (context->dtls)
            payload_size += 8;
        CHECK_SIZE(payload_size + 1, buf_len, TLS_NEED_MORE_DATA)
        switch (type) {
                // hello request
            case 0x00:
                CHECK_HANDSHAKE_STATE(context, 0, 1);
                DEBUG_PRINT(" => HELLO REQUEST (RENEGOTIATION?)\n");
                if (context->dtls)
                    context->dtls_seq = 0;
                if (context->is_server)
                    payload_res = TLS_UNEXPECTED_MESSAGE;
                else {
                    if (context->connection_status == 0xFF) {
                        // renegotiation
#ifdef TLS_ACCEPT_SECURE_RENEGOTIATION
                        if (context->critical_error)
                            payload_res = TLS_UNEXPECTED_MESSAGE;
                        else {
                            __private_tls_reset_context(context);
                            __private_tls_write_packet(tls_build_hello(context));
                            return 1;
                        }
#else
                        payload_res = TLS_NO_RENEGOTIATION;
#endif
                    } else
                        payload_res = TLS_UNEXPECTED_MESSAGE;
                }
                // no payload
                break;
                // client hello
            case 0x01:
                CHECK_HANDSHAKE_STATE(context, 1, (context->dtls ? 2 : 1));
                DEBUG_PRINT(" => CLIENT HELLO\n");
                if (context->is_server) {
                    payload_res = tls_parse_hello(context, buf + 1, payload_size, &write_packets, &dtls_cookie_verified);
                    DEBUG_PRINT(" => DTLS COOKIE VERIFIED: %i (%i)\n", dtls_cookie_verified, payload_res);
                    if ((context->dtls) && (payload_res > 0) && (!dtls_cookie_verified) && (context->connection_status == 1)) {
                        // wait client hello
                        context->connection_status = 0;
                        update_hash = 0;
                    }
                } else
                    payload_res = TLS_UNEXPECTED_MESSAGE;
                break;
                // server hello
            case 0x02:
                CHECK_HANDSHAKE_STATE(context, 2, 1);
                DEBUG_PRINT(" => SERVER HELLO\n");
                if (context->is_server)
                    payload_res = TLS_UNEXPECTED_MESSAGE;
                else
                    payload_res = tls_parse_hello(context, buf + 1, payload_size, &write_packets, &dtls_cookie_verified);
                break;
                // hello verify request
            case 0x03:
                DEBUG_PRINT(" => VERIFY REQUEST\n");
                CHECK_HANDSHAKE_STATE(context, 3, 1);
                if ((context->dtls) && (!context->is_server)) {
                    payload_res = tls_parse_verify_request(context, buf + 1, payload_size, &write_packets);
                    update_hash = 0;
                } else
                    payload_res = TLS_UNEXPECTED_MESSAGE;
                break;
                // certificate
            case 0x0B:
                CHECK_HANDSHAKE_STATE(context, 4, 1);
                DEBUG_PRINT(" => CERTIFICATE\n");
                if (context->connection_status == 1) {
                    if (context->is_server) {
                        // client certificate
                        payload_res = tls_parse_certificate(context, buf + 1, payload_size, 1);
                        if ((certificate_verify) && (context->client_certificates_count))
                            certificate_verify_alert = certificate_verify(context, context->client_certificates, context->client_certificates_count);
                        // empty certificates are permitted for client
                        if (payload_res <= 0)
                            payload_res = 1;
                    } else {
                        payload_res = tls_parse_certificate(context, buf + 1, payload_size, 0);
                        if ((certificate_verify) && (context->certificates_count))
                            certificate_verify_alert = certificate_verify(context, context->certificates, context->certificates_count);
                    }
                } else
                    payload_res = TLS_UNEXPECTED_MESSAGE;
                break;
                // server key exchange
            case 0x0C:
                CHECK_HANDSHAKE_STATE(context, 5, 1);
                DEBUG_PRINT(" => SERVER KEY EXCHANGE\n");
                if (context->is_server)
                    payload_res = TLS_UNEXPECTED_MESSAGE;
                else
                    payload_res = tls_parse_server_key_exchange(context, buf + 1, payload_size);
                break;
                // certificate request
            case 0x0D:
                CHECK_HANDSHAKE_STATE(context, 6, 1);
                // server to client
                if (context->is_server)
                    payload_res = TLS_UNEXPECTED_MESSAGE;
                else
                    context->client_verified = 2;
                DEBUG_PRINT(" => CERTIFICATE REQUEST\n");
                break;
                // server hello done
            case 0x0E:
                CHECK_HANDSHAKE_STATE(context, 7, 1);
                DEBUG_PRINT(" => SERVER HELLO DONE\n");
                if (context->is_server) {
                    payload_res = TLS_UNEXPECTED_MESSAGE;
                } else {
                    payload_res = tls_parse_server_hello_done(context, buf + 1, payload_size);
                    if (payload_res > 0)
                        write_packets = 1;
                }
                break;
                // certificate verify
            case 0x0F:
                CHECK_HANDSHAKE_STATE(context, 8, 1);
                DEBUG_PRINT(" => CERTIFICATE VERIFY\n");
                if (context->connection_status == 2)
                    payload_res = tls_parse_verify(context, buf + 1, payload_size);
                else
                    payload_res = TLS_UNEXPECTED_MESSAGE;
                break;
                // client key exchange
            case 0x10:
                CHECK_HANDSHAKE_STATE(context, 9, 1);
                DEBUG_PRINT(" => CLIENT KEY EXCHANGE\n");
                if (context->is_server)
                    payload_res = tls_parse_client_key_exchange(context, buf + 1, payload_size);
                else
                    payload_res = TLS_UNEXPECTED_MESSAGE;
                break;
                // finished
            case 0x14:
                if (context->cached_handshake) {
                    TLS_FREE(context->cached_handshake);
                    context->cached_handshake = NULL;
                    context->cached_handshake_len = 0;
                }
                CHECK_HANDSHAKE_STATE(context, 10, 1);
                DEBUG_PRINT(" => FINISHED\n");
                payload_res = tls_parse_finished(context, buf + 1, payload_size, &write_packets);
                if (payload_res > 0)
                    memset(context->hs_messages, 0, sizeof(context->hs_messages));
                break;
            default:
                DEBUG_PRINT(" => NOT UNDERSTOOD PAYLOAD TYPE: %x\n", (int)type);
                return TLS_NOT_UNDERSTOOD;
        }
        if ((type != 0x00) && (update_hash))
            __private_tls_update_hash(context, buf, payload_size + 1);
        
        if (certificate_verify_alert != no_error) {
            __private_tls_write_packet(tls_build_alert(context, 1, certificate_verify_alert));
            context->critical_error = 1;
        }
        
        if (payload_res < 0) {
            switch (payload_res) {
                case TLS_UNEXPECTED_MESSAGE:
                    __private_tls_write_packet(tls_build_alert(context, 1, unexpected_message));
                    break;
                case TLS_COMPRESSION_NOT_SUPPORTED:
                    __private_tls_write_packet(tls_build_alert(context, 1, decompression_failure));
                    break;
                case TLS_BROKEN_PACKET:
                    __private_tls_write_packet(tls_build_alert(context, 1, decode_error));
                    break;
                case TLS_NO_MEMORY:
                    __private_tls_write_packet(tls_build_alert(context, 1, internal_error));
                    break;
                case TLS_NOT_VERIFIED:
                    __private_tls_write_packet(tls_build_alert(context, 1, bad_record_mac));
                    break;
                case TLS_BAD_CERTIFICATE:
                    if (context->is_server) {
                        // bad client certificate, continue
                        __private_tls_write_packet(tls_build_alert(context, 0, bad_certificate));
                        payload_res = 0;
                    } else
                        __private_tls_write_packet(tls_build_alert(context, 1, bad_certificate));
                    break;
                case TLS_UNSUPPORTED_CERTIFICATE:
                    __private_tls_write_packet(tls_build_alert(context, 1, unsupported_certificate));
                    break;
                case TLS_NO_COMMON_CIPHER:
                    __private_tls_write_packet(tls_build_alert(context, 1, insufficient_security));
                    break;
                case TLS_NOT_UNDERSTOOD:
                    __private_tls_write_packet(tls_build_alert(context, 1, internal_error));
                    break;
                case TLS_NO_RENEGOTIATION:
                    __private_tls_write_packet(tls_build_alert(context, 0, no_renegotiation));
                    payload_res = 0;
                    break;
            }
            if (payload_res < 0)
                return payload_res;
        }
        if (certificate_verify_alert != no_error)
            payload_res = TLS_BAD_CERTIFICATE;
        
        // except renegotiation
        switch (write_packets) {
            case 1:
                if (context->client_verified == 2) {
                    DEBUG_PRINT("<= Building CERTIFICATE \n");
                    __private_tls_write_packet(tls_build_certificate(context));
                    context->client_verified = 0;
                }
                // client handshake
                DEBUG_PRINT("<= Building KEY EXCHANGE\n");
                __private_tls_write_packet(tls_build_client_key_exchange(context));
                DEBUG_PRINT("<= Building CHANGE CIPHER SPEC\n");
                __private_tls_write_packet(tls_build_change_cipher_spec(context));
                context->cipher_spec_set = 1;
                context->local_sequence_number = 0;
                DEBUG_PRINT("<= Building CLIENT FINISHED\n");
                __private_tls_write_packet(tls_build_finished(context));
                context->cipher_spec_set = 0;
                break;
            case 2:
                // server handshake
                if ((context->dtls) && (dtls_cookie_verified == 0)) {
                    __private_tls_write_packet(tls_build_verify_request(context));
                    __private_dtls_reset(context);
                } else {
                    DEBUG_PRINT("<= SENDING SERVER HELLO\n");
                    __private_tls_write_packet(tls_build_hello(context));
                    DEBUG_PRINT("<= SENDING CERTIFICATE\n");
                    __private_tls_write_packet(tls_build_certificate(context));
                    int ephemeral_cipher = tls_cipher_is_ephemeral(context);
                    if (ephemeral_cipher) {
                        DEBUG_PRINT("<= SENDING EPHEMERAL DH KEY\n");
                        __private_tls_write_packet(tls_build_server_key_exchange(context, ephemeral_cipher == 1 ? KEA_dhe_rsa : KEA_ec_diffie_hellman));
                    }
                    if (context->request_client_certificate) {
                        DEBUG_PRINT("<= SENDING CERTIFICATE REQUEST\n");
                        __private_tls_write_packet(tls_certificate_request(context));
                    }
                    DEBUG_PRINT("<= SENDING DONE\n");
                    __private_tls_write_packet(tls_build_done(context));
                }
                break;
            case 3:
                // finished
                __private_tls_write_packet(tls_build_change_cipher_spec(context));
                __private_tls_write_packet(tls_build_finished(context));
                context->connection_status = 0xFF;
                break;
            case 4:
                // dtls only
                context->dtls_seq = 1;
                __private_tls_write_packet(tls_build_hello(context));
                break;
        }
        payload_size++;
        buf += payload_size;
        buf_len -= payload_size;
    }
    return orig_len;
}

unsigned int __private_tls_hmac_message(unsigned char local, struct TLSContext *context, const unsigned char *buf, int buf_len, const unsigned char *buf2, int buf_len2, unsigned char *out, unsigned int outlen, uint64_t remote_sequence_number) {
    hmac_state hash;
    int mac_size = outlen;
    int hash_idx;
    if (mac_size == __TLS_SHA1_MAC_SIZE)
        hash_idx = find_hash("sha1");
    else
    if (mac_size == __TLS_SHA384_MAC_SIZE)
        hash_idx = find_hash("sha384");
    else
        hash_idx = find_hash("sha256");
    
    if (hmac_init(&hash, hash_idx, local ? context->crypto.ctx_local_mac.local_mac : context->crypto.ctx_remote_mac.remote_mac, mac_size))
        return 0;

    uint64_t squence_number;
    if (context->dtls)
        squence_number = htonll(remote_sequence_number);
    else
    if (local)
        squence_number = htonll(context->local_sequence_number);
    else
        squence_number = htonll(context->remote_sequence_number);

    if (hmac_process(&hash, (unsigned char *)&squence_number, sizeof(uint64_t)))
        return 0;
    
    if (hmac_process(&hash, buf, buf_len))
        return 0;
    if ((buf2) && (buf_len2)) {
        if (hmac_process(&hash, buf2, buf_len2))
            return 0;
    }
    unsigned long ref_outlen = outlen;
    if (hmac_done(&hash, out, &ref_outlen))
        return 0;
    
    return (unsigned int)ref_outlen;
}

int tls_parse_message(struct TLSContext *context, unsigned char *buf, int buf_len, tls_validation_function certificate_verify) {
    int res = 5;
    if (context->dtls)
        res = 13;
    int header_size = res;
    int payload_res = 0;
    
    CHECK_SIZE(res, buf_len, TLS_NEED_MORE_DATA)
    
    unsigned char type = *buf;

    int buf_pos = 1;

    unsigned short version = ntohs(*(unsigned short *)&buf[buf_pos]);
    buf_pos += 2;

    uint64_t dtls_sequence_number = 0;
    if (context->dtls) {
        CHECK_SIZE(buf_pos + 8, buf_len, TLS_NEED_MORE_DATA)
        dtls_sequence_number = ntohll(*(uint64_t *)&buf[buf_pos]);
        buf_pos += 8;
    }

    VERSION_SUPPORTED(version, TLS_NOT_SAFE)
    unsigned short length;
    length = ntohs(*(unsigned short *)&buf[buf_pos]);
    buf_pos += 2;

    unsigned char *pt = NULL;
    const unsigned char *ptr = buf + buf_pos;

    CHECK_SIZE(buf_pos + length, buf_len, TLS_NEED_MORE_DATA)
    DEBUG_PRINT("Message type: %0x, length: %i\n", (int)type, (int)length);
    if (context->cipher_spec_set) {
        DEBUG_DUMP_HEX_LABEL("encrypted", &buf[header_size], length);
        if (!context->crypto.created) {
            DEBUG_PRINT("Encryption context not created\n");
            __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
            return TLS_BROKEN_PACKET;
        }
        pt = (unsigned char *)TLS_MALLOC(length);
        if (!pt) {
            DEBUG_PRINT("Error in TLS_MALLOC (%i bytes)\n", (int)length);
            __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
            return TLS_NO_MEMORY;
        }
        unsigned char aad[16];
        if (context->crypto.created == 2) {
            int pt_length = length - 8 - __TLS_GCM_TAG_LEN;
            if (pt_length < 0) {
                DEBUG_PRINT("Invalid packet length");
                TLS_FREE(pt);
                __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
                return TLS_BROKEN_PACKET;
            }
            // build aad and iv
            if (context->dtls)
                *((uint64_t *)aad) = htonll(dtls_sequence_number);
            else
                *((uint64_t *)aad) = htonll(context->remote_sequence_number);
            unsigned char iv[12];
            memcpy(iv, context->crypto.ctx_remote_mac.remote_aead_iv, 4);
            memcpy(iv + 4, buf + header_size, 8);
            gcm_reset(&context->crypto.ctx_remote.aes_gcm_remote);
            int res0 = gcm_add_iv(&context->crypto.ctx_remote.aes_gcm_remote, iv, 12);
            
            DEBUG_DUMP_HEX_LABEL("aad iv", iv, 12);
            aad[8] = buf[0];
            aad[9] = buf[1];
            aad[10] = buf[2];
            
            *((unsigned short *)&aad[11]) = htons(pt_length);
            int res1 = gcm_add_aad(&context->crypto.ctx_remote.aes_gcm_remote, aad, 13);
            memset(pt, 0, length);
            DEBUG_PRINT("PT SIZE: %i\n", pt_length);
            int res2 = gcm_process(&context->crypto.ctx_remote.aes_gcm_remote, pt, pt_length, buf + header_size + 8, GCM_DECRYPT);
            unsigned char tag[32];
            unsigned long taglen = 32;
            int res3 = gcm_done(&context->crypto.ctx_remote.aes_gcm_remote, tag, &taglen);
            if ((res0) || (res1) || (res2) || (res3) || (taglen != __TLS_GCM_TAG_LEN)) {
                DEBUG_PRINT("ERROR: gcm_add_iv: %i, gcm_add_aad: %i, gcm_process: %i, gcm_done: %i\n", res0, res1, res2, res3);
                __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
                return TLS_BROKEN_PACKET;
            }
            DEBUG_DUMP_HEX_LABEL("decrypted", pt, pt_length);
            DEBUG_DUMP_HEX_LABEL("tag", tag, taglen);
            // check tag
            if (memcmp(buf + header_size + 8 + pt_length, tag, taglen)) {
                DEBUG_PRINT("INTEGRITY CHECK FAILED (msg length %i)\n", pt_length);
                DEBUG_DUMP_HEX_LABEL("TAG RECEIVED", buf + header_size + 8 + pt_length, taglen);
                DEBUG_DUMP_HEX_LABEL("TAG COMPUTED", tag, taglen);
                TLS_FREE(pt);
                __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
                __private_tls_write_packet(tls_build_alert(context, 1, bad_record_mac));
                return TLS_INTEGRITY_FAILED;
            }
            ptr = pt;
            length = pt_length;
#ifdef TLS_WITH_CHACHA20_POLY1305
        } else
        if (context->crypto.created == 3) {
            int pt_length = length - POLY1305_TAGLEN;
            unsigned int counter = 1;
            unsigned char poly1305_key[POLY1305_KEYLEN];
            unsigned char trail[16];
            unsigned char mac_tag[POLY1305_TAGLEN];

            if (pt_length < 0) {
                DEBUG_PRINT("Invalid packet length");
                TLS_FREE(pt);
                __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
                return TLS_BROKEN_PACKET;
            }
            if (context->dtls)
                *((uint64_t *)aad) = htonll(dtls_sequence_number);
            else
                *((uint64_t *)aad) = htonll(context->remote_sequence_number);
            aad[8] = buf[0];
            aad[9] = buf[1];
            aad[10] = buf[2];
            *((unsigned short *)&aad[11]) = htons(pt_length);
            aad[13] = 0;
            aad[14] = 0;
            aad[15] = 0;

            chacha_ivupdate(&context->crypto.ctx_remote.chacha_remote, context->crypto.ctx_remote_mac.remote_aead_iv, aad, (unsigned char *)&counter);

            chacha_encrypt_bytes(&context->crypto.ctx_remote.chacha_remote, buf + header_size, pt, pt_length);
            DEBUG_DUMP_HEX_LABEL("decrypted", pt, pt_length);
            ptr = pt;
            length = pt_length;

            chacha20_poly1305_key(&context->crypto.ctx_remote.chacha_remote, poly1305_key);
            poly1305_context ctx;
            poly1305_init(&ctx, poly1305_key);
            poly1305_update(&ctx, aad, 16);
            poly1305_update(&ctx, buf + header_size, pt_length);
            int rem = pt_length % 16;
            if (rem) {
                static unsigned char zeropad[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
                poly1305_update(&ctx, zeropad, 16 - rem);
            }
            
            U32TO8(&trail[0], 13);
            *(int *)&trail[4] = 0;
            U32TO8(&trail[8], pt_length);
            *(int *)&trail[12] = 0;

            poly1305_update(&ctx, trail, 16);
            poly1305_finish(&ctx, mac_tag);
            if (memcmp(mac_tag, buf + header_size + pt_length, POLY1305_TAGLEN)) {
                DEBUG_PRINT("INTEGRITY CHECK FAILED (msg length %i)\n", length);
                DEBUG_DUMP_HEX_LABEL("POLY1305 TAG RECEIVED", buf + header_size + pt_length, POLY1305_TAGLEN);
                DEBUG_DUMP_HEX_LABEL("POLY1305 TAG COMPUTED", mac_tag, POLY1305_TAGLEN);
                TLS_FREE(pt);

                // silently ignore packet for DTLS
                if (context->dtls)
                    return header_size + length;

                __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
                __private_tls_write_packet(tls_build_alert(context, 1, bad_record_mac));
                return TLS_INTEGRITY_FAILED;
            }
#endif
        } else {
            int err = __private_tls_crypto_decrypt(context, buf + header_size, pt, length);
            if (err) {
                TLS_FREE(pt);
                DEBUG_PRINT("Decryption error %i\n", (int)err);
                __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
                return TLS_BROKEN_PACKET;
            }
            unsigned char padding_byte = pt[length - 1];
            unsigned char padding = padding_byte + 1;
            
            // poodle check
            int padding_index = length - padding;
            if (padding_index > 0) {
                int i;
                int limit = length - 1;
                for (i = length - padding; i < limit; i++) {
                    if (pt[i] != padding_byte) {
                        TLS_FREE(pt);
                        DEBUG_PRINT("BROKEN PACKET (POODLE ?)\n");
                        __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
                        __private_tls_write_packet(tls_build_alert(context, 1, decrypt_error));
                        return TLS_BROKEN_PACKET;
                    }
                }
            }
            
            unsigned int decrypted_length = length;
            if (padding < decrypted_length)
                decrypted_length -= padding;
            
            DEBUG_DUMP_HEX_LABEL("decrypted", pt, decrypted_length);
            ptr = pt;
#ifdef TLS_LEGACY_SUPPORT
            if ((context->version != TLS_V10) && (decrypted_length > __TLS_AES_IV_LENGTH)) {
                decrypted_length -= __TLS_AES_IV_LENGTH;
                ptr += __TLS_AES_IV_LENGTH;
            }
#else
            if (decrypted_length > __TLS_AES_IV_LENGTH) {
                decrypted_length -= __TLS_AES_IV_LENGTH;
                ptr += __TLS_AES_IV_LENGTH;
            }
#endif
            length = decrypted_length;
            
            unsigned int mac_size = __private_tls_mac_length(context);
            if ((length < mac_size) || (!mac_size)) {
                TLS_FREE(pt);
                DEBUG_PRINT("BROKEN PACKET\n");
                __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
                __private_tls_write_packet(tls_build_alert(context, 1, decrypt_error));
                return TLS_BROKEN_PACKET;
            }
            
            length -= mac_size;
            
            const unsigned char *message_hmac = &ptr[length];
            unsigned char hmac_out[__TLS_MAX_MAC_SIZE];
            unsigned char temp_buf[5];
            memcpy(temp_buf, buf, 3);
            *(unsigned short *)&temp_buf[3] = htons(length);
            unsigned int hmac_out_len = __private_tls_hmac_message(0, context, temp_buf, 5, ptr, length, hmac_out, mac_size, dtls_sequence_number);
            if ((hmac_out_len != mac_size) || (memcmp(message_hmac, hmac_out, mac_size))) {
                DEBUG_PRINT("INTEGRITY CHECK FAILED (msg length %i)\n", length);
                DEBUG_DUMP_HEX_LABEL("HMAC RECEIVED", message_hmac, mac_size);
                DEBUG_DUMP_HEX_LABEL("HMAC COMPUTED", hmac_out, hmac_out_len);
                TLS_FREE(pt);

                // silently ignore packet for DTLS
                if (context->dtls)
                    return header_size + length;

                __private_random_sleep(context, __TLS_MAX_ERROR_SLEEP_uS);
                __private_tls_write_packet(tls_build_alert(context, 1, bad_record_mac));

                return TLS_INTEGRITY_FAILED;
            }
        }
    }
    context->remote_sequence_number++;
    
    switch (type) {
            // application data
        case TLS_APPLICATION_DATA:
            if (context->connection_status != 0xFF) {
                DEBUG_PRINT("UNEXPECTED APPLICATION DATA MESSAGE\n");
                payload_res = TLS_UNEXPECTED_MESSAGE;
                __private_tls_write_packet(tls_build_alert(context, 1, unexpected_message));
            } else {
                DEBUG_PRINT("APPLICATION DATA MESSAGE (TLS VERSION: %x):\n", (int)context->version);
                DEBUG_DUMP(ptr, length);
                DEBUG_PRINT("\n");
                __private_tls_write_app_data(context, ptr, length);
            }
            break;
            // handshake
        case TLS_HANDSHAKE:
            DEBUG_PRINT("HANDSHAKE MESSAGE\n");
            payload_res = tls_parse_payload(context, ptr, length, certificate_verify);
            break;
            // change cipher spec
        case TLS_CHANGE_CIPHER:
            context->dtls_epoch_remote++;
            if (context->connection_status != 2) {
                DEBUG_PRINT("UNEXPECTED CHANGE CIPHER SPEC MESSAGE (%i)\n", context->connection_status);
                __private_tls_write_packet(tls_build_alert(context, 1, unexpected_message));
                payload_res = TLS_UNEXPECTED_MESSAGE;
            } else {
                DEBUG_PRINT("CHANGE CIPHER SPEC MESSAGE\n");
                context->cipher_spec_set = 1;
                // reset sequence numbers
                context->remote_sequence_number = 0;
            }
            break;
            // alert
        case TLS_ALERT:
            DEBUG_PRINT("ALERT MESSAGE\n");
            if (length >= 2) {
                DEBUG_DUMP_HEX(ptr, length);
                int level = ptr[0];
                int code = ptr[1];
                if (level == TLS_ALERT_CRITICAL) {
                    context->critical_error = 1;
                    res = TLS_ERROR_ALERT;
                }
                context->error_code = code;
            }
            break;
        default:
            DEBUG_PRINT("NOT UNDERSTOOD MESSAGE TYPE: %x\n", (int)type);
            return TLS_NOT_UNDERSTOOD;
    }
    TLS_FREE(pt);
    
    if (payload_res < 0)
        return payload_res;
    
    if (res > 0)
        return header_size + length;
    
    return res;
}

unsigned int asn1_get_len(const unsigned char *buffer, int buf_len, unsigned int *octets) {
    *octets = 0;
    
    if (buf_len < 1)
        return 0;
    
    unsigned char size = buffer[0];
    int i;
    if (size & 0x80) {
        *octets = size & 0x7F;
        if ((int)*octets > buf_len - 1)
            return 0;
        // max 32 bits
        unsigned int ref_octets = *octets;
        if (*octets > 4)
            ref_octets = 4;
        if ((int)*octets > buf_len -1)
            return 0;
        unsigned int long_size = 0;
        unsigned int coef = 1;
        
        for (i = ref_octets; i > 0; i--) {
            long_size += buffer[i] * coef;
            coef *= 0x100;
        }
        ++*octets;
        return long_size;
    }
    ++*octets;
    return size;
}

void print_index(unsigned int *fields) {
    int i = 0;
    while (fields[i]) {
        if (i)
            DEBUG_PRINT(".");
        DEBUG_PRINT("%i", fields[i]);
        i++;
    }
}

int __is_field(unsigned int *fields, unsigned int *prefix) {
    int i = 0;
    while (prefix[i]) {
        if (fields[i] != prefix[i])
            return 0;
        i++;
    }
    return 1;
}

int __private_tls_hash_len(int algorithm) {
    switch (algorithm) {
        case TLS_RSA_SIGN_MD5:
            return 16;
        case TLS_RSA_SIGN_SHA1:
            return 20;
        case TLS_RSA_SIGN_SHA256:
            return 32;
        case TLS_RSA_SIGN_SHA384:
            return 48;
        case TLS_RSA_SIGN_SHA512:
            return 64;
    }
    return 0;
}

unsigned char *__private_tls_compute_hash(int algorithm, const unsigned char *message, unsigned int message_len) {
    unsigned char *hash = NULL;
    if ((!message) || (!message_len))
        return hash;
    int err;
    hash_state state;
    switch (algorithm) {
        case TLS_RSA_SIGN_MD5:
            DEBUG_PRINT("SIGN MD5\n");
            hash = (unsigned char *)TLS_MALLOC(16);
            if (!hash)
                return NULL;
            
            err = md5_init(&state);
            if (!err) {
                err = md5_process(&state, message, message_len);
                if (!err)
                    err = md5_done(&state, hash);
            }
            break;
        case TLS_RSA_SIGN_SHA1:
            DEBUG_PRINT("SIGN SHA1\n");
            hash = (unsigned char *)TLS_MALLOC(20);
            if (!hash)
                return NULL;
            
            err = sha1_init(&state);
            if (!err) {
                err = sha1_process(&state, message, message_len);
                if (!err)
                    err = sha1_done(&state, hash);
            }
            break;
        case TLS_RSA_SIGN_SHA256:
            DEBUG_PRINT("SIGN SHA256\n");
            hash = (unsigned char *)TLS_MALLOC(32);
            if (!hash)
                return NULL;
            
            err = sha256_init(&state);
            if (!err) {
                err = sha256_process(&state, message, message_len);
                if (!err)
                    err = sha256_done(&state, hash);
            }
            break;
        case TLS_RSA_SIGN_SHA384:
            DEBUG_PRINT("SIGN SHA384\n");
            hash = (unsigned char *)TLS_MALLOC(48);
            if (!hash)
                return NULL;
            
            err = sha384_init(&state);
            if (!err) {
                err = sha384_process(&state, message, message_len);
                if (!err)
                    err = sha384_done(&state, hash);
            }
            break;
        case TLS_RSA_SIGN_SHA512:
            DEBUG_PRINT("SIGN SHA512\n");
            hash = (unsigned char *)TLS_MALLOC(64);
            if (!hash)
                return NULL;
            
            err = sha512_init(&state);
            if (!err) {
                err = sha512_process(&state, message, message_len);
                if (!err)
                    err = sha512_done(&state, hash);
            }
            break;
        default:
            DEBUG_PRINT("UNKNOWN SIGNATURE ALGORITHM\n");
    }
    return hash;
}

int tls_certificate_verify_signature(struct TLSCertificate *cert, struct TLSCertificate *parent) {
    if ((!cert) || (!parent) || (!cert->sign_key) || (!cert->fingerprint) || (!cert->sign_len) || (!parent->der_bytes) || (!parent->der_len)) {
        DEBUG_PRINT("CANNOT VERIFY SIGNATURE");
        return 0;
    }
    init_dependencies();
    int hash_len = __private_tls_hash_len(cert->algorithm);
    if (hash_len <= 0)
        return 0;
    
    int hash_index = -1;
    switch (cert->algorithm) {
        case TLS_RSA_SIGN_MD5:
            hash_index = find_hash("md5");
            break;
        case TLS_RSA_SIGN_SHA1:
            hash_index = find_hash("sha1");
            break;
        case TLS_RSA_SIGN_SHA256:
            hash_index = find_hash("sha256");
            break;
        case TLS_RSA_SIGN_SHA384:
            hash_index = find_hash("sha384");
            break;
        case TLS_RSA_SIGN_SHA512:
            hash_index = find_hash("sha512");
            break;
        default:
            DEBUG_PRINT("UNKNOWN SIGNATURE ALGORITHM\n");
            return 0;
    }
    
    rsa_key key;
    int err = rsa_import(parent->der_bytes, parent->der_len, &key);
    if (err) {
        DEBUG_PRINT("Error importing RSA certificate (code: %i)\n", err);
        DEBUG_DUMP_HEX_LABEL("CERTIFICATE", parent->der_bytes, parent->der_len);
        return 0;
    }
    int rsa_stat = 0;
    unsigned char *signature = cert->sign_key;
    int signature_len = cert->sign_len;
    if (!signature[0]) {
        signature++;
        signature_len--;
    }
    err = rsa_verify_hash_ex(signature, signature_len, cert->fingerprint, hash_len, LTC_LTC_PKCS_1_V1_5, hash_index, 0, &rsa_stat, &key);
    rsa_free(&key);
    if (err) {
        DEBUG_PRINT("HASH VERIFY ERROR %i\n", err);
        return 0;
    }
    DEBUG_PRINT("CERTIFICATE VALIDATION: %i\n", rsa_stat);
    return rsa_stat;
}

int tls_certificate_chain_is_valid(struct TLSCertificate **certificates, int len) {
    if ((!certificates) || (!len))
        return bad_certificate;
    
    int i;
    len--;
    
    // expired certificate or not yet valid ?
    if (tls_certificate_is_valid(certificates[0]))
        return bad_certificate;
    
    // check
    for (i = 0; i < len; i++) {
        // certificate in chain is expired ?
        if (tls_certificate_is_valid(certificates[i+1]))
            return bad_certificate;
        if (!tls_certificate_verify_signature(certificates[i], certificates[i+1]))
            return bad_certificate;
    }
    return 0;
}

int tls_certificate_chain_is_valid_root(struct TLSContext *context, struct TLSCertificate **certificates, int len) {
    if ((!certificates) || (!len) || (!context->root_certificates) || (!context->root_count))
        return bad_certificate;
    int i;
    unsigned int j;
    for (i = 0; i < len; i++) {
        for (j = 0; j < context->root_count; j++) {
            // check if root certificate expired
            if (tls_certificate_is_valid(context->root_certificates[j]))
                continue;
            // if any root validates any certificate in the chain, then is root validated
            if (tls_certificate_verify_signature(certificates[i], context->root_certificates[j]))
                return 0;
        }
    }
    return bad_certificate;
}

int __private_is_oid(struct __private_OID_chain *ref_chain, unsigned char *looked_oid, int looked_oid_len) {
    while (ref_chain) {
        if (ref_chain->oid) {
            if (__is_oid2(ref_chain->oid, looked_oid, 16, looked_oid_len))
                return 1;
        }
        ref_chain = (struct __private_OID_chain *)ref_chain->top;
    }
    return 0;
}

int __private_asn1_parse(struct TLSContext *context, struct TLSCertificate *cert, const unsigned char *buffer, int size, int level, unsigned int *fields, unsigned char *has_key, int client_cert, unsigned char *top_oid, struct __private_OID_chain *chain) {
    struct __private_OID_chain local_chain;
    local_chain.top = chain;
    int pos = 0;
    // X.690
    int idx = 0;
    unsigned char oid[16];
    memset(oid, 0, 16);
    local_chain.oid = oid;
    if (has_key)
        *has_key = 0;
    unsigned char local_has_key = 0;
    const unsigned char *cert_data = NULL;
    unsigned int cert_len = 0;
    while (pos < size) {
        unsigned int start_pos = pos;
        CHECK_SIZE(2, size - pos, TLS_NEED_MORE_DATA)
        unsigned char first = buffer[pos++];
        unsigned char type = first & 0x1F;
        unsigned char constructed = first & 0x20;
        unsigned char element_class = first >> 6;
        unsigned int octets = 0;
        unsigned int temp;
        idx++;
        if (level <= __TLS_ASN1_MAXLEVEL)
            fields[level - 1] = idx;
        unsigned int length = asn1_get_len((unsigned char *)&buffer[pos], size - pos, &octets);
        if ((octets > 4) || (octets > size - pos))  {
            DEBUG_PRINT("CANNOT READ CERTIFICATE\n");
            return pos;
        }
        pos += octets;
        CHECK_SIZE(length, size - pos, TLS_NEED_MORE_DATA)
        //DEBUG_PRINT("FIRST: %x => %x (%i)\n", (int)first, (int)type, length);
        // sequence
        //DEBUG_PRINT("%2i: ", level);
#ifdef DEBUG
        DEBUG_INDEX(fields);
        int i1;
        for (i1 = 1; i1 < level; i1++)
            DEBUG_PRINT("  ");
#endif
        
        if ((length) && (constructed)) {
            switch (type) {
                case 0x03:
                    DEBUG_PRINT("CONSTRUCTED BITSTREAM\n");
                    break;
                case 0x10:
                    DEBUG_PRINT("SEQUENCE\n");
                    if ((level == 2) && (idx == 1)) {
                        cert_len = length + (pos - start_pos);
                        cert_data = &buffer[start_pos];
                    }
                    // private key on server or public key on client
                    if ((!cert->version) && (__is_field(fields, priv_der_id))) {
                        TLS_FREE(cert->der_bytes);
                        temp = length + (pos - start_pos);
                        cert->der_bytes = (unsigned char *)TLS_MALLOC(temp);
                        if (cert->der_bytes) {
                            memcpy(cert->der_bytes, &buffer[start_pos], temp);
                            cert->der_len = temp;
                        } else
                            cert->der_len = 0;
                    }
                    break;
                case 0x11:
                    DEBUG_PRINT("EMBEDDED PDV\n");
                    break;
                case 0x00:
                    if (element_class == 0x02) {
                        DEBUG_PRINT("CONTEXT-SPECIFIC\n");
                        break;
                    }
                default:
                    DEBUG_PRINT("CONSTRUCT TYPE %02X\n", (int)type);
            }
            local_has_key = 0;
            __private_asn1_parse(context, cert, &buffer[pos], length, level + 1, fields, &local_has_key, client_cert, top_oid, &local_chain);
            if ((((local_has_key) && (context) && ((!context->is_server) || (client_cert))) || (!context)) && (__is_field(fields, pk_id))) {
                TLS_FREE(cert->der_bytes);
                temp = length + (pos - start_pos);
                cert->der_bytes = (unsigned char *)TLS_MALLOC(temp);
                if (cert->der_bytes) {
                    memcpy(cert->der_bytes, &buffer[start_pos], temp);
                    cert->der_len = temp;
                } else
                    cert->der_len = 0;
            }
        } else {
            switch (type) {
                case 0x00:
                    // end of content
                    DEBUG_PRINT("END OF CONTENT\n");
                    return pos;
                    break;
                case 0x01:
                    // boolean
                    temp = buffer[pos];
                    DEBUG_PRINT("BOOLEAN: %i\n", temp);
                    break;
                case 0x02:
                    // integer
                    if (__is_field(fields, pk_id)) {
                        if (has_key)
                            *has_key = 1;
                        
                        if (idx == 1)
                            tls_certificate_set_key(cert, &buffer[pos], length);
                        else
                        if (idx == 2)
                            tls_certificate_set_exponent(cert, &buffer[pos], length);
                    } else
                    if (__is_field(fields, serial_id))
                        tls_certificate_set_serial(cert, &buffer[pos], length);
                    if ((__is_field(fields, version_id)) && (length == 1))
                        cert->version = buffer[pos];
                    if (level >= 2) {
                        unsigned int fields_temp[3];
                        fields_temp[0] = fields[level - 2];
                        fields_temp[1] = fields[level - 1];
                        fields_temp[2] = 0;
                        if (__is_field(fields_temp, priv_id))
                            tls_certificate_set_priv(cert, &buffer[pos], length);
                    }
                    DEBUG_PRINT("INTEGER(%i): ", length);
                    DEBUG_DUMP_HEX(&buffer[pos], length);
                    if ((chain) && (length > 2)) {
                        if (__private_is_oid(chain, san_oid, sizeof(san_oid) - 1)) {
                            cert->san = (unsigned char **)TLS_REALLOC(cert->san, sizeof(unsigned char *) * (cert->san_length + 1));
                            if (cert->san) {
                                cert->san[cert->san_length] = NULL;
                                tls_certificate_set_copy(&cert->san[cert->san_length], &buffer[pos], length);
                                DEBUG_PRINT(" => SUBJECT ALTERNATIVE NAME: %s", cert->san[cert->san_length ]);
                                cert->san_length++;
                            } else
                                cert->san_length = 0;
                        }
                    }
                    DEBUG_PRINT("\n");
                    break;
                case 0x03:
                    // bitstream
                    DEBUG_PRINT("BITSTREAM(%i): ", length);
                    DEBUG_DUMP_HEX(&buffer[pos], length);
                    DEBUG_PRINT("\n");
                    if (__is_field(fields, sign_id)) {
                        tls_certificate_set_sign_key(cert, &buffer[pos], length);
                    } else
                    if ((cert->ec_algorithm) && (__is_field(fields, pk_id))) {
                        tls_certificate_set_key(cert, &buffer[pos], length);
                    } else {
                        if ((buffer[pos] == 0x00) && (length > 256))
                            __private_asn1_parse(context, cert, &buffer[pos]+1, length - 1, level + 1, fields, &local_has_key, client_cert, top_oid, &local_chain);
                        else
                            __private_asn1_parse(context, cert, &buffer[pos], length, level + 1, fields, &local_has_key, client_cert, top_oid, &local_chain);
                        
                        if (top_oid) {
                            if (__is_oid2(top_oid, TLS_EC_prime256v1_OID, sizeof(oid), sizeof(TLS_EC_prime256v1) - 1)) {
                                cert->ec_algorithm = secp256r1.iana;
                            } else
                            if (__is_oid2(top_oid, TLS_EC_secp224r1_OID, sizeof(oid), sizeof(TLS_EC_secp224r1_OID) - 1)) {
                                cert->ec_algorithm = secp224r1.iana;
                            } else
                            if (__is_oid2(top_oid, TLS_EC_secp384r1_OID, sizeof(oid), sizeof(TLS_EC_secp384r1_OID) - 1)) {
                                cert->ec_algorithm = secp384r1.iana;
                            } else
                            if (__is_oid2(top_oid, TLS_EC_secp521r1_OID, sizeof(oid), sizeof(TLS_EC_secp521r1_OID) - 1)) {
                                cert->ec_algorithm = secp521r1.iana;
                            }
                            if ((cert->ec_algorithm) && (!cert->pk))
                                tls_certificate_set_key(cert, &buffer[pos], length);
                        }
                    }
                    break;
                case 0x04:
                    if ((top_oid) && (__is_field(fields, ecc_priv_id)) && (!cert->priv)) {
                        DEBUG_PRINT("BINARY STRING(%i): ", length);
                        DEBUG_DUMP_HEX(&buffer[pos], length);
                        DEBUG_PRINT("\n");
                        tls_certificate_set_priv(cert, &buffer[pos], length);
                    } else
                        __private_asn1_parse(context, cert, &buffer[pos], length, level + 1, fields, &local_has_key, client_cert, top_oid, &local_chain);
                    break;
                case 0x05:
                    DEBUG_PRINT("NULL\n");
                    break;
                case 0x06:
                    // object identifier
                    if (__is_field(fields, pk_id)) {
                        if ((length == 8) || (length == 5))
                            tls_certificate_set_algorithm(&cert->ec_algorithm, &buffer[pos], length);
                        else
                            tls_certificate_set_algorithm(&cert->key_algorithm, &buffer[pos], length);
                    }
                    if (__is_field(fields, algorithm_id))
                        tls_certificate_set_algorithm(&cert->algorithm, &buffer[pos], length);
                    
                    DEBUG_PRINT("OBJECT IDENTIFIER(%i): ", length);
                    DEBUG_DUMP_HEX(&buffer[pos], length);
                    DEBUG_PRINT("\n");
                    // check previous oid
                    if (__is_oid2(oid, ocsp_oid, 16, sizeof(ocsp_oid) - 1))
                        tls_certificate_set_copy(&cert->ocsp, &buffer[pos], length);

                    if (length < 16)
                        memcpy(oid, &buffer[pos], length);
                    else
                        memcpy(oid, &buffer[pos], 16);
                    if (top_oid)
                        memcpy(top_oid, oid, 16);
                    break;
                case 0x09:
                    DEBUG_PRINT("REAL NUMBER(%i): ", length);
                    DEBUG_DUMP_HEX(&buffer[pos], length);
                    DEBUG_PRINT("\n");
                    break;
                case 0x17:
                    // utc time
                    DEBUG_PRINT("UTC TIME: [");
                    DEBUG_DUMP(&buffer[pos], length);
                    DEBUG_PRINT("]\n");
                    
                    if (__is_field(fields, validity_id)) {
                        if (idx == 1)
                            tls_certificate_set_copy_date(&cert->not_before, &buffer[pos], length);
                        else
                            tls_certificate_set_copy_date(&cert->not_after, &buffer[pos], length);
                    }
                    break;
                case 0x18:
                    // generalized time
                    DEBUG_PRINT("GENERALIZED TIME: [");
                    DEBUG_DUMP(&buffer[pos], length);
                    DEBUG_PRINT("]\n");
                    break;
                case 0x13:
                    // printable string
                case 0x0C:
                case 0x14:
                case 0x15:
                case 0x16:
                case 0x19:
                case 0x1A:
                case 0x1B:
                case 0x1C:
                case 0x1D:
                case 0x1E:
                    if (__is_field(fields, issurer_id)) {
                        if (__is_oid(oid, country_oid, 3))
                            tls_certificate_set_copy(&cert->issuer_country, &buffer[pos], length);
                        else
                        if (__is_oid(oid, state_oid, 3))
                            tls_certificate_set_copy(&cert->issuer_state, &buffer[pos], length);
                        else
                        if (__is_oid(oid, location_oid, 3))
                            tls_certificate_set_copy(&cert->issuer_location, &buffer[pos], length);
                        else
                        if (__is_oid(oid, entity_oid, 3))
                            tls_certificate_set_copy(&cert->issuer_entity, &buffer[pos], length);
                        else
                        if (__is_oid(oid, subject_oid, 3))
                            tls_certificate_set_copy(&cert->issuer_subject, &buffer[pos], length);
                    } else
                    if (__is_field(fields, owner_id)) {
                        if (__is_oid(oid, country_oid, 3))
                            tls_certificate_set_copy(&cert->country, &buffer[pos], length);
                        else
                        if (__is_oid(oid, state_oid, 3))
                            tls_certificate_set_copy(&cert->state, &buffer[pos], length);
                        else
                        if (__is_oid(oid, location_oid, 3))
                            tls_certificate_set_copy(&cert->location, &buffer[pos], length);
                        else
                        if (__is_oid(oid, entity_oid, 3))
                            tls_certificate_set_copy(&cert->entity, &buffer[pos], length);
                        else
                        if (__is_oid(oid, subject_oid, 3))
                            tls_certificate_set_copy(&cert->subject, &buffer[pos], length);
                    }
                    DEBUG_PRINT("STR: [");
                    DEBUG_DUMP(&buffer[pos], length);
                    DEBUG_PRINT("]\n");
                    break;
                case 0x10:
                    DEBUG_PRINT("EMPTY SEQUENCE\n");
                    break;
                case 0xA:
                    DEBUG_PRINT("ENUMERATED(%i): ", length);
                    DEBUG_DUMP_HEX(&buffer[pos], length);
                    DEBUG_PRINT("\n");
                    break;
                default:
                    DEBUG_PRINT("========> NOT SUPPORTED %x\n", (int)type);
                    // not supported / needed
                    break;
            }
        }
        pos += length;
    }
    if ((level == 2) && (cert->sign_key) && (cert->sign_len) && (cert_len) && (cert_data)) {
        TLS_FREE(cert->fingerprint);
        cert->fingerprint = __private_tls_compute_hash(cert->algorithm, cert_data, cert_len);
#ifdef DEBUG
        if (cert->fingerprint) {
            DEBUG_DUMP_HEX_LABEL("FINGERPRINT", cert->fingerprint, __private_tls_hash_len(cert->algorithm));
        }
#endif
    }
    return pos;
}

struct TLSCertificate *asn1_parse(struct TLSContext *context, const unsigned char *buffer, int size, int client_cert) {
    unsigned int fields[__TLS_ASN1_MAXLEVEL];
    memset(fields, 0, sizeof(int) * __TLS_ASN1_MAXLEVEL);
    struct TLSCertificate *cert = tls_create_certificate();
    if (cert) {
        if (client_cert < 0) {
            client_cert = 0;
            // private key
            unsigned char top_oid[16];
            memset(top_oid, 0, sizeof(top_oid));
            __private_asn1_parse(context, cert, buffer, size, 1, fields, NULL, client_cert, top_oid, NULL);
        } else
            __private_asn1_parse(context, cert, buffer, size, 1, fields, NULL, client_cert, NULL, NULL);
    }
    return cert;
}

int tls_load_certificates(struct TLSContext *context, const unsigned char *pem_buffer, int pem_size) {
    if (!context)
        return TLS_GENERIC_ERROR;
    
    unsigned int len;
    int idx = 0;
    do {
        unsigned char *data = tls_pem_decode(pem_buffer, pem_size, idx++, &len);
        if ((!data) || (!len))
            break;
        struct TLSCertificate *cert = asn1_parse(context, data, len, 0);
        if (cert) {
            if (cert->version == 2) {
                TLS_FREE(cert->der_bytes);
                cert->der_bytes = data;
                cert->der_len = len;
                data = NULL;
                if (cert->priv) {
                    DEBUG_PRINT("WARNING - parse error (private key encountered in certificate)\n");
                    TLS_FREE(cert->priv);
                    cert->priv = NULL;
                    cert->priv_len = 0;
                }
                context->certificates = (struct TLSCertificate **)TLS_REALLOC(context->certificates, (context->certificates_count + 1) * sizeof(struct TLSCertificate));
                context->certificates[context->certificates_count] = cert;
                context->certificates_count++;
                DEBUG_PRINT("Loaded certificate: %i\n", (int)context->certificates_count);
            } else {
                DEBUG_PRINT("WARNING - certificate version error (v%i)\n", (int)cert->version);
                tls_destroy_certificate(cert);
            }
        }
        TLS_FREE(data);
    } while (1);
    return context->certificates_count;
}

int tls_load_private_key(struct TLSContext *context, const unsigned char *pem_buffer, int pem_size) {
    if (!context)
        return TLS_GENERIC_ERROR;
    
    unsigned int len;
    int idx = 0;
    do {
        unsigned char *data = tls_pem_decode(pem_buffer, pem_size, idx++, &len);
        if ((!data) || (!len))
            break;
        struct TLSCertificate *cert = asn1_parse(context, data, len, -1);
        if (!cert->der_len) {
            TLS_FREE(cert->der_bytes);
            cert->der_bytes = data;
            cert->der_len = len;
        } else
            TLS_FREE(data);
        if (cert) {
            if ((cert) && (cert->priv) && (cert->priv_len)) {
#ifdef TLS_ECDSA_SUPPORTED
                if (cert->ec_algorithm) {
                    DEBUG_PRINT("Loaded ECC private key\n");
                    if (context->ec_private_key)
                        tls_destroy_certificate(context->ec_private_key);
                    context->ec_private_key = cert;
                    return 1;
                } else
#endif
                {
                    DEBUG_PRINT("Loaded private key\n");
                    if (context->private_key)
                        tls_destroy_certificate(context->private_key);
                    context->private_key = cert;
                    return 1;
                }
            }
            tls_destroy_certificate(cert);
        }
    } while (1);
    return 0;
}

struct TLSPacket *tls_build_certificate(struct TLSContext *context) {
    int i;
    unsigned int all_certificate_size = 0;
    int certificates_count;
    struct TLSCertificate **certificates;
    if (context->is_server) {
        certificates_count = context->certificates_count;
        certificates = context->certificates;
    } else {
        certificates_count = context->client_certificates_count;
        certificates = context->client_certificates;
    }
    
#ifdef TLS_ECDSA_SUPPORTED
    int is_ecdsa = tls_is_ecdsa(context);
    if (is_ecdsa) {
        for (i = 0; i < certificates_count; i++) {
            struct TLSCertificate *cert = certificates[i];
            if ((cert) && (cert->der_len) && (cert->ec_algorithm))
                all_certificate_size += cert->der_len + 3;
        }
    } else {
        for (i = 0; i < certificates_count; i++) {
            struct TLSCertificate *cert = certificates[i];
            if ((cert) && (cert->der_len) && (!cert->ec_algorithm))
                all_certificate_size += cert->der_len + 3;
        }
    }
#else
    for (i = 0; i < certificates_count; i++) {
        struct TLSCertificate *cert = certificates[i];
        if ((cert) && (cert->der_len))
            all_certificate_size += cert->der_len + 3;
    }
#endif
    if (!all_certificate_size) {
        DEBUG_PRINT("NO CERTIFICATE SET\n");
        return NULL;
    }
    struct TLSPacket *packet = tls_create_packet(context, TLS_HANDSHAKE, context->version, 0);
    tls_packet_uint8(packet, 0x0B);
    if (all_certificate_size) {
        tls_packet_uint24(packet, all_certificate_size + 3);

        if (context->dtls)
            __private_dtls_handshake_data(context, packet, all_certificate_size + 3);

        tls_packet_uint24(packet, all_certificate_size);
        for (i = 0; i < certificates_count; i++) {
            struct TLSCertificate *cert = certificates[i];
            if ((cert) && (cert->der_len)) {
#ifdef TLS_ECDSA_SUPPORTED
                // is RSA certificate ?
                if ((is_ecdsa) && (!cert->ec_algorithm))
                    continue;
                // is ECC certificate ?
                if ((!is_ecdsa) && (cert->ec_algorithm))
                    continue;
#endif
                // 2 times -> one certificate
                tls_packet_uint24(packet, cert->der_len);
                tls_packet_append(packet, cert->der_bytes, cert->der_len);
            }
        }
    } else {
        tls_packet_uint24(packet, all_certificate_size);
        if (context->dtls)
            __private_dtls_handshake_data(context, packet, all_certificate_size);
    }
    tls_packet_update(packet);
    if (context->dtls)
        context->dtls_seq++;
    return packet;
}

struct TLSPacket *tls_build_finished(struct TLSContext *context) {
    struct TLSPacket *packet = tls_create_packet(context, TLS_HANDSHAKE, context->version, __TLS_MIN_FINISHED_OPAQUE_LEN + 64);
    tls_packet_uint8(packet, 0x14);
    tls_packet_uint24(packet, __TLS_MIN_FINISHED_OPAQUE_LEN);
    if (context->dtls)
        __private_dtls_handshake_data(context, packet, __TLS_MIN_FINISHED_OPAQUE_LEN);
    // verify
    unsigned char hash[__TLS_MAX_HASH_SIZE];
    unsigned char out[__TLS_MIN_FINISHED_OPAQUE_LEN];
    unsigned int hash_len;
    
    // server verifies client's message
    if (context->is_server) {
        hash_len = __private_tls_done_hash(context, hash);
        __private_tls_prf(context, out, __TLS_MIN_FINISHED_OPAQUE_LEN, context->master_key, context->master_key_len, (unsigned char *)"server finished", 15, hash, hash_len, NULL, 0);
        __private_tls_destroy_hash(context);
    } else {
        hash_len = __private_tls_get_hash(context, hash);
        __private_tls_prf(context, out, __TLS_MIN_FINISHED_OPAQUE_LEN, context->master_key, context->master_key_len, (unsigned char *)"client finished", 15, hash, hash_len, NULL, 0);
    }
    tls_packet_append(packet, out, __TLS_MIN_FINISHED_OPAQUE_LEN);
    tls_packet_update(packet);
    DEBUG_DUMP_HEX_LABEL("VERIFY DATA", out, __TLS_MIN_FINISHED_OPAQUE_LEN);
#ifdef TLS_ACCEPT_SECURE_RENEGOTIATION
    if (context->is_server) {
        // concatenate client verify and server verify
        context->verify_data = (unsigned char *)TLS_REALLOC(context->verify_data, __TLS_MIN_FINISHED_OPAQUE_LEN);
        if (context->verify_data) {
            memcpy(context->verify_data + context->verify_len, out, __TLS_MIN_FINISHED_OPAQUE_LEN);
            context->verify_len += __TLS_MIN_FINISHED_OPAQUE_LEN;
        } else
            context->verify_len = 0;
    } else {
        TLS_FREE(context->verify_data);
        context->verify_data = (unsigned char *)TLS_MALLOC(__TLS_MIN_FINISHED_OPAQUE_LEN);
        if (context->verify_data) {
            memcpy(context->verify_data, out, __TLS_MIN_FINISHED_OPAQUE_LEN);
            context->verify_len = __TLS_MIN_FINISHED_OPAQUE_LEN;
        }
    }
#endif
    return packet;
}

struct TLSPacket *tls_build_change_cipher_spec(struct TLSContext *context) {
    struct TLSPacket *packet = tls_create_packet(context, TLS_CHANGE_CIPHER, context->version, 64);
    tls_packet_uint8(packet, 1);
    tls_packet_update(packet);
    context->local_sequence_number = 0;
    return packet;
}

struct TLSPacket *tls_build_done(struct TLSContext *context) {
    struct TLSPacket *packet = tls_create_packet(context, TLS_HANDSHAKE, context->version, 0);
    tls_packet_uint8(packet, 0x0E);
    tls_packet_uint24(packet, 0);
    if (context->dtls) {
        __private_dtls_handshake_data(context, packet, 0);
        context->dtls_seq++;
    }
    tls_packet_update(packet);
    return packet;
}

struct TLSPacket *tls_build_message(struct TLSContext *context, unsigned char *data, unsigned int len) {
    if ((!data) || (!len))
        return 0;
    struct TLSPacket *packet = tls_create_packet(context, TLS_APPLICATION_DATA, context->version, len);
    tls_packet_append(packet, data, len);
    tls_packet_update(packet);
    return packet;
}

int tls_client_connect(struct TLSContext *context) {
    if ((context->is_server) || (context->critical_error))
        return TLS_UNEXPECTED_MESSAGE;
    
    return __private_tls_write_packet(tls_build_hello(context));
}

int tls_write(struct TLSContext *context, unsigned char *data, unsigned int len) {
    if (!context)
        return TLS_GENERIC_ERROR;
    if (context->connection_status != 0xFF)
        return TLS_UNEXPECTED_MESSAGE;
    if (len > __TLS_MAX_TLS_APP_SIZE)
        len = __TLS_MAX_TLS_APP_SIZE;
    int actually_written = __private_tls_write_packet(tls_build_message(context, data, len));
    if (actually_written <= 0)
        return actually_written;
    return len;
}

struct TLSPacket *tls_build_alert(struct TLSContext *context, char critical, unsigned char code) {
    struct TLSPacket *packet = tls_create_packet(context, TLS_ALERT, context->version, 0);
    tls_packet_uint8(packet, critical ? TLS_ALERT_CRITICAL : TLS_ALERT_WARNING);
    if (critical)
        context->critical_error = 1;
    tls_packet_uint8(packet, code);
    tls_packet_update(packet);
    return packet;
}

int __private_tls_read_from_file(const char *fname, void *buf, int max_len) {
    FILE *f = fopen(fname, "rb");
    if (f) {
        int size = fread(buf, 1, max_len, f);
        fclose(f);
        return size;
    }
    return 0;
}

int tls_consume_stream(struct TLSContext *context, const unsigned char *buf, int buf_len, tls_validation_function certificate_verify) {
    if ((buf_len <= 0) || (!buf)) {
        context->critical_error = 1;
        return TLS_NO_MEMORY;
    }
    if (!context)
        return TLS_NO_MEMORY;
    if (context->critical_error)
        return TLS_BROKEN_CONNECTION;
    unsigned int orig_len = context->message_buffer_len;
    context->message_buffer_len += buf_len;
    context->message_buffer = (unsigned char *)TLS_REALLOC(context->message_buffer, context->message_buffer_len);
    if (!context->message_buffer) {
        context->message_buffer_len = 0;
        return TLS_NO_MEMORY;
    }
    memcpy(context->message_buffer + orig_len, buf, buf_len);
    unsigned int index = 0;
    unsigned int tls_buffer_len = context->message_buffer_len;
    int err_flag = 0;
    
    int tls_header_size;
    int tls_size_offset;

    if (context->dtls) {
        tls_size_offset = 11;
        tls_header_size = 13;
    } else {
        tls_size_offset = 3;
        tls_header_size = 5;
    }
    while (tls_buffer_len >= 5) {
        unsigned int length = ntohs(*(unsigned short *)&context->message_buffer[index + tls_size_offset]) + tls_header_size;
        if (length > tls_buffer_len) {
            DEBUG_PRINT("NEED DATA: %i/%i\n", length, tls_buffer_len);
            break;
        }
        int consumed = tls_parse_message(context, &context->message_buffer[index], length, certificate_verify);
        DEBUG_PRINT("Consumed %i bytes\n", consumed);
        if (consumed < 0) {
            if (!context->critical_error)
                context->critical_error = 1;
            err_flag = consumed;
            break;
        }
        index += length;
        tls_buffer_len -= length;
        if (context->critical_error) {
            err_flag = TLS_BROKEN_CONNECTION;
            break;
        }
    }
    if (err_flag) {
        DEBUG_PRINT("ERROR IN CONSUME: %i\n", err_flag);
        context->message_buffer_len = 0;
        TLS_FREE(context->message_buffer);
        context->message_buffer = NULL;
        return err_flag;
    }
    if (index) {
        context->message_buffer_len -= index;
        if (context->message_buffer_len) {
            // no realloc here
            memmove(context->message_buffer, context->message_buffer + index, context->message_buffer_len);
        } else {
            TLS_FREE(context->message_buffer);
            context->message_buffer = NULL;
        }
    }
    return index;
}

void tls_close_notify(struct TLSContext *context) {
    if ((!context) || (context->critical_error))
        return;
    context->critical_error = 1;
    DEBUG_PRINT("CLOSE\n");
    __private_tls_write_packet(tls_build_alert(context, 1, close_notify));
}

void tls_alert(struct TLSContext *context, unsigned char critical, int code) {
    if (!context)
        return;
    if ((!context->critical_error) && (critical))
        context->critical_error = 1;
    DEBUG_PRINT("ALERT\n");
    __private_tls_write_packet(tls_build_alert(context, critical, code));
}

int tls_pending(struct TLSContext *context) {
    if (!context->message_buffer)
        return 0;
    return context->message_buffer_len;
}

void tls_make_exportable(struct TLSContext *context, unsigned char exportable_flag) {
    context->exportable = exportable_flag;
    if (!exportable_flag) {
        // zero the memory
        if ((context->exportable_keys) && (context->exportable_size))
            memset(context->exportable_keys, 0, context->exportable_size);
        // free the memory, if alocated
        TLS_FREE(context->exportable_keys);
        context->exportable_size = 0;
    }
}

int tls_export_context(struct TLSContext *context, unsigned char *buffer, unsigned int buf_len, unsigned char small_version) {
    // only negotiated AND exportable connections may be exported
    if ((!context) || (context->critical_error) || (context->connection_status != 0xFF) || (!context->exportable) || (!context->exportable_keys) || (!context->exportable_size) || (!context->crypto.created)) {
        DEBUG_PRINT("CANNOT EXPORT CONTEXT %i\n", (int)context->connection_status);
        return 0;
    }
    
    struct TLSPacket *packet = tls_create_packet(NULL, TLS_SERIALIZED_OBJECT, context->version, 0);
    // export buffer version
    tls_packet_uint8(packet, 0x01);
    tls_packet_uint8(packet, context->connection_status);
    tls_packet_uint16(packet, context->cipher);
    if (context->is_child)
        tls_packet_uint8(packet, 2);
    else
        tls_packet_uint8(packet, context->is_server);
    
    if (context->crypto.created == 2) {
        // aead
        tls_packet_uint8(packet, __TLS_AES_GCM_IV_LENGTH);
        tls_packet_append(packet, context->crypto.ctx_local_mac.local_aead_iv, __TLS_AES_GCM_IV_LENGTH);
        tls_packet_append(packet, context->crypto.ctx_remote_mac.remote_aead_iv, __TLS_AES_GCM_IV_LENGTH);
#ifdef TLS_WITH_CHACHA20_POLY1305
    } else
    if (context->crypto.created == 3) {
        // ChaCha20
        tls_packet_uint8(packet, __TLS_CHACHA20_IV_LENGTH);
        tls_packet_append(packet, context->crypto.ctx_local_mac.local_nonce, __TLS_CHACHA20_IV_LENGTH);
        tls_packet_append(packet, context->crypto.ctx_remote_mac.remote_nonce, __TLS_CHACHA20_IV_LENGTH);
#endif
    } else {
        unsigned char iv[__TLS_AES_IV_LENGTH];
        unsigned long len = __TLS_AES_IV_LENGTH;
        
        memset(iv, 0, __TLS_AES_IV_LENGTH);
        cbc_getiv(iv, &len, &context->crypto.ctx_local.aes_local);
        tls_packet_uint8(packet, __TLS_AES_IV_LENGTH);
        tls_packet_append(packet, iv, len);
        
        memset(iv, 0, __TLS_AES_IV_LENGTH);
        cbc_getiv(iv, &len, &context->crypto.ctx_remote.aes_remote);
        tls_packet_append(packet, iv, __TLS_AES_IV_LENGTH);
    }
    
    tls_packet_uint8(packet, context->exportable_size);
    tls_packet_append(packet, context->exportable_keys, context->exportable_size);
    
    if (context->crypto.created == 2) {
        tls_packet_uint8(packet, 0);
#ifdef TLS_WITH_CHACHA20_POLY1305
    } else
    if (context->crypto.created == 3) {
        // ChaCha20
        tls_packet_uint8(packet, 0);
        unsigned int i;
        for (i = 0; i < 16; i++)
            tls_packet_uint32(packet, context->crypto.ctx_local.chacha_local.input[i]);
        for (i = 0; i < 16; i++)
            tls_packet_uint32(packet, context->crypto.ctx_remote.chacha_remote.input[i]);
        tls_packet_append(packet, context->crypto.ctx_local.chacha_local.ks, CHACHA_BLOCKLEN);
        tls_packet_append(packet, context->crypto.ctx_remote.chacha_remote.ks, CHACHA_BLOCKLEN);
#endif
    } else {
        unsigned char mac_length = (unsigned char)__private_tls_mac_length(context);
        tls_packet_uint8(packet, mac_length);
        tls_packet_append(packet, context->crypto.ctx_local_mac.local_mac, mac_length);
        tls_packet_append(packet, context->crypto.ctx_remote_mac.remote_mac, mac_length);
    }
    
    if (small_version) {
        tls_packet_uint16(packet, 0);
    } else {
        tls_packet_uint16(packet, context->master_key_len);
        tls_packet_append(packet, context->master_key, context->master_key_len);
    }
    
    uint64_t sequence_number = htonll(context->local_sequence_number);
    tls_packet_append(packet, (unsigned char *)&sequence_number, sizeof(uint64_t));
    sequence_number = htonll(context->remote_sequence_number);
    tls_packet_append(packet, (unsigned char *)&sequence_number, sizeof(uint64_t));
    
    tls_packet_uint32(packet, context->tls_buffer_len);
    tls_packet_append(packet, context->tls_buffer, context->tls_buffer_len);
    
    tls_packet_uint32(packet, context->message_buffer_len);
    tls_packet_append(packet, context->message_buffer, context->message_buffer_len);
    
    tls_packet_uint32(packet, context->application_buffer_len);
    tls_packet_append(packet, context->application_buffer, context->application_buffer_len);
    tls_packet_uint8(packet, context->dtls);
    if (context->dtls) {
        tls_packet_uint16(packet, context->dtls_epoch_local);
        tls_packet_uint16(packet, context->dtls_epoch_remote);
    }
    tls_packet_update(packet);
    unsigned int size = packet->len;
    if ((buffer) && (buf_len)) {
        if (size > buf_len) {
            tls_destroy_packet(packet);
            DEBUG_PRINT("EXPORT BUFFER TO SMALL\n");
            return (int)buf_len - (int)size;
        }
        memcpy(buffer, packet->buf, size);
    }
    tls_destroy_packet(packet);
    return size;
}

struct TLSContext *tls_import_context(unsigned char *buffer, unsigned int buf_len) {
    if ((!buffer) || (buf_len < 64) || (buffer[0] != TLS_SERIALIZED_OBJECT) || (buffer[5] != 0x01)) {
        DEBUG_PRINT("CANNOT IMPORT CONTEXT BUFFER\n");
        return NULL;
    }
    // create a context object
    struct TLSContext *context = tls_create_context(0, TLS_V12);
    if (context) {
        unsigned char temp[0xFF];
        context->version = ntohs(*(unsigned short *)&buffer[1]);
        unsigned short length = ntohs(*(unsigned short *)&buffer[3]);
        if (length != buf_len - 5) {
            DEBUG_PRINT("INVALID IMPORT BUFFER SIZE\n");
            tls_destroy_context(context);
            return NULL;
        }
        context->connection_status = buffer[6];
        context->cipher = ntohs(*(unsigned short *)&buffer[7]);
        unsigned char server = buffer[9];
        if (server == 2) {
            context->is_server = 1;
            context->is_child = 1;
        } else
            context->is_server = server;
        
        unsigned char local_iv[__TLS_AES_IV_LENGTH];
        unsigned char remote_iv[__TLS_AES_IV_LENGTH];
        unsigned char iv_len = buffer[10];
        if (iv_len >  __TLS_AES_IV_LENGTH) {
            DEBUG_PRINT("INVALID IV LENGTH\n");
            tls_destroy_context(context);
            return NULL;
        }
        
        // get the initialization vectors
        int buf_pos = 11;
        memcpy(local_iv, &buffer[buf_pos], iv_len);
        buf_pos += iv_len;
        memcpy(remote_iv, &buffer[buf_pos], iv_len);
        buf_pos += iv_len;
        
        unsigned char key_lengths = buffer[buf_pos++];
        TLS_IMPORT_CHECK_SIZE(buf_pos, key_lengths, buf_len)
        memcpy(temp, &buffer[buf_pos], key_lengths);
        buf_pos += key_lengths;
        
        int is_aead = __private_tls_is_aead(context);
#ifdef TLS_WITH_CHACHA20_POLY1305
        if (is_aead == 2) {
            // ChaCha20
            if (iv_len > __TLS_CHACHA20_IV_LENGTH)
                iv_len = __TLS_CHACHA20_IV_LENGTH;
            memcpy(context->crypto.ctx_local_mac.local_nonce, local_iv, iv_len);
            memcpy(context->crypto.ctx_remote_mac.remote_nonce, remote_iv, iv_len);
        } else
#endif
        if (is_aead) {
            if (iv_len > __TLS_AES_GCM_IV_LENGTH)
                iv_len = __TLS_AES_GCM_IV_LENGTH;
            memcpy(context->crypto.ctx_local_mac.local_aead_iv, local_iv, iv_len);
            memcpy(context->crypto.ctx_remote_mac.remote_aead_iv, remote_iv, iv_len);
        }
        if (context->is_server) {
            if (__private_tls_crypto_create(context, key_lengths / 2, iv_len, temp, local_iv, temp + key_lengths / 2, remote_iv)) {
                DEBUG_PRINT("ERROR CREATING KEY CONTEXT\n");
                tls_destroy_context(context);
                return NULL;
            }
        } else {
            if (__private_tls_crypto_create(context, key_lengths / 2, iv_len, temp + key_lengths / 2, remote_iv, temp, local_iv)) {
                DEBUG_PRINT("ERROR CREATING KEY CONTEXT (CLIENT)\n");
                tls_destroy_context(context);
                return NULL;
            }
        }
        memset(temp, 0, sizeof(temp));
        
        unsigned char mac_length = buffer[buf_pos++];
        if (mac_length > __TLS_MAX_MAC_SIZE) {
            DEBUG_PRINT("INVALID MAC SIZE\n");
            tls_destroy_context(context);
            return NULL;
        }
        
        if (mac_length) {
            TLS_IMPORT_CHECK_SIZE(buf_pos, mac_length, buf_len)
            memcpy(context->crypto.ctx_local_mac.local_mac, &buffer[buf_pos], mac_length);
            buf_pos += mac_length;
            
            TLS_IMPORT_CHECK_SIZE(buf_pos, mac_length, buf_len)
            memcpy(context->crypto.ctx_remote_mac.remote_mac, &buffer[buf_pos], mac_length);
            buf_pos += mac_length;
        } else
#ifdef TLS_WITH_CHACHA20_POLY1305
        if (is_aead == 2) {
            // ChaCha20
            unsigned int i;
            TLS_IMPORT_CHECK_SIZE(buf_pos, 128 + CHACHA_BLOCKLEN * 2, buf_len)
            for (i = 0; i < 16; i++) {
                context->crypto.ctx_local.chacha_local.input[i] = ntohl(*(unsigned int *)&buffer[buf_pos]);
                buf_pos += sizeof(unsigned int);
            }
            for (i = 0; i < 16; i++) {
                context->crypto.ctx_remote.chacha_remote.input[i] = ntohl(*(unsigned int *)&buffer[buf_pos]);
                buf_pos += sizeof(unsigned int);
            }
            memcpy(context->crypto.ctx_local.chacha_local.ks, &buffer[buf_pos], CHACHA_BLOCKLEN);
            buf_pos += CHACHA_BLOCKLEN;
            memcpy(context->crypto.ctx_remote.chacha_remote.ks, &buffer[buf_pos], CHACHA_BLOCKLEN);
            buf_pos += CHACHA_BLOCKLEN;
        }
#endif
        
        TLS_IMPORT_CHECK_SIZE(buf_pos, 2, buf_len)
        unsigned short master_key_len = ntohs(*(unsigned short *)&buffer[buf_pos]);
        buf_pos += 2;
        if (master_key_len) {
            TLS_IMPORT_CHECK_SIZE(buf_pos, master_key_len, buf_len)
            context->master_key = (unsigned char *)TLS_MALLOC(master_key_len);
            if (context->master_key) {
                memcpy(context->master_key, &buffer[buf_pos], master_key_len);
                context->master_key_len = master_key_len;
            }
            buf_pos += master_key_len;
        }
        
        TLS_IMPORT_CHECK_SIZE(buf_pos, 16, buf_len)
        
        context->local_sequence_number = ntohll(*(uint64_t *)&buffer[buf_pos]);
        buf_pos += 8;
        context->remote_sequence_number = ntohll(*(uint64_t *)&buffer[buf_pos]);
        buf_pos += 8;
        
        TLS_IMPORT_CHECK_SIZE(buf_pos, 4, buf_len)
        unsigned int tls_buffer_len = ntohl(*(unsigned int *)&buffer[buf_pos]);
        buf_pos += 4;
        TLS_IMPORT_CHECK_SIZE(buf_pos, tls_buffer_len, buf_len)
        if (tls_buffer_len) {
            context->tls_buffer = (unsigned char *)TLS_MALLOC(tls_buffer_len);
            if (context->tls_buffer) {
                memcpy(context->tls_buffer, &buffer[buf_pos], tls_buffer_len);
                context->tls_buffer_len = tls_buffer_len;
            }
            buf_pos += tls_buffer_len;
        }
        
        TLS_IMPORT_CHECK_SIZE(buf_pos, 4, buf_len)
        unsigned int message_buffer_len = ntohl(*(unsigned int *)&buffer[buf_pos]);
        buf_pos += 4;
        TLS_IMPORT_CHECK_SIZE(buf_pos, message_buffer_len, buf_len)
        if (message_buffer_len) {
            context->message_buffer = (unsigned char *)TLS_MALLOC(message_buffer_len);
            if (context->message_buffer) {
                memcpy(context->message_buffer, &buffer[buf_pos], message_buffer_len);
                context->message_buffer_len = message_buffer_len;
            }
            buf_pos += message_buffer_len;
        }
        
        TLS_IMPORT_CHECK_SIZE(buf_pos, 4, buf_len)
        unsigned int application_buffer_len = ntohl(*(unsigned int *)&buffer[buf_pos]);
        buf_pos += 4;
        context->cipher_spec_set = 1;
        TLS_IMPORT_CHECK_SIZE(buf_pos, application_buffer_len, buf_len)
        if (application_buffer_len) {
            context->application_buffer = (unsigned char *)TLS_MALLOC(application_buffer_len);
            if (context->application_buffer) {
                memcpy(context->application_buffer, &buffer[buf_pos], application_buffer_len);
                context->application_buffer_len = application_buffer_len;
            }
            buf_pos += application_buffer_len;
        }
        TLS_IMPORT_CHECK_SIZE(buf_pos, 1, buf_len)
        context->dtls = buffer[buf_pos];
        buf_pos++;
        if (context->dtls) {
            TLS_IMPORT_CHECK_SIZE(buf_pos, 4, buf_len)
            context->dtls_epoch_local = ntohs(*(unsigned short *)&buffer[buf_pos]);
            buf_pos += 2;
            context->dtls_epoch_remote = ntohs(*(unsigned short *)&buffer[buf_pos]);
        }
    }
    return context;
}

int tls_is_broken(struct TLSContext *context) {
    if ((!context) || (context->critical_error))
        return 1;
    return 0;
}

int tls_request_client_certificate(struct TLSContext *context) {
    if ((!context) || (!context->is_server))
        return 0;
    
    context->request_client_certificate = 1;
    return 1;
}

int tls_client_verified(struct TLSContext *context) {
    if ((!context) || (context->critical_error))
        return 0;
    
    return (context->client_verified == 1);
}

const char *tls_sni(struct TLSContext *context) {
    if (!context)
        return NULL;
    return context->sni;
}

int tls_sni_set(struct TLSContext *context, const char *sni) {
    if ((!context) || (context->is_server) || (context->critical_error) || (context->connection_status != 0))
        return 0;
    TLS_FREE(context->sni);
    context->sni = NULL;
    if (sni) {
        int len = strlen(sni);
        if (len > 0) {
            context->sni = (char *)TLS_MALLOC(len + 1);
            if (context->sni) {
                context->sni[len] = 0;
                memcpy(context->sni, sni, len);
                return 1;
            }
        }
    }
    return 0;
}

int tls_load_root_certificates(struct TLSContext *context, const unsigned char *pem_buffer, int pem_size) {
    if (!context)
        return TLS_GENERIC_ERROR;
    
    unsigned int len;
    int idx = 0;
    
    do {
        unsigned char *data = tls_pem_decode(pem_buffer, pem_size, idx++, &len);
        if ((!data) || (!len))
            break;
        struct TLSCertificate *cert = asn1_parse(NULL, data, len, 0);
        if (cert) {
            if (cert->version == 2) {
                if (cert->priv) {
                    DEBUG_PRINT("WARNING - parse error (private key encountered in certificate)\n");
                    TLS_FREE(cert->priv);
                    cert->priv = NULL;
                    cert->priv_len = 0;
                }
                context->root_certificates = (struct TLSCertificate **)TLS_REALLOC(context->root_certificates, (context->root_count + 1) * sizeof(struct TLSCertificate));
                if (!context->root_certificates) {
                    context->root_count = 0;
                    return TLS_GENERIC_ERROR;
                }
                context->root_certificates[context->root_count] = cert;
                context->root_count++;
                DEBUG_PRINT("Loaded certificate: %i\n", (int)context->root_count);
            } else {
                DEBUG_PRINT("WARNING - certificate version error (v%i)\n", (int)cert->version);
                tls_destroy_certificate(cert);
            }
        }
        TLS_FREE(data);
    } while (1);
    return context->root_count;
}

int tls_default_verify(struct TLSContext *context, struct TLSCertificate **certificate_chain, int len) {
    int i;
    int err;
    
    if (certificate_chain) {
        for (i = 0; i < len; i++) {
            struct TLSCertificate *certificate = certificate_chain[i];
            // check validity date
            err = tls_certificate_is_valid(certificate);
            if (err)
                return err;
        }
    }
    // check if chain is valid
    err = tls_certificate_chain_is_valid(certificate_chain, len);
    if (err)
        return err;
    
    // check certificate subject
    if ((!context->is_server) && (context->sni) && (len > 0) && (certificate_chain)) {
        err = tls_certificate_valid_subject(certificate_chain[0], context->sni);
        if (err)
            return err;
    }
    
    err = tls_certificate_chain_is_valid_root(context, certificate_chain, len);
    if (err)
        return err;
    
    DEBUG_PRINT("Certificate OK\n");
    return no_error;
}

#ifdef DEBUG
void tls_print_certificate(const char *fname) {
    unsigned char buf[0xFFFF];
    char out_buf[0xFFFF];
    int size = __private_tls_read_from_file(fname, buf, 0xFFFF);
    if (size > 0) {
        int idx = 0;
        unsigned int len;
        do {
            unsigned char *data;
            if (buf[0] == '-')  {
                data = tls_pem_decode(buf, size, idx++, &len);
            } else {
                data = buf;
                len = size;
            }
            if ((!data) || (!len))
                return;
            struct TLSCertificate *cert = asn1_parse(NULL, data, len, -1);
            if (data != buf)
                TLS_FREE(data);
            if (cert) {
                fprintf(stderr, "%s", tls_certificate_to_string(cert, out_buf, 0xFFFF));
                tls_destroy_certificate(cert);
            }
            if (data == buf)
                break;
        } while (1);
    }
}
#endif

#ifdef SSL_COMPATIBLE_INTERFACE

int  SSL_library_init() {
    // dummy function
    return 1;
}

void SSL_load_error_strings() {
    // dummy function
}

void OpenSSL_add_all_algorithms() {
    // dummy function
}

void OpenSSL_add_all_ciphers() {
    // dummy function
}

void OpenSSL_add_all_digests() {
    // dummy function
}

void EVP_cleanup() {
    // dummy function
}

int __tls_ssl_private_send_pending(int client_sock, struct TLSContext *context) {
    unsigned int out_buffer_len = 0;
    const unsigned char *out_buffer = tls_get_write_buffer(context, &out_buffer_len);
    unsigned int out_buffer_index = 0;
    int send_res = 0;
    SOCKET_SEND_CALLBACK write_cb = NULL;
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if (ssl_data)
        write_cb = (SOCKET_SEND_CALLBACK)ssl_data->send;
    while ((out_buffer) && (out_buffer_len > 0)) {
        int res;
        if (write_cb)
            res = write_cb(client_sock, (char *)&out_buffer[out_buffer_index], out_buffer_len, 0);
        else
            res = send(client_sock, (char *)&out_buffer[out_buffer_index], out_buffer_len, 0);
        if (res <= 0) {
            send_res = res;
            break;
        }
        out_buffer_len -= res;
        out_buffer_index += res;
        send_res += res;
    }
    tls_buffer_clear(context);
    return send_res;
}

struct TLSContext *SSL_new(struct TLSContext *context) {
    return tls_accept(context);
}

int SSLv3_server_method() {
    return 1;
}

int SSLv3_client_method() {
    return 0;
}

int SSL_CTX_use_certificate_file(struct TLSContext *context, const char *filename, int dummy) {
    // max 64k buffer
    unsigned char buf[0xFFFF];
    int size = __private_tls_read_from_file(filename, buf, sizeof(buf));
    if (size > 0)
        return tls_load_certificates(context, buf, size);
    return size;
}

int SSL_CTX_use_PrivateKey_file(struct TLSContext *context, const char *filename, int dummy) {
    unsigned char buf[0xFFFF];
    int size = __private_tls_read_from_file(filename, buf, sizeof(buf));
    if (size > 0)
        return tls_load_private_key(context, buf, size);
    
    return size;
}

int SSL_CTX_check_private_key(struct TLSContext *context) {
    if ((!context) || (!context->private_key) || (!context->private_key->der_bytes) || (!context->private_key->der_len))
        return 0;
    return 1;
}

struct TLSContext *SSL_CTX_new(int method) {
    return tls_create_context(method, TLS_V12);
}

void SSL_free(struct TLSContext *context) {
    if (context) {
        TLS_FREE(context->user_data);
        tls_destroy_context(context);
    }
}

void SSL_CTX_free(struct TLSContext *context) {
    SSL_free(context);
}

int SSL_get_error(struct TLSContext *context, int ret) {
    if (!context)
        return TLS_GENERIC_ERROR;
    return context->critical_error;
}

int SSL_set_fd(struct TLSContext *context, int socket) {
    if (!context)
        return TLS_GENERIC_ERROR;
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if (!ssl_data) {
        ssl_data = (SSLUserData *)TLS_MALLOC(sizeof(SSLUserData));
        if (!ssl_data)
            return TLS_NO_MEMORY;
        memset(ssl_data, 0, sizeof(SSLUserData));
        context->user_data = ssl_data;
    }
    ssl_data->fd = socket;
    return 0;
}

void *SSL_set_userdata(struct TLSContext *context, void *data) {
    if (!context)
        return NULL;
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if (!ssl_data) {
        ssl_data = (SSLUserData *)TLS_MALLOC(sizeof(SSLUserData));
        if (!ssl_data)
            return NULL;
        memset(ssl_data, 0, sizeof(SSLUserData));
        context->user_data = ssl_data;
    }
    void *old_data = ssl_data->user_data;
    ssl_data->user_data = data;
    return old_data;
}

void *SSL_userdata(struct TLSContext *context) {
    if (!context)
        return NULL;
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if (!ssl_data)
        return NULL;
    
    return ssl_data->user_data;
}

int SSL_CTX_root_ca(struct TLSContext *context, const char *pem_filename) {
    if (!context)
        return TLS_GENERIC_ERROR;
    
    int count = TLS_GENERIC_ERROR;
    FILE *f = fopen(pem_filename, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size) {
            unsigned char *buf = (unsigned char *)TLS_MALLOC(size + 1);
            if (buf) {
                buf[size] = 1;
                if (fread(buf, 1, size, f) == size) {
                    count = tls_load_root_certificates(context, buf, size);
                    if (count > 0) {
                        SSLUserData *ssl_data = (SSLUserData *)context->user_data;
                        if (!ssl_data) {
                            ssl_data = (SSLUserData *)TLS_MALLOC(sizeof(SSLUserData));
                            if (!ssl_data)
                                return TLS_NO_MEMORY;
                            memset(ssl_data, 0, sizeof(SSLUserData));
                            context->user_data = ssl_data;
                        }
                        if (!ssl_data->certificate_verify)
                            ssl_data->certificate_verify = tls_default_verify;
                    }
                }
            }
        }
        fclose(f);
    }
    return count;
}

void SSL_CTX_set_verify(struct TLSContext *context, int mode, tls_validation_function verify_callback) {
    if (!context)
        return;
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if (!ssl_data) {
        ssl_data = (SSLUserData *)TLS_MALLOC(sizeof(SSLUserData));
        if (!ssl_data)
            return;
        memset(ssl_data, 0, sizeof(SSLUserData));
        context->user_data = ssl_data;
    }
    if (mode == SSL_VERIFY_NONE)
        ssl_data->certificate_verify = NULL;
    else
        ssl_data->certificate_verify = verify_callback;
}

int __private_tls_safe_read(struct TLSContext *context, void *buffer, int buf_size) {
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if ((!ssl_data) || (ssl_data->fd <= 0))
        return TLS_GENERIC_ERROR;

    SOCKET_RECV_CALLBACK read_cb = (SOCKET_RECV_CALLBACK)ssl_data->recv;
    if (read_cb)
        return read_cb(ssl_data->fd, (char *)buffer, buf_size, 0);
    return recv(ssl_data->fd, (char *)buffer, buf_size, 0);
}

int SSL_accept(struct TLSContext *context) {
    if (!context)
        return TLS_GENERIC_ERROR;
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if ((!ssl_data) || (ssl_data->fd <= 0))
        return TLS_GENERIC_ERROR;
    unsigned char client_message[0xFFFF];
    // accept
    int read_size;
    while ((read_size = __private_tls_safe_read(context, (char *)client_message, sizeof(client_message)))) {
        if (tls_consume_stream(context, client_message, read_size, ssl_data->certificate_verify) >= 0) {
            int res = __tls_ssl_private_send_pending(ssl_data->fd, context);
            if (res < 0)
                return res;
        }
        if (tls_established(context))
            return 1;
    }
    return 0;
}

int SSL_connect(struct TLSContext *context) {
    if (!context)
        return TLS_GENERIC_ERROR;
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if ((!ssl_data) || (ssl_data->fd <= 0) || (context->critical_error))
        return TLS_GENERIC_ERROR;
    int res = tls_client_connect(context);
    if (res < 0)
        return res;
    res = __tls_ssl_private_send_pending(ssl_data->fd, context);
    if (res < 0)
        return res;
    
    int read_size;
    unsigned char client_message[0xFFFF];

    while ((read_size = __private_tls_safe_read(context, (char *)client_message, sizeof(client_message)))) {
        if (tls_consume_stream(context, client_message, read_size, ssl_data->certificate_verify) >= 0) {
            res = __tls_ssl_private_send_pending(ssl_data->fd, context);
            if (res < 0)
                return res;
        }
        if (tls_established(context))
            return 1;
        if (context->critical_error)
            return TLS_GENERIC_ERROR;
    }
    return read_size;
}

int SSL_shutdown(struct TLSContext *context) {
    if (!context)
        return TLS_GENERIC_ERROR;
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if ((!ssl_data) || (ssl_data->fd <= 0))
        return TLS_GENERIC_ERROR;
    
    tls_close_notify(context);
    return 0;
}

int SSL_write(struct TLSContext *context, void *buf, unsigned int len) {
    if (!context)
        return TLS_GENERIC_ERROR;
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if ((!ssl_data) || (ssl_data->fd <= 0))
        return TLS_GENERIC_ERROR;
    
    int written_size = tls_write(context, (unsigned char *)buf, len);
    if (written_size > 0) {
        int res = __tls_ssl_private_send_pending(ssl_data->fd, context);
        if (res <= 0)
            return res;
    }
    return written_size;
}

int SSL_read(struct TLSContext *context, void *buf, unsigned int len) {
    if (!context)
        return TLS_GENERIC_ERROR;
    
    if (context->application_buffer_len)
        return tls_read(context, (unsigned char *)buf, len);

    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if ((!ssl_data) || (ssl_data->fd <= 0) || (context->critical_error))
        return TLS_GENERIC_ERROR;
    if (tls_established(context) != 1)
        return TLS_GENERIC_ERROR;
    
    if (!context->application_buffer_len) {
        unsigned char client_message[0xFFFF];
        // accept
        int read_size;
        while ((read_size = __private_tls_safe_read(context, (char *)client_message, sizeof(client_message)))) {
            if (tls_consume_stream(context, client_message, read_size, ssl_data->certificate_verify) > 0) {
                __tls_ssl_private_send_pending(ssl_data->fd, context);
                break;
            }
            if ((context->critical_error) && (!context->application_buffer_len)) {
                return TLS_GENERIC_ERROR;
            }
        }
        if ((read_size < 0) && (!context->application_buffer_len))
            return read_size;
    }
    
    return tls_read(context, (unsigned char *)buf, len);
}

int SSL_pending(struct TLSContext *context) {
    if (!context)
        return TLS_GENERIC_ERROR;
    return context->application_buffer_len;
}

int SSL_set_io(struct TLSContext *context, void *recv_cb, void *send_cb) {
    if (!context)
        return TLS_GENERIC_ERROR;
    SSLUserData *ssl_data = (SSLUserData *)context->user_data;
    if (!ssl_data) {
        ssl_data = (SSLUserData *)TLS_MALLOC(sizeof(SSLUserData));
        if (!ssl_data)
            return TLS_NO_MEMORY;
        memset(ssl_data, 0, sizeof(SSLUserData));
        context->user_data = ssl_data;
    }
    ssl_data->recv = recv_cb;
    ssl_data->send = send_cb;
    return 0;
}
#endif // SSL_COMPATIBLE_INTERFACE

#endif // TLSE_C

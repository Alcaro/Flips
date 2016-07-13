#ifdef ARLIB_SSL_WOLFSSL_SP
//I'll have to #include the parts of WolfSSL I need
//it's easier in preprocessor than in makefile

#define DEBUG_WOLFSSL

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
//#define NO_WOLFSSL_MEMORY // use malloc like a sane program
//#define NO_WOLFSSL_DIR // we scan the directories ourelves
//#define WOLFSSL_USER_IO // we set our own read/write callbacks

#ifdef _WIN32
#define USE_WINDOWS_API
#else
#define WOLFSSL_PTHREADS
#endif

//#ifndef ARLIB_THREAD
//#define SINGLE_THREADED
//#endif

//got these from ./configure
#define HAVE_THREAD_LS
#define HAVE_AESGCM
#define WOLFSSL_SHA512
#define WOLFSSL_SHA384
#define NO_DSA
#define HAVE_ECC
#define TFM_ECC256
#define ECC_SHAMIR
#define NO_RC4
#define NO_HC128
#define NO_RABBIT
#define HAVE_POLY1305
#define HAVE_ONE_TIME_AUTH
#define HAVE_CHACHA
#define HAVE_HASHDRBG
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES
#define NO_PSK
#define NO_MD4
#define NO_PWDBASED
#define USE_FAST_MATH
#define WOLFSSL_X86_64_BUILD
#define HAVE___UINT128_T

#include "wolfssl-3.9.0/src/crl.c"
#include "wolfssl-3.9.0/src/internal.c"
#define c16toa c16toa_b // these functions are copypasted. should be in a header
#define c32toa c32toa_b
#define ato16 ato16_b
#define c24to32 c24to32_b
#define GetSEQIncrement GetSEQIncrement_b
#include "wolfssl-3.9.0/src/io.c"
#include "wolfssl-3.9.0/src/keys.c"
#include "wolfssl-3.9.0/src/ocsp.c"
#include "wolfssl-3.9.0/src/sniffer.c"
#include "wolfssl-3.9.0/src/ssl.c"
#include "wolfssl-3.9.0/src/tls.c"

#include "wolfssl-3.9.0/wolfcrypt/src/aes.c"
#include "wolfssl-3.9.0/wolfcrypt/src/arc4.c"
//#include "wolfssl-3.9.0/wolfcrypt/src/asm.c"
#include "wolfssl-3.9.0/wolfcrypt/src/asn.c"
#include "wolfssl-3.9.0/wolfcrypt/src/blake2b.c"
#include "wolfssl-3.9.0/wolfcrypt/src/camellia.c"
#include "wolfssl-3.9.0/wolfcrypt/src/chacha20_poly1305.c"
#include "wolfssl-3.9.0/wolfcrypt/src/chacha.c"
#include "wolfssl-3.9.0/wolfcrypt/src/coding.c"
#include "wolfssl-3.9.0/wolfcrypt/src/compress.c"
#include "wolfssl-3.9.0/wolfcrypt/src/curve25519.c"
#include "wolfssl-3.9.0/wolfcrypt/src/des3.c"
#include "wolfssl-3.9.0/wolfcrypt/src/dh.c"
#include "wolfssl-3.9.0/wolfcrypt/src/dsa.c"
#include "wolfssl-3.9.0/wolfcrypt/src/ecc.c"
#include "wolfssl-3.9.0/wolfcrypt/src/ecc_fp.c"
#include "wolfssl-3.9.0/wolfcrypt/src/ed25519.c"
#include "wolfssl-3.9.0/wolfcrypt/src/error.c"
#include "wolfssl-3.9.0/wolfcrypt/src/fe_low_mem.c"
#include "wolfssl-3.9.0/wolfcrypt/src/fe_operations.c"
#include "wolfssl-3.9.0/wolfcrypt/src/ge_low_mem.c"
#include "wolfssl-3.9.0/wolfcrypt/src/ge_operations.c"
#include "wolfssl-3.9.0/wolfcrypt/src/hash.c"
#include "wolfssl-3.9.0/wolfcrypt/src/hc128.c"
#include "wolfssl-3.9.0/wolfcrypt/src/hmac.c"
#include "wolfssl-3.9.0/wolfcrypt/src/idea.c"
#include "wolfssl-3.9.0/wolfcrypt/src/integer.c"
#include "wolfssl-3.9.0/wolfcrypt/src/logging.c"
#include "wolfssl-3.9.0/wolfcrypt/src/md2.c"
#define Transform Transform_md4 // several functions and macros exist multiple times
#define AddLength AddLength_md4
#include "wolfssl-3.9.0/wolfcrypt/src/md4.c"
#undef Transform
#undef AddLength
#define Transform Transform_md5
#define AddLength AddLength_md5
#include "wolfssl-3.9.0/wolfcrypt/src/md5.c"
#undef Transform
#undef AddLength
#undef XTRANSFORM
#include "wolfssl-3.9.0/wolfcrypt/src/memory.c"
#include "wolfssl-3.9.0/wolfcrypt/src/misc.c"
#include "wolfssl-3.9.0/wolfcrypt/src/pkcs7.c"
#include "wolfssl-3.9.0/wolfcrypt/src/poly1305.c"
#undef LO
#include "wolfssl-3.9.0/wolfcrypt/src/pwdbased.c"
#include "wolfssl-3.9.0/wolfcrypt/src/rabbit.c"
#include "wolfssl-3.9.0/wolfcrypt/src/random.c"
#include "wolfssl-3.9.0/wolfcrypt/src/ripemd.c"
#include "wolfssl-3.9.0/wolfcrypt/src/rsa.c"
#define Transform Transform_sha256
#define AddLength AddLength_sha256
#include "wolfssl-3.9.0/wolfcrypt/src/sha256.c"
#undef Ch
#undef Maj
#undef R
#undef R2
#undef blk0
#undef Transform
#undef AddLength
#include "wolfssl-3.9.0/wolfcrypt/src/sha512.c"
#undef Ch
#undef Maj
#undef R
#undef R2
#undef blk0
#undef XTRANSFORM
#define _Transform _Transform_sha
#define AddLength AddLength_sha
#include "wolfssl-3.9.0/wolfcrypt/src/sha.c"
#undef Transform
#undef AddLength
#include "wolfssl-3.9.0/wolfcrypt/src/signature.c"
#include "wolfssl-3.9.0/wolfcrypt/src/srp.c"
#include "wolfssl-3.9.0/wolfcrypt/src/tfm.c"
#include "wolfssl-3.9.0/wolfcrypt/src/wc_encrypt.c"
#include "wolfssl-3.9.0/wolfcrypt/src/wc_port.c"

#endif

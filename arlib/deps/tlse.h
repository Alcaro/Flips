#ifndef TLSE_H
#define TLSE_H

// #define DEBUG

// define TLS_LEGACY_SUPPORT to support TLS 1.1/1.0 (legacy)
// legacy support it will use an additional 272 bytes / context
#define TLS_LEGACY_SUPPORT
// SSL_* style blocking APIs
#define SSL_COMPATIBLE_INTERFACE
// support ChaCha20/Poly1305
#define TLS_WITH_CHACHA20_POLY1305
// support forward secrecy (Diffie-Hellman ephemeral)
#define TLS_FORWARD_SECRECY
// support client-side ECDHE
#define TLS_CLIENT_ECDHE
// suport ecdsa
#define TLS_ECDSA_SUPPORTED
// suport ecdsa client-side
// #define TLS_CLIENT_ECDSA
// TLS renegotiation is disabled by default (secured or not)
// do not uncomment next line!
// #define TLS_ACCEPT_SECURE_RENEGOTIATION

#define SSL_V30                 0x0300
#define TLS_V10                 0x0301
#define TLS_V11                 0x0302
#define TLS_V12                 0x0303
#define DTLS_V10                0xFEFF
#define DTLS_V12                0xFEFD

#define TLS_NEED_MORE_DATA       0
#define TLS_GENERIC_ERROR       -1
#define TLS_BROKEN_PACKET       -2
#define TLS_NOT_UNDERSTOOD      -3
#define TLS_NOT_SAFE            -4
#define TLS_NO_COMMON_CIPHER    -5
#define TLS_UNEXPECTED_MESSAGE  -6
#define TLS_CLOSE_CONNECTION    -7
#define TLS_COMPRESSION_NOT_SUPPORTED -8
#define TLS_NO_MEMORY           -9
#define TLS_NOT_VERIFIED        -10
#define TLS_INTEGRITY_FAILED    -11
#define TLS_ERROR_ALERT         -12
#define TLS_BROKEN_CONNECTION   -13
#define TLS_BAD_CERTIFICATE     -14
#define TLS_UNSUPPORTED_CERTIFICATE -15
#define TLS_NO_RENEGOTIATION    -16
#define TLS_FEATURE_NOT_SUPPORTED   -17

#define TLS_RSA_WITH_AES_128_CBC_SHA          0x002F
#define TLS_RSA_WITH_AES_256_CBC_SHA          0x0035
#define TLS_RSA_WITH_AES_128_CBC_SHA256       0x003C
#define TLS_RSA_WITH_AES_256_CBC_SHA256       0x003D
#define TLS_RSA_WITH_AES_128_GCM_SHA256       0x009C
#define TLS_RSA_WITH_AES_256_GCM_SHA384       0x009D

// forward secrecy
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA      0x0033
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA      0x0039
#define TLS_DHE_RSA_WITH_AES_128_CBC_SHA256   0x0067
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA256   0x006B
#define TLS_DHE_RSA_WITH_AES_128_GCM_SHA256   0x009E
#define TLS_DHE_RSA_WITH_AES_256_GCM_SHA384   0x009F

#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA    0xC013
#define TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA    0xC014
#define TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256 0xC027
#define TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 0xC02F
#define TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384 0xC030

#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA    0xC009
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA    0xC00A
#define TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 0xC023
#define TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 0xC024
#define TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 0xC02B
#define TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 0xC02C

#define TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256     0xCCA8
#define TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256   0xCCA9
#define TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256       0xCCAA

#define TLS_FALLBACK_SCSV                     0x5600

#define TLS_UNSUPPORTED_ALGORITHM   0x00
#define TLS_RSA_SIGN_RSA            0x01
#define TLS_RSA_SIGN_MD5            0x04
#define TLS_RSA_SIGN_SHA1           0x05
#define TLS_RSA_SIGN_SHA256         0x0B
#define TLS_RSA_SIGN_SHA384         0x0C
#define TLS_RSA_SIGN_SHA512         0x0D

#define TLS_EC_PUBLIC_KEY           0x11
#define TLS_EC_prime192v1           0x12
#define TLS_EC_prime192v2           0x13
#define TLS_EC_prime192v3           0x14
#define TLS_EC_prime239v1           0x15
#define TLS_EC_prime239v2           0x16
#define TLS_EC_prime239v3           0x17
#define TLS_EC_prime256v1           0x18
#define TLS_EC_secp224r1            21
#define TLS_EC_secp256r1            23
#define TLS_EC_secp384r1            24
#define TLS_EC_secp521r1            25

#define TLS_ALERT_WARNING           0x01
#define TLS_ALERT_CRITICAL          0x02

typedef enum {
    close_notify = 0,
    unexpected_message = 10,
    bad_record_mac = 20,
    decryption_failed_RESERVED = 21,
    record_overflow = 22,
    decompression_failure = 30,
    handshake_failure = 40,
    no_certificate_RESERVED = 41,
    bad_certificate = 42,
    unsupported_certificate = 43,
    certificate_revoked = 44,
    certificate_expired = 45,
    certificate_unknown = 46,
    illegal_parameter = 47,
    unknown_ca = 48,
    access_denied = 49,
    decode_error = 50,
    decrypt_error = 51,
    export_restriction_RESERVED = 60,
    protocol_version = 70,
    insufficient_security = 71,
    internal_error = 80,
    inappropriate_fallback = 86,
    user_canceled = 90,
    no_renegotiation = 100,
    unsupported_extension = 110,
    no_error = 255
} TLSAlertDescription;

// forward declarations
struct TLSPacket;
struct TLSCertificate;
struct TLSContext;
struct ECCCurveParameters;
typedef struct TLSContext TLS;
typedef struct TLSCertificate Certificate;

typedef int (*tls_validation_function)(struct TLSContext *context, struct TLSCertificate **certificate_chain, int len);

unsigned char *tls_pem_decode(const unsigned char *data_in, unsigned int input_length, int cert_index, unsigned int *output_len);
struct TLSCertificate *tls_create_certificate();
int tls_certificate_valid_subject(struct TLSCertificate *cert, const char *subject);
int tls_certificate_valid_subject_name(const unsigned char *cert_subject, const char *subject);
int tls_certificate_is_valid(struct TLSCertificate *cert);
void tls_certificate_set_copy(unsigned char **member, const unsigned char *val, int len);
void tls_certificate_set_copy_date(unsigned char **member, const unsigned char *val, int len);
void tls_certificate_set_key(struct TLSCertificate *cert, const unsigned char *val, int len);
void tls_certificate_set_priv(struct TLSCertificate *cert, const unsigned char *val, int len);
void tls_certificate_set_sign_key(struct TLSCertificate *cert, const unsigned char *val, int len);
char *tls_certificate_to_string(struct TLSCertificate *cert, char *buffer, int len);
void tls_certificate_set_exponent(struct TLSCertificate *cert, const unsigned char *val, int len);
void tls_certificate_set_serial(struct TLSCertificate *cert, const unsigned char *val, int len);
void tls_certificate_set_algorithm(unsigned int *algorithm, const unsigned char *val, int len);
void tls_destroy_certificate(struct TLSCertificate *cert);
struct TLSPacket *tls_create_packet(struct TLSContext *context, unsigned char type, unsigned short version, int payload_size_hint);
void tls_destroy_packet(struct TLSPacket *packet);
void tls_packet_update(struct TLSPacket *packet);
int tls_packet_append(struct TLSPacket *packet, unsigned char *buf, unsigned int len);
int tls_packet_uint8(struct TLSPacket *packet, unsigned char i);
int tls_packet_uint16(struct TLSPacket *packet, unsigned short i);
int tls_packet_uint32(struct TLSPacket *packet, unsigned int i);
int tls_packet_uint24(struct TLSPacket *packet, unsigned int i);
int tls_random(unsigned char *key, int len);
const unsigned char *tls_get_write_buffer(struct TLSContext *context, unsigned int *outlen);
void tls_buffer_clear(struct TLSContext *context);
int tls_established(struct TLSContext *context);
void tls_read_clear(struct TLSContext *context);
int tls_read(struct TLSContext *context, unsigned char *buf, unsigned int size);
struct TLSContext *tls_create_context(unsigned char is_server, unsigned short version);
const struct ECCCurveParameters *tls_set_curve(struct TLSContext *context, const struct ECCCurveParameters *curve);
struct TLSContext *tls_accept(struct TLSContext *context);
int tls_set_default_dhe_pg(struct TLSContext *context, const char *p_hex_str, const char *g_hex_str);
void tls_destroy_context(struct TLSContext *context);
int tls_cipher_supported(struct TLSContext *context, unsigned short cipher);
int tls_cipher_is_fs(struct TLSContext *context, unsigned short cipher);
int tls_choose_cipher(struct TLSContext *context, const unsigned char *buf, int buf_len, int *scsv_set);
int tls_cipher_is_ephemeral(struct TLSContext *context);
const char *tls_cipher_name(struct TLSContext *context);
int tls_is_ecdsa(struct TLSContext *context);
struct TLSPacket *tls_build_client_key_exchange(struct TLSContext *context);
struct TLSPacket *tls_build_server_key_exchange(struct TLSContext *context, int method);
struct TLSPacket *tls_build_hello(struct TLSContext *context);
struct TLSPacket *tls_certificate_request(struct TLSContext *context);
struct TLSPacket *tls_build_verify_request(struct TLSContext *context);
int tls_parse_hello(struct TLSContext *context, const unsigned char *buf, int buf_len, unsigned int *write_packets, unsigned int *dtls_verified);
int tls_parse_certificate(struct TLSContext *context, const unsigned char *buf, int buf_len, int is_client);
int tls_parse_server_key_exchange(struct TLSContext *context, const unsigned char *buf, int buf_len);
int tls_parse_client_key_exchange(struct TLSContext *context, const unsigned char *buf, int buf_len);
int tls_parse_server_hello_done(struct TLSContext *context, const unsigned char *buf, int buf_len);
int tls_parse_finished(struct TLSContext *context, const unsigned char *buf, int buf_len, unsigned int *write_packets);
int tls_parse_verify(struct TLSContext *context, const unsigned char *buf, int buf_len);
int tls_parse_payload(struct TLSContext *context, const unsigned char *buf, int buf_len, tls_validation_function certificate_verify);
int tls_parse_message(struct TLSContext *context, unsigned char *buf, int buf_len, tls_validation_function certificate_verify);
int tls_certificate_verify_signature(struct TLSCertificate *cert, struct TLSCertificate *parent);
int tls_certificate_chain_is_valid(struct TLSCertificate **certificates, int len);
int tls_certificate_chain_is_valid_root(struct TLSContext *context, struct TLSCertificate **certificates, int len);
int tls_load_certificates(struct TLSContext *context, const unsigned char *pem_buffer, int pem_size);
int tls_load_private_key(struct TLSContext *context, const unsigned char *pem_buffer, int pem_size);
struct TLSPacket *tls_build_certificate(struct TLSContext *context);
struct TLSPacket *tls_build_finished(struct TLSContext *context);
struct TLSPacket *tls_build_change_cipher_spec(struct TLSContext *context);
struct TLSPacket *tls_build_done(struct TLSContext *context);
struct TLSPacket *tls_build_message(struct TLSContext *context, unsigned char *data, unsigned int len);
int tls_client_connect(struct TLSContext *context);
int tls_write(struct TLSContext *context, unsigned char *data, unsigned int len);
struct TLSPacket *tls_build_alert(struct TLSContext *context, char critical, unsigned char code);
int tls_consume_stream(struct TLSContext *context, const unsigned char *buf, int buf_len, tls_validation_function certificate_verify);
void tls_close_notify(struct TLSContext *context);
void tls_alert(struct TLSContext *context, unsigned char critical, int code);
int tls_pending(struct TLSContext *context);
void tls_make_exportable(struct TLSContext *context, unsigned char exportable_flag);
int tls_export_context(struct TLSContext *context, unsigned char *buffer, unsigned int buf_len, unsigned char small_version);
struct TLSContext *tls_import_context(unsigned char *buffer, unsigned int buf_len);
int tls_is_broken(struct TLSContext *context);
int tls_request_client_certificate(struct TLSContext *context);
int tls_client_verified(struct TLSContext *context);
const char *tls_sni(struct TLSContext *context);
int tls_sni_set(struct TLSContext *context, const char *sni);
int tls_load_root_certificates(struct TLSContext *context, const unsigned char *pem_buffer, int pem_size);
int tls_default_verify(struct TLSContext *context, struct TLSCertificate **certificate_chain, int len);
void tls_print_certificate(const char *fname);
int tls_add_alpn(struct TLSContext *context, const char *alpn);
int tls_alpn_contains(struct TLSContext *context, const char *alpn, unsigned char alpn_size);
const char *tls_alpn(struct TLSContext *context);

#ifdef SSL_COMPATIBLE_INTERFACE
    #define SSL_SERVER_RSA_CERT 1
    #define SSL_SERVER_RSA_KEY  2
    typedef struct TLSContext SSL_CTX;
    typedef struct TLSContext SSL;

    #define SSL_FILETYPE_PEM    1
    #define SSL_VERIFY_NONE     0
    #define SSL_VERIFY_PEER     1
    #define SSL_VERIFY_FAIL_IF_NO_PEER_CERT 2
    #define SSL_VERIFY_CLIENT_ONCE  3

    typedef struct {
        int fd;
        tls_validation_function certificate_verify;
        void *recv;
        void *send;
        void *user_data;
    } SSLUserData;

    int  SSL_library_init();
    void SSL_load_error_strings();
    void OpenSSL_add_all_algorithms();
    void OpenSSL_add_all_ciphers();
    void OpenSSL_add_all_digests();
    void EVP_cleanup();

    int SSLv3_server_method();
    int SSLv3_client_method();
    struct TLSContext *SSL_new(struct TLSContext *context);
    int SSL_CTX_use_certificate_file(struct TLSContext *context, const char *filename, int dummy);
    int SSL_CTX_use_PrivateKey_file(struct TLSContext *context, const char *filename, int dummy);
    int SSL_CTX_check_private_key(struct TLSContext *context);
    struct TLSContext *SSL_CTX_new(int method);
    void SSL_free(struct TLSContext *context);
    void SSL_CTX_free(struct TLSContext *context);
    int SSL_get_error(struct TLSContext *context, int ret);
    int SSL_set_fd(struct TLSContext *context, int socket);
    void *SSL_set_userdata(struct TLSContext *context, void *data);
    void *SSL_userdata(struct TLSContext *context);
    int SSL_CTX_root_ca(struct TLSContext *context, const char *pem_filename);
    void SSL_CTX_set_verify(struct TLSContext *context, int mode, tls_validation_function verify_callback);
    int SSL_accept(struct TLSContext *context);
    int SSL_connect(struct TLSContext *context);
    int SSL_shutdown(struct TLSContext *context);
    int SSL_write(struct TLSContext *context, void *buf, unsigned int len);
    int SSL_read(struct TLSContext *context, void *buf, unsigned int len);
    int SSL_pending(struct TLSContext *context);
    int SSL_set_io(struct TLSContext *context, void *recv, void *send);
#endif

#endif

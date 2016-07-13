#include "socket.h"

#ifdef ARLIB_SSL_WOLFSSL
//#define HAVE_SNI
#define HAVE_SUPPORTED_CURVES
#include <wolfssl/ssl.h>
#ifdef __unix__
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#endif
#include <stdlib.h>

#error "this thing is broken. try again once wolfSSL > 3.9 is released and see if that fixes the alert 40 handshake failed errors"

static WOLFSSL_CTX* ctx;

class socketssl_impl : public socketssl {
public:
	socket* sock;
	WOLFSSL* ssl;
	bool nonblock;
	
	socketssl_impl(socket* parent, const char * domain, bool permissive)
	{
		sock = parent;
		fd = get_fd(parent);
		//ssl = wolfSSL_new(ctx);
		nonblock = 0;
		
    wolfSSL_Init();
    wolfSSL_Debugging_ON();
    ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
	wolfSSL_SetIORecv(ctx, socketssl_impl::recv_raw);
	wolfSSL_SetIOSend(ctx, socketssl_impl::send_raw);

    wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);

#define err_sys puts
    ssl = wolfSSL_new(ctx);
		
		wolfSSL_SetIOReadCtx(ssl, this);
		wolfSSL_SetIOWriteCtx(ssl, this);
		
    if (ssl == NULL)
        err_sys("unable to get SSL object");

    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP256R1)
            != SSL_SUCCESS) {
        err_sys("unable to set curve secp256r1");
    }
    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP384R1)
            != SSL_SUCCESS) {
        err_sys("unable to set curve secp384r1");
    }
    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP521R1)
            != SSL_SUCCESS) {
        err_sys("unable to set curve secp521r1");
    }
    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP224R1)
            != SSL_SUCCESS) {
        err_sys("unable to set curve secp224r1");
    }
    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP192R1)
            != SSL_SUCCESS) {
        err_sys("unable to set curve secp192r1");
    }
    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP160R1)
            != SSL_SUCCESS) {
        err_sys("unable to set curve secp160r1");
    }

//printf("fd=%i ret=%i ok=%i f=%i\n", fd,
    //wolfSSL_set_fd(ssl, fd),
    //SSL_SUCCESS, SSL_FAILURE);

puts("AAAAAAA");
    if (wolfSSL_connect(ssl) != SSL_SUCCESS) {puts("NOOO");}
		
		//wolfSSL_check_domain_name(ssl, domain);
		
		
//#define err_sys puts
//    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP256R1)
//            != SSL_SUCCESS) {
//        err_sys("unable to set curve secp256r1");
//    }
//    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP384R1)
//            != SSL_SUCCESS) {
//        err_sys("unable to set curve secp384r1");
//    }
//    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP521R1)
//            != SSL_SUCCESS) {
//        err_sys("unable to set curve secp521r1");
//    }
//    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP224R1)
//            != SSL_SUCCESS) {
//        err_sys("unable to set curve secp224r1");
//    }
//    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP192R1)
//            != SSL_SUCCESS) {
//        err_sys("unable to set curve secp192r1");
//    }
//    if (wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP160R1)
//            != SSL_SUCCESS) {
//        err_sys("unable to set curve secp160r1");
//    }
//
//		wolfSSL_set_fd(ssl, fd);
//		puts("cactus");
//    if (wolfSSL_connect(ssl) != SSL_SUCCESS) {
//        /* see note at top of README */
//        int  err = wolfSSL_get_error(ssl, 0);
//        char buffer[80];
//        printf("err = %d, %s\n", err,
//                                wolfSSL_ERR_error_string(err, buffer));
//        err_sys("SSL_connect failed");
//        /* if you're getting an error here  */
//    }
    exit(0);
	}
	
	/*private*/ static int recv_raw(WOLFSSL* ssl, char* buf, int sz, void* ctx)
	{
		socketssl_impl* this_ = (socketssl_impl*)ctx;
		int ret = this_->sock->recv((uint8_t*)buf, sz);
printf("SSLDATRAW_R=%i\n",ret);
for(int i=0;i<ret;i++)printf("%.2X ",(uint8_t)buf[i]);
puts("");
		if (ret==0) return WOLFSSL_CBIO_ERR_WANT_READ;
		if (ret<0) return WOLFSSL_CBIO_ERR_GENERAL;
		return ret;
	}
	
	/*private*/ static int send_raw(WOLFSSL* ssl, char* buf, int sz, void* ctx)
	{
		socketssl_impl* this_ = (socketssl_impl*)ctx;
		int ret;
		if (this_->nonblock) ret = this_->sock->send0((uint8_t*)buf, sz);
		else                 ret = this_->sock->send1((uint8_t*)buf, sz);
printf("SSLDATRAW_S=%i\n",ret);
for(int i=0;i<ret;i++)printf("%.2X ",(uint8_t)buf[i]);
puts("");
		if (ret==0) return WOLFSSL_CBIO_ERR_WANT_WRITE;
		if (ret<0) return WOLFSSL_CBIO_ERR_GENERAL;
		return ret;
	}
	
	/*private*/ int fixret(int ret)
	{
printf("SSLDAT=%i\n",ret);
		if (ret > 0) return ret;
		
		int err = wolfSSL_get_error(ssl, ret);
		if (err==SSL_ERROR_WANT_READ || err==SSL_ERROR_WANT_WRITE) return 0;
printf("SSLERR=%i\n",err);
		return e_broken;
	}
	
	int recv(uint8_t* data, int len)
	{
		nonblock = false;
		return fixret(wolfSSL_read(ssl, data, len));
	}
	
	int send0(const uint8_t* data, int len)
	{
		nonblock = true;
		return fixret(wolfSSL_write(ssl, data, len));
	}
	
	int send1(const uint8_t* data, int len)
	{
		nonblock = false;
		return fixret(wolfSSL_write(ssl, data, len));
	}
	
	~socketssl_impl()
	{
		wolfSSL_free(ssl);
		delete sock;
	}
};

static void initialize()
{
	static bool initialized = false;
	if (initialized) return;
	initialized = true;
	

	
	
	//wolfSSL_Init();
	//
	//ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
	//if (!ctx) return;
	//wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);
	
	//wolfSSL_SetIORecv(ctx, socketssl_impl::recv_raw);
	//wolfSSL_SetIOSend(ctx, socketssl_impl::send_raw);
	
	#ifdef __unix__
	//mostly copypasta from wolfSSL_CTX_load_verify_locations, minus the abort-on-first-error thingy
	//there's some random weirdo files in my /etc/ssl/certs/, possibly duplicates?
	//DIR* dir = opendir("/etc/ssl/certs/");
	//if (dir)
	//{
		//while (true)
		//{
			//struct dirent* entry = readdir(dir);
			//if (!entry) break;
			//char name[256];
			//snprintf(name, sizeof(name), "%s%s", "/etc/ssl/certs/", entry->d_name);
			//
			//struct stat s;
			//if (stat(name, &s) == 0 && (s.st_mode & S_IFREG))
			//{
//printf("cert=%s\n",name);
				//wolfSSL_CTX_load_verify_locations(ctx, name, NULL);
			//}
		//}
		//closedir(dir);
	//}
#else
#error unsupported
#endif
	//wolfSSL_Debugging_ON();
}

socketssl* socketssl::create(socket* parent, const char * domain, bool permissive)
{
	initialize();
	//if (!ctx) return NULL;
	
	return new socketssl_impl(parent, domain, permissive);
}
#endif

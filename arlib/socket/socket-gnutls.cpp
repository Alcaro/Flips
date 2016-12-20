#include "socket.h"

#ifdef ARLIB_SSL_GNUTLS
//could use the gnutls C++ api, but why should I
//extra dependency, and the added convenience isn't relevant on such a small component
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

//based on http://www.gnutls.org/manual/html_node/Legacy-client-example-with-X_002e509-certificate-support.html

static gnutls_certificate_credentials_t xcred;
static bool init_ok = false;

#define CAFILE "/etc/ssl/certs/ca-certificates.crt"
static int _verify_certificate_callback(gnutls_session_t session);

static void initialize()
{
	static bool initialized = false;
	if (initialized) return;
	
#define CHECK(x) if ((x)<0) return
	CHECK(gnutls_check_version("3.1.4") != NULL);
	
	CHECK(gnutls_global_init());
	
	CHECK(gnutls_certificate_allocate_credentials(&xcred));
	CHECK(gnutls_certificate_set_x509_trust_file(xcred, CAFILE, GNUTLS_X509_FMT_PEM));
	gnutls_certificate_set_verify_function(xcred, _verify_certificate_callback);
#undef CHECK
	
	init_ok = true;
}
//to deinit:
//gnutls_certificate_free_credentials(xcred);
//gnutls_global_deinit();

static int _verify_certificate_callback(gnutls_session_t session)
{
#define CHECK(x) if ((x)<0) return GNUTLS_E_CERTIFICATE_ERROR
	const char * hostname = (char*)gnutls_session_get_ptr(session);
	if (!hostname) return 0;
	
	unsigned int status;
	CHECK(gnutls_certificate_verify_peers3(session, hostname, &status));
	
	//gnutls_certificate_type_t type = gnutls_certificate_type_get(session);
	//gnutls_datum_t out;
	//CHECK(gnutls_certificate_verification_status_print(status, type,
	//                                                   &out, 0));
	//printf("%s", out.data);
	//gnutls_free(out.data);
	
	if (status != 0) return GNUTLS_E_CERTIFICATE_ERROR;
	return 0;
#undef CHECK
}

class socketssl_impl : public socketssl {
public:
	socket* sock;
	gnutls_session_t session;
	bool block;
	
	
	bool init(socket* parent, cstring domain, bool permissive)
	{
#define CHECK(x) if ((x)<0) return false;
		this->sock = parent;
		this->fd = parent->get_fd();
		
		this->session = NULL;
		this->block = false;
		setblock(true);
		
		CHECK(gnutls_init(&this->session, GNUTLS_CLIENT));
		if (!permissive)
		{
			gnutls_session_set_ptr(this->session, (void*)(const char*)domain);
		}
		gnutls_server_name_set(this->session, GNUTLS_NAME_DNS, domain, domain.length());
		
		CHECK(gnutls_set_default_priority(this->session));
		//CHECK(gnutls_priority_set_direct(session, "NORMAL", NULL));
		
		CHECK(gnutls_credentials_set(this->session, GNUTLS_CRD_CERTIFICATE, xcred));
		gnutls_transport_set_int(this->session, this->fd);
		gnutls_handshake_set_timeout(this->session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
		
		int ret;
		do {
			ret = gnutls_handshake(session);
		}
		while (ret < 0 && gnutls_error_is_fatal(ret) == 0);
		if (ret < 0) return false;
		
		//char* desc = gnutls_session_get_desc(session);
		//puts(desc);
		//gnutls_free(desc);
		
		return true;
#undef CHECK
	}
	
	/*private*/ int fixretrecv(int ret)
	{
		if (ret > 0) return ret;
		if (ret == 0) return e_closed;
		
		if (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN) return 0;
		return e_ssl_failure;
	}
	
	/*private*/ int fixretsend(int ret)
	{
		if (ret >= 0) return ret;
		else return fixretrecv(ret);
	}
	
	void setblock(bool block)
	{
		if (block == this->block) return;
		this->block = block;
		socket::setblock(this->fd, block);
	}
	
	int recv(arrayvieww<uint8_t> data, bool block = false)
	{
		setblock(block);
		return fixretrecv(gnutls_record_recv(this->session, data.ptr(), data.size()));
	}
	
	int sendp(arrayview<uint8_t> data, bool block = true)
	{
		setblock(block);
		return fixretsend(gnutls_record_send(this->session, data.ptr(), data.size()));
	}
	
	~socketssl_impl()
	{
		if (this->session)
		{
			gnutls_bye(this->session, GNUTLS_SHUT_RDWR); // can fail, but let's just ignore that
		}
		gnutls_deinit(this->session);
		delete sock;
	}
};

socketssl* socketssl::create(socket* parent, cstring domain, bool permissive)
{
	if (!parent) return NULL;
	
	initialize();
	if (!init_ok) return NULL;
	
	socketssl_impl* ret = new socketssl_impl;
	if (!ret->init(parent, domain, permissive))
	{
		delete ret;
		return NULL;
	}
	return ret;
}
#endif

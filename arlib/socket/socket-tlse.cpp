#include "socket.h"

#ifdef ARLIB_SSL_TLSE
extern "C" {
#include "../deps/tlse.h"
}
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

//TLSe flaws and limitations:
//- can't share root certs between contexts (with possible exception of tls_accept, didn't check)
//- lack of const on some functions
//- have to load root certs myself
//- had to implement Subject Alternative Name myself
//- turns out tls_consume_stream(buf_len=0) throws an error - and no debug output
//- tls_export_context return value seems be 'bytes expected' for inputlen=0 and inputlen>=expected,
//     but 'additional bytes expected' for inputlen=1
//- lacks extern "C" on header
//- lack of documentation

// separate context here to ensure they're not loaded multiple times, saves memory and time
static TLSContext* rootcerts;

static void initialize()
{
	if (rootcerts) return;
	
	rootcerts = tls_create_context(false, TLS_V12);
	
#ifdef __unix__
	DIR* dir = opendir("/etc/ssl/certs/");
	uint8_t* cert = NULL;
	off_t cert_buf_len = 0;
	
	if (dir)
	{
		while (true)
		{
			struct dirent* entry = readdir(dir);
			if (!entry) break;
			char name[256];
			snprintf(name, sizeof(name), "%s%s", "/etc/ssl/certs/", entry->d_name);
			
			if (!strstr(name, "DST_Root")) continue;
			
			struct stat s;
			if (stat(name, &s) == 0 && (s.st_mode & S_IFREG))
			{
				if (s.st_size > cert_buf_len)
				{
					free(cert);
					cert_buf_len = s.st_size;
					cert = (uint8_t*)malloc(cert_buf_len);
				}
				
				int fd = open(name, O_RDONLY);
				if (fd >= 0)
				{
					off_t actualsize = read(fd, cert, s.st_size);
					tls_load_root_certificates(rootcerts, cert, actualsize);
				}
			}
		}
		closedir(dir);
	}
	free(cert);
#else
#error unsupported
#endif
}

class socketssl_impl : public socketssl {
public:
	socket* sock;
	TLSContext* ssl;
	
	//same as tls_default_verify, except tls_certificate_chain_is_valid_root is given another context
	static int verify(TLSContext* context, TLSCertificate* * certificate_chain, int len) {
		int err;
		if (certificate_chain) {
			for (int i = 0; i < len; i++) {
				TLSCertificate* certificate = certificate_chain[i];
				// check validity date
				err = tls_certificate_is_valid(certificate);
				if (err)
					return err;
				// check certificate in certificate->bytes of length certificate->len
					// the certificate is in ASN.1 DER format
			}
		}
		// check if chain is valid
		err = tls_certificate_chain_is_valid(certificate_chain, len);
		if (err)
			return err;
		
		const char * sni = tls_sni(context);
		if (len>0 && sni) {
			err = tls_certificate_valid_subject(certificate_chain[0], sni);
			if (err)
				return err;
		}
		
		// Perform certificate validation agains ROOT CA
		err = tls_certificate_chain_is_valid_root(rootcerts, certificate_chain, len);
		if (err)
			return err;
		
		//return certificate_expired;
		//return certificate_revoked;
		//return certificate_unknown;
		return no_error;
	}
	
	void process(bool block)
	{
		if (!sock) return;
		
		unsigned int outlen = 0;
		const uint8_t * out = tls_get_write_buffer(ssl, &outlen);
		if (out && outlen)
		{
			if (sock->send(arrayview<byte>(out, outlen)) < 0) { error(); return; }
			tls_buffer_clear(ssl);
		}
		
		uint8_t in[0x2000];
		int inlen = sock->recv(arrayvieww<byte>(in, sizeof(in)), block);
		if (inlen<0) { error(); return; }
		if (inlen>0) tls_consume_stream(ssl, in, inlen, verify);
	}
	
	void error()
	{
		delete sock;
		sock = NULL;
	}
	
	static socketssl_impl* create(socket* parent, const char * domain, bool permissive)
	{
		if (!parent) return NULL;
		
		socketssl_impl* ret = new socketssl_impl();
		ret->sock = parent;
		ret->fd = parent->get_fd();
		
		ret->ssl = tls_create_context(false, TLS_V12);
		
		tls_make_exportable(ret->ssl, true);
		tls_sni_set(ret->ssl, domain);
		
		tls_client_connect(ret->ssl);
		
		while (!tls_established(ret->ssl))
		{
			ret->process(true);
		}
		
		return ret;
	}
	
	int recv(arrayvieww<byte> data, bool block = false)
	{
		process(block);
		
		int ret = tls_read(ssl, data.ptr(), data.size());
		if (ret==0 && !sock) return e_broken;
		
		return ret;
	}
	
	int sendp(arrayview<byte> data, bool block = true)
	{
		if (!sock) return -1;
		
		int ret = tls_write(ssl, (uint8_t*)data.ptr(), data.size());
		process(false);
		return ret;
	}
	
	~socketssl_impl()
	{
		if (ssl && sock)
		{
			tls_close_notify(ssl);
			process(false);
		}
		if (ssl) tls_destroy_context(ssl);
		if (sock) delete sock;
	}
	
	//void q()
	//{
	//	uint8_t data[4096];
	//	int len = tls_export_context(ssl, NULL, 0, false);
	//	int len2 = tls_export_context(ssl, data, len, false);
	//	printf("len=%i len2=%i\n", len, len2);
	//	//tls_destroy_context(ssl);
	//	
	//	TLSContext* ssl2 = tls_import_context(data, len);
	//	
	//	uint8_t* p1 = (uint8_t*)ssl;
	//	uint8_t* p2 = (uint8_t*)ssl2;
	//	for (int i=0;i<140304;i++)
	//	{
	//		//if (p1[i] != p2[i]) printf("%i: g=%.2X b=%.2X\n", i, p1[i], p2[i]);
	//	}
	//	
	//	//ssl = ssl2;
	//}
	
	
	//size_t serialize_size()
	//{
	//	return tls_export_context(ssl, NULL, 0, false);
	//}
	//
	//int serialize(uint8_t* data, size_t len)
	//{
	//	process(true);
	//	
	//	tls_export_context(ssl, data, len, false);
	//	
	//	tls_destroy_context(this->ssl);
	//	this->ssl = NULL;
	//	
	//	int ret = decompose(this->sock);
	//	this->sock = NULL;
	//	
	//	delete this;
	//	
	//	return ret;
	//}
	//
	//static socketssl_impl* unserialize(int fd, const uint8_t* data, size_t len)
	//{
	//	socketssl_impl* ret = new socketssl_impl();
	//	ret->sock = socket::create_from_fd(fd);
	//	ret->fd = fd;
	//	ret->ssl = tls_import_context((uint8_t*)data, len);
	//	if (!ret->ssl) { delete ret; return NULL; }
	//	return ret;
	//}
};

socketssl* socketssl::create(socket* parent, cstring domain, bool permissive)
{
	initialize();
	return socketssl_impl::create(parent, domain, permissive);
}

//socketssl* socketssl::unserialize(int fd, const uint8_t* data, size_t len)
//{
//	initialize();
//	return socketssl_impl::unserialize(fd, data, len);
//}
#endif

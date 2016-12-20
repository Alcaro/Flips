#include "socket.h"

//based on http://wayback.archive.org/web/20100528130307/http://www.coastrd.com/c-schannel-smtp
//but heavily rewritten for stability and compactness

#ifdef ARLIB_SSL_SCHANNEL
#ifndef _WIN32
#error SChannel only exists on Windows
#endif

#define SECURITY_WIN32
#undef bind
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <schannel.h>
#include <security.h>
#include <sspi.h>

namespace {

static SecurityFunctionTable* SSPI;
static CredHandle cred;

#define SSPIFlags \
	(ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | \
	 ISC_RET_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM)

//my mingw headers are outdated
#ifndef SCH_USE_STRONG_CRYPTO
#define SCH_USE_STRONG_CRYPTO 0x00400000
#endif
#ifndef SP_PROT_TLS1_2_CLIENT
#define SP_PROT_TLS1_2_CLIENT 0x00000800
#endif
#ifndef SEC_Entry
#define SEC_Entry WINAPI
#endif

static void initialize()
{
	if (SSPI) return;
	
	//linking a DLL is easy, but when there's only one exported function, spending the extra effort is worth it
	HMODULE secur32 = LoadLibraryA("secur32.dll");
	typedef PSecurityFunctionTableA SEC_Entry (*InitSecurityInterfaceA_t)(void);
	InitSecurityInterfaceA_t InitSecurityInterfaceA = (InitSecurityInterfaceA_t)GetProcAddress(secur32, SECURITY_ENTRYPOINT_ANSIA);
	SSPI = InitSecurityInterfaceA();
	
	SCHANNEL_CRED SchannelCred = {};
	SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
	SchannelCred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO;
	// fun fact: IE11 doesn't use SCH_USE_STRONG_CRYPTO. I guess it favors accepting outdated servers over rejecting evil ones.
	SchannelCred.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT; // Microsoft recommends setting this to zero, but that makes it use TLS 1.0.
	//howsmyssl expects session ticket support for the Good rating, but that's only supported on windows 8, according to
	// https://connect.microsoft.com/IE/feedback/details/997136/internet-explorer-11-on-windows-7-does-not-support-tls-session-tickets
	//and I can't find which flag enables that, anyways
	
	SSPI->AcquireCredentialsHandleA(NULL, (char*)UNISP_NAME_A, SECPKG_CRED_OUTBOUND,
	                                NULL, &SchannelCred, NULL, NULL, &cred, NULL);
}

class socketssl_impl : public socketssl {
public:
	socket* sock;
	CtxtHandle ssl;
	SecPkgContext_StreamSizes bufsizes;
	
	BYTE* recv_buf;
	size_t recv_buf_len;
	BYTE* ret_buf;
	size_t ret_buf_len;
	
	bool in_handshake;
	bool permissive;
	
	void fetch(bool block)
	{
		array<byte> retbytes;
		int ret = sock->recv(retbytes, block);
		if (ret<0) return error();
		
		recv_buf = realloc(recv_buf, recv_buf_len + ret + 1024);
		memcpy(recv_buf+recv_buf_len, retbytes.ptr(), ret);
		recv_buf_len += ret;
	}
	
	void fetch() { fetch(true); }
	void fetchnb() { fetch(false); }
	
	
	void ret_realloc(int bytes)
	{
		if (bytes > 0)
		{
			ret_buf_len += bytes;
			if (ret_buf_len > 1024)
			{
				ret_buf = realloc(ret_buf, ret_buf_len + 1024);
			}
		}
	}
	
	
	BYTE* tmpptr()
	{
		return recv_buf + recv_buf_len;
	}
	
	
	void error()
	{
		SSPI->DeleteSecurityContext(&ssl);
		delete sock;
		sock = NULL;
		in_handshake = false;
	}
	
	void handshake()
	{
		if (!in_handshake) return;
		
		SecBuffer InBuffers[2] = { { recv_buf_len, SECBUFFER_TOKEN, recv_buf }, { 0, SECBUFFER_EMPTY, NULL } };
		SecBufferDesc InBufferDesc = { SECBUFFER_VERSION, 2, InBuffers };
		
		SecBuffer OutBuffer = { 0, SECBUFFER_TOKEN, NULL };
		SecBufferDesc OutBufferDesc = { SECBUFFER_VERSION, 1, &OutBuffer };
		
		DWORD ignore;
		SECURITY_STATUS scRet;
		ULONG flags = SSPIFlags;
		if (this->permissive) flags |= ISC_REQ_MANUAL_CRED_VALIDATION; // +1 for defaulting to secure
		scRet = SSPI->InitializeSecurityContextA(&cred, &ssl, NULL, flags, 0, SECURITY_NATIVE_DREP,
		                                         &InBufferDesc, 0, NULL, &OutBufferDesc, &ignore, NULL);
		
		// according to the original program, extended errors are success
		// but they also hit the error handler below, so I guess it just sends an error to the server?
		// either way, ignore
		if (scRet == SEC_E_OK || scRet == SEC_I_CONTINUE_NEEDED)
		{
			if (OutBuffer.cbBuffer != 0 && OutBuffer.pvBuffer != NULL)
			{
				if (sock->send(arrayview<byte>((BYTE*)OutBuffer.pvBuffer, OutBuffer.cbBuffer)) < 0)
				{
					SSPI->FreeContextBuffer(OutBuffer.pvBuffer);
					error();
					return;
				}
				SSPI->FreeContextBuffer(OutBuffer.pvBuffer);
			}
		}
		
		if (scRet == SEC_E_INCOMPLETE_MESSAGE) return;
		
		if (scRet == SEC_E_OK)
		{
			in_handshake = false;
		}
		
		if (FAILED(scRet))
		{
			error();
			return;
		}
		
		// SEC_I_INCOMPLETE_CREDENTIALS is possible and means server requested client authentication
		// we don't support that, just ignore it
		
		if (InBuffers[1].BufferType == SECBUFFER_EXTRA)
		{
			memmove(recv_buf, recv_buf + (recv_buf_len - InBuffers[1].cbBuffer), InBuffers[1].cbBuffer);
			recv_buf_len = InBuffers[1].cbBuffer;
		}
		else recv_buf_len = 0;
	}
	
	bool handshake_first(const char * domain)
	{
		SecBuffer OutBuffer = { 0, SECBUFFER_TOKEN, NULL };
		SecBufferDesc OutBufferDesc = { SECBUFFER_VERSION, 1, &OutBuffer };
		
		DWORD ignore;
		if (SSPI->InitializeSecurityContextA(&cred, NULL, (char*)domain, SSPIFlags, 0, SECURITY_NATIVE_DREP,
		                                     NULL, 0, &ssl, &OutBufferDesc, &ignore, NULL)
		    != SEC_I_CONTINUE_NEEDED)
		{
			return false;
		}
		
		if (OutBuffer.cbBuffer != 0)
		{
			if (sock->send(arrayview<byte>((BYTE*)OutBuffer.pvBuffer, OutBuffer.cbBuffer)) < 0)
			{
				SSPI->FreeContextBuffer(OutBuffer.pvBuffer);
				error();
				return false;
			}
			SSPI->FreeContextBuffer(OutBuffer.pvBuffer); // Free output buffer.
		}
		
		in_handshake = true;
		while (in_handshake) { fetch(); handshake(); }
		return true;
	}
	
	bool init(socket* parent, const char * domain, bool permissive)
	{
		if (!parent) return false;
		
		this->sock = parent;
		this->fd = parent->get_fd();
		this->recv_buf = malloc(2048);
		this->recv_buf_len = 0;
		this->ret_buf = malloc(2048);
		this->ret_buf_len = 0;
		
		this->permissive = permissive;
		
		if (!handshake_first(domain)) return false;
		SSPI->QueryContextAttributes(&ssl, SECPKG_ATTR_STREAM_SIZES, &bufsizes);
		
		return (sock);
	}
	
	void process()
	{
		handshake();
		
		bool again = true;
		
		while (again)
		{
			again = false;
			
			SecBuffer Buffers[4] = {
				{ recv_buf_len, SECBUFFER_DATA,  recv_buf },
				{ 0,            SECBUFFER_EMPTY, NULL },
				{ 0,            SECBUFFER_EMPTY, NULL },
				{ 0,            SECBUFFER_EMPTY, NULL },
			};
			SecBufferDesc Message = { SECBUFFER_VERSION, 4, Buffers };
			
			SECURITY_STATUS scRet = SSPI->DecryptMessage(&ssl, &Message, 0, NULL);
			if (scRet == SEC_E_INCOMPLETE_MESSAGE) return;
			else if (scRet == SEC_I_RENEGOTIATE)
			{
				in_handshake = true;
			}
			else if (scRet != SEC_E_OK)
			{
				error();
				return;
			}
			
			recv_buf_len = 0;
			
			// Locate data and (optional) extra buffers.
			for (int i=0;i<4;i++)
			{
				if (Buffers[i].BufferType == SECBUFFER_DATA)
				{
					memcpy(ret_buf+ret_buf_len, Buffers[i].pvBuffer, Buffers[i].cbBuffer);
					ret_realloc(Buffers[i].cbBuffer);
					again = true;
				}
				if (Buffers[i].BufferType == SECBUFFER_EXTRA)
				{
					memmove(recv_buf, Buffers[i].pvBuffer, Buffers[i].cbBuffer);
					recv_buf_len = Buffers[i].cbBuffer;
				}
			}
		}
	}
	
	int recv(arrayvieww<byte> data, bool block = false)
	{
		if (!sock) return -1;
		
		fetch(block);
		process();
		
		size_t bytes_ret = ret_buf_len;
		if (bytes_ret > data.size()) bytes_ret = data.size();
		memcpy(data.ptr(), ret_buf, bytes_ret);
		memmove(ret_buf, ret_buf+bytes_ret, ret_buf_len-bytes_ret);
		ret_buf_len -= bytes_ret;
		return bytes_ret;
	}
	
	int sendp(arrayview<byte> bytes, bool block = true)
	{
		if (!sock) return -1;
		
		const byte* data = bytes.ptr();
		unsigned int len = bytes.size();
		
		fetchnb();
		process();
		
		BYTE* sendbuf = tmpptr(); // let's reuse this
		
		unsigned int maxmsglen = 0x1000 - bufsizes.cbHeader - bufsizes.cbTrailer;
		if (len > maxmsglen) len = maxmsglen;
		
		memcpy(sendbuf+bufsizes.cbHeader, data, len);
		SecBuffer Buffers[4] = {
			{ bufsizes.cbHeader,  SECBUFFER_STREAM_HEADER,  sendbuf },
			{ len,                SECBUFFER_DATA,           sendbuf+bufsizes.cbHeader },
			{ bufsizes.cbTrailer, SECBUFFER_STREAM_TRAILER, sendbuf+bufsizes.cbHeader+len },
			{ 0,                  SECBUFFER_EMPTY,          NULL },
		};
		SecBufferDesc Message = { SECBUFFER_VERSION, 4, Buffers };
		if (FAILED(SSPI->EncryptMessage(&ssl, 0, &Message, 0))) { error(); return -1; }
		
		if (sock->send(arrayview<byte>(sendbuf, Buffers[0].cbBuffer + Buffers[1].cbBuffer + Buffers[2].cbBuffer)) < 0) error();
		
		return len;
	}
	
	~socketssl_impl()
	{
		error();
		free(recv_buf);
		free(ret_buf);
	}
};

}

socketssl* socketssl::create(socket* parent, cstring domain, bool permissive)
{
	initialize();
	socketssl_impl* ret = new socketssl_impl();
	if (!ret->init(parent, domain, permissive)) { delete ret; return NULL; }
	else return ret;
}
#endif

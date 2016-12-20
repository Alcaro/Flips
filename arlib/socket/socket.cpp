#include "socket.h"
#include <stdio.h>

#undef socket
#undef bind
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <mstcpip.h>
	#define MSG_NOSIGNAL 0
	#define MSG_DONTWAIT 0
	#define close closesocket
	#define usleep(n) Sleep(n/1000)
	#ifdef _MSC_VER
		#pragma comment(lib, "ws2_32.lib")
	#endif
#else
	#include <netdb.h>
	#include <errno.h>
	#include <unistd.h>
	#include <fcntl.h>
	
	#include <netinet/tcp.h>
#endif

//wrapper because 'socket' is a type in this code, so socket(2) needs another name
static int mksocket(int domain, int type, int protocol) { return socket(domain, type, protocol); }
#define socket socket_t

namespace {

static void initialize()
{
#ifdef _WIN32 // lol
	static bool initialized = false;
	if (initialized) return;
	initialized = true;
	
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

static int setsockopt(int socket, int level, int option_name, const void * option_value, socklen_t option_len)
{
	return ::setsockopt(socket, level, option_name, (char*)/*lol windows*/option_value, option_len);
}

static int setsockopt(int socket, int level, int option_name, int option_value)
{
	return setsockopt(socket, level, option_name, &option_value, sizeof(option_value));
}

static int connect(const char * domain, int port)
{
	initialize();
	
	char portstr[16];
	sprintf(portstr, "%i", port);
	
	addrinfo hints;
	memset(&hints, 0, sizeof(addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	
	addrinfo * addr = NULL;
	getaddrinfo(domain, portstr, &hints, &addr);
	if (!addr) return -1;
	
	int fd = mksocket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
#ifndef _WIN32
	//because 30 second pauses are unequivocally detestable
	timeval timeout;
	timeout.tv_sec = 4;
	timeout.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
#endif
	if (connect(fd, addr->ai_addr, addr->ai_addrlen) != 0)
	{
		freeaddrinfo(addr);
		close(fd);
		return -1;
	}
#ifndef _WIN32
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, 1); // enable
	setsockopt(fd, SOL_TCP, TCP_KEEPCNT, 3); // ping count before the kernel gives up
	setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, 30); // seconds idle until it starts pinging
	setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, 10); // seconds per ping once the pings start
#else
	u_long yes = 1;
	ioctlsocket(fd, FIONBIO, &yes);
	
	struct tcp_keepalive keepalive = {
		1,       // SO_KEEPALIVE
		30*1000, // TCP_KEEPIDLE in milliseconds
		3*1000,  // TCP_KEEPINTVL
		//On Windows Vista and later, the number of keep-alive probes (data retransmissions) is set to 10 and cannot be changed.
		//https://msdn.microsoft.com/en-us/library/windows/desktop/dd877220(v=vs.85).aspx
		//so no TCP_KEEPCNT; I'll reduce INTVL instead. And a polite server will RST anyways.
	};
	u_long ignore;
	WSAIoctl(fd, SIO_KEEPALIVE_VALS, &keepalive, sizeof(keepalive), NULL, 0, &ignore, NULL, NULL);
#endif
	
	freeaddrinfo(addr);
	return fd;
}

} // close namespace

//MSG_DONTWAIT is usually better, but accept() doesn't take that argument
void socket::setblock(int fd, bool newblock)
{
#ifdef _WIN32
	u_long nonblock = !newblock;
	ioctlsocket(fd, FIONBIO, &nonblock);
#else
	int flags = fcntl(fd, F_GETFL, 0);
	flags &= ~O_NONBLOCK;
	if (!newblock) flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);
#endif
}

namespace {

class socket_impl : public socket {
public:
#ifdef _WIN32
	bool m_blocking = true;
	/*private*/ void setblock(bool newblock)
	{
		//if (m_blocking == newblock) return;
		//m_blocking = newblock;
		socket::setblock(this->fd, newblock);
	}
#else
	/*private*/ void setblock(bool newblock) {} // MSG_DONTWAIT exists here
#endif
	
	socket_impl(int fd) { this->fd = fd; }
	
	/*private*/ int fixret(int ret)
	{
//printf("r=%i e=%i wb=%i\n", ret, WSAGetLastError(), WSAEWOULDBLOCK);
		if (ret > 0) return ret;
		if (ret == 0) return e_closed;
#ifdef __unix__
		if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
#endif
#ifdef _WIN32
		if (ret < 0 && WSAGetLastError() == WSAEWOULDBLOCK) return 0;
#endif
		return e_broken;
	}
	
	int recv(arrayvieww<byte> data, bool block = false)
	{
		setblock(block);
		return fixret(::recv(this->fd, (char*)data.ptr(), data.size(), MSG_NOSIGNAL | (block ? 0 : MSG_DONTWAIT)));
	}
	
	int sendp(arrayview<byte> data, bool block = true)
	{
		setblock(block);
		return fixret(::send(fd, (char*)data.ptr(), data.size(), MSG_NOSIGNAL | (block ? 0 : MSG_DONTWAIT)));
	}
	
	~socket_impl()
	{
		close(fd);
	}
};

static socket* socket_wrap(int fd)
{
	if (fd<0) return NULL;
	return new socket_impl(fd);
}

}

socket* socket::create_from_fd(int fd)
{
	return socket_wrap(fd);
}

socket* socket::create(cstring domain, int port)
{
	return socket_wrap(connect(domain, port));
}

//static socket* socket::create_async(const char * domain, int port);
//static socket* socket::create_udp(const char * domain, int port);

//int socket::select(socket* * socks, int nsocks, int timeout_ms)
//{
//	return -1;
//}


static MAYBE_UNUSED int socketlisten_create_ip4(int port)
{
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(port);
	
	int fd = mksocket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) goto fail;
	
	if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) goto fail;
	if (listen(fd, 10) < 0) goto fail;
	return fd;
	
fail:
	close(fd);
	return -1;
}

static int socketlisten_create_ip6(int port)
{
	struct sockaddr_in6 sa; // IN6ADDR_ANY_INIT should work, but doesn't.
	memset(&sa, 0, sizeof(sa));
	sa.sin6_family = AF_INET6;
	sa.sin6_addr = in6addr_any;
	sa.sin6_port = htons(port);
	
	int fd = mksocket(AF_INET6, SOCK_STREAM, 0);
	if (fd < 0) goto fail;
	
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, false) < 0) goto fail;
	if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) goto fail;
	if (listen(fd, 10) < 0) goto fail;
	return fd;
	
fail:
	close(fd);
	return -1;
}

socketlisten* socketlisten::create(int port)
{
	int fd = -1;
	if (fd<0) fd = socketlisten_create_ip6(port);
#if defined(_WIN32)
	//Windows XP can't dualstack the v6 addresses, so let's keep the fallback
	if (fd<0) fd = socketlisten_create_ip4(port);
#endif
	if (fd<0) return NULL;
	
	setblock(fd, false);
	return new socketlisten(fd);
}

socket* socketlisten::accept()
{
	return socket_wrap(::accept(this->fd, NULL,NULL));
}

socketlisten::~socketlisten() { close(this->fd); }

#include "../global.h"
#include "../containers.h"
#include "../function.h"
#include "../string.h"
#include <stdint.h>
#include <string.h>

//TODO: fiddle with https://github.com/ckennelly/hole-punch

#define socket socket_t
class socket : nocopy {
protected:
	socket(){}
	int fd; // Used by select().
	
	//deallocates the socket, returning its fd, while letting the fd remain valid
	static int decompose(socket* sock) { int ret = sock->fd; sock->fd=-1; delete sock; return ret; }
	
	static void setblock(int fd, bool newblock);
	
public:
	//Returns NULL on connection failure.
	static socket* create(cstring domain, int port);
	//Always succeeds. If the server can't be contacted, returns failure on first write or read.
	static socket* create_async(cstring domain, int port);
	//Always succeeds. If the server can't be contacted, may return e_broken at some point, or may just discard everything.
	static socket* create_udp(cstring domain, int port);
	
	enum {
		e_lazy_dev = -1, // Whoever implemented this socket layer was lazy and just returned -1. Treat it as e_broken or an unknown error.
		e_closed = -2, // Remote host chose to gracefully close the connection.
		e_broken = -3, // Connection was forcibly torn down.
		e_udp_too_big = -4, // Attempted to process an unacceptably large UDP packet.
		e_ssl_failure = -5, // Certificate validation failed, no algorithms in common, or other SSL error.
		e_not_supported = -6, // Attempted to read or write a listening socket, or other unsupported operation.
	};
	
	//WARNING: Most socket APIs treat read/write of zero bytes as EOF. Not this one! 0 is EWOULDBLOCK; EOF is an error.
	//The first two functions will process at least one byte, or if block is false, at least zero. send() sends all bytes before returning.
	//For UDP sockets, partial reads or writes aren't possible; you always get one or zero packets.
	virtual int recv(arrayvieww<byte> data, bool block = false) = 0;
	int recv(array<byte>& data, bool block = false)
	{
		if (data.size()==0) data.resize(4096);
		return recv((arrayvieww<byte>)data, block);
	}
	virtual int sendp(arrayview<byte> data, bool block = true) = 0;
	
	int send(arrayview<byte> bytes)
	{
		const byte * data = bytes.ptr();
		unsigned int len = bytes.size();
		unsigned int sent = 0;
		while (sent < len)
		{
			int here = this->sendp(arrayview<byte>(data+sent, len-sent));
			if (here<0) return here;
			sent += here;
		}
		return len;
	}
	
	//Convenience functions for handling textual data.
	//maybe<string> recvstr(bool block = false)
	//{
	//	maybe<array<byte>> ret = this->recv(block);
	//	if (!ret) return maybe<string>(NULL, ret.error);
	//	return maybe<string>((string)ret.value);
	//}
	int sendp(cstring data, bool block = true) { return this->sendp(data.bytes(), block); }
	int send(cstring data) { return this->send(data.bytes()); }
	
	//Returns an index to the sockets array, or negative if timeout expires. Negative timeout mean wait forever.
	//It's possible that an active socket returns zero bytes. However, this is rare; repeatedly select()ing and processing the data will eventually sleep.
	//(It may be caused by packets with wrong checksum, SSL renegotiation, or whatever.)
	//static int select(socket* * socks, unsigned int nsocks, int timeout_ms = -1);
	
	virtual ~socket() {}
	
	//Can be used to keep a socket alive across exec(). Don't use for an SSL socket.
	static socket* create_from_fd(int fd);
	int get_fd() { return fd; }
};

class socketssl : public socket {
protected:
	socketssl(){}
public:
	//If 'permissive' is true, the server certficate will be ignored.
	//Expired, self-signed, untrusted root, wrong domain, everything's fine.
	static socketssl* create(cstring domain, int port, bool permissive=false)
	{
		return socketssl::create(socket::create(domain, port), domain, permissive);
	}
	//On entry, this takes ownership of the socket. Even if connection fails, the socket may not be used anymore.
	//The socket must be a normal TCP socket (create_async is fine). UDP and nested SSL is not supported.
	static socketssl* create(socket* parent, cstring domain, bool permissive=false);
	
	//set_cert or set_cert_cb must be called before read or write.
	static socketssl* create_server(socket* parent);
	//Only usable on server sockets.
	void set_cert(array<byte> data); // Must be called exactly once.
	void set_cert_cb(function<void(socketssl* sock, cstring hostname)> cb); // Used for SNI. The callback must call set_cert.
	
	//Can be used to keep a socket alive across exec().
	//If successful, serialize() returns the the file descriptor needed to unserialize, and the socket is deleted.
	//On failure, negative return and nothing happens.
	virtual size_t serialize_size() { return 0; }
	virtual tuple<int, array<byte>> serialize() { return tuple<int, array<byte>>(-1, NULL); }
	static socketssl* unserialize(tuple<int, array<byte>> data);
};

//socket::select() works on these, but recv/send will fail
class socketlisten : public socket {
	socketlisten(int fd) { this->fd = fd; }
public:
	static socketlisten* create(int port);
	socket* accept();
	~socketlisten();
	
	int recv(arrayvieww<byte> data, bool block) { return e_not_supported; }
	int sendp(arrayview<byte> data, bool block) { return e_not_supported; }
};

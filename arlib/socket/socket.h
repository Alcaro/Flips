#include "../global.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define socket socket_t
class socket : nocopy {
protected:
	socket(){}
	int fd; // Used by select().
	
	//deallocates the socket, returning its fd, while letting the fd remain valid
	static int decompose(socket* sock) { int ret = sock->fd; sock->fd=-1; delete sock; return ret; }
	
public:
	//Returns NULL on connection failure.
	static socket* create(const char * domain, int port);
	//Always succeeds. If the server can't be contacted, returns failure on first write or read.
	static socket* create_async(const char * domain, int port);
	static socket* create_udp(const char * domain, int port);
	
	enum {
		e_lazy_dev = -1, // Whoever implemented this socket layer was lazy and just returned -1. Treat it as e_broken or an unknown error.
		e_closed = -2, // Remote host chose to gracefully close the connection.
		e_broken = -3, // Connection was forcibly torn down.
		e_udp_too_big = -4, // Attempted to process an unacceptably large UDP packet.
		e_ssl_failure = -5, // Certificate validation failed, no algorithms in common, or other SSL error.
	};
	
	//Negative means error, see above.
	//Positive is number of bytes handled.
	//WARNING: Unlike most socket layers, zero does not mean graceful close!
	// It means success, zero bytes processed, and is a valid byte count. Socket closed is in the error list above.
	//The first two functions will process at least one byte, or if block is false, at least zero. send() sends all bytes before returning.
	//block is ignored on Windows (always false), due to lack of MSG_NOWAIT and I don't want to do another syscall every time.
	//For UDP sockets, partial reads or writes aren't possible; you always get one or zero packets.
	virtual int recv(uint8_t* data, unsigned int len, bool block = true) = 0;
	virtual int sendp(const uint8_t* data, unsigned int len, bool block = true) = 0;
	int send(const uint8_t* data, unsigned int len)
	{
		unsigned int sent = 0;
		while (sent < len)
		{
			int here = sendp(data+sent, len-sent);
			if (here<0) return here;
			sent += here;
		}
		return len;
	}
	
	//Convenience functions for handling textual data.
	int recv(char* data, unsigned int len, bool block = false)
	{
		int ret = recv((uint8_t*)data, len-1, block);
		if (ret >= 0) data[ret]='\0';
		else data[0]='\0';
		return ret;
	}
	int sendp(const char * data, bool block = true) { return sendp((uint8_t*)data, strlen(data), block); }
	int send (const char * data) { return send((uint8_t*)data, strlen(data)); }
	
	//Returns an index to the sockets array, or negative if timeout expires.
	//Negative timeouts mean wait forever.
	//It's possible that an active socket returns zero bytes.
	//However, this is guaranteed to happen rarely enough that repeatedly select()ing will leave the CPU mostly idle.
	//(It may be caused by packets with wrong checksum, SSL renegotiation, or whatever.)
	static int select(socket* * socks, unsigned int nsocks, int timeout_ms = -1);
	
	virtual ~socket() {}
	
	//Can be used to keep a socket alive across exec(). Don't use for an SSL socket.
	static socket* create_from_fd(int fd);
	int get_fd() { return fd; }
};

class socketssl : public socket {
protected:
	socketssl(){}
public:
	//If 'permissive' is true, expired and self-signed server certificates will be accepted.
	//Other invalid certs, such as ones for a different domain, may or may not be accepted.
	static socketssl* create(const char * domain, int port, bool permissive=false)
	{
		return socketssl::create(socket::create(domain, port), domain, permissive);
	}
	//On entry, this takes ownership of the socket. Even if connection fails, the socket may not be used anymore.
	//The socket must be a normal TCP socket. UDP and nested SSL is not supported.
	static socketssl* create(socket* parent, const char * domain, bool permissive=false);
	
	
	virtual void q(){}
	
	//Can be used to keep a socket alive across exec().
	//If successful, serialize() returns the the file descriptor needed to unserialize, and the socket is deleted.
	//If failure, negative return and nothing happens.
	virtual size_t serialize_size() { return 0; }
	virtual int serialize(uint8_t* data, size_t len) { return -1; }
	static socketssl* unserialize(int fd, const uint8_t* data, size_t len);
};

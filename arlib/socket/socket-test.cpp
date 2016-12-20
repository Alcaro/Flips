#include "../arlib.h"
#include "../test.h"

//TODO:
//- fetch howsmyssl, ensure the only failure is the session cache

#ifdef ARLIB_TEST
//not in socket.h because this shouldn't really be used for anything, blocking is evil
static array<byte> recvall(socket* sock, unsigned int len)
{
	array<byte> ret;
	ret.resize(len);
	
	size_t pos = 0;
	while (pos < len)
	{
		int part = sock->recv(ret.slice(pos, (pos==0)?2:1), true); // funny slicing to ensure partial reads are processed sensibly
		assert_ret(part >= 0, NULL);
		assert_ret(part > 0, NULL); // this is a blocking recv, returning zero is forbidden
		pos += part;
	}
	return ret;
}

static void clienttest(socket* rs)
{
	//returns whether the socket peer speaks HTTP
	//discards the actual response, and since the Host: header is silly, it's most likely some variant of 404 not found
	//also closes the socket
	
	autoptr<socket> s = rs;
	assert(s);
	
	//in HTTP, client talks first, ensure this doesn't return anything
	array<byte> discard;
	discard.resize(1);
	assert(s->recv(discard) == 0);
	
	const char http_get[] =
		"GET / HTTP/1.1\n"
		"Host: example.com\n"
		"Connection: close\n"
		"\n";
	assert_eq(s->send(http_get), (int)strlen(http_get));
	
	array<byte> ret = recvall(s, 4);
	assert(ret.size() == 4);
	assert(!memcmp(ret.ptr(), "HTTP", 4));
}

test("plaintext client") { clienttest(socket::create("google.com", 80)); }
test("SSL client") { clienttest(socketssl::create("google.com", 443)); }
test("SSL SNI") { clienttest(socketssl::create("git.io", 443)); } // this server throws an error unless SNI is enabled
test("SSL permissiveness")
{
	autoptr<socket> s;
	assert(!(s=socketssl::create("badfish.filippo.io", 443))); // invalid cert root
	assert( (s=socketssl::create("badfish.filippo.io", 443, true)));
	assert(!(s=socketssl::create("172.217.18.142", 443)));       // invalid subject name, IP addresses don't have certs (this is Google)
	assert( (s=socketssl::create("172.217.18.142", 443, true))); // I'd use san.filippo.io, but that one is self-signed as well; I want only one failure at once
}

void listentest(const char * localhost, int port)
{
	autoptr<socketlisten> l = socketlisten::create(port);
	assert(l);
	autoptr<socket> c1 = socket::create(localhost, port);
	assert(c1);
	
#ifdef _WIN32
	//apparently the connection takes a while to make it through the kernel, at least on windows
	//socket* lr = l; // can't select &l because autoptr<socketlisten>* isn't socket**
	//assert(socket::select(&lr, 1, 100) == 0); // TODO: enable select()
	Sleep(50);
#endif
	autoptr<socket> c2 = l->accept();
	assert(c2);
	
	l = NULL;
	
	c1->send("foo");
	c2->send("bar");
	
	array<byte> ret;
	ret = recvall(c1, 3);
	assert(ret.size() == 3);
	assert(!memcmp(ret.ptr(), "bar", 3));
	
	ret = recvall(c2, 3);
	assert(ret.size() == 3);
	assert(!memcmp(ret.ptr(), "foo", 3));
}

test("listen on localhost") { listentest("localhost", 7777); }
test("listen on 127.0.0.1") { listentest("127.0.0.1", 7778); }
test("listen on ::1")       { listentest("::1", 7779); }
#endif

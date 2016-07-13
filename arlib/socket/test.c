#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#ifdef _WIN32
    #include <winsock2.h>
    #define socklen_t int
    #define sleep(x)    Sleep(x*1000)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h> 
#endif
#include "tlse.c"

void error(char *msg) {
    perror(msg);
    exit(0);
}

int send_pending(int client_sock, struct TLSContext *context) {
    unsigned int out_buffer_len = 0;
    const unsigned char *out_buffer = tls_get_write_buffer(context, &out_buffer_len);
    unsigned int out_buffer_index = 0;
    int send_res = 0;
    while ((out_buffer) && (out_buffer_len > 0)) {
        int res = send(client_sock, (char *)&out_buffer[out_buffer_index], out_buffer_len, 0);
        if (res <= 0) {
            send_res = res;
            break;
        }
        out_buffer_len -= res;
        out_buffer_index += res;
    }
    tls_buffer_clear(context);
    return send_res;
}

int validate_certificate(struct TLSContext *context, struct TLSCertificate **certificate_chain, int len) {
    int i;
    if (certificate_chain) {
        for (i = 0; i < len; i++) {
            struct TLSCertificate *certificate = certificate_chain[i];
            // check certificate ...
        }
    }
    //return certificate_expired;
    //return certificate_revoked;
    //return certificate_unknown;
    return no_error;
}

int main(int argc, char *argv[]) {
    int sockfd, portno, n;
    //tls_print_certificate("testcert/server.certificate");
    //tls_print_certificate("000.certificate");
    //exit(0);
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    char *ref_argv[] = {"", "google.com", "443"};
    if (argc < 3) {
        argv = ref_argv;
       //fprintf(stderr,"usage %s hostname port\n", argv[0]);
       //exit(0);
    }
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    signal(SIGPIPE, SIG_IGN);
#endif
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    struct TLSContext *context = tls_create_context(0, TLS_V12);
    tls_client_connect(context);
    send_pending(sockfd, context);
    unsigned char client_message[0xFFFF];
    int read_size;
    while ((read_size = recv(sockfd, client_message, sizeof(client_message) , 0)) > 0) {
        tls_consume_stream(context, client_message, read_size, validate_certificate);
        send_pending(sockfd, context);
        if (tls_established(context)) {
            const char * out = "GET / HTTP/1.1\nHost: example.com\nConnection: close\n\n";
            tls_write(context, out, strlen(out));
            send_pending(sockfd, context);

            unsigned char read_buffer[0xFFFF];
            int read_size = tls_read(context, read_buffer, 0xFFFF - 1);
            if (read_size > 0)
                fwrite(read_buffer, read_size, 1, stdout);
        }
    }
    return 0;
}

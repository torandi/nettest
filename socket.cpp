#include "socket.h"
#include "network_lib.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define DEBUG 0

/*
 * Wrapper functions to simplify socket change
 */

ssize_t read_raw(int sock,void * buffer,size_t len, int flags, addr_t * src_addr) {
	ssize_t size = recvfrom(sock,buffer,len, flags, (sockaddr*)&src_addr->addr, &src_addr->len);
	if(size < 0) {
		perror("read_raw()");
	}
	return size;
}

bool send_raw(int sockfd, void * data, const addr_t &to_addr) {
#if DEBUG
	printf("send_raw:: { ");
	for(int i=0;i<FRAME_SIZE; ++i) {
		if(((char*)data)[i] >= 32 && ((char*)data)[i] <=126) {
			printf("[%i=%c] ", ((char*)data)[i], ((char*)data)[i]);
		} else {
			printf("%i ", ((char*)data)[i]);
		}
	}
	printf("}\n");
#endif

	if(sendto(sockfd, data, FRAME_SIZE, 0, (sockaddr*) &to_addr.addr, sizeof(sockaddr_in))<0) {
		perror("send_raw()");
		return false;
	} else {
		return true;
	}
}

int create_udp_socket(int port, bool broadcast) {

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0) {
		perror("create socket()");
		exit(1);
	}

	int one = 1;
	if(broadcast) {
		if(setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(int)) == -1) {
			perror("create_socket(): setsockopt SO_BROADCAST");
			exit(1);
		}
	}

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1) {
		perror("create_socket(): setsockopt SO_REUSEADDR");
		exit(1);
	}

	int x;
	x=fcntl(sockfd,F_GETFL,0);
	fcntl(sockfd,F_SETFL,x | O_NONBLOCK);
	sockaddr_in addr;

	addr = create_addr(INADDR_ANY,port).addr;

	if(bind(sockfd, (sockaddr *) &addr, sizeof(sockaddr_in)) < 0) {
		perror("network(): bind");
		exit(1);
	}

	return sockfd;
}

int create_tcp_client(const char * hostname, int port) {
	int sockfd;
	sockaddr_in serv_addr;

	printf("Connecting to %s:%i\n", hostname, port);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)  {
		perror("ERROR opening socket");
		exit(1);
	}

	serv_addr = create_addr_from_hn(hostname, port).addr;


	if (connect(sockfd,(sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)  {
		perror("Connection failed");
		exit(1);
	}
	printf("Connected to %s:%i\n", hostname, port);

	int x;
	x=fcntl(sockfd,F_GETFL,0);
	fcntl(sockfd,F_SETFL,x | O_NONBLOCK);

	return sockfd;
}

int create_tcp_server(int port) {

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) {
		perror("create socket()");
		exit(1);
	}

	int one = 1;

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1) {
		perror("create_tcp_server(): setsockopt SO_REUSEADDR");
		exit(1);
	}


	sockaddr_in addr = create_addr(INADDR_ANY,port).addr;

	int x;
	x=fcntl(sockfd,F_GETFL,0);
	fcntl(sockfd,F_SETFL,x | O_NONBLOCK);

	if (bind(sockfd, (sockaddr *) &addr, sizeof(addr)) < 0)  {
		perror("network(): bind");
		exit(1);
	}

	listen(sockfd,5);
	printf("Listening on port %i\n", port);

	return sockfd;
}

int accept_client(int sockfd) {
	sockaddr_in cli_addr;
	socklen_t clilen = sizeof(cli_addr);
	int newsockfd = accept(sockfd, (sockaddr *) &cli_addr, &clilen);
	if (newsockfd < 0) 
		return -1;

	int x;
	x=fcntl(newsockfd,F_GETFL,0);
	fcntl(newsockfd,F_SETFL,x | O_NONBLOCK);
	return newsockfd;
}

void close_socket(int sock) {
	close(sock);
}

std::string getpeer(int sock) {
	socklen_t len;
	struct sockaddr_storage addr;
	char ipstr[INET6_ADDRSTRLEN];

	len = sizeof addr;
	getpeername(sock, (struct sockaddr*)&addr, &len);

	// deal with both IPv4 and IPv6:
	if (addr.ss_family == AF_INET) {
		 struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		 inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
	} else { // AF_INET6
		 struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		 inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
	}
	return std::string(ipstr);
}

addr_t create_addr(uint32_t address, int port) {
	addr_t addr;
	bzero((char *) &addr.addr, sizeof(addr.addr));
	addr.addr.sin_family = AF_INET;
	addr.addr.sin_port = htons(port);
	addr.addr.sin_addr.s_addr = htonl(address);	
	return addr;
}

addr_t broadcast_addr(int port) {
	return create_addr(INADDR_BROADCAST,port);
}

addr_t create_addr_from_hn(const char * hostname, int port) {
	hostent * host;
	host = gethostbyname(hostname);
	if(host == NULL) {
		fprintf(stderr,"create_addr_from_hn(): No such host %s\n",hostname);
		addr_t a;
		a.len = 0;
		return a;
	}
	addr_t addr = create_addr(0,port);
	bcopy((char *)host->h_addr,
		(char *)&addr.addr.sin_addr.s_addr,
		host->h_length);
	
	return addr;
}

bool data_available(int sock, int wait_sec, int wait_usec) {
	fd_set readset;
	struct timeval tv;

	FD_ZERO(&readset);
	FD_SET(sock,&readset);

	tv.tv_sec = wait_sec;
	tv.tv_usec = wait_usec;

	return (select(sock+1,&readset,NULL,NULL,&tv) > 0);
}

std::string addr_t::hostname() {
	char dst[INET6_ADDRSTRLEN];
	inet_ntop(addr.sin_family, (void*)&addr.sin_addr, dst, INET6_ADDRSTRLEN);
	return std::string(dst);
}

/*********************************
 * This file contains functions to 
 * abstract the socket useage.
 *********************************/


#ifndef SOCKET_H
#define SOCKET_H


#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>

struct addr_t {
	sockaddr_in addr;
	socklen_t len;

	addr_t() {
		bzero((char *) &addr, sizeof(addr));
		len = sizeof(addr);	
	};

	std::string hostname();
};

/**
 * Reads raw data from the socket
 */
ssize_t read_raw(int sock,void * buffer,size_t len, int flags, addr_t * src_addr);
bool send_raw(int sockfd, void * data, const addr_t &to_addr);

int create_udp_socket(int port, bool broadcast);
int create_tcp_server(int port);
int create_tcp_client(const char * hostname, int port);
int accept_client(int sockfd);
void close_socket(int sock);
bool data_available(int sock, int wait_sec, int wait_usec);
void bind(int sockfd, int port);

std::string getpeer(int sock);

addr_t create_addr(uint32_t address, int port);
addr_t create_addr_from_hn(const char * hostname, int port);
addr_t broadcast_addr(int port);

#endif

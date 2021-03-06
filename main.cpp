#include <cstdio>
#include <cstdlib>
#include <getopt.h>

#include "server.h"
#include "client.h"
#include "socket.h"
#include "network_lib.h"

void show_usage() {
	printf("-s: server, -c: client\n");
}

int main(int argc, char* argv[]){
	int verbose_flag=0, network_port=PORT, client=0, server=0, replier=0;
	std::string master_hostname;
	static struct option long_options[] =
	{
		 {"port",    required_argument, 0, 'p' },
		 {"help",    no_argument,       0, 'h'},
		 {"client",    required_argument,     0, 'c'},
		 {"server",    no_argument,      0, 's'},
		 {"replier",    no_argument,      &replier, 'r'},
		 {"verbose",    no_argument,      &verbose_flag, 'v'},
		 {0, 0, 0, 0}
	};

	int option_index = 0;
	int c;

	while( (c=getopt_long(argc, argv, "p:hc:srv", long_options, &option_index)) != -1 ) {
	 switch(c) {
		 case 0:
			 break;
		 case 'p':
			 network_port = atoi(optarg);
			 printf("Set port to %i\n", network_port);
			 break;
		 case 's':
			 server = 1;
			 break;
		 case 'c':
			 client = 1;
			 master_hostname = std::string(optarg);
			 break;
		 case 'h':
			 show_usage();
			 exit(0);
		 default:
			 break;
	 }
	}

	if(client && (server || client) && (replier || server) && replier) {
		printf("Only one modeflag can be used\n");
		exit(2);
	}

	if(replier) {
		//run_replier(network_port, verbose_flag);
	} else if(server) {
		run_server(network_port, verbose_flag);
	} else {
		run_client(master_hostname, network_port, verbose_flag);
	}
}

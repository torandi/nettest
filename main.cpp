#include <cstdio>
#include <cstdlib>
#include <getopt.h>

#include "socket.h"
#include "network_lib.h"

int main(int argc, char* argv[]){
	int verbose_flag, network_port, client, server, replier;
	std::string hostname;
	static struct option long_options[] =
	{
		 {"port",    required_argument, 0, 'p' },
		 {"help",    no_argument,       0, 'h'},
		 {"client",    no_argument,      &client, 'c'},
		 {"server",    no_argument,      &server, 's'},
		 {"replier",    no_argument,      &replier, 'r'},
		 {"verbose",    no_argument,      &verbose_flag, 'v'},
		 {0, 0, 0, 0}
	};

	int option_index = 0;
	int c;

	while( (c=getopt_long(argc, argv, "p:hvcsr", long_options, &option_index)) != -1 ) {
	 switch(c) {
		 case 0:
			 break;
		 case 'p':
			 network_port = atoi(optarg);
			 printf("Set port to %i\n", network_port);
			 break;
		 case 'h':
			 show_usage();
			 exit(0);
		 default:
			 break;
	 }
	}

	if ( client && (argc - optind != 1)) {
		printf("User error!");
		exit(1);
	}


}

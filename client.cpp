#include <cstdio>
#include <cstdlib>
#include <string.h>

#include "client.h"
#include "network_lib.h"
#include "protocol.h"

void read();
void run_test(int test);

static int sck;
static int verbose;

static int run = 1;

static nw_var_t * vars;

const char * test_urls[] =  {
	"http://speedtest.tele2.net/1GB.zip",
	"http://speedtest.tele2.net/10GB.zip"
};

void run_client(std::string server, int port, int verbose_flag) {
	verbose = verbose_flag;	

	vars = new nw_var_t[PAYLOAD_SIZE-1]; //Can't be more that this many vars

	sck = create_tcp_client(server.c_str(), port);

	if(sck != -1) {
		while(run) {
			read();
		}
	} else {
		printf("Connection failed\n");
	}
}

void read() {
	frame_t f;
	if(data_available(sck,30,0)) {
		f = read_frame(sck, vars, NULL);
		switch(f.cmd) {
			case NW_CMD_BAI:
				printf("Server send bai\n");
				run = 0;
				break;
			case NW_CMD_INVALID:
				printf("Invalid cmd recieved or disconnected.\n");
				run = 0;
				break;
			case NW_CMD_ERROR:
				printf("Server  responed with error: %s\n", vars[0].str);
				break;
			case NW_CMD_DOWNLOAD:
				printf("Server order us to start tcp test %d!\n", vars[0].i);
				run_test(vars[0].i);
			default:
				break;
		}
	}
}

void run_test(int test) {
	const char * url = test_urls[test];
	printf("Starting download to %s\n", url);
	printf("Download completed\n");
	vars[0].i = 10;
	vars[1].i = 1024;
	send_frame(sck, no_addr, NW_CMD_DOWNLOAD_COMPLETE, vars);
}

#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <cmath>
#include <curl/curl.h> 
#include <signal.h>

#include "client.h"
#include "network_lib.h"
#include "protocol.h"

void read();
void run_test(unsigned int test);
size_t nowrite( char *ptr, size_t size, size_t nmemb, void *userdata);

static int sck;
static int verbose;

static int run = 1;

static nw_var_t * vars;

const char * test_urls[] =  {
	"http://speedtest.tele2.net/100MB.zip",
	"http://speedtest.tele2.net/1GB.zip",
	"http://speedtest.tele2.net/10GB.zip"
};

void sigterm_client(int sig) {
	send_frame(sck, no_addr, NW_CMD_BAI, vars);
};	

void run_client(std::string server, int port, int verbose_flag) {
	verbose = verbose_flag;	

	signal(SIGINT, &sigterm_client);
	vars = new nw_var_t[PAYLOAD_SIZE-1]; //Can't be more that this many vars

	sck = create_tcp_client(server.c_str(), port);

	if(sck != -1) {
		printf("Client ready\n");
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
				printf("Connection closed\n");
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

void run_test(unsigned int test) {
	if(test >= sizeof(test_urls)) {
		vars[0].set_str("Test number out of range\n");
		send_frame(sck, no_addr, NW_CMD_ERROR, vars);
		return;
	}
	const char * url = test_urls[test];
	printf("Starting download of %s\n", url);
	CURL * curl_hndl = curl_easy_init();
	if(curl_hndl != NULL) {
		curl_easy_setopt(curl_hndl, CURLOPT_WRITEFUNCTION, &nowrite);
		if(verbose) 
			curl_easy_setopt(curl_hndl, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(curl_hndl, CURLOPT_NOPROGRESS, 0);
		curl_easy_setopt(curl_hndl, CURLOPT_URL, url);
		int ret;
		if((ret = curl_easy_perform(curl_hndl))==0) {
			printf("Download completed\n");
			double tmp=0.0;
			curl_easy_getinfo(curl_hndl, CURLINFO_SPEED_DOWNLOAD, &tmp);
			vars[0].i = (uint32_t)round(tmp/1024.0);
			curl_easy_getinfo(curl_hndl, CURLINFO_SIZE_DOWNLOAD, &tmp);
			vars[1].i = (uint32_t)round(tmp/1024.0);
			send_frame(sck, no_addr, NW_CMD_DOWNLOAD_COMPLETE, vars);
		} else {
			printf("Download failed. Curl returned %d\n", ret);
			vars[0].str = new char[64];
			sprintf(vars[0].str, "Curl returned %d", ret);
			send_frame(sck, no_addr, NW_CMD_ERROR, vars);
		}
		curl_easy_cleanup(curl_hndl);
	} else {
		vars[0].set_str("Failed to initialize curl\n");
		send_frame(sck, no_addr, NW_CMD_ERROR, vars);
	}
}

/**
 * Spoof write function
 */
size_t nowrite( char *ptr, size_t size, size_t nmemb, void *userdata) {
	return size*nmemb;
}

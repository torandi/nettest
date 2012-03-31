#include <cstdlib>
#include <cstdio>
#include <pthread.h>
#include <ctime>
#include <vector>
#include <signal.h>

#include "server.h"
#include "protocol.h"
#include "network_lib.h"

struct test_result_t {
	unsigned char success;
	unsigned int speed;
	unsigned int filesize;
};

void check_new_connections();
void *input_handler(void * data);
void send_frame_to_all(nw_cmd_t cmd);
void read_all_clients(int in_test);
float to_mbps(uint32_t speed_in_kbytes_per_sec);
float to_mbytes(uint32_t kbytes);
void help();
void test(int t);
test_result_t summarize(int &succ);
void stop_clients();


static int verbose;

static int sck;

static std::vector<int> clients;

static pthread_t input_thread;

static int run = 1;
static int run_test = -1;

static nw_var_t * vars;


int completed_tests;
int active_clients;

test_result_t * test_results;

void sigterm(int sig) {
	run = 0;
}

void run_server(int network_port, int verbose_flag) {
	verbose = verbose_flag;

	signal(SIGINT, &sigterm);

	vars = new nw_var_t[PAYLOAD_SIZE-1]; //Can't be more that this many vars

	sck = create_tcp_server(network_port);

	pthread_create(&input_thread, NULL, *input_handler, NULL);

	while(run) {
		check_new_connections();
		
		if(run_test > -1) {
			test(run_test);
			run_test = -1;
		}
		
		read_all_clients(0);
	}
	stop_clients();
	close_socket(sck);
}

void stop_clients() {
	send_frame_to_all(NW_CMD_BAI);
	std::vector<int>::iterator it;
	for(it = clients.begin(); it<clients.end(); ++it) {
		close_socket(*it);
	}
}

void check_new_connections() {
	int sockfd = accept_client(sck);
	if(sockfd != -1) {
		printf("\nNew client connected: %s\n", getpeer(sockfd).c_str());
		clients.push_back(sockfd);
	}
}

void *input_handler(void * data) {
	char * cmd = new char[64];
	int num = 0;

	while(run) {

		if(run_test == -1) {
			size_t  c=0;
			int r;
			printf("nettest(%d clients)>> ", clients.size());
			r = getline(&cmd,&c, stdin);
			if(strncmp(cmd, "tcp", 3)==0) {
				sscanf(cmd, "tcp %d", &num);
				run_test = num;
			} else if(strncmp(cmd, "help", 4) == 0) {
				help();
			} else if(strncmp(cmd, "exit", 4) == 0) {
				run = 0;
			} else if(r == -1) {
				printf("exit\n");
				run = 0;
			} else {
				printf("Unknown command, run help for help\n");
			}
		}
	}
	return NULL;
}

void help() {
	printf("Help:\n");
	printf("tcp n: Run tcp n\n");
	printf("exit\n");
}

void send_frame_to_all(nw_cmd_t cmd) {
	std::vector<int>::iterator it;
	for(it = clients.begin(); it<clients.end(); ++it) {
		if(!send_frame(*it, no_addr, cmd, vars)) {
			clients.erase(it);
		}
	}
}

void read_all_clients(int in_test) {
	std::vector<int>::iterator it;
	frame_t f;
	for(it = clients.begin(); it<clients.end(); ++it) {
		if(data_available(*it,0,0)) {
			f = read_frame(*it, vars, NULL);
			switch(f.cmd) {
				case NW_CMD_BAI:
					printf("Client closed connection. ");
					send_frame(*it, no_addr, NW_CMD_BAI, vars);
					clients.erase(it);
					if(in_test) {
						test_results[completed_tests++].success = 0;
						printf("%d/%d done\n", completed_tests, active_clients);
					} else {
						printf("\n");
					}
					break;
				case NW_CMD_INVALID:
					printf("Invalid cmd recieved or client disconnected. ");
					clients.erase(it);
					if(in_test) {
						test_results[completed_tests++].success = 0;
						printf("%d/%d done\n", completed_tests, active_clients);
					} else {
						printf("\n");
					}
					break;
				case NW_CMD_ERROR:
					printf("Client responed with error: %s ", vars[0].str);
					if(in_test) {
						test_results[completed_tests++].success = 0;
						printf("%d/%d done\n", completed_tests, active_clients);
					} else {
						printf("\n");
					}
					break;
				if(in_test) {
					case NW_CMD_DOWNLOAD_COMPLETE:
						test_results[completed_tests].success = 1;
						test_results[completed_tests].speed = vars[0].i;
						test_results[completed_tests].filesize = vars[1].i;
						++completed_tests;
						printf("Client %d downloaded %.2fMb in %.2f Mbps, %d/%d done\n", completed_tests, to_mbytes(vars[1].i), to_mbps(vars[0].i), completed_tests, active_clients);
						break;
				}
				default:
					break;
			}
		}
	}
}

void test(int t) {
	printf("Sending command to run TCP test %d to %d clients\n", t, clients.size());

	completed_tests = 0;
	active_clients = clients.size();
	test_results = new test_result_t[active_clients];

	time_t test_start = time(NULL);

	vars[0].i = t;
	send_frame_to_all(NW_CMD_DOWNLOAD);

	while(active_clients > completed_tests) {
		read_all_clients(1);
	}
	time_t total_time = time(NULL)-test_start;
	int succ = 0;
	test_result_t res = summarize(succ);
	printf("Test suite completed. %d/%d clients succeded.\n", succ, active_clients);
	if(succ > 0) {
		float speed = res.filesize/total_time;
		printf("Total summarized speed: %.2fMbps\n", to_mbps(res.speed));
		printf("Total filesize %.2fMb\n", to_mbytes(res.filesize));
		printf("Total time %ld seconds \n", total_time);
		printf("Total calculated speed: %.2fMbps\n", to_mbps(speed));
	}
}

float to_mbps(uint32_t speed_in_kbytes_per_sec) {
	return (speed_in_kbytes_per_sec/1024.f)*8;
}

float to_mbytes(uint32_t kbytes) {
	return kbytes/1024.f;
}

test_result_t summarize(int &succ) {
	test_result_t res;
	res.filesize = 0;
	res.speed = 0;
	std::vector<int>::iterator it;
	succ = 0;
	for(int i=0; i<active_clients; ++i) {
		if(test_results[i].success == 1) {
			++succ;
			res.filesize+= test_results[i].filesize;
			res.speed += test_results[i].speed;
		}
	}
	return res;
}	

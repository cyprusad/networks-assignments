#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "router.h"

int send_init() {
  // send init package to the network emulator
  return 0;
}

void usage(int argc) {
  printf("Incorrect number of arguments. Required 4 but provided %d\n", --argc);
  printf("Usage:\n\trouter <router_id> <nse_host> <nse_port> <router_port>\n");
}

int main (int argc, char** argv) {
  int router_id;
  char nse_host[128]; //max size of nse_host
  int nse_port;
  int router_port;

  if (argc != 5) {
    // error incorrect number of arguments
    usage(argc);
  } else {
    //correct number of arguments, start the router
    router_id = atoi(argv[1]);
    strcpy(nse_host, argv[2]);
    nse_port = atoi(argv[3]);
    router_port = atoi(argv[4]);
    printf("router::main - Starting the router\n");
  
  }

 
  return 0;
}
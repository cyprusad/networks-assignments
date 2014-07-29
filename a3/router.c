#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "router.h"

char filename[64]; // global filename string


int router_log(char* data) {
  int res = 0;
  if (filename != NULL) {
    printf("log - Filename is defined %s\n", filename);
    FILE* fp = fopen(filename, "ab");
    printf("log - %s\n", data);
    if (fp != NULL) {
      if (fputs(data, fp) != EOF) {
        res = 1;
      }
      fclose(fp);
    }
  }
  return res; //return 1 if things went well
}


int send_init(router_id) {
  // send init package to the network emulator
  router_log("INIT");
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
    printf("main - Starting the router\n");

    // set filename
    sprintf(filename, "router%d.log", router_id);

    send_init(router_id);


  } 
  return 0;
}
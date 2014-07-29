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

// get sockaddr, IPv4 or IPv6:
void* get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

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


int send_init(struct addrinfo* p, int sockfd, int router_id) {
  int numbytes;

  // send init package to the network emulator
  struct pkt_INIT init;
  init.router_id = router_id;
  printf("send_init - init.router_id = %d\n", init.router_id);
  printf("send_init - size of init struct = %lu\n", sizeof(init));

  unsigned char* data = (unsigned char*)malloc(sizeof(init));
  memcpy(data, &init, sizeof(init));

  if ((numbytes = sendto(sockfd, data, sizeof(init), 0,
               p->ai_addr, p->ai_addrlen)) == -1) {
    perror("router: sendto");
    exit(1);
  }

  printf("send_init - we sent %d bytes to the NSE\n", numbytes);

  router_log("INIT\n");
  
  return 0;
}

void usage(int argc) {
  printf("Incorrect number of arguments. Required 4 but provided %d\n", --argc);
  printf("Usage:\n\trouter <router_id> <nse_host> <nse_port> <router_port>\n");
}

int main (int argc, char** argv) {
  int router_id;
  char nse_host[128]; //max size of nse_host
  char nse_port[10]; 
  int router_port;

  if (argc != 5) {
    // error incorrect number of arguments
    usage(argc);
    exit(1);
  } 

  //correct number of arguments, start the router
  router_id = atoi(argv[1]);
  strcpy(nse_host, argv[2]);
  strcpy(nse_port, argv[3]);
  router_port = atoi(argv[4]);
  printf("main - Starting the router\n");

  // set filename
  sprintf(filename, "router%d.log", router_id);

  // set up the datagram socket
  // socket related variables
  int sockfd;
  struct addrinfo hints, *servinfo, *p;
  int rv;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(nse_host, nse_port, &hints, &servinfo)) != 0) {
     fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
     return 1;
  }

  // loop through all the results and make a socket
  for(p = servinfo; p != NULL; p = p->ai_next) {
     if ((sockfd = socket(p->ai_family, p->ai_socktype,
             p->ai_protocol)) == -1) {
         perror("router: socket");
         continue;
     }
     break;
  }

  if (p == NULL) {
     fprintf(stderr, "router: failed to bind socket\n");
     return 2;
  }

  // send init to the NSE
  send_init(p, sockfd, router_id);

  //recv database from NSE
  char s[INET6_ADDRSTRLEN];
  unsigned char recvBuffer[64];
  struct sockaddr_storage their_addr;
  socklen_t addr_len;
  int numbytes;

  printf("main - R%d is waiting to receive the circuitDB from NSE\n", router_id);
  addr_len = sizeof(their_addr);
  if ((numbytes = recvfrom(sockfd, recvBuffer, 64 , 0,
          (struct sockaddr *)&their_addr, &addr_len)) == -1) {
    perror("main - recvfrom");
    exit(1);
  }

  printf("main - router got packet from %s\n",
          inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s));
  printf("main - router received packet which is %d bytes long\n", numbytes);

  return 0;
}
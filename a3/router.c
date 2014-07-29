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

/*  GLOBAL VARIABLES  */

// router specific variables
int router_id;
char nse_host[128]; //max size of nse_host = 128 (practical upper bound)
char nse_port[10]; 
int router_port;

char filename[64]; // filename string specific to the router
struct circuit_DB circuit; // circuit DB provided by the NSE 

// socket related variables
int sockfd;
struct addrinfo hints, *servinfo, *p;
int rv;

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

  // log this message
  char logging[30];
  sprintf(logging, "R%d:SEND - INIT\n", router_id);
  router_log(logging);
  
  return 0;
}

int send_hello(struct addrinfo* p, int sockfd, int router_id, struct circuit_DB* circuit) {
  int numbytes;

  if (circuit == NULL) {
    printf("send_hello - circuit DB wasn't correctly set up");
  }

  //send hello messages to neighbors
  unsigned int num_nbrs = circuit->nbr_link;
  struct pkt_HELLO greetings[num_nbrs];
  unsigned char* data = (unsigned char*)malloc(sizeof(struct pkt_HELLO));
  int i;
  char logging[50];
  for (i=0; i < num_nbrs; i++) {
    memset(data, 0, sizeof(struct pkt_HELLO)); //clear data from prev iteration
    struct pkt_HELLO hello;
    hello.router_id = router_id;
    hello.link_id = circuit->linkcost[i].link;
    memcpy(data, &hello, sizeof(hello)); //copy hello packet into data

    if ((numbytes = sendto(sockfd, data, sizeof(hello), 0,
                 p->ai_addr, p->ai_addrlen)) == -1) {
      perror("router: sendto");
      exit(1);
    }

    printf("send_hello - we sent %d bytes to the NSE\n", numbytes);

    // log this message
    sprintf(logging, "R%d:SEND - HELLO to link_id:%d\n", router_id, hello.link_id);
    router_log(logging);
    memset(&logging, 0, strlen(logging));
  }

  
  return 0;
}

void receive_circuitDB(int sockfd, int router_id) {
  char s[INET6_ADDRSTRLEN];
  unsigned char recvBuffer[64]; //max 44 bytes
  struct sockaddr_storage their_addr;
  socklen_t addr_len;
  int numbytes;

  addr_len = sizeof(their_addr);
  if ((numbytes = recvfrom(sockfd, recvBuffer, 64 , 0,
          (struct sockaddr *)&their_addr, &addr_len)) == -1) {
    perror("receive_circuitDB - recvfrom");
    exit(1);
  }

  printf("receive_circuitDB - router got packet from %s\n",
          inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s));
  printf("receive_circuitDB - router received packet which is %d bytes long\n", numbytes);

  memcpy(&circuit, recvBuffer, sizeof(circuit));

  // log the receipt of this message
  char logging[40];
  sprintf(logging, "R%d:RECEIVE - circuitDB from the emulator\n", router_id);
  router_log(logging);
}

void usage(int argc) {
  printf("Incorrect number of arguments. Required 4 but provided %d\n", --argc);
  printf("Usage:\n\trouter <router_id> <nse_host> <nse_port> <router_port>\n");
}

int main (int argc, char** argv) {


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
  receive_circuitDB(sockfd, router_id);

  // send hello to all the neighbours
  send_hello(p, sockfd, router_id, &circuit);

  //loop between receiving hellos and LSPDUs

  return 0;
}
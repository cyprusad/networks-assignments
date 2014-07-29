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
#include <unistd.h>

#include <map>

#include "router.h"

using namespace std;

/*  GLOBAL VARIABLES  */

// router specific variables
int router_id;
char nse_host[128]; //max size of nse_host = 128 (practical upper bound)
char nse_port[10]; 
int router_port;

char filename[64]; // filename string specific to the router
struct circuit_DB circuit; // circuit DB provided by the NSE 
map<int, int> nbr_ids; // if nbr_ids.count(link_id) > 0, then router_id is a neighbour, else nbr_ids[link_id] = router_id

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


int send_init(int router_id) {
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

int send_hello(int router_id, struct circuit_DB* circuit) {
  int numbytes;

  if (circuit == NULL) {
    printf("send_hello - circuit DB wasn't correctly set up");
  }
  printf("send_hello - circuit DB properly set up\n");

  //send hello messages to neighbors
  unsigned int num_nbrs = circuit->nbr_link;
  unsigned char* data = (unsigned char*)malloc(sizeof(struct pkt_HELLO));
  int i;
  for (i=0; i < num_nbrs; i++) {
    memset(data, 0, sizeof(struct pkt_HELLO)); //clear data from prev iteration
    struct pkt_HELLO hello;
    hello.router_id = router_id;
    hello.link_id = circuit->linkcost[i].link;
    memcpy(data, &hello, sizeof(hello)); //copy hello packet into data

    printf("send_hello - R%d sending hello on link number=%d\n", router_id, i);

    if ((numbytes = sendto(sockfd, data, sizeof(hello), 0,
                 p->ai_addr, p->ai_addrlen)) == -1) {
      perror("router: sendto");
      exit(1);
    }

    printf("send_hello - we sent %d bytes to the NSE\n", numbytes);

    // log this message
    char logging[50];
    sprintf(logging, "R%d:SEND - HELLO to link_id:%d\n", router_id, hello.link_id);
    router_log(logging);
  }

  
  return 0;
}

void receive_circuitDB(int router_id) {
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
  char logging[50];
  sprintf(logging, "R%d:RECEIVE - circuitDB from the emulator\n", router_id);
  router_log(logging);
}


void heavy_lifting(int router_id) {
  char s[INET6_ADDRSTRLEN];
  unsigned char recvBuffer[sizeof(struct pkt_LSPDU)]; //max of 5 ints
  struct sockaddr_storage their_addr;
  socklen_t addr_len;
  int numbytes;

  addr_len = sizeof(their_addr);
  if ((numbytes = recvfrom(sockfd, recvBuffer, sizeof(struct pkt_LSPDU) , 0,
          (struct sockaddr *)&their_addr, &addr_len)) == -1) {
    perror("heavy_lifting - recvfrom");
    exit(1);
  }

  printf("heavy_lifting - router got packet from %s\n",
          inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr *)&their_addr),
              s, sizeof s));
  printf("heavy_lifting - router received packet which is %d bytes long\n", numbytes);

  if (numbytes == sizeof(struct pkt_HELLO)) {
    printf("heavy_lifting - received HELLO\n");

    struct pkt_HELLO hello;
    memcpy(&hello, recvBuffer, sizeof(hello));
    // log receiving the HELLO
    char logging[124];
    sprintf(logging, "R%d:RECEIVE - pkt_HELLO from R%d and link_id=%d\n", router_id, hello.router_id, hello.link_id);
    router_log(logging);
    // generate the neighbors list
    if (nbr_ids.count(hello.link_id) > 0) {
      printf("heavy_lifting - R%d is already a neighbour\n", nbr_ids[hello.link_id]);
    } else {
      printf("heavy_lifting - R%d added as a neighbour\n", hello.router_id);
      nbr_ids[hello.link_id] = hello.router_id;
    }
    usleep(20);
    // send multiple LSPDU packets to the neighbors
    unsigned char* data = (unsigned char*)malloc(sizeof(struct pkt_LSPDU));
    map<int,int>::iterator it;
    int i;
    // for each neighbour who has said hello to us, we send the circuit DB that we have
    for (it = nbr_ids.begin(); it != nbr_ids.end(); ++it) {
      for (i=0; i < circuit.nbr_link; i++) {
        memset(data, 0, sizeof(struct pkt_LSPDU)); //clear data from prev iteration
        struct pkt_LSPDU pdu;
        pdu.sender = router_id;
        pdu.router_id = router_id;
        pdu.link_id = circuit.linkcost[i].link;
        pdu.cost = circuit.linkcost[i].cost;
        pdu.via = it->first; // the via is the link of the neighbour in the neighbours map 
        memcpy(data, &pdu, sizeof(pdu)); //copy pdu packet into data

        printf("heavy_lifiting - R%d sending PDU via link=%d\n", router_id, it->first);

        if ((numbytes = sendto(sockfd, data, sizeof(pdu), 0,
                     p->ai_addr, p->ai_addrlen)) == -1) {
          perror("router: sendto");
          exit(1);
        }

        printf("heavy_lifting :: send_pdu - we sent %d bytes to the NSE\n", numbytes);

        // log sending the LSPDU
        char logging[256];
        sprintf(logging, "R%d:SEND LSPDU packet via link_id=%d\n", router_id, pdu.via);
        router_log(logging);
      }
    }

  } else if (numbytes == sizeof(struct pkt_LSPDU)) {
    printf("heavy_lifting - received an LSPDU packet\n");
  } else {
    printf("heavy_lifting - received a bad packet\n");
    exit(1);
  }
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
  send_init(router_id);

  //recv database from NSE
  receive_circuitDB(router_id);
  usleep(250);

  // send hello to all the neighbours
  send_hello(router_id, &circuit);
  usleep(100);

  //loop between receiving hellos and LSPDUs
  while(1){
    heavy_lifting(router_id);
    usleep(250);
  }

  return 0;
}
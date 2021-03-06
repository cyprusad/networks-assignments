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

#include <string>
#include <map>
#include <sstream>
#include <cstring>
#include <climits>

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

map< int, map<int, int> > topology;
map< int, map<int, int> > routing_table;

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

// general purpose method to log to the file
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

// prepare the output for the topology state at this router
string prepare_topology(int router_id) {
  stringstream ss;
  map< int, map<int, int> >::iterator it1;
  map<int,int>::iterator it2;
  ss << "# Topology database" << endl;
  for (it1 = topology.begin(); it1 != topology.end() ; ++it1) {
    ss << "R" << router_id << " -> " << "R" << it1->first << " nbr link " << it1->second.size() << endl;
    for (it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
      ss << "R" << router_id << " -> " << "R" << it1->first << " link " << it2->first << " cost " << it2->second << endl;
    }
  }
  return ss.str();
}

// prepare the output for the routing table state at this router
string prepare_routing_table(int router_id) {
  stringstream ss;
  map< int, map<int, int> >::iterator it1;
  map<int,int>::iterator it2;
  ss << "# RIB" << endl;
  for (it1 = routing_table.begin(); it1 != routing_table.end() ; ++it1) {
    for (it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
      if (it1->first == router_id) {
        ss << "R" << router_id << " -> " << "R" << it1->first << " -> Local, 0" << endl; 
      } else if (it2->first == -1) {
        ss << "R" << router_id << " -> " << "R" << it1->first << " -> INF, INF"<< endl; 
      }else {
        ss << "R" << router_id << " -> " << "R" << it1->first << " -> " << "R" << it2->first << ", " << it2->second << endl;
      }
    }
  }
  return ss.str();
}

// send pkt_INIT to the NSE
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
  sprintf(logging, "R%d :: Send - INIT\n", router_id);
  router_log(logging);
  
  return 0;
}

// send pkt_HELLO to neighboring routers
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
    sprintf(logging, "R%d :: Send - pkt_HELLO to link_id:%d\n", router_id, hello.link_id);
    router_log(logging);
  }

  return 0;
}

// receive circuit_DB from the NSE
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
  sprintf(logging, "R%d :: Receive - circuitDB from the emulator\n", router_id);
  router_log(logging);

  // initialize topology
  int i;
  for (i = 0; i < circuit.nbr_link; i++) {
    int link_id = circuit.linkcost[i].link;
    int cost = circuit.linkcost[i].cost;
    if (topology.count(router_id) > 0) {
      topology[router_id][link_id] = cost;
    } else {
      map<int, int> link_cost_map;
      link_cost_map[link_id] = cost;
      topology[router_id] = link_cost_map;
    }
  }

  // initialize routing table
  for (i=1; i < 6; i++) {
    map<int,int> via_cost_map;
    if (i == router_id) {
      via_cost_map[-1] = 0;
    } else {
      via_cost_map[-1] = UINT_MAX; 
    }
    routing_table[i] = via_cost_map;
  }

  //log the change in topology & routing table
  string data;
  char log_output[4096];
  data = prepare_topology(router_id);
  strcpy(log_output, data.c_str());
  router_log(log_output);
  data = prepare_routing_table(router_id);
  strcpy(log_output, data.c_str());
  router_log(log_output); 
}

// return 0 if the received PDU doesn't udpate the topology
int update_topology(struct pkt_LSPDU* pdu) {
  if (topology.count(pdu->router_id) > 0) {
    if (topology[pdu->router_id].count(pdu->link_id) > 0) {
      return 0; // link already exists in the topology
    } else {
      topology[pdu->router_id][pdu->link_id] = pdu->cost;
    }
  } else {
    map<int,int> link_cost_map;
    link_cost_map[pdu->link_id] = pdu->cost;
    topology[pdu->router_id] = link_cost_map;
  }
  return 1; // change happened
}

int find_cost_of_path(int first_link, int start, int end) {
  map<int, map<int,int> >::iterator it;
  map<int,int>::iterator path;
  int cumulative_cost = 0;
  int i;
  cumulative_cost += topology[start][first_link];
  for (i = 1; i < 6; i++) {
    if (i != start) { //shortest path to ourself is zero
      path = topology[i].find(first_link);
      if (path != topology[i].end()) {
        if (i == end) {
          return cumulative_cost;
        } else {
          for(path=topology[i].begin(); path!=topology[i].end(); ++path) {
            cumulative_cost = find_cost_of_path(path->first, i, end);
          }
        }
      }
    }
  }
  return -1;
}


int parent_funct(int start, int end) {
  map<int,int>::iterator path;
  int cost;
  for(path=topology[start].begin(); path!=topology[start].end(); ++path) {
    cost = find_cost_of_path(path->first, start, end);
  }
  return cost;
}

// run the Dijkstra algorithm and find the shortest paths to all visible routers
int update_routing_table(int router_id) {
  map<int, map<int,int> >::iterator it;
  
  int i,j;
  int end_node;
  // shortest path for all routers from us
  for (i = 1; i < 6; i++) {
    if (i != router_id) { //shortest path to ourself is zero
      if ((it = topology.find(i)) != topology.end()) { 
        // the end router is actually present in our topology so we can find a path to it
        end_node = it->first;
        parent_funct(router_id, end_node);

      }
    }
  }

  return 0;
}

// multipurpose method to receive pkt_HELLO and pkt_LSPDU and respond accordingly
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
    sprintf(logging, "R%d :: Receive - pkt_HELLO from R%d and link_id=%d\n", router_id, hello.router_id, hello.link_id);
    router_log(logging);
    // generate the neighbors list
    if (nbr_ids.count(hello.link_id) > 0) {
      printf("heavy_lifting - R%d is already a neighbour\n", nbr_ids[hello.link_id]);
    } else {
      printf("heavy_lifting - R%d added as a neighbour\n", hello.router_id);
      nbr_ids[hello.link_id] = hello.router_id;
    }
    usleep(20);
    // send multiple LSPDU packets to the neighbor who said hello to us
    unsigned char* data = (unsigned char*)malloc(sizeof(struct pkt_LSPDU));
    int i;
    // when a neighbour has said hello to us, we send the circuit DB that we have to the neighbour
    for (i=0; i < circuit.nbr_link; i++) {
      memset(data, 0, sizeof(struct pkt_LSPDU)); //clear data from prev iteration
      struct pkt_LSPDU pdu;
      pdu.sender = router_id;
      pdu.router_id = router_id;
      pdu.link_id = circuit.linkcost[i].link;
      pdu.cost = circuit.linkcost[i].cost;
      pdu.via = hello.link_id; // the via is the link of the neighbour who said hello 
      memcpy(data, &pdu, sizeof(pdu)); //copy pdu packet into data

      printf("heavy_lifiting - R%d sending PDU via link=%d\n", pdu.sender, pdu.via);

      if ((numbytes = sendto(sockfd, data, sizeof(pdu), 0,
                   p->ai_addr, p->ai_addrlen)) == -1) {
        perror("router: sendto");
        exit(1);
      }

      printf("heavy_lifting :: send_pdu - we sent %d bytes to the NSE\n", numbytes);

      // log sending the LSPDU
      char logging[256];
      sprintf(logging, "R%d :: Send pkt_LSPDU - sender=%d, router_id=%d, link_id=%d, cost=%d, via=%d\n", router_id, pdu.sender, pdu.router_id, pdu.link_id, pdu.cost, pdu.via);
      router_log(logging);
    }

  } else if (numbytes == sizeof(struct pkt_LSPDU)) {
    printf("heavy_lifting - received an LSPDU packet\n");

    struct pkt_LSPDU pdu;
    memcpy(&pdu, recvBuffer, sizeof(pdu));
    // log receiving the LSPDU
    char logging[400];
    sprintf(logging, "R%d :: Received pkt_LSPDU - sender=%d, router_id=%d, link_id=%d, cost=%d, via=%d\n", router_id, pdu.sender, pdu.router_id, pdu.link_id, pdu.cost, pdu.via);
    router_log(logging);

    // update topology and routing table (return 1 if there is a change, if not then 0 - that way we will know if we should send PDUs forward)
    int res = update_topology(&pdu);
    if (res != 0) { // proceed only if there was an update to the topology
      // Dijkstra algorithm - shortest path
      res = update_routing_table(router_id);

      // log the changes of topology and routing table
      string log_str;
      char log_output[4096];
      log_str = prepare_topology(router_id);
      strcpy(log_output, log_str.c_str());
      router_log(log_output);
      log_str = prepare_routing_table(router_id);
      strcpy(log_output, log_str.c_str());
      router_log(log_output); 

      // send PDU to other routers if update happened
      // send to everyone except those who didn't send HELLO and the one who sent the LSPDU
      unsigned char* data = (unsigned char*)malloc(sizeof(struct pkt_LSPDU));
      int i;
      for (i=0; i < circuit.nbr_link; i++) {
        if (nbr_ids.count(circuit.linkcost[i].link) > 0) { // we have received a pkt_HELLO from this neighbour
          if (nbr_ids[circuit.linkcost[i].link] != pdu.sender) { //don't send PDU back to the sender
            pdu.sender = router_id;
            pdu.via = circuit.linkcost[i].link;
            memset(data, 0, sizeof(struct pkt_LSPDU)); //clear data from prev iteration
            memcpy(data, &pdu, sizeof(pdu)); //copy pdu packet into data

            printf("heavy_lifting - R%d relaying PDU via link=%d\n", pdu.sender, pdu.via);

            if ((numbytes = sendto(sockfd, data, sizeof(pdu), 0,
                         p->ai_addr, p->ai_addrlen)) == -1) {
              perror("router: sendto");
              exit(1);
            }

            printf("heavy_lifting :: relayed_pdu - we relayed %d bytes to the NSE\n", numbytes);

          }
        }
      }

      // log the sending of PDU
      sprintf(logging, "R%d :: Send pkt_LSPDU - sender=%d, router_id=%d, link_id=%d, cost=%d, via=%d\n", router_id, pdu.sender, pdu.router_id, pdu.link_id, pdu.cost, pdu.via);
      router_log(logging);
    } 
    

  } else {
    printf("heavy_lifting - received a bad packet\n");
    exit(1);
  }
}

// usage for the command line arguments
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
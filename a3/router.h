#define NBR_ROUTER 5 /* for simplicity we consider only 5 routers */

struct pkt_HELLO {
  unsigned int router_id; /* id of the router who sends the HELLO PDU */ 
  unsigned int link_id; /* id of the link through which it is sent */
};

struct pkt_LSPDU {
  unsigned int sender; /* sender of the LS PDU */
  unsigned int router_id; /* router id */
  unsigned int link_id; /* link id */
  unsigned int cost; /* cost of the link */
  unsigned int via; /* id of the link through which the LS PDU is sent */
};

struct pkt_INIT {
  unsigned int router_id;
};

struct link_cost {
  unsigned int link; 
  unsigned int cost;
};

struct circuit_DB {
  unsigned int nbr_link;
  struct link_cost linkcost[NBR_ROUTER];
  /* we assume that at most NBR_ROUTER links are attached to each router */
};
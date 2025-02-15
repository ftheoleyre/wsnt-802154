/**
 *  \file   routing_ideal.h
 *  \brief  ideal routing with IEEE 802.15.4 mode (uses the tree hierarchy)
 *  \author Fabrice Theoleyre
 *  \date   2011
 **/


#ifndef	__ROUTING_IDEAL_H__

#define __ROUTING_IDEAL_H__

//graph manipulation  & algos
//#include <igraph/igraph.h>



#include "802154_slotted.h"


//--------------- CONNECTIVITY METRIC ------------------

//returns the average number of node to remove before having a disconnected network
double routing_get_connectivity_metric_node_removal(call_t *c);

//returns the average number of links to remove before having a disconnected network
double routing_get_connectivity_metric_link_removal(call_t *c);
  

//--------------- GRAPH  ------------------


//------ directly handles the graph

//does this link exist?
short routing_edge_exist_in_graph(igraph_t **G_ptr, igraph_integer_t node_a, igraph_integer_t node_b);

//add an edge for routing (i.e. link can be used to route packets)
//return 1 if the link was really added (none existed)
short routing_edge_add_in_graph(igraph_t **G_ptr, igraph_integer_t node_a, igraph_integer_t node_b);

//remove an edge randomly
short routing_edge_remove_random_in_graph(igraph_t **G_ptr);

//remove an edge for routing (it is not anymore present)
//return 1 if the link was really removed (it existed)
short routing_edge_remove_in_graph(igraph_t **G_ptr, igraph_integer_t node_a, igraph_integer_t node_b);

//remove a vertex for routing (it is not anymore present)
short routing_node_remove_in_graph(igraph_t **G_ptr, igraph_integer_t node_a);

//------- modifications from MAC

//add an edge for routing (i.e. link can be used to route packets)
void routing_edge_add_from_mac(call_t *c, uint16_t node_a, uint16_t node_b);

//remove an edge for routing (it is not anymore present)
void routing_edge_remove_from_mac(call_t *c, uint16_t node_a, uint16_t node_b);





//--------------- INFO ------------------

//distance from the sink
short routing_get_depth(uint16_t src);


//--------------- NEXT HOP ------------------


//finds the next hop with an ideal routing protocol
uint16_t routing_ideal_get_next_hop(call_t *c, uint16_t dest);




#endif


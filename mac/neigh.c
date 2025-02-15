
/*
 *  neigh.c
 *  
 *
 *  Created by Fabrice Theoleyre on 12/04/11.
 *  Copyright 2011 CNRS. All rights reserved.
 *
 */

#include "neigh.h"



//-------------------------------------------
//			TIMEOUT
//-------------------------------------------

//memory release
void neigh_free(neigh_info  *neigh_elem){
    int             i;
    
    //memory for some das lists
    das_destroy(neigh_elem->beacons);
    for(i=0; i<ADDR_MULTI_NB; i++)
        das_destroy(neigh_elem->seqnum_rx[i]);
}

//remove obsolete entries
int neigh_timeout_remove(call_t *c, void *arg){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	uint64_t		date_obsolete = get_time() - NEIGH_TIMEOUT * tools_get_bi_from_bo(nodedata->sframe.BO);
	
	//tmp vars
	neigh_info		*neigh_elem = NULL;
	uint64_t		oldest = -1;
	
		
	//neigh removing is useless if we are too early in the simulation (uint64_t problems on boundaries with 'negative' values)
	if (get_time() <= NEIGH_TIMEOUT * tools_get_bi_from_bo(nodedata->sframe.BO)){
		scheduler_add_callback(NEIGH_TIMEOUT * tools_get_bi_from_bo(nodedata->sframe.BO), c, neigh_timeout_remove, NULL);
		return(0);
	}
	
	//remove obsolete entries
	das_init_traverse(nodedata->neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL) {
		
        //myself -> particular case -> should never be removed from the neighborhood table
		if (neigh_elem->addr_short == nodedata->addr_short){
            neigh_elem->last_rx = get_time();
        }
		
        //particular case -> I do not remove my children
        else if (neigh_elem->is_child){            
            tools_debug_print(c, "[NEIGH] %d, we cannot remove a child. We should have monitored its beacons or have a kind of keep-alive solution\n", neigh_elem->addr_short);
            neigh_elem->last_rx = get_time();      
        }
        
        //oboslete node    
        else if (neigh_elem->last_rx <= date_obsolete){
			tools_debug_print(c, "[NEIGH] node %d removed\n", neigh_elem->addr_short);			
			
			//BUG
			if (neigh_elem->addr_short == nodedata->parent_addr_current)
				tools_exit(3, c, "[NEIGH] we cannot remove the current parent %d. We should have received a beacon, and it cannot be timeouted\n", neigh_elem->addr_short);

			
			//remove corresponding parent if one exists
			if (ctree_parent_get_info(nodedata->parents, neigh_elem->addr_short) != NULL){
                tools_debug_print(c, "[ASSOC] timeouted neighbor. We remove this parent\n");
				mgmt_parent_disassociated(c, neigh_elem->addr_short);			
            }
			
            //delete the entry and release the corresponding memory
            neigh_free(neigh_elem);
            das_delete(nodedata->neigh_table, neigh_elem);
            
			//reinit for suppression
			das_init_traverse(nodedata->neigh_table);
            neigh_elem = NULL;
		}
                
		//saves the oldest entry
		if ((neigh_elem != NULL) && ((oldest == -1) || (neigh_elem->last_rx < oldest)))
			oldest = neigh_elem->last_rx;		
	}

	//schedules the next verification
	if (oldest != -1){
		oldest += NEIGH_TIMEOUT * tools_get_bi_from_bo(nodedata->sframe.BO);
		scheduler_add_callback(oldest, c, neigh_timeout_remove, NULL);
		
		char	msg[150];		
		tools_debug_print(c, "[NEIGH] next timeout verification at %s\n", tools_time_to_str(oldest, msg, 150));
	}
	return 0;
}




//-------------------------------------------
//			NEIGH TABLE - INFO
//-------------------------------------------


//Create a neighbor without info
neigh_info *neigh_create_unitialized_neigh(uint16_t addr_short){
    int         i;
    neigh_info  *neigh_elem = malloc(sizeof(neigh_info));			

    //the address
    neigh_elem->addr_short  = addr_short;
    
    //default values
    neigh_elem->beacons         = das_create();
    neigh_elem->is_child        = FALSE;
    neigh_elem->hops            = INVALID_DISTANCE;
    neigh_elem->depth           = INVALID_DISTANCE;
    neigh_elem->has_a_child     = INVALID_BOOLEAN;
    neigh_elem->bop_slot        = BOP_SLOT_INVALID;
    neigh_elem->sf_slot         = SF_SLOT_INVALID;
    neigh_elem->multicast_to_rx = FALSE;
    
    //seqnums
    for(i=0; i<ADDR_MULTI_NB; i++){
        neigh_elem->seqnum_rx[i] = das_create();
  
        neigh_elem->seqnum_beacons[i].seqnum_min = SEQNUM_INVALID;
        neigh_elem->seqnum_beacons[i].seqnum_max = SEQNUM_INVALID;
    }   
    return(neigh_elem);
}



//update the information about a neighbor
short neigh_set(neigh_info *neigh_elem, uint16_t addr_short, uint8_t sf_slot, uint8_t bop_slot, uint8_t depth, short has_a_child, short hops, uint64_t last_rx){
    
    short has_changed = (
                           (neigh_elem->sf_slot	!= sf_slot && sf_slot != SF_SLOT_INVALID)
                           ||
                           (neigh_elem->depth != depth && depth != INVALID_DISTANCE)
                           ||
                           (neigh_elem->has_a_child	!= has_a_child && has_a_child != INVALID_BOOLEAN)
                           ||
                           (neigh_elem->hops != hops && hops != INVALID_DISTANCE)                           
                           );
    
    neigh_elem->addr_short      = addr_short;
    neigh_elem->sf_slot			= sf_slot;	
    neigh_elem->bop_slot		= bop_slot;	
    neigh_elem->depth			= depth;
    neigh_elem->has_a_child		= has_a_child;
    neigh_elem->hops			= hops;
    neigh_elem->last_rx			= last_rx;
    
    return(has_changed);
   
}

//is that a ctree neighbor?
short neigh_is_ctree_neigh(call_t *c, uint16_t addr){
	nodedata_info *nodedata = (nodedata_info*) get_node_private_data(c);
    
    if (neigh_is_a_child(nodedata->neigh_table, addr))
        return(TRUE);
    else return(ctree_parent_get_info(nodedata->parents, addr) != NULL); 
}

//is this node one child?
short neigh_is_a_child(neigh_info *neigh_table, uint16_t addr_short){
	neigh_info	*neigh_elem = NULL;
    
	das_init_traverse(neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(neigh_table)) != NULL) {
        if (neigh_elem->addr_short == addr_short)
            return(neigh_elem->is_child);

    }
    
    return(FALSE);
}


//how many children are there in this neigh table?
int neigh_table_get_nb_children(neigh_info *neigh_table){
	int	nb_children = 0;

	//temporary pointer
	neigh_info	*neigh_elem = NULL;
	
	//does this neighbor already exist in the neigh table?
	das_init_traverse(neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(neigh_table)) != NULL)
        if (neigh_elem->is_child)
			nb_children++;
	
	return(nb_children);
}


//returns the searched^th child
uint16_t neigh_table_get_child(neigh_info *neigh_table, int searched){
   	int	nb_children = 0;
	
	//temporary pointer
	neigh_info	*neigh_elem = NULL;
	
	//does this neighbor already exist in the neigh table?
	das_init_traverse(neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(neigh_table)) != NULL) {
		
        if ((nb_children == searched) && (neigh_elem->is_child))
            return(neigh_elem->addr_short);        
        
        if (neigh_elem->is_child)
			nb_children++;
	}	
	return(ADDR_INVALID_16);    
}


//do I have a child or not?
short neigh_table_I_have_a_child(call_t *c){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	
	//temporary pointer
	neigh_info	*neigh_elem;
	
	//does this neighbor already exist in the neigh table?
	das_init_traverse(nodedata->neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL) {
		if (neigh_elem->is_child)
			return(TRUE);
	}
    
	//I may have pending association requests in my buffer
	return(0); //nodedata->buffer_mgmt->size != 0);
}

	
//returns the entry associated to this address
neigh_info *neigh_table_get(call_t *c, uint16_t addr_short){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	
	//control
	neigh_info	*neigh_elem;
	
	//does this neighbor exist in the neigh table?
	das_init_traverse(nodedata->neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL) {
		
		if (neigh_elem->addr_short == addr_short)
			return(neigh_elem);
	}
	
	return(NULL);
}

//returns the nb of 1-neighbors
int neigh_table_get_nb_neigh(call_t *c){
	nodedata_info *nodedata = (nodedata_info*) get_node_private_data(c);
	neigh_info	*neigh_elem;
	int		nb = 0;
	
	das_init_traverse(nodedata->neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL) {
		
		if (neigh_elem->hops == 1)
			nb++;
	}
	
	return(nb);		
}


//the entry for myself in the neightable (0-neighbor)
void neigh_update_myself(call_t *c){
 	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	neigh_info  *neigh_elem;
    
    //the element does not exist
    if ((neigh_elem = neigh_table_get(c, nodedata->addr_short)) == NULL){
        neigh_elem = neigh_create_unitialized_neigh(nodedata->addr_short);
        das_insert(nodedata->neigh_table, neigh_elem);
    }
    
    neigh_elem->addr_short		=	nodedata->addr_short;
    neigh_elem->sf_slot         =   nodedata->sframe.my_sf_slot;
    neigh_elem->bop_slot        =   nodedata->sframe.my_bop_slot;
    neigh_elem->has_a_child     =   neigh_table_I_have_a_child(c);
    neigh_elem->hops            =   0;
    neigh_elem->is_child		=	FALSE;
    neigh_elem->hops            =   0;
    neigh_elem->last_rx			=	get_time();
    
}




//returns the link metric toward this node
double neigh_get_link_metric_for(call_t *c, uint16_t addr){
    
    switch (param_get_depth_metric()){
        
        //Number of hops
        case DEPTH_HOPS:
  
            return(1.0);
            break;
            
        //Expected Transmission Count (any packet)
        case DEPTH_ETX:
            printf("bad metric\n");
            exit(5);
            break;
            
            
        //Beacon Delivery Ratio
        //case DEPTH_BDR:
            
            
        //PHY BER
        case DEPTH_BER:
            ;
            double  dist, rxdBm;
            double  dist0 = 2.0;        //parameters for the shadowing model
            double  pathloss = 2.19;    //idem
            double  Pr_dBm0 = -53.4;    //dBm, idem           
            double  deviation = 2.0;    //idem
            /*
             *  Pr_dBm(d) = Pr_dBm(d0) - 10 * beta * log10(d/d0) + X
             *
             *  Note: rxdBm = [Pt + Gt + Gr]_dBm, L = 1, and X a normal distributed RV (in dBm)
             *
             *  cf p104-105 ref "Wireless Communications: Principles and Practice", Theodore Rappaport, 1996.
             *
             */

            //received power
            dist = tools_distance(c->node, addr);
            rxdBm = Pr_dBm0 + -10.0 * pathloss * log10(dist/dist0);

            //probability this rx power is under the reception threshold
            double rx_threshold = -95;  //dBm

            //random part of the received signal
            //X = normal(0.0, deviation);
             
            // Pr [ x < rxdbm - threshold ] = 1/2 * ( 1 + erf( (rxdbm - threshold) / (dev * \sqrt(2))
            // Pr [ x > rxdbm - threshold ] = 1 - Pr [ x < rxdbm - threshold ]
            double ber = 1.0 - 0.5  * (1 + erf((rxdBm - rx_threshold) / (sqrt(2) * deviation)  ));
            
            tools_debug_print(c, "ber=%f\n", ber);
            tools_debug_print(c, "weight=%f\n", 1.0 / (1.0 - ber));
            
            //nb of transmissions -> inverse of the sucess probability
            //NB: the units are DEPTH_UNIT (i.e. not any double value is authorized)
            int metric =  (1.0 / DEPTH_UNIT) / (1.0 - ber);
            
            return((double) metric * DEPTH_UNIT);            
            
            break;
        default:
            tools_debug_print(c, "[DEPTH] this depth metric (%d) is not implemented\n", param_get_depth_metric());
            exit(3);
            break;
    }            
}

//-------------------------------------------
//			BEACON RX
//-------------------------------------------
	

//returns 1 if the element has an older date
int neigh_date_cmp(void* elem, void* time_ptr){
	uint64_t	time = *(uint64_t*)time_ptr;
	beacon_info	*beacon_elem = elem;
	
	//this is an old entry compared to time_ptr
	//NB: a uint64_t can roll -> thus we have to take the relative unsigned difference
	if (beacon_elem->time <= time){
		//	printf("I should remove %d (%llu)\n", beacon_elem->seq_num, beacon_elem->time);
		return(1);
	}
	return(0);
}


//inserts a new neighbor if none exists, and update its info if it is at a smaller distance
void neigh_table_add_neigh_from_sfbop_table(call_t *c, uint16_t addr_short, uint8_t sf_slot, uint8_t bop_slot, int hops){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	neigh_info	*neigh_elem;
    short     neigh_table_has_changed = FALSE;
    
	//I don't have to add my own address in the neigh table!
	if (nodedata->addr_short == addr_short)
		return;
	
	//does this neighbor already exist in the neigh table?
	short found = FALSE;
	das_init_traverse(nodedata->neigh_table);	
	while ((!found) && ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL)) {
		
		//it exists, let maintain elem pointed to this entity
		if (neigh_elem->addr_short == addr_short){
			found = TRUE;
		}
		
		//the saved entry is closer -> discard the new one
		if ((neigh_elem->addr_short == addr_short) && (neigh_elem->hops < hops)){
		//	tools_debug_print(c, "[NEIGH] %d already exists in the neigh table and is closer\n", addr_short);
			return;
		}
	}	

	//else, create a new neighbor in the table
	if (!found){
		//scheduling timeout verification
		if (das_getsize(nodedata->neigh_table) == 0)
			scheduler_add_callback(get_time() + NEIGH_TIMEOUT * tools_get_bi_from_bo(nodedata->sframe.BO), c, neigh_timeout_remove, NULL);
		
		//actual insertion
        neigh_elem = neigh_create_unitialized_neigh(addr_short);
        das_insert(nodedata->neigh_table, neigh_elem);        
        neigh_table_has_changed = TRUE;
        tools_debug_print(c, "[NEIGH] creation of neighbor %d\n", addr_short);
	}
	
    //Does the neighbor have the same info?
    if (!neigh_table_has_changed){
        if ((neigh_elem->sf_slot != sf_slot) || (neigh_elem->bop_slot != bop_slot)){
            neigh_table_has_changed = TRUE;         
            tools_debug_print(c, "[NEIGH] sf/bop slot has changed for neighbor %d\n", neigh_elem->addr_short);
        }
            
        if ((!neigh_table_has_changed) && (neigh_elem->hops != hops) && (hops == 1 || neigh_elem->hops == 1)){
            neigh_table_has_changed = TRUE;    
            tools_debug_print(c, "[NEIGH] distance has changed for neighbor %d\n", neigh_elem->addr_short);
        }
    }
    
    //neigh table has changed -> we must enqueue a new hello (replacing possibly old obsolete hellos)
     if (neigh_table_has_changed)
        neigh_hello_enqueue(c);
     
	
	//--- INFO
    neigh_set(neigh_elem, addr_short, sf_slot, bop_slot, -1, TRUE, hops, get_time());
    
//	tools_debug_print(c, "[NEIGH] %d info updated (sf %d bop %d)\n", addr_short, sf_slot, bop_slot);
}

 


//adds or updates a neighbor after a beacon is received
void neigh_table_add_after_beacon(call_t *c, packet_t *beacon_pk){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	//	call_t c_radio			= {get_entity_bindings_down(c)->elts[0], c->node, c->entity};
	//	uint64_t	tx_time		= beacon_pk->real_size * radio_get_Tb(&c_radio);
	//	uint64_t	beacon_time	= get_time() - tx_time;
	
	//temporary pointer
	neigh_info	*neigh_elem;
	beacon_info	*beacon_elem;
	short		found = FALSE;
	short     neigh_table_has_changed = FALSE;
	
	//packet
	_802_15_4_Beacon_header *hdr_beacon = (_802_15_4_Beacon_header *) (beacon_pk->data + sizeof(_802_15_4_header));
	
	//does this neighbor already exist in the neigh table?
	das_init_traverse(nodedata->neigh_table);	
	while ((!found) && ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL)) {
		
		//it exists, let maintain elem pointed to this entity
		if (neigh_elem->addr_short == hdr_beacon->src){
			found = TRUE;
		}
	}	
	//else, create a new neighbor in the table
	if (!found){
		//scheduling timeout verification
		if (das_getsize(nodedata->neigh_table) == 0)
			scheduler_add_callback(get_time() + NEIGH_TIMEOUT * tools_get_bi_from_bo(nodedata->sframe.BO), c, neigh_timeout_remove, NULL);
		
        //new entry for this node
        neigh_elem = neigh_create_unitialized_neigh(hdr_beacon->src);
        das_insert(nodedata->neigh_table, neigh_elem);    
   
		neigh_table_has_changed = TRUE;
		tools_debug_print(c, "[NEIGH] neighbor created after beacon reception\n");
	}
    
	//update this neighbor in my neigh table
    neigh_table_has_changed  = neigh_set(neigh_elem, hdr_beacon->src, pk_beacon_get_sf_slot(beacon_pk), pk_beacon_get_bop_slot(beacon_pk), pk_beacon_get_depth(beacon_pk), pk_beacon_get_has_a_child(beacon_pk), 1, get_time());
    
    //neigh table has changed -> we must generate a new hello
    if (neigh_table_has_changed)
        neigh_hello_enqueue(c);        
        
    //we received a beacon from its node -> it keeps on being alive
    neigh_elem->last_rx = get_time();
    
    
	//--- Beacon_seqnum TABLE to compute the PDR of beacons

	//inserts this sequence number in the list
	beacon_elem = (beacon_info*)malloc(sizeof(beacon_info));
	beacon_elem->seq_num	= pk_get_seqnum(beacon_pk);
	beacon_elem->time		= get_time();
	das_insert(neigh_elem->beacons, (void*) beacon_elem);	
	
	//remove oldest seq nums
	uint64_t	time_past;
	if (get_time() < tools_get_bi_from_bo(nodedata->sframe.BO) * BEACONS_FOR_STATS)
		time_past = 0;
	else
		time_past = get_time() - tools_get_bi_from_bo(nodedata->sframe.BO) * BEACONS_FOR_STATS;
	das_selective_delete(neigh_elem->beacons, neigh_date_cmp, &time_past);
	
	
	
	tools_debug_print(c, "[NEIGH] %d updated after beacon reception\n", hdr_beacon->src);
}




//-------------------------------------------
//		NEIGH ADD AFTER (DIS)ASSOCIATION
//-------------------------------------------



//maintains a list of my children
void neigh_table_add_child_after_assoc(call_t *c, packet_t *packet_rcvd){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
		
	//temporary pointer
	neigh_info	*neigh_elem;
	short		found = FALSE;
    uint16_t    addr_short;

	//this is not a command packet
	if (pk_get_ftype(packet_rcvd) != MAC_COMMAND_PACKET)
		return;

	//This must be an association reply/req
	_802_15_4_COMMAND_header	*hdr_cmd	= (_802_15_4_COMMAND_header *) (packet_rcvd->data + sizeof(_802_15_4_header));
	switch(hdr_cmd->type_command){
			
		case ASSOCIATION_RESPONSE :
			addr_short = pk_get_dst_short(packet_rcvd);

			break;
			
		case ASSOCIATION_REQUEST:
			addr_short = pk_get_src_short(packet_rcvd);

			break;
			
		default:
			return;
	}
    tools_debug_print(c, "[NEIGH] child %d inserted in the neighborhood table\n", addr_short);

 
	//does this neighbor already exist in the neigh table?
	das_init_traverse(nodedata->neigh_table);	
	while ((!found) && ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL)) {
		
		//it exists, let maintain elem pointed to this entity
		if (neigh_elem->addr_short == addr_short){
			found = TRUE;
		}
	}

	
    //else, create a new neighbor in the table
	if (!found){
		//scheduling timeout verification
		if (das_getsize(nodedata->neigh_table) == 0)
			scheduler_add_callback(get_time() + NEIGH_TIMEOUT * tools_get_bi_from_bo(nodedata->sframe.BO), c, neigh_timeout_remove, NULL);

		//actual insertion
        neigh_elem = neigh_create_unitialized_neigh(addr_short);
        das_insert(nodedata->neigh_table, neigh_elem);  
 		tools_debug_print(c, "[NEIGH] a neighbor is created after the association\n");
		
	}

	//update the content
	neigh_elem->addr_short			= addr_short;
	neigh_elem->sf_slot				= SF_SLOT_INVALID;					//not used for a child
	neigh_elem->bop_slot			= BOP_SLOT_INVALID;					//not used for a child
	neigh_elem->depth				= ctree_compute_my_depth(c) + 1;	//this a child!
	neigh_elem->is_child			= TRUE;
	neigh_elem->has_a_child			= 0;		//I don't care, we don't have the same superframe slot
	neigh_elem->hops				= 1;
	neigh_elem->last_rx				= get_time();
	
	
	//NB: we don't maintain a list of received beacons for a child
}


//----- REMOVE AFTER DISASSOCIATION ----


//remove a child
void neigh_table_remove_after_disassoc(call_t *c, packet_t *disassoc_notif){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	
	//temporary pointer
	neigh_info	*neigh_elem;
	short		found = FALSE;
	
	//this is not a command packet
	if (pk_get_ftype(disassoc_notif) != MAC_COMMAND_PACKET)
		return;	
	
	//This must be an disassociation notif
	_802_15_4_COMMAND_header	*hdr_cmd	= (_802_15_4_COMMAND_header *) (disassoc_notif->data + sizeof(_802_15_4_header));
	if (hdr_cmd->type_command != DISASSOCIATION_NOTIFICATION)
		return;
	
	//packet
	_802_15_4_DISASSOC_header	*header_disassoc	= (_802_15_4_DISASSOC_header *) (disassoc_notif->data + sizeof(_802_15_4_header));
	
	//does this neighbor already exist in the neigh table?
	das_init_traverse(nodedata->neigh_table);	
	while ((!found) && ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL)) {
			if (neigh_elem->addr_short == header_disassoc->src){
			found = TRUE;
		}
	}	
	
	//no neighbor corresponds (SHOULD not happen, but COULD if timeouts are short and the child is retransmitting its dissasoc notif)
	if (!found){
		tools_debug_print(c, "[NEIGH] BUG child %d not present in the neighborhood table\n", header_disassoc->src);
		return;
	}
    	
	//update the content
	neigh_elem->is_child			= FALSE;
	neigh_elem->last_rx				= get_time();	
	tools_debug_print(c, "[NEIGH] child %d removed in the neighborhood table (now a 'normal' neighbor)\n", header_disassoc->src);
}




//-------------------------------------------
//			NEIGH TABLE UPDATE last rx
//-------------------------------------------



//we received one packet from this node
void neigh_table_update_last_rx_for_addr(call_t *c, uint16_t addr){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	
	//control
	neigh_info	*neigh_elem;

	//does this neighbor already exist in the neigh table?
	das_init_traverse(nodedata->neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL) {
		
		//it exists, let maintain elem pointed to this entity
		if (neigh_elem->addr_short == addr){
			neigh_elem->last_rx = get_time();
			return;
		}
	}
	//Unkwnown neighbor
	tools_debug_print(c, "[NEIGH] unknown neighbor %d. We do not update the neighborhood table\n", addr);
	return;
}





//-------------------------------------------
//			SF SLOT MONITORING
//-------------------------------------------



//returns the next superframe slot we have to listen to
//returns 0 if no sfslot has to be scanned
uint64_t neigh_get_next_sfslot_to_listen(call_t *c){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	
	//control
	neigh_info	*neigh_elem;
	
	//the next beacon to listen
	uint64_t	time_sf_next = 0;
	uint64_t	time_sf_tmp;
    
    //debug
    short       to_scan = FALSE;
    uint16_t    node = ADDR_INVALID_16;
	
//	tools_debug_print(c, "current sf slot %d\n", tools_compute_sfslot(get_time()));
	
	
	//what is the closest sf slot in the neigh table?
	das_init_traverse(nodedata->neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL) {
		time_sf_tmp = beacon_compute_next_sf(neigh_elem->sf_slot);
		
		//this neighbor has a closer beacon
		if (neigh_elem->hops == 1)
			if ((time_sf_next == 0) || (time_sf_tmp < time_sf_next)){
				time_sf_next = time_sf_tmp;
                node = neigh_elem->addr_short;
                to_scan = FALSE;
                
				//tools_debug_print(c, "[NEIGH] node %d uses sfslot %d\n", 
                //                  neigh_elem->addr_short, 
                //                  neigh_elem->sf_slot);
			}
	}
	
	
	//searches for the closest unscanned sfslot
	int sfslot;
    if (get_time() > param_get_sfslot_scan_period())
        for(sfslot=0; sfslot<param_get_nb_sfslot(); sfslot++){
		
            //the next time this sfslot will occur 
            uint64_t time_sfslot = beacon_compute_next_sf(sfslot);
		
		
            //this sfslot was scanned a long time ago
            if (nodedata->sfslot_table[sfslot].last_scan <= get_time() - param_get_sfslot_scan_period()){
                                                                 
                //and it is closer!
                if ((time_sf_next == 0) || (time_sfslot < time_sf_next)){
                    time_sf_next = time_sfslot;
                    
                    to_scan = TRUE;
                    
                    //neigh_sfslot_table_print(c);
                    //tools_debug_print(c, "[NEIGH] sfslot %d was scanned at %s\n",                                      
                    //                  sfslot,
                    //                  tools_time_to_str(nodedata->sfslot_table[sfslot].last_scan, msg, 150));
                }        
            }
        }

    //debug message
    char msg[150];	
    if (time_sf_next == 0)
        tools_debug_print(c, "[NEIGH] no sfslot to listen to, we may sleep safely\n");
    else if (!to_scan)
        tools_debug_print(c, "[NEIGH] we must listen the beacon of node %d at %s\n", node, tools_time_to_str(time_sf_next, msg, 150));       
    else
        tools_debug_print(c, "[NEIGH] we must scan the sfslot %d (%s) because it was not scanned for a long time\n",                     
                          tools_compute_sfslot(time_sf_next),
                          tools_time_to_str(time_sf_next, msg, 150));
    //neigh_table_print(c);
	
	//the final result (0 means we did not find anything to scan!
	return(time_sf_next);
}



//for a debuging purpose
void neigh_sfslot_table_print(call_t *c){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	int		i;
	char	msg[150];
	
    if (!param_debug())
        return;
    
    
	tools_debug_print(c, "SF Slot	NB PK_rx	Last Scanned\n");
	for(i=0; i<param_get_nb_sfslot(); i++)
		tools_debug_print(c,  "%d		%d		%s\n", \
						  i, \
						  nodedata->sfslot_table[i].nb_pk_rx, \
						  tools_time_to_str(nodedata->sfslot_table[i].last_scan, msg, 150));

	
}





//-------------------------------------------
//			HELLO TRANSMISSION
//-------------------------------------------



//SHOULD we include this neighbor in the hello?
//NB: I do not consider I am my own neighbor (hop=0)
short neigh_hello_include(neigh_info *neigh_elem, int hops){
	return(
		   (neigh_elem->sf_slot != SF_SLOT_INVALID)
		   &&
		   (neigh_elem->bop_slot != BOP_SLOT_INVALID)
           &&
           (neigh_elem->hops <= hops && neigh_elem->hops > 0)
           &&
           (neigh_elem->last_rx <= get_time() - NEIGH_TIMEOUT * tools_get_bi_from_bo(param_get_global_bo()))
		   );
}


//we have to enqueue a new hello packet (some information has changed)
void neigh_hello_enqueue(call_t *c){
  	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	//walk in the neigh table
	int			nb_neighs = 0;
	neigh_info	*neigh_elem;
	
    //invalid short address -> we should not yet send an hello
    if (nodedata->addr_short == ADDR_INVALID_16){
        tools_debug_print(c, "[HELLO] we should not yet send an hello packet, we are not yet associated (no short address)\n");
        return;
    }

    //remove all previous (obsolete hellos)
    buffer_queue_empty(nodedata->buffer_multicast[ADDR_MULTI_HELLO-ADDR_MULTI_MIN]);
    
	//count the nb of neighbors to include
	das_init_traverse(nodedata->neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL) {
		if (neigh_hello_include(neigh_elem, HELLO_MAX_HOPS))
			nb_neighs++;
	}
    //myself
    nb_neighs++;
    
   //create the packet
	packet_t				*pk_hello		= packet_create(c, sizeof(_802_15_4_header) + sizeof(_802_15_4_HELLO_header) + sizeof(hello_info) * nb_neighs, -1);
	_802_15_4_HELLO_header	*header_hello	= (_802_15_4_HELLO_header*) (pk_hello->data + sizeof(_802_15_4_header));
	
    
    tools_debug_print(c, "[HELLO] pk size %d, creation, %d neighbors\n", sizeof(_802_15_4_header) + sizeof(_802_15_4_HELLO_header) + sizeof(hello_info) * nb_neighs, nb_neighs);
    
    //common headers
	pk_set_A(pk_hello, FALSE);					//no ack
	pk_set_SAM(pk_hello, AM_ADDR_SHORT);
	pk_set_DAM(pk_hello, AM_ADDR_SHORT);
	pk_set_ftype(pk_hello, MAC_HELLO_PACKET);
    pk_set_seqnum(pk_hello, mgmt_assign_seqnum(c, ADDR_MULTI_HELLO));
    
	//general hello info
    header_hello->dst       = ADDR_MULTI_HELLO;
	pk_hello_set_nb_neighs(pk_hello, nb_neighs);
    
    //myself
    pk_hello_set_addr(			pk_hello, 0, nodedata->addr_short);
    pk_hello_set_sf_slot(		pk_hello, 0, nodedata->sframe.my_sf_slot);
    pk_hello_set_bop_slot(		pk_hello, 0, nodedata->sframe.my_bop_slot);
    pk_hello_set_has_a_child(	pk_hello, 0, neigh_table_I_have_a_child(c));
    pk_hello_set_hops(          pk_hello, 0, 0);
 
    
	//each neighbor
	int neigh_id = 1;
	das_init_traverse(nodedata->neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL) {
		
		if (neigh_hello_include(neigh_elem, HELLO_MAX_HOPS)){
			pk_hello_set_addr(			pk_hello, neigh_id, neigh_elem->addr_short);
			pk_hello_set_sf_slot(		pk_hello, neigh_id, neigh_elem->sf_slot);
			pk_hello_set_bop_slot(		pk_hello, neigh_id, neigh_elem->bop_slot);
			pk_hello_set_has_a_child(	pk_hello, neigh_id, neigh_elem->has_a_child);
            pk_hello_set_hops(          pk_hello, neigh_id, neigh_elem->hops);
            
			neigh_id++;
		}
	}
	
	//BUG
	if (neigh_id != nb_neighs)
		tools_exit(3, c, "BUG in neigh_hello_enqueue(). I did not include the correct nb of neighbors\n");
    
	tools_debug_print(c, "[HELLO] packet enqueue (dst=%d), %d neighbors\n", ADDR_MULTI_HELLO, nb_neighs);
    buffer_insert_pk(c, nodedata->buffer_multicast[ADDR_MULTI_HELLO-ADDR_MULTI_MIN], buffer_pk_info_create(c, pk_hello), TIMEOUT_INFINITY);
}




//-------------------------------------------
//			HELLO RECEPTION
//-------------------------------------------



//neighborhood table update when I received an hello
void neigh_hello_rx(call_t *c, packet_t *pk_hello){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);

	int			nb_neighs = pk_hello_get_nb_neighs(pk_hello);
	int			i;
	neigh_info	*neigh_elem;
	            
	//for each node in the packet (the source is included has a 0-neighbor in the table)
	for(i=0; i<nb_neighs; i++){
        
		//I don't care about my own info
		if (pk_hello_get_addr(pk_hello, i) == nodedata->addr_short){
            ;	
		}
				
		//the element exist
		else if ((neigh_elem = neigh_table_get(c, pk_hello_get_addr(pk_hello, i))) != NULL){
			
			//I know already this neighbor through a shorter path
			if (neigh_elem->hops < pk_hello_get_hops(pk_hello, i))
				return;
			
			//else, update info			
			neigh_elem->sf_slot         = pk_hello_get_sf_slot(pk_hello, i);
			neigh_elem->bop_slot        = pk_hello_get_bop_slot(pk_hello, i);
			neigh_elem->has_a_child     = pk_hello_get_has_a_child(pk_hello, i);
			neigh_elem->hops            = pk_hello_get_hops(pk_hello, i) + 1;
		
			//tools_debug_print(c, "[NEIGH] node %d updated (sfslot %d, bopslot %d, hasachild %d, hops %d, ischild %d) \n", pk_hello_get_addr(pk_hello, i), neigh_elem->sf_slot, neigh_elem->bop_slot, neigh_elem->has_a_child, neigh_elem->hops, neigh_elem->is_child);
		}			
		
		//or it has to be created
		else{
            neigh_elem = neigh_create_unitialized_neigh(pk_hello_get_addr(pk_hello, i));
			das_insert(nodedata->neigh_table, neigh_elem);    

			neigh_elem->addr_short		=	pk_hello_get_addr(pk_hello, i);
			neigh_elem->sf_slot			=	pk_hello_get_sf_slot(pk_hello, i);
			neigh_elem->bop_slot		=	pk_hello_get_bop_slot(pk_hello, i);
			neigh_elem->depth			=	INVALID_DISTANCE;							//not used!
			neigh_elem->is_child		=	FALSE;
			neigh_elem->has_a_child		=	pk_hello_get_has_a_child(pk_hello, i);
            neigh_elem->hops            =   pk_hello_get_hops(pk_hello, i) + 1;
			neigh_elem->last_rx			=	get_time();
			
			tools_debug_print(c, "[NEIGH] node %d created (sfslot %d, bopslot %d, hasachild %d, hops %d) \n", pk_hello_get_addr(pk_hello, i), neigh_elem->sf_slot, neigh_elem->bop_slot, neigh_elem->has_a_child, neigh_elem->hops);
		}
	}
}


//-------------------------------------------
//			DEBUG
//-------------------------------------------

void neigh_table_print(call_t *c){
	nodedata_info	*nodedata	= (nodedata_info*)get_node_private_data(c);
	
	if (!param_debug())
		return;
		
	//control
	neigh_info	*neigh_elem;
	char	msg[150]; 
	
	tools_debug_print(c, "Addr | SFslot | BOPslot | Depth       | Hops | IsChild | HasChild | LastRx (NeighTable of %d)\n", param_addr_long_to_short(nodedata->addr_long));
	
	
	//walk in the table
	das_init_traverse(nodedata->neigh_table);	
	while ((neigh_elem = (neigh_info*) das_traverse(nodedata->neigh_table)) != NULL) {
		
		
		tools_debug_print(c,  "%3d    %3d      %3d       %4f     %3d       %1d        %1d       %s\n", \
						  neigh_elem->addr_short, \
						  neigh_elem->sf_slot,\
						  neigh_elem->bop_slot,\
						  neigh_elem->depth,\
						  neigh_elem->hops,\
						  neigh_elem->is_child, \
						  neigh_elem->has_a_child, \
						  tools_time_to_str(neigh_elem->last_rx, msg, 150));		//BUG: very slow
				//		  (unsigned long int)neigh_elem->last_rx);
	
 
	}
 

}



/*
 *  radio.c
 *  
 *
 *  Created by Fabrice Theoleyre on 11/02/11.
 *  Copyright 2011 CNRS. All rights reserved.
 *
 */

#include "radio.h"





//----------------------------------------------
//				OFF / ON
//----------------------------------------------

int total_zero = 0;
//mode into string
char *radio_mode_to_str(int mode){
    
    switch(mode){
        case RADIO_RX:
            return("RX");
        case RADIO_TX:
            return("TX");
        case RADIO_OFF:
            return("OFF");
     }
    fprintf(stderr, "[RADIO] Unknown radio mode %d\n", mode);
    return("");
}

//returns the current radio mode
int radio_get_current_mode(call_t *c){
	int mode;
    
    call_t c_radio = {get_entity_bindings_down(c)->elts[0], c->node, c->entity};
    IOCTL(&c_radio, RADIO_IOCTL_MODE_GET, NULL, (void**)&mode);
 
    return(mode);
}


// RADIO module ON/OFF
void radio_switch(call_t *c, int mode) {
    
    //debug
    if (mode != radio_get_current_mode(c))
        tools_debug_print(c, "[RADIO-MODE] -> %s\n", radio_mode_to_str(mode));
    
    //change the radio mode via the ioctl interface
 	call_t c_radio = {get_entity_bindings_down(c)->elts[0], c->node, c->entity};
    IOCTL(&c_radio, RADIO_IOCTL_MODE_SET, (void*)&mode, NULL);
    
}




//----------------------------------------------
//				Triggers a CCA
//----------------------------------------------

//for carrier sensing
int radio_check_channel_busy(call_t *c) {	
	//nodedata_info *nodedata = (nodedata_info*)get_node_private_data(c);
	call_t c_radio = {get_entity_bindings_down(c)->elts[0], c->node, c->entity};

	    
	//energy threshold
    double signal;
	IOCTL(&c_radio, RADIO_IOCTL_GETRXPOWER, NULL, (void**)&signal);
  //  printf("signal level %e (< %e)\n", signal, EDThresholdMin);
    if (signal >= EDThresholdMin){
  		return(TRUE);
	}
    
    
    //carrier sense
    short carrier_sense;
	IOCTL(&c_radio, RADIO_IOCTL_GETCS, NULL, (void**)&carrier_sense);
 //   printf("carrier_sense %d\n", carrier_sense);
    return(carrier_sense);
}






//----------------------------------------------
//				TRANSMISSION
//----------------------------------------------

//print the packet has finished to be txed
int radio_pk_tx_print_finished(call_t *c, void *arg){

 	tools_debug_print(c, "[TX] finished\n");	
	return(0);
}



//fill some generic fields before the transmission 
//NB: this is a 'stub' function that MUST be called instead of TX for EACH transmission
int radio_pk_tx(call_t *c, void *arg){
	packet_t *pk = (packet_t*)arg;
	call_t c_radio = {get_entity_bindings_down(c)->elts[0], c->node, c->entity};
 	
	//debug
	tools_debug_print(c, "[TX] started id %d, seqnum %d, size %d bits, type %d\n", pk->id, pk_get_seqnum(pk), pk->real_size, pk_get_ftype(pk));
	//stats update
	stats_pk_tx(pk);

    //pk_print_content(c, pk, stdout);
    
  //  char msg[50];
   // printf("temps bit : %s\n", tools_time_to_str(radio_get_Tb(&c_radio), msg, 50));
  //  printf("temps tx : %s\n", tools_time_to_str(pk->real_size * radio_get_Tb(&c_radio), msg, 50));
    
	//debug
	if (param_debug())
        scheduler_add_callback(get_time() + pk->real_size * radio_get_Tb(&c_radio), c, radio_pk_tx_print_finished, NULL);
    
//   char msg[100];
//	tools_debug_print(c, "[RADIO] end scheduled at %s (size %d)\n", 
//                      tools_time_to_str(get_time() + pk->real_size * radio_get_Tb(&c_radio), msg, 100), 
//                      pk->real_size);
    
    
    
	//default values (unused fields)
	pk_set_C(pk, 1);		//we do not use the PAN identifiers
	pk_set_S(pk, 0);		//we do not use security headers
	pk_set_FV(pk, 0x01);	//for 2006 standard
	
	//oana: for RPL update information about nr of packets received from RPL 
	nodedata_info *nodedata = get_node_private_data(c);
	_802_15_4_DATA_header	*header_data	= (_802_15_4_DATA_header *) (pk->data + sizeof(_802_15_4_header));
	
	if(pk->size == 127 && !tools_is_addr_multicast(header_data->dst)){
		etx_elt_t *etx_elt; //= (my_retransmit_t *) malloc(sizeof(my_retransmit_t));								
			
		das_init_traverse(nodedata->neigh_retransmit);
		while((etx_elt = (etx_elt_t*) das_traverse(nodedata->neigh_retransmit)) != NULL){
			if (etx_elt->neighbor == header_data->dst) {
				etx_elt->packs_tx++;
				if(nodedata->mac.nb_retry == 0)
					etx_elt->packs_tx_once++;
				//printf("Node %d sent to node %d, %d packets on the radio, %d from RPL. Nodedata->nb_retries %d, total %d\n", c->node, etx_elt->neighbor, etx_elt->packs_tx, etx_elt->packs_net, nodedata->mac.nb_retry, ++total_zero);
			}			
		}
	}
	
	//transmit it to the other layer
	TX(&c_radio, pk);	  

	//that's ok
	return(0);
}






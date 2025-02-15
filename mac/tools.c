/**
 *  \file   tools.c
 *  \brief  tools functions
 *  \author Fabrice Theoleyre
 *  \date   2011
 **/


#include "tools.h"




//----------------------------------------------
//				GRAPH
//----------------------------------------------

//euclidean distance between both nodes
double  tools_distance(nodeid_t a, nodeid_t b){
	position_t	*pos_a = get_node_position(a);	
	position_t	*pos_b = get_node_position(b);	
	
	return(distance(pos_a, pos_b));    
}

//do these nodes interfere with each other?
//NB we assume the interference range is the double of the radio range (Ri = 2 * Rt)
short tools_graph_nodes_interf(nodeid_t a, nodeid_t b){
   return(tools_distance(a, b) <= INTERF_RANGE);
}

//does this pair of links interfere? 
short tools_graph_links_interf(nodeid_t s1, nodeid_t d1, nodeid_t s2, nodeid_t d2){

	if (tools_graph_nodes_interf(s1, s2))
		return(TRUE);
	if (tools_graph_nodes_interf(s1, d2))
		return(TRUE);
	if (tools_graph_nodes_interf(d1, s2))
		return(TRUE);
	if (tools_graph_nodes_interf(d1, d2))
		return(TRUE);

	return(FALSE);
}

//----------------------------------------------
//				ADDRESSES
//----------------------------------------------


//for this compressed multicast address, are seqnums replaced or inserted in the buffer? 
//NB: for instance, the hello packets are replaced by the new ones
short tools_is_addr_multicast_seqnum_replacement(uint16_t caddr){
    
    if (caddr == ADDR_MULTI_HELLO - ADDR_MULTI_MIN)
        return(TRUE);
    
    //default behavior: addition in the buffer
    return(FALSE);
}

//is this a multicast address?
//NB: ADDR_MULTI_MIN=ffffff thus, the second test is useless
short tools_is_addr_multicast(uint16_t addr){
   return(addr>=ADDR_MULTI_MIN);  
    //return(addr>=ADDR_MULTI_MIN && addr<=ADDR_MULTI_MAX);    
}
//is this an anycast address?
short tools_is_addr_anycast(uint16_t addr){
    return(addr>=ADDR_ANY_MIN && addr<=ADDR_ANY_MAX);    
}

//converts this MAM value in a number of bits
int tools_MAM_to_bits(uint8_t MAM){
   
    return(4 + MAM * 8);    
}

//converts a number of seqnum into a MAM
uint8_t tools_nb_to_MAM(int nb){
    
    //bug
    if (nb > 8*7 + 4){
        fprintf(stderr, "[BUG] we have too many sequence numbers to include in the beacon (%d > %d)\n", nb, 8*7 + 4);
        exit(7);
    }
    
    return(ceil(nb - 4)/8);    
}


//multicast address type into an human readable string
char *tools_multicast_addr_to_str(uint16_t addr){
    
    switch(addr){
        case ADDR_MULTI_HELLO:
            return("HELLO");
        case ADDR_MULTI_BEACONS:
            return("BEACONS");
        case ADDR_MULTI_DISC:
            return("Discovery");
        case ADDR_MULTI_CTREE:
            return("Ctree");
        case ADDR_MULTI_FLOOD_DISC:
            return("Flooding with discovery");
        case ADDR_MULTI_FLOOD_CTREE:
            return("Flooding with Ctree");
    }   
    return("NO");
}
//----------------------------------------------
//				MATHS
//----------------------------------------------

//returns the minimum
double tools_min_dbl(double a, double b){
	 if (a < b)
		 return(a);
	 else
		 return(b);
}
 
 //returns the minimum
int tools_min(int a, int b){
	if (a < b)
		return(a);
	else
		return(b);
}



//-----------------------------------------------------------------------------
//						TOOLS FUNCTIONS
//-----------------------------------------------------------------------------


//returns the maximum x-coordinate
double tools_get_x_max(){
	int		i;
	//Positions
	double		x_max =0.0;
	position_t	*pos;	
	
	for(i=0; (i<get_node_count()) && (god_knowledge[i] != NULL) ; i++){
		pos = get_node_position(god_knowledge[i]->my_id);
		if (pos->x > x_max)
			x_max = pos->x;
	}
	return(x_max);
}

//returns the maximum y-coordinate
double tools_get_y_max(){
	int		i;
	//Positions
	double		y_max =0.0 ;
	position_t	*pos;	
	
	for(i=0; (i<get_node_count()) && (god_knowledge[i] != NULL) ; i++){
		pos = get_node_position(god_knowledge[i]->my_id);
		if (pos->y > y_max)
			y_max = pos->y;		
	}
	return(y_max);
}

//returns the color (the WHITE is forbidden, I must shift all the values larger than WHITE)
int tools_set_color(int color){
	color = color % 30;
    
    if (color < WHITE)
		return(color);
	else
		return(color+1);
}


//-----------------------------------------------------------------------------
//						.FIG	FILE
//-----------------------------------------------------------------------------

// DESCRIPTION for each depth
// 1 node ids + slots
// 2 vertices (color = superframe slot)
// 3 cluster-tree links
// 4 radio links (neighborhood table)
// 5 graph links (UDG constructed via the radio ranges of each node and its position)

//generates the .fig file
void tools_generate_figures(call_t *c){
	short		color , color2 , depth , thickness , linestyle;
	int			radius;
	//Figure file
	FILE*		pfile;
	char		filename[200];
	//tmp
	int			x1 , y1 , x2 , y2;
	//Control
	int			i;
	//parameters
	int			GRAPHIC_XMAX , GRAPHIC_YMAX;
	double		MAX_X = tools_get_x_max() , MAX_Y = tools_get_y_max() ;
	int			nb_nodes = get_node_count();
	double		degree = 0;
	//position (tmp var)
	position_t	*pos;
	
	//Opens the associated file and 
	//Initialization
	snprintf(filename , 200, "topology.fig");
	pfile = fopen(filename , "w");
	if (pfile==NULL){
		tools_exit(2, c, "we cannot create the file %s\n", filename);
	}	
	
	//line networks
	if (MAX_X == 0)
		MAX_X = 1;
	if (MAX_Y == 0)
		MAX_Y = 1;
	
	//Horizontal & vertical Scaling
	if (MAX_X > MAX_Y){
		GRAPHIC_XMAX = GRAPHIC_MAX;
		GRAPHIC_YMAX = MAX_Y * GRAPHIC_MAX / MAX_X;
	}
	else{
		GRAPHIC_YMAX = GRAPHIC_MAX;
		GRAPHIC_XMAX = MAX_X * GRAPHIC_MAX / MAX_Y;
	}
	

	
	//--------
	//Headers
	//--------
	fprintf(pfile,"#FIG 3.2 \n#Snapshot of The Network Topology \nLandscape \nCenter \nMetric \nA0 \n100.00 \nSingle \n-2 \n1200 2 \n");						
	//fprintf(pfile,"1 3 0 1 0 0 50 -1 15 0.000 1 0.000 0 0 1 1 0 0 0 0\n");
	//gray color
	fprintf(pfile , "#Gray color\n0 %d #696969\n", GRAY);
	
		
	//-----------------------------
	//Nodes Position and addresses
	//-----------------------------
	fprintf(pfile,"#NODE POSITIONS\n");
	for(i=0 ; i < nb_nodes ; i++){	
		
		//Disks which represent the node position
		color	= tools_set_color(god_knowledge[i]->sframe.my_sf_slot);
		color2	= tools_set_color(god_knowledge[i]->sframe.my_sf_slot);
		radius	= GRAPHIC_RADIUS;
		
		//location
		pos = get_node_position(i);
		
		//the disk representing the node
		fprintf(pfile ,"1 3 0 1 %d %d %d -1 15 0.000 1 0.000 %d %d %d %d 0 0 0 0\n", color2 , color , 1 , (int)(pos->x * GRAPHIC_XMAX / MAX_X) , (int)(pos->y * GRAPHIC_YMAX / MAX_Y) , radius , radius);
		
		//Address associated to this node (this must be printed not far from the node itself)
		fprintf(pfile ,"4 0 0 %d -1 0 %d 0.0000 4 135 135 %d %d %d\\001\n", 0 , GRAPHIC_POLICE_SIZE , GRAPHIC_SHIFT_X + (int)(pos->x * GRAPHIC_XMAX / MAX_X) , GRAPHIC_SHIFT_Y + (int)(pos->y * GRAPHIC_YMAX / MAX_Y) , i);
		int shift_x;
		if (i < 10)
			shift_x = GRAPHIC_SHIFT_X + (int)(pos->x * GRAPHIC_XMAX / MAX_X) + 9 * GRAPHIC_POLICE_SIZE;
		else
			shift_x = GRAPHIC_SHIFT_X + (int)(pos->x * GRAPHIC_XMAX / MAX_X) + 18 * GRAPHIC_POLICE_SIZE;
		fprintf(pfile ,"4 0 0 %d -1 0 %d 0.0000 4 135 135 %d %d (%d %d)\\001\n", 0 , (int)(GRAPHIC_POLICE_SIZE * 0.66) , shift_x , GRAPHIC_SHIFT_Y + (int)(pos->y * GRAPHIC_YMAX / MAX_Y) , god_knowledge[i]->sframe.my_sf_slot, god_knowledge[i]->sframe.my_bop_slot);
	}
	
	
	//------
	//Links
	//------
	fprintf(pfile,"#LINKS\n");
	neigh_info		*neighbor;
	for(i=0 ; i < nb_nodes ; i++){
		das_init_traverse(god_knowledge[i]->neigh_table);      
		while ((neighbor = (neigh_info *) das_traverse(god_knowledge[i]->neigh_table)) != NULL) 
			if (neighbor->hops == 1){
				degree++;
				
				//Coordinates of the source and destination of link
				pos = get_node_position(i);
				x1 = (int)(pos->x / MAX_X * GRAPHIC_XMAX);
				y1 = (int)(pos->y / MAX_Y * GRAPHIC_YMAX);
				
				pos = get_node_position(neighbor->addr_short);
				x2 = (int)(pos->x / MAX_X * GRAPHIC_XMAX);
				y2 = (int)(pos->y / MAX_Y * GRAPHIC_YMAX);
				
				//Distinction between the different type of links
				// Parent link
				
				//arrows if cluster-tree link, else dashed link for radio links
				if (ctree_parent_associated_get_info(god_knowledge[i]->parents, neighbor->addr_short) != NULL){
					depth		= 2;
					color		= BLACK;
					thickness	= 2;
					linestyle	= SOLID;
				}
				//radio link only
				else{
					depth		= 4;
					color		= GRAY;
					linestyle	= DOTTED;
					thickness	= 1;
				}
				
				//Arrow (tree link)
				if (depth == 2)
					fprintf(pfile, "#%d-%d (type %d)\n3 2 %d %d %d 7 %d -1 -1 1.0 0 1 0 2\n			2 1 3.0 160.0 190.0\n			%d %d %d %d\n			0.000 0.000\n", i , neighbor->addr_short , depth , linestyle , thickness,  color , depth , x1 , y1 , x2 , y2);
				//other link
				else
					fprintf(pfile, "#%d-%d (type %d)\n3 2 %d %d %d 0 %d -1 -1 25.0 0 0 0 2\n			%d %d %d %d\n			0.000 0.000\n", i , neighbor->addr_short , depth , linestyle , thickness,  color , depth , x1 , y1 , x2 , y2);
			}
	}

	
	//--------
	//Footers
	//--------
	fprintf(pfile,"#END OF FIGURE\n");
	fclose(pfile);
}





//----------------------------------------------
//				DEBUG
//----------------------------------------------


//a fatal error occured
void tools_exit(int code, call_t *c, const char* fmt, ...){	
	
	//	nodedata_info				*nodedata		= (nodedata_info*)get_node_private_data(c);
	va_list 	argptr;
	char		msg[50];
	
    //BUG
    tools_generate_figures(c);
	
	//Prepares the args
	va_start(argptr, fmt);	
	
	
	fprintf(stdout , "[%s, %3.1i] [FATAL ERROR] "	, tools_time_to_str(get_time(), msg, 50), c->node);	
	vfprintf(stdout , fmt, argptr);     
	//fprintf(stdout, "Seed: %lu\n", get_rng_seed());
	
	//flush the printed messages (for all opened file pointers)
	fflush(NULL);	
	va_end(argptr);
	
	exit(code);	
}	

//a fatal error occured (error messages not printed through tools_debug_print since we did not have a pointer to c)
void tools_exit_short(int code,  const char* fmt, ...){	
		
	va_list 	argptr;
	va_start(argptr, fmt);	
	fprintf(stderr, "[FATAL ERROR] ");
	vfprintf(stderr , fmt, argptr);     
	fflush(NULL);	
	va_end(argptr);
	
	exit(code);	
}


// Print all debug messages classified in different files
void tools_debug_print(call_t *c, const char* fmt, ...){	
	if (!param_debug())
		return;
	
//	nodedata_info				*nodedata		= (nodedata_info*)get_node_private_data(c);
	va_list 	argptr;
	char		msg[50];
	
	
	//Prepares the args
	va_start(argptr, fmt);	
	
	fprintf(stdout , "[%s, %3.1i] "	, tools_time_to_str(get_time(), msg, 50), c->node);	
	
    
//    nodedata_info *nodedata		= (nodedata_info*)get_node_private_data(c);
  //  fprintf(stdout, "-%d-", nodedata->pk_tx_up_pending);
    
    vfprintf(stdout , fmt, argptr);     
	
	
	//flush the printed messages (for all opened file pointers)
	fflush(NULL);
	
	va_end(argptr);
}	


//converts a uint64_t time into a human readeable string
char *tools_time_to_str(uint64_t time_u, char *msg, int length){
    float   time = time_u *1.0;
    
    
    if (!param_debug())
        return("");

    double  minutes_f;
    double  seconds = modf(time / 60e9, &minutes_f);    //separate minutes and seconds
    seconds *= 60;
    int     days    = minutes_f / 1440;  
    int     hours   = (minutes_f - days * 1440) / 60;
    int     minutes = minutes_f - days * 1440 - hours * 60;
   
    
  /*    
    printf("time %lu\n", time_u);
    printf("time float %f\n", time);
    printf("time float %f minutes float %f seconds %f\n", time, minutes_f, seconds);
    */
    
    if (days ==0 && hours ==0)
        snprintf(msg, length , "%2dmn, %2ds, %3dms, %3dus", 
			 (int) minutes, 
			 (int) floor(seconds), 
			 (int) (floor(seconds * 1E3) - 1E3 * floor(seconds)), 
			 (int) (floor(seconds * 1E6) - 1E3 * floor(seconds * 1E3)));	
	else if (days == 0)
        snprintf(msg, length , "%2dh %2dmn, %2ds, %3dms, %3dus", 
                 (int) hours, 
                 (int) minutes, 
                 (int) floor(seconds), 
                 (int) (floor(seconds * 1E3) - 1E3 * floor(seconds)), 
                 (int) (floor(seconds * 1E6) - 1E3 * floor(seconds * 1E3)));	
    else
        snprintf(msg, length , "%2dd %2dh %2dmn, %2ds, %3dms, %3dus", 
                 (int) days, 
                 (int) hours, 
                 (int) minutes, 
                 (int) floor(seconds), 
                 (int) (floor(seconds * 1E3) - 1E3 * floor(seconds)), 
                 (int) (floor(seconds * 1E6) - 1E3 * floor(seconds * 1E3)));	
	return(msg);

}


//conversion of the superframe scheduling algo into a string
char *tools_algo_sf_to_str(int algo){

	switch(algo){
		case ALGO_SF_ORG_802154:
			return("802514");
		case ALGO_SF_ORG_RAND:
			return("RANDOM");
		case ALGO_SF_ORG_GREEDY:
			return("GREEDY");
		case ALGO_SF_ORG_GOD:
			return("GOD");
		default:
			fprintf(stderr, "Unkwnown superframe scheduling algo %d\n", algo);
			exit(3);
	}
}

//conversion of the BOP algo into a string
char *tools_bop_algo_to_str(int algo){
    
	switch(algo){
		case ALGO_BOP_ORIG:
			return("ORIG");
		case ALGO_BOP_BACKOFF:
			return("BACKOFF");
		default:
			fprintf(stderr, "Unkwnown BOP algo %d\n", algo);
			exit(3);
	}
}
//conversion of the multicast algo into a string
char *tools_multicast_algo_to_str(int algo){
    
	switch(algo){
		case ALGO_MULTICAST_DUPLICATE:
			return("DUPLICATE");
		case ALGO_MULTICAST_DUPLICATE_ACK:
			return("DUPLICATE_ACK");
		case ALGO_MULTICAST_SEQ:
			return("SEQUENCE");
		default:
			fprintf(stderr, "Unkwnown multicast algo %d\n", algo);
			exit(3);
	}
}

//int alias into a string
char* tools_depth_metric_to_str(int metric){
 	
    switch(metric){
		case DEPTH_HOPS:
			return("DEPTH_HOPS");
		case DEPTH_ETX:
			return("DEPTH_ETX");
		case DEPTH_BER:
			return("DEPTH_BER");
		default:
			fprintf(stderr, "Unkwnown depth metric %d\n", metric);
			exit(3);
	}
   
}

//state machine into string
char *tools_fsm_state_to_str(int state){
	
	switch(state){
		case STATE_SLEEP:
			return("STATE_SLEEP");
		
        case STATE_COORD_IDLE:
			return("STATE_COORD_IDLE");
		case STATE_COORD_DATA_TX:
			return("STATE_COORD_DATA_TX");
		case STATE_COORD_WAIT_ACK:
			return("STATE_COORD_WAIT_ACK");
            
       case STATE_CHILD_WAIT_BEACON:
			return("STATE_CHILD_WAIT_BEACON");
		case STATE_CHILD_STOP_WAIT_BEACON:
			return("STATE_CHILD_STOP_WAIT_BEACON");
		case STATE_CHILD_IDLE:
			return("STATE_CHILD_IDLE");
		case STATE_CHILD_BACKOFF:
			return("STATE_CHILD_BACKOFF");
		case STATE_CHILD_CHECK_CHANNEL:
			return("STATE_CHILD_CHECK_CHANNEL");
		case STATE_CHILD_CHECK_CHANNEL_CCA_END:
			return("STATE_CHILD_CHECK_CHANNEL_CCA_END");
		case STATE_CHILD_TX:
			return("STATE_CHILD_TX");
		case STATE_CHILD_WAIT_ACK:
			return("STATE_CHILD_WAIT_ACK");
		case STATE_CHILD_WAIT_REP:
			return("STATE_CHILD_WAIT_REP");
		
        case STATE_BOP_WAIT_FREE:
			return("STATE_BOP_WAIT_FREE");
		case STATE_BOP_BACKOFF:
			return("STATE_BOP_BACKOFF");

		case STATE_UNASSOC_WAIT_BEACON:
			return("STATE_UNASSOC_WAIT_BEACON");
		case STATE_UNASSOC_WAIT_ACK_ASSOC_REQ:
			return("STATE_UNASSOC_WAIT_ACK_ASSOC_REQ");
		case STATE_UNASSOC_WAIT_ASSOC_REP:
			return("STATE_UNASSOC_WAIT_ASSOC_REP");
		default:
			fprintf(stderr, "Unknown state %d\n", state);
			exit(4);
	}	
}


//the reason fordropping the packet
char *tools_drop_reason_to_str(int reason){
	switch(reason){
		case DROP_NO:
			return("OK");
			
		case DROP_CCA:
			return("CCA");
			
		case DROP_RETX:
			return("RETX");
			
		case DROP_NOT_ASSOCIATED:
			return("ASSOC");
			
		case DROP_BUFFER_TIMEOUT:
			return("TIMEOUT");
			
		default:
			return("?");					
			
	}
}


//----------------------------------------------
//				CONVERSION
//----------------------------------------------


//converts the BO SO into BI SD (i.e. constants into durations)
uint64_t tools_get_bi_from_bo(int BO){
	return(aNumSuperFrameSlot*aBaseSLotDuration * pow(2, BO));
}
uint64_t tools_get_sd_from_so(int SO){
	return(aNumSuperFrameSlot*aBaseSLotDuration * pow(2, SO));
}

//returns the corresponding sf slot
int tools_compute_sfslot(uint64_t time){
	
	uint64_t	BI = tools_get_bi_from_bo(param_get_global_bo());
	uint64_t	SD = tools_get_sd_from_so(param_get_global_so());
	
	//modulo
	while (time >= BI)
		time -= BI;	
		
	//associated sf slot number
	int sfslot = floor(time / SD);
	
	//bounds verification
	if ((sfslot < 0) || (sfslot >= param_get_nb_sfslot())){
		fprintf(stderr, "ERROR this slot nb was changed until we initialize the sfslot_table\n");
		fprintf(stderr, "%d is too large (> %d)\n", sfslot, param_get_nb_sfslot());
		exit(2);
	}
	return(sfslot);		
}




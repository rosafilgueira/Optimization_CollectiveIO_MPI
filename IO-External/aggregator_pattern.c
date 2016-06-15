#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "mpi.h"
#include "mpio.h"
#include "lap.h"


struct ADIO_cb_name_arrayD {   
  int refct;              
  int namect;
  char **names;
};  
typedef struct ADIO_cb_name_arrayD *ADIO_cb_name_array;

int calc_interval( int cnode_off, int min_off,int  *len, int fd_size,int *fd_start, int *fd_end){
  int rank_index; 
  int avail_bytes;

  rank_index = (int) ((cnode_off - min_off + fd_size)/ fd_size - 1);
  avail_bytes = fd_end[rank_index] + 1 - cnode_off;
  if (avail_bytes < *len) {
    *len = avail_bytes;
  }
  return rank_index;
}

void config_list_name(int mynod, int cb_nodes, int len, ADIO_cb_name_array array, char *dest, int * ranklist){
  char *ptr;
  int i, p,l;
  
  if (mynod==0) {
    ptr = dest;
    for (i=0; i<cb_nodes; i++ ) {
      p = snprintf(ptr, len, "%s,", array->names[ranklist[i]]);
      ptr += p;
      }
      /* chop off that last comma */
    dest[strlen(dest) - 1] = '\0';
  }
  MPI_Bcast(dest, len, MPI_CHAR, 0, MPI_COMM_WORLD);
}


int cb_gather_name_array(int commsize, int commrank, ADIO_cb_name_array *arrayp){
    char my_procname[MPI_MAX_PROCESSOR_NAME], **procname = 0;
    int *procname_len = NULL, my_procname_len, *disp = NULL, i;
    ADIO_cb_name_array array = NULL;

    MPI_Get_processor_name(my_procname, &my_procname_len);
    /* allocate space for everything */
    array = (ADIO_cb_name_array) malloc(sizeof(*array));
    if (array == NULL) {
	return -1;
    }
    array->refct = 1; 

    if (commrank == 0) {
	/* process 0 keeps the real list */
	array->namect = commsize;

	array->names = (char **) malloc(sizeof(char *) * commsize);
	if (array->names == NULL) {
	    return -1;
	}
	procname = array->names; /* simpler to read */

	procname_len = (int *) malloc(commsize * sizeof(int));
	if (procname_len == NULL) { 
	    return -1;
	}
    }
    else {
	/* everyone else just keeps an empty list as a placeholder */
	array->namect = 0;
	array->names = NULL;
    }
    /* gather lengths first */
    MPI_Gather(&my_procname_len, 1, MPI_INT, procname_len, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (commrank == 0) {
#ifdef CB_CONFIG_LIST_DEBUG
	for (i=0; i < commsize; i++) {
	    FPRINTF(stderr, "len[%d] = %d\n", i, procname_len[i]);
	}
#endif

	for (i=0; i < commsize; i++) {
	    /* add one to the lengths because we need to count the
	     * terminator, and we are going to use this list of lengths
	     * again in the gatherv.  
	     */
	    procname_len[i]++;
	    procname[i] = malloc(procname_len[i]);
	    if (procname[i] == NULL) {
		return -1;
	    }
	}
	
	/* create our list of displacements for the gatherv.  we're going
	 * to do everything relative to the start of the region allocated
	 * for procname[0]
	 *
	 * I suppose it is theoretically possible that the distance between 
	 * malloc'd regions could be more than will fit in an int.  We don't
	 * cover that case.
	 */
	disp = malloc(commsize * sizeof(int));
	disp[0] = 0;
	for (i=1; i < commsize; i++) {
	    disp[i] = (int) (procname[i] - procname[0]);
	}

    }

    /* now gather strings */
    if (commrank == 0) {
	MPI_Gatherv(my_procname, my_procname_len + 1, MPI_CHAR, 
		    procname[0], procname_len, disp, MPI_CHAR,
		    0, MPI_COMM_WORLD);
    }
    else {
	/* if we didn't do this, we wnould need to allocate procname[]
	 * on all processes...which seems a little silly.
	 */
	MPI_Gatherv(my_procname, my_procname_len + 1, MPI_CHAR, 
		    NULL, NULL, NULL, MPI_CHAR, 0, MPI_COMM_WORLD);
    }

    if (commrank == 0) {
	/* no longer need the displacements or lengths */
	free(disp);
	free(procname_len);

#ifdef CB_CONFIG_LIST_DEBUG
	for (i=0; i < commsize; i++) {
	    fprintf(stderr, "name[%d] = %s\n", i, procname[i]);
	}
#endif
    }

    *arrayp = array;
    return 0;
}


/*****************************************************************************************/
/* If criteria == 1, this means, ACN:  This criteria assigns each aggregator to the node who has more highest number of contiguous data blocks of the file domain associated with the aggregator. *//* If criteria == 2, this means, AVN: This criteria assigns each aggregator to the node who has more data of the file domain associated with the aggregator.*/


void aggregation_interval(int myrank, int partition_number, int num_nodes,int Ds[], int Bl[],int contblocks, char *cb_config_string, int criteria)
 { 
  int num_interval;
  int *interval_assigment;
  int **interval;
  int **interval_processes;
  int id_interval;
  int *processor_assigment;
  int **change;
  int *u;
  int *v;
  int size_interval,i,j,k;
  int * interval_begin;
  int * interval_end;
  int my_procname_len;
  char my_procname[1000];
  int *procname_len; 
  char **procname;
  int ip_direction[1000];
  int ** array_nodes;
  int ncpus[1000];
  int cnodes;
  int *cont_proc;
  int off, fd_len, *list_data_intervals,rem_len;
  int *ranklist;
  int cb_config_len;
  MPI_Status stat;
  int flag;
  ADIO_cb_name_array array;
    
   cb_gather_name_array(partition_number,myrank , &array);
    if (myrank==0){
      for (i=0;i<partition_number;i++){
	ip_direction[i]=-1;
	ncpus[i]=-1;
      }
      flag=0;
      for(i=0;i<partition_number;i++)
	if (ip_direction[i]==-1){
	  ncpus[flag]++;
	  ip_direction[i]=flag;
	  for (j=i+1;j<partition_number;j++)
	    if ((strcmp (array->names[i],array->names[j])==0) && (ip_direction[j]==-1)){
	      ip_direction[j]=flag;
	      ncpus[flag]++;
	    }
	    flag++;
	  } 
        cnodes=flag;
     // printf("Number of different nodes detected is  %d\n",cnodes);
     cont_proc=(int*)malloc(cnodes*sizeof(int));
     j=0;
    for(i=0;i<cnodes;i++){
      cont_proc[j]=ncpus[i]+1;
      j++;
    }
    array_nodes=(int**)malloc(cnodes*sizeof(int*));
    for(i=0;i<cnodes;i++)
      array_nodes[i]= (int*)malloc(cont_proc[i]*sizeof(int)); 
    for(i=0;i<cnodes;i++){
      k=0;
      for(j=0;j<cont_proc[i];j++){
	while ((ip_direction[k]!=i) && (k<partition_number))
	  k++;
	if (k!=partition_number){
	  array_nodes[i][j]=k;
          k++;	  
	} 
      }
     }        
    }
     
    MPI_Bcast ( &cnodes,1, MPI_INT, 0,MPI_COMM_WORLD);
    cb_config_len=0;

    num_interval=cnodes;
    interval_assigment = (int *) malloc(cnodes*sizeof(int));  
    ranklist=(int*)malloc(cnodes*sizeof(int));  
      interval_begin= (int *)malloc(cnodes*sizeof(int));
      interval_end = (int *)malloc(cnodes*sizeof(int));
      interval_processes=(int **)malloc(partition_number*sizeof(int*));
      interval=(int **)malloc(cnodes*sizeof(int*));
      list_data_intervals=(int *)malloc(cnodes*sizeof(int*));
      for (i=0;i<partition_number;i++)
	interval_processes[i]=(int *)malloc(cnodes*sizeof(int));
      for(i=0;i<cnodes;i++){
	interval_begin[0]=0; 
	interval_end[0]=0;
	list_data_intervals[i]=0;  
	interval[i]=(int *)malloc(cnodes*sizeof(int));
      }
      for(i=0;i<partition_number;i++)
	for(j=0;j<cnodes;j++)
	  interval_processes[i][j]=0;
	
	for(i=0;i<cnodes;i++)
	  for(j=0;j<cnodes;j++)
	    interval[i][j]=0;
	  
        size_interval=num_nodes/cnodes;
	interval_begin[0]=0;
        interval_end[0]=size_interval-1;
      if(num_nodes%cnodes==0){
	for(i=1;i<num_interval;i++)
	{
	  interval_begin[i]=interval_end[i-1]+1;
	  interval_end[i]=(interval_begin[i]+size_interval)-1;
	}
      }
      else{
	for(i=1;i<num_interval-1;i++){  
	  interval_begin[i]=interval_end[i-1]+1;
	  interval_end[i]=(interval_begin[i]+size_interval)-1;
	}
	interval_begin[num_interval-1]=interval_end[num_interval-2]+1;
	interval_end[num_interval-1]=num_nodes-1;
      }
      
      for (i=0; i < contblocks; i++) {
	off = Ds[i];
	fd_len = Bl[i];
	id_interval = calc_interval(off, 0, &fd_len, size_interval, interval_begin, interval_end);
	if (criteria==1)
          list_data_intervals[id_interval]++;
        else
	  list_data_intervals[id_interval]+=fd_len;
	rem_len = Bl[i] - fd_len;
	while (rem_len != 0) {
	  off += fd_len; /* point to first remaining byte */
	  fd_len = rem_len; /* save remaining size, pass to calc */
	  id_interval = calc_interval( off,0, &fd_len, size_interval, interval_begin, interval_end);
	  if (criteria==1)
            list_data_intervals[id_interval]++;
          else
	    list_data_intervals[id_interval]+=fd_len;
	  rem_len -= fd_len; /* reduce remaining length by amount from fd */
	}
      }
      
      if (myrank!=0){
	  MPI_Send( list_data_intervals,num_interval, MPI_INT,0, 1, MPI_COMM_WORLD );
      }
      else
	for(i=1;i<partition_number;i++)
	  MPI_Recv( interval_processes[i],num_interval, MPI_INT,i,1,MPI_COMM_WORLD ,&stat);
	if (myrank==0){
            for (i=0;i<num_interval;i++) 
	    interval_processes[0][i]=list_data_intervals[i]; 	  
	    
            for(i=0;i<cnodes;i++)
	      for (j=0;j<cont_proc[i];j++)
		for (k=0;k<num_interval;k++)
		  interval[i][k]+=interval_processes[array_nodes[i][j]][k];
                  
	    //for(i=0;i<cnodes;i++)
		//for (k=0;k<num_interval;k++)
                  // printf("Node[%d]- for interval[%d] has :%d data\n",i,k,interval[i][k]); 
		  processor_assigment=(int *)malloc(num_interval*sizeof(int));//jonker
		  for(i=0;i<num_interval;i++){
		    interval_assigment[i]=0;
		    processor_assigment[i]=0;
		  }

		  change=(int **)malloc(num_interval*sizeof(int*));
		  for(i=0;i<num_interval;i++){
		    change[i]=(int *)malloc(num_interval*sizeof(int));
		  } 
		  for (i=0;i<num_interval;i++) 
		    for(j=0;j<num_interval;j++)
		      change[i][j]=interval[i][j]*(-1);/// Creo una copia de la matriz de costes
		    
		    u=(int *)malloc(num_interval*sizeof(int));//jonker
		    v=(int *)malloc(num_interval*sizeof(int));//jonker
		    lap(num_interval,change,processor_assigment,interval_assigment,u,v);//lap.c
		    /*for (i=0;i<cnodes;i++)
		      printf("Interval %d is for %d\n",i,interval_assigment[i]);
                      */      
		    for (i=0;i<cnodes;i++)
		      ranklist[i]= array_nodes[interval_assigment[i]][0];
		    for (i=0;i<cnodes;i++)
		      cb_config_len+=strlen(array->names[ranklist[i]])+3; 
		    
		    ++cb_config_len;
		    ++cb_config_len;
		    ++cb_config_len;
	}  
	
	MPI_Bcast(ranklist, cnodes, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&cb_config_len, 1, MPI_INT, 0, MPI_COMM_WORLD);
        config_list_name(myrank, cnodes, cb_config_len, array, cb_config_string,ranklist);
}

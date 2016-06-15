#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "mpi.h"
#include "mpio.h"
#include "lap.h"

#define NUMMAXCHARACTERS 255
#define MPI_TYPE MPI_FLOAT
#define TYPE int

struct nodes {
  float *Load;
} ;

struct list{
  int num;
  struct nodes * nodelist;
};

struct times{
  double *t1;
  double *t2;
  double *t3;
  double *t4; 
  int *data;
  int size;
};
struct file_i{
  int * distribution;	
  int  num_nodes;
  struct times t;	
  int * sender;
  int * receiver;
  int * processor;
};
struct timeval tv ;

void handle_error(int errcode, char *str) {
  char msg[MPI_MAX_ERROR_STRING];
  int resultlen;
  MPI_Error_string(errcode, msg, &resultlen);
  fprintf(stderr, "%s: %s\n", str, msg);
  MPI_Abort(MPI_COMM_WORLD, 1);
}

int main (int argc, char *argv[]){
  
  MPI_Info info1,info2;
  struct nodes *node;
  struct list *listnodes;
  struct list nodepart;
  int *list_num_nodes;
  int *List_randomly;
  int *List_randomly2;
  int p,m;
  float *romiodata;
  float *romioread;
  struct file_i *file_info;
  struct file_i *result;
  int * datap;
  int num_nodes,aux;
  float numradomly; 
  char line[1255];
  char lineux[1255];
  int ret;
  char name_file_metis[NUMMAXCHARACTERS];
  char name_file_masc[NUMMAXCHARACTERS];
  int partition_number=0;
  FILE *pf1;
  FILE *pf2;
  FILE *pf3;
  FILE *pf5;
  int t,iter;
  int r;
  int i;
  int j;
  int k;
  int L;
  double time_t8;
  double time_t9;
  int data_sender=0;
  int data_receiver=0;
  int cont;
  char cadena [255];
  char fichero[255] ;
  int square;
  int myrank;	
  int dest;	
  int size;
  int tag=1;
  char * buf;
  MPI_Status stat;
  MPI_File fh;
  double t1;//Begining time
  double t2;
  double t3;
  double t5;
  double ta;//t2-t1 
  double tb;//t3-t1 (Operation Time)
  double tc;//t3-t4 (Collective IO write)
  double tt;//t4-t1 (Total Time)
  char cb_config_string[1000];
  char key[MPI_MAX_INFO_KEY], value[MPI_MAX_INFO_VAL];
  int nkeys;
  char info_data2[MPI_MAX_INFO_VAL];
  int contblocks;
  int percent;
  int * Bl;
  int * Ds;
  int flag=0;
  MPI_Datatype ADG;
  MPI_Datatype ADG2;
  MPI_Datatype loads;
  setbuf(stdout,NULL);
  char version [255],strip_size[255];
  int criteria;
  ret = MPI_Init(&argc,&argv);
  if (ret != MPI_SUCCESS){
    printf("PROBLEMAS !!!!!!!!!!!!!!!!!! -> exit() \n");
    exit(-1) ;
  }
  t1=MPI_Wtime();
  MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  MPI_Barrier(MPI_COMM_WORLD);
    L=atoi(argv[1]);//Load
    square=atoi(argv[2]);//number of operations
    criteria=atoi(argv[3]);//aggregation-pattern criteria
    percent=atoi(argv[4]);//percent of number equal 0
    partition_number=size;
 
  for(iter=0;iter<5;iter++){

    if (myrank==0){	
      list_num_nodes=(int*) malloc (partition_number*sizeof(int)); 
      pf1 = fopen("Mesh/mesh-metis_Si_4.0.dat","r");
      if (pf1 == NULL){ 
	printf("Cant open mesh file\n");
        exit(-1);
      }	

      strcpy(cadena,"Mesh/asignacion");
      sprintf(name_file_metis,"%s%d",cadena,partition_number);
      pf2 = fopen(name_file_metis,"r");
      if (pf2 == NULL){ 
	printf("Cant open metis file\n");
        exit(-1);
      }	
      for (j=0;j<partition_number;j++)
	list_num_nodes[j]=0;
     
      while(!feof(pf2)){
	fscanf(pf2,"%[^\n]\n",line);
	sscanf(line,"%d",&cont);
	list_num_nodes[cont]++;
      }
            
      rewind(pf2);
   /**
   number of nodes of the mesh
   **/
      fscanf(pf1,"%[^\n]\n",line);
      sscanf(line,"%d",&num_nodes);
      datap=(int *) malloc(num_nodes*(sizeof(int)));
      if (datap==NULL)
	printf("Problems in the memory malloc\n");
      i=0;
      while(!feof(pf2)){
	fscanf(pf2,"%[^\n]\n",line);
	sscanf(line,"%d",&cont);
	datap[i]=cont;
	i++;
      }
      /**
     Read the randomly masc
     **/
      strcpy(cadena,"Mesh/Mascara");
      sprintf(name_file_masc,"%s-%d-%d",cadena,percent,1);
      pf3=fopen(name_file_masc,"r");
      if (pf3==NULL)
	printf("Cant open Masc \n");
      List_randomly=(int*)malloc(num_nodes*sizeof(int));
      i=0;
      aux=0;  
      while(!feof(pf3)) {
	fscanf(pf3,"%[^\n]\n",line);
	sscanf(line,"%d",&cont);
	List_randomly[i]=cont;
	i++;
      }
      
      if (fclose (pf3) != 0)
	printf ("Error: close the file list\n.");

      if (fclose (pf1) != 0)
	printf ("Error: close the mesh file\n");
      if (fclose (pf2) != 0)
	printf ("Error: close the metis file \n.");
    }
  
    MPI_Barrier(MPI_COMM_WORLD);
    if(myrank==0){ 
      i=1;
      j=0;
      while(i<partition_number){
	dest=i;
	MPI_Send(&list_num_nodes[i],1,MPI_INT,dest,tag,MPI_COMM_WORLD);
	MPI_Send(&num_nodes,1,MPI_INT,dest,tag,MPI_COMM_WORLD);
	MPI_Send(datap,num_nodes,MPI_INT,dest,tag,MPI_COMM_WORLD);
	MPI_Send(List_randomly,num_nodes,MPI_INT,dest,tag,MPI_COMM_WORLD);
	i++;
      }
        
      nodepart.num=list_num_nodes[0];
      nodepart.nodelist=(struct nodes *) malloc(nodepart.num * (sizeof(struct nodes)));
      for(r=0;r<nodepart.num;r++)
	nodepart.nodelist[r].Load=(float *)malloc(L*sizeof(float));
     }

    if(myrank!=0){ 
      MPI_Recv(&nodepart.num,1,MPI_INT,0,tag,MPI_COMM_WORLD,&stat);
      nodepart.nodelist=(struct nodes *) malloc(nodepart.num * (sizeof(struct nodes)));
      for(r=0;r<nodepart.num;r++)
	nodepart.nodelist[r].Load=(float *)malloc(L*sizeof(float));
      MPI_Recv(&num_nodes,1,MPI_INT,0,tag,MPI_COMM_WORLD,&stat);
      datap=(int *) malloc(num_nodes*(sizeof(int)));
      List_randomly=(int*)malloc(num_nodes*sizeof(int));
      MPI_Recv(datap,num_nodes,MPI_INT,0,tag,MPI_COMM_WORLD,&stat);
      MPI_Recv(List_randomly,num_nodes,MPI_INT,0,tag,MPI_COMM_WORLD,&stat);
      
    } 
    
    MPI_Barrier(MPI_COMM_WORLD);

   /**
    Creation of datatype ADG and ADG2
   **/ 
    contblocks=0;
    flag=0;
    for(i=0;i<num_nodes;i++){
      if (datap[i]==myrank){
	if (flag==0){
	  flag=1;
	  contblocks++;
	}
      }
      if (datap[i]!=myrank && (flag==1)) 
	flag=0;
    }     
    MPI_Barrier(MPI_COMM_WORLD);
    Bl=(int *)malloc(contblocks*(sizeof(int)));
    Ds=(int *)malloc(contblocks*(sizeof(int)));
    for(i=0;i<contblocks;i++){
      Bl[i]=0;
      Ds[i]=0;
    }
    
    flag=0;
    cont=0;
    for(i=0;i<num_nodes;i++){
      if (datap[i]==myrank)
      {
	if (flag==0){ 
	  flag=1;
	  cont++;
	  Bl[cont-1]++;
	  Ds[cont-1]=i; 
	}  
       else
	Bl[cont-1]++;
      }
      if (datap[i]!=myrank && (flag==1))
	flag=0;
    }

    MPI_Type_contiguous(L,MPI_TYPE,&loads);	
    MPI_Type_commit(&loads);
    MPI_Type_indexed(contblocks,Bl,Ds,loads,&ADG);	
    MPI_Type_indexed(contblocks,Bl,Ds,MPI_INT,&ADG2);
    MPI_Type_commit(&ADG);
    MPI_Type_commit(&ADG2);

    /**
    End of datatype creation
    **/

    p=0;
    List_randomly2=(int*)malloc(nodepart.num*sizeof(int));
    MPI_Pack(List_randomly,1,ADG2,List_randomly2,nodepart.num*sizeof(int),&p,MPI_COMM_WORLD);
    for (i=0;i<nodepart.num;i++)
      if( List_randomly2[i]==0){
	for(j=0;j<L;j++)
	  nodepart.nodelist[i].Load[j]=(float)0; 
      }
      else{
	gettimeofday(&tv,NULL);
	srand(tv.tv_usec) ;
	for(j=0;j<L;j++)
	  nodepart.nodelist[i].Load[j]=(float)(rand()%100+j*7);
      }
      
  
    for (i=0;i<nodepart.num;i++)
      for (j=0;j<L;j++)
	for(k=0;k<square;k++) 
	  nodepart.nodelist[i].Load[j]= sqrt( nodepart.nodelist[i].Load[j]);
    
    MPI_Barrier(MPI_COMM_WORLD);
    romiodata=(float *)malloc(nodepart.num*L*(sizeof(float)));
    cont=0;
    for (j=0;j<nodepart.num;j++){
      for(k=0;k<L;k++){
	romiodata[cont]=nodepart.nodelist[j].Load[k];
	cont++;
      }
    }
    /**
     AGGREGATION_PATTERN:
     If criteria ==0 -> Default aggregation pattern
     If criteria ==1 -> ACN
     If criteria ==2 -> AVN
     **/
    if (criteria!=0)
    aggregation_interval(myrank,partition_number,num_nodes,Ds,Bl,contblocks,cb_config_string,criteria);
    ret=MPI_Info_create(&info1);
    if (ret < 0)
    {
      printf("MPI_Info_create: error\n") ;
      exit(0) ;
    }
    strcpy(value,"enable");
    if (criteria!=0)
    MPI_Info_set(info1, "cb_config_list", cb_config_string);
    //MPI_Info_set(info1,"striping_unit",info_data2);

    /**
     Collective IO write using PVFS2
    **/
     
    i=MPI_File_open( MPI_COMM_WORLD, "pvfs2:/exports/work/inf_dir/bin/pvfs2/mnt/workfile",MPI_MODE_RDWR | MPI_MODE_CREATE, info1, &fh ) ;
    if(i!=MPI_SUCCESS)
      printf("Cant opne the file: pvfs2:/exports/work/inf_dir/bin/pvfs2/mnt/workfile -%d\n",myrank); 
    MPI_File_set_view( fh, 0, MPI_TYPE, ADG, "native", info1 ) ;
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_File_write_all(fh,romiodata,nodepart.num*L,MPI_TYPE,&stat);
    i=MPI_File_open( MPI_COMM_WORLD, "pvfs2:/exports/work/inf_dir/bin/pvfs2/mnt/workfile",MPI_MODE_RDWR | MPI_MODE_CREATE, info1, &fh ) ;
    if(i!=MPI_SUCCESS)
      if (myrank==0){
	
	romioread=(float *)malloc(num_nodes*(sizeof(float)));
	MPI_File_read(fh, romioread,num_nodes,MPI_INT, MPI_STATUS_IGNORE); 
	MPI_File_get_info(fh,&info2);
      }
      MPI_File_get_info(fh,&info2);
    MPI_File_close(&fh);
   
   /* If you want display the collective IO option used for write, use this code:
     MPI_Info_get_nkeys(info2, &nkeys);
    for (i=0; i<nkeys; i++) 
    {
      MPI_Info_get_nthkey(info2, i, key);
      MPI_Info_get(info2, key, MPI_MAX_INFO_VAL-1, value, &flag);
      if(myrank==0) 
	printf("Process %d, Default:  key = %s, value = %s\n", myrank,key, value);
      if(i==2)
	strcpy(strip_size,value);
    }
    
    */
    
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Info_free(&info1);
  }
  t2=MPI_Wtime();
  t3=MPI_Wtime();
  ta=2*t2-t1-t3;
  printf ("Time Total %d %d %s %d %lf \n",size,myrank,strip_size,L,ta*1000);
  ret=MPI_Finalize();
  if (ret != MPI_SUCCESS)
  { 
    printf("PROBLEMS FINALIZE!!!!!!!!!!!!!!!!!! -> exit() \n");
    exit(-1) ;
  }
  return 0;
  
}



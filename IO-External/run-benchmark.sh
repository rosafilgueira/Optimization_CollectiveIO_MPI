#!/bin/sh
#$ -N My_Rosa
#$ -cwd
#$ -pe openmpi_smp8_mark2 8
#$ -l h_rt=00:30:00


####################BORRAR FICHEROS########################################
 rm -f machines 
 rm -f machines_unique 
 rm -f pvfs_machines 
 rm -f pvfs_machines_orig
 rm -f mpi_machines
 rm -f num_pvfs
 rm -f num_mpi
########################################################################### 
NUM_PVFS=4
NUM_MPI=$NSLOTS



########################################################################### 
 NUM=1
 out=`cat $PE_HOSTFILE`
 while test $NUM -le $NSLOTS
 do
   STRING1=`head -n $NUM $PE_HOSTFILE | tail -1 | awk '{print $1}'`
   echo "$STRING1" >> machines 
   NUM=`expr $NUM + 1`
 done

uniq machines > machines_unique

##########################MACHINES################################################# 
./partition_machines.sh $NUM_PVFS machines_unique
# output : mpi_machines, pvfs_machines_orig
#####################PVFS2###################################################### 

STRING1=`awk -F . '{print $1}' pvfs_machines_orig`
echo "$STRING1" > pvfs_machines

/exports/work/inf_dir/bin/pvfs2/scripts/pvfs2-stop.sh pvfs_machines 
/exports/work/inf_dir/bin/pvfs2/scripts/lanzar-pvfs.sh pvfs_machines

export PVFS2TAB_FILE=/exports/work/inf_dir/bin/pvfs2/conf/pvfs2tab

env PVFS2TAB_FILE=/exports/work/inf_dir/bin/pvfs2/conf/pvfs2tab /exports/work/inf_dir/bin/pvfs2/scripts/pvfs2-status.sh pvfs_machines 

############################MPICH2######################################################
NUM_NODOS=`cat machines_unique | wc -l`
/exports/work/inf_dir/bin/mpich2/bin/mpdboot -n $NUM_NODOS -f machines_unique
MESH="1"
LOAD="100"
PROC="8"

  for mesh in $MESH
   do
      echo "mesh ""$mesh" > out-lap-acn-$mesh
      echo "mesh ""$mesh" > out-lap-avn-$mesh
      echo "mesh ""$mesh" > out-original-$mesh
     ./copy$mesh.sh
     for proc in $PROC
       do
       echo "process ""$proc" >> out-lap-acn-$mesh
       echo "process ""$proc" >> out-lap-avn-$mesh
       echo "process ""$proc" >> out-original-$mesh
         for load in $LOAD
           do
            echo "load ""$load" >> out-lap-acn-$mesh
            echo "load ""$load" >> out-lap-avn-$mesh
            echo "load ""$load" >> out-original-$mesh
            echo ""
            #input parameters if benchmark-IO: load, number of square operation, aggregation_criteria (0:default, 1:ACN, 2:AVN), percent of data equal 0.
              /exports/work/inf_dir/bin/mpich2/bin/mpirun -np $proc ./benchmark-IO $load 1 1 0 >> out-lap-acn-$mesh 
              /exports/work/inf_dir/bin/mpich2/bin/mpirun -np $proc ./benchmark-IO $load 1 2 0 >> out-lap-avn-$mesh 
              /exports/work/inf_dir/bin/mpich2/bin/mpirun -np $proc ./benchmark-IO $load 1 0 0 >> out-original-$mesh 
            done
       done
   done

/exports/work/inf_dir/bin/mpich2/bin/mpdallexit
########################################################################################
/exports/work/inf_dir/bin/pvfs2/scripts/pvfs2-stop.sh pvfs_machines 

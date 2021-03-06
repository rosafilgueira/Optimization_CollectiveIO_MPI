# Optimization_Collective_IO_MPI

This repostiroy is dedicated to store the optimization works that I developed for improving 
collective IO operations (Two-Phase collective I/O from ROMIO) in MPI Applications.

These opitmizations (techniques) employ the Linear Assignment Problem (LAP) for finding an optimal I/O data communication schedule,
reducing then the number of communications involved in the I/O collective operations.

Here, two different aggregation criteria are proposed:

- Aggregation-by-communication-number (ACN): This criteria assigns each
        aggregator to the node who has more highest number of contiguous data blocks
        of the file domain associated with the aggregator. 

- Aggregation-by-voume-number (AVN): This criteria assigns each
        aggregator to the node who has more data of the file domain associated with
        the aggregator. 

More information in the following sections. 

# Material stored in IO-External 

- Explanation of the benchmark (*bencharm-IO*) used for testing the optimizations 
- Explanation of aggregation_pattern library

## (1) The bencharmk-IO 

The *benchmark-IO* it is a version of BISP3D simulatador. This benchmark
can be defined as a 3-dimensional simulator of BJT and HBT bipolar devices.
The goal of the 3D simulation is to relate electrical characteristics of the
device with its physical and geometrical parameters. The basic equations to be
solved are Poisson'sequation and electron and hole continuity in a
stationary state. More details at [Optimization and evaluation of parallel I/O in BIPS3D paral
lel irregular application](http://www.arcos.inf.uc3m.es/~desingh/papers/2007/2007PMEO.pdf)

Finite element methods are applied in order to discretize the Poisson
equation, hole and electron continuity equations by using tetrahedral
elements. The result is an unstructured mesh. In this
work, we have used four different meshes, as described later.
Using the METIS library, this mesh is divided into sub-domains, in such
a manner that one sub-domain corresponds to one process.

The next step is decoupling the Poisson equation from
the hole and electron continuity equations. They are linearized by the Newton
method. Then we construct for each sub-domain in a parallel manner, the part
corresponding to the associated linear system. Each system is solved using
domain decomposition methods. Finally, the results are written to a file.

In the original BIPS3D version, the results are gathered at a root node, which
stores the data sequentially to the file system. 

We have modified BIPS3D to use collective writes during the I/O phase.
In the parallel I/O BIPS3D version, each compute node uses the distribution
information initially obtained from METIS and constructs a view over the file.
The view is based on an MPI data type. 

In order to achieve the MPI data type MPI_Type_Indexed is used. This data
type represents non-contiguous chunks of data of equal sizes and with
different displacements between consecutive elements.
Once the view on the common file is declared, the compute nodes write the data
to its corresponding file part by using Two_Phase I/O technique.

For the evaluations the benchrmak-IO could be executed using four different meshes: mesh1
[47200 nodes](https://github.com/rosafilgueira/Optimization_CollectiveIO_MPI/tree/master/IO-External/Mesh1), mesh2 [32888 nodes](https://github.com/rosafilgueira/Optimization_CollectiveIO_MPI/tree/master/IO-External/Mesh2), mesh3 [732563 nodes](https://github.com/rosafilgueira/Optimization_CollectiveIO_MPI/tree/master/IO-External/Mesh3) and mesh4 [289648 nodes](https://github.com/rosafilgueira/Optimization_CollectiveIO_MPI/tree/master/IO-External/Mesh4). 
The *benchmark-IO* associates a data structure to each node of a mesh. The
contents of these data structures are the data written to disk during the I/O
phase. The number of elements that this structure has per each mesh entry is
given by the load parameter (The FIRST PARAMETER of the executable). This means that, given a mesh and a load, the
number of data written is the product of the number of mesh elements and the
load. We have evaluated different loads, concretely, 100, 200 and
500.

For compiling *bechmark-IO*, a [script is stored in this repository](https://github.com/rosafilgueira/Optimization_CollectiveIO_MPI/blob/master/IO-External/compile-benchmark)

## (2) The aggregation_pattern.c library

Two-Phase I/O takes place in two phases: redistributed data exchange and an I/O
phase. In the first phase, by means of communication, small file requests are
grouped into larger ones. In the second phase, contiguous transfers are
performed to or from the file system. Before that, Two-Phase I/O divides the
file into equal contiguous parts (called File Domains (FD)), and assigns each
FD to a configurable number of compute nodes, called aggregators. Each
aggregator is responsible for aggregating all the data, which it maps inside
its assigned FD, and for transferring the FD to or from the file system. In
the default implementation of Two-Phase I/O the assignment of each aggregator
(aggregator pattern) is fixed, independent of distribution of data over the
processes.  

This fixed aggregator pattern might create a I/O bottleneck , as a
consequence of the multiple requests performed to collect all data assigned to
their FD. Therefore I proposed replacing the rigid assignment of aggregators
over the processes by new two different aggregation criteria:

	*Aggregation-by-communication-number (ACN): This criteria assigns each
	aggregator to the node who has more highest number of contiguous data blocks
	of the file domain associated with the aggregator. 

	*Aggregation-by-voume-number (AVN): This criteria assigns each
	aggregator to the node who has more data of the file domain associated with
	the aggregator. 

I have developed "aggregation_pattern.c" library that has implemented this two
aggregation patterns in a function called "aggregation_inteval". This function could be called from a parallel MPI
application, to obtain the aggregation list (called in this library cb_config_string) and configure
the aggregators by using one of MPIO-HINT (cb_config_list).

Important:

A script for running the *benchmark-IO* with the aggregation_patern is also [provided in this repository](https://github.com/rosafilgueira/Optimization_CollectiveIO_MPI/blob/master/IO-External/run-benchmark.sh), which has been used in [EDDIE cluster](http://www.ed.ac.uk/information-services/research-support/research-computing/ecdf)

Note:
We have used the collective IO write operations using the *PVFS2* parallel file system. 

For more details: 
- [Rosa Filgueira PhD Thesis](http://www.arcos.inf.uc3m.es/~rosaf/tesis.pdf)
- [Paper Data Locality Aware- VECPAR 2009] (http://link.springer.com/chapter/10.1007%2F978-3-540-92859-1_14)
- [Slides VECPAR 2009](https://github.com/rosafilgueira/Optimization_CollectiveIO_MPI/blob/master/Vecpar.pdf)
- [Journal Paper](http://link.springer.com/article/10.1007/s11227-010-0440-0#/page-1)

## Prototype of the function
	
	void aggregation_interval(int myrank, int partition_number, int num_nodes,int Ds[], int Bl[],int contblocks, char *cb_config_string, int criteria)

## Parameters of the function

	int myrank: (intput parameter) Process' rank.
	int partition_number: (input parameter) Number of processes.
	int num_nodes: (input parameter) Number of elements (in this case number of nodes of the mesh).
	in Bl[]: (input parameter) List of data blocks (the position of the first element of each data blocks)
	int Ds[]: (input parameter) List of size of data blocks (the number of the elements of each data blocks) 
	int contblocks: (input parameter) Number of data blocks that has each process. 
	char * cb_config_string: (output parameter) In this array, the aggreagation list is kept. 

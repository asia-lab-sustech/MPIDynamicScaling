# _Dynamic Scaling on MPI_ : MPI Process Provisioning / De-provisioning at Runtime

This is a header-only library to simplify the implementation of dynamic scaling on MPI 2.0.
You can add or remove MPI processes during runtime. The added processes can communicate to the running processes immediately.

- _**Scale Out**_: Add new MPI processes to the running processes. 
The scaled processes join in *the same MPI Intra-communicator* as the running processes so that they can communicate each other.
- _**Scale In**_: Remove MPI processes from the running processes.

#### Reference
> M. Hanai and G. Theodoropoulos _"Performance Evaluation of Dynamic Scaling on MPI"_ (2020)

## Quick Start
```bash
$ git clone git@github.com:masatoshihanai/MPIDynamicScaling.git
$ cd MPIDynamicScaling
$ mkdir build; cd build
$ cmake ..; make
$ mpirun -np 2 ./DynamicScalingExample 2 1 
```

## How to use
We provide the usage for (1) scaling out and (2) scaling in. See also the example code `example/main_example.cpp`.
### 1. Scale Out
There are two APIs for scaling out.
- __`scaleOut(MPI_Comm oldComm, int numAddProcs, string childCmd, MPI_Comm& newComm, vector<string> hosts)`__ :
This is used in the original program. The original program invokes new processes and waits for them to join the new intra-communicator.
  - `oldComm` is the original intra-communicator
  - `newAddProcs` is # additional MPI processes 
  - `childCmd` is the path to Children's program. 
  - `newComm` is the new intra-communicator including the additional processes. Their rank IDs are assigned incrementally.
  - `hosts` specifies additional hosts (optional). Each value is formatted as "`host01`" or "`host01 slots=32`" if you want to specify # slots.  
- __`initNewProcess(MPI_Comm& newComm)`__: This is used for the new processes invoked via `scaleOut()`. The new processes join to the new intra-communicator.
  - `newComm`: is the new intra-communicator.
  
### 2. Scale In
There is one API for scaling in. 
- __`scaleIn(MPI_Comm& oldComm, bool isRemoving, MPI_Comm& newComm, bool& thisHostCanTerminate)`__: The processes are removed after calling this function.
  - `oldComm` is the original communicator 
  - `isRemoving` the process is removed if this value is true; otherwise the process keeps alive. 
  - `newComm` is the new communicator excluding the removal processes.
  - `thisHostCanTerminate` is true if there are no MPI processes in this host (optional).

_**!!! Note !!!**_ **: The original MPI processes cannot be removed. Only ones added via `scaleOut()` can be removed**.  

## How to integrate to your project
The library is hear-only. You may simply copy and include `dynamic_scaling.hpp` to your MPI code.

## Tested Environment
- `Open MPI 2.1.1` `gcc/g++ 9.1.0`

#### Contact
mhanai at acm.org

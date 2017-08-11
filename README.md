# YAL
MPI and pthreads implementation of Yang-Anderson arbitration tree for mutual exclusion (https://www.cs.unc.edu/~anderson/papers/dc95.pdf)

YAL stands for Yang-Anderson Lock.
Repository has 3 files: YAL_listener.c, YAL_thread.c and YAL_main.c.
Execution occurs on an N-node MPI cluster, which can be built on Amazon Web Services (AWS) using tools such as starcluster (http://mpitutorial.com/tutorials/launching-an-amazon-ec2-mpi-cluster/).
At master node of the MPI cluster, compile as follows: /usr/bin/mpicc -lpthread YAL_thread.c YAL_listener.c YAL_main.c -o YAL -lm
Execute as in the following example: /usr/bin/mpirun -host master,node001,node002,node003,node004,node005,node006,node007 ./YAL 10
The above will run the YAL simulation on N=8 nodes for 10 seconds.
Simulation involves N processes repeatedly entering their critical section (CS), each process being coded as pthread YAL_thread.c.
Each lock is coded as YAL_listener.c: in the YAL algorithm, there are N processes (where N is a power of 2) and N-1 locks.
In this implementation, each YAL_listener accepts MPI-message requests from YAL_thread running on any node to change data structures C, T.
Each YAL_listener is also responsible for changing the P data structure of the YAL_thread on the node which it is running on.
Each of the N nodes thus runs a pthread representing a repeatedly CS-entering process, YAL_thread, as well as a listening process YAL_listener (N-1 of these listening processes are also locks of the YAL-tree).
Each YAL_thread will begin an access procedure to the CS, achieve its turn for the CS, and then exit - to begin another access after a random amount of time.
The time related settings are manipulable in YAL_thread.c and YAL_main.c


# YAL
MPI and pthreads implementation of Yang-Anderson Lock (YAL) arbitration tree for mutual exclusion (https://www.cs.unc.edu/~anderson/papers/dc95.pdf)

Execution occurs on an N-node MPI cluster, which can be built on Amazon Web Services (AWS) using tools such as MIT Starcluster (http://mpitutorial.com/tutorials/launching-an-amazon-ec2-mpi-cluster/). Caution: use of AWS in cluster-form is chargeable and can be expensive.

Repository has 4 files: YAL_listener.c, YAL_thread.c and YAL_main.c and a sample config file for starcluster tool.

At master node of the MPI cluster, compile as follows:

/usr/bin/mpicc -lpthread YAL_thread.c YAL_listener.c YAL_main.c -o YAL -lm

Now, execute as in the following example:

/usr/bin/mpirun -host master,node001,node002,node003,node004,node005,node006,node007 ./YAL 10

The above will run the YAL simulation on N=8 nodes for 10 seconds.

Simulation involves N processes repeatedly entering their critical section (CS), each process being coded as the pthread in YAL_thread.c.
Each lock is coded as the pthread in YAL_listener.c.
In the YAL algorithm, there are N processes (where N is a power of 2) numbered 0 to N-1 and N-1 locks arranged in a binary contention tree.
In this implementation, each YAL_listener accepts MPI-message requests from YAL_thread running on any node to change the lock's variables C, T.
Each YAL_listener also changes (due to MPI-message requests from YAL_thread of other nodes) the P variable of the YAL_thread corresponding to the node which it is running on.
Each of the N nodes thus runs a pthread representing a repeatedly CS-entering process, YAL_thread, as well as a listening process YAL_listener (N-1 of these listening processes are also locks of the YAL-tree).
Each YAL_thread will begin an access procedure to the CS, achieve its turn for the CS, read and increment a global variable (via MPI), and then exit - to begin another access episode immediately after.
The global variable is held, communicated to threads and updated via MPI, at node number (N-1), as it is lightly loaded owing to having no corresponding lock.

Sample output:

root@master:/home/ubuntu# /usr/bin/mpicc -lpthread YAL*.c -o YAL -lm
root@master:/home/ubuntu# for i in {2..30..2}; do /usr/bin/mpirun -host master,node001,node002,node003,node004,node005,node006,node007 ./YAL $i; done

In 2s, each node enters C-S average 2.00x, 1.67s delay (max 2.65s) Remote Accesses (RA): 66.50, RA-per-CS: 33.25, Mutex-Count: 16.0
In 4s, each node enters C-S average 3.00x, 1.95s delay (max 2.90s) Remote Accesses (RA): 101.12, RA-per-CS: 33.71, Mutex-Count: 24.0
In 6s, each node enters C-S average 4.38x, 1.99s delay (max 2.82s) Remote Accesses (RA): 150.88, RA-per-CS: 34.49, Mutex-Count: 34.0
In 8s, each node enters C-S average 5.50x, 1.99s delay (max 3.18s) Remote Accesses (RA): 185.50, RA-per-CS: 33.73, Mutex-Count: 43.0
In 10s, each node enters C-S average 7.12x, 1.94s delay (max 2.95s) Remote Accesses (RA): 238.12, RA-per-CS: 33.42, Mutex-Count: 57.0
In 12s, each node enters C-S average 9.25x, 1.82s delay (max 2.85s) Remote Accesses (RA): 303.62, RA-per-CS: 32.82, Mutex-Count: 73.0
In 14s, each node enters C-S average 10.00x, 1.89s delay (max 3.27s) Remote Accesses (RA): 334.50, RA-per-CS: 33.45, Mutex-Count: 78.0
In 16s, each node enters C-S average 11.00x, 1.99s delay (max 3.10s) Remote Accesses (RA): 365.25, RA-per-CS: 33.20, Mutex-Count: 88.0
In 18s, each node enters C-S average 12.50x, 1.95s delay (max 2.85s) Remote Accesses (RA): 421.12, RA-per-CS: 33.69, Mutex-Count: 99.0
In 20s, each node enters C-S average 13.50x, 2.04s delay (max 3.20s) Remote Accesses (RA): 459.25, RA-per-CS: 34.02, Mutex-Count: 107.0
In 22s, each node enters C-S average 16.12x, 1.88s delay (max 2.73s) Remote Accesses (RA): 535.12, RA-per-CS: 33.19, Mutex-Count: 128.0
In 24s, each node enters C-S average 16.88x, 1.97s delay (max 2.96s) Remote Accesses (RA): 570.88, RA-per-CS: 33.83, Mutex-Count: 134.0
In 26s, each node enters C-S average 17.50x, 1.98s delay (max 3.26s) Remote Accesses (RA): 580.38, RA-per-CS: 33.16, Mutex-Count: 139.0
In 28s, each node enters C-S average 17.25x, 2.12s delay (max 3.36s) Remote Accesses (RA): 588.50, RA-per-CS: 34.12, Mutex-Count: 136.0
In 30s, each node enters C-S average 18.50x, 2.10s delay (max 3.04s) Remote Accesses (RA): 629.88, RA-per-CS: 34.05, Mutex-Count: 143.0

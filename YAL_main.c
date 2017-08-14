/*11.8.17: Implementation of Yang-Anderson arbitration tree for Mutual Exclusion (https://www.cs.unc.edu/~anderson/papers/dc95.pdf).*/
/*Uses MPI and pthreads. Handles N=2^k, k=1,2,3,... processes contending for critical section, thus needing a k-level binary arbitration tree.*/
/*Each lock in the arbitration tree is represented by pthread YAL_listener.c. Thus, save for the N-th node, all nodes have a YAL_listener.*/
/*This lock node receives messages from various pthreads named YAL_threads (numbered from 0 to N-1) for data structures C and T*/
/*YAL_listener also receives requests to change the P corresponding to the YAL_thread that is running on its node*/
/*If separate nodes were needed for locks and processes, then 2*N-1 ndoes would be required, which would be expensive if renting out via AWS*/
/*Easiest way to run the program is through starcluster as seen here (http://mpitutorial.com/tutorials/launching-an-amazon-ec2-mpi-cluster/)*/
/*In the config file, the field "CLUSTER_SIZE = 32" (for example) indicates N. Field "AWS_USER_ID = XXXXXXXXXXX" requires AWS Account No.*/
/*In the critical section, each YAL_thread increments a global variable (YAL_listener at node (N-1) is in custody of this global variable)*/

#define OWN_P_TAG 1
#define P_TAG 2
#define FREEDYA_TAG 3
#define NUMBER_TAG 4
#define T_TAG 5
#define C_TAG 6
#define WRAPUP_TAG 7
#define THREAD_WINDUP_TAG 9
#define LISTENER0_WINDUP_TAG 10
#define GLOBAL_COUNT_TAG 11

#define READ_P_TAG 12
#define READ_FREEDYA_TAG 13
#define READ_NUMBER_TAG 14
#define READ_T_TAG 15
#define READ_C_TAG 16
#define READ_WRAPUP_TAG 17

#define SENDER_TAG 7
#define RECEIVER_TAG 8

#define TRUE 1
#define FALSE 0
#define VERBOSE 0

#define TOTAL_PROCESSES 8	/*12.4.11: has to match during MPI run instantiation at CLI*/

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>
#include <sys/timeb.h>

int process_count;
int remote_accesses;

int MCS_times_count;
int MCS_queue_number;
int MCS_already_linkedin;
int LISTENER_ALIVE;
int GLOBAL_COUNT;

float MCS_avg_queue_length;
float MCS_avg_locks_scanned;

float spartime;
float max_spartime;
float wrapup_results[TOTAL_PROCESSES][5];

MPI_Status status, status_main;

float system_time;
float creation_time;
long int baseline_time;
struct timeb baseline_precise_time;

int Number;
int C[2];
int P;
int T;
int Freed_YA;
int time_horizon;
int total_processes;
int process_index;

struct flagdata { float start_time; float end_time; };
struct flagdata flag;

void *Process (void *parameters);
void *Listener (void *parameters);
MPI_Status   status_thread;

int main(int argc, char* argv[])
{
int i, j, k, l, retries, namelen, message, ierr, iprobe_flag, threading_provided;
int Listener_identifier[2], Process_identifier[2];
MPI_Request wrapup_request[TOTAL_PROCESSES];

int thread_process_index, count_THREAD_WRAP_UP_received, results_received_count;

float past_creation_time=0.0, average_CS=0.0, average_RA=0.0, average_SPAR=0.0, max_SPAR=0.0;
float inter_arrival_time=0.0, toss=0.0, GLOBAL_COUNT_TOTAL=0.0;

char processor_name[20];

long int GFLOPS;
double gflops, gflops_j;

struct timeb scrap_precise_time;

pthread_t Process_tid;
pthread_t Listener_tid;
pthread_attr_t Process_attr;
pthread_attr_t Listener_attr;

system_time=0.0; creation_time=0.0; spartime=0.0, max_spartime=0.0;
process_count=0; retries=0; remote_accesses=0, namelen=0;

MCS_avg_queue_length=0.0; MCS_avg_locks_scanned=0.0;
MCS_times_count=0; MCS_queue_number=0; MCS_already_linkedin=0;

srandom(time(NULL));

sscanf(((char *)(argv[1])), "%d", &time_horizon);

/*MPI_Init(&argc, &argv);*/
MPI_Init_thread( &argc, &argv, MPI_THREAD_MULTIPLE, &threading_provided);

MPI_Comm_rank(MPI_COMM_WORLD,&process_index);
MPI_Comm_size(MPI_COMM_WORLD,&total_processes);
MPI_Get_processor_name(processor_name, &namelen);

baseline_time=(long int)(time(&baseline_time));
ftime(&baseline_precise_time);

Listener_identifier[0]=process_index;
Listener_identifier[1]=time_horizon;

Process_identifier[0]=process_index;
Process_identifier[1]=time_horizon;

Number=0;
Freed_YA=0; T=0; P=0; C[0]=-1; C[1]=-1;

pthread_attr_init(&Listener_attr);
pthread_attr_init(&Process_attr);

LISTENER_ALIVE=TRUE;
GLOBAL_COUNT=0;

pthread_create(&Listener_tid, &Listener_attr, Listener, (void *)(Listener_identifier));
pthread_create(&Process_tid, &Process_attr, Process, (void *)(Process_identifier));

pthread_join(Process_tid, NULL);

/*11.8.17: Node 0 does the job of receiving simulation results from all other nodes and processing them to printf averages*/
if(!process_index)
	{

	wrapup_results[process_index][0]=process_count;
	wrapup_results[process_index][1]=remote_accesses;
	wrapup_results[process_index][2]=spartime;
	wrapup_results[process_index][3]=max_spartime;
	wrapup_results[process_index][4]=GLOBAL_COUNT;

	for (i=0;i<TOTAL_PROCESSES;i++)
		{
		thread_process_index=process_index;
		ierr=MPI_Send( &thread_process_index, 1, MPI_INT, i, THREAD_WINDUP_TAG, MPI_COMM_WORLD);
		if (VERBOSE)
			{ { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
			printf("P%d: THREAD_WINDUP to P%d", process_index, i);
			fflush(stdout); }
		}

	pthread_join(Listener_tid, NULL);

	/*9.8.17: Before other nodes send result packets (which the Listener may consume), node 0 sends them a message indicating its YAL_Listener has now shut down*/

	for (i=1;i<TOTAL_PROCESSES;i++)
		{
		thread_process_index=process_index;
		ierr=MPI_Send( &thread_process_index, 1, MPI_INT, i, LISTENER0_WINDUP_TAG, MPI_COMM_WORLD);
		if (VERBOSE)
			{ { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
			printf("P%d: LISTENER0_WINDUP to P%d", process_index, i);
			fflush(stdout); }
		}

	/*9.8.17: Node 0 receives result packets from all other nodes about their performance in critical sections*/
	results_received_count=0;
	while (results_received_count < TOTAL_PROCESSES-1)		
		{		
		        
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &iprobe_flag, &status_main);
			
		if(iprobe_flag)
			{
 			i=status_main.MPI_SOURCE;
			MPI_Recv(&wrapup_results[i], 5, MPI_FLOAT, i, READ_WRAPUP_TAG, MPI_COMM_WORLD, &status_main);

			if (VERBOSE)
			{{ printf("\n"); fflush(stdout); for (l=0; l<i; l++) {printf("\t"); fflush(stdout);} }
			printf("P%d result in", i);
			fflush(stdout);}
			
			results_received_count++;
			} 
		}

	/*MPI_Waitall(TOTAL_PROCESSES-1, (MPI_Request *)(wrapup_request+1), MPI_STATUSES_IGNORE);*/

	for (i=0;i<TOTAL_PROCESSES;i++)
		{
		average_CS+=wrapup_results[i][0];
		average_RA+=wrapup_results[i][1];
		average_SPAR+=wrapup_results[i][2];
		max_SPAR=(max_SPAR<wrapup_results[i][3])? wrapup_results[i][3]:max_SPAR;
		GLOBAL_COUNT_TOTAL+=wrapup_results[i][4];
		}

	average_CS/=TOTAL_PROCESSES;
	average_SPAR/=TOTAL_PROCESSES;
	average_RA/=TOTAL_PROCESSES;

	printf("\nIn %ds, each node enters C-S average %0.2fx, %0.2fs delay (max %0.2fs) Remote Accesses (RA): %0.2f, RA-per-CS: %0.2f, Mutex-Count: %0.1f", time_horizon, average_CS, average_SPAR, max_SPAR, average_RA, average_RA/average_CS, GLOBAL_COUNT_TOTAL);
	fflush(stdout);

	}
else	{
	
	/*9.8.17: Each YAL_Listener process should shut down only after all the YAL_thread stop, hence THREAD_WINDUP messages to all nodes, which the YAL_Listener will count - and finish via pthread_join below this loop.*/
	for (i=0;i<TOTAL_PROCESSES;i++)
		{
		thread_process_index=process_index;
		ierr=MPI_Send( &thread_process_index, 1, MPI_INT, i, THREAD_WINDUP_TAG, MPI_COMM_WORLD);
		if (VERBOSE)
			{ { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
			printf("P%d: THREAD_WINDUP to P%d", process_index, i);
			fflush(stdout);}
		}

	pthread_join(Listener_tid, NULL);

	wrapup_results[process_index][0]=process_count;
	wrapup_results[process_index][1]=remote_accesses;
	wrapup_results[process_index][2]=spartime;
	wrapup_results[process_index][3]=max_spartime;
	wrapup_results[process_index][4]=GLOBAL_COUNT;

	ierr=MPI_Send( &wrapup_results[process_index], 5, MPI_FLOAT, 0, READ_WRAPUP_TAG, MPI_COMM_WORLD);

	if (VERBOSE)
		{ { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
		printf("P%d: Result Sent to P%d", process_index, 0);
		fflush(stdout);}
	}

/*6.5.11: run repeated MPI_Recv to clear MPI_COMM_WORLD of messages, if any*/
MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &iprobe_flag, &status_main);
while(iprobe_flag)
	{ 
	MPI_Recv(&Number, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status_main);
	printf("\n\t%d: 1 message thrown out", process_index);
	MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &iprobe_flag, &status_thread);
	}

MPI_Finalize();
}




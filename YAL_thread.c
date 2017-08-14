#define OWN_P_TAG 1
#define P_TAG 2
#define FREEDYA_TAG 3
#define NUMBER_TAG 4
#define T_TAG 5
#define C_TAG 6
#define WRAPUP_TAG 7

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

#define REGENERATE_INTERVAL (TOTAL_PROCESSES) /*26.5.11: experimentally make REGENERATE_INTERVAL to 1xTOTAL_PROCESSES*/
#define REGENERATE_TIME 3

#define CS_INTERVAL 1.0 	/*29.3.11: stands for 2ms*/
#define CS_TIME     3		/*6.4.11: runtime in C-S or time between regeneration is 1x, 2x, ..., RUNTIME_LIMITx*/

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>
#include <sys/timeb.h>

/*D-S for keeping time information*/

extern long int system_time;
extern long int creation_time;
extern long int baseline_time;
extern int time_horizon;
extern struct timeb baseline_precise_time;

extern int remote_accesses;

extern int MCS_queue_number;
extern int MCS_already_linkedin;
extern int MCS_times_count;
extern float MCS_avg_queue_length;
extern float MCS_avg_locks_scanned;

extern float spartime;
extern float max_spartime;
extern float wrapup_results[TOTAL_PROCESSES][4];

extern MPI_Status   status_thread;

extern int C[2];
extern int T;
extern int P;
extern int Freed_YA;
extern int Number;
extern int total_processes;
extern int process_count;
extern int process_index;
extern int GLOBAL_COUNT;

struct flagdata { float start_time; float end_time; };
extern struct flagdata flag;

int lock_number(int process_index, int level)
	{
	int current_level_lock, locks, levels;

	current_level_lock=floor( process_index/ ((int) pow(2, level+1)));
	
	locks=0;

	for (levels=0; levels<level; levels++)
		locks+=floor(TOTAL_PROCESSES/((int) pow(2, levels+1)));

	if(!level) locks=current_level_lock; else locks+=current_level_lock;

	return locks;
	}

int lock_sequence(int level, int order)
	{
	int levels, locks;

	locks=0;

	for (levels=0; levels<level; levels++)
		locks+=(TOTAL_PROCESSES/pow(2, levels+1));

	locks+=order;
	return locks;
	}

int side(int process_index, int level)
	{
	/*merely probing the relevant bit in process_index should do the job*/
	int temporary, j;
	temporary = process_index & ((int)(pow(2, level)));

	temporary = (temporary)?1:0;

	return( temporary );
	}

/*31.7.2017: Recursive routine that acquires locks leading towards apex. This thread is the actual process thread (not the lock thread)*/
void YA_CS(int process_index, int level, int running_time)
	{

	int rival, j, J, k, l, ierr, message, message1, message2, T_process_index, P_rival, NTjk, Number_Temp;
	long int scrap_time;
	long int GFLOPS;
	double gflops, gflops_j;

	int MCS_queue_length, MCS_locks_scanned, irecv_count, junk;
	int global_count;
	int head_lowcost_MCS, tail_lowcost_MCS, Tjk, Cjk0, Cjk1, P_Freed_YA_request_index;
	struct timeb scrap_precise_time;
	MPI_Request P_Freed_YA_request[3], Number_request;

	MCS_queue_length=0;

	/*Request for lock-holder thread to write its 'C' followed by 'T' next*/
	message=2*(process_index+1) + side(process_index, level) - 1;
	ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), C_TAG, MPI_COMM_WORLD);
	remote_accesses++;

	if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): C[%d][%d]:=1", process_index, level, lock_number(process_index, level), side(process_index, level));
	fflush(stdout);}

	message=1000+process_index;
	ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), T_TAG, MPI_COMM_WORLD);
	remote_accesses++;

	if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): T[%d]:=%d", process_index, level, lock_number(process_index, level), process_index);
	fflush(stdout);}

	/*20.4.11: Request to write own 'P' as 3*level+0. 31.7.2017: YA paper only requires identifiers 0, 1 and 2*/
	P=3*level+0;

	if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): P[%d]:=%d", process_index, level, process_index, P);
	fflush(stdout);}

	/*12.4.11: Request-Response pair to read 'C' of other side entrant at the lock thread*/
	message=-2*(process_index+1) - (1-side(process_index, level)) +1;
	ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), C_TAG, MPI_COMM_WORLD);
	MPI_Recv( &rival, 1, MPI_INT,  lock_number(process_index, level), READ_C_TAG, MPI_COMM_WORLD, &status_thread);
	remote_accesses+=2;

	if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): Rival P%d", process_index, level, rival);
	fflush(stdout);}

	/*If there is a non-trivial rival process contending for the lock thread*/
	if (rival!=-1)
		{
		/*12.4.11: Request-Response pair to read 'T' from lock-holder thread*/
		message=-1000-process_index;
		ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), T_TAG, MPI_COMM_WORLD);
		MPI_Recv(&T_process_index, 1, MPI_INT, lock_number(process_index, level), READ_T_TAG, MPI_COMM_WORLD, &status_thread);
		remote_accesses+=2;

		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): T[%d]==%d", process_index, level, lock_number(process_index, level), T_process_index);
		fflush(stdout);}

		if ( T_process_index  == process_index )
			{
			
			/*12.4.11: Request-Response pair to read rival's 'P'*/
			message=-1500-process_index;
			ierr=MPI_Send( &message, 1, MPI_INT, rival, P_TAG, MPI_COMM_WORLD);
			MPI_Recv(&P_rival, 1, MPI_INT, rival, READ_P_TAG, MPI_COMM_WORLD, &status_thread);
			remote_accesses+=2;

			if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
			printf("(%d, %d): P[%d]==%d", process_index, level, rival, P_rival);
			fflush(stdout);}

			/*12.4.11: If 'P' at rival is 0, write this 'P' as 1, or equivalent 3*level+1 (note explanation later for 3*level+0)*/
			if (P_rival==3*level+0)
				{
				message=1500+(3*level+1);
				ierr=MPI_Send( &message, 1, MPI_INT, rival, P_TAG, MPI_COMM_WORLD);
				remote_accesses++;

				if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
				printf("(%d, %d): P[%d]:=%d", process_index, level, rival, message-1500);
				fflush(stdout);}
				}

			/*16.5.11: waiting for P to change to 3*level+1*/
			/*Local-spinning on P for a lock that may be at any level in the YA-tree*/

			if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
			printf("(%d, %d): SPIN(P==0) for %d", process_index, level, rival);
			fflush(stdout);}

			/*3.8.2017: Explanation for why level must be encoded in P-value: T5 noticed T4 as rival in level 0. Changes the P of T4 to 1. T4 though is lock-holder at level 0 and is spinning at (P==0) but at level 1 (with Rival T7). Now, T4 advances to next step because T5 changes its P. While that is a clear error, there is the surprise that both T4 and T5 indicate that they're spinning to obtain relief from T5 and T2, respectively. This situation also causes the spin to be effectively on P<=3*level+0 and P<=3*level+1 below*/

			while( P==3*level+0 ); 

			/*11.8.2017: Like in YADL, permitting change in P only if it corresponds to level solves earlier problem*/
			/*4.8.2017: Explanation for 2nd OR condition: say T1 is exiting C-S in apex lock (1, 2) for N=8. It transfers value of T to 'rival' variable (line 17 of YA algorithm). T may have earlier occupant of apex lock, say T2. This T2 may now have restarted contention, and is perhaps at lock (2, 0). T1 would otherwise change T2's P to 2 and T2 would wrongly claim the (2, 0) lock.*/

			/*26.5.11: reading current T*/
			message=-1000-process_index;
			ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), T_TAG, MPI_COMM_WORLD);
			MPI_Recv(&T_process_index, 1, MPI_INT, lock_number(process_index, level), READ_T_TAG, MPI_COMM_WORLD, &status_thread);
			remote_accesses+=2;

			if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
			printf("(%d, %d): T[%d]==%d", process_index, level, lock_number(process_index, level), T_process_index);
			fflush(stdout);}

			if (T_process_index==process_index)
				{
				if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
				printf("(%d, %d): SPIN(P<=1) for %d", process_index, level, lock_number(process_index, level), T_process_index, rival);
				fflush(stdout);}

				while ( P<=3*level+1 ) ; /*11.8.2017: statement simplified after ensuring P changes only for same level*/
				}

			}
		else 	
			; 
		} /*end of if(rival!=-1)*/


	if ( level!=(log10(TOTAL_PROCESSES)/log10(2.0))-1)
		{
		/*demanding lock at next level for the same process*/
		YA_CS(process_index, level+1, running_time);
		}
	else 
		{
		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): Enters C-S", process_index, level, lock_number(process_index, level));
		fflush(stdout);}

		/*12.8.17: Request-Response pair to read 'GLOBAL_COUNT' from listener (N-1)*/
		if (process_index != TOTAL_PROCESSES-1)		
			{
			message=-process_index;
			ierr=MPI_Send( &message, 1, MPI_INT, TOTAL_PROCESSES-1, GLOBAL_COUNT_TAG, MPI_COMM_WORLD);
			MPI_Recv( &global_count, 1, MPI_INT, TOTAL_PROCESSES-1, GLOBAL_COUNT_TAG, MPI_COMM_WORLD, &status_thread);
			}
		else
			global_count=GLOBAL_COUNT;

		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): GLOBAL_COUNT==%d", process_index, level, global_count);
		fflush(stdout);}
		
		global_count++;

		/*12.8.17: Write 'global_count' using listener (N-1)'s services, provided this thread isn't at node (N-1)*/
		if (process_index != TOTAL_PROCESSES-1)		
			{
			message=global_count;
			ierr=MPI_Send( &message, 1, MPI_INT, TOTAL_PROCESSES-1, GLOBAL_COUNT_TAG, MPI_COMM_WORLD);
			}
		else
			GLOBAL_COUNT=global_count;

		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): GLOBAL_COUNT:=%d", process_index, level, global_count);
		fflush(stdout);}
		}

	/*READ: message=-2*(process_index+1) - (1-side(process_index, level)) +1;*/
	/*message=2*(process_index+1)+side(process_index, level)-1;*/

	message= 2*(process_index+1) + side(process_index, level)-1;
	ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), C_TAG, MPI_COMM_WORLD);
	remote_accesses++;

	if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): reset C[%d]", process_index, level, lock_number(process_index, level));
	fflush(stdout);}

	/*12.4.11: Request-Response pair to read 'T' from lock-holder thread*/
	message=-1000-process_index;
	ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), T_TAG, MPI_COMM_WORLD);
	MPI_Recv(&rival, 1, MPI_INT, lock_number(process_index, level), READ_T_TAG, MPI_COMM_WORLD, &status_thread);
	remote_accesses++;

	if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): T[%d]==%d", process_index, level, lock_number(process_index, level), rival);
	fflush(stdout);}

	/*12.4.11: Request to write rival's 'P' as 2*/
	if ( rival!=process_index )
		{ 
		message=1500+(3*level+2);
		ierr=MPI_Send( &message, 1, MPI_INT, rival, P_TAG, MPI_COMM_WORLD);
		remote_accesses++;

		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): P[%d]:=%d", process_index, level, rival, message-1500);
		fflush(stdout);}
		}

	return;
	}

void* Process(void *parameters)
{

	int i, j, k, l, rival, running_time, retries, message, ierr;
	int head_local;
	float start_time, end_time, inter_arrival_time, toss;
	long int GFLOPS;
	double gflops, gflops_j;

	float waiting_time_YA, waiting_time_MCS, waiting_time_total, past_creation_time;
	struct timeb scrap_precise_time;

	past_creation_time=0.0;
	inter_arrival_time=0.0;

	do
	{

	past_creation_time=creation_time;

	if (Number==0)
		{ 
		Number=-(process_index+1);
		/*21.4.11: will need to write Number using MPI routine*/

		ftime(&scrap_precise_time);
		system_time = (scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm);

		toss=(int)(ceil(CS_TIME*((float)(random()/(RAND_MAX+1.0)))));

		i=process_index;
		start_time=(system_time > creation_time)? system_time: creation_time;
		flag.start_time=start_time;
		running_time=toss;

		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d,%d): N[%d]:=%d", process_index, -1, process_index, Number);
		fflush(stdout);}

		YA_CS(i, 0, running_time); /*is a recursive routine for accesssing locks in YA binary tree during contention*/

		process_count++;
		
		Number=0;

		ftime(&scrap_precise_time);
		system_time = (scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm);
		end_time=(system_time > creation_time) ? system_time: creation_time;

		if (VERBOSE)
		{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("P%d Done %dx @%0.2f", i, process_count, end_time);
		fflush(stdout);}

		ftime(&scrap_precise_time);
		waiting_time_total=((scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm))-start_time;

		spartime=((process_count-1)*spartime+(waiting_time_total-0.001*running_time))/process_count;
		max_spartime=(max_spartime<waiting_time_total-0.001*running_time)? (waiting_time_total-0.001*running_time):max_spartime;

		ftime(&scrap_precise_time);
		flag.start_time=start_time;
		flag.end_time=(scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm);

		}
	else 
		retries++;

	ftime(&scrap_precise_time);
	creation_time=(scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm);
	inter_arrival_time=((process_count-1)*inter_arrival_time+(creation_time-past_creation_time))/process_count;

	}while(creation_time<time_horizon);

	if(VERBOSE)
	{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
	printf("Thread-%d exits", i);
	fflush(stdout);}

	pthread_exit(&process_count);
}


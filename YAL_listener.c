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
#include <unistd.h>
#include <time.h>
#include <sys/timeb.h>

extern int C[2];
extern int T;
extern int P;
extern int Freed_YA;
extern int Number;
extern int time_horizon;
extern int total_processes;
extern int LISTENER_ALIVE;
extern int GLOBAL_COUNT;

extern struct timeb baseline_precise_time;

MPI_Status status_listener;

void *Listener(void* parameters)
{
  int message, code, side, process, process_index, ierr, iprobe_flag, l, temp_P;
  float listening_time;
  int count_THREAD_WINDUP_TAG_received;
  struct timeb scrap_precise_time;
  MPI_Request Isend_request;

  process_index=((int *)(parameters))[0];

  do {

    /*Continously check for a message from some process related to lock D-Ses C, T or own Number, P, Freed_YA D-Ses*/
     MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &iprobe_flag, &status_listener);

     if (iprobe_flag && status_listener.MPI_TAG != READ_C_TAG && status_listener.MPI_TAG != READ_P_TAG && status_listener.MPI_TAG != READ_T_TAG && status_listener.MPI_TAG != READ_NUMBER_TAG && status_listener.MPI_TAG != READ_FREEDYA_TAG && status_listener.MPI_TAG != READ_WRAPUP_TAG)
     {
     MPI_Recv(&message, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status_listener);

     /* check for message conditions */
     switch( status_listener.MPI_TAG )
     {

     case  C_TAG:		/*write C (set or reset) if message is a +ve integer, read C and send to contending process if -ve integer*/
					{ 
					  code=abs(message);
					  side=(code-1)%2;
					  process=floor((code-1)/2);

					  if(message>0) { 
							if(VERBOSE) { { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) { printf("\t"); fflush(stdout); } }
							printf("C[%d][%d]==%d", process_index, side, C[side]);
							fflush(stdout); }

							if (C[side]==-1)
								C[side]=process;
							else
								if (C[side]==process)
									C[side]=-1;
							if(VERBOSE) { { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) { printf("\t"); fflush(stdout); } }
							printf("C[%d][%d]:=%d", process_index, side, C[side]);
							fflush(stdout); }

							}
					  else 		{ 
							ierr=MPI_Send( &(C[side]), 1, MPI_INT, process, READ_C_TAG, MPI_COMM_WORLD); 

							if(VERBOSE) {{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) { printf("\t"); fflush(stdout); } }
							printf("To P%d: C[%d][%d]==%d", process, process_index, side, C[side]);
							fflush(stdout);}
							}

					}

					break;

     case  T_TAG: 		/*write T if message is +ve (code for T is such that it is staggered to be integer above 1000), read T if -ve*/
					{ 
					code=abs(message);
					process=code-1000;

					if(message>=1000)
						{
						T=process;
						if(VERBOSE) { { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) { printf("\t"); fflush(stdout); } }
						printf("T[%d]:=%d", process_index, T);
						fflush(stdout); }
						}
					else { 	
						ierr=MPI_Send( &T, 1, MPI_INT, process, READ_T_TAG, MPI_COMM_WORLD/*, &Isend_request*/);

						if(VERBOSE) {{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) { printf("\t"); fflush(stdout); } }
						printf("To P%d: T[%d]==%d", process, process_index, T);
						fflush(stdout);}
					     }
					}
					break;

    case P_TAG:			/*write or read for P*/
				/*31.7.2017: Note that this P is being written or read by YAL_listener on behalf of process (thread), not lock*/
				/*31.7.2017: Changes due to writing P will occur in YA_CS of YAL_thread.c. Those changes not shown here*/
					{
					  code=abs(message);
					  process=code-1500;

					  if(message>=1500)
						{
						int candidate_P, candidate_status, candidate_level;
						int current_P, current_status, current_level;
						candidate_P=message-1500;	

						/*10.8.17: candidate_P is examined for higher or lower value vis-a-vis current value of P*/
						/*the idea is to not assign a value to P if it is communicated from a level of lock that is higher (or lower) than the current level of contention*/
						/*10.8.17: A check for this already conducted in YADL_thread (2 places, part of YA algorithm) but spinning on P *could* continue if a correctly-set value is over-ruled by a wrongly set value immediately after*/
						candidate_status = candidate_P%3;
						candidate_level  = (candidate_P-candidate_status)/3;

						current_status = P%3;
						current_level  = (P-current_status)/3;

						if (candidate_level > current_level || candidate_level < current_level)
							;
						else
							P=candidate_P;

						if(VERBOSE) { { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) { printf("\t"); fflush(stdout); } }
						printf("P[%d]:=%d", process_index, P);
						fflush(stdout); }

						}
					  else  {
						ierr=MPI_Send( &P, 1, MPI_INT, process, READ_P_TAG, MPI_COMM_WORLD);

						if(VERBOSE) { { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) { printf("\t"); fflush(stdout); } }
						printf("To P%d: P[%d]==%d", process, process_index, P);
						fflush(stdout); }
						}
					}

					break;

					/*12.8.17: segment where read/write requests for GLOBAL_COUNT variable arrives. Two prevent 2 listener thread on the same node, these requests arrive only from nodes numbered other than (TOTAL_PROCESSES-1), as and when they enter C-S*/

    case GLOBAL_COUNT_TAG:		
					if (message < 1)
						{
						if (VERBOSE)
						{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
						printf("To P%d: GLOBAL_COUNT==%d", -message, GLOBAL_COUNT);
						fflush(stdout);}
						ierr=MPI_Send( &GLOBAL_COUNT, 1, MPI_INT, -message, GLOBAL_COUNT_TAG, MPI_COMM_WORLD);
						}
					else
						{
						/*Assigning GLOBAL_COUNT to whatever number is sent by YAL_thread at a node*/
						GLOBAL_COUNT=message;

						if (VERBOSE)
						{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
						printf("From P%d: GLOBAL_COUNT:=%d", status_listener.MPI_SOURCE, GLOBAL_COUNT);
						fflush(stdout);}
						}
					break;


					/*9.8.17: Handshake protocol to complete the simulation. Taken from YAMCS simulation: when the YAL_thread process at each node completes, it sends a THREAD_WINDUP message. These messages are counted, and have to number TOTAL_PROCESSES for node 0, whereas they must number TOTAL_PROCESSES+1 for all other nodes. Reason is written in the next 'case'*/
    case THREAD_WINDUP_TAG:		if (VERBOSE)
					{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
					printf("P%d: THREAD_WINDUP of P%d, #%d", process_index, message, count_THREAD_WINDUP_TAG_received+1);
					fflush(stdout);}
					count_THREAD_WINDUP_TAG_received++;				
					if (count_THREAD_WINDUP_TAG_received == TOTAL_PROCESSES+ (process_index>0))
						LISTENER_ALIVE=FALSE;
					
					break;
					/*9.8.17: Because all other nodes marked 1 to (TOTAL_PROCESSES-1) will send result packet to node 0, they must do so only after ensuring that node 0 has its YAL_Listener completed (since it'd gobble the result packet otherwise). Hence, each node (other than 0) receives a LISTENER0_WINDUP_TAG from node 0, and proceeds to shut down its YAL_Listener only when count_THREAD_WINDUP_TAG_received hits the quantity (TOTAL_PROCESSES+1)*/

    case LISTENER0_WINDUP_TAG:		if (VERBOSE)
					{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
					printf("P%d: LISTENER0_WINDUP from P%d, #%d", process_index, message, count_THREAD_WINDUP_TAG_received+1);
					fflush(stdout);}
					count_THREAD_WINDUP_TAG_received++;				
					if (count_THREAD_WINDUP_TAG_received == TOTAL_PROCESSES+ (process_index>0))
						LISTENER_ALIVE=FALSE;

					break;

    }	/*end of switch*/
    }

    ftime(&scrap_precise_time);
    listening_time=((scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm));

    } while(LISTENER_ALIVE);

    if(VERBOSE)
	{
	{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
	printf("Listener-%d out", process_index);
	fflush(stdout); }

    pthread_exit(&time_horizon);
}


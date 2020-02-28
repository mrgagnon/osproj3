#include <pthread.h>
#include <semaphore.h>
#include <time.h> 
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define MAXTHREAD 10
#define MAXBUFF 10
#define REQUEST 1
#define REPLY 2 
 

struct msg {
	int iFrom;	/* who sent the message ( 0 ... number-of-threads) */ 
	int value; 	/* its value */
	int cnt; 	/* count of operations (not need by all msgs) */
	int tot; 	/* total time (not needed by all msgs) */
};

struct msg **mailboxes; 
struct msg **mailBoxQueue;
sem_t **csems;
sem_t **psems;

/* Set up mailbox, create semaphores  
*/
void InitMailBox(sem_t **csems, sem_t **psems){  
	for (int i = 0; i <= MAXTHREAD; i++){
		csems[i] = (sem_t*)(malloc(sizeof(sem_t)));
		psems[i] = (sem_t*)(malloc(sizeof(sem_t)));
		sem_init(csems[i], 0, 0); 
		sem_init(psems[i], 0, 1); 
	}
}

/* Should block if another message is already in recipient's mail box 
 * iTo Mailbox to send to
 * pMsg Message to be sent  
 */
void SendMsg(int iTo, struct msg *pMsg) {
	//printf("In SendMsg\n");
	sem_wait(psems[iTo]);
		mailboxes[iTo] = pMsg; 
	sem_post(csems[iTo]);
}

/* Should block if no message available in the mailbox 
 * iRecv Mailbox to recieve from 
 * pMsg Message structure to fill in with recieved messaged 
 */
void RecvMsg(int irecv, struct msg *pMsg){
	//fprintf(stderr, "In RecvMsg\n");
	sem_wait(csems[irecv]);
		*pMsg = *mailboxes[irecv];
	sem_post(psems[irecv]);
}

int NBSendMsg(int iTo, struct msg *pmsg) {
	fprintf(stderr, "In NBSendMsg\n");
	if(sem_trywait(psems[iTo]) == 1) { // need to wait for semaphore 
		fprintf(stderr, "try wait = 1");
		return -1;
	}
	else {
		fprintf(stderr, "try wait = 0");
		mailboxes[iTo] = pmsg;
		sem_post(csems[iTo]);	
		return 0;
	}
}

int NBRecvMsg(int irecv, struct msg *pMsg) {
	fprintf(stderr, "In NBRecvMsg\n");
	if(sem_trywait(csems[irecv]) == 1) {
		fprintf(stderr, "try wait rec = 1");
		return -1;
	}
	else {
		fprintf(stderr, "try wait rec = 0");
		*pMsg = *mailboxes[irecv];
		sem_post(psems[irecv]);
		return 0;
	}
}

/* 
*/
void *adder(void *tid) {
	//printf("In adder\n");
	time_t startTime; 
	time_t endTime; 
	double totTime = 0;
	int totalVal = 0;
	int ctOps = 00;
	int mailboxNum = (int)tid;
	struct msg *msgR = (struct msg *)malloc(sizeof(struct msg));
	struct msg *msgEnd = (struct msg *)malloc(sizeof(struct msg));
	time(&startTime);
	while(1){
		RecvMsg(mailboxNum, msgR);
		int val = msgR->value;
		//(stderr, "value: %d", val);
		if(val >= 0){
			totalVal = totalVal+val;
			ctOps++;
			sleep(1);
		}
		else {
			break; // stop recieving messages 
		}	
	}
	//fprintf(stderr, "Stopped recieving messages\n");
	time(&endTime);
	totTime = endTime-startTime;
	//fprintf(stderr, "mailboxNum: %d totalVal: %d ctOps: %d totTime: %d\n", mailboxNum, totalVal, ctOps, totTime);

	msgEnd->iFrom = mailboxNum; 
	msgEnd->value = totalVal; 
	msgEnd->cnt = ctOps; 
	msgEnd->tot = totTime; 
	SendMsg(0, msgEnd);
}

int main(int argc, char* argv[]){
	//printf("In main\n");
	int nb; 
	int numThreads;
	
	if (argc > 2 && (strcmp(argv[2], "nb") == 0)){
		nb = 1; // in non-blocking mode 
		fprintf(stderr, "in nb\n");
	}
	else {
		nb = 0; // in blocking mode 	
		fprintf(stderr, "in blocking\n");
	}

	if ( argc >= 2 ) { // read in # of threads 
		int tempArg = atoi(argv[1]);
		if (tempArg > 0 && tempArg <= MAXTHREAD) {
			numThreads = tempArg;	
		}
		else {
			fprintf(stderr, "Too many threads, reenter\n");		
		}
	}
	//printf("numThreads:%d\n",numThreads);
 
	mailBoxQueue = (struct msg **)(malloc(sizeof(struct msg *)*(MAXTHREAD+1))); 

	//Mailbox create
	mailboxes = (struct msg **)(malloc(sizeof(struct msg *)*(MAXTHREAD+1))); 
	csems = (sem_t **)(malloc(sizeof(sem_t *)*MAXTHREAD+1)); 
	psems = (sem_t **)(malloc(sizeof(sem_t *)*MAXTHREAD+1)); 
	InitMailBox(csems, psems);

	//Thread create
	pthread_t ** threadIds= (pthread_t **)malloc(sizeof(pthread_t *)*MAXTHREAD); 
	pthread_t *tid;
	tid = (pthread_t*)malloc(sizeof(pthread_t *));	
	for (int i = 1; i <= numThreads; i++) {
		threadIds[i] = (pthread_t *)malloc(sizeof(pthread_t));
		*tid = i;		
		if (pthread_create(threadIds[i], NULL, adder, (void *)i) != 0){
			perror("pthread_create");
			exit(1);
		} 
	}

	char* inputLine; 
	char line[MAXBUFF]; 
	int value = 0;
	int sendTo = 0;
	struct msg *newMsg = (struct msg *)malloc(sizeof (struct msg));
	
	while(1) {
		inputLine = fgets(line, MAXBUFF, stdin);
		if (inputLine == NULL) {   
			printf("End of file\n");
			newMsg->iFrom = 0;
			newMsg->value = -1;
			newMsg->cnt = 0;
			newMsg->tot = 0;
			for (int i = 1; i <= numThreads; i ++) {
				SendMsg(i, newMsg);
			}
			break;		
		}
		if (line[0] == '\n'){
			fprintf(stderr, "Line empty, reenter\n");
			continue;
		}
		int value = 0;
		int sendTo = 0;
		sscanf(line, "%d %d", &value, &sendTo);
 
		fprintf(stderr, "value: %d sendTo: %d\n", value, sendTo);
	
		if (value < 0 || sendTo <= 0 || sendTo > numThreads ) {
			fprintf(stderr, "Invalid Input, reenter\n");
			continue;
		}
		
		newMsg->iFrom = 0;
		newMsg->value = value;
		newMsg->cnt = 0;
		newMsg->tot = 0;
		
		if(nb == 1) {
			if (NBSendMsg(sendTo, newMsg) == -1){ // message failed to send
				// need to add Msg to the queue 
			}
		}
		
		else { // blocking like normal
			SendMsg(sendTo, newMsg);
		}
	} // end while 

	while(1){
	// dequeue 
	NBSendMsg(sendTo, 
	}

		
	// Printing Results
	for (int i = 1; i <= numThreads; i++) {
		struct msg *msgRec = (struct msg *)malloc(sizeof (struct msg));
		RecvMsg(0, msgRec);
		fprintf(stderr, "The result from thread %d is %d from %d operations during %d secs.\n", 
				msgRec->iFrom, msgRec->value, msgRec->cnt, msgRec->tot);
	}

	// destory semaphores & free(csems) free(psems) 
	
	// Joining threads
	for (int i = 1; i <= numThreads; i ++) {
		(void)pthread_join(*(threadIds[i]), NULL);
	}
	return 0;
}

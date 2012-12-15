#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional or bidirectional
   data transfer protocols (from A to B. Bidirectional transfer of data
   is for extra credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
 **********************************************************************/

#define BIDIRECTIONAL 0    /* change to 1 if you're doing extra credit */
/* and write a routine called B_output */

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg {
	char data[20];
};

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
	int seqnum;
	int acknum;
	int checksum;
	char payload[20];
};

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

/********* Global variables for Selective Repeat ***********/
int snd_base, nextseqnum;
int snd_window_size = 10;
int snd_buff_size = 1000; /* Size of the buffer */
int head, tail; /* These are pointers to iterate the buffer circularly */
struct pkt *snd_buffer; /* This is the sender buffer that contains unsent packets */
int rcv_base; /* The base number in the receive window */
struct pkt *rcv_buffer; /* The receive buffer for packets that arrive out-of-order */
int rcv_window_size = 10;
float time_interval = 30;

/* These are the data structures to maintain if a sent
 * packet inside a given window is acknowledged or not.
 */
enum ack_status {no, yes};

struct ackdpkt {
	int seqnum;
	enum ack_status ackd;
};

struct ackdpkt *snd_window;

/* Counters for displaying the simulation results */
int number_1 = 0; /* Packets sent from A application layer */
int number_2 = 0; /* Packets sent from A transport layer */
int number_3 = 0; /* Packets received at B transport layer */
int number_4 = 0; /* Packets received at B application layer */

int timeout_count = 0;
/**********************************************************/
starttimer(int,float);
stoptimer(int);
tolayer3(int, struct pkt);
tolayer5(int, char*);
init();
generate_next_arrival();

/* called from layer 5, passed the data to be sent to other side */
A_output(message)
struct msg message;
{
	int i, temp_head;

	/* Increment counter for number of packets
	 * generated at the application layer.
	 */
	number_1++;
	message.data[20] = 0;

	printf("[Sender] Application data \"%s\" generated.\n", message.data);


	// Check if the queue is full and if yes, then exit program
	if (tail != -1 && (tail + 1) % snd_buff_size == head) {
		printf ("Sender buffer overflow. Exiting.\n");
		exit (0);
	}

	// If not, then prepare a packet and
	// buffer it for possible retransmission
	struct pkt packet;
	packet.seqnum = nextseqnum;
	packet.acknum = packet.seqnum;
	strncpy(packet.payload, message.data, 20);
	packet.checksum = packet.seqnum + packet.acknum;

	// Add up the payload data byte by byte into the checksum
	for (i = 0; i < 20; i++) {
		packet.checksum += message.data[i];
	}

	// Put the packet to the end of the buffer
	tail = (tail + 1) % snd_buff_size;
	snd_buffer[tail] = packet;

	// Send the packets that are already in buffer.
	// Be careful about going beyond the buffer. Hence
	// using a temp variable to prevent that.
	temp_head = (nextseqnum - snd_base + head) % snd_buff_size;
	while ((nextseqnum < snd_base + snd_window_size)
			&& temp_head != (tail + 1) % snd_buff_size) {

		// Add this packet as an unacknowledged
		// packet in the sender window buffer
		snd_window[(nextseqnum - 1) % snd_window_size].seqnum =
				snd_buffer[temp_head].seqnum;
		snd_window[(nextseqnum - 1) % snd_window_size].ackd = no;

		// Hand the packet to the network layer
		tolayer3(0,snd_buffer[temp_head]);

		printf("[Sender] Packet %d sent.\n", snd_buffer[temp_head].seqnum);


		/* Increment counter for number of
		 * packets sent from the transport layer.
		 */
		number_2++;

		// Start timer if this is the first packet in the window
		if (snd_base == nextseqnum) {
			starttimer(0, time_interval);
		}

		nextseqnum++;
		temp_head = (temp_head + 1) % snd_buff_size;
	}
}

B_output(message)  /* need be completed only for extra credit */
struct msg message;
{

}

/* called from layer 3, when a packet arrives for layer 4 */
A_input(packet)
struct pkt packet;
{
	int checksum, i;

	// Build the packet checksum to verify packet corruption
	checksum = packet.seqnum + packet.acknum;

	for (i = 0; i < 20; i++) {
		checksum += packet.payload[i];
	}

	// Compare the header checksum and the computed checksum
	if (checksum != packet.checksum) {

		printf("[Sender] Corrupt ACK received.\n");

		return;
	}

	/* Checksum test passed and packet is not corrupted.
	 * Now check if the ack is for the base packet. If yes,
	 * then move the base ahead to the smallest unackd packet
	 */

	printf("[Sender] ACK %d received.\n", packet.acknum);


	if (packet.acknum == snd_base) {
		timeout_count = 0;
		i = snd_base + 1;
		head = (head + 1) % snd_buff_size;

		while (snd_window[(i - 1) % snd_window_size].seqnum != -100
				&& snd_window[(i - 1) % snd_window_size].ackd != no) {
			snd_window[(i - 1) % snd_window_size].seqnum = -100;
			snd_window[(i - 1) % snd_window_size].ackd = no;
			i++;
			head = (head + 1) % snd_buff_size;
		}

		snd_base = i;

		if ((tail + 1) % snd_buff_size == head) {
			/* This essentially means the buffer is now empty */
			/* To work around the false positive "if" checking in
			 * A_output, lets reset head and tail here */
			head = 0;
			tail = -1;
		}

		if (snd_base == nextseqnum) {
			stoptimer(0);
		}
		else {
			starttimer(0, time_interval);
		}
	}

	/* If the ack is not for the base packet, then we
	 * need to mark the packet in the window as ackd.
	 */
	else if (packet.acknum > snd_base &&
			packet.acknum < nextseqnum) {
		snd_window[(packet.acknum - 1) % snd_window_size].seqnum = packet.acknum;
		snd_window[(packet.acknum - 1) % snd_window_size].ackd = yes;
	}
}

/* called when A's timer goes off */
A_timerinterrupt()
{
	int i, temp_head;

	/* Send only the base packet and start the timer */

	printf("[Sender] Packet %d timeout. Re-sending.\n",
			snd_buffer[head].seqnum);


	/* Increment counter for number of
	 * packets sent from the transport layer.
	 */
	number_2++;

	tolayer3(0, snd_buffer[head]);
	starttimer(0, time_interval);

	timeout_count++;
	temp_head = (head + 1) % snd_buff_size;

	for (i = 1; i < timeout_count && i < snd_window_size; i++) {

		if ((tail + 1) % snd_buff_size == temp_head) {
			break;
		}

		if (snd_window[(snd_base + i - 1) % snd_window_size].seqnum != -100 &&
				snd_window[(snd_base + i - 1) % snd_window_size].ackd == no) {

			printf("[Sender] Packet %d timeout. Re-sending.\n",
					snd_buffer[temp_head].seqnum);

			tolayer3(0, snd_buffer[temp_head]);

			/* Increment counter for number of
			 * packets sent from the transport layer.
			 */
			number_2++;
		}

		temp_head = (temp_head + 1) % snd_buff_size;
	}
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
A_init()
{
	int i;

	// Initialize all the global variables
	snd_base = nextseqnum = 1;
	snd_buffer = (struct pkt *) malloc (sizeof (struct pkt) * snd_buff_size);
	snd_window = (struct ackdpkt *) malloc (sizeof (struct ackdpkt) * snd_window_size);
	head = 0;
	tail = -1;

	/*
	 * Initialize the snd_window to invalid values
	 * since we have no packets in the window yet.
	 * -100 is the check for empty slots.
	 */

	for (i = 0; i < snd_window_size; i++) {
		snd_window[i].seqnum = -100;
		snd_window[i].ackd = no;
	}
}


/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
B_input(packet)
struct pkt packet;
{
	int checksum, i, count;
	struct pkt sndpkt;
	int index;

	/* Increment counter for number of packets
	 * received at the transport layer.
	 */
	number_3++;

	// Build the packet checksum to verify packet corruption
	checksum = packet.seqnum + packet.acknum;

	for (i = 0; i < 20; i++) {
		checksum += packet.payload[i];
	}

	// Compare the header checksum and the computed checksum
	if (checksum != packet.checksum) {
		// Packet is corrupted. Drop and do nothing

		printf("[Receiver] Corrupt packet received.\n");

		return;
	}

	// Packet is not corrupted. Determine if this is
	// an in-order packet. If yes, deliver the data.
	// If not, buffer it.

	// This if condition is for in-order packet check


	if (packet.seqnum == rcv_base) {

		// Deliver this packet to layer5

		printf("[Receiver] In-order packet %d received.\n", packet.seqnum);

		packet.payload[20] = 0;
		tolayer5(1, packet.payload);

		printf("[Receiver] Data \"%s\" handed over to application layer.\n",
				packet.payload);

		/* Increment counter for number of packets
		 * delivered to the application layer.
		 */
		number_4++;

		// Now deliver all the in-order buffered packets
		// Counter to count the number of buffered packets delivered
		count = 1;
		i = rcv_base + 1;

		while (rcv_buffer[(i - 1) % rcv_window_size].seqnum != -100) {



			printf("[Receiver] Buffered packet %d delivered to layer 5.\n",
					rcv_buffer[(i - 1) % rcv_window_size].seqnum);



			//rcv_buffer[(i - 1) % rcv_window_size].payload[20] = 0;
			tolayer5(1, rcv_buffer[(i - 1) % rcv_window_size].payload);

			/* Increment counter for number of packets
			 * delivered to the application layer.
			 */
			number_4++;


			printf("[Receiver] Data \"%s\" handed over to application layer.\n",
					rcv_buffer[(i - 1) % rcv_window_size].payload);



			count++;
			rcv_buffer[(i - 1) % rcv_window_size].seqnum = -100;
			i++;
		}

		// Increment rcv_base by the number of delivered packets
		rcv_base += count;
	}

	/*
	 * Now this is the check for an out-of-order packet
	 * that still falls inside the receive window. We need
	 * to buffer this packet if it is not already buffered.
	 */
	else if ((packet.seqnum > rcv_base)
			&& (packet.seqnum < rcv_base + rcv_window_size)) {

		printf("[Receiver] Out-of-order packet %d received. "
				"Will be buffered.\n", packet.seqnum);

		index = (packet.seqnum - 1) % rcv_window_size;

		/* If this if-condition holds, then we have a new
		 * out-of-order packet that needs to be buffered.
		 * Else, we already have this packet buffered.
		 */
		if (rcv_buffer[index].seqnum != packet.seqnum) {
			// Buffer the packet
			rcv_buffer[index] = packet;
		}
	}

	/*
	 * This is the check for a retransmitted packet
	 * that falls inside a window that is one window
	 * behind the current receive window. This usually
	 * happens when ACKs are corrupted or lost. Just
	 * ACK this packet and drop it.
	 */
	else if (packet.seqnum >= rcv_base - rcv_window_size
			&& packet.seqnum < rcv_base) {

		/* Do nothing. Even though the body is empty
		 * we retain this else-if for the sake of the
		 * else below this.
		 */
	}

	/*
	 * This packet is an invalid. It is neither inside
	 * the current window, nor inside the previous. Drop
	 * this packet and return
	 */
	else {
		return;
	}

	/* When we come to this line, it means we have received
	 * a packet, new or duplicate. We need to acknowledge this
	 * packet. Send an ACK.
	 */
	sndpkt.acknum = packet.seqnum;
	sndpkt.seqnum = sndpkt.acknum;
	strncpy(sndpkt.payload, packet.payload, 20);
	sndpkt.checksum = sndpkt.seqnum + sndpkt.acknum;

	// Add up the payload data byte by byte into the checksum
	for (i = 0; i < 20; i++) {
		sndpkt.checksum += packet.payload[i];
	}

	// Hand the packet to the network layer
	printf("[Receiver] ACK %d sent.\n", sndpkt.acknum);

	tolayer3(1, sndpkt);
}

/* called when B's timer goes off */
B_timerinterrupt()
{
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
B_init()
{
	int i;

	rcv_base = 1;
	rcv_buffer = (struct pkt *) malloc (sizeof (struct pkt) * rcv_window_size);

	/*
	 * Initialize the rcv_buffer to invalid values
	 * since we have no packets buffered yet. -100
	 * is the check for empty slots.
	 */

	for (i = 0; i < rcv_window_size; i++) {
		rcv_buffer[i].seqnum = -100;
	}
}


/*****************************************************************
 ***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
 ******************************************************************/

struct event {
	float evtime;           /* event time */
	int evtype;             /* event type code */
	int eventity;           /* entity where event occurs */
	struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
	struct event *prev;
	struct event *next;
};
struct event *evlist = NULL;   /* the event list */

/* possible events: */
#define  TIMER_INTERRUPT 0
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1
#define   A    0
#define   B    1



int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */
int nsimmax = 0;           /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/

main()
{
	struct event *eventptr;
	struct msg  msg2give;
	struct pkt  pkt2give;

	int i,j;
	char c;

	init();
	A_init();
	B_init();

	while (1) {
		eventptr = evlist;            /* get next event to simulate */
		if (eventptr==NULL)
			goto terminate;
		evlist = evlist->next;        /* remove this event from event list */
		if (evlist!=NULL)
			evlist->prev=NULL;
		if (TRACE>=2) {
			printf("\nEVENT time: %f,",eventptr->evtime);
			printf("  type: %d",eventptr->evtype);
			if (eventptr->evtype==0)
				printf(", timerinterrupt  ");
			else if (eventptr->evtype==1)
				printf(", fromlayer5 ");
			else
				printf(", fromlayer3 ");
			printf(" entity: %d\n",eventptr->eventity);
		}
		time = eventptr->evtime;        /* update time to next event time */
		if (nsim==nsimmax)
			break;                        /* all done with simulation */
		if (eventptr->evtype == FROM_LAYER5 ) {
			generate_next_arrival();   /* set up future arrival */
			/* fill in msg to give with string of same letter */
			j = nsim % 26;
			for (i=0; i<20; i++)
				msg2give.data[i] = 97 + j;
			if (TRACE>2) {
				printf("          MAINLOOP: data given to student: ");
				for (i=0; i<20; i++)
					printf("%c", msg2give.data[i]);
				printf("\n");
			}
			nsim++;
			if (eventptr->eventity == A)
				A_output(msg2give);
			else
				B_output(msg2give);
		}
		else if (eventptr->evtype ==  FROM_LAYER3) {
			pkt2give.seqnum = eventptr->pktptr->seqnum;
			pkt2give.acknum = eventptr->pktptr->acknum;
			pkt2give.checksum = eventptr->pktptr->checksum;
			for (i=0; i<20; i++)
				pkt2give.payload[i] = eventptr->pktptr->payload[i];
			if (eventptr->eventity ==A)      /* deliver packet by calling */
				A_input(pkt2give);            /* appropriate entity */
			else
				B_input(pkt2give);
			free(eventptr->pktptr);          /* free the memory for packet */
		}
		else if (eventptr->evtype ==  TIMER_INTERRUPT) {
			if (eventptr->eventity == A)
				A_timerinterrupt();
			else
				B_timerinterrupt();
		}
		else  {
			printf("INTERNAL PANIC: unknown event type \n");
		}
		free(eventptr);
	}

	terminate:
	printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n",time,nsim);
	/*****************************************************************************************/
	printf("\nProtocol: [Selective Repeat Protocol]\n");
	printf("[%d] packets sent from the Application Layer of Sender A\n", number_1);
	printf("[%d] packets sent from the Transport Layer of Sender A\n", number_2);
	printf("[%d] packets received at the Transport layer of receiver B\n", number_3);
	printf("[%d] packets received at the Application layer of receiver B\n", number_4);
	printf("Total time: [%f] time units\n", time);
	printf("Throughput = [%f] packets/time units\n\n", number_4 / time);
	/*****************************************************************************************/
}



init()                         /* initialize the simulator */
{
	int i;
	float sum, avg;
	float jimsrand();


	printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
	printf("Enter the number of messages to simulate: ");
	scanf("%d",&nsimmax);
	printf("Enter  packet loss probability [enter 0.0 for no loss]:");
	scanf("%f",&lossprob);
	printf("Enter packet corruption probability [0.0 for no corruption]:");
	scanf("%f",&corruptprob);
	printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
	scanf("%f",&lambda);
	printf("Enter TRACE:");
	scanf("%d",&TRACE);

	srand(9999);              /* init random number generator */
	sum = 0.0;                /* test random number generator for students */
	for (i=0; i<1000; i++)
		sum=sum+jimsrand();    /* jimsrand() should be uniform in [0,1] */
	avg = sum/1000.0;
	if (avg < 0.25 || avg > 0.75) {
		printf("It is likely that random number generation on your machine\n" );
		printf("is different from what this emulator expects.  Please take\n");
		printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
		exit(0);
	}

	ntolayer3 = 0;
	nlost = 0;
	ncorrupt = 0;

	time=0.0;                    /* initialize time to 0.0 */
	generate_next_arrival();     /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand()
{
	double mmm = 2147483647;   /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
	float x;                   /* individual students may need to change mmm */
	x = rand()/mmm;            /* x should be uniform in [0,1] */
	return(x);
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/

generate_next_arrival()
{
	double x,log(),ceil();
	struct event *evptr;
	//	char *malloc();
	float ttime;
	int tempint;

	if (TRACE>2)
		printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

	x = lambda*jimsrand()*2;  /* x is uniform on [0,2*lambda] */
	/* having mean of lambda        */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtime =  time + x;
	evptr->evtype =  FROM_LAYER5;
	if (BIDIRECTIONAL && (jimsrand()>0.5) )
		evptr->eventity = B;
	else
		evptr->eventity = A;
	insertevent(evptr);
}


insertevent(p)
struct event *p;
{
	struct event *q,*qold;

	if (TRACE>2) {
		printf("            INSERTEVENT: time is %lf\n",time);
		printf("            INSERTEVENT: future time will be %lf\n",p->evtime);
	}
	q = evlist;     /* q points to header of list in which p struct inserted */
	if (q==NULL) {   /* list is empty */
		evlist=p;
		p->next=NULL;
		p->prev=NULL;
	}
	else {
		for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next)
			qold=q;
		if (q==NULL) {   /* end of list */
			qold->next = p;
			p->prev = qold;
			p->next = NULL;
		}
		else if (q==evlist) { /* front of list */
			p->next=evlist;
			p->prev=NULL;
			p->next->prev=p;
			evlist = p;
		}
		else {     /* middle of list */
			p->next=q;
			p->prev=q->prev;
			q->prev->next=p;
			q->prev=p;
		}
	}
}

printevlist()
{
	struct event *q;
	int i;
	printf("--------------\nEvent List Follows:\n");
	for(q = evlist; q!=NULL; q=q->next) {
		printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
	}
	printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
stoptimer(AorB)
int AorB;  /* A or B is trying to stop timer */
{
	struct event *q,*qold;

	if (TRACE>2)
		printf("          STOP TIMER: stopping timer at %f\n",time);
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
	for (q=evlist; q!=NULL ; q = q->next)
		if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) {
			/* remove this event */
			if (q->next==NULL && q->prev==NULL)
				evlist=NULL;         /* remove first and only event on list */
			else if (q->next==NULL) /* end of list - there is one in front */
				q->prev->next = NULL;
			else if (q==evlist) { /* front of list - there must be event after */
				q->next->prev=NULL;
				evlist = q->next;
			}
			else {     /* middle of list */
				q->next->prev = q->prev;
				q->prev->next =  q->next;
			}
			free(q);
			return;
		}
	printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


starttimer(AorB,increment)
int AorB;  /* A or B is trying to stop timer */
float increment;
{

	struct event *q;
	struct event *evptr;
	//	char *malloc();

	if (TRACE>2)
		printf("          START TIMER: starting timer at %f\n",time);
	/* be nice: check to see if timer is already started, if so, then  warn */
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
	for (q=evlist; q!=NULL ; q = q->next)
		if ( (q->evtype==TIMER_INTERRUPT  && q->eventity==AorB) ) {
			printf("Warning: attempt to start a timer that is already started\n");
			return;
		}

	/* create future event for when timer goes off */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtime =  time + increment;
	evptr->evtype =  TIMER_INTERRUPT;
	evptr->eventity = AorB;
	insertevent(evptr);
}


/************************** TOLAYER3 ***************/
tolayer3(AorB,packet)
int AorB;  /* A or B is trying to stop timer */
struct pkt packet;
{
	struct pkt *mypktptr;
	struct event *evptr,*q;
	//	char *malloc();
	float lastime, x, jimsrand();
	int i;


	ntolayer3++;

	/* simulate losses: */
	if (jimsrand() < lossprob)  {
		nlost++;
		if (TRACE>0)
			printf("          TOLAYER3: packet being lost\n");
		return;
	}

	/* make a copy of the packet student just gave me since he/she may decide */
	/* to do something with the packet after we return back to him/her */
	mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
	mypktptr->seqnum = packet.seqnum;
	mypktptr->acknum = packet.acknum;
	mypktptr->checksum = packet.checksum;
	for (i=0; i<20; i++)
		mypktptr->payload[i] = packet.payload[i];
	if (TRACE>2)  {
		printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
				mypktptr->acknum,  mypktptr->checksum);
		for (i=0; i<20; i++)
			printf("%c",mypktptr->payload[i]);
		printf("\n");
	}

	/* create future event for arrival of packet at the other side */
	evptr = (struct event *)malloc(sizeof(struct event));
	evptr->evtype =  FROM_LAYER3;   /* packet will pop out from layer3 */
	evptr->eventity = (AorB+1) % 2; /* event occurs at other entity */
	evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
	/* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
	lastime = time;
	/* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
	for (q=evlist; q!=NULL ; q = q->next)
		if ( (q->evtype==FROM_LAYER3  && q->eventity==evptr->eventity) )
			lastime = q->evtime;
	evptr->evtime =  lastime + 1 + 9*jimsrand();



	/* simulate corruption: */
	if (jimsrand() < corruptprob)  {
		ncorrupt++;
		if ( (x = jimsrand()) < .75)
			mypktptr->payload[0]='Z';   /* corrupt payload */
		else if (x < .875)
			mypktptr->seqnum = 999999;
		else
			mypktptr->acknum = 999999;
		if (TRACE>0)
			printf("          TOLAYER3: packet being corrupted\n");
	}

	if (TRACE>2)
		printf("          TOLAYER3: scheduling arrival on other side\n");
	insertevent(evptr);
}

tolayer5(AorB,datasent)
int AorB;
char datasent[20];
{
	int i;
	if (TRACE>2) {
		printf("          TOLAYER5: data received: ");
		for (i=0; i<20; i++)
			printf("%c",datasent[i]);
		printf("\n");
	}

}

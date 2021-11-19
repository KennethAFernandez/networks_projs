#include "includes.h"

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

/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOULD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

simulator::simulator(long n, double l, double c, double t) {


    // ********************************************************************
    // * User configureable Variables.
    // ********************************************************************
    nsimmax = n;
    lossprob = l;
    corruptprob = c;
    lambda = t;


    // ***************************************************************************
    // * Internal variables.
    // ***************************************************************************
    evlist = nullptr;
    nsim = 0;
    kr_time = 0.000;
    ntolayer3 = 0;
    nlost = 0;
    ncorrupt = 0;
    kr_time = 0.0;
    srandom(time(nullptr));
    generate_next_arrival();


    // ***************************************************************************
    // * Basic Sanity Checks
    // ***************************************************************************
    if (nsimmax <= 0) {
        FATAL << "Can't have a simulation without at least 1 message." << std::endl;
        exit(-1);
    }
    if ((lossprob < 0) || (lossprob > 1)) {
        FATAL << "Invalid loss probability (" << lossprob << ")." << std::endl;
        exit(-1);
    }
    if ((corruptprob < 0) || (corruptprob > 1)) {
        FATAL << "Invalid corruption probability (" << corruptprob << ")." << std::endl;
        exit(-1);
    }
    if (lambda < 0) {
        FATAL << "Invalid average delay between messages from the application (" << lambda << ")." << std::endl;
        exit(-1);
    }


    INFO << "-----  Stop and Wait Network Simulator Version 1.1 -------- ";
    INFO << "Number of messages to simulate: " << nsimmax;
    INFO << "Packet loss probability [0.0 for no loss]: " << lossprob;
    INFO << "Packet corruption probability [0.0 for no corruption]: " << corruptprob;
    INFO << "Average time between messages from sender's layer5: " << lambda;

}


void simulator::go() {
    srand(time(nullptr));

    struct event *eventptr;
    while (eventptr = evlist) {


        //
        // Pop the next event off the list.
        //
        evlist = evlist->next;        /* remove this event from event list */
        if (evlist != nullptr)
            evlist->prev = nullptr;

        //
        // Jump the clock forward to the time the next event needs to happen.
        //
        kr_time = eventptr->evtime;

        //
        // Process the event.
        //
        if ((eventptr->evtype == FROM_LAYER5) && (nsim != nsimmax)) {

            // This adds the next FROM_LAYER5 event to the event list.
            generate_next_arrival();

            /* fill in msg to give with string of same letter */
            struct msg msg2give { };
            std::fill(msg2give.data, msg2give.data + sizeof(msg2give.data),(char)(97 + (nsim % 26)));
            DEBUG << "MAINLOOP (" << kr_time << "): Triggering "
                 << EVENT_NAMES[eventptr->evtype] << ", on side " << SIDE_NAMES[eventptr->eventity]
                 << ", " << msg2give;

            // Pass the message down to the student.
            if (eventptr->eventity == A) {
                if (rdt_sendA(msg2give)) { nsim++; }
            } else {
                if (rdt_sendB(msg2give)) { nsim++; }
            }
        }


        if (eventptr->evtype == FROM_LAYER3) {
            struct pkt pkt2give = {
                    .seqnum = eventptr->pktptr->seqnum,
                    .acknum = eventptr->pktptr->acknum,
                    .checksum = eventptr->pktptr->checksum,
                    .payload = {}
            };

            for (int i = 0; i < 20; i++)
                pkt2give.payload[i] = eventptr->pktptr->payload[i];

            DEBUG << "MAINLOOP (" << kr_time << "): Triggering "
                << EVENT_NAMES[eventptr->evtype] << ", on side " << SIDE_NAMES[eventptr->eventity]
                << ", " << pkt2give;
            if (eventptr->eventity == A)      /* deliver packet by calling */
                rdt_rcvA(pkt2give);            /* appropriate entity */
            else
                rdt_rcvB(pkt2give);

            free(eventptr->pktptr);          /* free the memory for packet */

        }

        if (eventptr->evtype == TIMER_INTERRUPT) {
            DEBUG << "MAINLOOP (" << kr_time << "): Triggering "
                 << EVENT_NAMES[eventptr->evtype] << ", on side " << SIDE_NAMES[eventptr->eventity];
            if (eventptr->eventity == A)
                A_timeout();
            else
                B_timeout();
        }

        free(eventptr);
    }

    INFO << "MAINLOOP (" << kr_time << "): Simulator terminated after sending " << nsim << " msgs from layer5.";
}




/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
double simulator::jimsrand() {
    return ((double) random() / (double) RAND_MAX);
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/
void simulator::generate_next_arrival() {

    auto *evptr = new  event();

    /* Delay will be uniform on [0,2*lambda] */
    evptr->evtime = kr_time + ( lambda * jimsrand() * 2);

    DEBUG << "GENERATE NEXT ARRIVAL (" << kr_time
        << "): scheduling next message from application to be given to layer 4 at " << evptr->evtime;

    evptr->evtype = FROM_LAYER5;
    if (BIDIRECTIONAL && (jimsrand() > 0.5))
        evptr->eventity = B;
    else
        evptr->eventity = A;
    insertevent(evptr);
}


void simulator::insertevent(struct event *p) {
    struct event *q, *qold;

    TRACE << "INSERTEVENT (" << kr_time << "): Inserting " << EVENT_NAMES[p->evtype]
        << " type event to happen at " << p->evtime;

    q = evlist;     /* q points to header of list in which p struct inserted */
    if (q == nullptr) {   /* list is empty */
        evlist = p;
        p->next = nullptr;
        p->prev = nullptr;
    } else {
        for (qold = q; q != nullptr && p->evtime > q->evtime; q = q->next)
            qold = q;
        if (q == nullptr) {   /* end of list */
            qold->next = p;
            p->prev = qold;
            p->next = nullptr;
        } else if (q == evlist) { /* front of list */
            p->next = evlist;
            p->prev = nullptr;
            p->next->prev = p;
            evlist = p;
        } else {     /* middle of list */
            p->next = q;
            p->prev = q->prev;
            q->prev->next = p;
            q->prev = p;
        }
    }
}

void simulator::printevlist() {
    struct event *q;
    printf("--------------\nEvent List Follows:\n");
    for (q = evlist; q != nullptr; q = q->next) {
        printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype, q->eventity);
    }
    printf("--------------\n");
}

void simulator:: reportPacketsInFlight(int AorB) {
    struct event *q;
    std::list<int> sequenceNumbers;

    for (q = evlist; q != nullptr; q = q->next) {
        if ((q->evtype == FROM_LAYER3) && (q->eventity == AorB)) {
            sequenceNumbers.push_back(q->pktptr->seqnum);
        }
    }
    std::cout << "TOLAYER3 (" << kr_time << "): "
        << sequenceNumbers.size() << " packets in flight to side " << SIDE_NAMES[AorB] << " (";
    for (auto sn : sequenceNumbers) {
        std::cout << sn << ", ";
    }
    std::cout << ")" << std::endl;
}


/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void simulator::stoptimer(int AorB) {

    DEBUG << "STOPTIMER (" << kr_time << "): stopping timer on side " << SIDE_NAMES[AorB];

    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (struct event *q = evlist; q != nullptr; q = q->next)
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
            /* remove this event */
            if (q->next == nullptr && q->prev == nullptr)
                evlist = nullptr;         /* remove first and only event on list */
            else if (q->next == nullptr) /* end of list - there is one in front */
                q->prev->next = nullptr;
            else if (q == evlist) { /* front of list - there must be event after */
                q->next->prev = nullptr;
                evlist = q->next;
            } else {     /* middle of list */
                q->next->prev = q->prev;
                q->prev->next = q->next;
            }
            TRACE << "STOPTIMER (" << kr_time << "): removing timer scheduled for " << q->evtime;
            free(q);
            return;
        }
    WARNING << "STOPTIMER (" << kr_time << "): WARNING: unable to cancel your timer. It wasn't running.";
}


void simulator::starttimer(int AorB, float increment) {
    struct event *q;
    struct event *evptr;

    DEBUG << "STARTTIMER (" << kr_time << "): starting timer to expire at " << kr_time + increment;

    /* be nice: check to see if timer is already started, if so, then  warn */
    /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
    for (q = evlist; q != nullptr; q = q->next)
        if ((q->evtype == TIMER_INTERRUPT) && (q->eventity == AorB)) {
            WARNING << "STARTTIMER (" << kr_time << "): WARNING: unable to start timer, there is one already running, "
            << "scheduled to go off at " << q->evtime ;
            return;
        }

    /* create future event for when timer goes off */
    evptr = (struct event *) malloc(sizeof(struct event));
    evptr->evtime = kr_time + increment;
    evptr->evtype = TIMER_INTERRUPT;
    evptr->eventity = AorB;
    insertevent(evptr);
}


/************************** TOLAYER3 ***************/
void simulator::tolayer3(int AorB, struct pkt packet) {
    struct pkt *mypktptr;
    struct event *evptr, *q;
    double lastime, x;

    ntolayer3++;

    /* simulate losses: */
    if (jimsrand() < lossprob) {
        nlost++;
        TRACE << "TOLAYER3: Loosing packet: " << packet;
        return;
    }

    /* make a copy of the packet student just gave me since he/she may decide */
    /* to do something with the packet after we return back to him/her */
    mypktptr = (struct pkt *) malloc(sizeof(struct pkt));
    mypktptr->seqnum = packet.seqnum;
    mypktptr->acknum = packet.acknum;
    mypktptr->checksum = packet.checksum;
    for (int i = 0; i < 20; i++)
        mypktptr->payload[i] = packet.payload[i];


    /* create future event for arrival of packet at the other side */
    evptr = (struct event *) malloc(sizeof(struct event));
    evptr->evtype = FROM_LAYER3;   /* packet will pop out from layer3 */
    evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
    evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */


    /* finally, compute the arrival time of packet at the other end.
     medium can not reorder, so make sure packet arrives between 1 and 10
     time units after the latest arrival time of packets
     currently in the medium on their way to the destination */
    lastime = kr_time;
    for (q = evlist; q != nullptr; q = q->next)
        if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity))
            lastime = q->evtime;
    evptr->evtime = lastime + 1 + 9 * jimsrand();


    /* simulate corruption: */
    if (jimsrand() < corruptprob) {
        ncorrupt++;
        if ((x = jimsrand()) < .75)
            std::fill(mypktptr->payload, mypktptr->payload + sizeof(mypktptr->payload), (rand()  % 93) + 33  );
        else if (x < .875)
            mypktptr->seqnum =  rand() ;
        else
            mypktptr->acknum = rand();
        TRACE << "TOLAYER3 (" << kr_time << ") Corrupting packet " << packet << " as " << *mypktptr;
    }


    DEBUG << "TOLAYER3 (" << kr_time << "): Scheduling " << packet
        << " to arrive on side " << SIDE_NAMES[(AorB + 1) % 2]
        << " at " << evptr->evtime << ".";
    insertevent(evptr);

    if ((BIDIRECTIONAL) || (AorB == A))
        reportPacketsInFlight((AorB + 1) % 2);
}


void simulator::tolayer5(int AorB, struct msg message) {

    DEBUG << "TOLAYER5 (" << kr_time << "): Data received at application layer on side " << SIDE_NAMES[AorB]
         << ", (" << message << ").";
}

double simulator::getSimulatorClock() {
    return(kr_time);
}

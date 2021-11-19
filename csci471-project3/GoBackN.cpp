#include "includes.h"
// ***************************************************************************
// * ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose
// *
// * These are the functions you need to fill in.
// ***************************************************************************

// ***************************************************************************
// * The following routine will be called once (only) before any other
// * entity A/B  routines are called. You can use it to do any initialization
// ***************************************************************************
//************************************
// * Checksum: seqnum + first 5 bytes
//************************************

// variables used - lot of them are now unnecessary
int base;
int seq_A;
int seq_B;
int buf_pos;
int snd_pos;
int clockDec;
int exp_seq_B;
int exp_ack_num;
int win_size = 10;
double rtt = 1000;
int buf[10000];

// Packet structures that are used when sending data
// from host A to host B
struct pkt ack;
struct pkt sndpkt[10];
struct pkt packet;

// initalize some variables to be used by Host A & Host B
void A_init() {
    base = 1;
    seq_A = 1;
}

void B_init() {
    exp_seq_B = 1;
    ack.seqnum = 0;
}

int checksum(struct pkt packet) {

    // set checksum values
    int checksum = 0;
    checksum += packet.seqnum;
    checksum += packet.acknum;

    // iterate through packet to calc. checksum
    for (int i = 0; i < 15; i++) { checksum += packet.payload[i]; }
    return checksum;
}

struct pkt make_packet(struct msg message) {

    // set packet seq./ack. nums.
    packet.seqnum = seq_A;
    packet.acknum = 0;
    // copy memory from msg to pkt
    strncpy(packet.payload, message.data, sizeof(packet.payload));

    // find & set checksum of pkt
    packet.checksum = checksum(packet);
    return packet;
}

int rdt_sendA(struct msg message) {

    // if window is not full
    if (seq_A < base + win_size ) { 

        // create packet
        struct pkt packet = make_packet(message);

        // send to simulation
        simulation->tolayer3(A, packet);

        INFO << "rdt_sendA(): sending " << &packet;
        sndpkt[seq_A % 10] = packet;

        // start timer 
        if (base == seq_A) {
            simulation->starttimer(A, rtt);
        }
        // increment vars.
        seq_A++;

    } else {

        // for some reason need this to have more than 11 packets
        return (0);
    } 

    // refuse data from simulator
    return 1;
}

void rdt_rcvB(struct pkt packet) {

    // if statement checks for any sort of corruption
    if (packet.checksum == checksum(packet) && packet.seqnum == exp_seq_B) {

        // create message object & copy packet payload into it
        // send it to layer 5
        struct msg message;
        memcpy(message.data, packet.payload, 20);
        simulation->tolayer5(B, message);

        // create ack & set values then send to layer 3
        ack.acknum = exp_seq_B;
        ack.seqnum = 0;
        ack.checksum = checksum(ack);
        simulation->tolayer3(B, ack);

        // increment expected seq. num.
        exp_seq_B++;
    } 

}

void rdt_rcvA(struct pkt packet) {
    
    // check that the packet is not corrupt
    if (packet.checksum == checksum(packet)) {

        // increment base by most recent ack num
        base = packet.acknum + 1;
        
        // start/stop timer
        if (base == seq_A) {
            simulation->stoptimer(A);
        } else {
            simulation->starttimer(A, rtt);
        }
    } else {

        // return if packet corrupt & wait for time out
        return;
    }
}

void A_timeout() {

    // start timer at junc. of timeout()
    simulation->starttimer(A, rtt);

    // iterate from last acked to curr. seq. num sending 
    // unacked packets
    for (int i = base; i < seq_A; i++) {
        simulation->tolayer3(A, sndpkt[i % 10]);
    }
}

// // ***************************************************************************
// // * Called from layer 5, passed the data to be sent to other side 
// // ***************************************************************************

// // ***************************************************************************
// // * Called from layer 3, when a packet arrives for layer 4 on side A
// // ***************************************************************************

// // ***************************************************************************
// // * Called from layer 5, passed the data to be sent to other side
// // ***************************************************************************

// // ***************************************************************************
// // // called from layer 3, when a packet arrives for layer 4 on side B 
// // ***************************************************************************

// // ***************************************************************************
// // * Called when A's timer goes off 
// // ***************************************************************************

// }
// // ***************************************************************************
// // * Called when B's timer goes off 
// // ***************************************************************************
void B_timeout() {
    INFO << "B_TIMEOUT: Side B's timer has gone off.";
}
int rdt_sendB(struct msg message) {
    INFO<< "RDT_SEND_B: Layer 4 on side B has received a message from the application that should be sent to side A: "
              << message;
    return 1; /* Return a 0 to refuse the message */
}


#include "worker.h"

unsigned long grx_saved_checksum = 0xff; 

void rxworker ( struct rxworker_s * rxworker ) {
int readsize;
int restartme=0; 
int readlen;  // XXX 
char * buffer;
char  okphrase[] ="ok";
char  checkphrase[] ="yoes";
buffer = calloc ( 1 , (size_t)kfootsize );

while ( !rxworker->rxconf_parent->done_mbox ) {
	restartme =0;
	assert ( rxworker->id < kthreadmax ); 
	do {
		pthread_mutex_lock  ( &rxworker->rxconf_parent->sa_mutex); 
		whisper ( 7, "rxw:%02i accepting and locked\n", rxworker->id); 
		rxworker->sockfd = tcp_accept ( &(rxworker->rxconf_parent->sa), rxworker->rxconf_parent->socknum); 
		whisper ( 5, "rxw:%02i accepted fd:%i \n", rxworker->id, rxworker->sockfd); 
		pthread_mutex_unlock( &rxworker->rxconf_parent->sa_mutex ); 
	} while (0);
	whisper ( 16, "rxw:%02i fd:%i expect yoes\n", rxworker->id, rxworker->sockfd); 
	read ( rxworker->sockfd , buffer, (size_t)  4 ); //XXX this is vulnerable to slow starts
	if ( bcmp ( buffer, checkphrase, 4 ) == 0 )  // XXX use programmamble checkphrase
		{ whisper ( 13, "rxw:%02d checkphrase ok\n", rxworker->id); } 
	else 
		{ whisper  (1,  "rxw:%02d checkphrase failure ", rxworker->id); 
		assert ( -1 && "checkphrase failure "); 
		exit ( -1); 
		}	

	checkperror ("checkphrase  nuiscance ");
	whisper ( 18, "rxw:%02i send ok\n", rxworker->id ); 
	if ( write (rxworker->sockfd, okphrase, (size_t) 2 ) != 2 )  {
		whisper( 1, "write fail " ); 
		assert ( -1  && "okwritefail");
		exit (-36); 
	}
	checkperror ( "signaturewrite "); 
	// /usr/src/contrib/netcat has a nice pipe input routine XXX perhaps lift it
	// XXX stop using bespoke heirloom bufferfill routines. 
	while ( !rxworker->rxconf_parent->done_mbox && (!restartme)) {
		struct millipacket_s pkt; 
		int cursor = 0;
		int preamble_cursor=0;
		int preamble_fuse=0; 
		unsigned long rx_checksum =0 ;
		while ( preamble_cursor < sizeof(struct millipacket_s) && (!restartme))   {
			//fill the preamble + millipacket structure whole
			checkperror ("nuicsance before preamble read");
			whisper ( ((errno == 0)?19:3) ,"rxw:%02i prepreamble and millipacket: errno:%i cursor:%i\n", rxworker->id, errno, preamble_cursor); 
			//XX regrettable pointer cast + arithmatic otherwise we get sizeof(pkt) * cursor
			readlen = read ( rxworker->sockfd , ((u_char*)&pkt)+preamble_cursor, ( sizeof(struct millipacket_s) - preamble_cursor )); 
			checkperror ("preamble read");
			whisper ( ((errno == 0)?19:3) ,"rxw:%02i preamble and millipacket: len %i errno:%i cursor:%i\n", rxworker->id, readlen,errno, preamble_cursor); 
			assert (( readlen >= 0 ) && " bad packet header read");
			if ( readlen == 0 ) { 	
				whisper ( 6, "rxw:%02i exits after empty preamble\n", rxworker->id); 
				pthread_exit( rxworker );  // no really we are done, and who wants our exit status?
				continue;
			}; 
			if ( readlen < (sizeof(struct millipacket_s) - preamble_cursor) ) { 
				whisper ( 7, "rx: short header %i(read)< %lu (headersize), preamble cursor: %i \n",
					 readlen, sizeof(struct millipacket_s),preamble_cursor); 
				checkperror ("short read");
			}
			preamble_cursor += readlen;
			assert ( preamble_cursor <= sizeof(struct millipacket_s));
			preamble_fuse ++ ;
			assert ( preamble_fuse < 100000 && " preamble fuse popped, check network ");
		}
		assert ( preamble_cursor == sizeof(struct millipacket_s) ); 
		assert ( pkt.preamble == preamble_cannon_ul   && "preamble check");
		assert ( pkt.size >= 0 ); 
		assert ( pkt.size <= kfootsize); 
		whisper ( 9, "rxw:%02i leg:%lu siz:%lu op:%x caught new leg  \n", rxworker->id,  pkt.leg_id , pkt.size,pkt.opcode); 
		int remainder = pkt.size; 
		int remainder_counter =0; 
		assert ( remainder <= kfootsize); 
		while ( remainder  && (!restartme) ) {
			readsize = read( rxworker->sockfd, buffer+cursor, MIN(remainder, MAXBSIZE )); 
			checkperror ( "rx: read failure\n"); 
			if ( errno != 0   || readsize == 0  ) {
				whisper (2 , "rxw:%02i retired due to read errno:%i\n",rxworker->id, errno); 
				restartme=1; 
			} 
			cursor += readsize; 
			assert( cursor <= kfootsize ); 
			remainder -= readsize ; 
			assert (remainder >=0);
			if (readsize == 0 && (!restartme) ) { 
				whisper ( 2, "rx: 0 byte read ;giving up. are we done?\n" );  // XXX this should not be the end
				break;
			}
			checkperror ("read buffer"); 	
		
			whisper( (errno != 0)?3:19 , "rx%02i %lu[%i]-[%i]\t%c", 
				rxworker->id, pkt.leg_id ,  
				readsize >> 10 ,remainder >>10,
				((remainder_counter++)%16 == 0 ) ? (int)10:(int)' ' ) ; 
		}
		whisper( 8, "\nrxw:%02i leg:%lu buffer filled to :%i\n", rxworker->id, pkt.leg_id,  cursor) ; 
		checkperror ("read leg"); 	
		/*block until the sequencer is ready to push this 
		 XXXX  suboptimal  sequencer ?? prove it!
		 perhaps a minheap??
		 bufferblock == next expected bufferblock
		*/
		int sequencer_stalls =0 ; 
		while( pkt.leg_id != rxworker->rxconf_parent->next_leg  && ( !restartme ) ) {
			pthread_mutex_unlock( &rxworker->rxconf_parent->rxmutex );
#define ktimeout ( 1000 * 13000 ) 
#define ktimeout_chunks ( 250000  )
			usleep ( ktimeout / ktimeout_chunks ); 
			sequencer_stalls++; 
			assert ( sequencer_stalls  < ktimeout_chunks && "rx seqencer stalled");
			pthread_mutex_lock( &rxworker->rxconf_parent->rxmutex ); // do nothing but compare seqeuncer under lock
		}
		pthread_mutex_unlock( &rxworker->rxconf_parent->rxmutex );
		if ( !restartme ) {
			whisper ( 5, "rxw:%02i sequenced leg:%08lu[%08lu]after %05i stalls\n", rxworker->id,  pkt.leg_id, pkt.size, sequencer_stalls); 
			remainder = pkt.size; 
			int writesize=0; 
			cursor = 0 ; 
			while (  remainder ) {
				writesize = write ( STDOUT_FILENO, buffer+cursor, (size_t) MIN( remainder, MAXBSIZE )); 
				cursor += writesize; 
				remainder -= writesize ; 
			}
			checkperror ("write buffer"); 	
			readlen = readsize = -111;
			if ( pkt.opcode == end_of_millipede ) {
				whisper ( 5, "rxw:%02i caught %x done with last frame\n", rxworker->id,  pkt.opcode); 
				rxworker->rxconf_parent->done_mbox = 1; //XXX xxx 
			} 
			else {
				// the last frame will be empty and have a borken cksum
				if ( pkt.checksum )	{

					rx_checksum = mix ( grx_saved_checksum + pkt.leg_id, buffer, pkt.size ); 
					if ( rx_checksum != pkt.checksum ) 
						{ 
						whisper( 2 , "rx checksum mismatch %lu != %lu", rx_checksum, pkt.checksum); 
						whisper ( 2, "rxw:%02i offending leg:%lu.%lu after %i stalls\n", rxworker->id,  pkt.leg_id, pkt.size, sequencer_stalls); 
						}
					assert ( rx_checksum == pkt.checksum ) ;
					grx_saved_checksum = rx_checksum; 
				};

			}
				pthread_mutex_lock( &rxworker->rxconf_parent->rxmutex );
			rxworker->rxconf_parent->next_leg ++ ; // do this last or race out the cksum code
			pthread_mutex_unlock( &rxworker->rxconf_parent->rxmutex );
		} // if not restartme 
	}// while !done_mbox
	whisper( 5, "rxw:%02i exiting work restartme block\n", rxworker->id);
} //restartme
whisper( 4, "rxw:%02i done\n", rxworker->id);
}

void rxlaunchworkers ( struct rxconf_s * rxconf ) {
	int worker_cursor=0; 
	int retcode ;
	rxconf->done_mbox = 0; 
	rxconf->next_leg = 0;  //initalize sequencer
	if (tcp_recieve_prep(&(rxconf->sa), &(rxconf->socknum),  rxconf->port ) != 0) {
		whisper ( 1, "rx: tcp prep failed. this is unfortunate. " ); 	
		assert( -1); 
	}  
	do  {
		rxconf->workers[worker_cursor].id = worker_cursor; 
		rxconf->workers[worker_cursor].rxconf_parent = rxconf; 
		// all the bunnies made to hassenfeffer 
		retcode = pthread_create ( 
			&rxconf->workers[worker_cursor].thread, 
			NULL, //attrs - none
			(void *(* _Nonnull)(void *))&rxworker,
                        &(rxconf->workers[worker_cursor])
		); 
		whisper ( 18, "rxw:%02i threadlaunched\n", worker_cursor); 
		checkperror ( "rxpthreadlaunch");  
		assert ( retcode == 0 && "pthreadlaunch");
		worker_cursor++; 
	} while ( worker_cursor < (kthreadmax ) ); 
	whisper( 7, "rx: worker group launched\n");
}
void rx (struct rxconf_s* rxconf) {
	pthread_mutex_init ( &(rxconf->sa_mutex), NULL ); 
	pthread_mutex_init ( &(rxconf->rxmutex ), NULL ); 
	rxconf->done_mbox = 0; 
	rxlaunchworkers( rxconf); 
	while ( ! rxconf->done_mbox ) {
		pthread_mutex_unlock ( &(rxconf->rxmutex) ); 
		usleep ( 10000); 
		//XXX  join or  hoist out the lame pthread_exits(); 
		pthread_mutex_lock ( &(rxconf->rxmutex) ); 
	}
	whisper ( 4,"rx: done\n");
}


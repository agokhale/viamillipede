#include "worker.h"

void rxworker ( struct rxworker_s * rxworker ) {
	int done =0; 
	int readsize;
	int readlen;  // XXX 
	char * buffer;
	char  okphrase[] ="ok";
	char  checkphrase[] ="yoes";
	buffer = calloc ( 1 , (size_t)kfootsize );
	whisper ( 8, "   rx worker  connected id%i fd:%i\n", rxworker->id, rxworker->sockfd); 
	read ( rxworker->sockfd , buffer, (size_t)  4 ); //XXX this is vulnerable to slow starts
	if ( bcmp ( buffer, checkphrase, 4 ) == 0 ) 
		{ whisper ( 8, "checkphrase ok\n"); } 
	else 
		{ assert (-1 && "checkphrase failure "); }	

	checkperror ("checkphrase  nuiscance ");
	assert ( write (rxworker->sockfd, okphrase, (size_t)  2 ) && "okwritefail");
	checkperror ( "signaturewrite "); 
	// /usr/src/contrib/netcat has a nice pipe input routine XXX perhaps lift it
	// XXX stop using bespoke heirloom bufferfill routines. 
	while ( ! done && !rxworker->rxconf_parent->done_mbox) {
		struct millipacket_s pkt; 
		int cursor = 0;
		int preamble_cursor=0;
		int preamble_fuse=0; 
		while ( preamble_cursor < sizeof(struct millipacket_s))   {
			// fill the preamble + millipacket structure whole
			checkperror ("nuicsance before preamble read");
			whisper ( ((errno == 0)?19:3) ,"rxw:%i prepreamble and millipacket: len %i errno:%i cursor:%i\n", rxworker->id, readlen,errno, preamble_cursor); 
			//XX regrettable pointer cast  other wise we get sizeof(pkt) * cursor
			readlen = read ( rxworker->sockfd , ((u_char*)&pkt)+preamble_cursor, ( sizeof(struct millipacket_s) - preamble_cursor )); 
			checkperror ("preamble read");
			whisper ( ((errno == 0)?19:3) ,"rxw:%i preamble and millipacket: len %i errno:%i cursor:%i\n", rxworker->id, readlen,errno, preamble_cursor); 
			assert (( readlen >= 0 )  && " badpackethreader read");
			if ( readlen == 0 ) { 	
				whisper ( 6, "rxw:%i  exits after empty preamble\n", rxworker->id); 
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
			assert ( preamble_fuse ++ < 100000 && " preamble fuse popped, check network ");
		}
		assert ( preamble_cursor == sizeof(struct millipacket_s) ); 
		assert ( pkt.preamble == preamble_cannon_ul   && "preamble check");
		assert ( pkt.size >= 0 ); 
		assert ( pkt.size <= kfootsize); 
		// it's ok for pkt.size == 0 for the end of the line 
		//assert ( pkt.size >  0);  
		whisper ( 9, "wrk:%i leg:%lu siz:%lu op:%x caught new leg  \n", rxworker->id,  pkt.leg_id , pkt.size,pkt.opcode); 
		int remainder = pkt.size; 
		assert ( remainder <= kfootsize); 

		while ( remainder && !done  ) {
			readsize = read( rxworker->sockfd, buffer+cursor, MIN(remainder, MAXBSIZE )); 
			assert( readsize > 0 );  //XXX 
			cursor += readsize; 
			assert( cursor <= kfootsize ); 
			remainder -= readsize ; 
			assert (remainder >=0);
			if (readsize == 0) { //XXXXXXXX 
				whisper ( 2, "0 byte read ;giving up. are we done?" );  // XXX this should not be the end
				done = 1; 	
				break;
			}
			checkperror ("read buffer"); 	
			whisper( (errno != 0)?3:9 , "rxw:%i leg:%lu siz:%i-%i\t", rxworker->id, pkt.leg_id ,  readsize >> 10 ,remainder >>10 ) ; 
		}
		whisper( 8, "\nrxw: %i leg:%lu buffer filled to :%i\n", rxworker->id, pkt.leg_id,  cursor) ; 
		checkperror ("read leg"); 	
		//assert ( errno != 0 && "read leg"); 
		// XXX crc32 check
		//block until the sequencer is ready to push this 
		///XXXX  terrible sequencer
		// bufferblock == next expected bufferblock
		// possibly voilates some cocurrency noise
		int sequencer_stalls =0 ; 
		while ( pkt.leg_id !=  rxworker->rxconf_parent->next_leg ) {
#define ktimeout ( 1000 * 3000 ) 
#define ktimeout_chunks ( 10000  )
			usleep ( ktimeout / ktimeout_chunks ); 
			sequencer_stalls++; 
			assert ( sequencer_stalls  < ktimeout_chunks && "rx seqencer stalled");
		}
		whisper ( 5, "rxw:%i dumping leg:%lu.%lu after %i stalls\n", rxworker->id,  pkt.leg_id, pkt.size, sequencer_stalls); 
		remainder = pkt.size; 
		int writesize=0; 
		cursor = 0 ; 
		while (  remainder ) {
			writesize = write ( STDOUT_FILENO, buffer+cursor, (size_t) MIN( remainder, MAXBSIZE )); 
			cursor += writesize; 
			remainder -= writesize ; 
		}
		checkperror ("write buffer"); 	
		//XXX protect with mutex?
		rxworker->rxconf_parent->next_leg ++ ;
		readlen = readsize = -111;
		if ( pkt.opcode == end_of_millipede ) {
			whisper ( 5, "rxw:%i caught %x done with last frame\n", rxworker->id,  pkt.opcode); 
			//rxworker->rxconf_parent->done_mbox = 1; 
			exit (0); 
		}
	}// while !done
	whisper ( 7, "rxw:%i  done\n", rxworker->id); 
	
}

void rxlaunchworkers ( struct rxconf_s * rxconf ) {
	int done =0; 
	int worker_cursor=0; 
	int retcode ;
	rxconf->done_mbox =0; 
	rxconf->next_leg =0;  //initalize sequencer
	assert (tcp_recieve_prep(&(rxconf->sa), &(rxconf->socknum),  rxconf->port ) == 0  && "prep");
	do  {
		rxconf->workers[worker_cursor].id = worker_cursor; 
		rxconf->workers[worker_cursor].rxconf_parent= rxconf; 
		whisper ( 8, "rxworker %i stalled ", worker_cursor); 
		rxconf->workers[worker_cursor].sockfd = tcp_accept ( &(rxconf->sa), rxconf->socknum); 
		whisper ( 5, "rxw:%i connected fd:%i\n", worker_cursor, rxconf->workers[worker_cursor].sockfd); 
		// all the bunnies made to hassenfeffer
		retcode = pthread_create ( 
			&rxconf->workers[worker_cursor].thread, 
			NULL, //attrs - none
			(void *(* _Nonnull)(void *))&rxworker,
                        &(rxconf->workers[worker_cursor])
		); 
		checkperror ( "rxpthreadlaunch");  assert ( retcode == 0 && "pthreadlaunch");
		done = rxconf->done_mbox; 
		worker_cursor++; 
	} while (! done ); 
	
}
void rx (struct rxconf_s* rxconf) {
	rxlaunchworkers( rxconf); 
}


#include "cfg.h"
#include "worker.h"

void usage ()  { 
printf ( "viamillipede scatter gather multiplexed tcp for pipe transport between hosts usage: \n"); 
printf ( "tranmitter:  vimillipede tx 192.168.0.2 12323  tx 192.168.0.3 12323 threads 3 verbose 3\n"); 
printf ( "receiver:  vimillipede rx 12323 3\n"); 
}
	
int gverbose = 0; 

int main ( int argc, char ** argv ) {

int arg_cursor = 0;
int mode =-1; 
int users_input_port; 
struct txconf_s txconf; 
struct rxconf_s rxconf; 

( argc > 1) ? : usage (); 
txconf.worker_count = 16;
txconf.target_port_count=0; 
while ( arg_cursor  < argc  ) {
	//printf ( "  arg: %d, %s\n", arg_cursor , argv[arg_cursor]);
	if ( strcmp (argv[arg_cursor], "rx" ) == 0 ) {
		/* rx <portnumber> */
		assert ( ++ arg_cursor < argc  && " rx requires a  <portnumber> from 0-SHRT_MAX" );
		users_input_port  = atoi ( argv[ arg_cursor ] ); 
		assert ( 0 < users_input_port  && users_input_port < USHRT_MAX && "port number should be 0-USHRT_MAX" );
		rxconf.port = (short) users_input_port;
		whisper (3," being a server at port %i \n\n ", rxconf.port); 
		mode = 0; //xxx enums
		}
	if ( strcmp ( argv[arg_cursor] , "tx" )  == 0 ) {
		//
		assert ( ++ arg_cursor < argc  && "tx needs <host> and <port> arguments");
		assert ( strlen ( argv[arg_cursor] ) > 0  && "hostname seems fishy" );
		txconf.target_ports[txconf.target_port_count].name =  argv[arg_cursor]; 
		// XXX NDEBUG wil break
		assert ( ++ arg_cursor < argc  && " tx  requires a  <portnumber> from 0-SHRT_MAX" );
		users_input_port  = atoi ( argv[ arg_cursor ] ); 
		assert ( 0 < users_input_port  && users_input_port < USHRT_MAX && "port number should be 0-USHRT_MAX" );
		txconf.target_ports[txconf.target_port_count].port = (short) users_input_port;
		whisper ( 2, "tx host: %s port:%i \n", 
			txconf.target_ports[txconf.target_port_count].name,
			txconf.target_ports[txconf.target_port_count].port); 
		txconf.target_port_count ++; 
		mode = 1;
	}
	if ( strcmp ( argv[arg_cursor] , "threads" )  == 0 ) {
		assert ( ++ arg_cursor < argc  && "threads needs <numeber> arguments");
		txconf.worker_count  = atoi ( argv[ arg_cursor ] ); 
		assert ( txconf.worker_count <= 16 && "it's unlikely that a large threadcount is beneficial");
		
	}
	if ( strcmp ( argv[arg_cursor] , "verbose" )  == 0 ) {
		/// XXXX NDEBUG will break
		assert ( ++ arg_cursor < argc  && "verbose  needs <level ( 0 - 19) > argument");
		gverbose  = atoi ( argv[arg_cursor]);
		whisper ( 11, "verbose set to %i", gverbose ); 	
	}
	arg_cursor ++;
	}

switch ( mode ) {
	case 1: tx (&txconf); break; 
	case 0: rx (&rxconf); break;
	default: assert ( -1 && " mode incorrect, internal error"); break; 
	}
exit (0); 
}

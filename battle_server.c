// battle_server.c 

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define CMD_ID_SIZE 4

struct client_t {
	char* username;
	int portUDP;
	struct sockaddr_in addr;
	int fd;
	int ingame;
	struct client_t* next;
};

char* ip_addr = "127.0.0.1";
const char* disponibile = "( disponibile )";
const char* non_disponibile = "( non disponibile )";
struct client_t* clients = NULL;
int n_clients = 0;
int end = 0;
int listener;
char* buff_pointer;
char* size_buff;
char* cmd_buffer;
char* printed_clients;
fd_set master;
fd_set read_fds;

void get_info_client( int fd, char* username, int& port, char* ip ) {
	struct client_t* tmp;
	for( tmp = clients; tmp->next != NULL; tmp = tmp->next ) {	
		if ( tmp->fd  == fd ) {
			strcpy( username, tmp->username );
			port = tmp->portUDP;
			inet_ntop( AF_INET, &(tmp->sin_addr), ip, INET_ADDRSTRLEN);	
		}
	}
}

int get_fd( char* username ) {
	struct client_t* tmp;
	for( tmp = clients; tmp->next != NULL; tmp = tmp->next ) {	
		if ( strcmp( tmp->username, username ) == 0 ) {
			printf( "[DEBUG] fd client sfidato trovato %d \n", tmp->fd);
			return tmp->fd;
		}
	}
	return -1;
}

void catch_stop( int sig_num ) {
	signal( SIGINT, catch_stop );
	printf( " *** SERVER INTERROTTO ***\n" );
	// for each client send close
	close(listener);
	FD_CLR( listener, &master );
	end = 1;

}

int extract_int( int offset ) {
    int tmp; 
	void* pointer = (void*) &tmp;
	memcpy( pointer, cmd_buffer + offset, sizeof(int) );
    printf( "[DEBUG] intero estratto %d\n", tmp );
    return tmp;
}

int check_user_presence( char* username ) {
	struct client_t* tmp;
	for( tmp = clients; tmp->next != NULL; tmp = tmp->next ) {	
		if ( strcmp( tmp->username, username ) == 0 ) {
			printf( "[DEBUG] utente gia' registato \n");
			return -1;
		}
	}
	return 0;
}

char* print_clients() {
    struct client_t* i;
    printf( "[DEBUG] print %d clients \n", n_clients);
	int string_size = 0;
	for ( i = clients; i->next != NULL; i = i-> next ) {
		string_size += strlen( i->username ) + 10; // 
		if ( i->ingame == 1 )
			string_size += strlen( non_disponibile );
		else
			string_size += strlen( disponibile );
	}
	printed_clients = malloc(string_size);
	for ( i = clients; i->next != NULL; i = i->next ) {
		strcat( printed_clients, "\n ");
       	strcat( printed_clients, i->username );
		strcat( printed_clients, " ");
        if ( i->ingame ) 
            strcat(printed_clients, non_disponibile );
        else 
            strcat( printed_clients, disponibile );
    }
	return printed_clients;
}

void insert_buff( void* src, int n ) {
	memcpy( buff_pointer, src, n );
	buff_pointer += n;
}

int set_pkt( int* response_id, char* string_msg, int* portUDP, char* ip_addr ) {
	int ret = sizeof(int);
	int string_size = 0;
	
	if ( string_msg != NULL ) {
		string_size = strlen(string_msg) + 1;
		ret += string_size + sizeof(int);
	}
	if ( portUDP != NULL )
		ret += sizeof(int);

	if ( ip_addr != NULL )
		ret += 
	cmd_buffer = malloc(ret);
	buff_pointer = cmd_buffer;
	insert_buff( response_id, sizeof(int));
	if ( string_msg != NULL ) {
		insert_buff( &string_size, sizeof(int));
		insert_buff( string_msg, string_size );
	}
	return ret;
}

void send_response( int client_fd, int* size ) {
	int ret;
	ret = send( client_fd, (void*) size, sizeof(int), 0 ); //invio dimensione pacchetto
	if ( ret < sizeof(int) ) {
		printf("[ERRORE] Invio dimensione comando \n");
		return;
	}
	int s = *(size);
	printf("[DEBUG] bytes da inviare %d \n", s);
	ret = send( client_fd, (void*) cmd_buffer, s, 0 );
	if ( ret < *(size) ) {
		printf( "[ERRORE] Invio comando \n" );
		return;
	}
	printf( "[DEBUG] bytes inviati %d \n", ret );

}

int recv_cmd(int fd) {
	int ret, cmd_size;
	size_buff = (char*) &cmd_size;
	ret = recv( fd, size_buff, sizeof(cmd_size), 0 );
	if ( ret == 0 ) {
		printf( "[INFO] Disconessione del client %d \n", fd );
		//rimuovi dalla lista 
		close(fd);
		FD_CLR( fd, &master );
		return -1;
	} 
	if ( ret < 0 ) {
		printf( "[ERRORE] Dimensione comando \n");
		return -1;
	}
	//free( cmd_buffer );
	cmd_buffer = malloc( cmd_size );
    printf("[DEBUG] bytes attesi %d \n", cmd_size );
	ret = recv( fd, cmd_buffer, cmd_size, 0);
    if ( ret < 0 ) {
		printf("[ERRORE] Bytes ricevuti %d insufficienti per il comando scelto\n", ret);
		return -1;
	}
    return 0;
}	


int main( int argc, char** argv ) {
	if ( argc != 2 ) {
		printf( "[ERRORE] Uso: ./battle_server.exe <porta> \n");
		exit(1);
	}

	int username_size, i, fdmax, port, ret, response_id, cmd_id, challenged_user_fd, challenging_user_fd;
	struct sockaddr_in server_addr;
    socklen_t addrlen;
    struct client_t* client_i;
	char* username;
	char ip[INET_ADDRSTRLEN];

    clients = (struct client_t*) malloc( sizeof( struct client_t));
   
    FD_ZERO(&master);
	FD_ZERO(&read_fds);

	port = atoi(argv[1]);
	printf( "[DEBUG] porta %d\n", port );
	listener = socket(AF_INET, SOCK_STREAM, 0);
	
	memset( &server_addr, 0 ,sizeof( server_addr ) );

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons( port );
	inet_pton( AF_INET, ip_addr, &server_addr.sin_addr);

	ret = bind( listener, (struct sockaddr*) &server_addr, sizeof(server_addr));
	if ( ret == -1 ) {
		perror( "[ERRORE] Binding \n" );
		exit(1);
	} 
	ret = listen(listener, 10 );
	if( ret == -1  ) {
		perror("[ERRORE] Listening \n");
		exit(1);
	}
	printf( "[INFO] Server con indirizzo %s in ascolto sulla porta %d \n", ip_addr, port );   
	FD_SET(listener, &master);

	fdmax = listener;
	signal( SIGINT, catch_stop );	
	for (;;) {
		read_fds = master;
		select( fdmax + 1, &read_fds, NULL, NULL, NULL );
		for ( i = 0; i <= fdmax; i++ ) {
			if ( FD_ISSET( i, &read_fds ) ) {
				if ( i == listener ) {
					if ( end == 1 )
						return 0;
					printf( "[DEBUG] listener \n" );
					struct client_t* new_client = malloc( sizeof(struct client_t));
					if ( new_client == NULL ) 
						printf( "[ERRORE] Memoria insufficiente, impossibile gestire un nuovo cliente \n" );
					addrlen = sizeof(struct sockaddr_in);
					new_client->fd = accept( listener, ( struct sockaddr * ) &new_client->addr, &addrlen );
					new_client->username = "-";
					new_client->portUDP = 0;
					new_client->ingame = 0;
					new_client->next = clients;
   					clients = new_client;
 					n_clients++;				
				
					FD_SET( new_client->fd, &master);
					if ( new_client->fd > fdmax )
						fdmax = new_client->fd;
					printf( "[INFO] Nuovo utente registrato \n");
                    printf( "[DEBUG] Nuovo fd %d, fdmax %d \n", new_client->fd, fdmax );
				}
                if ( clients != NULL ) {
                    for ( client_i = clients; client_i->next != NULL; client_i = client_i->next ) {
						if ( i == client_i->fd ) {
							printf( "[DEBUG] fd %d pronto \n", i );
                         	fflush(stdout);
							ret = recv_cmd(client_i->fd);				
							if ( ret == -1 )
                                break;

                            cmd_id = extract_int(0);					
							if ( strlen( cmd_buffer ) > 4 ) {
									username_size = extract_int(4);
									username = malloc( username_size );
									memcpy( username, cmd_buffer + 8, username_size);
							}		
							switch( cmd_id ) {
								case -1: // l'utente attuale a rifiutato la sfida
									response_id = -1;
									ret = set_pkt( &response_id, username );
									challenging_user_fd = get_fd( username );
									send_response( challenging_user_fd, &ret );
									break;
								case 0: // ricezione username e porta UDP 
									client_i->portUDP = extract_int(8 + username_size );
									printf("[DEBUG] username %s \n", username ); 
									response_id = check_user_presence( username );
						        	if ( response_id == 0 )
										client_i->username = username;
									printf( "[DEBUG] response_id %d\n", response_id);
									ret = set_pkt( &response_id, NULL );
									send_response( client_i->fd, &ret ); 
									printf("[DEBUG] porta UDP %d\n", client_i->portUDP );
									break;
						    	case 1: // !who -  invio lista clienti connessi
									 response_id = 1;
									 ret = set_pkt( &response_id, print_clients() );
									 send_response( client_i->fd, &ret );
									 break;		    				
				  		    	case 2: // !connect username
									response_id = check_user_presence( username );
									if ( response_id == 0 ) {
										response_id = -2; //utente non esistente 
										ret = set_pkt( &response_id, NULL, NULL, NULL );
										send_response( client_i->fd, &ret );
										break;
									}						
									challenged_user_fd = get_fd( username );
									free( username );
									get_info_client( client_i->fd, username, &port, ip );
									printf( "[DEBUG] username %s, porta %d, ip %s \n", username, port, ip_addr );
									response_id = 2;
									ret = set_pkt( &response_id, username, &port, ip );
									send_response( challenged_user_fd );
									break;
								case 3:
									
							// trovare fd del client sfidato
							// formare il pacchetto con { 2, dim username, username, porta, ip }
									
		//				    		/* !connect username 
		//	    					 * ricevi dimensione username
		//							 * estrai username
		//							 * verifica presenza e disponibilita' a giocare di username
		//							 * invia a username la richiesta di giocare da parte di client_i->username
		//							 * se accetta inviare a client_i la conferma 
		//							 *
		//							 */ break;
							}
				   		}
				   	}
				}
			}
        }
    }
}


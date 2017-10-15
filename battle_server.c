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

#define CMD_ID_SIZE 4

struct client_t {
	char* username;
	int portUDP;
	struct sockaddr_in addr;
	int fd;
	int ingame;
	struct client_t* next;
};

const char* ip_addr = "127.0.0.1";
struct client_t* clients = NULL;
int n_clients = 0;
char* size_buff;
char* cmd_buffer;

int extract_int( int offset ) {
    int tmp; 
	void* pointer = (void*) &tmp;
	memcpy( pointer, cmd_buffer + offset, sizeof(uint32_t) );
    printf( "[DEBUG] intero estratto %d\n", tmp );
    return tmp;
}

int check_user_presence( char* username ) {
	struct client_t* tmp;
	for( tmp = clients; tmp->next != NULL; tmp = tmp->next ) {	
		if ( strcmp( tmp->username, username ) == 0 ) {
			printf( "[DEBUG] utente gia' registato \n");
			return 1;
		}
	}
	return 0;
}

void print_clients() {
    struct client_t* i;
    printf( "[DEBUG] print %d clients \n", n_clients);
    for ( i = clients; i->next != NULL; i = i->next ) {
        printf( " Username %s ", i->username );
        if ( i->ingame ) 
            printf( "( non disponibile ) \n");
        else 
            printf( "( disponibile ) %d \n", i->fd);
    }
}


void send_response( int client_fd, int* size ) {
	int ret;
	ret = send( client_fd, (void*) size, sizeof(uint32_t), 0 ); //invio dimensione pacchetto
	if ( ret < sizeof(uint32_t) ) {
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
	free( cmd_buffer );
}

int recv_cmd(int fd) {
	int ret, cmd_size;
	size_buff = (char*) &cmd_size;
	if ( recv( fd, size_buff, sizeof(cmd_size), 0 ) < 0 ) {
		printf("[ERRORE] ricezione dimensione comando \n");
    	return -1;
	}
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

	int username_size, i, fdmax, port, ret, listener, cmd_id;
	struct sockaddr_in server_addr;
    socklen_t addrlen;
    struct client_t* client_i;
	fd_set master;
	fd_set read_fds;

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
	
	for (;;) {
		read_fds = master;
		select( fdmax + 1, &read_fds, NULL, NULL, NULL );
		for ( i = 0; i <= fdmax; i++ ) {
			if ( FD_ISSET( i, &read_fds ) ) {
				if ( i == listener ) {
					struct client_t* new_client = malloc( sizeof(struct client_t));
					if ( new_client == NULL ) 
						printf( "[ERRORE] Memoria insufficiente, impossibile gestire un nuovo cliente \n" );
					addrlen = sizeof(struct sockaddr_in);
					new_client->fd = accept( listener, ( struct sockaddr * ) &new_client->addr, &addrlen );
					new_client->username = "";
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
                            ret = recv_cmd(client_i->fd);				
							if ( ret == -1 )
                                break;

                            cmd_id = extract_int(0);					
						    switch( cmd_id ) {
								case 0:
									username_size = extract_int(4);
									char* username = malloc( username_size );
									memcpy( username, cmd_buffer + 8, username_size);
									client_i->portUDP = extract_int(8 + username_size );
									printf("[DEBUG] username %s \n", username ); 
									if ( check_user_presence( username ) == 1 ) {
										// invia errore gia' registrato 
										break;
									}
						        	client_i->username = username;
									printf("[DEBUG] porta UDP %d\n", client_i->portUDP );
									break;
																	
		//							/*# estrai dimensione username 
		//							 *# estrai username 
		//							 *# verifica username non registrata 
		//							 *# client_i->username = username estratta
		//							 *# client_i->portUDP = estrai portaUDP
		//							 * invia al client conferma registrazione */ break;
		//			    		case 1: 
		//							
		//							/* !help  invia stringa con i comandi */ break;
		//				    	case 2: 
		//		    				/* !who   inviare array string con username dei client */ break;
		//		   		    	case 3:
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


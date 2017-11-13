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
	char* opponent_username;
	int portUDP;
	struct sockaddr_in addr;
	int fd;
	int ingame;
	int under_request;
	struct client_t* next;
};

char* ip_addr = "127.0.0.1";
const char* disponibile = " ( libero )\n\t";
const char* non_disponibile = " ( in partita )\n\t";
struct client_t* clients = NULL;
int listener;
char* buff_pointer;
char* size_buff;
char* cmd_buffer;
char* printed_clients;
fd_set master;
fd_set read_fds;

struct client_t* get_client( char* username ) {
	struct client_t* tmp;
	for( tmp = clients; tmp->next != NULL; tmp = tmp->next ) 
		if ( strcmp( tmp->username, username ) == 0 ) { 
			return tmp; 
		}
	return NULL;
}

void delete_list() {
	struct client_t* next;
	struct client_t* current = clients;
	if ( clients == NULL )
		return;
	for( ;current != NULL; current = next ) {
		close( current->fd );
		next = current->next; 
		if ( current->ingame == 1 )
			free( current->opponent_username );
		free( current->username );
		free( current );
	}
	clients = NULL;
}

void catch_stop( int sig_num ) {
	signal( SIGINT, catch_stop );
	printf( " \n*** SERVER INTERROTTO ***\n" );
	delete_list();
	close(listener);
	FD_CLR( listener, &master );
	kill( getpid(), 15 );
}

int extract_int( int offset ) {
    int tmp; 
	void* pointer = (void*) &tmp;
	memcpy( pointer, cmd_buffer + offset, sizeof(int) );
    return tmp;
}

int check_user_status( char* username ) {
	struct client_t* tmp;
	for( tmp = clients; tmp->next != NULL; tmp = tmp->next ) {	
		if ( strcmp( tmp->username, username ) == 0 ) {
			if ( tmp->ingame == 1 )
				return -3; // giocatore in partita
			if ( tmp->under_request == 1 )
				return -4; // giocatore sotto richiesta
			return 0; // giocatore libero
		}
	}
	return -2; // giocatore insesistente
}

int check_user_presence( char* username ) {
	struct client_t* tmp;
	for( tmp = clients; tmp->next != NULL; tmp = tmp->next ) {	
		if ( strcmp( tmp->username, username ) == 0 ) {
			return -1;
		}
	}
	return 0;
}

char* print_clients() {
    struct client_t* i;
	int string_size = 0;
	for ( i = clients; i->next != NULL; i = i-> next ) {
		string_size += strlen( i->username ) + 1; 
		if ( i->ingame == 1 )
			string_size += strlen( non_disponibile );
		else
			string_size += strlen( disponibile );
	//	printf( "[DEBUG] %s dimesione %d \n", i->username, strlen(i->username) );
	}
	free( printed_clients );
	printed_clients = malloc(string_size);
	memset( printed_clients, 0, string_size );
	for ( i = clients; i->next != NULL; i = i->next ) {
       	strcat( printed_clients, i->username );
        if ( i->ingame ) 
            strcat(printed_clients, non_disponibile );
        else 
            strcat( printed_clients, disponibile );
    }
//	printf("[DEBUG] %s \n %d ", printed_clients, string_size ); 
	return printed_clients;
}

void insert_buff( void* src, int n ) {
	memcpy( buff_pointer, src, n );
	buff_pointer += n;
}

int set_pkt( int* response_id, char* string_msg, int* portUDP, char* ip) {
	int ret = sizeof(int);
	int string_size = 0;
	if ( string_msg != NULL ) {
		string_size = strlen(string_msg) + 1;
		ret += string_size + sizeof(int);
	}
	if ( portUDP != NULL ) 
		ret += sizeof( int );
	if ( ip != NULL )
		ret += INET_ADDRSTRLEN;
	free(cmd_buffer);
	cmd_buffer = malloc(ret);
	buff_pointer = cmd_buffer;
	insert_buff( response_id, sizeof(int));
	if ( string_msg != NULL ) {
		insert_buff( &string_size, sizeof(int));
		insert_buff( string_msg, string_size );
	} 
	if ( portUDP != NULL ) 
		insert_buff( portUDP, sizeof(int));
	if ( ip != NULL )
		insert_buff( ip, INET_ADDRSTRLEN );
	return ret;
}

void send_response( int client_fd, int* size );
void disconnect( char* username ) {
	int ret;
	struct client_t* user = get_client( username );
	user->ingame = 0;
	int id = -5;
	ret = set_pkt( &id, NULL, NULL, NULL ); 
	send_response( user->fd, &ret );
	printf( " %s si e' disconnesso dalla partita con %s \n", user->username, user->opponent_username );
	printf( " %s e' libero \n", user->opponent_username );
	free( user->opponent_username );
} 

void remove_client( int fd ) {
	struct client_t* previous = clients;
	struct client_t* current = clients->next;

	if ( previous->fd == fd ) {
		clients = current;
		printf(" %s ha interrotto la connessione \n", previous->username );
		if ( previous->ingame == 1 ) 
			disconnect( previous->opponent_username);
		close( previous->fd );
		free( previous );
		return;
	}

	for( ;current != NULL; previous = current, current = current->next ) {	
		if ( current->fd == fd ) {
			previous->next = current->next;
			current->next = NULL;
			printf(" %s ha interrotto la connessione \n", current->username );
			if ( current->ingame == 1 ) 
				disconnect( current->opponent_username);
			close( current->fd );
			free( current );
			return;
		}
	}
}

/********************* SEND RECV **********************************/

void send_response( int client_fd, int* size ) {
	int ret;
	ret = send( client_fd, (void*) size, sizeof(int), 0 ); //invio dimensione pacchetto
	if ( ret < sizeof(int) ) {
		printf("[ERRORE] Invio dimensione comando \n");
		return;
	}
	int s = *(size);
	ret = send( client_fd, (void*) cmd_buffer, s, 0 );
	if ( ret < *(size) ) {
		printf( "[ERRORE] Invio comando \n" );
		return;
	}
}

int recv_cmd(int fd) {
	int ret, cmd_size;
	size_buff = (char*) &cmd_size;
	ret = recv( fd, size_buff, sizeof(cmd_size), 0 );
	if ( ret == 0 ) {
		remove_client(fd);
		FD_CLR( fd, &master );
		return -1;
	} 
	if ( ret < 0 ) {
		printf( "[ERRORE] Dimensione comando \n");
		return -1;
	}
	free( cmd_buffer );
	cmd_buffer = malloc( cmd_size );
	ret = recv( fd, cmd_buffer, cmd_size, 0);
    if ( ret < 0 ) {
		printf("[ERRORE] Bytes ricevuti %d insufficienti per il comando scelto\n", ret);
		return -1;
	}
    return 0;
}	

/************************ MAIN ****************************************/

int main( int argc, char** argv ) {
	if ( argc != 2 ) {
		printf( "[ERRORE] Uso: ./battle_server.exe <porta> \n");
		exit(1);
	}

	int username_size, i, fdmax, port, ret, response_id, cmd_id;
	struct sockaddr_in server_addr;
    socklen_t addrlen;
    struct client_t* client_i;
	struct client_t* c_pointer;
	char* username;
	char ip[INET_ADDRSTRLEN];

    clients = (struct client_t*) malloc( sizeof( struct client_t));
   
    FD_ZERO(&master);
	FD_ZERO(&read_fds);

	port = atoi(argv[1]);
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
	printf( " Indirizzo %s (Porta: %d) \n\n", ip_addr, port );   
	FD_SET(listener, &master);

	fdmax = listener;
	signal( SIGINT, catch_stop );	
	signal( SIGQUIT, catch_stop );
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
					new_client->under_request = 0;
					new_client->next = clients;
   					clients = new_client;
				
					FD_SET( new_client->fd, &master);
					if ( new_client->fd > fdmax )
						fdmax = new_client->fd;
					printf( " Connessione stabilita con il client \n");
				}
                if ( clients != NULL ) {
                    for ( client_i = clients; client_i->next != NULL; client_i = client_i->next ) {
						if ( i == client_i->fd ) {
							ret = recv_cmd(client_i->fd);				
							if ( ret == -1 )
                                break;
					
                            cmd_id = extract_int(0);					
							if ( cmd_id != 1 && cmd_id != -2 && cmd_id != 4 ) {
								username_size = extract_int(4);
								username = malloc( username_size );
								memcpy( username, cmd_buffer + 8, username_size);
							}		
							switch( cmd_id ) {
								case -2: // utente disconnesso
									if ( client_i->ingame == 0 ) // disconnessione contemporanea dei client
										break;
									c_pointer = get_client( client_i->opponent_username );
									client_i->ingame = 0;
									disconnect( c_pointer->username );
									printf( " %s e' libero \n", c_pointer->username ); 
									break;
								case -1: // l'utente attuale ha rifiutato la sfida
									response_id = -1;
									ret = set_pkt( &response_id, username, NULL, NULL);
									c_pointer = get_client( username );
									c_pointer->under_request = 0;
									client_i->under_request = 0;
									send_response( c_pointer->fd, &ret );
									break;
								case 0: // ricezione username e porta UDP 
									client_i->portUDP = extract_int(8 + username_size );
									response_id =  check_user_presence( username );
						        	if ( response_id == 0 ) {
										client_i->username = malloc( username_size );
										strcpy( client_i->username,	username);
										printf( " %s si e' connesso\n %s e' libero \n", client_i->username, client_i->username );
									}
									ret = set_pkt( &response_id, NULL, NULL, NULL);
									send_response( client_i->fd, &ret ); 
									break;
						    	case 1: // !who -  invio lista clienti connessi
								    response_id = 1;
									ret = set_pkt( &response_id, print_clients(), NULL, NULL);
									send_response( client_i->fd, &ret );
									break;		    				
				  		    	case 2: // !connect username
									response_id = check_user_status(username );
									if ( response_id < 0 ) {
										ret = set_pkt( &response_id, NULL, NULL, NULL);
										send_response( client_i->fd, &ret ); break;
									}
									c_pointer = get_client( username );
									c_pointer->under_request = 1;
									client_i->under_request = 1;
									response_id = 2;
									ret = set_pkt( &response_id, client_i->username, NULL, NULL); // invio sfida con il nome dello sfidante
									send_response( c_pointer->fd, &ret );
									break;
								case 3: // sfida accettata | sfidato
									c_pointer = get_client( username );
									port = client_i->portUDP;
									inet_ntop( AF_INET, &(client_i->addr.sin_addr), ip, INET_ADDRSTRLEN);	
									
									c_pointer->opponent_username = malloc( strlen(client_i->username) );
									strcpy( c_pointer->opponent_username, client_i->username );
									
									client_i->opponent_username = malloc( strlen(username) );
									strcpy( client_i->opponent_username, username );
									
									c_pointer->under_request = 0;
									client_i->under_request = 0;
									c_pointer->ingame = 1; //sfidante "in partita"
									client_i->ingame = 1; //sfidato "in partita"
									printf( " %s si e' connesso a %s \n", c_pointer->username, client_i->username );
									response_id = 3;
									ret = set_pkt( &response_id, NULL, &port, ip);
									send_response( c_pointer->fd, &ret );
									break;
								case 4: // fine partita 
									c_pointer = get_client( client_i->opponent_username );
									client_i->ingame = 0;
									c_pointer->ingame = 0;
									printf( " %s e' libero \n", c_pointer->username );
									printf( " %s e' libero \n", client_i->username );
									response_id = 4;
									ret = set_pkt( &response_id, NULL, NULL, NULL );
									send_response( c_pointer->fd, &ret );
									free( client_i->opponent_username );
									free( c_pointer->opponent_username );
									break;
							}
							if ( cmd_id != 1 && cmd_id != -2 && cmd_id != 4 )
								free(username);
				   		}
				   	}
				}
			}
        }
    }
}


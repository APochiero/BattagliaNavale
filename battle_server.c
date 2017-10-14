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
struct client_t* clients;
int n_clients = 0;
char n_bytes_buff[4];
char* cmd_buffer;

void add_client( struct client_t* new_client ) {
	struct client_t* tmp = clients;
	clients = new_client;
	new_client->next = tmp;
}

int extract_int( int size, int offset ) {
	char tmp[size + 1 ];
	int i;
	for ( i = 0; i < size; i++ )
		tmp[i] = cmd_buffer[i+offset];
	tmp[size + 1] = '\0';
	return atoi(tmp);
}

void extract_username( int size, char* username ) {
	int i;
	for ( i = 0; i < 4; i++ ) 
		username[i] = cmd_buffer[ i + 8 ]; // 4 bytes per cmd_id e 4 per username_size
}

int check_user_presence( char* username ) {
	struct client_t* tmp;
	for ( tmp = clients; tmp->next != NULL; tmp = tmp->next ) 
		if ( strcmp( tmp->username, username ) == 0 )
			return 1;
	return 0;
}

void recv_cmd(int fd) {
	int ret, len;
	len = sizeof(n_bytes_buff);
	ret = recv( fd, n_bytes_buff, len, 0 );
	if ( ret < len ) {
		printf("[ERRORE] ricezione dimensione comando");
		return;
	}
	
	cmd_buffer = malloc( *(n_bytes_buff) );
	len = sizeof(cmd_buffer);
	ret = recv( fd, cmd_buffer, len, 0);
	if ( ret < *(n_bytes_buff) ) {
		printf("[ERRORE] Bytes ricevuti insufficienti per il comando scelto");
		return;
	}
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
		perror( "[ERRORE] Binding " );
		exit(1);
	} 
	ret = listen(listener, 10 );
	if( ret == -1  ) {
		perror("[ERRORE] Listening ");
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
				for ( client_i = clients; client_i->next != NULL; client_i = client_i->next ) {
					if ( i == client_i->fd ) {
						recv_cmd(client_i->fd);				
						cmd_id = extract_int(4,0);					
					    switch( cmd_id ) {
							case 0:
									username_size = extract_int(4,4);
									char* username = malloc( username_size );
									extract_username( username_size, username );
									printf("[DEBUG] username %s \n", username ); 
									if ( check_user_presence( username ) == 1 ) {
										// invia errore gia' registrato 
										break;
									}
									client_i->username = username;
									client_i->portUDP = extract_int(16, 8 + username_size );
									printf("[DEBUG] porta UDP %d\n", client_i->portUDP );

									
									/*# estrai dimensione username 
									 *# estrai username 
									 *# verifica username non registrata 
									 *# client_i->username = username estratta
									 *# client_i->portUDP = estrai portaUDP
									 * invia al client conferma registrazione */ break;
							case 1: 
									
								/* !help  invia stringa con i comandi */ break;
							case 2: 
								/* !who   inviare array string con username dei client */ break;
							case 3:
								/* !connect username 
								 * ricevi dimensione username
								 * estrai username
								 * verifica presenza e disponibilita' a giocare di username
								 * invia a username la richiesta di giocare da parte di client_i->username
								 * se accetta inviare a client_i la conferma 
								 *
								 */ break;
				    	}
			    	}
				}	
				if ( i == listener ) {
					struct client_t* new_client= (struct client_t*) malloc( sizeof(struct client_t));
					if ( new_client == NULL ) 
						printf( "[ERRORE] Memoria insufficiente, impossibile gestire un nuovo cliente " );
					add_client( new_client ); 
					addrlen = sizeof(struct sockaddr_in);
					new_client->fd = accept( listener, ( struct sockaddr * ) &new_client->addr, &addrlen );
					new_client->ingame = 0;
					FD_SET( new_client->fd, &master);
					if ( new_client->fd > fdmax )
						fdmax = new_client->fd;
					
				}
			}
        }
    }
}


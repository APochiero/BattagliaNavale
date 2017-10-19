// battle_client.c

#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#define READ_BUFFER_SIZE 256
#define STDIN 0
#define PORT_SIZE 	 16
#define CMD_ID_SIZE  4
#define N_ROWS	  	 6
#define N_COLUMNS 	 6
#define N_BOATS	  	 7
#define HELP "!help\0"
#define CONNECT "!connect\0"
#define QUIT "!quit\0"
#define WHO "!who\0"

#define HELP_MSG "Sono disponibili i seguenti comandi:\n!help --> mostra l'elenco dei comandi disponibili \n!who --> mostra l'elenco dei client connessi al server \n!connect username --> avvia una partita con l'utente username \n!quit --> disconnette il client dal server\n\n\0"

enum cell_t  { BUSY, FREE, HIT, MISS };

enum cell_t  enemy_grid[N_ROWS][N_COLUMNS];
enum cell_t  my_grid[N_ROWS][N_COLUMNS];
unsigned enemy_hits;

char* size_buff;
char* cmd_buffer;
char* buff_pointer;
fd_set master;
fd_set read_fds;

void catch_stop( int sig_num ) {
	signal( SIGINT, catch_stop );
	printf( " *** Tentativo di interruzione del Client *** \nUsare il comando !quit per un corretto abbandono della connessione\n> ");
	fflush(stdout);
}

int extract_int( int offset ) {
    int tmp; 
	void* pointer = (void*) &tmp;
	memcpy( pointer, cmd_buffer + offset, sizeof(int) );
    printf( "[DEBUG] intero estratto %d\n", tmp );
    return tmp;
}



void insert_buff( void* src, int n ) {
	memcpy( buff_pointer, src, n );
	buff_pointer += n;
}

int set_buffer( int* cmd_id, char* username, int* portUDP ) {
	int ret = sizeof(int);
	int user_size = 0;

	if ( username != NULL ) {
		user_size = strlen(username) + 1 ;
		ret += user_size + sizeof(int);
	}
	if ( portUDP != NULL ) 
		ret += sizeof(int);
	cmd_buffer = malloc( ret );
	buff_pointer = cmd_buffer;
	insert_buff( cmd_id, sizeof(int)); 
	if ( username != NULL ) {
		insert_buff( &user_size, sizeof(int));
		insert_buff( username, user_size );
	}
	if ( portUDP != NULL )
		insert_buff( portUDP, sizeof(int));
	return ret;
}	

void send_cmd( int server_d, int* size ) {
	int ret;
	ret = send( server_d, (void*) size, sizeof(int), 0 ); //invio dimensione pacchetto
	if ( ret < sizeof(int) ) {
		printf("[ERRORE] Invio dimensione comando \n");
		return;
	}
	int s = *(size);
//	printf("[DEBUG] bytes da inviare %d \n", s);
	ret = send( server_d, (void*) cmd_buffer, s, 0 );
	if ( ret < *(size) ) {
		printf( "[ERRORE] Invio comando \n" );
		return;
	}
//	printf( "[DEBUG] bytes inviati %d \n", ret );
	printf( "> " );
	fflush(stdout);
	free( cmd_buffer );
}

int recv_response(int fd) {
	int ret, cmd_size;
	size_buff = (char*) &cmd_size;
	ret = recv( fd, size_buff, sizeof(cmd_size), 0 );
	if ( ret == 0 ) {
		printf( "[INFO] Disconessione del server %d \n", fd );
		close(fd);
		FD_CLR( fd, &master );
		return -1;
	} 
	if ( ret < 0 ) {
		printf( "[ERRORE] Dimensione comando \n");
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



int main( int  argc, char** argv) {
	if ( argc != 3 ) {
		printf( "[ERRORE] Uso: ./battle_client.exe <host remoto> <porta> \n");
		exit(1);
	}

    int i, fdmax, cmd_id, portUDP, ret,response_id, list_size, server_d, server_port;
	char read_buffer[READ_BUFFER_SIZE];
	char* cmd_name;
	char* challenged_user;
	const char delimiter[2] = " ";
	struct sockaddr_in server_addr;
	
	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	signal( SIGINT, catch_stop );
    memset( &server_addr, 0, sizeof( server_addr ));
   
	server_d = socket( AF_INET, SOCK_STREAM, 0 ); 
	server_port = atoi(argv[2]);
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( server_port );
    inet_pton( AF_INET, argv[1], &server_addr.sin_addr );

    ret = connect( server_d, (struct sockaddr* ) &server_addr, sizeof( server_addr ));
    if ( ret == -1 ) {
        perror( "[ERRORE] Connessione al server \n");
        exit(1);
    }
 	FD_SET(server_d, &master);
	FD_SET(STDIN, &master);
	fdmax = server_d;
    printf( "[INFO] Connessone al server %s ( porta %d ) avvenuta con successo \n\n%s", argv[1], server_port, HELP_MSG );
	
	do {
		if ( ret == -1 )
			printf( "Username gia' presente nel server \n");
		printf( "Inserisci il tuo nome: " );
		scanf( "%s" , read_buffer );
		printf( "Inserisci la porta UDP di ascolto: ");
		scanf( "%d", &portUDP );
		cmd_id = 0;
		ret = set_buffer( &cmd_id, read_buffer, &portUDP );
		send_cmd( server_d, &ret );
		recv_response( server_d );
		ret = extract_int(0);
		printf( "[DEBUG] risposta registrazione %d \n", ret );
	} while ( ret == -1 );

	for(;;) {
		read_fds = master;
		select( fdmax + 1, &read_fds, NULL, NULL, NULL );
		for( i = 0; i <= fdmax; i++ ) {
			if ( FD_ISSET( i, &read_fds ) ) {
				printf( "[DEBUG] fd pronto %d \n", i );
				if ( i == STDIN ) {
					fgets( read_buffer, READ_BUFFER_SIZE, stdin );
					char *rmv_newline = strchr( read_buffer, '\n');
					*rmv_newline = ' ';
					strcat( read_buffer, delimiter);
					cmd_name = strtok( read_buffer, delimiter);	
					if ( cmd_name == '\0' )
						break;
					if ( strcmp( cmd_name, QUIT ) == 0 ) {
						printf( "[INFO] Disconnessione dal server \n" );
						close( server_d );
						return 0;
					} 
					if ( strcmp( cmd_name, HELP ) == 0 ) {
						printf ( "%s>", HELP_MSG );
						fflush(stdout);
						break;
					}
					if ( strcmp( cmd_name, WHO ) == 0 ) {
						cmd_id = 1;
						ret = set_buffer( &cmd_id, NULL, NULL ); 
					} else if ( strcmp( cmd_name, CONNECT ) == 0 ) {
						cmd_id = 2;
						challenged_user = strtok( NULL, delimiter );
						printf( "username avversario %s \n", challenged_user );
						ret = set_buffer( &cmd_id, challenged_user, NULL ); 
					} else {
						printf("Comando non riconosciuto \n> " );
						fflush(stdout);
						break;
					}
		
					printf("[DEBUG] Richiesta comando %d \n", cmd_id );
					send_cmd(server_d, &ret);
					printf( "> ");
					fflush(stdout);
				}
				if ( i == server_d ) {
					printf( "[DEBUG] server socket pronto \n");
					ret = recv_response( server_d );
					if ( ret == -1 )
						break;

					response_id = extract_int(0);
					switch( response_id ) {
						case 1: // risposta al comando !who contenente la lista di client connnessi
							list_size = extract_int(4);
							char* list = malloc(list_size);
							memcpy( list, cmd_buffer + 8, list_size);
							printf( " Client connessi al server: \n %s", list);
							fflush(stdout); 
							break;
					}
				}
			}
		}
	}
    return 0;
}

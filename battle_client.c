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

char* cmd_buffer;
char* buff_pointer;

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


int main( int  argc, char** argv) {
	if ( argc != 3 ) {
		printf( "[ERRORE] Uso: ./battle_client.exe <host remoto> <porta> \n");
		exit(1);
	}

    int i, fdmax, cmd_id, portUDP, ret, server_d, server_port;
	char read_buffer[READ_BUFFER_SIZE];
	char* cmd_name;
	char* challenged_user;
	const char delimiter[2] = " ";
	struct sockaddr_in server_addr;
	fd_set master;
	fd_set read_fds;

	FD_ZERO(&master);
	FD_ZERO(&read_fds);

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
	

	printf( "Inserisci il tuo nome: " );
	scanf( "%s" , read_buffer );
	printf( "Inserisci la porta UDP di ascolto: ");
	scanf( "%d", &portUDP );
	cmd_id = 0;
	ret = set_buffer( &cmd_id, read_buffer, &portUDP );
	send_cmd( server_d, &ret );

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
						ret = set_buffer( &cmd_id, challenged_user, NULL ); 
					} else if ( strcmp( cmd_name, QUIT ) == 0 ) {
						cmd_id = 3;
						ret = set_buffer( &cmd_id, NULL, NULL ); 
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
				}
			}
		}
	}
    return 0;
}

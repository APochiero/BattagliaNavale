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
int server_d, UDP_fd;
int end = 0;
char* size_buff;
char* cmd_buffer;
char* buff_pointer;
char read_buffer[READ_BUFFER_SIZE];
fd_set master;
fd_set read_fds;

/**************** UTILITA' *********************/

void catch_stop( int sig_num ) {
	signal( SIGINT, catch_stop );
	printf( " *** Tentativo di interruzione del Client *** \nUsare il comando !quit per un corretto abbandono della connessione\n> ");
	fflush(stdout);
}

int extract_int( char* buffer, int offset ) {
    int tmp; 
	void* pointer = (void*) &tmp;
	memcpy( pointer, buffer + offset, sizeof(int) );
    printf( "[DEBUG] intero estratto %d\n", tmp );
    return tmp;
}

void insert_buff( void* src, int n ) {
	memcpy( buff_pointer, src, n );
	buff_pointer += n;
}

int set_pkt( int* cmd_id, char* username, int* portUDP ) {
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

/************ SEND E RECEIVE ********************/

void send_cmd( int* size, struct sockaddr_in* opponent_addr ) {
	int ret;
	if ( opponent_addr == NULL )
		ret = send( server_d, (void*) size, sizeof(int), 0 ); //invio dimensione pacchetto
	else 
		ret = sendto( UDP_fd, (void*) size, sizeof(int), 0, (struct sockaddr*) opponent_addr, sizeof(struct sockaddr));

	if ( ret < sizeof(int) ) {
		perror("[ERRORE] Invio dimensione comando \n");
		return;
	}
	int s = *(size);
	printf("[DEBUG] bytes da inviare %d \n", s);
	if ( opponent_addr == NULL )
		ret = send( server_d, (void*) cmd_buffer, s, 0 );
	else 
		ret = sendto( UDP_fd, (void*) cmd_buffer, s, 0, (struct sockaddr*) opponent_addr, sizeof(struct sockaddr));
	if ( ret < *(size) ) {
		perror( "[ERRORE] Invio comando \n" );
		return;
	}
	printf( "[DEBUG] bytes inviati %d \n", ret );
}

int recv_response(int fd) {
	int ret, cmd_size;
	size_buff = (char*) &cmd_size;
	ret = recv( fd, size_buff, sizeof(cmd_size), 0 );
	if ( ret == 0 ) {
		printf( " *** SERVER INTERROTTO *** \n" );
		close(fd);
		FD_CLR( fd, &master );
		end = 1;
		return -1;
	} 
	if ( ret < 0 ) {
		printf( "[ERRORE] Dimensione comando \n");
		return -1;
	}
//	free( cmd_buffer );
	cmd_buffer = malloc( cmd_size );

	ret = recv( fd, cmd_buffer, cmd_size, 0);
    if ( ret < 0 ) {
		printf("[ERRORE] Bytes ricevuti %d insufficienti per il comando scelto\n", ret);
		return -1;
	}
    printf("[DEBUG] bytes ricevuti %d \n", ret );
    return 0;
}	

/*************** GESTIONE GIOCO ******************/

void set_tokens() {
	int x,y,i;
	i = 0;
	printf( "Posiziona 7 caselle: (formato \"x,y\")\n " );
	while ( i < 7 ) {
		scanf( "%s", read_buffer );
		printf( "[DEBUG] buffer %s\n", read_buffer );
		x = extract_int( read_buffer,0);
		y = extract_int( read_buffer,6);
		my_grid[x][y] = BUSY;
	}
}


/******************* MAIN ************************/

int main( int  argc, char** argv) {
	if ( argc != 3 ) {
		printf( "[ERRORE] Uso: ./battle_client.exe <host remoto> <porta> \n");
		exit(1);
	}

    int i, fdmax, username_size, cmd_id, portUDP, ret,response_id, list_size, server_port;
	char* cmd_name;
	char* my_username;
	char* challenged_user;
	char* challenging_user;
	char ip[INET_ADDRSTRLEN];
	char y_n;
	const char delimiter[2] = " ";
	struct sockaddr_in server_addr, my_addr, opponent_addr;
	
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
    printf( "[INFO] Connessone al server %s ( porta %d ) avvenuta con successo \n\n%s", argv[1], server_port, HELP_MSG );
	portUDP = 4444;
	do {
		if ( ret == -1 )
			printf( "Username gia' presente nel server \n");
		if ( portUDP < 1024 || portUDP > 65536 )
			printf( "Porta non esistente ( intervallo porte [1024,65536] )\n");
		printf( "Inserisci il tuo nome: " );
		scanf( "%s" , read_buffer );
		printf( "Inserisci la porta UDP di ascolto: ");
		scanf( "%d", &portUDP );
		cmd_id = 0;
		ret = set_pkt( &cmd_id, read_buffer, &portUDP );
		send_cmd( &ret, NULL);
		recv_response( server_d );
		ret = extract_int( cmd_buffer, 0);
	//	printf( "[DEBUG] risposta registrazione %d \n", ret );
	} while ( ret == -1 ||  portUDP < 1024 || portUDP > 65536  );
	
	my_username = malloc( strlen( read_buffer ));
	strcpy( my_username, read_buffer );

	printf( "\n> " );
	fflush(stdout);
	UDP_fd = socket( AF_INET, SOCK_DGRAM, 0 );
	memset( &my_addr, 0, sizeof( my_addr ));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons( portUDP );
	my_addr.sin_addr.s_addr = INADDR_ANY;
	ret = bind ( UDP_fd, (struct sockaddr*) &my_addr, sizeof( my_addr ));
	FD_SET( UDP_fd, &master );
	fdmax = UDP_fd;
	
	while( end == 0 ) {
		read_fds = master;
		select( fdmax + 1, &read_fds, NULL, NULL, NULL );
		for( i = 0; i <= fdmax; i++ ) {
			if ( FD_ISSET( i, &read_fds ) ) {
			//	printf( "[DEBUG] fd pronto %d \n", i );
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
						printf ( "%s> ", HELP_MSG );
						fflush(stdout);
						break;
					}
					if ( strcmp( cmd_name, WHO ) == 0 ) {
						cmd_id = 1;
						ret = set_pkt( &cmd_id, NULL, NULL ); 
					} else if ( strcmp( cmd_name, CONNECT ) == 0 ) {
						cmd_id = 2;
						challenged_user = strtok( NULL, delimiter );
						if ( challenged_user == NULL ) {
							printf( "Uso: !connect <username> \n> ");
							fflush(stdout);
							break;
						}
						if ( strcmp( challenged_user, my_username ) == 0 ) {
							printf( " Specificare un username diverso dal proprio \n" );
							break;
						}
						ret = set_pkt( &cmd_id, challenged_user, NULL ); 
					} else {
						printf("Comando non riconosciuto \n> " );
						fflush(stdout);
						break;
					}
		
				//	printf("[DEBUG] Richiesta comando %d \n", cmd_id );
					send_cmd(&ret, NULL);
				}
				if ( i == server_d ) {
				//	printf( "[DEBUG] server socket pronto \n");
					ret = recv_response( server_d );
					if ( ret == -1 )
						break;

					response_id = extract_int( cmd_buffer, 0);
					switch( response_id ) { 
						case -3: // utente non esiste
							printf( " %s inesistente \n> ", challenged_user );
							break;
						case -2: // utente sfidato non e' disponibile a giocare
							printf( " %s gia' occupato in una partita \n> ", challenged_user );
							break;
						case -1: // l'utente sfidato ha rifiutato la partita
							printf( " %s ha rifutato la partita \n> ", challenged_user );
							break;
						case 1: // risposta al comando !who contenente la lista di client connnessi
							list_size = extract_int( cmd_buffer, 4);
							char* list = malloc(list_size);
							memcpy( list, cmd_buffer + 8, list_size);
							printf( " Client connessi al server: %s \n> ", list);
							break;
						case 2: // sei stato sfidato 
							username_size = extract_int( cmd_buffer, 4);
							challenging_user = malloc(username_size);
							memcpy( challenging_user, cmd_buffer + 8, username_size );
							printf( " Sei stato sfidato da %s \n Premere (S) per accettare o (N) per rifiutare: ", challenging_user );
							do {
								scanf("%c", &y_n);
							} while ( y_n != 's' && y_n != 'S' && y_n != 'n' && y_n != 'N' );
							printf("> ");
							if ( y_n == 's' || y_n == 'S' ) 			
								cmd_id = 3; //sfida accettata
							 else if ( y_n == 'n' || y_n == 'N') 
								cmd_id = -1; // sfida rifiutata 
							
							ret = set_pkt( &cmd_id, challenging_user, NULL ); 
							send_cmd( &ret, NULL );
							break;
						case 3: // l'utente sfidato ha accettato la partita
							printf( " %s ha accettato la partita \n# ", challenged_user ); 
							memset( &opponent_addr, 0, sizeof( opponent_addr ));
							opponent_addr.sin_family = AF_INET;
							opponent_addr.sin_port = htons( extract_int( cmd_buffer,  4 ));
							memcpy( ip, cmd_buffer + 8, INET_ADDRSTRLEN );
							printf("[DEBUG] info avversario ricevute: ip %s, porta %d \n", ip, opponent_addr.sin_port );
							inet_pton( AF_INET, ip, &opponent_addr.sin_addr );	
							cmd_id = 0; // presentazione 
							ret = set_pkt( &cmd_id, NULL, NULL );
							send_cmd( &ret, &opponent_addr );
							printf( " \n ### sfidante PARTITA CONTRO %s INIZIATA \n", challenged_user);	
							set_tokens();
							break;
					} 
					fflush(stdout);
				} if ( i == UDP_fd ) {
					ret = recv_response( UDP_fd );
					if ( ret == -1 )
						break;
					
					response_id = extract_int( cmd_buffer, 0);
					printf("[DEBUG] UDP fd richiede %d \n", response_id);
					switch( response_id ) { 
						case 0: // lo sfidato riceve dallo sfidante il primo pacchetto udp
							printf( " \n ### PARTITA CONTRO %s INIZIATA \n", challenging_user);	
							set_tokens();
							break;
					}
				}
			}
		}
	}
    return 0;
}

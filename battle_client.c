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

#define ANSI_COLOR_WHITE "\033[37;1m"
#define ANSI_COLOR_RED "\033[31;1m"
#define ANSI_COLOR_GREEN "\033[32;1m"
#define ANSI_COLOR_BLUE "\033[34;1m"
#define ANSI_COLOR_YELLOW "\033[33;1m"
#define ANSI_COLOR_CYANO "\033[36;1m"
#define ANSI_COLOR_RESET "\033[0m"
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
#define DISCONNECT "!disconnect\0"
#define SHOT "!shot\0"
#define SHOW "!show\0"

#define HELP_MSG "Sono disponibili i seguenti comandi:\n  !help --> mostra l'elenco dei comandi disponibili \n  !who --> mostra l'elenco dei client connessi al server \n  !connect username --> avvia una partita con l'utente username \n  !quit --> disconnette il client dal server\n\n\0"

#define HELP_MSG_INGAME "Sono disponibili i seguenti comandi:\n  !help --> mostra l'elenco dei comandi disponibili\n  !disconnect --> disconnette il client dall'attuale partita\n  !shot square --> fai un tentativo con la casella square\n  !show --> visuallizza griglia di gioco\n\n\0"

enum cell_t  { BUSY, FREE, HIT, MISS };

enum cell_t  opponent_grid[N_ROWS][N_COLUMNS];
enum cell_t  my_grid[N_ROWS][N_COLUMNS];
int server_fd, UDP_fd, ingame;
int opponent_hits = 0;
int my_hits = 0;
int square[2];
char* size_buff;
char* cmd_buffer;
char* buff_pointer;
char* my_username;
char* opponent_username;
char* who_list;
char read_buffer[READ_BUFFER_SIZE];
struct sockaddr_in opponent_addr;
struct timeval tv = {60, 0};   
fd_set master;
fd_set read_fds;

/**************** UTILITA' *********************/

void catch_stop( int sig_num ) {
	close(server_fd);
	close(UDP_fd);
	printf( " \n*** CLIENT INTERROTTO *** \n" );
	if ( ingame == 1 )
		free( opponent_username );
	free(my_username);
	kill( getpid(), 15 );
}

int extract_int( char* buffer, int offset ) {
    int tmp; 
	void* pointer = (void*) &tmp;
	memcpy( pointer, buffer + offset, sizeof(int) );
    //printf( "[DEBUG] intero estratto %d\n", tmp );
    return tmp;
}

void insert_buff( void* src, int n ) {
	memcpy( buff_pointer, src, n );
	buff_pointer += n;
}

int set_pkt( int* cmd_id, char* string_msg, int* n1, int* n2, char* ip ) {
	int ret = sizeof(int);
	int string_size = 0;
	//printf("[DEBUG] pacchetto con comando %d \n", *(cmd_id));
	if ( string_msg != NULL ) {
		string_size = strlen(string_msg) + 1 ;
		ret += string_size + sizeof(int);
	}
	if ( n1 != NULL ) 
		ret += sizeof(int);
	if ( n2 != NULL )
		ret += sizeof(int);
	if ( ip != NULL )
		ret += INET_ADDRSTRLEN;
	free(cmd_buffer);
	cmd_buffer = malloc( ret );
	buff_pointer = cmd_buffer;
	insert_buff( cmd_id, sizeof(int)); 
	if ( string_msg != NULL ) {
		insert_buff( &string_size, sizeof(int));
		insert_buff( string_msg, string_size );
	}
	if ( n1 != NULL )
		insert_buff( n1, sizeof(int));
	if ( n2 != NULL )
		insert_buff( n2, sizeof(int));
	if ( ip != NULL )
		insert_buff( ip, INET_ADDRSTRLEN );
	return ret;
}

/************ SEND E RECEIVE ********************/

void send_cmd( int* size, int TCP_UDP ) {
	int ret;
	if ( TCP_UDP == 0 )
		ret = send( server_fd, (void*) size, sizeof(int), 0 ); //invio dimensione pacchetto
	else 
		ret = sendto( UDP_fd, (void*) size, sizeof(int), 0, (struct sockaddr*) &opponent_addr, sizeof(struct sockaddr));

	if ( ret < sizeof(int) ) {
		perror("[ERRORE] Invio dimensione comando \n");
		return;
	}
	int s = *(size);
	//printf("[DEBUG] bytes da inviare %d\n", s);
	if ( TCP_UDP == 0 )
		ret = send( server_fd, (void*) cmd_buffer, s, 0 );
	else 
		ret = sendto( UDP_fd, (void*) cmd_buffer, s, 0, (struct sockaddr*) &opponent_addr, sizeof(struct sockaddr));
	if ( ret < *(size) ) {
		perror( "[ERRORE] Invio comando \n" );
		return;
	}
	//printf( "[DEBUG] bytes inviati %d \n", ret );

}

int recv_response(int TCP_UDP ) {
	int ret, cmd_size;
	socklen_t addrlen = sizeof(opponent_addr);
	size_buff = (char*) &cmd_size;
	if ( TCP_UDP == 0 )
		ret = recv( server_fd, size_buff, sizeof(cmd_size), 0 );
	else 
		ret = recvfrom( UDP_fd, size_buff, sizeof(cmd_size), 0, (struct sockaddr*) &opponent_addr, &addrlen);

	if ( ret == 0 && TCP_UDP == 0 ) {
		printf( " *** SERVER INTERROTTO *** \n" );
		close(server_fd);
		FD_CLR( server_fd, &master );
		catch_stop(0);
		return -1;
	} else if ( ret < 0 ) {
		printf( "[ERRORE] Dimensione comando via TCP_UDP %d\n", TCP_UDP);
		return -1;
	}

	free(cmd_buffer);
	cmd_buffer = malloc( cmd_size );

	if ( TCP_UDP == 0 )
		ret = recv( server_fd, cmd_buffer, cmd_size, 0);
	else
		ret = recvfrom( UDP_fd, cmd_buffer, cmd_size, 0,(struct sockaddr*) &opponent_addr, &addrlen);

    if ( ret < 0 ) {
		printf("[ERRORE] Bytes ricevuti %d insufficienti per il comando scelto\n", ret);
		return -1;
	}
    //printf("[DEBUG] bytes ricevuti %d \n", ret );
    return 0;
}	

/*************** GESTIONE GIOCO ******************/

void clean_grid() {
	int i,j;
	for ( i = 0; i < N_ROWS; i++ ) 
		for ( j = 0; j < N_COLUMNS; j++ ) {
			my_grid[i][j] = opponent_grid[i][j] = FREE;
		}
}

int get_square( char* string, int* square ) {
	const char delimiter[2] = ",";
	char *s;
	s = strtok(string, delimiter );
	if ( s == NULL ) 
		return -1;
	square[0] = atoi(s);
	s =  strtok( NULL, delimiter );
	if ( s == NULL )
		return -1;
	square[1] = atoi(s);
	if ( square[0] < 1 || square[0] > 6 || square[1] < 1 || square[1] > 6 ) {
		printf( " Casella non valida ( intervallo valido [1-6] ) \n" );
		return -1;
	}
	square[0]--; square[1]--;
	//printf( "[DEBUG] letti %d,%d \n", square[0], square[1]);
	return 0;
}	

void set_tokens() {
	int i = 1;
	clean_grid();
	printf( "Posiziona 7 caselle: (formato \"x,y\")\n" );
	while ( i <= 7 ) {
		printf( " Inserisci casella %d): ", i );
		scanf( "%s", read_buffer );
		if ( get_square( read_buffer, square ) < 0 )
			continue;
		else if ( my_grid[square[0]][square[1]] == BUSY ) 
			printf( " Casella gia' occupata \n" );
		else {
			my_grid[square[0]][square[1]] = BUSY;
			i++;
		}
	}
	printf( "\n In attesa che %s abbia posizionato le sue navi... \n", opponent_username); 
}

void print_symbol( enum cell_t cell, int opponent) {  // BUSY # FREE - HIT X MISS O
	switch ( cell ) {
		case BUSY: printf( "%s %c %s", ANSI_COLOR_WHITE, '#', ANSI_COLOR_RESET );break;
		case FREE: printf( "%s %c %s", ANSI_COLOR_CYANO, '-', ANSI_COLOR_RESET );break;
		case HIT: if ( opponent == 0 )
				  	printf( "%s %c %s", ANSI_COLOR_RED, 'X', ANSI_COLOR_RESET );
				  else 
					printf( "%s %c %s", ANSI_COLOR_GREEN, 'X', ANSI_COLOR_RESET );break;
		case MISS: printf( "%s %c %s", ANSI_COLOR_YELLOW, 'O', ANSI_COLOR_RESET );break;
	}
}

void print_grid() {	
	int i,j;
	printf( "\t   1  2  3  4  5  6 \n" );
	for ( i = 0; i < N_ROWS; i++ ) {
		printf( "\t%d ", i+1); 
		for ( j = 0; j < N_COLUMNS; j++ ) 
			print_symbol(opponent_grid[i][j], 1);
		printf( "\n" );
	}
	printf( "| Punteggio:  %s: %d | %s: %d |\n\t   1  2  3  4  5  6 \n" , my_username, my_hits, opponent_username, opponent_hits ); 
	for ( i = 0; i < N_ROWS; i++ ) {
		printf( "\t%d ", i+1); 
		for ( j = 0; j < N_COLUMNS; j++ )
			print_symbol( my_grid[i][j], 0);
		printf( "\n" );
	}
	printf( "# ");
}

/******************* MAIN ************************/

int main( int  argc, char** argv) {
	if ( argc != 3 ) {
		printf( "[ERRORE] Uso: ./battle_client.exe <host remoto> <porta> \n");
		exit(1);
	}

    int opponent_rtp, turn, i, fdmax, username_size, cmd_id, portUDP, ret,response_id, list_size, server_port;
	int square[2];
	char* cmd_name;
	char* square_string;
	char ip[INET_ADDRSTRLEN];
	char y_n;
	const char delimiter[2] = " ";
	struct sockaddr_in server_addr, my_addr;
	
	FD_ZERO(&master);
	FD_ZERO(&read_fds);

	signal( SIGINT, catch_stop );
    signal( SIGQUIT, catch_stop );
	memset( &server_addr, 0, sizeof( server_addr ));
   
	server_fd = socket( AF_INET, SOCK_STREAM, 0 ); 
	server_port = atoi(argv[2]);
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( server_port );
    inet_pton( AF_INET, argv[1], &server_addr.sin_addr );

    ret = connect( server_fd, (struct sockaddr* ) &server_addr, sizeof( server_addr ));
    if ( ret == -1 ) {
        perror( "[ERRORE] Connessione al server \n");
        exit(1);
    }
 	FD_SET(server_fd, &master);
	FD_SET(STDIN, &master);
    printf( "[INFO] Connessone al server %s ( porta %d ) avvenuta con successo \n\n%s", argv[1], server_port, HELP_MSG );
	portUDP = 4444;
	do {
		if ( ret == -1 )
			printf( "Username gia' presente nel server \n");
		printf( "Inserisci il tuo nome: " );
		scanf( "%s" , read_buffer );
		printf( "Inserisci la porta UDP di ascolto: ");
		scanf( "%d", &portUDP );
		if ( portUDP < 1024 || portUDP > 65535 ) {
			printf( "Porta non esistente ( intervallo porte [1024,65535] )\n");
			continue;
		}
		cmd_id = 0;
		ret = set_pkt( &cmd_id, read_buffer, &portUDP, NULL, NULL );
		send_cmd( &ret,0);
		recv_response( 0 );
		ret = extract_int( cmd_buffer, 0);
	//	printf( "[DEBUG] risposta registrazione %d \n", ret );
	} while ( ret == -1 || portUDP < 1024 || portUDP > 65535 );
	
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
	FD_SET(UDP_fd, &master );
	fdmax = UDP_fd;

	ingame = 0;
	for( ;; ) {
		read_fds = master;
		ret = select( fdmax + 1, &read_fds, NULL, NULL, &tv );
		if ( ret == 0 && ingame == 1  ) {
			cmd_id = -2; 
			ret = set_pkt( &cmd_id, NULL, NULL, NULL, NULL ); 
			send_cmd( &ret, 0 );
			ingame = 0;
			free(opponent_username);
			printf( " Disconnessione per inattivita'\n> ");
			fflush(stdout);
		} else {
			tv.tv_sec = 60;
			tv.tv_usec = 0;
			for( i = 0; i <= fdmax; i++ ) {
				if ( FD_ISSET( i, &read_fds ) ) {
/******	*********** LETTURA COMANDI **************************/
					if ( i == STDIN ) {
						fgets( read_buffer, READ_BUFFER_SIZE, stdin );
						char *rmv_newline = strchr( read_buffer, '\n');
						*rmv_newline = ' ';
						strcat( read_buffer, delimiter);
						cmd_name = strtok( read_buffer, delimiter);	
						if ( cmd_name == '\0' && ingame == 0 ) {
							printf("> ");
							break;
						} else if ( cmd_name == '\0' && ingame == 1 ) {
							printf("# ");
							break;
						}
						switch ( ingame ) {
							case 0:
								if ( strcmp( cmd_name, QUIT ) == 0 ) {
									printf( "[INFO] Disconnessione dal server \n" );
									free(my_username);
									close( server_fd );
									return 0;
								} 
								if ( strcmp( cmd_name, HELP ) == 0 ) 
									printf ( "%s> ", HELP_MSG );
								else if ( strcmp( cmd_name, WHO ) == 0 ) {
									cmd_id = 1;
									ret = set_pkt( &cmd_id, NULL, NULL, NULL, NULL ); 
									send_cmd(&ret,0);
								} else if ( strcmp( cmd_name, CONNECT ) == 0 ) {
									cmd_id = 2;
									char* tmp = strtok( NULL, delimiter );
									opponent_username = malloc( strlen( tmp) );
									strcpy( opponent_username, tmp );
									if ( opponent_username == NULL ) 
										printf( "Uso: !connect <username> \n> ");
									else if ( strcmp( opponent_username, my_username ) == 0 ) 
										printf( " Specificare un username diverso dal proprio \n> " );
								 	else {
										printf( " In attesa di %s... \n", opponent_username );
										ret = set_pkt( &cmd_id, opponent_username, NULL, NULL, NULL ); 
										send_cmd(&ret, 0);
									}
								} else 
									printf(" Comando non riconosciuto \n> " );
								break;
							case 1:
								if ( strcmp( cmd_name, DISCONNECT ) == 0 ) {
									cmd_id = -2; // resa 
									//printf("[DEBUG] opponent_username %s \n", opponent_username );
									ret = set_pkt( &cmd_id, NULL, NULL, NULL, NULL ); 
									send_cmd( &ret, 0 );
									ingame = 0;
									free(opponent_username);
									printf( " Disconnessione avvenuta con successo: TI SEI ARRESO\n> ");
								} else if ( strcmp( cmd_name, HELP ) == 0 ) 
									printf( "%s# ", HELP_MSG_INGAME );
								else if ( strcmp( cmd_name, SHOT ) == 0 ) {
									square_string = strtok( NULL, delimiter );
									if ( square_string == NULL )
										printf( " Uso: !shot <casella> \n# ");
									else if ( opponent_rtp == 0 )
										printf( " %s non e' ancora pronto\n# ", opponent_username );
									else if ( turn == 0 ) 
										printf(" Attendi il tuo turno \n# " ); 
									else if ( get_square(square_string, square) == 0 ) {
										if ( opponent_grid[square[0]][square[1]] == HIT || opponent_grid[square[0]][square[1]] == MISS ) 
											printf( " Hai gia' sparato questa casella \n# " );
										else {
											cmd_id = 2;
											ret = set_pkt( &cmd_id, NULL, &square[0], &square[1], NULL);
											send_cmd( &ret, 1);
										}
									} else 
										printf( " Uso: !shot <casella> \n# ");
								} else if ( strcmp( cmd_name, SHOW ) == 0 ) 
									print_grid();		
								else 
									printf( "Comando non riconosciuto \n# ");
								break;
						}
						fflush(stdout);
					}
/******	************* RISPOSTE DAL SERVER ****************************/
					if ( i == server_fd ) {
					//	printf( "[DEBUG] server socket pronto \n");
						ret = recv_response(0);
						if ( ret == -1 )
							break;
	
						response_id = extract_int( cmd_buffer, 0);
						switch( response_id ) { 
							case -5: // l'avversario si e' disconnesso
								ingame = 0;
								memset( &opponent_addr, 0, sizeof( opponent_addr ));
								printf( " %s si e' arreso: VITTORIA!! \n> ", opponent_username );
								break;	
							case -4: // utente sotto un'altra richiesta
								printf( " %s sta rispondendo ad un'altra sfida \n> ",opponent_username );
								break;
							case -3:// utente sfidato non e' disponibile a giocare
								printf( " %s gia' occupato in una partita \n> ", opponent_username );
								break; 
							case -2:// utente non esiste
								printf( " Impossibile connettersi a %s: utente inesistente \n> ", opponent_username );
								break;
							case -1: // l'utente sfidato ha rifiutato la partita
								printf( " %s ha rifutato la partita \n> ", opponent_username );
								break;
							case 1: // risposta al comando !who contenente la lista di client connnessi
								list_size = extract_int( cmd_buffer, 4);
								free( who_list );
								who_list = malloc(list_size);
								memcpy( who_list, cmd_buffer + 8, list_size);
								printf( " Client connessi al server: %s \n> ", who_list);
								break;
							case 2: // sei stato sfidato
								username_size = extract_int( cmd_buffer, 4);
								opponent_username = malloc(username_size);
								memcpy( opponent_username, cmd_buffer + 8, username_size );
								printf( " Sei stato sfidato da %s \n Premere (S) per accettare o (N) per rifiutare: ", opponent_username );
								do {
									scanf("%c", &y_n);
								} while ( y_n != 's' && y_n != 'S' && y_n != 'n' && y_n != 'N' );
								printf("> ");
								if ( y_n == 's' || y_n == 'S' ) 			
									cmd_id = 3; //sfida accettata
								else if ( y_n == 'n' || y_n == 'N') 
									cmd_id = -1; // sfida rifiutata 
								
								ret = set_pkt( &cmd_id, opponent_username, NULL, NULL, NULL ); 
								send_cmd( &ret, 0 );
								break;
							case 3: // l'utente sfidato ha accettato la partita
								printf( " %s ha accettato la partita \n", opponent_username );
								memset( &opponent_addr, 0, sizeof( opponent_addr ));
								opponent_addr.sin_family = AF_INET;
								opponent_addr.sin_port = htons( extract_int( cmd_buffer,  4 ));
								memcpy( ip, cmd_buffer + 8, INET_ADDRSTRLEN );
								//printf("[DEBUG] info avversario ricevute: ip %s, porta %d \n", ip, opponent_addr.sin_port );
								inet_pton( AF_INET, ip, &opponent_addr.sin_addr );	
								cmd_id = 0; // presentazione 
								ret = set_pkt( &cmd_id, NULL, &portUDP, NULL ,NULL );
								send_cmd( &ret, 1 );
								printf( " \n ### PARTITA CONTRO %s INIZIATA ### \n", opponent_username);
								ingame = 1;
								set_tokens();
								cmd_id = 1; // ready_to_play
								turn = 0;// lo sfidante inizia per secondo
								ret = set_pkt( &cmd_id, NULL, NULL, NULL, NULL); 
								send_cmd( &ret, 1 );
								printf( "\n# ");
								break;
							case 4: // VITTORIA
								printf( " `.`.`.`.` VITTORIA `.`.`.`.` \n Hai distrutto la flotta nemica!! \n> " );
								ingame = 0;
								free(opponent_username);
								my_hits = opponent_hits = 0;
								memset( &opponent_addr, 0, sizeof( opponent_addr ));
								break;
						} 
						fflush(stdout);
/**	*************** INTERAZIONE CON L'AVVERSARIO *************************/ 
					} if ( i == UDP_fd ) {
						ret = recv_response(1);
						if ( ret == -1 )
							break;
						
						response_id = extract_int( cmd_buffer, 0);
						//printf("[DEBUG] UDP fd richiede %d \n", response_id);
						switch( response_id ) { 
							case -1:
								printf( " %s dice: mancato :( \n", opponent_username );
								printf( " E' il turno di %s \n# ", opponent_username);
								opponent_grid[square[0]][square[1]] = MISS; 
								turn = 0; break;
							case 0: // lo sfidato riceve dallo sfidante il primo pacchetto udp
								printf( " \n ### PARTITA CONTRO %s INIZIATA ### \n", opponent_username);
								response_id = ingame = turn = 1; // rtp / in partita / lo sfidato inizia prima
								set_tokens();
								ret = set_pkt( &response_id, NULL, NULL, NULL, NULL ); 
								send_cmd( &ret, 1);
								break;
							case 1:
								printf(" %s ha posizionato le sue navi ed e' pronto a giocare \n", opponent_username );
								printf( "%s \n", HELP_MSG_INGAME );
								if ( turn == 0 ) 
									printf( " E' il turno di %s \n# ", opponent_username);
								else
									printf( " E' il tuo turno\n# ");	 
								opponent_rtp = 1; break;
							case 2: // ricezione colpo
								square[0] = extract_int( cmd_buffer, 4 ); // 2 | X |  Y |
								square[1] = extract_int( cmd_buffer, 8);
								switch ( my_grid[square[0]][square[1]] ) {
									case FREE:
										 my_grid[square[0]][square[1]] = MISS;
										 response_id = -1; 
										 printf( " %s spara in posizione %d,%d. Mancato :) \n", opponent_username, square[0]+1, square[1]+1); break;
									case BUSY:
										 my_grid[square[0]][square[1]] = HIT;
										 opponent_hits++;
										 response_id = 3; 
										 printf( " %s spara in posizione %d,%d. Colpito :(\n", opponent_username, square[0]+1, square[1]+1); break;
									default: break;
								}
								if ( opponent_hits == 7 ) {
								 	response_id = 4; // Vittoria
								   	printf( " ....... SCONFITTA ....... \n %s ha distrutto la tua flotta :( \n> ", opponent_username );
									ingame = 0;
									free(opponent_username);
									my_hits = opponent_hits = 0;
									ret = set_pkt( &response_id, NULL, NULL, NULL, NULL ); 
									send_cmd( &ret, 0 ); 
									break;
								} 
								turn = 1;
								ret = set_pkt( &response_id, NULL, NULL, NULL, NULL ); 
								send_cmd( &ret, 1 );
								printf( " E' il tuo turno\n# "); break;
							case 3:
								printf( " %s dice: Colpito! :) \n", opponent_username );
								opponent_grid[square[0]][square[1]] = HIT; 
								my_hits++;
								turn = 0; 
								printf( " E' il turno di %s \n# ", opponent_username); break;
						}
						fflush(stdout);
					}
				}
			}
		}
	}
	return 0;
}

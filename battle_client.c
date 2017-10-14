// battle_client.c

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>

#define PORT_SIZE 	 16
#define CMD_ID_SIZE  4
#define N_ROWS	  	 6
#define N_COLUMNS 	 6
#define N_BOATS	  	 7

enum cell_t  { BUSY, FREE, HIT, MISS };

enum cell_t  enemy_grid[N_ROWS][N_COLUMNS];
enum cell_t  my_grid[N_ROWS][N_COLUMNS];
unsigned enemy_hits;

char* cmd_buffer;

int set_packet( int cmd_id, char* username, int portUDP ) {
	int ret = 0; 
	if ( username == NULL ) {
		cmd_buffer = malloc(CMD_ID_SIZE);
		itoa(cmd_id, cmd_buffer, 10);
		ret = CMD_ID_SIZE;
	} else {
		int username_size = strlen(username) + 1;
		if ( portUDP == -1 ) {
			ret = CMD_ID_SIZE + strlen(username) + 1;
			cmd_buffer = malloc(ret);
		} else {
			ret = CMD_ID_SIZE + strlen(username) + 1 + PORT_SIZE;
			cmd_buffer = malloc(ret);	
			itoa( portUDP, cmd_buffer + CMD_ID_SIZE + username_size );
		}
		strcpy( cmd_buffer + CMD_ID_SIZE, username, username_size );
	}
	return ret;
}	

void send_cmd( int server_d, int* size ) {
	int ret;
	ret = send( server_d, (void*) size, sizeof(int), 0 );
	if ( ret < sizeof(size) ) {
		printf("[ERRORE] Invio dimensione comando \n");
		return;
	}

	ret = send( server_d, (void*) cmd_buffer, *(size), 0 );
	if ( ret < *(size) ) {
		pritf( "[ERRORE] Invio comando \n" );
		return;
	}
	free( cmd_buffer );
}


int main( int  argc, char** argv) {
	/*  int i,j;
    	for ( i = 0; i < N_ROWS; i++ ) {
    		for ( j = 0; j < N_COLUMNS; j++ ) {
    			my_grid[i][j] = FREE;
    			enemy_grid[i][j] = FREE;
    		}
    	}
    	printf( " Stato Casella in posizione (%d,%d): %d", 0, 0, my_grid[0][0] );
    */
	if ( argc != 3 ) {
		printf( "[ERRORE] Uso: ./battle_client.exe <host remoto> <porta> \n");
		exit(1);
	}

    int portUDP, ret, server_d, server_port;
	char username[30];
	struct sockaddr_in server_addr;

    memset( &server_addr, 0, sizeof( server_addr ));
   
	server_d = socket( AF_INET, SOCK_STREAM, 0 ); 
	server_port = atoi(argv[2]);
  	printf( "[DEBUG] porta server %d \n",  server_port);
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( server_port );
    inet_pton( AF_INET, argv[1], &server_addr.sin_addr );

    ret = connect( server_d, (struct sockaddr* ) &server_addr, sizeof( server_addr ));
    if ( ret == -1 ) {
        perror( "[ERRORE] Connessione al server \n");
        exit(1);
    }
        
    printf( "[INFO] Connessone al server %s ( porta %d ) avvenuta con successo \n", argv[1], server_port );
	
	printf( "Inserisci il tuo nome: " );
	scanf( "%s" , username );
	printf( "Inserisci la porta UDP di ascolto: ");
	scanf( "%d", portUDP );
	
	ret = set_buffer( 0, username, portUDP );
	
	send_command( server_d, &ret );

    return 0;
}

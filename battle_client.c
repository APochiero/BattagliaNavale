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

#define N_ROWS	   6
#define N_COLUMNS  6
#define N_BOATS	   7

enum cell_t  { BUSY, FREE, HIT, MISS };

enum cell_t  enemy_grid[N_ROWS][N_COLUMNS];
enum cell_t  my_grid[N_ROWS][N_COLUMNS];
unsigned enemy_hits;

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

    int ret, sd, server_port;
    struct sockaddr_in server_addr;

    memset( &server_addr, 0, sizeof( server_addr ));
   
	sd = socket( AF_INET, SOCK_STREAM, 0 ); 
	server_port = atoi(argv[2]);
  	printf( "[DEBUG] porta server %d \n",  server_port);
	server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( server_port );
    inet_pton( AF_INET, argv[1], &server_addr.sin_addr );

    ret = connect( sd, (struct sockaddr* ) &server_addr, sizeof( server_addr ));
    if ( ret == -1 ) {
        perror( "[ERRORE] Connessione al server \n");
        exit(1);
    }
        
    printf( "[INFO] Connessone al server %s ( porta %d ) avvenuta con successo \n", argv[1], server_port );

    return 0;
}

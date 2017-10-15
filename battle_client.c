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

#define 
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
char* buff_pointer;

void insert_buff( void* src, int n ) {
	memcpy( buff_pointer, src, n );
	buff_pointer += n;
}

int set_buffer( int* cmd_id, char* username, int* portUDP ) {
	int ret = sizeof(uint32_t);
	int user_size = 0;

	if ( username != NULL ) {
		user_size = strlen(username) + 1 ;
		ret += user_size + sizeof(uint32_t);
	}
	if ( portUDP != NULL ) 
		ret += sizeof(int32_t);
	cmd_buffer = malloc( ret );
	buff_pointer = cmd_buffer;
	insert_buff( cmd_id, sizeof(uint32_t)); 
	insert_buff( &user_size, sizeof(uint32_t));
	insert_buff( username, user_size );
	insert_buff( portUDP, sizeof(uint32_t));

	printf( "[DEBUG] cmd_buffer %x - %x\n", (unsigned int) cmd_buffer[0], (unsigned int) cmd_buffer[4]  );
	return ret;
}	

void send_cmd( int server_d, int* size ) {
	int ret;
	ret = send( server_d, (void*) size, sizeof(uint32_t), 0 ); //invio dimensione pacchetto
	if ( ret < sizeof(uint32_t) ) {
		printf("[ERRORE] Invio dimensione comando \n");
		return;
	}
	int s = *(size);
	printf("[DEBUG] bytes da inviare %d \n", s);
	ret = send( server_d, (void*) cmd_buffer, s, 0 );
	if ( ret < *(size) ) {
		printf( "[ERRORE] Invio comando \n" );
		return;
	}
	printf( "[DEBUG] bytes inviati %d \n", ret );
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

    int cmd_id, portUDP, ret, server_d, server_port;
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
	scanf( "%d", &portUDP );
	cmd_id = 0;
	ret = set_buffer( &cmd_id, username, &portUDP );
	send_cmd( server_d, &ret );
	
	recv_response( server_d );
	
    return 0;
}

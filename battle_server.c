// battle_server.c 

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

struct client_t {
	char* username;
	int portUDP;
	struct sockaddr_in cl_addr;
	int cl_sd;
};

const char* ip_addr = "127.0.0.1";
struct client_t* clients;


int main( int argc, char** argv ) {
	if ( argc != 2 ) {
		printf( "[ERRORE] Uso: ./battle_server.exe <porta> \n");
		exit(1);
	}

	int port, ret, sd, new_sd, len;
	struct sockaddr_in server_addr;
	
	port = atoi(argv[1]);
	printf( "[DEBUG] porta %d\n", port );
	sd = socket(AF_INET, SOCK_STREAM, 0);
	
	memset( &server_addr, 0 ,sizeof( server_addr ) );

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons( port );
	inet_pton( AF_INET, ip_addr, &server_addr.sin_addr);

	ret = bind( sd, (struct sockaddr*) &server_addr, sizeof(server_addr));
	if ( ret == -1 ) {
		perror( "Errore: " );
		exit(1);
	} 
	ret = listen(sd, 10 );
	if( ret == -1  ) {
		perror("Errore: ");
		exit(1);
	}
	printf( "[INFO] Server con indirizzo %s in ascolto sulla porta %d \n", ip_addr, port );   

}


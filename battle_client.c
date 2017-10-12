// battle_client.c

#include <stdio.h>

#define N_ROWS	   6
#define N_COLUMNS  6
#define N_BOATS	   7

enum cell_t  { BUSY, FREE, HIT, MISS };

enum cell_t  enemy_grid[N_ROWS][N_COLUMNS];
enum cell_t  my_grid[N_ROWS][N_COLUMNS];
unsigned enemy_hits;

int main() {
	int i,j;
	for ( i = 0; i < N_ROWS; i++ ) {
		for ( j = 0; j < N_COLUMNS; j++ ) {
			my_grid[i][j] = FREE;
			enemy_grid[i][j] = FREE;
		}
	}
	printf( " Stato Casella in posizione (%d,%d): %d", 0, 0, my_grid[0][0] );
}




#ifndef MYMYSOUND_H
#define MYMYSOUND_H

#define MAX_BUFFER 4

#define MAX_CHAR 64

#define MAX_CHUNK_SIZE 1024 << 3


typedef struct
{
	char			name[ MAX_CHAR ];

	OggVorbis_File	*file;

	vorbis_info		*info;

	MEMORY			*memory;

	unsigned int	bid[ MAX_BUFFER ];

} MYMYSOUNDBUFFER;


typedef struct
{
	char			name[ MAX_CHAR ];

	unsigned int	sid;

	int				loop;

	MYMYSOUNDBUFFER		*MYSOUNDbuffer;

} MYMYSOUND;


MYMYSOUNDBUFFER *MYSOUNDBUFFER_load( char *name, MEMORY *memory );

MYMYSOUNDBUFFER *MYSOUNDBUFFER_load_stream( char *name, MEMORY *memory );

unsigned char MYSOUNDBUFFER_decompress_chunk( MYSOUNDBUFFER *MYSOUNDbuffer, unsigned int buffer_index );

MYMYSOUNDBUFFER *MYSOUNDBUFFER_free( MYSOUNDBUFFER *MYSOUNDbuffer );

MYMYSOUND *MYSOUND_add( char *name, MYSOUNDBUFFER *MYSOUNDbuffer );

MYMYSOUND *MYSOUND_free( MYSOUND *MYSOUND );

void MYSOUND_play( MYSOUND *MYSOUND, int loop );

void MYSOUND_pause( MYSOUND *MYSOUND );

void MYSOUND_stop( MYSOUND *MYSOUND );

void MYSOUND_set_speed( MYSOUND *MYSOUND, float speed );

void MYSOUND_set_volume( MYSOUND *MYSOUND, float volume );

void MYSOUND_set_location( MYSOUND *MYSOUND, vec3 *location, float reference_distance );

void MYSOUND_rewind( MYSOUND *MYSOUND );

float MYSOUND_get_time( MYSOUND *MYSOUND );

int MYSOUND_get_state( MYSOUND *MYSOUND );

float MYSOUND_get_volume( MYSOUND *MYSOUND );

void MYSOUND_update_queue( MYSOUND *MYSOUND );

#endif

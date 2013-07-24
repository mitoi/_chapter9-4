/*

Book:      	Game and Graphics Programming for iOS and Android with OpenGL(R) ES 2.0
Author:    	Romain Marucchi-Foino
ISBN-10: 	1119975913
ISBN-13: 	978-1119975915
Publisher: 	John Wiley & Sons	

Copyright (C) 2011 Romain Marucchi-Foino

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of
this software. Permission is granted to anyone who either own or purchase a copy of
the book specified above, to use this software for any purpose, including commercial
applications subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that
you wrote the original software. If you use this software in a product, an acknowledgment
in the product would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be misrepresented
as being the original software.

3. This notice may not be removed or altered from any source distribution.

*/

#include "templateApp.h"

#define OBJ_FILE ( char * )"Scene.obj"

#define PHYSIC_FILE ( char * )"Scene.bullet"


OBJ *obj = NULL;

vec3 eye,
	 next_eye,
     up = { 0.0f, 0.0f, 1.0f };

OBJMESH *player = NULL;

THREAD *thread = NULL;

vec4 frustum[ 6 ];

int viewport_matrix[ 4 ];

FONT *font_small = NULL,
	 *font_big   = NULL;

float rotx	   = 40.0f,
	  rotz	   = 0.0f,
	  distance = 5.0f;

/* The current accelerometer X and Y values.
 * Because the application runs in landscape mode, X is used to go forward and backward,
 *  and Y is used to control the Z rotation of the camera. */
vec2 accelerometer = { 0.0f, 0.0f };

/* The next accelerometer values to linearly interpolate to.
 *  The values delivered by the accelerometer can be quite jumpy, so we need to use linear interpolation*/
vec2 next_accelerometer = { 0.0f, 0.0f };

/* The maximum speed of the ball. */
float ball_speed  = 10.0f,
		/* The sensitivity of the accelerometer. */
      sensitivity = 1.0f;

/* The current game state. 0 indicates that the game is running, 1 indicates that the level is cleared,
 * and 2 indicates that the level has to be reloaded. We will be implementing the logic code later,
 * but for now, weÕll declare the game state to be able to determine if the player is able to control
 * the ball or not based on the current game state. */
unsigned char game_state = 0;

/* The current time spent inside the level. */
float game_time  = 0.0f,
		/* Boost the reference distance of the positional sound sources of the gems
		 * (since the gem radius is pretty small). */
      gem_factor = 20.0f;
/* Total points gathered in the level so far. */
unsigned int gem_points = 0;

//the background sound music
SOUNDBUFFER *background_soundbuffer = NULL;
SOUND *background_sound = NULL;

//to use with the four type of gems red, blue, green and yellow
SOUNDBUFFER *gems_soundbuffer[ 4 ];
SOUND *gems_sound[ 4 ];

SOUNDBUFFER *water_soundbuffer = NULL;
SOUND *water_sound = NULL;

SOUNDBUFFER *lava_soundbuffer = NULL;
SOUND *lava_sound = NULL;

SOUNDBUFFER *toxic_soundbuffer = NULL;
SOUND *toxic_sound = NULL;


TEMPLATEAPP templateApp = { templateAppInit,
							templateAppDraw,
							templateAppToucheBegan,
							NULL,
							NULL,
							templateAppAccelerometer };


btSoftBodyRigidBodyCollisionConfiguration *collisionconfiguration = NULL;

btCollisionDispatcher *dispatcher = NULL;

btBroadphaseInterface *broadphase = NULL;

btConstraintSolver *solver = NULL;

btSoftRigidDynamicsWorld *dynamicsworld = NULL;


void init_physic_world( void )
{
	collisionconfiguration = new btSoftBodyRigidBodyCollisionConfiguration();

	dispatcher = new btCollisionDispatcher( collisionconfiguration );

	broadphase = new btDbvtBroadphase();

	solver = new btSequentialImpulseConstraintSolver();

	dynamicsworld = new btSoftRigidDynamicsWorld( dispatcher,	
												  broadphase,
												  solver,
												  collisionconfiguration );

	dynamicsworld->setGravity( btVector3( 0.0f, 0.0f, -9.8f ) );
}


void load_physic_world( void )
{
	btBulletWorldImporter *btbulletworldimporter = new btBulletWorldImporter( dynamicsworld );

	MEMORY *memory = mopen( PHYSIC_FILE, 1 );

	btbulletworldimporter->loadFileFromMemory( ( char * )memory->buffer, memory->size );

	mclose( memory );

	unsigned int i = 0;

	while( i != btbulletworldimporter->getNumRigidBodies() ) { 

		OBJMESH *objmesh = OBJ_get_mesh( obj,
										 btbulletworldimporter->getNameForPointer(
										 btbulletworldimporter->getRigidBodyByIndex( i ) ), 0 );
										 
		if( objmesh ) { 

			objmesh->btrigidbody = ( btRigidBody * )btbulletworldimporter->getRigidBodyByIndex( i );
			
			objmesh->btrigidbody->setUserPointer( objmesh );
		} 

		++i; 
	} 

	delete btbulletworldimporter;
}


void free_physic_world( void )
{
	while( dynamicsworld->getNumCollisionObjects() )
	{
		btCollisionObject *btcollisionobject = dynamicsworld->getCollisionObjectArray()[ 0 ];
		
		btRigidBody *btrigidbody = btRigidBody::upcast( btcollisionobject );

		if( btrigidbody )
		{
			delete btrigidbody->getCollisionShape();
			
			delete btrigidbody->getMotionState();
			
			dynamicsworld->removeRigidBody( btrigidbody );
			
			dynamicsworld->removeCollisionObject( btcollisionobject );
			
			delete btrigidbody;
		}
	}
	
	delete collisionconfiguration; collisionconfiguration = NULL;

	delete dispatcher; dispatcher = NULL;

	delete broadphase; broadphase = NULL;

	delete solver; solver = NULL;
	
	delete dynamicsworld; dynamicsworld = NULL;	
}


void program_bind_attrib_location( void *ptr ) {

	PROGRAM *program = ( PROGRAM * )ptr;

	glBindAttribLocation( program->pid, 0, "POSITION"  );
	glBindAttribLocation( program->pid, 2, "TEXCOORD0" );
}


void program_draw( void *ptr )
{
	PROGRAM *program = ( PROGRAM * )ptr;
	
	unsigned int i = 0;
		
	while( i != program->uniform_count )
	{
		if( !program->uniform_array[ i ].constant &&
			!strcmp( program->uniform_array[ i ].name, "DIFFUSE" ) )
		{
			glUniform1i( program->uniform_array[ i ].location, 1 );
		
			program->uniform_array[ i ].constant = 1;
		}

		else if( !strcmp( program->uniform_array[ i ].name, "MODELVIEWPROJECTIONMATRIX" ) )
		{
			glUniformMatrix4fv( program->uniform_array[ i ].location,
								1,
								GL_FALSE,
								( float * )GFX_get_modelview_projection_matrix() );		
		}
		
		/* Check if you are dealing with the TEXTUREMATRIX uniform. */
		else if( !strcmp( program->uniform_array[ i ].name, "TEXTUREMATRIX" ) ) { 

		   static vec2 scroll = { 0.0f, 0.0f };

		   GFX_set_matrix_mode( TEXTURE_MATRIX );

		   GFX_push_matrix();

		   scroll.x += 0.0025f;
		   scroll.y += 0.0025f;

		   GFX_translate( scroll.x, scroll.y, 0.0f );

		   glUniformMatrix4fv( program->uniform_array[ i ].location,
							   1,
							   GL_FALSE,
							   ( float * )GFX_get_texture_matrix() );

		   GFX_pop_matrix();

		   GFX_set_matrix_mode( MODELVIEW_MATRIX ); 
		}		

		++i;
	}
}


class ClosestNotMeRayResultCallback:public btCollisionWorld::ClosestRayResultCallback { 

	public:
		ClosestNotMeRayResultCallback( btRigidBody *rb,
									   const btVector3 &p1,
									   const btVector3 &p2 ) :
		btCollisionWorld::ClosestRayResultCallback( p1, p2 )
		{ m_btRigidBody = rb; }

	virtual btScalar addSingleResult( btCollisionWorld::LocalRayResult &localray,
									  bool normalinworldspace )
	{ 
		if( localray.m_collisionObject == m_btRigidBody )
		{ return 1.0f; }
		
		return ClosestRayResultCallback::addSingleResult( localray, normalinworldspace );
	}

	protected:
		btRigidBody *m_btRigidBody;
};


bool contact_added_callback( btManifoldPoint &btmanifoldpoint,
							 const btCollisionObject *btcollisionobject0,
							 int part_0, int index_0,
							 const btCollisionObject *btcollisionobject1,
							 int part_1, int index_1 ) {

	OBJMESH *objmesh0 = ( OBJMESH * )( ( btRigidBody * )btcollisionobject0 )->getUserPointer();

	OBJMESH *objmesh1 = ( OBJMESH * )( ( btRigidBody * )btcollisionobject1 )->getUserPointer();
	
	/* If one of the mesh object names is like Òlevel_clearÓ it means that the ball has reached the end target.
	 * Set the game state to 1 to indicate that the level has to be restarted. */
	if( ( strstr( objmesh0->name, "level_clear" ) || 
		  strstr( objmesh1->name, "level_clear" ) ) )
		game_state = 1;
	/* If the two mesh objects that are involved in the collision are a gem and the player. */
	else if( ( strstr( objmesh0->name, "player" ) || 
			   strstr( objmesh1->name, "player" ) )
			&&
			 ( strstr( objmesh0->name, "gem" ) || 
			   strstr( objmesh1->name, "gem" ) ) ) { 
		/* To store the gem mesh pointer and its collision object. */
		OBJMESH *objmesh = NULL;
		btCollisionObject *btcollisionobject = NULL;

		/* Depending on which mesh (either 0 or 1) is the gem, store the appropriate pointers. */
		if( strstr( objmesh0->name, "gem" ) ) {

			objmesh = objmesh0;
			btcollisionobject = ( btCollisionObject * )btcollisionobject0;
		}
		else { 
		
			objmesh = objmesh1;
			btcollisionobject = ( btCollisionObject * )btcollisionobject1;
		}

		/* Temporary variable to store the gem index for the sound source based
		 *  on the name of the current gem that gets picked up by the player. */
		unsigned char index = 0;
		/* If itÕs a red gem, add one gem point and store the index number 0. */
		if( strstr( objmesh->name, "red" ) ) { 
			
			gem_points += 1;
			index = 0;
		}
		else if( strstr( objmesh->name, "green" ) ) { 

			gem_points += 2;
			index = 1;
		}
		
		else if( strstr( objmesh->name, "blue" ) ) { 
			
			gem_points += 3;
			index = 2;
		}
		
		else if( strstr( objmesh->name, "yellow" ) ) { 
		
			gem_points += 4;
			index = 3;
		}

		/*Set the location of the sound source for the appropiate gem picked up and use the current location
		 * and the radius of the mesh to modify the sound source location and reference distance*/
		SOUND_set_location( gems_sound[ index ],
							&objmesh->location,
							/* Gems have a small radius. We will give it a boost so the player will hear the
							 * sound more clearly. */
							objmesh->radius * gem_factor );
		/* Play the sound source. */
		SOUND_play( gems_sound[ index ], 0 );
		/* Set the current gem mesh to be invisible.
		 * It has been picked up, so you donÕt want to draw it again. */
		objmesh->visible = 0;
		/* Remove the rigid body and associated data from the physical world.
		 * When a gem is picked, it cannot be part of the physics
		 * simulation any more. */
		delete objmesh->btrigidbody->getCollisionShape();

		delete objmesh->btrigidbody->getMotionState();

		dynamicsworld->removeRigidBody( objmesh->btrigidbody );

		dynamicsworld->removeCollisionObject( btcollisionobject );

		delete objmesh->btrigidbody;

		objmesh->btrigidbody = NULL;
	}		 

	return false;
}


void decompress_stream( void *ptr )
{
	SOUND_update_queue( background_sound );	
}


void load_level( void )
{
	obj = OBJ_load( OBJ_FILE, 1 );

	unsigned int i = 0;

	while( i != obj->n_objmesh ) {
	
		OBJ_optimize_mesh( obj, i, 128 );

		OBJ_build_mesh( obj, i );
		
		OBJ_free_mesh_vertex_data( obj, i );

		++i;
	}
	
	
	init_physic_world();
	
	load_physic_world();
	
	gContactAddedCallback = contact_added_callback;
	
	/* Get the mesh object name level_clear, which is the cylinder located
	 * in the middle of the level end target. */
   OBJMESH *level_clear = OBJ_get_mesh( obj, "level_clear", 0 );

   /* On top of the usual CF_CUSTOM_MATERIAL_CALLBACK collision flag,
   add CF_NO_CONTACT_RESPONSE. This collision flag makes your rigid
   body object act like a ghost, meaning that it will not respond to collision.
   This can be used to turn off the collision response of any rigid body,
    which is great for this typical scenario, where you only want the object to trigger the callback. */
   level_clear->btrigidbody->setCollisionFlags( level_clear->btrigidbody->getCollisionFlags()  |
												btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK |
												btCollisionObject::CF_NO_CONTACT_RESPONSE );

   /* Make the level_clear mesh invisible for rendering. */
   level_clear->visible = 0;

   i = 0;
   while( i != obj->n_objmesh ) { 

      OBJMESH *objmesh = &obj->objmesh[ i ];

      if( strstr( objmesh->name, "gem" ) ) { 

         objmesh->rotation.z = ( float )( random() % 360 );

         objmesh->btrigidbody->setCollisionFlags( objmesh->btrigidbody->getCollisionFlags() |
												  btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK );
      }
	  
	  ++i;
   }	
	
	
	player = OBJ_get_mesh( obj, "player", 0 );
	
	player->btrigidbody->setFriction( 10.0f );
	
	memcpy( &eye, &player->location, sizeof( vec3 ) );
	
	
	i = 0;
	while( i != obj->n_texture ) { 

		OBJ_build_texture( obj,
						   i,
						   obj->texture_path,
						   TEXTURE_MIPMAP | TEXTURE_16_BITS,
						   TEXTURE_FILTER_2X,
						   0.0f );
		++i;
	}


	i = 0;
	while( i != obj->n_program ) { 

		OBJ_build_program( obj,
						   i,
						   program_bind_attrib_location,
						   program_draw,
						   1,
						   obj->program_path );
		++i;
	}


	i = 0;
	while( i != obj->n_objmaterial ) { 

		OBJ_build_material( obj, i, NULL );
		
		++i;
	}	

	

	font_small = FONT_init( ( char * )"foo.ttf" );

	FONT_load( font_small,
			   font_small->name,
			   1,
			   24.0f,
			   512,
			   512,
			   32,
			   96 );
			   

	font_big = FONT_init( ( char * )"foo.ttf" );

	FONT_load( font_big,
			   font_big->name,
			   1,
			   48.0f,
			   512,
			   512,
			   32,
			   96 );
			   
	/* Declare a memory structure that you will use (and reuse) to load each sound buffer. */
	MEMORY *memory = NULL;
	/* Reset the counter. */
	i = 0;
	while( i != 4 ) {
	
		switch( i ) { 
		
			case 0: {
				/* Load the red.ogg file. */
				memory = mopen( ( char * )"red.ogg", 1 );
				break;
			}
			case 1: {
				/* Load the green.ogg file. */
				memory = mopen( ( char * )"green.ogg", 1 );
				break;
			}
			case 2: {
				/* Load the blue.ogg file. */
				memory = mopen( ( char * )"blue.ogg", 1 );
				break;
			}
			case 3: {
				/* Load the yellow.ogg file. */
				memory = mopen( ( char * )"yellow.ogg", 1 );
				break;
			}
		}

		/* For the current gem buffer index, create a sound buffer using the content
		 * of the memory structure that we have loaded. */
		gems_soundbuffer[ i ] = SOUNDBUFFER_load( ( char * )"gem", memory );

		mclose( memory );
		
		/* Create a new sound source for the current index and link the current sound buffer. */
		gems_sound[ i ] =  SOUND_add( ( char * )"gem", gems_soundbuffer[ i ] );

		SOUND_set_volume( gems_sound[ i ], 1.0f );

		++i;
	}
	
	/* Temporary variable to contain the mesh pointer of the object that will be emitting the sound. */
	OBJMESH *objmesh = NULL;
	/* Load the water.ogg file in memory. */
	memory = mopen( ( char * )"water.ogg", 1 );

	water_soundbuffer = SOUNDBUFFER_load( ( char * )"water", memory );

	/* Free the memory , because the sound buffer is loaded as a static sound and the raw audio buffer has been transferred to the
	audio memory by the previous function call. */
	mclose( memory );

	/* Create the water sound source. */
	water_sound = SOUND_add( ( char * )"water", water_soundbuffer );

	/* Here comes the part of code where we are going to associate the
	sound source to the object. First, get the objmesh pointer for the water object. */
	objmesh = OBJ_get_mesh( obj, "water", 0 );

	/* Assign to the sound source the location of the mesh in 3D space and
	use the radius as the reference distance (how far the sound can be heard). */
	SOUND_set_location( water_sound,
						&objmesh->location,
						objmesh->radius );

	/* Set the volume of the water at 50%. */
	SOUND_set_volume( water_sound, 0.5f );

	SOUND_play( water_sound, 1 );

	//the same as above for the lava sound
	memory = mopen( ( char * )"lava.ogg", 1 );
	
	lava_soundbuffer = SOUNDBUFFER_load( ( char * )"lava", memory );
	
	mclose( memory );
	
	lava_sound = SOUND_add( ( char * )"lava", lava_soundbuffer );

	objmesh = OBJ_get_mesh( obj, "lava", 0 );

	SOUND_set_location( lava_sound,
						&objmesh->location, 
						objmesh->radius );
						
	SOUND_set_volume( lava_sound, 0.5f );

	SOUND_play( lava_sound, 1 );

	//finally for the toxic waste sound
	memory = mopen( ( char * )"toxic.ogg", 1 );
	
	toxic_soundbuffer = SOUNDBUFFER_load( ( char * )"toxic", memory );

	mclose( memory );

	toxic_sound = SOUND_add( ( char * )"toxic", toxic_soundbuffer );
	
	objmesh = OBJ_get_mesh( obj, "toxic", 0 );

	SOUND_set_location( toxic_sound,
						&objmesh->location, 
						objmesh->radius );
	
	SOUND_set_volume( toxic_sound, 0.5f );
	
	SOUND_play( toxic_sound, 1 );

	/* Load the background sound as a streamed buffer. */
	memory = mopen( ( char * )"background.ogg", 1 );

	background_soundbuffer = SOUNDBUFFER_load_stream( ( char * )"background", memory );

	background_sound = SOUND_add( ( char * )"background", background_soundbuffer );
	
	SOUND_set_volume( background_sound, 0.5f );
	/* Play the background sound in a loop. */
	SOUND_play( background_sound, 1 );

	//Start the decompression thread
	THREAD_play( thread );

}


void free_level( void )
{
	gem_points =
	game_state = 0;
	game_time  = 0.0f;


	unsigned int i = 0;
	/* Pause the thread, because we donÕt want the process to
	 * continue to decompress while we are trying to free the background music.
	 * That would make the app to crash for sure.
	 */
	THREAD_pause( thread );   
	/* Free the background sound source. */
	background_sound = SOUND_free( background_sound );
	/* Because the background music was streamed, free the sound buffer from
	the local memory structure. */
	background_soundbuffer->memory = mclose( background_soundbuffer->memory );
	/* It is now okay to free the sound buffer. */
	background_soundbuffer = SOUNDBUFFER_free( background_soundbuffer );

	while( i != 4 ) {
		/* Free the sound source and the associated buffer. */
		gems_sound[ i ] = SOUND_free( gems_sound[ i ] );
		gems_soundbuffer[ i ] = SOUNDBUFFER_free( gems_soundbuffer[ i ] );
	
		++i;
	}
	/* Now deal with the water, lava, and toxic stuff sound sources and buffers. */
	water_sound = SOUND_free( water_sound );
	water_soundbuffer = SOUNDBUFFER_free( water_soundbuffer );

	lava_sound = SOUND_free( lava_sound );
	lava_soundbuffer = SOUNDBUFFER_free( lava_soundbuffer );

	toxic_sound = SOUND_free( toxic_sound );
	toxic_soundbuffer = SOUNDBUFFER_free( toxic_soundbuffer );
   
	player = NULL;

	THREAD_pause( thread );	

	font_small = FONT_free( font_small );
	
	font_big = FONT_free( font_big );
	
	free_physic_world();

	obj = OBJ_free( obj );
}


void templateAppInit( int width, int height ) {

	atexit( templateAppExit );

	GFX_start();
	
	AUDIO_start();

	thread = THREAD_create( decompress_stream, NULL, THREAD_PRIORITY_NORMAL, 1 );

	glViewport( 0.0f, 0.0f, width, height );
	
	glGetIntegerv( GL_VIEWPORT, viewport_matrix );

	srandom( get_milli_time() );

	load_level();
}


void templateAppDraw( void ) {

	if( game_state == 2 ) {

		free_level();
		load_level();
	}
   
	glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
	glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );

	GFX_set_matrix_mode( PROJECTION_MATRIX );
	GFX_load_identity();
	
	GFX_set_perspective( 80.0f,
						 ( float )viewport_matrix[ 2 ] / ( float )viewport_matrix[ 3 ],
						 0.1f,
						 50.0f,
						 -90.0f );

	GFX_set_matrix_mode( MODELVIEW_MATRIX );
	GFX_load_identity();

	/* Linearly interpolate the accelerometer values to get a smooth transition. */
	next_accelerometer.x = accelerometer.x * 0.1f + next_accelerometer.x * 0.9f;
	next_accelerometer.y = accelerometer.y * 0.1f + next_accelerometer.y * 0.9f;

	/* Assign the current Y rotation of the accelerometer to the Z rotation of the camera,
	 * multiplied by the accelerometer sensitivity factor. */
	rotz += next_accelerometer.y * sensitivity;

	/* The forward vector of the ball. */
	vec3 forward = { 0.0f, 1.0f, 0.0f },
			/* The current direction vector of the ball. Basically, this is the
			forward vector rotated by the cameraÕs Z rotation. */
		 direction;

	/* If the game is running, let the user move the ball. */
	if( !game_state ) {
		/* Pre-calculate a few variables before rotating the forward vector by the cameraÕs Z rotation. */
		float r = rotz * DEG_TO_RAD,
			  c = cosf( r ),
			  s = sinf( r );

		/* Rotate the forward vector and store the result into the
		direction variable. Because both vectors are already normalized,
		thereÕs no need to re-normalize them again. */
		direction.x = c * forward.y - s * forward.x;
		direction.y = s * forward.y + c * forward.x;

		float speed = CLAMP( ( -next_accelerometer.x * sensitivity ) * ball_speed,
							   -ball_speed,
							    ball_speed );

		/* Assign the direction vector multiplied by the current speed to the
		angular velocity of the ball. */
		player->btrigidbody->setAngularVelocity( btVector3( direction.x * speed,
															direction.y * speed,
														    0.0f ) );
		/* Activate the rigid body to make sure that the angular velocity
		will be applied. */
		player->btrigidbody->setActivationState( ACTIVE_TAG );
	}

	next_eye.x = player->location.x + 
				 distance * 
				 cosf( rotx * DEG_TO_RAD ) * 
				 sinf( rotz * DEG_TO_RAD );

	next_eye.y = player->location.y - 
				 distance *
				 cosf( rotx * DEG_TO_RAD ) *
				 cosf( rotz * DEG_TO_RAD );

	next_eye.z = player->location.z +
				 distance *
				 sinf( rotx * DEG_TO_RAD );

	player->location.z += player->dimension.z;

	btVector3 p1( player->location.x,
				  player->location.y,
				  player->location.z ),

			  p2( next_eye.x,
				  next_eye.y,
				  next_eye.z );

	ClosestNotMeRayResultCallback back_ray( player->btrigidbody,
											p1,
											p2 );

	dynamicsworld->rayTest( p1,
							p2,
							back_ray );

	if( back_ray.hasHit() ) { 

		back_ray.m_hitNormalWorld.normalize();

		next_eye.x =  back_ray.m_hitPointWorld.x() +
					( back_ray.m_hitNormalWorld.x() * 0.1f );

		next_eye.y =  back_ray.m_hitPointWorld.y() +
					( back_ray.m_hitNormalWorld.y() * 0.1f );

		next_eye.z =  back_ray.m_hitPointWorld.z() +
					( back_ray.m_hitNormalWorld.z()* 0.1f );
	}

	eye.x = next_eye.x * 0.05f + eye.x * 0.95f;
	eye.y = next_eye.y * 0.05f + eye.y * 0.95f;
	eye.z = next_eye.z * 0.05f + eye.z * 0.95f;

	/* Calculate the direction vector from the player to the current eye location. */
	vec3_diff( &direction, &player->location, &eye );
	/* Normalize the direction vector. */
	vec3_normalize( &direction, &direction );

	AUDIO_set_listener( &eye, &direction, &up );


	GFX_look_at( &eye,
				 &player->location,
				 &up );
	
	build_frustum( frustum,
				  GFX_get_modelview_matrix(),
				  GFX_get_projection_matrix() );	


	unsigned int i = 0;

	while( i != obj->n_objmesh ) {

		OBJMESH *objmesh = &obj->objmesh[ i ];

		objmesh->distance = sphere_distance_in_frustum( frustum, 
														&objmesh->location,
														objmesh->radius );

		if( objmesh->distance && objmesh->visible )
		{
			GFX_push_matrix();
			/* Check if the current objmesh name contains Ògem.Ó
			 * If yes, donÕt ask Bullet for the transformation matrix and handle the position and
			 * rotation manually. */
			if( strstr( objmesh->name, "gem" ) ) {

				GFX_translate( objmesh->location.x, 
							   objmesh->location.y, 
							   objmesh->location.z );
				
				objmesh->rotation.z += 1.0f;

				GFX_rotate( objmesh->rotation.z, 0.0f, 0.0f, 1.0f );
			}
			
			else if( objmesh->btrigidbody )
			{
				mat4 mat;

				objmesh->btrigidbody->getWorldTransform().getOpenGLMatrix( ( float * )&mat );
				
				memcpy( &objmesh->location, ( vec3 * )&mat.m[ 3 ], sizeof( vec3 ) );

				GFX_multiply_matrix( &mat );				
			}
			else
			{
				GFX_translate( objmesh->location.x, 
							   objmesh->location.y, 
							   objmesh->location.z );
			}

			OBJ_draw_mesh( obj, i );

			GFX_pop_matrix();
		}
		
		++i;
	}
	
	dynamicsworld->stepSimulation( 1.0f / 60.0f );

	
	GFX_set_matrix_mode( PROJECTION_MATRIX );
	GFX_load_identity();

	float half_width  = ( float )viewport_matrix[ 2 ] * 0.5f,
		  half_height = ( float )viewport_matrix[ 3 ] * 0.5f;

	GFX_set_orthographic_2d( -half_width,
							  half_width,
						     -half_height,
							  half_height );

	GFX_rotate( -90.0f, 0.0f, 0.0f, 1.0f );

	GFX_translate( -half_height, -half_width, 0.0f );

	GFX_set_matrix_mode( MODELVIEW_MATRIX );

	GFX_load_identity();
	
	vec4 font_color = { 0.0f, 0.0f, 0.0f, 1.0f };

	char gem_str  [ MAX_CHAR ] = {""},
		 time_str [ MAX_CHAR ] = {""},
		 level_str[ MAX_CHAR ] = {""};

	if( game_state ) { 
	
		sprintf( level_str, "Level Clear!" );

		FONT_print( font_big,
					viewport_matrix[ 3 ] * 0.5f - FONT_length( font_big, level_str ) * 0.5f + 4.0f,
					viewport_matrix[ 2 ] - font_big->font_size * 1.5f - 4.0f,
					level_str,
					&font_color );

		/* Yellow. */
		font_color.x = 1.0f;
		font_color.y = 1.0f;
		font_color.z = 0.0f;

		FONT_print( font_big,
					viewport_matrix[ 3 ] * 0.5f - FONT_length( font_big, level_str ) * 0.5f,
					viewport_matrix[ 2 ] - font_big->font_size * 1.5f,
		level_str,
		&font_color );   
	}

	font_color.x = 0.0f;
	font_color.y = 0.0f;
	font_color.z = 0.0f;

	sprintf( gem_str, "Gem Points:%02d", gem_points );
	sprintf( time_str, "Game Time:%02.2f", game_time * 0.1f );

	FONT_print( font_small,
				viewport_matrix[ 3 ] - FONT_length( font_small, gem_str ) - 6.0f,
				( font_small->font_size * 0.5f ),
				gem_str,
				&font_color );
	
	FONT_print( font_small,
				8.0f,
				( font_small->font_size * 0.5f ),
				time_str,
				&font_color );

	font_color.x = 1.0f;
	font_color.y = 1.0f;
	font_color.z = 0.0f;

	FONT_print( font_small,
				viewport_matrix[ 3 ] - FONT_length( font_small, gem_str ) - 8.0f,
				( font_small->font_size * 0.5f ),
				gem_str,
				&font_color );
	
	FONT_print( font_small,
				6.0f,
				( font_small->font_size * 0.5f ),
				time_str,
				&font_color );

	if( !game_state ) game_time += SOUND_get_time( background_sound );
}


void templateAppToucheBegan( float x, float y, unsigned int tap_count )
{
	if( game_state == 1 && tap_count >= 2 ) game_state = 2;

	if(game_state == 0)
	{
		if(tap_count == 1)
		{
			ball_speed = 30.0f;
		}

		if(tap_count == 2)
		{
			ball_speed = 10.0f;
		}
	}
}


void templateAppAccelerometer( float x, float y, float z )
{
	/* Store and normalize the XYZ value of the accelerometer.
	 * These values differ from iOS to Android, so working with a normalized vector will keep things easier. */
	vec3 tmp = { x, y, z };
	
	vec3_normalize( &tmp, &tmp );

	/* Add a little offset to the X axis of the accelerometer.
	 * The 0 position typically means that the device is fully flat, but because this game
	 * is in landscape mode and the player is holding the device in their hands,
	 * the 0 position should be slightly inclined. */
	accelerometer.x = tmp.x + 0.75f;

	/*The 0 value on the Y axis differs from iOs to Android on some device. To compensate for this
	 * difference on the Y axis of the accelerometer, check which platform we are
	 *  dealing with and adjust it accordingly. */
	#ifndef __IPHONE_4_0 /* Valid for every iOS version greater or equal to 4.0 */
	  accelerometer.y = tmp.y + 0.35f;
	#else
	  accelerometer.y = tmp.y;
	#endif
}


void templateAppExit( void ) {
	
	free_level();

	thread = THREAD_free( thread );
	
	AUDIO_stop();
}

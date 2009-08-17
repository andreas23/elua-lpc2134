#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "type.h"
#include "devman.h"
#include "platform.h"
#include "romfs.h"
#include "xmodem.h"
#include "shell.h"
#include "lua.h"
#include "term.h"
#include "platform_conf.h"
#ifdef ELUA_SIMULATOR
#include "hostif.h"
#endif

// Validate eLua configuratin options
#include "validate.h"

extern char etext[];


void boot_remote( void )
{
  lua_State *L = lua_open();
  luaL_openlibs(L);  /* open libraries */
  lua_getglobal( L, "rpc" );
  lua_getfield( L, -1, "server" );
  lua_pcall( L, 0, 0, 0 );
}

// ****************************************************************************
//  Program entry point

int main( void )
{
  FILE* fp;
  
  // Initialize platform first
  if( platform_init() != PLATFORM_OK )
  {
    // This should never happen
    while( 1 );
  }
  
  // Initialize device manager
  dm_init();
  
  // Register the ROM filesystem
  dm_register( romfs_init() );  

  // Autorun: if "autorun.lua" is found in the ROM file system, run it first
  if( ( fp = fopen( "/rom/autorun.lua", "r" ) ) != NULL )
  {
    fclose( fp );
    char* lua_argv[] = { "lua", "/rom/autorun.lua", NULL };
    lua_main( 2, lua_argv );    
  }
  
#ifdef ELUA_BOOT_REMOTE
  boot_remote();
#else
  
  // Run the shell
  if( shell_init() == 0 )
  {
    printf( "Unable to initialize the eLua shell!\n" );
    // Start Lua directly
    char* lua_argv[] = { "lua", NULL };
    lua_main( 1, lua_argv );
  }
  else
    shell_start();
#endif // #ifdef ELUA_BOOT_REMOTE

#ifdef ELUA_SIMULATOR
  hostif_exit(0);
  return 0;
#else
  while( 1 );
#endif
}

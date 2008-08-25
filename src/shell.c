// eLua shell

#include "shell.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "xmodem.h"
#include "version.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "platform.h"

#include "build.h"
#ifdef BUILD_SHELL

// Shell alternate ' ' char
#define SHELL_ALT_SPACE           '\x07'

// Shell command handler function
typedef void( *p_shell_handler )( char* args );

// Command/handler pair structure
typedef struct
{
  const char* cmd;
  p_shell_handler handler_func;
} SHELL_COMMAND;

// Shell data
static char* shell_prog;
static unsigned shell_max_prog;

// ****************************************************************************
// Shell functions

// 'help' handler
static void shell_help( char* args )
{
  args = args;
  printf( "Shell commands:\n" );
  printf( "  help - print this help\n" );
  printf( "  lua [args] - run Lua with the given arguments\n" );
  printf( "  recv - receive a file (XMODEM) and execute it\n" );
  printf( "  ver - print eLua version\n" );
  printf( "  exit - exit from this shelll\n" );
}

// 'lua' handler
static void shell_lua( char* args )
{
  int nargs = 0;
  char* lua_argv[ SHELL_MAX_LUA_ARGS + 2 ];
  char *p, *prev, *temp;
  
  lua_argv[ 0 ] = "lua";
  // Process "args" if needed
  if( *args )
  {
    prev = args;
    p = strchr( args, ' ' );
    while( p )
    {
      if( nargs == SHELL_MAX_LUA_ARGS )
      {
        printf( "Too many arguments to 'lua' (maxim %d)\n", SHELL_MAX_LUA_ARGS );
        return;
      }
      *p = 0;
      lua_argv[ nargs + 1 ] = temp = prev;
      nargs ++;
      prev = p + 1;
      p = strchr( p + 1, ' ' );
      // If the argument is quoted, remove the quotes and transform the 'alternate chars' back to space
      if( *temp == '\'' || *temp == '"' )
      {
        temp ++;
        lua_argv[ nargs ] = temp;
        while( *temp )
        {
          if( *temp == SHELL_ALT_SPACE )
            *temp = ' ';
          temp ++;
        }
        *( temp - 1 ) = '\0';
      }
    }
  }
  lua_argv[ nargs + 1 ] = NULL;
  printf( "Press CTRL+Z to exit Lua\n" );
  lua_main( nargs + 1, lua_argv );  
  clearerr( stdin );
}

// 'recv' handler
static void shell_recv( char* args )
{
  args = args;
  
#ifndef BUILD_XMODEM  
  printf( "XMODEM support not compiled, unable to recv\n" );
  return;
#else // #ifndef BUILD_XMODEM

  char *p;
  long actsize;
  lua_State* L;   

  if( ( shell_prog = malloc( shell_max_prog ) ) == NULL )
  {
    printf( "Unable to allocate memory\n" );
    return;
  }
  printf( "Waiting for file ... " );
  while( 1 )
  {
    actsize = xmodem_receive( shell_prog, shell_max_prog );
    if( actsize > 0 )
      break;
    if( actsize == XMODEM_ERROR_OUTOFMEM )
    {
      printf( "file too big\n ... " ); 
      printf( "Waiting for file ... " );
    }
  }
  // Eliminate the XMODEM padding bytes
  p = shell_prog + actsize - 1;
  while( *p == '\x1A' )
    p --;
  p ++;
  printf( "done, got %ld bytes\n", p - shell_prog );          
  
  // Execute
  if( ( L = lua_open() ) == NULL )
  {
    printf( "Unable to create Lua state\n" );
    free( shell_prog );
    shell_prog = NULL;
    return;
  }
  luaL_openlibs( L );
  if( luaL_loadbuffer( L, shell_prog, p - shell_prog, "xmodem" ) != 0 )
    printf( "Error: %s\n", lua_tostring( L, -1 ) );
  else
    if( lua_pcall( L, 0, LUA_MULTRET, 0 ) != 0 )
      printf( "Error: %s\n", lua_tostring( L, -1 ) );
  lua_close( L );
  free( shell_prog );
  shell_prog = NULL;
#endif // #ifndef BUILD_XMODEM  
}

// 'ver' handler
static void shell_ver( char* args )
{
  args = args;
  printf( "eLua version %s\n", ELUA_STR_VERSION );
  printf( "For more information go to http://elua.berlios.de\n" );
}

// Insert shell commands here
static const SHELL_COMMAND shell_commands[] = 
{
  { "help", shell_help },
  { "lua", shell_lua },
  { "recv", shell_recv },
  { "ver", shell_ver },
  { "exit", NULL },
  { NULL, NULL }
};

// Execute the eLua "shell" in an infinite loop
void shell_start()
{
  char cmd[ SHELL_MAXSIZE ];
  char *p, *temp;
  const SHELL_COMMAND* pcmd;
  int i, inside_quotes;
  char quote_char;
  
  while( 1 )
  {
    // Show prompt
    printf( SHELL_PROMPT );
    
    // Read command
    while( fgets( cmd, SHELL_MAXSIZE, stdin ) == NULL )
      clearerr( stdin );
          
    // Change '\r' and '\n' chars to ' ' to ease processing
    p = cmd;
    while( *p )
    {
      if( *p == '\r' || *p == '\n' )
        *p = ' ';
      p ++;
    }
    
    // Transform ' ' characters inside a '' or "" quoted string in
    // a 'special' char. We do this to let the user execute something
    // like "lua -e 'quoted string'" without disturbing the quoted
    // string in any way.
    for( i = 0, inside_quotes = 0, quote_char = '\0'; i < strlen( cmd ); i ++ )
      if( ( cmd[ i ] == '\'' ) || ( cmd[ i ] == '"' ) )
      {
        if( !inside_quotes )
        {
          inside_quotes = 1;
          quote_char = cmd[ i ];
        }
        else
        {
          if( cmd[ i ] == quote_char )
          {
            inside_quotes = 0;
            quote_char = '\0';
          }
        }
      }
      else if( ( cmd[ i ] == ' ' ) && inside_quotes )
        cmd[ i ] = SHELL_ALT_SPACE;
    if( inside_quotes )
    {
      printf( "Invalid quoted string\n" );
      continue;
    }
    
    // Transform consecutive sequences of spaces into a single space
    p = strchr( cmd, ' ' );
    while( p )
    {
      temp = p + 1;
      while( *temp && *temp == ' ' )
        memmove( temp, temp + 1, strlen( temp ) );
      p = strchr( p + 1, ' ' );
    }
    if( strlen( cmd ) == 1 )
      continue;
        
    // Look for the first ' ' to separate the command from its args    
    temp = cmd;
    if( *temp == ' ' )
      temp ++; 
    if( ( p = strchr( temp, ' ' ) ) == NULL )
    {
      printf( SHELL_ERRMSG );
      continue;
    }
    *p = 0;
    i = 0;
    while( 1 )
    {
      pcmd = shell_commands + i;
      if( pcmd->cmd == NULL )
      {
        printf( SHELL_ERRMSG );
        break;
      }
      if( !strcasecmp( pcmd->cmd, temp ) )
      {
        // Special case: the "exit" command has a NULL handler
        if( pcmd->handler_func )
          pcmd->handler_func( p + 1 );
        break;
      }
      i ++;
    }
    // Check for 'exit' command
    if( pcmd->cmd && !pcmd->handler_func )
      break;
  }
  // Shell exit point
  if( shell_prog )
    free( shell_prog );
}

// Initialize the shell, returning 1 for OK and 0 for error
int shell_init( unsigned maxprog )
{
  shell_prog = NULL;
  shell_max_prog = maxprog;
  return 1;
}

#else // #ifdef BUILD_SHELL

int shell_init( unsigned maxprog )
{
  maxprog = maxprog;
  return 0;
}

void shell_start()
{
}

#endif // #ifdef BUILD_SHELL

#include "log.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

static void LogPath( char *out, size_t len )
{
	char tmp[MAX_PATH] = { 0 };
	if ( !GetTempPathA( MAX_PATH, tmp ) )
		strcpy_s( tmp, sizeof( tmp ), ".\\" );
	_snprintf_s( out, len, _TRUNCATE, "%spulse_shader.log", tmp );
}

// Off unless -pulse_log is on the command line. Each call opens and closes the
// file, which is fine for load-time tracing but ruinous in the draw path - it
// cost roughly 170 fps when left enabled per draw call.
static bool LoggingEnabled()
{
	static int s_state = -1;
	if ( s_state < 0 )
	{
		const char *pCmd = GetCommandLineA();
		s_state = ( pCmd && strstr( pCmd, "-pulse_log" ) ) ? 1 : 0;
	}
	return s_state == 1;
}

void PulseLog( const char *fmt, ... )
{
	if ( !LoggingEnabled() )
		return;

	char path[MAX_PATH];
	LogPath( path, sizeof( path ) );

	FILE *f = nullptr;
	if ( fopen_s( &f, path, "a" ) != 0 || !f )
		return;

	SYSTEMTIME st;
	GetLocalTime( &st );
	fprintf( f, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds );

	va_list args;
	va_start( args, fmt );
	vfprintf( f, fmt, args );
	va_end( args );

	fputc( '\n', f );
	fclose( f );
}

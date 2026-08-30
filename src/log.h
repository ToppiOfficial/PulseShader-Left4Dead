#pragma once
// Load-time tracing to %TEMP%\pulse_shader.log. The game's console is not
// reachable this early and we never write inside the game directory.
void PulseLog( const char *fmt, ... );

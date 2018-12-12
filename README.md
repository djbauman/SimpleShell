# SimpleShell

To compile:
gcc -o smallsh smallsh.c

Commands:

"exit" - exits the shell.

"cd" - changes the working directory of the shell.

"status" - prints out either the exit status or the terminating signal of the last foreground process

"CTRL-C" - sends a SIGINT signal to the parent shell process and all child processes

"CTRL-Z" - sets Foreground-Only Mode; when enabled, commands cannot be run in the background

"<" - redirects standard input

">" - redirects standard output

"&" - if added at the end of a command, sets it to be run in the background
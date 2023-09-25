# This makefile is included for convenience and you can disregard it if you like.
# Otherwise, 'make clean' and 'make q2master' or 'make' will build it.
# Only 1 file, master.c is needed on Linux.
# Single file build command is 'gcc -o q2master master.c'.

CFLAGS =-DLINUX

q2master:
	$(CC) $(CFLAGS) -o $@ master.c

clean:
	/bin/rm -f q2master

# Dependencies
q2master: master.c

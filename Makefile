# DJGPP Makefile
# STUBEDIT CC1.EXE to set a minimum stack size of 1MB before compiling this
# project

# Define this to make the debug version
# DEBUG = 1

CC      = gcc
AS      = gcc
LNK     = gcc
AFLAGS  = -c -Wall

LFLAGS  = -s
CFLAGS  = -c -Wall -fomit-frame-pointer -O2 -Wno-parentheses
OBJECTS = ngpc.o

%.o : %.c
	$(CC) $(CFLAGS) $<
%.o : %.S
	$(AS) $(AFLAGS) $<

ngpc.exe: $(OBJECTS)
	$(LNK) $(LFLAGS) -o ngpc.exe -Wl,-Map,ngpc.map $(OBJECTS)

ngpc.o: ngpc.c

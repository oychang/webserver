CC = gcc
LD = $(CC)
CFLAGS = -Wall -Wextra -Werror -std=gnu99
VPATH = src


#tftp: server.o client.o tftp.c tftp.h Boolean.h Logging.h
#server.o: server.c server.h Boolean.h Logging.h Types.h


.PHONY: clean 
clean:
	rm -f *.o

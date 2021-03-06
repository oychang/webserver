CC = gcc
LD = $(CC)
CFLAGS = -Wall -Wextra -Werror -std=gnu99
VPATH = src


server: server.o
server.o: server.c server.h


.PHONY: clean clean-submit
clean:
	rm -f *.o
clean-submit: clean
	rm -rf .git .gitignore server docs

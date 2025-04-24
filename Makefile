CFLAGS = -Wall -Wextra -Werror -g

all: sshell

sshell: sshell.c
	gcc $(CFLAGS) -o sshell sshell.c

clean:
	rm -f sshell

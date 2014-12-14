#
# Project: GAL 2014
# Authors: Vendula Poncova, xponco00
#          Chernikava Alena, xcerni0700
# Date:    13.11.2014
#

NAME=matching
CC=gcc
CFLAGS=-std=c99 -pedantic -Wall -W -g -O3

all: $(NAME).c
	$(CC) $(CFLAGS) $(NAME).c -o $(NAME) -lpthread

profile:
	$(CC) $(CFLAGS) -pg $(NAME).c -o $(NAME) -lpthread

clean:
	rm $(NAME)

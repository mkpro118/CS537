# Compiler and compilation flags
CC := gcc
CFLAGS := -Wall -Werror -pedantic -std=gnu18

# Submission variables
LOGIN := mrigank
SUBMITPATH := ~cs537-1/handin/$(LOGIN)/P3/

# Targets
.PHONY: all wsh run pack submit

all: wsh

wsh: wsh.c wsh.h
	$(CC) $(CFLAGS) $^ -o $@

run: wsh
	./wsh

pack: wsh.h wsh.c Makefile README.md
	tar -czf $(LOGIN).tar.gz $<

submit: pack
	cp $(LOGIN).tar.gz $(SUBMITPATH)

# test:
# 	# TODO

# Clean target
clean:
	rm -fr wsh $(LOGIN).tar.gz

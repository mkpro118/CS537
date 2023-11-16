CC=gcc
CFLAGS=-ggdb3 -c -Wall -Werror -std=gnu99 -g # -fsanitize=address
LDFLAGS=-pthread # -fsanitize=address
SOURCES=safequeue.c proxyserver.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=proxyserver

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

testers:
	~cs537-1/tests/P6/runtests

local_testers:
	../tests/runtests

pq_test:
	$(CC) -Wall -Werror -std=gnu99 -pthread safequeue.c pq_tester.c -o pq_tester
	chmod 777 ./pq_tester
	-./pq_tester
	rm -fr ./pq_tester

clean:
	rm -f $(EXECUTABLE) $(OBJECTS)

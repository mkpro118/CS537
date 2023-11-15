CC=gcc
CFLAGS=-ggdb3 -c -Wall -Werror -std=gnu99
LDFLAGS=-pthread
SOURCES=proxyserver.c safequeue.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=proxyserver

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

pq_test:
	$(CC) -Wall -Werror -std=gnu99 -pthread safequeue.c pq_tester.c -o pq_tester
	chmod 777 ./pq_tester
	./pq_tester
	rm -fr ./pq_tester

clean:
	rm -f $(EXECUTABLE) $(OBJECTS)

CC=gcc
CFLAGS=-I. -I/usr/lib/epics/include -I/usr/lib/epics/include/os/Linux -I/usr/lib/epics/include/compiler/gcc

LDIR=/usr/lib/epics/lib/linux-x86_64
LIBS=-lca -lezca

DEPS=test_agent.h
OBJ=test_agent.o

%o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

test_agent: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -L$(LDIR) $(LIBS) -pthread

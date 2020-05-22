FLAGS = -std=c99 -pthread -lm -I ./
DEPS = header.h

all: server worker

server: server.c $(DEPS)
	gcc -o server server.c $(FLAGS)

worker: worker.c $(DEPS)
	gcc -o worker worker.c $(FLAGS)

.PHONY: all clean

clean:
	rm server worker
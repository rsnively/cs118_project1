all: makeserver

makeserver:
	gcc server.c -o server

clean:
	rm server


default: test

build:
	gcc -o myServer server.c -lcurl -pthread

test: build
	./myServer 
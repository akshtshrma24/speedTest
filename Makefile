default: test

build:
	gcc -o myServer server.c -lcurl

test: build
	./myServer 
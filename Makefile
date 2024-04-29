default: test

build:
	gcc -o myServer main.c server.c -lcurl

test: build
	./myServer 
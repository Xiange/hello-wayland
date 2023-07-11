all: helloworld
	echo "done"

helloworld: hello.c
	gcc -o $@ $^ -lwayland-client

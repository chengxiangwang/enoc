all: clean compile
compile:
	gcc -o main main.c
clean:
	sudo rm -f main

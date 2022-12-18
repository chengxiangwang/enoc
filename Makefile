all: clean compile
	sudo ./main
compile:
	gcc -o main main.c
clean:
	sudo rm -f main

all: main

main: obj/main.o obj/parser.o
	gcc -Wall -Wextra -Werror obj/main.o obj/parser.o -o main

obj/main.o: src/solution.c include/parser.h | obj
	gcc -c -Iinclude src/solution.c -o obj/main.o

obj/parser.o: src/parser.c include/parser.h | obj
	gcc -c -Iinclude src/parser.c -o obj/parser.o
	
obj:
	mkdir obj

clean:
	rm -rf obj main

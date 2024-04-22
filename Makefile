all: main

main: obj/main.o obj/libcoro.o
	gcc -Wall -Wextra -Werror -lrt -ldl -rdynamic obj/main.o obj/libcoro.o -o main

obj/main.o: src/solution.c include/libcoro.h | obj
	gcc -c -Iinclude src/solution.c -o obj/main.o

obj/libcoro.o: src/libcoro.c include/libcoro.h | obj
	gcc -c -Iinclude src/libcoro.c -o obj/libcoro.o
	
obj:
	mkdir obj

clean:
	rm -rf obj main

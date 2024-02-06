all: main

main: obj/main.o obj/sort.o obj/libcoro.o
	gcc -Wall -Wextra -Werror obj/main.o obj/libcoro.o obj/sort.o -o main

obj/main.o: src/solution.c include/libcoro.h include/sort.h | obj
	gcc -c -Iinclude src/solution.c -o obj/main.o

obj/libcoro.o: src/libcoro.c include/libcoro.h | obj
	gcc -c -Iinclude src/libcoro.c -o obj/libcoro.o
	
obj/sort.o: src/sort.c include/sort.h | obj
	gcc -c -Iinclude src/sort.c -o obj/sort.o
	
obj:
	mkdir obj

clean:
	rm -rf obj main

PROJECT=$${PWD\#\#*/}

LIB_IGN_WARN= -Wno-address-of-packed-member -Wno-format
CONSOLE_SRC= src/main.c

all: run

clear:
	@clear

compile:
	@echo "Compiling project ${PROJECT}"
	gcc -Wall ${LIB_IGN_WARN} -Iinclude src/compost.c -shared -o lib/libcompost.so -fPIC -g
	@echo "Compiling console for ${PROJECT}"
	gcc -Wall -Iinclude -lreadline ${CONSOLE_SRC} -o console -g -L./lib -Wl,-R./lib/ -lcompost

run: clear compile
	@./console

valgrind: clear compile
	valgrind --leak-check=full ./console

dbg: clear compile
	@gdb -q console
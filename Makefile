# need to set up AFL once parser is written https://medium.com/@ayushpriya10/fuzzing-applications-with-american-fuzzy-lop-afl-54facc65d102

.PHONY: test64 builtest64 valgrind64 clean test32 buildtest32 valgrind32 deps show checkleaks

test: buildtest64 valgrind64 buildtest32 valgrind32 clean checkleaks

VALGRINDOPTS = --track-origins=yes --leak-check=full --show-reachable=yes --main-stacksize=1000

ifeq (,$(findstring raspberrypi,$(shell uname -a)))
	VALGRINDOPTS += --show-leak-kinds=all
endif

buildtest64:
	g++ --std=c++11 pickle_test.cpp -g -o pickletest64

valgrind64: buildtest64
	valgrind $(VALGRINDOPTS) ./pickletest64 > test/out64.txt 2> test/valgrind64.txt

buildtest32:
	g++ --std=c++11 -m32 pickle_test.cpp -g -o pickletest32

valgrind32: buildtest32
	valgrind $(VALGRINDOPTS) ./pickletest32 > test/out32.txt 2> test/valgrind32.txt

clean:
	rm -f pickletest64
	rm -f pickletest32
	rm -f vgcore.*

deps:
	sudo dpkg --add-architecture i386
	sudo apt-get update
	sudo apt-get install valgrind gcc-multilib g++-multilib libgcc-s1:i386 libc6-dbg:i386 --yes

show:
	cat test/out64.txt
	cat test/valgrind64.txt
	cat test/out32.txt
	cat test/valgrind32.txt

checkleaks:
	cat test/valgrind64.txt | grep "no leaks are possible" >/dev/null
	cat test/valgrind32.txt | grep "no leaks are possible" >/dev/null

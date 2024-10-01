# need to set up AFL once parser is written https://medium.com/@ayushpriya10/fuzzing-applications-with-american-fuzzy-lop-afl-54facc65d102

ifneq ($(shell uname -s),Darwin)
.PHONY: test64 builtest64 valgrind64 clean test32 buildtest32 valgrind32 deps show checkleaks

test: buildtest64 valgrind64 buildtest32 valgrind32 clean checkleaks

VALGRINDOPTS = -s --track-origins=yes --leak-check=full --show-reachable=yes --main-stacksize=1000

ifeq (,$(findstring raspberrypi,$(shell uname -a)))
	VALGRINDOPTS += --show-leak-kinds=all
endif

CXXFLAGS += --std=c++11 -g

buildtest64:
	$(CXX) $(CXXFLAGS) pickle_test.cpp -o pickletest64

valgrind64: buildtest64
	valgrind $(VALGRINDOPTS) ./pickletest64 > test/out64.txt 2> test/valgrind64.txt


buildtest32: CXXFLAGS += -m32
buildtest32:
	$(CXX) $(CXXFLAGS) pickle_test.cpp -o pickletest32

valgrind32: buildtest32
	valgrind $(VALGRINDOPTS) ./pickletest32 > test/out32.txt 2> test/valgrind32.txt

clean:
	$(RM) -f pickletest64
	$(RM) -f pickletest32
	$(RM) -f vgcore.*

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
else
.PHONY: test buildtest valgrind clean deps show checkleaks

VALGRINDOPTS = -atExit

test: buildtest valgrind clean checkleaks

CXXFLAGS += --std=c++11 -g

buildtest:
	$(CXX) $(CXXFLAGS) pickle_test.cpp -o pickletest

valgrind: buildtest
	tmpf=`mktemp stderr.XXX`; MallocStackLogging=1 leaks $(VALGRINDOPTS) -- ./pickletest > test/outMac.txt 2>"$$tmpf"; cat "$$tmpf" >>test/outMac.txt; rm $$tmpf

clean:
	$(RM) -f pickletest
	$(RM) -rf pickletest.dSYM

show:
	cat test/outMac.txt
	cat test/valgrindMac.txt

checkleaks:
	cat test/outMac.txt | grep "0 leaks for 0 total leaked bytes" >/dev/null
endif

.PHONY: test valgrind clean

test:
	gcc -o pickle pickletest.cpp -lstdc++
	chmod +x ./pickle

valgrind: test
	valgrind ./pickle

clean:
	rm -rf ./pickle

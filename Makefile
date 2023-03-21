.PHONY: test valgrind clean

test:
	gcc -o pickle -g pickletest.cpp -lstdc++
	chmod +x ./pickle

valgrind: test
	valgrind --leak-check=full ./pickle

clean:
	rm -rf ./pickle

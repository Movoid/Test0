hello:hello.cpp
	g++ hello.cpp -o hello

PHONY: clean

clean:
	rm -f hello.o
	rm -f hello

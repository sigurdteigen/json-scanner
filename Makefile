all: lib

lib:
	$(CC) $(CFLAGS) -std=c99 -c json-scanner.c -o json-scanner.o
	ar rcu libjson-scanner.a json-scanner.o
	ranlib libjson-scanner.a

check: lib
	$(CC) $(CFLAGS) -std=c99 json-scanner-test.c -static -L. -ljson-scanner -o json-scanner-test
	./json-scanner-test

clean:
	rm -f *.o
	rm -f *.a
	rm -f json-scanner-test

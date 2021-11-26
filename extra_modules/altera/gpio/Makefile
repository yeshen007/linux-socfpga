.PHONY: all clean

all:
	arm-linux-gnueabihf-gcc gpio-test.c -o gpio-test
	arm-linux-gnueabihf-gcc led-test.c -o led-test
	arm-linux-gnueabihf-gcc uio-test.c -o uio-test
	arm-linux-gnueabihf-gcc build-data.c -o build-data
clean:
	rm gpio-test -rf
	rm led-test -rf
	rm uio-test -rf
	rm build-data -rf

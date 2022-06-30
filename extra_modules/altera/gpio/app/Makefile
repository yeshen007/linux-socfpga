.PHONY: all gpio_test gpio_test_legacy led_test uart_test clean help

all:
	arm-linux-gnueabihf-gcc gpio-test.c -o gpio-test
	arm-linux-gnueabihf-gcc gpio-test-legacy.c -o gpio-test-legacy
	arm-linux-gnueabihf-gcc led-test.c -o led-test
	arm-linux-gnueabihf-gcc uart-test.c -o uart-test

gpio_test:
	arm-linux-gnueabihf-gcc gpio-test.c -o gpio-test

gpio_test_legacy:
	arm-linux-gnueabihf-gcc gpio-test-legacy.c -o gpio-test-legacy

led_test:
	arm-linux-gnueabihf-gcc led-test.c -o led-test

uart_test:
	arm-linux-gnueabihf-gcc uart-test.c -o uart-test
	
clean:
	rm gpio-test -rf
	rm gpio-test-legacy -rf
	rm led-test -rf
	rm uart-test -rf

help:
	@echo taget :
	@echo -- all: compile all the subtarget
	@echo -- gpio_test : compile gpio-test.c to gpio-test
	@echo -- gpio_test_legacy : compile gpio-test-legacy.c to gpio-test-legacy
	@echo -- led_test : compile led-test.c to led-test
	@echo -- uart_test : compile uart-test.c to uart-test
	@echo -- clean : clean all the subtargets
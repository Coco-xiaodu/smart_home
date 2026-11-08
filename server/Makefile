TARGET = main
CC = arm-buildroot-linux-gnueabihf-gcc
CFLAGS = -Wall
LDFLAGS = -lpthread

c_files = main.c led.c server.c http.c dht11.c pir.c

all:
	${CC} ${CFLAGS} -o ${TARGET} ${c_files} ${LDFLAGS}

clean:
	rm -rf ${TARGET}

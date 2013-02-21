CFLAGS=-g -Wall -O2 -DVERSION=\"3.36\" -DSTORAGEDIR=\"/var/lib/bluetooth\" $(shell pkg-config --cflags bluez)
LIBS=$(shell pkg-config --libs bluez) -lm
OBJ = main.o sdp.o textfile.o

all: hidd

hidd: $(OBJ)
	$(CC) -g -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean: .PHONY
	rm -f $(OBJ)

.PHONY:	

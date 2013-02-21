bin_PROGRAMS = l2cap

l2cap_SOURCES = l2cap.h main.c sdp.c textfile.c

l2cap_LDADD = @BLUEZ_LIBS@ -lm

AM_CFLAGS = @BLUEZ_CFLAGS@ -DVERSION=\"3.36\" -DSTORAGEDIR=\"/var/lib/bluetooth\"

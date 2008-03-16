JAVAC=javac
JAVACFLAGS=-source 1.4 -target 1.4 -bootclasspath $(BOOTCLASSPATHS) -cp .
PREVERIFY=preverify
BOOTCLASSPATHS=/opt/j2me/lib/midpapi20.jar:/opt/j2me/lib/cldcapi11.jar:/opt/j2me/lib/jsr082.jar
JAR=jar
JARSIGNER=jarsigner
EMULATOR=emulator

SOURCES=L2cap.java
CLASSES=$(patsubst %.java,%.class,$(SOURCES))

all: $(CLASSES)

jar: L2cap.jar

%.class: %.java
	$(JAVAC) $(JAVACFLAGS) $^
	$(PREVERIFY) -classpath $(BOOTCLASSPATHS):. -d . $(patsubst %.class,%,$@)

L2cap.jar: MANIFEST $(CLASSES) icon.png
	$(JAR) cvfm $@ $^ *.class && \
	$(JARSIGNER) L2cap.jar mykey

L2cap.jad: L2cap.jar L2cap.jad.profile
	SIZE=`du -b $<|sed 's/\([0-9]*\).*$$/\1/'` && \
	sed "s/%SIZE%/$$SIZE/" $@.profile > $@

emu: $(CLASSES)
	$(EMULATOR) -cp . L2cap

emujad: L2cap.jad
	$(EMULATOR) -Xdescriptor:$^

clean:
	rm -f *.class *.jar *.jad

.PHONY: all clean jar emu emujad

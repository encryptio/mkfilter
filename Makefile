
CFLAGS += -O2 -Wall -g -std=c99 -pipe

CFLAGS += -D_BSD_SOURCE

CFLAGS += `pkg-config --cflags sndfile`
LIBS += `pkg-config --libs sndfile`

OBJECTS = src/kissfft/kiss_fft.o src/kissfft/kiss_fftr.o src/audiobuf.o src/make.o src/file.o src/main.o src/tools.o src/analyze.o src/wantcurve.o

.SUFFIXES: .c .o

all: mkfilter

mkfilter: $(OBJECTS)
	$(CC) $(LIBS) $(LDFLAGS) $(OBJECTS) -o mkfilter

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS)
	rm -f mkfilter

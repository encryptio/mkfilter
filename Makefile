
CFLAGS += -O2 -Wall -g -std=c99 -pipe

CFLAGS += -D_BSD_SOURCE

CFLAGS += `pkg-config --cflags sndfile`
LIBS += `pkg-config --libs sndfile`

KISSFFT_OBJECTS = src/kissfft/kiss_fft.o src/kissfft/kiss_fftr.o
MKFILTER_OBJECTS = src/mkfilter/audiobuf.o src/mkfilter/make.o src/mkfilter/file.o src/mkfilter/main.o src/mkfilter/tools.o src/mkfilter/analyze.o src/mkfilter/wantcurve.o
SMOOTHRESPONSE_OBJECTS = src/smoothresponse.o

.SUFFIXES: .c .o

all: mkfilter smoothresponse

mkfilter: $(KISSFFT_OBJECTS) $(MKFILTER_OBJECTS)
	$(CC) $(LIBS) $(LDFLAGS) $(KISSFFT_OBJECTS) $(MKFILTER_OBJECTS) -o mkfilter

smoothresponse: $(SMOOTHRESPONSE_OBJECTS)
	$(CC) $(LIBS) $(LDFLAGS) $(SMOOTHRESPONSE_OBJECTS) -o smoothresponse

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(KISSFFT_OBJECTS)
	rm -f $(MKFILTER_OBJECTS)
	rm -f $(SMOOTHRESPONSE_OBJECTS)
	rm -f mkfilter

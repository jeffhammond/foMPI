include ../Makefile.inc

LIBTOPODISC_OBJS = 	findcliques.o \
										meshmap2d.o \
										libtopodisc.o

all: libtopodisc.a

libtopodisc.a: $(LIBTOPODISC_OBJS)
	ar cru libtopodisc.a $(LIBTOPODISC_OBJS)
	ranlib libtopodisc.a

clean:
	rm -f $(LIBTOPODISC_OBJS) libtopodisc.a

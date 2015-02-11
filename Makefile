include Makefile.inc

FOMPIOPTS= -DXPMEM -DNA -DNDEBUG

LIBS=-Lmpitypes/install/lib -lmpitypes -ldmapp -Llibtopodisc -ltopodisc
INC=-Impitypes/install/include -Ilibtopodisc

CCFLAGS+=$(FOMPIOPTS) $(INC)
FCFLAGS+=$(FOMPIOPTS) $(INC)
CXXFLAGS+=$(FOMPIOPTS) $(INC)

OBJS = \
  fompi_fortran.o \
	fompi_helper.o \
	fompi_op.o \
	fompi_overloaded.o \
	fompi_win_allocate.o \
	fompi_win_attr.o \
	fompi_win_create.o \
	fompi_win_dynamic_create.o \
	fompi_win_fence.o \
	fompi_win_free.o \
	fompi_win_group.o \
	fompi_win_lock.o \
	fompi_win_name.o \
	fompi_win_pscw.o \
	fompi_win_rma.o \
	module_fompi.o \
	fompi_comm.o \
	fompi_req.o \
	fompi_notif.o \
	fompi_seg.o \
	fompi_notif_uq.o \
	fompi_notif_xpmem.o  

EXE = \
	c_test \
	fortran_test_f77 \
	fortran_test_f90

# some general rules
all: fompi-na.ar $(EXE)

clean:
	rm -f *.o fompi_op.c fompi-na.ar $(EXE)

recursive-clean: clean
	make -C mpitypes clean
	rm -f mpitypes/install/lib/libmpitypes.a
	make -C libtopodisc clean

distclean: clean 
	rm -rf mpitypes
	make -C libtopodisc clean

# libtopodisc.a is actual not a real dependency, but is here to ensure it is build
fompi-na.ar: $(OBJS) libtopodisc/libtopodisc.a
	ar -r fompi-na.ar $(OBJS)
	ranlib fompi-na.ar

c_test: c_test.o fompi-na.ar
	${CC} ${CCFLAGS} ${LDFLAGS} -o $@ c_test.o fompi-na.ar ${LIBS}

fortran_test_f77: fortran_test_f77.o fompi-na.ar
	${FC} ${FCFLAGS} ${LDFLAGS} -o $@ fortran_test_f77.o fompi-na.ar ${LIBS}

fortran_test_f90: fortran_test_f90.o fompi-na.ar
	${FC} ${FCFLAGS} ${LDFLAGS} -o $@ fortran_test_f90.o fompi-na.ar ${LIBS}

fompi_fortran.o: fompi.h

fompi_helper.o: fompi_helper.c fompi.h

fompi_op.o: fompi_op.c fompi.h
fompi_op.c: fompi_op.c.m4
	m4 fompi_op.c.m4 > fompi_op.c

fompi_overloaded.o: fompi_overloaded.c fompi.h libtopodisc/libtopodisc.h

fompi_win_allocate.o: fompi_win_allocate.c fompi.h

fompi_win_attr.o: fompi_win_attr.c fompi.h

fompi_win_create.o: fompi_win_create.c fompi.h

fompi_win_dynamic_create.o: fompi_win_dynamic_create.c fompi.h

fompi_win_fence.o: fompi_win_fence.c fompi.h

fompi_win_free.o: fompi_win_free.c fompi.h

fompi_win_group.o: fompi_win_group.c fompi.h

fompi_win_lock.o: fompi_win_lock.c fompi.h

fompi_win_name.o: fompi_win_name.c fompi.h

fompi_win_pscw.o: fompi_win_pscw.c fompi.h

fompi_win_rma.o: fompi_win_rma.c fompi.h mpitypes/install/include/mpitypes.h mpitypes/install/lib/libmpitypes.a

fompi_comm.o: fompi_comm.c fompi.h

fompi_req.o: fompi_req.c fompi.h

fompi_notif.o: fompi_notif.c fompi.h

fompi_notif_uq.o: fompi_notif_uq.c  fompi_notif_uq.h fompi.h

fompi_notif_xpmem.o: fompi_notif_xpmem.c fompi.h

fompi_seg.o: fompi_seg.c fompi.h

fompi.h: fompi_internal.h

fompi.mod module_fompi.o: module_fompi.f90
	$(FC) $(FCFLAGS) $(INC) -c $<

# target to build mpitypes with a separate compiler
mpitypes: mpitypes/install/include/mpitypes.h mpitypes/install/lib/libmpitypes.a

mpitypes/install/include/mpitypes.h mpitypes/install/lib/libmpitypes.a: mpitypes.tar.bz2
	tar xfj mpitypes.tar.bz2
	find mpitypes/configure.ac -type f -print0 | xargs -0 sed -i 's/mpicc/$(cc)/g'
	cd mpitypes ; \
	./prepare ; \
	./configure --prefix=$(CURDIR)/mpitypes/install
	make -C mpitypes
	make -C mpitypes install
	cp mpitypes/mpitypes-config.h mpitypes/install/include
	cp mpitypes/src/dataloop/dataloop_create.h mpitypes/install/include

# target to build mpitypes with a separate compiler
libtopodisc: libtopodisc/libtopodisc.a

libtopodisc/libtopodisc.a: libtopodisc/findcliques.c libtopodisc/findcliques.h libtopodisc/meshmap2d.c libtopodisc/libtopodisc.c
	make -C libtopodisc

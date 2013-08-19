CC=cc
FC=ftn

FOMPIOPTS= -DXPMEM -DNDEBUG

CCFLAGS=-O3 $(FOMPIOPTS)
FCFLAGS=-O3 $(FOMPIOPTS)

LDFLAGS=
LIBS=-Lmpitypes/install/lib -lmpitypes -ldmapp -Llibtopodisc/.libs -ltopodisc
INC=-Impitypes/install/include -Ilibtopodisc

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
	module_fompi.o

EXE = \
	c_test \
	fortran_test_f77 \
	fortran_test_f90

# clear out all suffixes
.SUFFIXES:
# list only those we use
.SUFFIXES: .o .c .f90

# some implicit rules
.c.o:
	$(CC) $(CCFLAGS) $(INC) -c $<

.f90.o:
	$(FC) $(FCFLAGS) $(INC) -c $<

# some general rules
all: fompi.ar $(EXE)

clean:
	rm -f *.o fompi_op.c fompi.ar $(EXE)

recursive-clean: clean
	make -C mpitypes clean
	rm -f mpitypes/install/lib/libmpitypes.a

distclean: clean 
	rm -rf autoconf-2.69 libtopodisc mpitypes

fompi.ar: $(OBJS)
	ar -r fompi.ar $(OBJS)
	ranlib fompi.ar

c_test: c_test.o fompi.ar
	${CC} ${CCFLAGS} ${LDFLAGS} -o $@ c_test.o fompi.ar ${LIBS}

fortran_test_f77: fortran_test_f77.o fompi.ar
	${FC} ${FCFLAGS} ${LDFLAGS} -o $@ fortran_test_f77.o fompi.ar ${LIBS}

fortran_test_f90: fortran_test_f90.o fompi.ar
	${FC} ${FCFLAGS} ${LDFLAGS} -o $@ fortran_test_f90.o fompi.ar ${LIBS}

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

fompi.h: fompi_internal.h

fompi.mod module_fompi.o: module_fompi.f90
	$(FC) $(FCFLAGS) $(INC) -c $<

autoconf-2.69: autoconf-2.69/install/bin/autoreconf

autoconf-2.69/install/bin/autoreconf: autoconf-2.69.tar.gz
	tar xfz autoconf-2.69.tar.gz
	mkdir autoconf-2.69/install
	cd autoconf-2.69 ; \
	./configure --prefix=$(CURDIR)/autoconf-2.69/install ; \
	make ; \
	make install

# target to build mpitypes with a separate compiler
mpitypes: mpitypes/install/include/mpitypes.h mpitypes/install/lib/libmpitypes.a

mpitypes/install/include/mpitypes.h mpitypes/install/lib/libmpitypes.a: mpitypes.tar.bz2
	tar xfj mpitypes.tar.bz2
	find mpitypes/configure.ac -type f -print0 | xargs -0 sed -i 's/mpicc/cc/g'
	cd mpitypes ; \
	./prepare ; \
	./configure --prefix=$(CURDIR)/mpitypes/install
	make -C mpitypes
	make -C mpitypes install
	cp mpitypes/mpitypes-config.h mpitypes/install/include
	cp mpitypes/src/dataloop/dataloop_create.h mpitypes/install/include

# target to build mpitypes with a separate compiler
libtopodisc: libtopodisc/libtopodisc.h libtopodisc/.libs/libtopodisc.a

libtopodisc/libtopodisc.h libtopodisc/.libs/libtopodisc.a: libtopodisc.tbz autoconf-2.69/install/bin/autoreconf
	tar xfj libtopodisc.tbz
	cd libtopodisc ; \
	rm -rf .svn ; \
	cat Makefile.am | sed -e '5,7!d' > temp ; \
	mv temp Makefile.am ; \
	echo "/\#include \"findcliques.h\"/a\\" > sed.cmd; \
	echo "extern \"C\" \{\\" >> sed.cmd ; \
	echo "  int TPD_Get_num_hier_levels(int* num, MPI_Comm comm);\\" >> sed.cmd ; \
	echo "  int TPD_Get_hier_levels(MPI_Comm *comms, int max_num, MPI_Comm comm);\\" >> sed.cmd ; \
	echo "\}" >> sed.cmd ; \
	cat libtopodisc.cpp | sed -f sed.cmd > temp; \
	mv temp libtopodisc.cpp ; \
	rm sed.cmd ; \
	$(CURDIR)/autoconf-2.69/install/bin/autoreconf -ivf ; \
	./configure CC=cc CXX=CC ; \
	make ; \
	chmod a+r libtopodisc.h ; \
	sed -i "1 s/^/#ifdef __cplusplus\nextern \"C\" {\n#endif/" libtopodisc.h; \
	sed -i "$$ s/^/#ifdef __cplusplus\n}\n#endif/" libtopodisc.h; \
	touch libtopodisc.h #so libtopodisc will be not rebuilt

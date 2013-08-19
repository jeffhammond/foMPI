CC = cc
FC = ftn

CCFLAGS = -O3
FCFLAGS = -O3

OBJS = \
	test_address.o \
	test_address_c.o

# clear out all suffixes
.SUFFIXES:
# list only those we use
.SUFFIXES: .o .c .f90

# some suffix rules
.c.o:
	$(CC) $(CCFLAGS) $(INC) -c $<

.f90.o:
	$(FC) $(FCFLAGS) $(INC) -c $<

# some general rules

all: test_address

clean:
	rm -f $(OBJS) test_address

test_address: $(OBJS)
	$(FC) $(FCFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

##############################
#
# Libre library's Makefile
#
# V0.0.1
##-Wl,--version-script=${PWD}/libfre_export.map ${LDFLAGS}
##############################

# GCC compile flags
GNUCC = gcc
GNUCFLAGS =  -g3 -ggdb -pg -Wno-format -Wall -Wextra -Wpedantic \
            -Wpointer-arith -Wstrict-prototypes -fsanitize=signed-integer-overflow
GNULDFLAGS = -lpthread

# Other compilers will go here


##############################
${CC} = ${GNUCC}                 # Your compiler.
CFLAGS = ${GNUCFLAGS}            # Your compiler's compile flags.
LDFLAGS = ${GNULDFLAGS}          # Your linker's flags.

OBJECTS = fre_internal_utils.o fre_internal_init.o fre_internal_main.o fre_bind.o

libname = libfre.so.0.0.1

${libname} : ${OBJECTS} fre_internal.h 
	${CC} ${CFLAGS} -shared ${OBJECTS} -o ${libname} ${LDFLAGS}

fre_internal_utils.o : fre_internal_utils.c fre_internal.h
	${CC} ${CFLAGS} -fPIC -c fre_internal_utils.c ${LDFLAGS}

fre_internal_init.o : fre_internal_init.c fre_internal.h
	${CC} ${CFLAGS} -fPIC -c fre_internal_init.c ${LDFLAGS}

fre_internal_main.o : fre_internal_main.c fre_internal.h
	${CC} ${CFLAGS} -fPIC -c fre_internal_main.c ${LDFLAGS}

fre_bind.o : fre_internal.h fre.h fre_bind.c
	${CC} ${CFLAGS} -fPIC -c fre_bind.c ${LDFLAGS} 

.PHONY : clean
clean :
	rm -f *.o ${libname}

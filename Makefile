##############################
#
# Libre library's Makefile
#
# V0.0.1
##-Wl,--version-script=${PWD}/libfre_export.map ${LDFLAGS}
##############################

# GCC compile flags
GNUCC = gcc
GNUCFLAGS =  -O2 -Wno-format -Wall -Wextra -pedantic \
            -Wpointer-arith -Wstrict-prototypes -fsanitize=signed-integer-overflow \
            -Wno-unused-variable
GNULDFLAGS = -lpthread

# Other compilers will go here


##############################
${CC} = ${GNUCC}                 # Your compiler.
CFLAGS = ${GNUCFLAGS}            # Your compiler's compile flags.
LDFLAGS = ${GNULDFLAGS}          # Your linker's flags.

OBJECTS = fre_internal_utils.o fre_internal_memutils.o fre_internal_init.o fre_internal_main.o fre_bind.o
INTERNAL_HEADERS = fre_internal_errcodes.h fre_internal_macros.h fre_internal.h

libname = libfre.so.0.0.1

${libname} : ${OBJECTS} ${INTERNAL_HEADERS}
	${CC} ${CFLAGS} -shared ${OBJECTS} -o ${libname} ${LDFLAGS}

fre_internal_utils.o : fre_internal_utils.c ${INTERNAL_HEADERS}
	${CC} ${CFLAGS} -fPIC -c fre_internal_utils.c ${LDFLAGS}

fre_internal_memutils.o : fre_internal_memutils.c ${INTERNAL_HEADERS}
	${CC} ${CFLAGS} -fPIC -c fre_internal_memutils.c ${LDFLAGS}

fre_internal_init.o : fre_internal_init.c ${INTERNAL_HEADERS}
	${CC} ${CFLAGS} -fPIC -c fre_internal_init.c ${LDFLAGS}

fre_internal_main.o : fre_internal_main.c ${INTERNAL_HEADERS}
	${CC} ${CFLAGS} -fPIC -c fre_internal_main.c ${LDFLAGS}

fre_bind.o : fre_bind.c ${INTERNAL_HEADERS} fre.h 
	${CC} ${CFLAGS} -fPIC -c fre_bind.c ${LDFLAGS} 

.PHONY : clean
clean :
	rm -f *.o ${libname}

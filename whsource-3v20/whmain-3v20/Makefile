BIN = winterhill-3v20
SRC = main.c rpi2c.c nim.c stv0910.c stv0910_utils.c stvvglna.c stvvglna_utils.c stv6120.c stv6120_utils.c
OBJ = ${SRC:.c=.o}

ifndef CC
CC = gcc 
endif
COPT = -O3 -march=native -mtune=native -I/opt/vc/include/ -L/opt/vc/lib/ -lbcm_host
#CFLAGS += -std=c99 -g -Wall -Wextra -Wpedantic -Wunused -DVERSION=\"${VER}\" -pthread -D_GNU_SOURCE 
#CFLAGS +=  -g -Wall -Wextra -Wpedantic -Wunused -DVERSION=\"${VER}\" -pthread -D_GNU_SOURCE 
CFLAGS +=  -g -Wall -Wextra -Wunused -DVERSION=\"${VER}\" -pthread -D_GNU_SOURCE 

all: ${BIN} 

debug: COPT = -Og 
debug: CFLAGS += -ggdb -fno-omit-frame-pointer
debug: all

werror: CFLAGS += -Werror
werror: all

$(BIN): ${OBJ}
	@echo "  LD     "$@
	@${CC} ${COPT} ${CFLAGS} -o $@ ${OBJ} ${LDFLAGS}

%.o: %.c *.h 
	@echo "  CC     "$<
	@${CC} ${COPT} ${CFLAGS} -c -fPIC -o $@ $<

clean:
	@rm -rf ${BIN} 
	@rm -rf ${OBJ} 

tags:
	@ctags *

.PHONY: all clean


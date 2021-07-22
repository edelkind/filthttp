debuglevel = 3

FTD_OBJ =              \
	src/auth.o         \
	src/base64.o       \
	src/die.o          \
	src/fs.o           \
	src/global.o       \
	src/header.o       \
	src/index.o        \
	src/main.o         \
	src/priv.o         \
	src/req.o          \
	src/resp.o         \


DEFINES = -DDEBUGLEVEL=$(debuglevel)
LIBS =
LIBS = -lcrypt
#WARNS = -Wall -Werror -Wextra -Wno-sign-compare -Wno-pointer-sign -Wno-missing-field-initializers
#WARNS = -Wall -Werror -Wno-sign-compare 
WARNS = -Wno-sign-compare 
CFLAGS += -g $(WARNS) -Ilx_lib/lib -Iminilib -Iget_opts $(DEFINES)
LDFLAGS += -Llx_lib/lib -llx -Lminilib -lminilib -Lget_opts -lget_opts $(LIBS)

all: filthttp

.c.o:
	gcc -o $@ -c $< $(CFLAGS)

clean:
	rm -f $(LIB_OBJ) $(FTD_OBJ) filthttp

filthttp:  $(FTD_OBJ)
	gcc -o $@ $(FTD_OBJ) $(LDFLAGS)


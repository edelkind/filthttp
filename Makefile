debuglevel = 3

FTD_OBJ =              \
	auth.o         \
	base64.o       \
	die.o          \
	fs.o           \
	global.o       \
	header.o       \
	index.o        \
	main.o         \
	priv.o         \
	req.o          \
	resp.o         \


DEFINES = -DDEBUGLEVEL=$(debuglevel)
LIBS =
LIBS = -lcrypt
#WARNS = -Wall -Werror -Wextra -Wno-sign-compare -Wno-pointer-sign -Wno-missing-field-initializers
#WARNS = -Wall -Werror -Wno-sign-compare 
WARNS = -Wno-sign-compare 
CFLAGS += -g $(WARNS) -I../lx_lib/lib -I../minilib -I../get_opts $(DEFINES)
LDFLAGS += -L../lx_lib/lib -llx -L../minilib -lminilib -L../get_opts -lget_opts $(LIBS)

all: filthttp

.c.o:
	gcc -o $@ -c $< $(CFLAGS)

clean:
	rm -f $(LIB_OBJ) $(FTD_OBJ) filthttp

filthttp:  $(FTD_OBJ)
	gcc -o $@ $(FTD_OBJ) $(LDFLAGS)


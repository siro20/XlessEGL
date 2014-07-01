INC = -I/usr/include/drm
CPPFLAGS = -Wall -msse -msse2 -msse3 -Wextra -march=native -pedantic -O2
LIBDIR = 
LIB = -lEGL -lGL -ldrm -lgbm
LDFLAGS = $(LIBDIR) $(LIB)

all: eglkms 

eglkms: eglkms.c
	$(CC) $(CFLAGS) $(INC) $(LDFLAGS) -o $@ $<


#  IMPORTANT:
#  Uncomment the following line, if you want
#  usermount to make the user the owner of
#  read-write mounted filesystems:
# OWNER = -DSET_OWNER

#  Uncomment the following line if you want to log
#  all (un)mounting operations:
LOG = -DLOGFILE=\"/var/log/usermount\"

CFLAGS = -O2 -m486 -fomit-frame-pointer -Wall $(OWNER) $(LOG)
LDFLAGS = -s

all: usermount userumount

usermount: usermount.c Makefile
	rm -f usermount
	gcc $(CFLAGS) $(LDFLAGS) -o usermount usermount.c

userumount: userumount.c Makefile
	rm -f userumount
	gcc $(CFLAGS) $(LDFLAGS) -o userumount userumount.c

install: usermount userumount
	cp -f usermount userumount /usr/local/bin/
	if [ ! -e /usr/local/lib/userfstab ]; then cp -f userfstab /usr/local/lib/; fi
	chown root.root /usr/local/bin/usermount /usr/local/bin/userumount /usr/local/lib/userfstab
	chmod u+s /usr/local/bin/usermount /usr/local/bin/userumount

install.man:
	cp -f usermount.man /usr/local/man/man1/usermount.1

clean:
	rm -f usermount userumount usermount.o userumount.o

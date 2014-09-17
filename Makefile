CFLAGS = -g
HEADERS = lll.h

all: lll

%.o: %.c $(HEADERS)
	$(CC) -c $< $(CFLAGS)

lll: lll.o 
	$(CC) -o $@ $(CFLAGS) $^

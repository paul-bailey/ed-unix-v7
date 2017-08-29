.PHONY: all clean

prog := ed
objs := \
 ed.o \
 term.o \
 file.o \
 blk.o \
 cr.o \
 code.o \
 signal.o \
 line.o \
 subst.o \
 simplebuf.o

CFLAGS += -Wall

all: $(prog)

$(prog): $(objs)

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	$(RM) -rf $(prog) $(objs)

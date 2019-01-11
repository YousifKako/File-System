CFLAGS = -g

objects = Main.c SubParser.c

Checker : $(objects)
	cc -o FS $(objects)

.PHONY : clean
clean :
	rm FS $(objects)
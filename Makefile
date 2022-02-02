EXE = rtfsed
HDR = STATIC/*.h   rtfsed.h 
SRC = STATIC/*.c   rtfsed.c   main.c



CC = cc

RFLAGS = -std=c17 -Oz
DFLAGS = -std=c17 -O0 -gfull  
SFLAGS = -W -Wall -Werror -Wno-poison-system-directories -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wswitch -Wshadow -Wcast-align -Wwrite-strings -Wmisleading-indentation
SSFLAGS = -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls



all: superstrict

release: $(SRC) $(HDR)
	$(CC) $(RFLAGS) $(SRC) -o $(EXE)
	strip $(EXE)

debug: $(SRC) $(HDR)
	$(CC) $(DFLAGS) $(SRC) -o $(EXE)

strict: $(SRC) $(HDR)
	$(CC) $(SFLAGS) $(DFLAGS) $(SRC) -o $(EXE)

superstrict: $(SRC) $(HDR)
	$(CC) $(SFLAGS) $(SSFLAGS) $(DFLAGS) $(SRC) -o $(EXE)


clean:
	rm -Rf core *.o *~ $(EXE) $(EXE).dSYM/ DOCS/new.rtf

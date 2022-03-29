EXE = rtfsed
EXTRA = DOCS/new.rtf
HDR = STATIC/*.h   *.h 
SRC = STATIC/*.c   *.c



CC = cc

RFLAGS = -std=c17 -Oz
DFLAGS = -std=c17 -O0 -gfull  
SFLAGS = -W -Wall  -Werror  -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wswitch -Wshadow -Wcast-align -Wwrite-strings -Wmisleading-indentation
SSFLAGS = -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls
SSSFLAGS = -Weverything -Wno-padded -Wno-poison-system-directories -Wno-unused-function 



all: superstrictest

release: $(SRC) $(HDR)
	$(CC) $(RFLAGS) $(SRC) -o $(EXE)
	strip $(EXE)

debug: $(SRC) $(HDR)
	$(CC) $(DFLAGS) $(SRC) -o $(EXE)

strict: $(SRC) $(HDR)
	$(CC) $(SFLAGS) $(DFLAGS) $(SRC) -o $(EXE)

superstrict: $(SRC) $(HDR)
	$(CC) $(SFLAGS) $(SSFLAGS) $(DFLAGS) $(SRC) -o $(EXE)

superstrictest: $(SRC) $(HDR)
	$(CC) $(SFLAGS) $(SSFLAGS) $(SSSFLAGS) $(DFLAGS) $(SRC) -o $(EXE)


clean:
	rm -Rf core *.o *~ $(EXE) $(EXE).dSYM/ $(EXTRA)

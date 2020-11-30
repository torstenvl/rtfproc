TGT = rtfrep

HDR = rtfrep.h

SRC = rtfrep.c driver.c



CC = cc

RFLAGS = -std=c17 -O2

DFLAGS = -std=c17 -gfull

SFLAGS = -W -Wall -Werror -Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wswitch -Wshadow -Wcast-align -Wwrite-strings

SSFLAGS = -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls



all: release



release:	$(SRC) $(HDR)
		$(CC) $(RFLAGS) $(SRC) -o $(TGT)
		strip $(TGT)

debug: 		$(SRC) $(HDR)
		$(CC) $(DFLAGS) $(SRC) -o $(TGT)



strict-release:	$(SRC) $(HDR)
		$(CC) $(SFLAGS) $(RFLAGS) $(SRC) -o $(TGT)
		strip $(TGT)

strict-debug: 	$(SRC) $(HDR)
		$(CC) $(SFLAGS) $(DFLAGS) $(SRC) -o $(TGT)



superstrict-release:	$(SRC) $(HDR)
			$(CC) $(SFLAGS) $(SSFLAGS) $(RFLAGS) $(SRC) -o $(TGT)
			strip $(TGT)

superstrict-debug:	$(SRC) $(HDR)
			$(CC) $(SFLAGS) $(SSFLAGS) $(DFLAGS) $(SRC) -o $(TGT)



clean:
	rm -Rf core *.o *~ $(TGT) $(TGT).dSYM/
SHELL:=/bin/bash

EXE=rtfsed
HDR=STATIC/*.h *.h 
SRC=STATIC/*.c *.c

SUCC="\342\234\205 SUCCESS\n"
FAIL="\342\235\214\033[1;31m FAILED!!!\033[m\n"
TESTOUTPUT=TEST/rtfprocess-output.rtf


CC = cc

RELFLAGS = -std=c2x -Oz
DBGFLAGS = -std=c2x -O0 -gfull
STRFLAGS = -W -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wswitch -Wshadow -Wcast-align -Wwrite-strings -Wmisleading-indentation
XSTRFLAGS = -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls
XXSTRFLAGS = -Weverything -Wno-padded -Wno-poison-system-directories -Wno-unused-function


.PHONY: all
all: superstrict

.PHONY: release
release: $(SRC) $(HDR)
	$(CC) $(RELFLAGS) $(SRC) -o $(EXE)
	strip $(EXE)

.PHONY: debug
debug: $(SRC) $(HDR)
	$(CC) $(DBGFLAGS) $(SRC) -o $(EXE)

.PHONY: semistrict
semistrict: $(SRC) $(HDR)
	$(CC) $(STRFLAGS) $(DBGFLAGS) $(SRC) -o $(EXE)

.PHONY: strict
strict: $(SRC) $(HDR)
	$(CC) $(STRFLAGS) $(XSTRFLAGS) $(DBGFLAGS) $(SRC) -o $(EXE)

.PHONY: superstrict
superstrict: $(SRC) $(HDR)
	$(CC) $(STRFLAGS) $(XSTRFLAGS) $(XXSTRFLAGS) $(DBGFLAGS) $(SRC) -o $(EXE)

.PHONY: test
test: testprologue testsuite testepilogue

.PHONY: testprologue
testprologue:
	@echo
	@echo "RUNNING TEST SUITE"
	@echo "------------------"

.PHONY: testepilogue
testepilogue:
	@echo

.PHONY: testsuite
testsuite: testrtfprocess

.PHONY: testrtfprocess
testrtfprocess:
	@$(CC) $(STRFLAGS) $(XSTRFLAGS) $(XXSTRFLAGS) $(DBGFLAGS) $(SRC) -o $(EXE)
	@printf "Testing rtfprocess... "
	@./rtfsed < TEST/rtfprocess-input.rtf > TEST/rtfprocess-output.rtf
	@diff TEST/rtfprocess-output.rtf TEST/rtfprocess-correct.rtf > /dev/null \
		&& printf $(SUCC) || printf $(FAIL)

.PHONY: clean
clean:
	rm -Rf core *.o *~ $(EXE) $(EXE).dSYM/ $(TESTOUTPUT)

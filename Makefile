SHELL:=/bin/bash

EXE=rtfsed
HDR=STATIC/*.h *.h 
SRC=STATIC/*.c *.c

SUCC="\342\234\205 SUCCESS\n"
FAIL="\342\235\214\033[1;31m FAILED!!!\033[m\n"
TESTOUTPUT=TEST/rtfprocess-output.rtf


CC = cc

RELEASE = -std=c2x -Oz
DEBUG = -std=c2x -O0 -gfull
STRICT = -W -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wswitch -Wshadow -Wcast-align -Wwrite-strings -Wmisleading-indentation
SUPERSTRICT = -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls
ULTRASTRICT = -Weverything -Wno-padded -Wno-poison-system-directories -Wno-unused-function


.PHONY: all
all: superstrict

.PHONY: release
release: $(SRC) $(HDR)
	$(CC) $(RELEASEFLAGS) $(SRC) -o $(EXE)
	strip $(EXE)

.PHONY: debug
debug: $(SRC) $(HDR)
	$(CC) $(DEBUGFLAGS) $(SRC) -o $(EXE)

.PHONY: semistrict
semistrict: $(SRC) $(HDR)
	$(CC) $(STRFLAGS) $(DEBUGFLAGS) $(SRC) -o $(EXE)

.PHONY: strict
strict: $(SRC) $(HDR)
	$(CC) $(STRFLAGS) $(SUPSTRFLAGS) $(DEBUGFLAGS) $(SRC) -o $(EXE)

.PHONY: superstrict
superstrict: $(SRC) $(HDR)
	$(CC) $(STRFLAGS) $(SUPSTRFLAGS) $(ULTSTRFLAGS) $(DEBUGFLAGS) $(SRC) -o $(EXE)

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
	@$(CC) $(STRFLAGS) $(SUPSTRFLAGS) $(ULTSTRFLAGS) $(DEBUGFLAGS) $(SRC) -o $(EXE)
	@printf "Testing rtfprocess... "
	@./rtfsed
	@diff TEST/rtfprocess-output.rtf TEST/rtfprocess-correct.rtf > /dev/null \
		&& printf $(SUCC) \
		|| printf $(FAIL)
	@rm TEST/rtfprocess-output.rtf


.PHONY: clean
clean:
	rm -Rf core *.o *~ $(EXE) $(EXE).dSYM/ $(TESTOUTPUT)

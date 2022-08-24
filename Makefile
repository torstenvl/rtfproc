SHELL:=/bin/bash

EXE=rtfsed
HDR=STATIC/*.h *.h 
SRC=STATIC/*.c *.c

SUCC="\342\234\205 SUCCESS\n"
FAIL="\342\235\214\033[1;31m FAILED!!!\033[m\n"

CC = cc

RELFLAGS = -std=c2x -Oz
DBGFLAGS = -std=c2x -O0 -gfull
STRFLAGS = -W -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wswitch -Wshadow -Wcast-align -Wwrite-strings -Wmisleading-indentation -Wno-bitwise-op-parentheses -Wno-unused-function
XSTRFLAGS = -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls
XXSTRFLAGS = -Weverything -Wno-padded -Wno-poison-system-directories -Wno-gnu-binary-literal

.PHONY: all
all: debug

.PHONY: release
release: $(SRC) $(HDR)
	$(CC) $(RELFLAGS) $(SRC) -o $(EXE)
	strip $(EXE)

.PHONY: debug
debug: $(SRC) $(HDR)
	$(CC) $(DBGFLAGS) $(SRC) -o $(EXE)

.PHONY: strict
strict: $(SRC) $(HDR)
	$(CC) $(STRFLAGS) $(DBGFLAGS) $(SRC) -o $(EXE)

.PHONY: xtrastrict
xtrastrict: $(SRC) $(HDR)
	$(CC) $(STRFLAGS) $(XSTRFLAGS) $(DBGFLAGS) $(SRC) -o $(EXE)

.PHONY: xtraxtrastrict
xtraxtrastrict: $(SRC) $(HDR)
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
testsuite: testutf8 testrtfprocess

.PHONY: testrtfprocess
testrtfprocess:
	@$(CC) $(STRFLAGS) $(XSTRFLAGS) $(XXSTRFLAGS) $(SRC) -o $(EXE)
	@printf "%-30s" "Testing rtfprocess... "
	@./rtfsed < TEST/rtfprocess-input.rtf > TEST/rtfprocess-output.rtf
	@diff TEST/rtfprocess-output.rtf TEST/rtfprocess-correct.rtf > /dev/null \
		&& printf $(SUCC) && rm rtfsed TEST/rtfprocess-output.rtf || printf $(FAIL)

.PHONY: testutf8
testutf8:
	@$(CC) STATIC/*.c TEST/utf8test.c -o TEST/utf8test
	@printf "%-30s" "Testing utf8test... "
	@./TEST/utf8test \
		&& printf $(SUCC) && rm TEST/utf8test || printf $(FAIL)

.PHONY: clean
clean:
	@rm -Rf .DS_Store core *.o *~ $(EXE) 
	@rm -Rf *.dSYM/ TEST/*.dSYM
	@rm -Rf Info.plist TEST/Info.plist
	@rm -Rf TEST/rtfprocess-output.rtf
	@rm -Rf TEST/utf8test
	@echo Repository cleaned

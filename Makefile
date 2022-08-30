ifeq ($(OS),Windows_NT)
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        REL_FLG = -std=c2x -Os
        DBG_FLG = -std=c2x -O0 -pg
        STR_FLG = -W -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
		          -Wreturn-type -Wcast-qual -Wswitch -Wshadow -Wcast-align -Wwrite-strings \
				  -Wmisleading-indentation -Wno-parentheses -Wno-unused-function -Wno-unused-value
        VSTR_FLG = -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls
        XSTR_FLG = -Wextra 
    endif
    ifeq ($(UNAME_S),Darwin)
        REL_FLG  =  -std=c2x -Oz
        DBG_FLG  =  -std=c2x -O0 -gfull
        STR_FLG  =  -W -Wall -Werror -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith \
		            -Wreturn-type -Wcast-qual -Wswitch -Wshadow -Wcast-align -Wwrite-strings \
				    -Wmisleading-indentation -Wno-bitwise-op-parentheses -Wno-unused-function
        VSTR_FLG =  -Wunused-parameter -Wchar-subscripts -Winline -Wnested-externs -Wredundant-decls
        XSTR_FLG =  -Weverything -Wno-gnu-binary-literal -Wno-poison-system-directories \
		            -Wno-padded -Wno-c99-compat
    endif
endif




SHELL:=/bin/bash

EXE=rtfsed
HDR=STATIC/*.h *.h 
SRC=STATIC/*.c *.c

SUCC="\342\234\205 SUCCESS\n"
FAIL="\342\235\214\033[1;31m FAILED!!!\033[m\n"

CC = cc

.PHONY: all
all: xtrastrict

.PHONY: release
release: $(SRC) $(HDR)
	@$(CC) $(REL_FLG) $(SRC) -o $(EXE)
	strip $(EXE)

.PHONY: debug
debug: $(SRC) $(HDR)
	@$(CC) $(DBG_FLG) $(SRC) -o $(EXE)

.PHONY: strict
strict: $(SRC) $(HDR)
	@$(CC) $(STR_FLG) $(DBG_FLG) $(SRC) -o $(EXE)

.PHONY: verystrict
verystrict: $(SRC) $(HDR)
	@$(CC) $(STR_FLG) $(VSTR_FLG) $(DBG_FLG) $(SRC) -o $(EXE)

.PHONY: xtrastrict
xtrastrict: $(SRC) $(HDR)
	@$(CC) $(STR_FLG) $(VSTR_FLG) $(XSTR_FLG) $(DBG_FLG) $(SRC) -o $(EXE)

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
	@$(CC) $(STR_FLG) $(VSTR_FLG) $(XSTR_FLG) $(SRC) -o $(EXE)
	@printf "%-25s" "Testing rtfprocess... "
	@./rtfsed < TEST/rtfprocess-input.rtf > TEST/rtfprocess-output.rtf
	@diff TEST/rtfprocess-output.rtf TEST/rtfprocess-correct.rtf > /dev/null \
		&& printf $(SUCC) && rm rtfsed TEST/rtfprocess-output.rtf || printf $(FAIL)

.PHONY: testutf8
testutf8:
	@$(CC) $(STR_FLG) $(VSTR_FLG) $(XSTR_FLG) STATIC/*.c TEST/utf8test.c -o TEST/utf8test
	@printf "%-25s" "Testing utf8test... "
	@./TEST/utf8test \
		&& printf $(SUCC) && rm TEST/utf8test || printf $(FAIL)

.PHONY: clean
clean:
	@rm -Rf .DS_Store core *.o *~ $(EXE) 
	@rm -Rf *.dSYM/ TEST/*.dSYM
	@rm -Rf Info.plist TEST/Info.plist
	@rm -Rf TEST/rtfprocess-output.rtf
	@rm -Rf TEST/utf8test
	@rm -Rf TEST/bigfile-output.rtf
	@echo Repository cleaned

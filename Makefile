#=============================================================================
#                               PROJECT SETTINGS
#=============================================================================
EXEC	:=	$(shell basename `pwd`)
HDR		:=	$(wildcard *.h)
SRC		:=	$(wildcard *.c)
LIBSRC  :=	$(wildcard STATIC/*/*.c)
LIBHDR  :=	$(wildcard STATIC/*/*.h)
ALLSRC	:=	$(SRC) $(LIBSRC)
ALLHDR	:=  $(HDR) $(LIBHDR)


#=============================================================================
#                                COMPILER FLAGS
#=============================================================================
ifeq ($(shell cc -v 2>&1 | grep clang),"")
	CC			:=	gcc
	OPTFLAG		:=	-Os
	DBUGFLAG	:=	-pg -O0
	WEXCLUDE	:=	-Wno-parentheses -Wno-padded -Wno-unused-value
	WEVERYTHING	:=	-Wextra
else
	CC			:=	clang
	OPTFLAG		:=	-Oz
	DBUGFLAG	:=	-gfull -O0
	WEXCLUDE	:=	-Wno-gnu-binary-literal -Wno-c++98-compat -Wno-padded \
	                -Wno-c99-compat -Wno-poison-system-directories \
					-Wno-unused-function
	WEVERYTHING	:=	-Wextra -Weverything 
endif

CFLAGS	:= -std=c11 -funsigned-char
RELEASE	:= $(CFLAGS) $(OPTFLAG) -DNDEBUG
DEBUG	:= $(CFLAGS) $(DBUGFLAG)
STRICT1	:= -W -Wall 
STRICT2	:= $(STRICT1) -Werror -pedantic                                  \
		   -Wstrict-prototypes -Wmissing-prototypes -Wchar-subscripts    \
		   -Wpointer-arith -Wcast-qual -Wswitch -Wshadow -Wcast-align    \
		   -Wreturn-type -Wwrite-strings -Winline -Wredundant-decls      \
		   -Wmisleading-indentation -Wunused-parameter -Wnested-externs 
STRICT3	:= $(STRICT2) $(WEVERYTHING)


#=============================================================================
#                BUILD TARGETS AND FOUNDATIONAL TESTING HARNESS
#=============================================================================
.PHONY:     release debug strict stricter strictest clean test
.PHONY:     stackdebug demo 
all:		strictest
release:	$(ALLSRC) $(ALLHDR)
	@$(CC)	$(ALLSRC) $(RELEASE) 						-o $(EXEC)
	@strip	$(EXEC)
debug:		$(ALLSRC) $(ALLHDR)
	@$(CC)	$(ALLSRC) $(DEBUG) 							-o $(EXEC)
stackdebug:	$(ALLSRC) $(ALLHDR)
	@$(CC)	$(ALLSRC) $(DEBUG) -DUL__NEED_STACKTRACE	-o $(EXEC)
strict:		$(ALLSRC) $(ALLHDR)
	@$(CC)	$(ALLSRC) $(RELEASE) $(STRICT1)	$(WEXCLUDE)	-o $(EXEC)
stricter:	$(ALLSRC) $(ALLHDR)
	@$(CC)	$(ALLSRC) $(RELEASE) $(STRICT2)	$(WEXCLUDE)	-o $(EXEC)
strictest:	$(ALLSRC) $(ALLHDR)
	@$(CC)	$(ALLSRC) $(RELEASE) $(STRICT3)	$(WEXCLUDE)	-o $(EXEC)
demo:		$(ALLSRC) $(ALLHDR)
	@$(CC)	$(ALLSRC) $(RELEASE) -DRTFAUTOOPEN	-o $(EXEC)
	@strip	$(EXEC)
	@./$(EXEC)
clean:
	@rm -Rf .DS_Store core *.o *~ *.dSYM/ */*.dSYM Info.plist */Info.plist 
	@rm -Rf *-output* TEST/*-output* $(EXEC) $(TESTTGT)
	@echo Repository cleaned


#=============================================================================
#                                 TEST HARNESS
#=============================================================================
TESTTGT		:=	TEST/testexec
TESTCC		:=	$(CC) $(RELEASE) $(STRICT3) $(WEXCLUDE) -I.
SUCCESS		:=	"\342\234\205 OK\n"
FAILURE		:=	"\342\235\214\033[1;31m FAILED!!!\033[m\n"
TESTEND		:=	printf $(SUCCESS) || printf $(FAILURE)
TESTSTART	=	printf "%s %-36s" "Testing" $(subst test_,,$@)
test:		testsuiteprologue testsuite testsuiteepilogue
testsuiteprologue:
	@printf "\nRUNNING TEST SUITE\n——————————————————\n"
testsuiteepilogue:
	@printf "\n\n"


#=============================================================================
#                                    TESTS
#=============================================================================
testsuite:	test_rtfprocess test_utf8test test_cpgtoutest test_speedtest

test_rtfprocess: $(ALLSRC) $(ALLHDR)
	@$(TESTSTART)
	@$(TESTCC) $(ALLSRC) -o $(TESTTGT)
	@$(TESTTGT) < TEST/rtfprocess-input.rtf > test_result.tmp
	@diff test_result.tmp TEST/rtfprocess-correct.rtf > /dev/null && $(TESTEND)
	@rm $(TESTTGT) test_result.tmp

test_utf8test: $(LIBSRC) TEST/utf8test.c
	@$(TESTSTART)
	@$(TESTCC) $(LIBSRC) TEST/utf8test.c -o $(TESTTGT)
	@$(TESTTGT) && $(TESTEND)
	@rm $(TESTTGT) 

test_cpgtoutest: $(LIBSRC) TEST/cpgtoutest.c
	@$(TESTSTART)
	@$(TESTCC) $(LIBSRC) TEST/cpgtoutest.c -o $(TESTTGT)
	@$(TESTTGT) && $(TESTEND)
	@rm $(TESTTGT) 

test_speedtest:
	@$(TESTSTART)
	@$(TESTCC) $(ALLSRC) -o $(TESTTGT)
	@strip $(TESTTGT)
	@echo
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@rm $(TESTTGT) temp.rtf

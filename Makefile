#############################################################################
##        Operating system detection and meta-setting configuration
#############################################################################
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	HOSTOS		=	linux
	CC			=	gcc
	SUCCESS		=	"\342\234\205 OK\n"
	FAILURE		=	"\342\235\214\033[1;31m FAILED!!!\033[m\n"
else ifeq ($(UNAME_S),NetBSD)
	HOSTOS		=	 bsd
	CC			=	 gcc
	SUCCESS		=	"\342\234\205 OK\n"
	FAILURE		=	 "\342\235\214\033[1;31m FAILED!!!\033[m\n"
else ifeq ($(UNAME_S),OpenBSD)
	HOSTOS		=	bsd
	CC			=	clang
	SUCCESS		=	"\342\234\205 OK\n"
	FAILURE		=	"\342\235\214\033[1;31m FAILED!!!\033[m\n"
else ifeq ($(UNAME_S),FreeBSD)
	HOSTOS		=	bsd
	CC			=	clang
	SUCCESS		=	"\342\234\205 OK\n"
	FAILURE		=	"\342\235\214\033[1;31m FAILED!!!\033[m\n"
else ifeq ($(UNAME_S),Darwin)
	HOSTOS 		=	macos
	CC			=	clang
	SUCCESS		=	"\342\234\205 OK\n"
	FAILURE		=	"\342\235\214\033[1;31m FAILED!!!\033[m\n"
else 
	HOSTOS 		=	unknown
	CC			=	gcc
	SUCCESS		=	"\. OK\n"
	FAILURE		=	"X FAILED!!!\n"
endif

ifeq ($(CC),clang)
	O_SZ_FLAG	=	-Ofast
	DBUG_FLAG	=	-gfull -O0
	WEVERYTHING	=	-Weverything -Wno-poison-system-directories              \
					-Wno-tautological-unsigned-char-zero-compare -Wno-padded \
					-Wno-c++98-compat -Wno-gnu-binary-literal -Wno-c99-compat
else ifeq ($(CC),gcc)
	O_SZ_FLAG	=	-Os
	DBUG_FLAG	=	-pg -O0
	WEVERYTHING	=	
else ifeq ($(CC),msvcpp)
	O_SZ_FLAG	=	/Os
	DBUG_FLAG	=	/Zi /O0
	WEVERYTHING	=	
endif


#############################################################################
##      Basic build flags for different levels of strictness/debugging
#############################################################################
CFLAGS    = -std=c2x -funsigned-char 
RELEASE   = $(CFLAGS) $(O_SZ_FLAG) -DNDEBUG
DEBUG     = $(CFLAGS) $(DBUG_FLAG)
STRICT1   = -W -Wall -Werror -Wno-unused-function 
STRICT2   = $(STRICT1) -pedantic -Wno-gnu-binary-literal                \
			-Wstrict-prototypes -Wmissing-prototypes -Wchar-subscripts  \
			-Wpointer-arith -Wcast-qual -Wswitch -Wshadow -Wcast-align  \
			-Wreturn-type -Wwrite-strings -Winline -Wredundant-decls    \
			-Wmisleading-indentation -Wunused-parameter -Wnested-externs 
STRICT3   = $(STRICT2) -Wextra 
STRICT4   = $(STRICT3) $(WEVERYTHING)



#############################################################################
##                             Target constants 
#############################################################################
EXEC	=	$(shell basename `stat -f %R .`)
HEADERS	=	*.h STATIC/*/*.h 
SOURCE	=	*.c STATIC/*/*.c
ALLSRC	=   $(SOURCE) $(HEADERS)



#############################################################################
##                               Build targets 
#############################################################################
all:		release
release:	$(ALLSRC)
	@$(CC)  $(SOURCE) $(RELEASE) 			-o $(EXEC)
	@strip  $(EXEC)
debug:	$(ALLSRC)
	@$(CC)  $(SOURCE) $(DEBUG) 				-o $(EXEC)
strict:		$(ALLSRC)
	@$(CC)  $(SOURCE) $(RELEASE) $(STRICT1)	-o $(EXEC)
stricter:	$(ALLSRC)
	@$(CC)  $(SOURCE) $(RELEASE) $(STRICT2)	-o $(EXEC)
strictest:	$(ALLSRC)
	@$(CC)  $(SOURCE) $(RELEASE) $(STRICT3)	-o $(EXEC)
ULTRASTRICT:$(ALLSRC)
	@$(CC)  $(SOURCE) $(RELEASE) $(STRICT4)	-o $(EXEC)
demo:		$(ALLSRC)
	@$(CC)  $(SOURCE) $(RELEASE) -DRTFAUTOOPEN	-o $(EXEC)
	@strip  $(EXEC)
	@./$(EXEC)
test:		testsuiteprologue testsuite testsuiteepilogue
testsuiteprologue:
	@echo
	@printf "OS:        %s\n" $(HOSTOS)
	@printf "COMPILER:  %s\n" $(CC)
	@printf "VERSION:   "
	@$(CC) -v 2>&1 | head -n 1
	@echo
	@echo "RUNNING TEST SUITE"
	@echo "------------------"
testsuite:	test_rtfprocess test_utf8test test_cpgtoutest test_speedtest
testsuiteepilogue:
	@echo



TESTCC		=	$(CC) $(RELEASE) $(STRICT3)
TESTSTART	=	@printf "%s %-17s%s" "Testing" $(subst test_,,$@...)
TESTEND		=	printf $(SUCCESS) || printf $(FAILURE)
TESTTGT		=	TEST/testexec



test_rtfprocess: $(ALLSRC)
	$(TESTSTART)
	@$(TESTCC) $(SOURCE) -o $(TESTTGT)
	@$(TESTTGT) < TEST/rtfprocess-input.rtf > TEST/rtfprocess-output.rtf
	@diff TEST/rtfprocess-output.rtf TEST/rtfprocess-correct.rtf > /dev/null && $(TESTEND)
	@rm $(TESTTGT) TEST/rtfprocess-output.rtf

test_utf8test: STATIC/regex/*.c TEST/utf8test.c
	$(TESTSTART)
	@$(TESTCC) STATIC/regex/*.c TEST/utf8test.c -o $(TESTTGT)
	@$(TESTTGT) && $(TESTEND)
	@rm $(TESTTGT) 

test_cpgtoutest: STATIC/regex/*.c TEST/cpgtoutest.c
	$(TESTSTART)
	@$(TESTCC) STATIC/regex/*.c TEST/cpgtoutest.c -o $(TESTTGT)
	@$(TESTTGT) && $(TESTEND)
	@rm $(TESTTGT) 

test_speedtest:
	$(TESTSTART)
	@$(TESTCC) $(SOURCE) -o $(TESTTGT)
	@strip $(TESTTGT)
	@echo
	@time $(TESTTGT) TEST/bigfile-input.rtf TEST/bigfile-output.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf TEST/bigfile-output.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf TEST/bigfile-output.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf TEST/bigfile-output.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf TEST/bigfile-output.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf TEST/bigfile-output.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf TEST/bigfile-output.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf TEST/bigfile-output.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf TEST/bigfile-output.rtf
	@time $(TESTTGT) TEST/bigfile-input.rtf TEST/bigfile-output.rtf
	@rm $(TESTTGT) TEST/bigfile-output.rtf


clean:
	@rm -Rf .DS_Store core *.o *~
	@rm -Rf *.dSYM/ */*.dSYM
	@rm -Rf Info.plist */Info.plist
	@rm -Rf *-output* */*-output*
	@rm -Rf $(TESTTGT)
	@rm -Rf $(EXEC)
	@echo Repository cleaned

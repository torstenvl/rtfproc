#############################################################################
##          Operating system detection and OS-specific constants
#############################################################################
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	HOSTOS		=	linux
	CC			=	gcc
	TIMECMD		=	/usr/bin/time --format "\t%e real\t%U user\t%S sys"
	SUCCESS		=	"\342\234\205 OK\n"
	FAILURE		=	"\342\235\214\033[1;31m FAILED!!!\033[m\n"
else ifeq ($(UNAME_S),NetBSD)
	HOSTOS		=	bsd
	CC			=	gcc
	TIMECMD		=	/usr/bin/time --format "\t%e real\t%U user\t%S sys"
	SUCCESS		=	"\342\234\205 OK\n"
	FAILURE		=	"\342\235\214\033[1;31m FAILED!!!\033[m\n"
else ifeq ($(UNAME_S),OpenBSD)
	HOSTOS		=	bsd
	CC			=	clang
	TIMECMD		=	/usr/bin/time --format "\t%e real\t%U user\t%S sys"
	SUCCESS		=	"\342\234\205 OK\n"
	FAILURE		=	"\342\235\214\033[1;31m FAILED!!!\033[m\n"
else ifeq ($(UNAME_S),FreeBSD)
	HOSTOS		=	bsd
	CC			=	clang
	TIMECMD		=	/usr/bin/time --format "\t%e real\t%U user\t%S sys"
	SUCCESS		=	"\342\234\205 OK\n"
	FAILURE		=	"\342\235\214\033[1;31m FAILED!!!\033[m\n"
else ifeq ($(UNAME_S),Darwin)
	HOSTOS 		=	macos
	CC			=	clang
	TIMECMD		=	time
	SUCCESS		=	"\342\234\205 OK\n"
	FAILURE		=	"\342\235\214\033[1;31m FAILED!!!\033[m\n"
else 
	HOSTOS 		=	unknown
	CC			=	cc
	TIMECMD		=	
	SUCCESS		=	"\. OK\n"
	FAILURE		=	"X FAILED!!!\n"
endif

#############################################################################
##                          Compiler-specific flags
#############################################################################
ifeq ($(CC),clang)
	OPT_FLAG	=	-Ofast
	DBUG_FLAG	=	-gfull -O0
	WEVERYTHING	=	-Weverything 
	WEXCLUDE	=	-Wno-gnu-binary-literal                    \
					-Wno-c++98-compat                          \
					-Wno-c99-compat                            \
					-Wno-padded                                \
	                -Wno-poison-system-directories
else ifeq ($(CC),gcc)
	OPT_FLAG	=	-Os
	DBUG_FLAG	=	-pg -O0
	WEVERYTHING	=	
	WEXCLUDE	=	-Wno-parentheses                           \
				  	-Wno-padded                                \
	            	-Wno-unused-value
else ifeq ($(CC),msvcpp)
	OPT_FLAG	=	/Os
	DBUG_FLAG	=	/Zi /O0
	WEVERYTHING	=	
endif


#############################################################################
##      Basic build flags for different levels of strictness/debugging
#############################################################################
CFLAGS    = -std=c2x -funsigned-char 
RELEASE   = $(CFLAGS) $(OPT_FLAG) -DNDEBUG
DEBUG     = $(CFLAGS) $(DBUG_FLAG)
STRICT1   = -W -Wall -Werror 
STRICT2   = $(STRICT1) -pedantic                                        \
			-Wstrict-prototypes -Wmissing-prototypes -Wchar-subscripts  \
			-Wpointer-arith -Wcast-qual -Wswitch -Wshadow -Wcast-align  \
			-Wreturn-type -Wwrite-strings -Winline -Wredundant-decls    \
			-Wmisleading-indentation -Wunused-parameter -Wnested-externs 
STRICT3   = $(STRICT2) -Wextra 
STRICT4   = $(STRICT3) $(WEVERYTHING)



#############################################################################
##                             Target constants 
#############################################################################
EXEC	=	$(shell basename `pwd`)
HEADERS	=	*.h STATIC/*/*.h 
SOURCE	=	*.c STATIC/*/*.c
ALLSRC	=   $(SOURCE) $(HEADERS)



#############################################################################
##                               Build targets 
#############################################################################
all:		release
release:	$(ALLSRC)
	@$(CC)	$(SOURCE) $(RELEASE) 						-o $(EXEC)
	@strip	$(EXEC)
pponly:		$(ALLSRC)
	@$(CC)	$(SOURCE) $(RELEASE) -E						> $(EXEC)full.c
debug:	$(ALLSRC)
	@$(CC)	$(SOURCE) $(DEBUG) 							-o $(EXEC)
stackdebug:	$(ALLSRC)
	@$(CC)	$(SOURCE) $(DEBUG) -DUL__NEED_STACKTRACE	-o $(EXEC)
fortify:	$(ALLSRC)
	@$(CC)	$(SOURCE) $(DEBUG) -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=4 -fsanitize=address -o $(EXEC)
strict:		$(ALLSRC)
	@$(CC)	$(SOURCE) $(RELEASE) $(STRICT1)	$(WEXCLUDE)	-o $(EXEC)
stricter:	$(ALLSRC)
	@$(CC)	$(SOURCE) $(RELEASE) $(STRICT2)	$(WEXCLUDE)	-o $(EXEC)
strictest:	$(ALLSRC)
	@$(CC)	$(SOURCE) $(RELEASE) $(STRICT3)	$(WEXCLUDE)	-o $(EXEC)
ultrastrict:$(ALLSRC)
	@$(CC)	$(SOURCE) $(RELEASE) $(STRICT4)	$(WEXCLUDE)	-o $(EXEC)
insane:		$(ALLSRC)
	@$(CC)  $(SOURCE) $(RELEASE) $(STRICT4)				-o $(EXEC)
demo:		$(ALLSRC)
	@$(CC)	$(SOURCE) $(RELEASE) -DRTFAUTOOPEN	-o $(EXEC)
	@strip	$(EXEC)
	@./$(EXEC)
test:		testsuiteprologue testsuite testsuiteepilogue
testsuiteprologue:
	@echo
	@echo "RUNNING TEST SUITE"
	@echo "------------------"
testsuite:	test_rtfprocess test_utf8test test_cpgtoutest test_speedtest
testsuiteepilogue:
	@echo



TESTCC		=	$(CC) $(RELEASE) $(WEXCLUDE)
TESTSTART	=	@printf "%s %-36s" "Testing" $(subst test_,,$@)
TESTEND		=	printf $(SUCCESS) || printf $(FAILURE)
TESTTGT		=	TEST/testexec



test_rtfprocess: $(ALLSRC)
	$(TESTSTART)
	@$(TESTCC) $(SOURCE) -o $(TESTTGT)
	@$(TESTTGT) < TEST/rtfprocess-input.rtf > test_result.tmp
	@diff test_result.tmp TEST/rtfprocess-correct.rtf > /dev/null && $(TESTEND)
#	@rm $(TESTTGT) test_result.tmp

test_utf8test: STATIC/crex/*.c TEST/utf8test.c STATIC/cpgtou/cpgtou.c
	$(TESTSTART)
	@$(TESTCC) STATIC/crex/*.c TEST/utf8test.c STATIC/cpgtou/cpgtou.c -o $(TESTTGT)
	@$(TESTTGT) && $(TESTEND)
	@rm $(TESTTGT) 

test_cpgtoutest: STATIC/crex/*.c TEST/cpgtoutest.c STATIC/cpgtou/*.c
	$(TESTSTART)
	@$(TESTCC) STATIC/crex/*.c TEST/cpgtoutest.c STATIC/cpgtou/*.c -o $(TESTTGT)
	@$(TESTTGT) && $(TESTEND)
	@rm $(TESTTGT) 

test_speedtest:
	$(TESTSTART)
	@$(TESTCC) $(SOURCE) -o $(TESTTGT)
	@strip $(TESTTGT)
	@echo
	@$(TIMECMD) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIMECMD) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIMECMD) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIMECMD) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIMECMD) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIMECMD) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIMECMD) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIMECMD) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIMECMD) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIMECMD) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@rm $(TESTTGT) temp.rtf


clean:
	@rm -Rf .DS_Store core *.o *~
	@rm -Rf *.dSYM/ */*.dSYM
	@rm -Rf Info.plist */Info.plist
	@rm -Rf *-output* TEST/*-output*
	@rm -Rf $(TESTTGT)
	@rm -Rf $(EXEC)
	@echo Repository cleaned

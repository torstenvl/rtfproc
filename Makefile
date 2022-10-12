#=============================================================================
#                               PROJECT SETTINGS
#=============================================================================
EXEC    :=  $(shell basename `pwd`)
HDR     :=  $(wildcard *.h)
SRC     :=  $(wildcard *.c)
LIBDIR  :=  $(wildcard STATIC/*)
LIBSRC  :=  $(wildcard STATIC/*/*.c)
LIBHDR  :=  $(wildcard STATIC/*/*.h)
ALLSRC  :=  $(SRC) $(LIBSRC)
ALLHDR  :=  $(HDR) $(LIBHDR)


#=============================================================================
#                          OS-SPECIFIC CONFIGURATION
#=============================================================================
ifeq ($(shell uname -s),Linux)
    TIME         :=  /usr/bin/time --format "\t%e real\t%U user\t%S sys"
    OPEN         :=  xdg-open
else ifeq ($(shell uname -s),FreeBSD)
    TIME         :=  /usr/bin/time
    OPEN         :=  xdg-open
else ifeq ($(shell uname -s),Darwin)
    TIME         :=  /usr/bin/time
    OPEN         :=  open
endif


#=============================================================================
#                       COMPILER-SPECIFIC CONFIGURATION
#=============================================================================
ifeq ($(shell cc -v 2>&1 | grep clang),"")
    CC           :=  gcc
    OPTFLAG      :=  -DNDEBUG -Ofast
    DBUGFLAG     :=  -pg -Ofast
    WEXCLUDE     :=  -Wno-parentheses -Wno-padded -Wno-unused-value        \
                     -Wno-unused-function
    WEVERYTHING  :=  -Wextra
else
    CC           :=  clang
    OPTFLAG      :=  -DNDEBUG -Ofast
    DBUGFLAG     :=  -gfull -Ofast
    WEXCLUDE     :=  -Wno-gnu-binary-literal -Wno-c++98-compat -Wno-padded \
                     -Wno-c99-compat -Wno-poison-system-directories        \
                     -Wno-unused-function
    WEVERYTHING  :=  -Wextra -Weverything
endif


#=============================================================================
#                                 BUILD FLAGS
#=============================================================================
CFLAGS   :=  -funsigned-char -Wno-gnu-label-as-value $(subst STATIC,-I./STATIC,$(LIBDIR)) $(ADDFLAGS)
RELEASE  :=  $(CFLAGS) $(OPTFLAG)
DEBUG    :=  $(CFLAGS) $(DBUGFLAG)
STRICT1  :=  -W -Wall
STRICT2  :=  $(STRICT1) -pedantic                                          \
             -Wstrict-prototypes -Wmissing-prototypes -Wchar-subscripts    \
             -Wpointer-arith -Wcast-qual -Wswitch -Wshadow -Wcast-align    \
             -Wreturn-type -Wwrite-strings -Winline -Wredundant-decls      \
             -Wmisleading-indentation -Wunused-parameter -Wnested-externs
STRICT3  :=  $(STRICT2) $(WEVERYTHING) -Werror 


#=============================================================================
#                                BUILD TARGETS
#=============================================================================
.PHONY:     release debug strict stricter strictest clean test
.PHONY:     stackdebug demo
all:        strictest
release:    $(ALLSRC) $(ALLHDR)
	@$(CC)  $(ALLSRC) $(RELEASE)                        -o $(EXEC)
	@strip  $(EXEC)
debug:      $(ALLSRC) $(ALLHDR)
	@$(CC)  $(ALLSRC) $(DEBUG)                          -o $(EXEC)
perfrun:    $(ALLSRC) $(ALLHDR)
	@$(CC)  $(ALLSRC) $(CFLAGS) $(OPTFLAG) $(DBUGFLAG)  -o ./perfrun
	@strip  ./perfrun
	@sample perfrun 30 1 -wait -mayDie -fullPaths -file /tmp/processsample.txt &
	@./perfrun TEST/bigfile-input.rtf ./bigfile-output.rtf
	@sleep 1
	@$(OPEN) /tmp/processsample.txt
	@rm perfrun ./bigfile-output.rtf
stackdebug: $(ALLSRC) $(ALLHDR)
	@$(CC)  $(ALLSRC) $(DEBUG)   -DUL__NEED_STACKTRACE  -o $(EXEC)
strict:     $(ALLSRC) $(ALLHDR)
	@$(CC)  $(ALLSRC) $(RELEASE) $(STRICT1) $(WEXCLUDE) -o $(EXEC)
stricter:   $(ALLSRC) $(ALLHDR)
	@$(CC)  $(ALLSRC) $(RELEASE) $(STRICT2) $(WEXCLUDE) -o $(EXEC)
strictest:  $(ALLSRC) $(ALLHDR)
	@$(CC)  $(ALLSRC) $(RELEASE) $(STRICT3) $(WEXCLUDE) -o $(EXEC)
demo: $(ALLSRC) $(ALLHDR)
	@$(CC)  $(ALLSRC) $(RELEASE)                        -o $(EXEC)
	@./$(EXEC) TEST/test-letter-input.rtf test-letter-output.rtf
	@$(OPEN) rtfprocess-output.rtf
clean:
	@rm -Rf .DS_Store core *.o *~ *.dSYM/ */*.dSYM Info.plist */Info.plist
	@rm -Rf *-output* TEST/*-output* temp.rtf *-output.rtf $(EXEC) $(TESTTGT)
	@echo Repository cleaned


#=============================================================================
#                                 TEST HARNESS
#=============================================================================
TESTTGT    :=  TEST/testexec
TESTDIR    :=  $(wildcard TEST)
TESTINCL   :=  -I. $(subst TEST,-I./TEST,$(TESTDIR))
TESTCC     :=  $(CC) $(RELEASE) $(STRICT3) $(WEXCLUDE) $(TESTINCL)
SUCCESS    :=  "\342\234\205 OK\n"
FAILURE    :=  "\342\235\214\033[1;31m FAILED!!!\033[m\n"
TESTEND    :=  printf $(SUCCESS) || printf $(FAILURE)
TESTSTART   =  printf "%s %-36s" "Testing" $(subst test_,,$@...)

test: testsuiteprologue testsuite testsuiteepilogue
testsuiteprologue:
	@printf "\nRUNNING TEST SUITE\n——————————————————\n"
testsuiteepilogue:
	@printf "\n\n"
	@rm $(TESTTGT)


#=============================================================================
#                                    TESTS
#=============================================================================
testsuite: test_utf8test test_cpgtoutest test_rtfprocess test_latepartialmatches test_speedtest

test_rtfprocess: $(ALLSRC) $(ALLHDR)
	@$(TESTSTART)
	@$(TESTCC) $(ALLSRC) -o $(TESTTGT)
	@strip $(TESTTGT)
	@$(TESTTGT) TEST/test-letter-input.rtf test-letter-output.rtf
	@diff TEST/test-letter-correct.rtf test-letter-output.rtf > /dev/null && rm test-letter-output.rtf && $(TESTEND)

test_utf8test: $(LIBSRC) TEST/utf8test.c
	@$(TESTSTART)
	@$(TESTCC) $(LIBSRC) TEST/utf8test.c -o $(TESTTGT)
	@$(TESTTGT) && $(TESTEND)

test_cpgtoutest: $(LIBSRC) TEST/cpgtoutest.c
	@$(TESTSTART)
	@$(TESTCC) $(LIBSRC) TEST/cpgtoutest.c -o $(TESTTGT)
	@$(TESTTGT) && $(TESTEND)

test_latepartialmatches: $(LIBSRC) TEST/latepartial.c rtfsed.c
	@$(TESTSTART)
	@$(TESTCC) $(LIBSRC) TEST/latepartial.c rtfsed.c -o $(TESTTGT)
	@$(TESTTGT)
	@diff TEST/test-latepartial-correct.rtf test-latepartial-output.rtf > /dev/null && rm test-latepartial-output.rtf && $(TESTEND)

test_speedtest: $(ALLSRC) $(ALLHDR)
	@$(TESTSTART)
	@echo
	@$(TESTCC) $(ALLSRC) -o $(TESTTGT)
	@strip $(TESTTGT)
	@$(TIME) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIME) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIME) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIME) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIME) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIME) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIME) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIME) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIME) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@$(TIME) $(TESTTGT) TEST/bigfile-input.rtf temp.rtf
	@diff TEST/bigfile-input.rtf temp.rtf && rm temp.rtf || echo "FILES DON'T MATCH!" 

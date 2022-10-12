#=============================================================================
#                               PROJECT SETTINGS
#=============================================================================
EXEC    :=  $(shell basename `pwd`)
LIBDIR  :=  $(wildcard STATIC/*)
LIBSRC  :=  $(wildcard STATIC/*/*.c)
LIBHDR  :=  $(wildcard STATIC/*/*.h)

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
ifeq ($(shell clang -v 2>&1 | grep clang),"")
    CC           :=  gcc
    OPTFLAG      :=  -Ofast
    DBUGFLAG     :=  -pg -Ofast
    WEXCLUDE     :=  -Wno-parentheses -Wno-unused-value -Wno-padded          \
                     -Wno-unused-function
else
    CC           :=  clang
    OPTFLAG      :=  -Ofast
    DBUGFLAG     :=  -gfull -Ofast
    WEXCLUDE     :=  -Wno-poison-system-directories -Wno-padded              \
                     -Wno-c99-compat
endif


#=============================================================================
#                                 BUILD FLAGS
#=============================================================================
CFLAGS   :=  -funsigned-char -Wno-gnu-label-as-value $(subst STATIC,-I./STATIC,$(LIBDIR))
RELEASE  :=  $(CFLAGS) $(OPTFLAG) -DNDEBUG
DEBUG    :=  $(CFLAGS) $(DBUGFLAG)
STACKDBG :=  $(DEBUG)  -DUL__NEED_STACKTRACE
STRICT1  :=  -W -Wall $(WEXCLUDE)
STRICT2  :=  $(STRICT1) -pedantic                                            \
             -Wstrict-prototypes -Wmissing-prototypes -Wchar-subscripts      \
             -Wpointer-arith -Wcast-qual -Wswitch -Wshadow -Wcast-align      \
             -Wreturn-type -Wwrite-strings -Winline -Wredundant-decls        \
             -Wmisleading-indentation -Wunused-parameter -Wnested-externs
STRICT3  :=  $(STRICT2) -Weverything -Werror 


#=============================================================================
#                                BUILD TARGETS
#=============================================================================
.PHONY:     release debug stackdebug demo strict stricter strictest clean test
all:        release
release:    main.c $(LIBSRC) $(LIBHDR)
	@$(CC)  main.c $(LIBSRC) $(RELEASE)                 -o main
debug:      main.c $(LIBSRC) $(LIBHDR)
	@$(CC)  main.c $(LIBSRC) $(DEBUG)                   -o main
stackdebug: main.c $(LIBSRC) $(LIBHDR)
	@$(CC)  main.c $(LIBSRC) $(STACKDBG)                -o main
strict:     main.c $(LIBSRC) $(LIBHDR)
	@$(CC)  main.c $(LIBSRC) $(RELEASE) $(STRICT1)      -o main
stricter:   main.c $(LIBSRC) $(LIBHDR)
	@$(CC)  main.c $(LIBSRC) $(RELEASE) $(STRICT2)      -o main
strictest:  main.c $(LIBSRC) $(LIBHDR)
	@$(CC)  main.c $(LIBSRC) $(RELEASE) $(STRICT3)      -o main
demo:       release
	@./$(EXEC) TEST/test-letter-input.rtf test-letter-output.rtf
	@$(OPEN) rtfprocess-output.rtf
clean:
	@rm -Rf .DS_Store core *.o *~ *.dSYM/ */*.dSYM Info.plist */Info.plist
	@rm -Rf main testexec
	@echo Repository cleaned


#=============================================================================
#                                 TEST HARNESS
#=============================================================================
SUCCESS    :=  "\342\234\205 OK\n"
FAILURE    :=  "\342\235\214\033[1;31m FAILED!!!\033[m\n"
TESTCC     :=  $(CC) $(RELEASE) $(STRICT3) $(LIBSRC)
TESTTGT    :=  ./testexec
TESTDIR    :=  $(wildcard TEST)
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

test_utf8test:   $(LIBSRC) $(LIBHDR) TEST/utf8test.c 
	@$(TESTSTART)
	@$(TESTCC) TEST/utf8test.c -o $(TESTTGT)
	@$(TESTTGT) && $(TESTEND)

test_cpgtoutest: $(LIBSRC) $(LIBHDR) TEST/cpgtoutest.c 
	@$(TESTSTART)
	@$(TESTCC) TEST/cpgtoutest.c -o $(TESTTGT)
	@$(TESTTGT) && $(TESTEND)

test_rtfprocess: $(LIBSRC) $(LIBHDR) main.c 
	@$(TESTSTART)
	@$(TESTCC) main.c -o $(TESTTGT)
	@$(TESTTGT) TEST/letter-input.rtf TEST/letter-output.rtf
	@diff TEST/letter-output.rtf TEST/letter-correct.rtf > /dev/null && \
	      rm TEST/letter-output.rtf && \
		  $(TESTEND)
	
test_latepartialmatches: $(LIBSRC) $(LIBHDR) TEST/latepartial.c
	@$(TESTSTART)
	@$(TESTCC) TEST/latepartial.c -o $(TESTTGT)
	@$(TESTTGT)
	@diff TEST/latepartial-output.rtf TEST/latepartial-correct.rtf > /dev/null && \
	      rm TEST/latepartial-output.rtf && \
		  $(TESTEND)

test_speedtest: $(LIBSRC) $(LIBHDR) main.c 
	@$(TESTSTART)
	@echo
	@$(CC) $(RELEASE) $(STRICT3) $(LIBSRC) main.c -o $(TESTTGT)
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



# perfrun:    main.c $(LIBSRC) $(LIBHDR)
# 	@$(CC)  main.c $(LIBSRC) $(CFLAGS) $(OPTFLAG)       -o perfrun
# 	@strip  perfrun
# 	@sample perfrun 30 1 -wait -mayDie -fullPaths -file /tmp/processsample.txt &
# 	@./perfrun TEST/bigfile-input.rtf ./bigfile-output.rtf
# 	@sleep 1
# 	@$(OPEN) /tmp/processsample.txt
# 	@rm perfrun ./bigfile-output.rtf
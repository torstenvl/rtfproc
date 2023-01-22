#–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
#                                CONFIGURATION
#–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
override \
CFLAGS     +=  -funsigned-char \
               -W -Wall -Wextra -Werror \
               -Wno-unknown-warning -Wno-unknown-warning-option -Wno-padded \
               -Wno-parentheses -Wno-c99-compat -Wno-unused-function \
               -Isrc -Itemplib \
			   -DBUILDSTAMP=$(shell date +"%Y%m%d.%H%M%S")

ifdef STACKDEBUG
override CFLAGS += -O1 -ggdb3 -DulNEEDTRACE
else ifdef DEBUG
override CFLAGS += -O1 -ggdb3
else
override CFLAGS += -Ofast -DNDEBUG
endif

FORCEPREP  :=  $(shell mkdir -p templib && find . -iregex ".*src/.*\.[ch].*" -exec cp -a {} templib/ \;)
VPATH       =  templib



#–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
#                                   TARGETS
#–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
.PHONY:			clean test DT
all:			test

%.o : %.c
	@$(CC) $(CFLAGS) -c $<

clean:
	@rm -fr   .DS_Store    Thumbs.db    core    *.dSYM    *.o
	@rm -fr */.DS_Store  */Thumbs.db  */core  */*.dSYM  */*.o
	-@$(foreach dir,$(shell find modules -type d -depth 1 2>/dev/null),$(MAKE) clean -C $(dir) 2>/dev/null;)
	@rm -fr $(TESTEXE) templib



#–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
#                                 TEST HARNESS
#–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
TESTEXE     =   ./testprog
TESTSTART   =   printf "%s %-36s" "Testing" $(subst test_,,$@...)
TESTEND     =   printf "\342\234\205 OK\n" || printf "\342\235\214\033[1;31m FAILED!!!\033[m\n"
TESTCC      =   $(CC) $(CFLAGS) -o $(TESTEXE)
test: 			testbegin testsuite testfinish
testbegin:	;	@printf "RUNNING TEST SUITE\n——————————————————\n"
testfinish:	;	@rm -fr $(TESTEXE) templib temp.rtf



#–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
#                                SPECIFIC TESTS
#–––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––––
testsuite: test_utf8test      \
           test_cpgtoutest    \
		   test_letter        \
		   test_latepartial   \
		   test_speedtest

test_utf8test:		test/utf8test.c
	@$(TESTSTART)
	@$(TESTCC)		test/utf8test.c
	@$(TESTEXE) && $(TESTEND)

test_cpgtoutest:	cpgtou.o test/cpgtoutest.c
	@$(TESTSTART)
	@$(TESTCC)		cpgtou.o test/cpgtoutest.c
	@$(TESTEXE) && $(TESTEND)

test_letter:		rtfproc.o cpgtou.o trex.o test/letter.c
	@$(TESTSTART)
	@$(TESTCC)		rtfproc.o cpgtou.o trex.o test/letter.c
	@$(TESTEXE) test/letter-input.rtf temp.rtf && \
	 diff temp.rtf test/letter-correct.rtf && \
	 $(TESTEND)
	
test_latepartial:	rtfproc.o cpgtou.o trex.o test/latepartial.c
	@$(TESTSTART)
	@$(TESTCC)		rtfproc.o cpgtou.o trex.o test/latepartial.c
	@$(TESTEXE) && \
	 diff temp.rtf test/latepartial-correct.rtf && \
	 $(TESTEND)

test_speedtest:		rtfproc.o cpgtou.o trex.o test/letter.c
	@$(TESTSTART)
	@$(TESTCC)		rtfproc.o cpgtou.o trex.o test/letter.c
	@strip $(TESTEXE)
	@echo
	@time $(TESTEXE) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTEXE) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTEXE) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTEXE) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTEXE) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTEXE) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTEXE) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTEXE) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTEXE) TEST/bigfile-input.rtf temp.rtf
	@time $(TESTEXE) TEST/bigfile-input.rtf temp.rtf
	@diff TEST/bigfile-input.rtf temp.rtf

# perfrun:    main.c $(LIBSRC) $(LIBHDR)
# 	@$(CC)  main.c $(LIBSRC) $(CFLAGS) $(OPTFLAG)       -o perfrun
# 	@strip  perfrun
# 	@sample perfrun 30 1 -wait -mayDie -fullPaths -file /tmp/processsample.txt &
# 	@./perfrun TEST/bigfile-input.rtf ./bigfile-output.rtf
# 	@sleep 1
# 	@$(OPEN) /tmp/processsample.txt
# 	@rm perfrun ./bigfile-output.rtf

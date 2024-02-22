###############################################################################
#  University of Hawaii, College of Engineering
#  Lab 4 - Memory Scanner - EE 491F (Software Reverse Engineering) - Spr 2024
#
## Build and test a memory scanning program
##
## @see     https://www.gnu.org/software/make/manual/make.html
##
## @file   Makefile
## @author Mark Nelson <marknels@hawaii.edu>
###############################################################################

TARGET = memscan

all: clang

clang: CC=clang
clang: $(TARGET)

gcc: CC=gcc
gcc: $(TARGET)

CFLAGS = -Wall -Wextra -Werror -march=native -mtune=native $(DEBUG_FLAGS)
LINTER = clang-tidy --quiet

debug: DEBUG_FLAGS = -g -DDEBUG
debug: clean $(TARGET)

memscan.o: memscan.c
	$(CC) $(CFLAGS) -c $^

memscan: memscan.o
	$(CC) $(CFLAGS) -o $(TARGET) $^

lint: $(TARGET)
	$(LINTER) memscan.c --

doc: $(TARGET)
	rsync --recursive --mkpath --checksum --delete --compress --stats --chmod=o+r,Do+x .doxygen/images .doxygen/docs/html/.doxygen
	doxygen .doxygen/Doxyfile
	
publish: doc
	rsync --recursive --checksum --delete --compress --stats --chmod=o+r,Do+x .doxygen/docs/html/ marknels@uhunix.hawaii.edu:~/public_html/sre/memscan

test: $(TARGET)
	./$(TARGET)

valgrind: $(TARGET)
	DEBUGINFOD_URLS="https://debuginfod.archlinux.org" \
	valgrind --leak-check=full    \
	         --track-origins=yes  \
	         --error-exitcode=1   \
	         --quiet              \
	./$(TARGET)

clean:
	rm -fr $(TARGET) *.o .doxygen/docs

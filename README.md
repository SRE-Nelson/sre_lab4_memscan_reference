Memscan - Memory Scanner
========================

<img src=".doxygen/images/logo_memscan_660x495.png" style="width:300px; float: left; margin: 0 10px 10px 0;" alt="Tinfoil Hat Cat"/>

We live in an ocean of illegal addresses dotted with islands of legal
addresses.  Let's explore every grain of sand on our islands.

#### Links
The project's home page:  https://github.com/marknelsonengineer-sp23/sre_lab4_memscan  (hosted by GitHub)

The source code is documented at:  https://www2.hawaii.edu/~marknels/sre/memscan  (hosted by The University of Hawaii at Mnoa)

# Memscan
Memscan is an assignment for my Software Reverse Engineering class.
It reads from `/proc/self/maps` and reports information about 
the virtual memory regions for the current process.

memscan can be run with no command line options and will generate:

    $ ./memscan    
    Memory scanner
     0: 0x567af350d000 - 0x567af350dfff  r--p  Number of bytes read      4,096  Count of 0x41 is       0
     1: 0x567af350e000 - 0x567af350efff  r-xp  Number of bytes read      4,096  Count of 0x41 is       2
     2: 0x567af350f000 - 0x567af350ffff  r--p  Number of bytes read      4,096  Count of 0x41 is       3
    ...
    20: 0x7ffe6d709000 - 0x7ffe6d729fff  rw-p  Number of bytes read    135,168  Count of 0x41 is     107
    21: 0x7ffe6d7be000 - 0x7ffe6d7c1fff  r--p  [vvar] excluded
    22: 0x7ffe6d7c2000 - 0x7ffe6d7c3fff  r-xp  Number of bytes read      8,192  Count of 0x41 is      33
    23: 0xffffffffff600000 - 0xffffffffff600fff  --xp  read permission not set on [vsyscall]

...which reports the valid virtual memory regions for `self` (the memscan process),
their permissions and the size of the regions.


# Details
This lab looks easy at first, but (hopefully) proves to be quite challenging.

As you know, user-processes run in "Virtual Memory" process space  a process 
space that is unique for each process.  
In actuality, the process space is made up of "regions".  Some the regions are 
completely unique to the process (such as the read-write data pages and the 
stack for the process).  There are other regions that are shared amongst many 
processes.

It's the Kernel's responsibility to manage (and assign/map) the memory regions 
for each process  however, there's nothing secret about how they are mapped.  
Therefore, Linux publishes the memory map information on the filesystem for 
every process.  Information about Linux processes are contained in a directory 
called `/proc`.

`/proc` contains a "special process directory" for every process currently 
running and another directory called `self` that links to the currently running
process.  Each of these directories contain information about the process, such 
as the environment `environ`, command line `cmdline`, and overall status `status`.

`maps` describes a region of contiguous virtual memory in a process or thread.  
Here's an example:

    55cb8756b000-55cb87598000 r--p 00000000 08:04 1574849  /usr/bin/bash
    55cb87598000-55cb87669000 r-xp 0002d000 08:04 1574849  /usr/bin/bash
    55cb87669000-55cb876a2000 r--p 000fe000 08:04 1574849  /usr/bin/bash
    55cb876a2000-55cb876a6000 r--p 00136000 08:04 1574849  /usr/bin/bash
    55cb876a6000-55cb876af000 rw-p 0013a000 08:04 1574849  /usr/bin/bash
    55cb876af000-55cb876b9000 rw-p 00000000 00:00 0        
    55cb8832a000-55cb88494000 rw-p 00000000 00:00 0        [heap]
    7f3ff0ecf000-7f3ffe3ff000 r--p 00000000 08:04 1838829  /usr/lib/locale/locale
    7f3ffe3ff000-7f3ffecd3000 r--s 00000000 08:07 131095   /var/lib/sss/mc/passwd
    7f3ffecd3000-7f3ffecd5000 r--p 00000000 08:04 1588322  /usr/lib64/libnss.so.2

Each row has the following fields:

| Command       | Purpose                                                                                                                                                                                                                                                                                                                                                                                                                                                      |
|---------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `address`     | The starting and ending address of the region in the process's address space (See Notes below).                                                                                                                                                                                                                                                                                                                                                              |
| `permissions` | This describes how pages in the region can be accessed.  There are four different permissions: `r`ead, `w`rite, e`x`ecute, and `s`hared. If read/write/execute are disabled, a `-` will appear instead of the `r`/`w`/`x`. If a region is not shared, it is private, and a `p` will appear instead of an `s`.  If the process attempts to access memory in a way that is not permitted, the memory manager will interrupt the CPU with a segmentation fault. |
| `offset`      | If the region was mapped from a file (using [mmap()][2]), this is the offset in the file where the mapping begins.  If the memory was not mapped from a file, it's `0`.                                                                                                                                                                                                                                                                                      |
| `device`      | If the region was mapped from a file, this is the major and minor device number (in hex) where the file lives.                                                                                                                                                                                                                                                                                                                                               |
| `inode`       | If the region was mapped from a file, this is the file number.                                                                                                                                                                                                                                                                                                                                                                                               |
| `pathname`    | If the region was mapped from a file, this is the name of the file. This field is blank for anonymously mapped regions. There are also special regions with names like `[heap]`, `[stack]`, or `[vdso]`.                                                                                                                                                                                                                                                     |


Notes:
  - `maps` reports addresses like this:  [ `00403000-00404000` )... the "end address" one byte past the valid range.  
    When memscan prints a range, it shows inclusive addresses like this: [ `00403000-00403fff` ]
  - `[vdso]` stands for virtual dynamic shared object.  It's used by system calls to switch to kernel mode.
  - Permissions can be changed using the [mprotect()][1] system call.

[1]: https://man7.org/linux/man-pages/man2/mprotect.2.html
[2]: https://man7.org/linux/man-pages/man2/mmap.2.html


## The Assignment

You are to write a program from scratch.  Feel free to incorporate artifacts 
from other labs such as `.gitignore`, header blocks or a simple `Makefile`.  
Your lab must include a Makefile, which must have (at least) the following 
targets:
  - `make clean`
  - `make`
  - `make test`

You can name your program whatever you want and `$ make test` must run it.

The program will open and read `/proc/self/maps` and parse the contents.

You can skip the `[vvar]` region.  You can also skip non-readable regions (of course).

Finally, scan each region and print the following information:

    $ ./memscan
    Memory scanner
     0: 0x567af350d000 - 0x567af350dfff  r--p  Number of bytes read      4,096  Count of 0x41 is       0
     1: 0x567af350e000 - 0x567af350efff  r-xp  Number of bytes read      4,096  Count of 0x41 is       2
     2: 0x567af350f000 - 0x567af350ffff  r--p  Number of bytes read      4,096  Count of 0x41 is       3
     3: 0x567af3510000 - 0x567af3510fff  r--p  Number of bytes read      4,096  Count of 0x41 is       8
     4: 0x567af3511000 - 0x567af3511fff  rw-p  Number of bytes read      4,096  Count of 0x41 is       9
     5: 0x567af3512000 - 0x567af3555fff  rw-p  Number of bytes read    278,528  Count of 0x41 is      26
     6: 0x567af4031000 - 0x567af4051fff  rw-p  Number of bytes read    135,168  Count of 0x41 is     361
     7: 0x73410ba00000 - 0x73410bceafff  r--p  Number of bytes read  3,059,712  Count of 0x41 is   3,472
     8: 0x73410bcec000 - 0x73410bceefff  rw-p  Number of bytes read     12,288  Count of 0x41 is      25
     9: 0x73410bcef000 - 0x73410bd12fff  r--p  Number of bytes read    147,456  Count of 0x41 is     138
    10: 0x73410bd13000 - 0x73410be6dfff  r-xp  Number of bytes read  1,421,312  Count of 0x41 is  19,496
    11: 0x73410be6e000 - 0x73410bec2fff  r--p  Number of bytes read    348,160  Count of 0x41 is   5,143
    12: 0x73410bec3000 - 0x73410bec6fff  r--p  Number of bytes read     16,384  Count of 0x41 is   1,126
    13: 0x73410bec7000 - 0x73410bec8fff  rw-p  Number of bytes read      8,192  Count of 0x41 is     359
    14: 0x73410bec9000 - 0x73410bed2fff  rw-p  Number of bytes read     40,960  Count of 0x41 is     122
    15: 0x73410bedd000 - 0x73410beddfff  r--p  Number of bytes read      4,096  Count of 0x41 is       3
    16: 0x73410bede000 - 0x73410bf04fff  r-xp  Number of bytes read    159,744  Count of 0x41 is   1,919
    17: 0x73410bf05000 - 0x73410bf0ffff  r--p  Number of bytes read     45,056  Count of 0x41 is     690
    18: 0x73410bf10000 - 0x73410bf11fff  r--p  Number of bytes read      8,192  Count of 0x41 is      58
    19: 0x73410bf12000 - 0x73410bf13fff  rw-p  Number of bytes read      8,192  Count of 0x41 is     118
    20: 0x7ffe6d709000 - 0x7ffe6d729fff  rw-p  Number of bytes read    135,168  Count of 0x41 is     107
    21: 0x7ffe6d7be000 - 0x7ffe6d7c1fff  r--p  [vvar] excluded
    22: 0x7ffe6d7c2000 - 0x7ffe6d7c3fff  r-xp  Number of bytes read      8,192  Count of 0x41 is      33
    23: 0xffffffffff600000 - 0xffffffffff600fff  --xp  read permission not set on [vsyscall]

This program counts the number of `A`s in each region.  It doesn't matter what 
you do as long as you read every single byte you can read.

Your program should compile clean (no warnings) and must not core dump when it runs.

Note:  The goal of this program is to focus on 2 things:
- Parsing the `maps` file
- Scanning memory

To that end, your program does not have to match the output perfectly... here are some exceptions:
- You don't have to use commas in your numbers, but if you wanted to try this:  `Number of bytes read %'10d`.
- Ideally, you should print a reason you are skipping a region, but you don't have to do that either.
- You can print the end of the region as the actual end (as I do) or the end that you get from the `maps` file.
- The columns don't have to line up perfectly


# Makefile {#MakeTargets}
Memscan uses the following `Makefile` targets:

| Command        | Purpose                                                       |
|----------------|---------------------------------------------------------------|
| `make`         | Compile memscan (with clang)                                  |
| `make clang`   | Compile w/ clang                                              |
| `make gcc`     | Compile memscan w/ gcc                                        |
| `make test`    | Compile memscan and run it.  Run as `root` to pass all tests. |
| `make debug`   | Compile memscan with debug mode ( `DEBUG` is defined)         |
| `make clean`   | Remove all compiler-generated files                           |
| `make doc`     | Make a Doxygen website                                        |
| `make publish` | Push the Doxygen website to UH UNIX                           |
| `make lint`    | Use `clang-tidy` to do static analysis on the source code     |
| `make valgrind`| Use `valgrind` to do dynamic analysis on the source code      |


# Toolchain
This project is the product of a tremendous amount of R&D and would not be 
possible without the following world-class tools:

| Tool           | Website                    |                                                          Logo                                                          |
|----------------|----------------------------|:----------------------------------------------------------------------------------------------------------------------:|
| **gcc**        | https://gcc.gnu.org        |        <img src=".doxygen/images/logo_gcc.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="GCC"/>        |
| **llvm/clang** | https://clang.llvm.org     |      <img src=".doxygen/images/logo_llvm.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="clang"/>       |
| **Boost**      | https://boost.org          |      <img src=".doxygen/images/logo_boost.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="Boost"/>      |
| **Doxygen**    | https://doxygen.nl         |    <img src=".doxygen/images/logo_doxygen.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="Doxygen"/>    |
| **DOT**        | https://graphviz.org       |        <img src=".doxygen/images/logo_dot.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="Dot"/>        |
| **Git**        | https://git-scm.com        |        <img src=".doxygen/images/logo_git.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="Git"/>        |
| **GitHub**     | https://github.com         |     <img src=".doxygen/images/logo_github.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="GitHub"/>     |
| **Linux**      | https://kernel.org         |      <img src=".doxygen/images/logo_linux.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="Linux"/>      |
| **ArchLinux**  | https://archlinux.org      |  <img src=".doxygen/images/logo_archlinux.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="ArchLinux"/>  |
| **VirtualBox** | https://www.virtualbox.org | <img src=".doxygen/images/logo_virtualbox.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="VirtualBox"/> |
| **Valgrind**   | https://valgrind.org       |   <img src=".doxygen/images/logo_valgrind.png" style="height:40px; float: center; margin: 0 0 0 0;" alt="Valgrind"/>   |

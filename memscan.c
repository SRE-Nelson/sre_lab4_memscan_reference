///////////////////////////////////////////////////////////////////////////////
//   University of Hawaii, College of Engineering
//   Lab 4 - Memory Scanner - EE 491F (Software Reverse Engineering) - Spr 2024
//
/// We live in an ocean of illegal addresses dotted with islands of legal
/// addresses.  Let's explore every grain of sand on our islands.
///
/// Each row in `/proc/$PID/maps` describes a region of contiguous
/// virtual memory in a process or thread.  When `$PID` is `self`
/// the file is the memory map for the current process.
///
/// For example:
/// ````
/// address                  perms offset   dev   inode      pathname
/// 00403000-00404000         r--p 00002000 00:30 641        /mnt/src/Src/tmp/t
/// 00404000-00405000         rw-p 00003000 00:30 641        /mnt/src/Src/tmp/t
/// 00405000-00587000         rw-p 00000000 00:00 0
/// 00cc3000-00ce4000         rw-p 00000000 00:00 0          [heap]
/// 7f8670628000-7f867062a000 rw-p 00000000 00:00 0
/// 7f867062a000-7f8670650000 r--p 00000000 00:1f 172006     /usr/lib64/libc-2.32.so
/// 7f8670650000-7f867079f000 r-xp 00026000 00:1f 172006     /usr/lib64/libc-2.32.so
/// ````
///
/// Each row has the following fields:
///   - `address`:     The starting and ending address of the region in the
///                    process's address space
///   - `permissions`: This describes how pages in the region can be accessed.
///                    There are four different permissions: `r`ead, `w`rite,
///                    e`x`ecute, and `s`hared. If read/write/execute are disabled,
///                    a `-` will appear instead of the `r`/`w`/`x`. If a region is not
///                    shared, it is private, and a `p` will appear instead of an `s`.
///                    If the process attempts to access memory in a way that is
///                    not permitted, the memory manager will interrupt the CPU
///                    with a segmentation fault.
///                    Permissions can be changed using the [mprotect()][1] system call.
///   - `offset`:      If the region was mapped from a file (using [mmap()][2]), this
///                    is the offset in the file where the mapping begins.
///                    If the memory was not mapped from a file, it's `0`.
///   - `device`:      If the region was mapped from a file, this is the major
///                    and minor device number (in hex) where the file lives.
///   - `inode`:       If the region was mapped from a file, this is the file number.
///   - `pathname`:    If the region was mapped from a file, this is the name
///                    of the file. This field is blank for anonymous mapped
///                    regions. There are also special regions with names like
///                    `[heap]`, `[stack]`, or `[vdso]`. `[vdso]` stands for virtual
///                    dynamic shared object.  It's used by system calls to
///                    switch to kernel mode.
///
/// Notes:
///   - maps reports addresses like this:  [ `00403000-00404000` )...
///     the "end address" one byte past the valid range.  When memscan prints
///     the range, it shows inclusive addresses like this: [ `00403000-00403fff` ]
///
/// [1]: https://man7.org/linux/man-pages/man2/mprotect.2.html
/// [2]: https://man7.org/linux/man-pages/man2/mmap.2.html
///
/// @file   memscan.c
/// @author Mark Nelson <marknels@hawaii.edu>
///////////////////////////////////////////////////////////////////////////////

#include <locale.h>   // For set_locale() LC_NUMERIC
#include <stdbool.h>  // For bool
#include <stdio.h>    // For printf() fopen() and FILE
#include <stdlib.h>   // For EXIT_SUCCESS and EXIT_FAILURE
#include <string.h>   // For strtok() strcmp() and memset()


/// The `maps` file we intend to read from `/proc`
#define MEMORY_MAP_FILE "/proc/self/maps"

/// The longest allowed length from #MEMORY_MAP_FILE
#define MAX_LINE_LENGTH 1024

/// The maximum number of MapEntry records in #map
#define MAX_ENTRIES     256

/// The byte to scan for
#define CHAR_TO_SCAN_FOR 'A'

/// Define an array of paths that should be excluded from the scan.
/// On x86 architectures, we should avoid the `[vvar]` path.
///
/// The list ends with an empty value.
///
/// @see https://lwn.net/Articles/615809/
char* ExcludePaths[] = { "[vvar]", "" };


/// Holds the original (and some processed data) from each map entry
struct MapEntry {
   char  szLine[MAX_LINE_LENGTH];  ///< String buffer for the entire line
                                   ///  All of the string pointers in this
                                   ///  struct will point to strings in this
                                   ///  buffer after they've been tokenized
   char* sAddressStart;  ///< String pointer to the start of the address range
   char* sAddressEnd;    ///< String pointer to the end of the address range
   void* pAddressStart;  ///< Pointer to the start of the memory mapped region
   void* pAddressEnd;    ///< Pointer to the byte just *after* the end of
                         ///  the memory mapped region
   char* sPermissions;   ///< String pointer to the permissions
   char* sOffset;        ///< String pointer to the offset
   char* sDevice;        ///< String pointer to the device name
   char* sInode;         ///< String pointer to the iNode number
   char* sPath;          ///< String pointer to the path (may be NULL)
} ;


/// Holds up to #MAX_ENTRIES of MapEntry structs
struct MapEntry map[MAX_ENTRIES] ;


/// The number of entries in #map
size_t numMaps = 0 ;


/// The name of the program (copied from `argv[0]`)
char* programName = NULL ;


/// Parse each line from #MEMORY_MAP_FILE, mapping the data into
/// a MapEntry field.  This function makes heavy use of [strtok()][3].
///
/// [3]: https://man7.org/linux/man-pages/man3/strtok_r.3.html
void readEntries() {
   FILE* file = NULL ;  // File handle to #MEMORY_MAP_FILE

   file = fopen( MEMORY_MAP_FILE, "r" ) ;

   if( file == NULL ) {
      printf( "%s: Unable to open [%s].  Exiting.\n", programName, MEMORY_MAP_FILE ) ;
      exit( EXIT_FAILURE );
   }

   char* pRead ;

   pRead = fgets( (char *)&map[numMaps].szLine, MAX_LINE_LENGTH, file ) ;

   while( pRead != NULL ) {
      #ifdef DEBUG
         printf( "%s", map[numMaps].szLine ) ;
      #endif

      map[numMaps].sAddressStart = strtok( map[numMaps].szLine, "-" ) ;
      map[numMaps].sAddressEnd   = strtok( NULL, " "   ) ;
      map[numMaps].sPermissions  = strtok( NULL, " "   ) ;
      map[numMaps].sOffset       = strtok( NULL, " "   ) ;
      map[numMaps].sDevice       = strtok( NULL, " "   ) ;
      map[numMaps].sInode        = strtok( NULL, " "   ) ;
      map[numMaps].sPath         = strtok( NULL, " \n" ) ;
      /// @todo Add tests to check if anything returns `NULL` or does anything
      ///       out of the ordinary

      // Convert the strings holding the start & end address into pointers
      int retVal1;
      int retVal2;
      retVal1 = sscanf( map[numMaps].sAddressStart, "%p", &(map[numMaps].pAddressStart) ) ;
      retVal2 = sscanf( map[numMaps].sAddressEnd,   "%p", &(map[numMaps].pAddressEnd  ) ) ;

      if( retVal1 != 1 || retVal2 != 1 ) {
         printf( "Map entry %zu is unable parse start [%s] or end address [%s].  Exiting.\n"
               ,numMaps
               ,map[numMaps].sAddressStart
               ,map[numMaps].sAddressEnd ) ;
         exit( EXIT_FAILURE );
      }

      #ifdef DEBUG
         printf( "DEBUG:  " ) ;
         printf( "numMaps[%zu]  ",       numMaps );
         printf( "sAddressStart=[%s]  ", map[numMaps].sAddressStart ) ;
         printf( "pAddressStart=[%p]  ", map[numMaps].pAddressStart ) ;
         printf( "sAddressEnd=[%s]  ",   map[numMaps].sAddressEnd ) ;
         printf( "pAddressEnd=[%p]  ",   map[numMaps].pAddressEnd ) ;
         printf( "sPermissions=[%s]  ",  map[numMaps].sPermissions ) ;
         printf( "sOffset=[%s]  ",       map[numMaps].sOffset ) ;
         printf( "sDevice=[%s]  ",       map[numMaps].sDevice ) ;
         printf( "sInode=[%s]  ",        map[numMaps].sInode ) ;
         printf( "sPath=[%s]  ",         map[numMaps].sPath ) ;
         printf( "\n" ) ;
      #endif

      numMaps++;
      pRead = fgets( (char *)&map[numMaps].szLine, MAX_LINE_LENGTH, file );
   } // while()

   int iRetVal;
   iRetVal = fclose( file ) ;
   if( iRetVal != 0 ) {
      printf( "%s: Unable to close [%s].  Exiting.\n", programName, MEMORY_MAP_FILE ) ;
      exit( EXIT_FAILURE ) ;
   }

} // readEntries()


/// This is the workhorse of this program... Scan all readable memory
/// regions, counting the number of bytes scanned and the number of
/// times #CHAR_TO_SCAN_FOR appears in the region...
void scanEntries() {
   for( size_t i = 0 ; i < numMaps ; i++ ) {
      printf( "%2zu: %p - %p  %s  ",
               i
              ,map[i].pAddressStart
              ,map[i].pAddressEnd - 1
              ,map[i].sPermissions
      ) ;

      int numBytesScanned = 0 ;  // Number of bytes scanned in this region
      int numBytesFound   = 0 ;  // Number of CHAR_TO_SCAN_FOR found in this region

      // Skip non-readable regions
      if( map[i].sPermissions[0] != 'r' ) {
         printf( "read permission not set on %s", map[i].sPath );
         goto finishRegion;
      }

      // Skip excluded paths
      if( map[i].sPath != NULL ) {
         for( size_t j = 0 ; ExcludePaths[j][0] != '\0' ; j++ ) {
            if( strcmp( map[i].sPath, ExcludePaths[j] ) == 0 ) {
               printf( "%s excluded", map[i].sPath );
               goto finishRegion;
            }
         }
      }

      char aByteInMemory = 0;
      bool writable = map[i].sPermissions[1] == 'w';

      for( void* scanThisAddress = map[i].pAddressStart ; scanThisAddress < map[i].pAddressEnd ; scanThisAddress++ ) {
         aByteInMemory = *(char*)scanThisAddress;         // Read memory
         if( writable ) {
            *(char*)scanThisAddress = aByteInMemory;      // Write memory
//          *(char*)scanThisAddress = aByteInMemory + 1;  // Just for fun, try this
         }
         if( aByteInMemory == CHAR_TO_SCAN_FOR ) {
            numBytesFound++ ;
         }
         numBytesScanned++ ;
      }

      printf( "Number of bytes read %'10d  Count of 0x%02x is %'7d",
           numBytesScanned
          ,CHAR_TO_SCAN_FOR
          ,numBytesFound) ;

      finishRegion:
      printf( "  %s", map[i].sPath != NULL ? map[i].sPath : "" );
      printf( "\n" );
   } // for()
} // scanEntries()


/// A basic memory scanner
///
/// @param argc The number of arguments passed to `memscan`
/// @param argv An array of arguments passed to `memscan`
/// @return The program's return code
int main( int argc, char* argv[] ) {
   printf( "Memory scanner\n" ) ;

   if( argc != 1 ) {
      printf( "%s: Usage memscan\n", argv[0] ) ;
      return EXIT_FAILURE ;
   }

   programName = argv[0];

   char* sRetVal;
   sRetVal = setlocale( LC_NUMERIC, "" ) ;
   if( sRetVal == NULL ) {
      printf( "%s: Unable to set locale.  Exiting.\n", programName ) ;
      return EXIT_FAILURE ;
   }

   memset( map, 0, sizeof( map ));  // Zero out the map data structure

   readEntries() ;

   scanEntries() ;

   return EXIT_SUCCESS ;
} // main()

// When reviewing this for the class:
//   - Describe what happens with a fork()
//   - Then parlay that into a discussion of backing memory (file vs. swap)

#ifndef __fbstream_h
#define __fbstream_h

#include "config.h"
#include "bstream.h"
#include <fstream>

namespace libbase {

/*!
   \brief   Bitstream File-handling.
   \author  Johann Briffa

   \par Version Control:
   - $Revision$
   - $Date$
   - $Author$

  Version 1.01 (27 Nov 2001)
  added a function which returns the number of bits left in the buffer for ifbstream.
  Also made eof, fail, bad, good, all const functions. Also made the filename for open
  and creator functions (const char *).

  Version 1.02 (30 Nov 2001)
  edited write/read buffer routines - now read will only get 8 bits at a time, and
  write will only output as many characters as necessary to eject the buffer.

  Version 1.03 (3 Dec 2001)
  fixed opening of bitstream files - now they are opened as binary files.

  Version 1.04 (6 Mar 2002)
  changed vcs version variable from a global to a static class variable.
  also changed use of iostream from global to std namespace.

  Version 1.10 (26 Oct 2006)
  * defined class and associated data within "libbase" namespace.
  * removed use of "using namespace std", replacing by tighter "using" statements as needed.
*/

class fbstream {
};

class ofbstream : virtual public obstream, public fbstream {
   std::ofstream c;
   void write_buffer();
public:
   ofbstream(const char *name);
   ~ofbstream();
   void open(const char *name);
   void close();
   bool eof() const { return c.eof() != 0; };
   bool fail() const { return c.fail() != 0; };
   bool bad() const { return c.bad() != 0; };
   bool good() const { return c.good() != 0; };
};

class ifbstream : virtual public ibstream, public fbstream {
   std::ifstream c;
   void read_buffer();
public:
   ifbstream(const char *name);
   ~ifbstream();
   void open(const char *name);
   void close();
   bool eof() const { return c.eof() != 0; };
   bool fail() const { return c.fail() != 0; };
   bool bad() const { return c.bad() != 0; };
   bool good() const { return c.good() != 0; };
   int buffer_bits() const { return ptr; };
};

}; // end namespace

#endif

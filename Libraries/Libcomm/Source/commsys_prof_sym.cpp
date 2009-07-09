/*!
 \file

 \section svn Version Control
 - $Revision$
 - $Date$
 - $Author$
 */

#include "commsys_prof_sym.h"
#include "fsm.h"
#include <sstream>

namespace libcomm {

// commsys functions

void commsys_prof_sym::updateresults(libbase::vector<double>& result,
      const int i, const libbase::vector<int>& source, const libbase::vector<
            int>& decoded) const
   {
   assert(i >= 0 && i < get_iter());
   const int skip = count() / get_iter();
   // Update the count for every bit in error
   assert(source.size() == get_symbolsperblock());
   assert(decoded.size() == get_symbolsperblock());
   for (int t = 0; t < get_symbolsperblock(); t++)
      {
      assert(source(t) != fsm::tail);
      if (source(t) != decoded(t))
         result(skip * i + source(t))++;
      }
   }

/*!
 \copydoc experiment::result_description()

 The description is a string SER_X_Y, where 'X' is the symbol value
 (starting at zero), and 'Y' is the iteration, starting at 1.
 */
std::string commsys_prof_sym::result_description(int i) const
   {
   assert(i >= 0 && i < count());
   std::ostringstream sout;
   const int x = i % get_alphabetsize();
   const int y = (i / get_alphabetsize()) + 1;
   sout << "SER_" << x << "_" << y;
   return sout.str();
   }

} // end namespace

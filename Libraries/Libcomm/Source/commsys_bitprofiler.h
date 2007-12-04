#ifndef __commsys_bitprofiler_h
#define __commsys_bitprofiler_h

#include "config.h"
#include "vcs.h"
#include "commsys.h"

/*
  Version 1.00 (17 Jun 1999)

  Version 1.10 (2 Sep 1999)
  added a hook for clients to know the number of frames simulated in a particular run.

  Version 1.11 (1 Mar 2002)   
  edited the classes to be compileable with Microsoft extensions enabled - in practice,
  the major change is in for() loops, where MS defines scope differently from ANSI.
  Rather than taking the loop variables into function scope, we chose to avoid having
  more than one loop per function, by defining private helper functions (or doing away 
  with them if there are better ways of doing the same operation).

  Version 1.12 (6 Mar 2002)
  changed vcs version variable from a global to a static class variable.
  also changed use of iostream from global to std namespace.

  Version 1.20 (19 Mar 2002)
  changed constructor to take also the modem and an optional puncturing system, besides
  the already present random source (for generating the source stream), the channel
  model, and the codec. This change was necessitated by the definition of codec 1.41.
  Also changed the sample loop to bail out after 0.5s rather than after at least 1000
  modulation symbols have been transmitted.

  Version 1.30 (19 Mar 2002)
  changed system to use commsys 1.41 as its base class, overriding cycleonce().

  Version 1.40 (30 Oct 2006)
  * defined class and associated data within "libcomm" namespace.
  * removed use of "using namespace std", replacing by tighter "using" statements as needed.
*/

namespace libcomm {

class commsys_bitprofiler : public commsys {
   static const libbase::vcs version;
protected:
   void cycleonce(libbase::vector<double>& result);
public:
   commsys_bitprofiler(libbase::randgen *src, codec *cdc, modulator *modem, puncture *punc, channel *chan);
   ~commsys_bitprofiler() {};
   int count() const { return (tau-m)*iter; };
};

}; // end namespace

#endif

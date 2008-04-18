/*!
   \file

   \par Version Control:
   - $Revision$
   - $Date$
   - $Author$
*/

#include "turbo.h"
#include "flat.h"
#include <sstream>

namespace libcomm {

using std::cerr;
using libbase::trace;
using libbase::vector;
using libbase::matrix;
using libbase::matrix3;

// initialization / de-allocation

template <class real, class dbl> void turbo<real,dbl>::init()
   {
   bcjr<real,dbl>::init(*encoder, tau);

   assertalways(enc_parity()*num_inputs() == enc_outputs());
   assertalways(num_sets() > 0);
   assertalways(tau > 0);
   // TODO: check interleavers
   assertalways(!endatzero || !circular);
   assertalways(iter > 0);

   seed(0);
   initialised = false;
   }

template <class real, class dbl> void turbo<real,dbl>::free()
   {
   if(encoder != NULL)
      delete encoder;
   for(int i=0; i<inter.size(); i++)
      delete inter(i);
   }

template <class real, class dbl> void turbo<real,dbl>::reset()
   {
   if(circular)
      {
      assert(initialised);
      for(int set=0; set<num_sets(); set++)
         {
         ss(set) = dbl(1.0/double(enc_states()));
         se(set) = dbl(1.0/double(enc_states()));
         }
      }
   else if(endatzero)
      {
      bcjr<real,dbl>::setstart(0);
      bcjr<real,dbl>::setend(0);
      }
   else
      {
      bcjr<real,dbl>::setstart(0);
      bcjr<real,dbl>::setend();
      }
   }


// constructor / destructor

template <class real, class dbl> turbo<real,dbl>::turbo()
   {
   encoder = NULL;
   }

template <class real, class dbl> turbo<real,dbl>::turbo(const fsm& encoder, const int tau, \
   const vector<interleaver *>& inter, const int iter, \
   const bool endatzero, const bool parallel, const bool circular)
   {
   turbo::encoder = encoder.clone();
   turbo::tau = tau;
   turbo::inter = inter;
   turbo::endatzero = endatzero;
   turbo::parallel = parallel;
   turbo::circular = circular;
   turbo::iter = iter;
   init();
   }

// memory allocator (for internal use only)

template <class real, class dbl> void turbo<real,dbl>::allocate()
   {
   r.init(num_sets());
   R.init(num_sets());
   for(int i=0; i<num_sets(); i++)
      {
      r(i).init(tau, num_inputs());
      R(i).init(tau, enc_outputs());
      }

   rp.init(tau, num_inputs());
   ri.init(tau, num_inputs());
   rai.init(tau, num_inputs());
   rii.init(tau, num_inputs());

   if(parallel)
      {
      ra.init(num_sets());
      for(int i=0; i<num_sets(); i++)
         ra(i).init(tau, num_inputs());
      }
   else
      {
      ra.init(1);
      ra(0).init(tau, num_inputs());
      }

   if(circular)
      {
      ss.init(num_sets());
      se.init(num_sets());
      for(int i=0; i<num_sets(); i++)
         {
         ss(i).init(enc_states());
         se(i).init(enc_states());
         }
      }

   initialised = true;
   }

// wrapping functions

/*!
   \brief Computes extrinsic probabilities
   \param[in]  set Parity sequence being decoded
   \param[in]  ra  A-priori (extrinsic) probabilities of input values
   \param[in]  ri  A-posteriori probabilities of input values
   \param[in]  r   A-priori intrinsic probabilities of input values
   \param[out] re  Extrinsic probabilities of input values

   \note Before vectorizing this, the division was only computed at matrix
         elements where the corresponding 'ri' was greater than zero.
         We have no idea why this was done - will need to check old
         documentation.
   \warning See note above - this may affect results/speed.
*/
template <class real, class dbl> void turbo<real,dbl>::work_extrinsic(const matrix<dbl>& ra, const matrix<dbl>& ri, const matrix<dbl>& r, matrix<dbl>& re)
   {
   // the following are repeated at each frame element, for each possible symbol
   //matrix<bool> mask = (ri > 0);
   //re = ri;
   //re.mask(mask).divideby(ra);
   //re.mask(mask).divideby(r);
   //re.mask(!mask) = 0;
   // calculate extrinsic information
   for(int t=0; t<tau; t++)
      for(int x=0; x<num_inputs(); x++)
         if(ri(t, x) > dbl(0))
            re(t, x) = ri(t, x) / (ra(t, x) * r(t, x));
         else
            re(t, x) = 0;
   }

/*!
   \brief Complete BCJR decoding cycle
   \param[in]  set Parity sequence being decoded
   \param[in]  ra  A-priori (extrinsic) probabilities of input values
   \param[out] ri  A-posteriori probabilities of input values
   \param[out] re  Extrinsic probabilities of input values (will be used later
                   as the new 'a-priori' probabilities)

   This method performs a complete decoding cycle, including start/end state
   probability settings for circular decoding, and any interleaving/de-interleaving.
*/
template <class real, class dbl> void turbo<real,dbl>::bcjr_wrap(const int set, const matrix<dbl>& ra, matrix<dbl>& ri, matrix<dbl>& re)
   {
#ifndef NDEBUG
   trace << "DEBUG (turbo): bcjr_wrap - set=" << set << ", ra=" << &ra << ", ri=" << &ri << ", re=" << &re;
   trace << ", ra(mean) = " << ra.mean();
#endif
   // when using a circular trellis, re-initialize the start- and end-state
   // probabilities with the stored values from the previous turn
   if(circular)
      {
      bcjr<real,dbl>::setstart(ss(set));
      bcjr<real,dbl>::setend(se(set));
      }
   // pass through BCJR algorithm
   // perform interleaving and de-interleaving
   inter(set)->transform(ra, rai);
   bcjr<real,dbl>::fdecode(R(set), rai, rii);
   work_extrinsic(rai, rii, r(set), rai);
   inter(set)->inverse(rii, ri);
   inter(set)->inverse(rai, re);
#ifndef NDEBUG
   trace << ", ri(mean) = " << ri.mean() << ", re(mean) = " << re.mean() << ".\n";
#endif
   // when using a circular trellis, store the start- and end-state
   // probabilities for the previous turn
   if(circular)
      {
      ss(set) = bcjr<real,dbl>::getstart();
      se(set) = bcjr<real,dbl>::getend();
      }
   }

template <class real, class dbl> void turbo<real,dbl>::hard_decision(const matrix<dbl>& ri, vector<int>& decoded)
   {
   // Decide which input sequence was most probable.
   for(int t=0; t<tau; t++)
      {
      decoded(t) = 0;
      for(int i=1; i<num_inputs(); i++)
         if(ri(t, i) > ri(t, decoded(t)))
            decoded(t) = i;
      }
#ifndef NDEBUG
   static int iter=0;
   const int ones = decoded.sum();
   trace << "DEBUG (turbo): iter=" << iter \
      << ", decoded ones = " << ones << "/" << tau \
      << ", ri(mean) = " << ri.mean() \
      << ", rp(mean) = " << rp.mean() << '\n';
   if(fabs(ones/double(tau) - 0.5) > 0.05)
      {
      trace << "DEBUG (turbo): decoded = " << decoded << '\n';
      trace << "DEBUG (turbo): ri = " << ri << '\n';
      }
   iter++;
#endif
   }

template <class real, class dbl> void turbo<real,dbl>::decode_serial(matrix<dbl>& ri)
   {
   // after working all sets, ri is the intrinsic+extrinsic information
   // from the last stage decoder.
   for(int set=0; set<num_sets(); set++)
      {
      bcjr_wrap(set, ra(0), ri, ra(0));
      bcjr<real,dbl>::normalize(ra(0));
      }
   bcjr<real,dbl>::normalize(ri);
   }

template <class real, class dbl> void turbo<real,dbl>::decode_parallel(matrix<dbl>& ri)
   {
   // here ri is only a temporary space
   // and ra(set) is updated with the extrinsic information for that set
   for(int set=0; set<num_sets(); set++)
      bcjr_wrap(set, ra(set), ri, ra(set));
   // the following are repeated at each frame element, for each possible symbol
   // work in ri the sum of all extrinsic information
   ri = ra(0);
   for(int set=1; set<num_sets(); set++)
      ri.multiplyby(ra(set));
   // compute the next-stage a priori information by subtracting the extrinsic
   // information of the current stage from the sum of all extrinsic information.
   for(int set=0; set<num_sets(); set++)
      ra(set) = ri.divide(ra(set));
   // add the channel information to the sum of extrinsic information
   ri.multiplyby(rp);
   // normalize results
   for(int set=0; set<num_sets(); set++)
      bcjr<real,dbl>::normalize(ra(set));
   bcjr<real,dbl>::normalize(ri);
   }

// encoding and decoding functions

template <class real, class dbl> void turbo<real,dbl>::seed(const int s)
   {
   for(int set=0; set<num_sets(); set++)
      inter(set)->seed(s+set);
   }

template <class real, class dbl> void turbo<real,dbl>::encode(vector<int>& source, vector<int>& encoded)
   {
   // Initialise result vector
   encoded.init(tau);

   // Allocate space for the encoder outputs
   matrix<int> x(num_sets(), tau);
   // Allocate space for the interleaved sources
   vector<int> source2(tau);

   // Consider sets in order
   for(int set=0; set<num_sets(); set++)
      {
      // Advance interleaver to the next block
      inter(set)->advance();
      // Create interleaved version of source
      inter(set)->transform(source, source2);

      // Reset the encoder to zero state
      encoder->reset(0);

      // When dealing with a circular system, perform first pass to determine end state,
      // then reset to the corresponding circular state.
      int cstate = 0;
      if(circular)
         {
         for(int t=0; t<tau; t++)
            encoder->advance(source2(t));
         encoder->resetcircular();
         cstate = encoder->state();
         }

      // Encode source (non-interleaved must be done first to determine tail bit values)
      for(int t=0; t<tau; t++)
         x(set, t) = encoder->step(source2(t)) / num_inputs();

      // If this was the first (non-interleaved) set, copy back the source
      // to fix the tail bit values, if any
      if(endatzero && set == 0)
         source = source2;

      // check that encoder finishes correctly
      if(circular)
         assertalways(encoder->state() == cstate);
      if(endatzero)
         assertalways(encoder->state() == 0);
      }

   // Encode source stream
   for(int t=0; t<tau; t++)
      {
      // data bits
      encoded(t) = source(t);
      // parity bits
      for(int set=0, mul=num_inputs(); set<num_sets(); set++, mul*=enc_parity())
         encoded(t) += x(set, t)*mul;
      }
   }

template <class real, class dbl> void turbo<real,dbl>::translate(const matrix<double>& ptable)
   {
   // Compute factors / sizes & check validity
   const int S = ptable.ysize();
   const int sp = int(round(log(double(enc_parity()))/log(double(S))));
   const int sk = int(round(log(double(num_inputs()))/log(double(S))));
   const int s = sk + num_sets()*sp;
   if(enc_parity() != pow(double(S), sp) || num_inputs() != pow(double(S), sk))
      {
      cerr << "FATAL ERROR (turbo): each encoder parity (" << enc_parity() << ") and input (" << num_inputs() << ")";
      cerr << " must be represented by an integral number of modulation symbols (" << S << ").";
      cerr << " Suggested number of mod. symbols/encoder input and parity were (" << sp << "," << sk << ").\n";
      exit(1);
      }
   if(ptable.xsize() != tau*s)
      {
      cerr << "FATAL ERROR (turbo): demodulation table should have " << tau*s;
      cerr << " symbols, not " << ptable.xsize() << ".\n";
      exit(1);
      }

   // initialise memory if necessary
   if(!initialised)
      allocate();

   // Allocate space for temporary matrices
   matrix3<dbl> p(num_sets(), tau, enc_parity());

   // Get the necessary data from the channel
   for(int t=0; t<tau; t++)
      {
      // Input (data) bits [set 0 only]
      for(int x=0; x<num_inputs(); x++)
         {
         rp(t, x) = 1;
         for(int i=0, thisx = x; i<sk; i++, thisx /= S)
            rp(t, x) *= ptable(t*s+i, thisx % S);
         }
      // Parity bits [all sets]
      for(int x=0; x<enc_parity(); x++)
         for(int set=0, offset=sk; set<num_sets(); set++)
            {
            p(set, t, x) = 1;
            for(int i=0, thisx = x; i<sp; i++, thisx /= S)
               p(set, t, x) *= ptable(t*s+i+offset, thisx % S);
            offset += sp;
            }
      }

   // Initialise a priori probabilities (extrinsic)
   for(int set=0; set<(parallel ? num_sets() : 1); set++)
      ra(set) = 1.0;

   // Normalize and compute a priori probabilities (intrinsic - source)
   bcjr<real,dbl>::normalize(rp);
   for(int set=0; set<num_sets(); set++)
      inter(set)->transform(rp, r(set));

   // Compute and normalize a priori probabilities (intrinsic - encoded)
   for(int set=0; set<num_sets(); set++)
      {
      for(int t=0; t<tau; t++)
         for(int x=0; x<enc_outputs(); x++)
            R(set)(t, x) = r(set)(t, x%num_inputs()) * p(set, t, x/num_inputs());
      bcjr<real,dbl>::normalize(R(set));
      }

   // Reset start- and end-state probabilities
   reset();
   }

template <class real, class dbl> void turbo<real,dbl>::decode(vector<int>& decoded)
   {
   // Initialise result vector
   decoded.init(tau);

   // initialise memory if necessary
   if(!initialised)
      allocate();

   // do one iteration, in serial or parallel as required
   if(parallel)
      decode_parallel(ri);
   else
      decode_serial(ri);

   // Decide which input sequence was most probable, based on BCJR stats.
   hard_decision(ri, decoded);
   }

// description output

template <class real, class dbl> std::string turbo<real,dbl>::description() const
   {
   std::ostringstream sout;
   sout << "Turbo Code (" << output_bits() << "," << input_bits() << ") - ";
   sout << encoder->description() << ", ";
   for(int i=0; i<inter.size(); i++)
      sout << inter(i)->description() << ", ";
   sout << (endatzero ? "Terminated, " : "Unterminated, ");
   sout << (circular ? "Circular, " : "Non-circular, ");
   sout << (parallel ? "Parallel Decoding, " : "Serial Decoding, ");
   sout << iter << " iterations";
   return sout.str();
   }

// object serialization - saving

template <class real, class dbl> std::ostream& turbo<real,dbl>::serialize(std::ostream& sout) const
   {
   // format version
   sout << 1 << '\n';
   sout << encoder;
   sout << tau << '\n';
   sout << num_sets() << '\n';
   for(int i=0; i<inter.size(); i++)
      sout << inter(i);
   sout << int(endatzero) << '\n';
   sout << int(circular) << '\n';
   sout << int(parallel) << '\n';
   sout << iter << '\n';
   return sout;
   }

// object serialization - loading

template <class real, class dbl> std::istream& turbo<real,dbl>::serialize(std::istream& sin)
   {
   assertalways(sin.good());
   free();
   // get format version
   int version;
   sin >> version;
   // handle old-format files
   if(sin.fail())
      {
      version = 0;
      sin.clear();
      }
   sin >> encoder;
   sin >> tau;
   int sets;
   sin >> sets;
   inter.init(sets);
   if(version == 0)
      {
      inter(0) = new flat(tau);
      for(int i=1; i<inter.size(); i++)
         sin >> inter(i);
      }
   else
      {
      for(int i=0; i<inter.size(); i++)
         sin >> inter(i);
      }
   sin >> endatzero;
   sin >> circular;
   sin >> parallel;
   sin >> iter;
   init();
   assertalways(sin.good());
   return sin;
   }

}; // end namespace

// Explicit Realizations

#include "mpreal.h"
#include "mpgnu.h"
#include "logreal.h"
#include "logrealfast.h"

namespace libcomm {

using libbase::mpreal;
using libbase::mpgnu;
using libbase::logreal;
using libbase::logrealfast;

using libbase::serializer;

template class turbo<double>;
template <> const serializer turbo<double>::shelper = serializer("codec", "turbo<double>", turbo<double>::create);

template class turbo<mpreal>;
template <> const serializer turbo<mpreal>::shelper = serializer("codec", "turbo<mpreal>", turbo<mpreal>::create);

template class turbo<mpgnu>;
template <> const serializer turbo<mpgnu>::shelper = serializer("codec", "turbo<mpgnu>", turbo<mpgnu>::create);

template class turbo<logreal>;
template <> const serializer turbo<logreal>::shelper = serializer("codec", "turbo<logreal>", turbo<logreal>::create);

template class turbo<logrealfast>;
template <> const serializer turbo<logrealfast>::shelper = serializer("codec", "turbo<logrealfast>", turbo<logrealfast>::create);

template class turbo<logrealfast,logrealfast>;
template <> const serializer turbo<logrealfast,logrealfast>::shelper = serializer("codec", "turbo<logrealfast,logrealfast>", turbo<logrealfast,logrealfast>::create);

}; // end namespace

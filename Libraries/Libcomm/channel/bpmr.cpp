/*!
 * \file
 *
 * Copyright (c) 2010 Johann A. Briffa
 *
 * This file is part of SimCommSys.
 *
 * SimCommSys is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimCommSys is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimCommSys.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bpmr.h"
#include "itfunc.h"
#include <sstream>
#include <limits>
#include <exception>
#include <cmath>

namespace libcomm {

// Determine debug level:
// 1 - Normal debug output only
// 2 - Show Markov state and tx/rx vectors during transmission process
// 3 - Show tx/rx vectors and posterior probabilities from receive process
// 4 - Show lattice rows as they are computed in receive process
#ifndef NDEBUG
#  undef DEBUG
#  define DEBUG 1
#endif

// Internal functions

/*!
 * \brief Sets up pre-computed values
 *
 * This function computes all cached quantities used within actual channel
 * operations. Since these values depend on the channel conditions, this
 * function should be called any time a channel parameter is changed.
 */
template <class real>
void bpmr<real>::metric_computer::precompute(double Pd, double Pi, int T,
      int Zmin, int Zmax)
   {
   // block size
   this->T = T;
   // fba decoder parameters
   this->Zmin = Zmin;
   this->Zmax = Zmax;
   // channel parameters
   this->Pd = real(Pd);
   this->Pi = real(Pi);
   }

// Channel receiver for host

#ifndef USE_CUDA

// Batch receiver interface
template <class real>
void bpmr<real>::metric_computer::receive(const array1b_t& tx,
      const array1b_t& rx, const int S0, const bool first, const bool last,
      array1r_t& ptable) const
   {
#if DEBUG>=3
   libbase::trace << "DEBUG (bpmr): starting receive..." << std::endl;
   libbase::trace << "DEBUG (bpmr): tx = " << tx << std::endl;
   libbase::trace << "DEBUG (bpmr): rx = " << rx << std::endl;
   libbase::trace << "DEBUG (bpmr): rx_prev = " << rx_prev << std::endl;
   libbase::trace << "DEBUG (bpmr): S0 = " << S0 << std::endl;
   libbase::trace << "DEBUG (bpmr): last = " << last << std::endl;
#endif
   using std::swap;
   using std::min;
   using std::max;
   // Determine limits over this sequence
   const int mT_max = std::min(T, Zmax - S0);
   const int mT_min = std::max(-T, Zmin - S0);
#if DEBUG>=3
   libbase::trace << "DEBUG (bpmr): mT_max = " << mT_max << std::endl;
   libbase::trace << "DEBUG (bpmr): mT_min = " << mT_min << std::endl;
#endif
   // Compute sizes
   const int n = tx.size();
   const int rho = rx.size();
   // Set up three slices of lattice on the stack as a fixed size;
   // this avoids dynamic allocation (which would otherwise be necessary
   // as the size is non-const)
   assertalways(rho + 1 <= arraysize);
   real F[3][arraysize];
   real *F0 = &F[0][0];
   real *F1 = &F[1][0];
   real *F2 = &F[2][0];
#if DEBUG>=4
   // mark all lattice entries as uninitialized
   for (int i = 0; i < 3; i++)
      for (int j = 0; j <= rho; j++)
         F[i][j] = -1;
#endif
   // *** initialize first row of lattice (i = 0) [insertion only]
   F0[0] = 1;
   const int jmax = min(mT_max, rho);
   if (first)
      {
      // assume equiprobable prior value
      for (int j = 1; j <= jmax; j++)
         F0[j] = F0[j - 1] * real(0.5) * Pi;
      }
   else
      {
      for (int j = 1; j <= jmax; j++)
         F0[j] = 0;
      }
#if DEBUG>=4
   libbase::trace << "DEBUG (bpmr): F = " << std::endl;
   for (int j = 0; j <= rho; j++)
      libbase::trace << F0[j] << '\t';
   libbase::trace << std::endl;
#endif
   // *** compute remaining rows (1 <= i <= n)
   // and -Zmin virtual rows if this is the last codeword
   const int imax = n + (last ? -Zmin : 0);
   for (int i = 1; i <= imax; i++)
      {
      // advance slices
      real *Ft = F2;
      F2 = F1;
      F1 = F0;
      F0 = Ft;
      // determine limits for remaining columns (after first)
      const int jmin = max(i + mT_min, 1);
      const int jmax = min(i + mT_max, rho);
      // overwrite up to three columns before corridor, as needed
      // [first is actually within corridor if (i + mT_min <= 0)]
      for (int j = jmin - 1; j >= 0 && j >= jmin - 3; j--)
         F0[j] = 0;
      // remaining columns
      for (int j = jmin; j <= jmax; j++)
         {
         real temp = 0;
         // in all cases, corresponding tx/rx bits must be equal
         // [repeat last tx bit for virtual rows]
         if (tx(std::min(i, n) - 1) == rx(j - 1))
            {
            // transmission path
            temp += F1[j - 1] * get_transmission_coefficient(j - i + S0);
            // deletion path (if previous node was within corridor)
            if (j - i < mT_max && i >= 2) // (j-1)-(i-2) <= mT_max
               temp += F2[j - 1] * Pd;
            // insertion path (if previous node was within corridor)
            if (j - i > mT_min) // (j-1)-i >= mT_min
               temp += F0[j - 1] * Pi;
            }
         // implicit free delete with no transmission at end of last codeword
         if (last && j - i < mT_max && j + S0 == n) // (j)-(i-1) <= mT_max
            temp += F1[j];
         // store result
         F0[j] = temp;
         }
#if DEBUG>=4
      for (int j = 0; j <= rho; j++)
         libbase::trace << F0[j] << '\t';
      libbase::trace << std::endl;
#endif
      }
   // *** copy results and return
   assertalways(ptable.size() == Zmax - Zmin + 1);
   for (int x = Zmin; x <= Zmax; x++)
      {
      // convert index (x = j - n + S0)
      const int j = x + n - S0;
      if (j >= 0 && j <= rho)
         {
         // factor in delete with no transmission for intermediate codewords
         if (!last && j - n < mT_max) // (j)-(n-1) <= mT_max
            F0[j] += F1[j] * Pd / get_transmission_coefficient(x);;
         ptable(x - Zmin) = F0[j];
         }
      else
         ptable(x - Zmin) = 0;
      }
#if DEBUG>=3
   libbase::trace << "DEBUG (bpmr): ptable = " << ptable << std::endl;
#endif
   }

#endif

/*!
 * \brief Initialization
 *
 * Sets the channel with fixed values for Pd, Pi. This way, if the user
 * never calls set_parameter(), the values are valid.
 */
template <class real>
void bpmr<real>::init()
   {
   // channel parameters
   Pd = fixedPd;
   Pi = fixedPi;
   // initialize metric computer
   computer.init();
   }

/*!
 * \brief Generate Markov state sequence
 *
 * The channel model implemented is described by the following general case
 * state transition probabilities:
 *
 * Pr{Z_i = z+1 | Z_{i-1} = z} = P_i
 * Pr{Z_i = z-1 | Z_{i-1} = z} = P_d
 * Pr{Z_i = z   | Z_{i-1} = z} = 1-P_i-P_d
 *
 * with all other transitions having zero probability. Exceptions to the above
 * occur where z-1 or z+1 are not valid states:
 *
 * For z = Zmax:
 *   Pr{Z_i = z-1 | Z_{i-1} = z} = P_d
 *   Pr{Z_i = z   | Z_{i-1} = z} = 1-P_d
 * For z = Zmin:
 *   Pr{Z_i = z+1 | Z_{i-1} = z} = P_i
 *   Pr{Z_i = z   | Z_{i-1} = z} = 1-P_i
 *
 * It is assumed here (though not explicitly stated in Iyengar et al) that the
 * initial condition for the channel is Z_0 = 0 (where Z_1 refers to the first
 * input bit X_1 and output bit Y_1).
 */
template <class real>
void bpmr<real>::generate_state_sequence(const int tau)
   {
   // Allocate and initialize Markov state sequence
   Z.init(tau);
   Z = 0;
   // determine state sequence
   int Zprev = 0;
   for (int i = 0; i < tau; i++)
      {
      const double p = this->r.fval_closed();
      // upper limit
      if (Zprev == Zmax)
         {
         if (p < Pd)
            Z(i) = Zprev - 1;
         else
            Z(i) = Zprev;
         }
      else
      // lower limit
      if (Zprev == Zmin)
         {
         if (p < Pi)
            Z(i) = Zprev + 1;
         else
            Z(i) = Zprev;
         }
      else // general case
         {
         if (p < Pi)
            Z(i) = Zprev + 1;
         else if (p < (Pi + Pd))
            Z(i) = Zprev - 1;
         else
            Z(i) = Zprev;
         }
      Zprev = Z(i);
      }
#if DEBUG>=2
   libbase::trace << "DEBUG (bpmr): Z = " << Z << std::endl;
#endif
   }

// Constructors / Destructors

/*!
 * \brief Principal constructor
 *
 * \sa init()
 */
template <class real>
bpmr<real>::bpmr(const bool varyPd, const bool varyPi) :
      varyPd(varyPd), varyPi(varyPi), fixedPd(0), fixedPi(0)
   {
   // channel update flags
   assert(varyPd || varyPi);
   // other initialization
   init();
   }

// Channel parameter handling

/*!
 * \brief Set channel parameter
 *
 * This function sets any of Pd, or Pi that are flagged to change. Any of
 * these parameters that are not flagged to change will instead be set to the
 * specified fixed value.
 *
 * \note We set fixed values every time to ensure that there is no leakage
 * between successive uses of this class. (i.e. once this function is called,
 * the class will be in a known determined state).
 */
template <class real>
void bpmr<real>::set_parameter(const double p)
   {
   set_pd(varyPd ? p : fixedPd);
   set_pi(varyPi ? p : fixedPi);
   libbase::trace << "DEBUG (bpmr): Pd = " << Pd << ", Pi = " << Pi
         << std::endl;
   }

/*!
 * \brief Get channel parameter
 *
 * This returns the value of the first of Pd, or Pi that are flagged to
 * change. If none of these are flagged to change, this constitutes an error
 * condition.
 */
template <class real>
double bpmr<real>::get_parameter() const
   {
   assert(varyPd || varyPi);
   if (varyPd)
      return Pd;
   // must be varyPi
   return Pi;
   }

// Channel functions

/*!
 * \copydoc channel::transmit()
 *
 * The channel output has the following correspondence with the input:
 *
 * Y_i = X_{i - Z_i}
 *
 * where any input X_i for an index 'i' before the defined range is
 * equiprobable (represeting the unknown prior magnetic state of the first
 * island, while input X_i for index 'i' after the defined range is the same
 * as the last valid input X_i.
 *
 * \note We have to make sure that we don't corrupt the vector we're reading
 * from (in the case where tx and rx are the same vector); therefore,
 * the result is first created as a new vector and only copied over at
 * the end.
 *
 * \sa corrupt()
 */
template <class real>
void bpmr<real>::transmit(const array1b_t& tx, array1b_t& rx)
   {
   const int tau = tx.size();
   // Generate Markov state sequence
   generate_state_sequence(tau);
   // Initialize results vector
   array1b_t newrx(tau);
   // Compute the output vector (simulate the channel)
   for (int i = 0; i < tau; i++)
      {
      // determine the corresponding index into input vector
      const int j = i - Z(i);
      // valid index
      if (j >= 0 && j < tau)
         newrx(i) = tx(j);
      // early index -> equiprobable
      else if (j < 0)
         newrx(i) = (this->r.fval_closed() < 0.5);
      // late index -> repeat last valid input
      else
         newrx(i) = tx(tau - 1);
      }
   // copy results back
   rx = newrx;
#if DEBUG>=2
   libbase::trace << "DEBUG (bpmr): tx = " << tx << std::endl;
   libbase::trace << "DEBUG (bpmr): rx = " << rx << std::endl;
#endif
   }

// description output

template <class real>
std::string bpmr<real>::description() const
   {
   std::ostringstream sout;
   sout << "BPMR channel (";
   // List set of valid states
   sout << "Z in [" << Zmin << ".." << Zmax << "], ";
   // List varying components
   if (varyPi)
      sout << "Pi=";
   if (varyPd)
      sout << "Pd=";
   sout << "p";
   // List non-varying components, with their value
   if (!varyPd)
      sout << ", Pd=" << fixedPd;
   if (!varyPi)
      sout << ", Pi=" << fixedPi;
   sout << ")";
#ifdef USE_CUDA
   sout << " [CUDA]";
#endif
   return sout.str();
   }

// object serialization - saving

template <class real>
std::ostream& bpmr<real>::serialize(std::ostream& sout) const
   {
   sout << "# Version" << std::endl;
   sout << 1 << std::endl;
   sout << "# Zmin" << std::endl;
   sout << Zmin << std::endl;
   sout << "# Zmax" << std::endl;
   sout << Zmax << std::endl;
   sout << "# Vary Pd?" << std::endl;
   sout << varyPd << std::endl;
   sout << "# Vary Pi?" << std::endl;
   sout << varyPi << std::endl;
   sout << "# Fixed Pd value" << std::endl;
   sout << fixedPd << std::endl;
   sout << "# Fixed Pi value" << std::endl;
   sout << fixedPi << std::endl;
   return sout;
   }

// object serialization - loading

/*!
 * \version 1 Initial version
 */
template <class real>
std::istream& bpmr<real>::serialize(std::istream& sin)
   {
   // get format version
   int version;
   sin >> libbase::eatcomments >> version >> libbase::verify;
   // read state range
   sin >> libbase::eatcomments >> Zmin >> libbase::verify;
   sin >> libbase::eatcomments >> Zmax >> libbase::verify;
   // read flags
   sin >> libbase::eatcomments >> varyPd >> libbase::verify;
   sin >> libbase::eatcomments >> varyPi >> libbase::verify;
   // read fixed Pd,Pi
   sin >> libbase::eatcomments >> fixedPd >> libbase::verify;
   sin >> libbase::eatcomments >> fixedPi >> libbase::verify;
   // sanity checks
   assertalways(Zmin <= 0);
   assertalways(Zmax > Zmin);
   // initialise the object and return
   init();
   return sin;
   }

} // end namespace

#include "mpgnu.h"
#include "logrealfast.h"

namespace libcomm {

// Explicit Realizations
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>

using libbase::serializer;
using libbase::mpgnu;
using libbase::logrealfast;

#ifdef USE_CUDA
#define REAL_TYPE_SEQ \
   (float)(double)
#else
#define REAL_TYPE_SEQ \
   (float)(double)(mpgnu)(logrealfast)
#endif

/* Serialization string: bpmr<real>
 * where:
 *      real = float | double | [mpgnu | logrealfast (CPU only)]
 */
#define INSTANTIATE(r, x, type) \
      template class bpmr<type>; \
      template <> \
      const serializer bpmr<type>::shelper( \
            "channel", \
            "bpmr<" BOOST_PP_STRINGIZE(type) ">", \
            bpmr<type>::create);

BOOST_PP_SEQ_FOR_EACH(INSTANTIATE, x, REAL_TYPE_SEQ)

} // end namespace

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
 * 
 * \section svn Version Control
 * - $Id: qids.h 6455 2011-12-15 10:35:33Z jabriffa $
 */

#ifndef __qids_h
#define __qids_h

#include "config.h"
#include "channel_stream.h"
#include "serializer.h"
#include "matrix.h"
#include "cuda-all.h"
#include <cmath>

namespace libcomm {

// Determine debug level:
// 1 - Normal debug output only
// 2 - Show result of computation of xmax
// 3 - Show tx and rx vectors when computing RecvPr
// NOTE: since this is a header, it may be included in other classes as well;
//       to avoid problems, the debug level is reset at the end of this file.
#ifndef NDEBUG
#  undef DEBUG
#  define DEBUG 1
#endif

/*!
 * \brief   q-ary insertion/deletion/substitution channel.
 * \author  Johann Briffa
 *
 * \section svn Version Control
 * - $Revision: 6455 $
 * - $Date: 2011-12-15 10:35:33 +0000 (Thu, 15 Dec 2011) $
 * - $Author: jabriffa $
 */

template <class G, class real = float>
class qids : public channel_stream<G> {
private:
   // Shorthand for class hierarchy
   typedef channel_stream<G> Base;
public:
   /*! \name Type definitions */
   typedef libbase::matrix<real> array2r_t;
   typedef libbase::vector<real> array1r_t;
   typedef libbase::vector<G> array1g_t;
   typedef libbase::vector<double> array1d_t;
   typedef libbase::vector<array1d_t> array1vd_t;
   // @}
private:
   /*! \name User-defined parameters */
   bool varyPs; //!< Flag to indicate that \f$ P_s \f$ should change with parameter
   bool varyPd; //!< Flag to indicate that \f$ P_d \f$ should change with parameter
   bool varyPi; //!< Flag to indicate that \f$ P_i \f$ should change with parameter
   int Icap; //!< Maximum usable value of I (0 indicates no cap is placed)
   double fixedPs; //!< Value to use when \f$ P_s \f$ does not change with parameter
   double fixedPd; //!< Value to use when \f$ P_d \f$ does not change with parameter
   double fixedPi; //!< Value to use when \f$ P_i \f$ does not change with parameter
   // @}
public:
   /*! \name Metric computation */
   class metric_computer {
   public:
      /*! \name Channel-state and pre-computed parameters */
      int N; //!< Block size in symbols over which we want to synchronize
      int I; //!< Assumed limit for insertions between two time-steps
      int xmax; //!< Assumed maximum drift over a whole \c N -symbol block
      real Rval; //!< Receiver coefficient value for mu = -1
//#ifdef USE_CUDA
#if 0
      cuda::matrix_auto<real> Rtable; //!< Receiver coefficient set for mu >= 0
#else
      array2r_t Rtable; //!< Receiver coefficient set for mu >= 0
#endif
      // @}
      /*! \name Hardwired parameters */
      static const int arraysize = 2 * 63 + 1; //!< Size of stack-allocated arrays
      static const double Pr; //!< Probability of event outside range
      // @}
   private:
      //! Functor for drift probability computation with prior
      class compute_drift_prob_functor {
      public:
         typedef double (*pdf_func_t)(int, int, double, double);
      private:
         const pdf_func_t func;
         const libbase::vector<double>& sof_pdf;
         const int offset;
      public:
         compute_drift_prob_functor(const pdf_func_t& func,
               const libbase::vector<double>& sof_pdf, const int offset) :
            func(func), sof_pdf(sof_pdf), offset(offset)
            {
            }
         double operator()(int x, int tau, double Pi, double Pd) const
            {
            return compute_drift_prob_with(func, x, tau, Pi, Pd, sof_pdf,
                  offset);
            }
      };
   public:
      /*! \name FBA decoder parameter computation */
      // drift PDF - known start of frame
      static double compute_drift_prob_davey(int x, int tau, double Pi,
            double Pd);
      static double compute_drift_prob_exact(int x, int tau, double Pi,
            double Pd);
      // drift PDF - given start of frame pdf
      template <typename F>
      static double compute_drift_prob_with(const F& compute_pdf, int x,
            int tau, double Pi, double Pd,
            const libbase::vector<double>& sof_pdf, const int offset);
      // limit on successive insertions
      static int compute_I(int tau, double Pi, int Icap);
      // limit on drift
      static int compute_xmax_davey(int tau, double Pi, double Pd);
      template <typename F>
      static int
      compute_xmax_with(const F& compute_pdf, int tau, double Pi, double Pd);
      static int compute_xmax(int tau, double Pi, double Pd,
            const libbase::vector<double>& sof_pdf = libbase::vector<double>(),
            const int offset = 0)
         {
         try
            {
            compute_drift_prob_functor f(compute_drift_prob_exact, sof_pdf,
                  offset);
            const int xmax = compute_xmax_with(f, tau, Pi, Pd);
#if DEBUG>=2
            std::cerr << "DEBUG (qids): [with exact] for N = " << tau
            << ", xmax = " << xmax << "." << std::endl;
#endif
            return xmax;
            }
         catch (std::exception&)
            {
            compute_drift_prob_functor f(compute_drift_prob_davey, sof_pdf,
                  offset);
            const int xmax = compute_xmax_with(f, tau, Pi, Pd);
#if DEBUG>=2
            std::cerr << "DEBUG (qids): [with davey] for N = " << tau
            << ", xmax = " << xmax << "." << std::endl;
#endif
            return xmax;
            }
         }
      static int compute_xmax(int tau, double Pi, double Pd, int I,
            const libbase::vector<double>& sof_pdf = libbase::vector<double>(),
            const int offset = 0);
      // receiver metric pre-computation
      static real compute_Rtable_entry(bool err, int mu, double Ps, double Pd,
            double Pi);
      static void compute_Rtable(array2r_t& Rtable, int I, double Ps,
            double Pd, double Pi);
      // @}
      /*! \name Internal functions */
      //! Check validity of Pi and Pd
      static void validate(double Pd, double Pi)
         {
         assert(Pi >= 0 && Pi < 1.0);
         assert(Pd >= 0 && Pd < 1.0);
         assert(Pi + Pd >= 0 && Pi + Pd < 1.0);
         }
      void precompute(double Ps, double Pd, double Pi, int Icap);
      void init();
      // @}
//#ifdef USE_CUDA
#if 0
      /*! \name Device methods */
#ifdef __CUDACC__
      __device__
      void receive(const cuda::bitfield& tx, const cuda::vector_reference<bool>& rx,
            cuda::vector_reference<real>& ptable) const
         {
         using cuda::min;
         using cuda::max;
         using cuda::swap;
         // Compute sizes
         const int n = tx.size();
         const int rho = rx.size();
         cuda_assert(n <= N);
         cuda_assert(labs(rho - n) <= xmax);
         // Set up two slices of forward matrix, and associated pointers
         // Arrays are allocated on the stack as a fixed size; this avoids dynamic
         // allocation (which would otherwise be necessary as the size is non-const)
         cuda_assertalways(2 * xmax + 1 <= arraysize);
         real F0[arraysize];
         real F1[arraysize];
         real *Fthis = F1;
         real *Fprev = F0;
         // for prior list, reset all elements to zero
         for (int x = 0; x < 2 * xmax + 1; x++)
            {
            Fthis[x] = 0;
            }
         // we also know x[0] = 0; ie. drift before transmitting symbol t0 is zero.
         Fthis[xmax + 0] = 1;
         // compute remaining matrix values
         for (int j = 1; j <= n; ++j)
            {
            // swap 'this' and 'prior' lists
            swap(Fthis, Fprev);
            // for this list, reset all elements to zero
            for (int x = 0; x < 2 * xmax + 1; x++)
               {
               Fthis[x] = 0;
               }
            // event must fit the received sequence:
            // 1. j-1+a >= 0
            // 2. j-1+y < rx.size()
            // limits on insertions and deletions must be respected:
            // 3. y-a <= I
            // 4. y-a >= -1
            // note: a and y are offset by xmax
            const int ymin = max(0, xmax - j);
            const int ymax = min(2 * xmax, xmax + rho - j);
            for (int y = ymin; y <= ymax; ++y)
               {
               real result = 0;
               const int amin = max(max(0, xmax + 1 - j), y - I);
               const int amax = min(2 * xmax, y + 1);
               // check if the last element is a pure deletion
               int amax_act = amax;
               if (y - amax < 0)
                  {
                  result += Fprev[amax] * Rval;
                  amax_act--;
                  }
               // elements requiring comparison of tx and rx symbols
               for (int a = amin; a <= amax_act; ++a)
                  {
                  // received subsequence has
                  // start:  j-1+a
                  // length: y-a+1
                  // therefore last element is: start+length-1 = j+y-1
                  const bool cmp = tx(j - 1) != rx(j + (y - xmax) - 1);
                  result += Fprev[a] * Rtable(cmp, y - a);
                  }
               Fthis[y] = result;
               }
            }
         // copy results and return
         cuda_assertalways(ptable.size() == 2 * xmax + 1);
         for (int x = 0; x < 2 * xmax + 1; x++)
            {
            ptable(x) = Fthis[x];
            }
         }
#endif
      // @}
#endif
      /*! \name Host methods */
      real receive(const array1g_t& tx, const array1g_t& rx) const;
      void
      receive(const array1g_t& tx, const array1g_t& rx, array1r_t& ptable) const;
      // @}
   };
   // @}
private:
   /*! \name Metric computation */
   metric_computer computer;
   double Ps; //!< Symbol substitution probability \f$ P_s \f$
   double Pd; //!< Symbol deletion probability \f$ P_d \f$
   double Pi; //!< Symbol insertion probability \f$ P_i \f$
   // @}
private:
   /*! \name Internal functions */
   void init();
   static array1d_t resize_drift(const array1d_t& in, const int offset,
         const int xmax);
   // @}
protected:
   // Channel function overrides
   G corrupt(const G& s);
   double pdf(const G& tx, const G& rx) const
      {
      return (tx == rx) ? 1 - Ps : Ps / G::elements();
      }
public:
   /*! \name Constructors / Destructors */
   qids(const bool varyPs = true, const bool varyPd = true, const bool varyPi =
         true);
   // @}

   /*! \name FBA decoder parameter computation */
   /*!
    * \copydoc qids::metric_computer::compute_I()
    *
    * \note Provided for use by clients; depends on object parameters
    */
   int compute_I(int tau) const
      {
      return metric_computer::compute_I(tau, Pi, Icap);
      }
   /*!
    * \copydoc qids::metric_computer::compute_xmax()
    *
    * \note Provided for use by clients; depends on object parameters
    */
   int compute_xmax(int tau) const
      {
      const int I = metric_computer::compute_I(tau, Pi, Icap);
      return metric_computer::compute_xmax(tau, Pi, Pd, I);
      }
   /*!
    * \copydoc qids::metric_computer::compute_xmax()
    *
    * \note Provided for use by clients; depends on object parameters
    */
   int compute_xmax(int tau, const libbase::vector<double>& sof_pdf,
         const int offset) const
      {
      const int I = metric_computer::compute_I(tau, Pi, Icap);
      return metric_computer::compute_xmax(tau, Pi, Pd, I, sof_pdf, offset);
      }
   // @}

   /*! \name Channel parameter handling */
   void set_parameter(const double p);
   double get_parameter() const;
   // @}

   /*! \name Channel parameter setters */
   //! Set the symbol-substitution probability
   void set_ps(const double Ps)
      {
      assert(Ps >= 0 && Ps <= 0.5);
      this->Ps = Ps;
      }
   //! Set the symbol-deletion probability
   void set_pd(const double Pd)
      {
      assert(Pd >= 0 && Pd <= 1);
      assert(Pi + Pd >= 0 && Pi + Pd <= 1);
      this->Pd = Pd;
      computer.precompute(Ps, Pd, Pi, Icap);
      }
   //! Set the symbol-insertion probability
   void set_pi(const double Pi)
      {
      assert(Pi >= 0 && Pi <= 1);
      assert(Pi + Pd >= 0 && Pi + Pd <= 1);
      this->Pi = Pi;
      computer.precompute(Ps, Pd, Pi, Icap);
      }
   //! Set the block size
   void set_blocksize(int N)
      {
      if (N != computer.N)
         {
         assert(N > 0);
         computer.N = N;
         computer.precompute(Ps, Pd, Pi, Icap);
         }
      }
   // @}

   /*! \name Channel parameter getters */
   //! Get the current symbol-substitution probability
   double get_ps() const
      {
      return Ps;
      }
   //! Get the current symbol-deletion probability
   double get_pd() const
      {
      return Pd;
      }
   //! Get the current symbol-insertion probability
   double get_pi() const
      {
      return Pi;
      }
   // @}

   /*! \name Stream-oriented channel characteristics */
   void get_drift_pdf(int tau, libbase::vector<double>& eof_pdf,
         libbase::size_type<libbase::vector>& offset) const;
   void get_drift_pdf(int tau, libbase::vector<double>& sof_pdf,
         libbase::vector<double>& eof_pdf,
         libbase::size_type<libbase::vector>& offset) const;
   // @}

   // Channel functions
   void transmit(const array1g_t& tx, array1g_t& rx);
   using Base::receive;
   void
   receive(const array1g_t& tx, const array1g_t& rx, array1vd_t& ptable) const;
   double receive(const array1g_t& tx, const array1g_t& rx) const
      {
#if DEBUG>=3
      libbase::trace << "DEBUG (qids): Computing RecvPr for" << std::endl;
      libbase::trace << "tx = " << tx;
      libbase::trace << "rx = " << rx;
#endif
      const real result = computer.receive(tx, rx);
#if DEBUG>=3
      libbase::trace << "RecvPr = " << result << std::endl;
#endif
      return result;
      }
   double receive(const G& tx, const array1g_t& rx) const
      {
      // Compute sizes
      const int mu = rx.size() - 1;
      // If this was not a deletion, return result from table
      if (mu >= 0)
         return computer.compute_Rtable_entry(tx != rx(mu), mu, Ps, Pd, Pi);
      // If this was a deletion, it's a fixed value
      return computer.Rval;
      }

   // Interface for CUDA
   const metric_computer& get_computer() const
      {
      return computer;
      }

   // Description
   std::string description() const;

   // Serialization Support
DECLARE_SERIALIZER(qids)
};

// Reset debug level, to avoid affecting other files
#ifndef NDEBUG
#  undef DEBUG
#  define DEBUG
#endif

} // end namespace

#endif

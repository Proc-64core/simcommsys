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
 * - $Id$
 */

#include "map_interleaved.h"
#include <cstdlib>
#include <sstream>

namespace libcomm {

// Interface with mapper

template <template <class > class C, class dbl>
void map_interleaved<C, dbl>::advance() const
   {
   lut.init(This::output_block_size(), r);
   }

template <template <class > class C, class dbl>
void map_interleaved<C, dbl>::dotransform(const C<int>& in, C<int>& out) const
   {
   // do the base (straight) mapping into a temporary space
   C<int> s;
   Base::dotransform(in, s);
   // final vector is the same size as straight-mapped one
   out.init(s.size());
   // shuffle the results
   assert(out.size() == lut.size());
   for (int i = 0; i < out.size(); i++)
      out(lut(i)) = s(i);
   }

template <template <class > class C, class dbl>
void map_interleaved<C, dbl>::doinverse(const C<array1d_t>& pin,
      C<array1d_t>& pout) const
   {
   assert(pin.size() == lut.size());
   // temporary matrix is the same size as input
   C<array1d_t> ptable;
   ptable.init(lut.size());
   // invert the shuffling
   for (int i = 0; i < lut.size(); i++)
      ptable(i) = pin(lut(i));
   // do the base (straight) mapping
   Base::doinverse(ptable, pout);
   }

// Description

template <template <class > class C, class dbl>
std::string map_interleaved<C, dbl>::description() const
   {
   std::ostringstream sout;
   sout << "Interleaved Mapper";
   return sout.str();
   }

// Serialization Support

template <template <class > class C, class dbl>
std::ostream& map_interleaved<C, dbl>::serialize(std::ostream& sout) const
   {
   Base::serialize(sout);
   return sout;
   }

template <template <class > class C, class dbl>
std::istream& map_interleaved<C, dbl>::serialize(std::istream& sin)
   {
   Base::serialize(sin);
   return sin;
   }

} // end namespace

#include "logrealfast.h"

namespace libcomm {

// Explicit Realizations
#include <boost/preprocessor/seq/for_each_product.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/stringize.hpp>

using libbase::serializer;
using libbase::logrealfast;
using libbase::matrix;
using libbase::vector;

#define CONTAINER_TYPE_SEQ \
   (vector)
#define REAL_TYPE_SEQ \
   (float)(double)(logrealfast)

/* Serialization string: map_interleaved<container,real>
 * where:
 *      container = vector
 *      real = float | double | logrealfast
 *              [real is the interface arithmetic type]
 */
#define INSTANTIATE(r, args) \
      template class map_interleaved<BOOST_PP_SEQ_ENUM(args)>; \
      template <> \
      const serializer map_interleaved<BOOST_PP_SEQ_ENUM(args)>::shelper( \
            "mapper", \
            "map_interleaved<" BOOST_PP_STRINGIZE(BOOST_PP_SEQ_ELEM(0,args)) "," \
            BOOST_PP_STRINGIZE(BOOST_PP_SEQ_ELEM(1,args)) ">", \
            map_interleaved<BOOST_PP_SEQ_ENUM(args)>::create);

BOOST_PP_SEQ_FOR_EACH_PRODUCT(INSTANTIATE, (CONTAINER_TYPE_SEQ)(REAL_TYPE_SEQ))

} // end namespace

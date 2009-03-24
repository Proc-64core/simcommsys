#include "serializer_libcomm.h"
#include "commsys.h"
#include "truerand.h"
#include "timer.h"

#include <boost/program_options.hpp>
#include <iostream>

/*!
   \brief Vector-type container for S-Random interleaver creation
   This vector container has the following addional abilities:
   * removing an element from the middle
   * initializing with numerical sequence
*/
template <class T>
class DerivedVector : public libbase::vector<T>
{
private:
   typedef libbase::vector<T> Base;
   using Base::m_xsize;
   using Base::m_data;
public:
   DerivedVector(const int x=0) : Base(x) {};
   void remove(const int x);
   void sequence();
};

template <class T>
void DerivedVector<T>::remove(const int x)
   {
   assert(x < m_xsize);
   for(int i=x; i<m_xsize-1; i++)
      m_data[i] = m_data[i+1];
   m_xsize--;
   }

template <class T>
void DerivedVector<T>::sequence()
   {
   for(int i=0; i<m_xsize; i++)
      m_data[i] = i;
   }

//! S-Random creation process

libbase::vector<int> create_srandom(const int tau, int& spread, libbase::int32u& seed, const int max_attempts)
   {
   // seed generator
   libbase::truerand trng;
   // intialize space for results
   libbase::vector<int> lut(tau);

   bool failed;
   int attempt = 0;
   // loop for a number of attempts at the given Spread, then
   // reduce and continue as necessary
   do {
      std::cerr << "Attempt " << attempt << " at spread " << spread << "\r";
      // re-seed random generator
      const libbase::int32u seed = trng.ival();
      libbase::randgen prng;
      prng.seed(seed);
      // set up working variables
      DerivedVector<int> unused(tau);
      unused.sequence();
      // loop to fill all entries in the interleaver - or until we fail
      failed = false;
      for(int i=0; i<tau && !failed; i++)
         {
         // set up for the current entry
         DerivedVector<int> untried = unused;
         DerivedVector<int> index(unused.size());
         index.sequence();
         int n, ndx;
         bool good;
         // loop for the current entry - until we manage to find a suitable value
         // or totally fail in trying
         do {
            // choose a random number from what's left to try
            ndx = prng.ival(untried.size());
            n = untried(ndx);
            // see if it's a suitable value (ie satisfies spread constraint)
            good = true;
            for(int j=std::max(0,i-spread); j<i; j++)
               if(abs(lut(j)-n) < spread)
                  {
                  good = false;
                  break;
                  }
            // if it's no good remove it from the list of options,
            // if it's good then insert it into the interleaver & mark that number as used
            if(!good)
               {
               untried.remove(ndx);
               index.remove(ndx);
               failed = (untried.size() == 0);
               }
            else
               {
               unused.remove(index(ndx));
               lut(i) = n;
               }
            } while(!good && !failed);
         }
      // if this failed, prepare for the next attempt
      if(failed)
         {
         attempt++;
         if(attempt >= max_attempts)
            {
            attempt = 0;
            spread--;
            }
         }
      } while(failed);

   return lut;
   }

/*!
   \brief   S-Random Interleaver Creator
   \author  Johann Briffa

   \section svn Version Control
   - $Revision$
   - $Date$
   - $Author$
*/

int main(int argc, char *argv[])
   {
   libbase::timer tmain("Main timer");

   // Set up user parameters
   namespace po = boost::program_options;
   po::options_description desc("Allowed options");
   desc.add_options()
      ("help", "print this help message")
      ("tau,t", po::value<int>(),
         "interleaver length")
      ("spread,s", po::value<int>(),
         "interleaver spread to start with")
      ("attempts,n", po::value<int>()->default_value(1000),
         "number of attempts before reducing spread")
      ;
   po::variables_map vm;
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);

   // Validate user parameters
   if(vm.count("help"))
      {
      std::cerr << desc << "\n";
      return 1;
      }

   // Interpret arguments
   const int tau = vm["tau"].as<int>();
   int spread = vm["spread"].as<int>();
   const int max_attempts = vm["attempts"].as<int>();
   // Main process
   libbase::int32u seed;
   libbase::vector<int> lut = create_srandom(tau, spread, seed, max_attempts);
   // Output
   std::cout << "#% Size: " << tau << "\n";
   std::cout << "#% Spread: " << spread << "\n";
   std::cout << "#% Seed: " << seed << "\n";
   std::cout << "# Date: " << libbase::timer::date() << "\n";
   std::cout << "# Time taken: " << libbase::timer::format(tmain.elapsed()) << "\n";
   lut.serialize(std::cout, '\n');

   return 0;
   }

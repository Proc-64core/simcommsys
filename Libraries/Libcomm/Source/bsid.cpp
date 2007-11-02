#include "bsid.h"
#include "fba.h"
#include "secant.h"

namespace libcomm {

const libbase::vcs bsid::version("Binary Substitution, Insertion, and Deletion Channel module (bsid)", 1.22);

const libbase::serializer bsid::shelper("channel", "bsid", bsid::create);


// internal functions

void bsid::init()
   {
   // channel parameters
   Ps = 0;
   Pd = 0;
   Pi = 0;
   }

// constructors / destructors

bsid::bsid(const int I, const int xmax, const bool varyPs, const bool varyPd, const bool varyPi)
   {
   // fba decoder parameters
   bsid::I = I;
   bsid::xmax = xmax;
   // channel update flags
   bsid::varyPs = varyPs;
   bsid::varyPd = varyPd;
   bsid::varyPi = varyPi;
   // other initialization
   init();
   }

// channel parameter updates
void bsid::set_ps(const double Ps)
   {
   assert(Ps >=0 && Ps <= 0.5);
   bsid::Ps = Ps;
   //libbase::secant Qinv(libbase::Q);
   //const double x = Qinv(Ps);
   //const double No = 1/(get_eb()*x*x);
   //set_no(No);
   }
   
void bsid::set_pd(const double Pd)
   {
   assert(Pd >=0 && Pd <= 1);
   assert(Pi+Pd >=0 && Pi+Pd <= 1);
   bsid::Pd = Pd;
   }
   
void bsid::set_pi(const double Pi)
   {
   assert(Pi >=0 && Pi <= 1);
   assert(Pi+Pd >=0 && Pi+Pd <= 1);
   bsid::Pi = Pi;
   }

// handle functions

void bsid::compute_parameters(const double Eb, const double No)
   {
   // computes substitution probability assuming Eb/No describes an AWGN channel with hard-decision demodulation
   const double p = libbase::Q(1/sqrt(Eb*No));
   if(varyPs)
      Ps = p;
   if(varyPd)
      Pd = p;
   if(varyPi)
      Pi = p;
   libbase::trace << "DEBUG (bsid): Ps = " << Ps << ", Pd = " << Pd << ", Pi = " << Pi << "\n";
   }
   
// channel handle functions

sigspace bsid::corrupt(const sigspace& s)
   {
   const double p = r.fval();
   //libbase::trace << "DEBUG (bsid): p(s) = " << p << "\n";
   if(p < Ps)
      return -s;
   return s;
   }

double bsid::pdf(const sigspace& tx, const sigspace& rx) const
   {      
   if(tx != rx)
      return Ps;
   return 1-Ps;
   }

// channel functions

void bsid::transmit(const libbase::vector<sigspace>& tx, libbase::vector<sigspace>& rx)
   {
   // We have initially no idea how long the received sequence will be, so we first determine the state sequence at every timestep
   // keeping track of (a) the number of insertions *before* given position, and (b) whether the given position is transmitted or deleted
   const int tau = tx.size();
   libbase::vector<int> insertions(tau);
   insertions = 0;
   libbase::vector<int> transmit(tau);
   transmit = 1;
   // determine state sequence
   for(int i=0; i<tau; i++)
      {
      double p;
      while((p = r.fval()) < Pi)
         insertions(i)++;
      if(p < (Pi+Pd))
         transmit(i) = 0;
      }
   // Initialize results vector
#ifndef NDEBUG
   if(tau < 10)
      {
      libbase::trace << "DEBUG (bsid): transmit = " << transmit << "\n";
      libbase::trace << "DEBUG (bsid): insertions = " << insertions << "\n";
      }
#endif
   rx.init(transmit.sum() + insertions.sum());
   // Corrupt the modulation symbols (simulate the channel)
   for(int i=0, j=0; i<tau; i++)
      {
      while(insertions(i)--)
         rx(j++) = (r.fval() < 0.5) ? sigspace(1,0) : sigspace(-1,0);
      if(transmit(i))
         rx(j++) = corrupt(tx(i));
      }
   }

/********************************* FBA sub-class object *********************************/

class myfba : public fba<double> {
   // user-defined parameters
   libbase::vector<sigspace>  tx;   // presumed transmitted sequence
   const bsid* channel;
   // implementations of channel-specific metrics for fba
   double P(const int a, const int b);
   double Q(const int a, const int b, const int i, const libbase::vector<sigspace>& s);
public:
   // constructor & destructor
   myfba() {};
   ~myfba() {};
   // set transmitted sequence
   void settx(const libbase::vector<sigspace>& tx) { myfba::tx.init(tx.size()+1); myfba::tx.copyfrom(tx); };
   // attach channel
   void attach(const bsid* channel) { myfba::channel = channel; };

};

// implementations of channel-specific metrics for fba

double myfba::P(const int a, const int b)
   {
   const double Pd = channel->get_pd();
   const double Pi = channel->get_pi();
   const int m = b-a;
   if(m == -1)
      return Pd;
   else if(m >= 0)
      return pow(Pi,m)*(1-Pi)*(1-Pd);
   return 0;
   }
   
double myfba::Q(const int a, const int b, const int i, const libbase::vector<sigspace>& s)
   {
   // 'a' and 'b' are redundant because 's' already contains the difference
   assert(s.size() == b-a+1);
   // 'tx' is a matrix of all possible transmitted symbols
   // we know exactly what was transmitted at this timestep
   libbase::matrix<sigspace> tx(1,1);
   tx(0,0) = myfba::tx(i);
   // compute the conditional probability
   libbase::matrix<double> ptable;
   channel->receive(tx, s, ptable);
   return ptable(0,0);
   }
   
/********************************* END FBA *********************************/

void bsid::receive(const libbase::matrix<sigspace>& tx, const libbase::vector<sigspace>& rx, libbase::matrix<double>& ptable) const
   {
   // Compute sizes
   const int tau = tx.xsize();
   const int M = tx.ysize();
   const int m = rx.size()-tau;
   // Initialize results vector
   ptable.init(1, M);
   if(tau == 1)   // Selection of possible transmitted symbols for one transmission step
      {
      // Work out the probabilities of each possible signal
      for(int x=0; x<M; x++)
         ptable(0,x) = pdf(tx(0,x), rx(m));
      ptable *= (1-Pi-Pd);
      ptable += 0.5*Pi*Pd;
      ptable /= (1<<m)*(1-Pi)*(1-Pd);
      }
   else if(M == 1)   // One possible sequence of transmitted symbols
      {
      myfba f;
      f.init(tau+1, I, xmax);
      f.attach(this);
      f.settx(tx);
      f.prepare(rx);
      ptable(1,1) = f.getF(tau+1,m);
      }
   else
      {
      std::cerr << "ERROR (bsid): cannot handle " << M << " possibilities over " << tau << " time steps.\n";
      exit(1);
      }
   }

// description output

std::string bsid::description() const
   {
   return "BSID channel";
   }

// object serialization - saving

std::ostream& bsid::serialize(std::ostream& sout) const
   {
   sout << I << "\n";
   sout << xmax << "\n";
   sout << varyPs << "\n";
   sout << varyPd << "\n";
   sout << varyPi << "\n";
   return sout;
   }

// object serialization - loading

std::istream& bsid::serialize(std::istream& sin)
   {
   sin >> I;
   sin >> xmax;
   sin >> varyPs;
   sin >> varyPd;
   sin >> varyPi;
   return sin;
   }

}; // end namespace

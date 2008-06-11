#ifndef __montecarlo_h
#define __montecarlo_h

#include "config.h"

#include "timer.h"
#include "sha.h"
#include "experiment.h"
#include "masterslave.h"
#include <iostream>

namespace libcomm {

/*!
   \brief   Monte Carlo Estimator.
   \author  Johann Briffa

   \par Version Control:
   - $Revision$
   - $Date$
   - $Author$
*/

class montecarlo : public libbase::masterslave {
   /*! \name Object-wide constants */
   static const libbase::int64u  min_samples;   //!< minimum number of samples
   // @}
   /*! \name Bound objects */
   /*! \note If 'init' is false, and 'system' is not NULL, then there is a dynamically allocated
             object at this address. This should be deleted when no longer necessary.
   */
   bool           init;          //!< Flag to indicate that a system has been bound (only in master)
   experiment     *system;       //!< System being sampled            
   // @}
   /*! \name Internal variables */
   double         cfactor;       //!< factor dependent on confidence level
   double         accuracy;      //!< accuracy level required
   libbase::timer t;             //!< timer to keep track of running estimate
   sha            sysdigest;     //!< digest of the currently-simulated system
   // @}
   /*! \name Slave process functions & their functors */
   void slave_getcode(void);
   void slave_getparameter(void);
   void slave_work(void);
   libbase::specificfunctor<montecarlo> *fgetcode;
   libbase::specificfunctor<montecarlo> *fgetparameter;
   libbase::specificfunctor<montecarlo> *fwork;
   // @}
private:
   /*! \name Helper functions */
   void seed_experiment();
   void createfunctors(void);
   void destroyfunctors(void);
   // @}
   /*! \name Main estimator helper functions */
   void sampleandaccumulate();
   void updateresults(libbase::vector<double>& result, libbase::vector<double>& tolerance) const;
   void initslave(slave *s, std::string systemstring);
   void initnewslaves(std::string systemstring);
   void workidleslaves(bool converged);
   bool readpendingslaves();
   // @}
protected:
   /*! \name Overrideable user-interface functions */
   virtual bool interrupt() { return false; };
   virtual void display(libbase::int64u pass, double cur_accuracy, double cur_mean);
   // @}
public:
   /*! \name Constructor/destructor */
   montecarlo();
   virtual ~montecarlo();
   // @}
   /*! \name Simulation initialization/finalization */
   void initialise(experiment *system);
   void reset();
   // @}
   /*! \name Simulation parameters */
   //! Set confidence limit, say, 0.95 => 95% probability
   void set_confidence(const double confidence);
   //! Set target accuracy, say, 0.10 => 10% of mean
   void set_accuracy(const double accuracy);
   // @}
   /*! \name Simulation results */
   //! Number of samples taken to produce the result
   libbase::int64u get_samplecount() const { return system->get_samplecount(); };
   //! Time taken to produce the result
   const libbase::timer& get_timer() const { return t; };
   // @}
   /*! \name Main process */
   void estimate(libbase::vector<double>& result, libbase::vector<double>& tolerance);
   // @}
};

}; // end namespace

#endif

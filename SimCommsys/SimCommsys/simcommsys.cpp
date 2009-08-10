#include "randgen.h"
#include "serializer_libcomm.h"
#include "experiment/binomial/commsys_simulator.h"
#include "montecarlo.h"
#include "masterslave.h"
#include "timer.h"

#include <boost/program_options.hpp>

#include <math.h>
#include <string.h>
#include <iostream>
#include <iomanip>

namespace simcommsys {

using std::cout;
using std::cerr;
using std::setprecision;
namespace po = boost::program_options;

class mymontecarlo : public libcomm::montecarlo {
public:
   // make interrupt function public to allow use by main program
   bool interrupt()
      {
      return libbase::keypressed() > 0 || libbase::interrupted();
      }
};

libcomm::experiment *createsystem(const std::string& fname)
   {
   const libcomm::serializer_libcomm my_serializer_libcomm;
   // load system from string representation
   libcomm::experiment *system;
   std::ifstream file(fname.c_str(), std::ios_base::in | std::ios_base::binary);
   file >> system;
   // check for errors in loading system
   libbase::verifycompleteload(file);
   return system;
   }

libbase::vector<double> getlinrange(double beg, double end, double step)
   {
   // validate range
   int steps = int(floor((end - beg) / step) + 1);
   assertalways(steps >= 1 && steps <= 65535);
   // create required range
   libbase::vector<double> pset(steps);
   pset(0) = beg;
   for (int i = 1; i < steps; i++)
      pset(i) = pset(i - 1) + step;
   return pset;
   }

libbase::vector<double> getlogrange(double beg, double end, double mul)
   {
   // validate range
   int steps = 0;
   if (end == 0 && beg == 0)
      steps = 1;
   else
      steps = int(floor((log(end) - log(beg)) / log(mul)) + 1);
   assertalways(steps >= 1 && steps <= 65535);
   // create required range
   libbase::vector<double> pset(steps);
   pset(0) = beg;
   for (int i = 1; i < steps; i++)
      pset(i) = pset(i - 1) * mul;
   return pset;
   }

/*!
 \brief   Simulation of Communication Systems
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
   po::options_description desc("Allowed options");
   desc.add_options()("help", "print this help message");
   desc.add_options()("quiet,q", po::bool_switch(),
         "suppress all output except benchmark");
   desc.add_options()("priority,p", po::value<int>()->default_value(10),
         "process priority");
   desc.add_options()("endpoint,e", po::value<std::string>()->default_value(
         "local"), "- 'local', for local-computation model\n"
      "- ':port', for server-mode, bound to given port\n"
      "- 'hostname:port', for client-mode connection");
   desc.add_options()("system-file,i", po::value<std::string>(),
         "input file containing system description");
   desc.add_options()("results-file,o", po::value<std::string>(),
         "output file to hold results");
   desc.add_options()("start", po::value<double>(), "first parameter value");
   desc.add_options()("stop", po::value<double>(), "last parameter value");
   desc.add_options()("step", po::value<double>(),
         "parameter increment (for a linear range)");
   desc.add_options()("mul", po::value<double>(),
         "parameter multiplier (for a logarithmic range)");
   desc.add_options()("min-error", po::value<double>()->default_value(1e-5),
         "stop simulation when result falls below this threshold");
   desc.add_options()("confidence", po::value<double>()->default_value(0.90),
         "confidence level (e.g. 0.90 for 90%)");
   desc.add_options()("tolerance", po::value<double>()->default_value(0.15),
         "confidence interval (e.g. 0.15 for +/- 15%)");
   po::variables_map vm;
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);

   // Validate user parameters
   if (vm.count("help"))
      {
      cout << desc << "\n";
      return 0;
      }

   // Create estimator object and initilize cluster
   mymontecarlo estimator;
   estimator.enable(vm["endpoint"].as<std::string> (), vm["quiet"].as<bool> (),
         vm["priority"].as<int> ());

   // If this is a server instance, check the remaining parameters
   if (vm.count("system-file") == 0 || vm.count("results-file") == 0
         || vm.count("start") == 0 || vm.count("stop") == 0
         || (vm.count("step") == 0 && vm.count("mul") == 0)
         || (vm.count("step") && vm.count("mul")))
      {
      cout << desc << "\n";
      return 0;
      }

   // Simulation system & parameters
   estimator.set_resultsfile(vm["results-file"].as<std::string> ());
   libcomm::experiment *system = createsystem(
         vm["system-file"].as<std::string> ());
   estimator.bind(system);
   const double min_error = vm["min-error"].as<double> ();
   libbase::vector<double> pset = vm.count("step") ? getlinrange(
         vm["start"].as<double> (), vm["stop"].as<double> (), vm["step"].as<
               double> ()) : getlogrange(vm["start"].as<double> (),
         vm["stop"].as<double> (), vm["mul"].as<double> ());
   estimator.set_confidence(vm["confidence"].as<double> ());
   estimator.set_accuracy(vm["tolerance"].as<double> ());

   // Work out the following for every SNR value required
   for (int i = 0; i < pset.size(); i++)
      {
      system->set_parameter(pset(i));

      cerr << "Simulating system at parameter = " << pset(i) << "\n";
      libbase::vector<double> result, tolerance;
      estimator.estimate(result, tolerance);

      cerr << "Statistics: " << setprecision(4) << estimator.get_samplecount()
            << " frames in " << estimator.get_timer() << " - "
            << estimator.get_samplecount() / estimator.get_timer().elapsed()
            << " frames/sec\n";

      // handle pre-mature breaks
      if (estimator.interrupt() || result.min() < min_error)
         break;
      }

   return 0;
   }

} // end namespace

int main(int argc, char *argv[])
   {
   return simcommsys::main(argc, argv);
   }
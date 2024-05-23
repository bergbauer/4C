/*----------------------------------------------------------------------*/
/*! \file

\brief Wrapper for the structural time integration which gives fine grained
       access in the time loop


\level 1

*/
/*----------------------------------------------------------------------*/

#include "4C_adapter_str_timeloop.hpp"

#include "4C_global_data.hpp"
#include "4C_inpar_structure.hpp"

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
int ADAPTER::StructureTimeLoop::Integrate()
{
  // error checking variables
  INPAR::STR::ConvergenceStatus convergencestatus = INPAR::STR::conv_success;

  // target time #timen_ and step #stepn_ already set
  // time loop
  while (NotFinished() and (convergencestatus == INPAR::STR::conv_success or
                               convergencestatus == INPAR::STR::conv_fail_repeat))
  {
    // call the predictor
    PrePredict();
    PrepareTimeStep();

    // integrate time step, i.e. do corrector steps
    // after this step we hold disn_, etc
    PreSolve();
    convergencestatus = Solve();

    // if everything is fine
    if (convergencestatus == INPAR::STR::conv_success)
    {
      // calculate stresses, strains and energies
      // note: this has to be done before the update since otherwise a potential
      // material history is overwritten
      constexpr bool force_prepare = false;
      PrepareOutput(force_prepare);

      // update displacements, velocities, accelerations
      // after this call we will have disn_==dis_, etc
      // update time and step
      // update everything on the element level
      PreUpdate();
      Update();
      PostUpdate();

      // write output
      Output();
      PostOutput();

      // print info about finished time step
      PrintStep();
    }
    // todo: remove this as soon as old structure time integration is gone
    else if (CORE::UTILS::IntegralValue<INPAR::STR::IntegrationStrategy>(
                 GLOBAL::Problem::Instance()->structural_dynamic_params(), "INT_STRATEGY") ==
             INPAR::STR::int_old)
    {
      convergencestatus =
          PerformErrorAction(convergencestatus);  // something went wrong update error code
                                                  // according to chosen divcont action
    }
  }

  PostTimeLoop();

  // that's it say what went wrong
  return convergencestatus;
}

FOUR_C_NAMESPACE_CLOSE

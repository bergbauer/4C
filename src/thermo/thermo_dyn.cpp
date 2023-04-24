/*----------------------------------------------------------------------*/
/*! \file
\brief entry point for (in)stationary heat conduction
\level 1
*/

/*----------------------------------------------------------------------*
 | definitions                                                gjb 01/08 |
 *----------------------------------------------------------------------*/

#include <Teuchos_TimeMonitor.hpp>

/*----------------------------------------------------------------------*
 | headers                                                   gjb 01/08 |
 *----------------------------------------------------------------------*/
#include "thermo_dyn.H"
#include "inpar_validparameters.H"
#include "lib_globalproblem.H"
#include "adapter_thermo.H"
#include "thermo_resulttest.H"

/*----------------------------------------------------------------------*
 | main control routine for (in)stationary heat conduction              |
 *----------------------------------------------------------------------*/
void thr_dyn_drt()
{
  // access the discretization
  Teuchos::RCP<DRT::Discretization> thermodis = Teuchos::null;
  thermodis = DRT::Problem::Instance()->GetDis("thermo");

  // set degrees of freedom in the discretization
  if (not thermodis->Filled()) thermodis->FillComplete();

  const Teuchos::ParameterList& tdyn = DRT::Problem::Instance()->ThermalDynamicParams();

  // create instance of thermo basis algorithm (no structure discretization)
  Teuchos::RCP<ADAPTER::ThermoBaseAlgorithm> thermoonly =
      Teuchos::rcp(new ADAPTER::ThermoBaseAlgorithm(tdyn, thermodis));

  // do restart if demanded from input file
  const int restart = DRT::Problem::Instance()->Restart();
  if (restart)
  {
    thermoonly->ThermoField().ReadRestart(restart);
  }

  // enter time loop to solve problem
  (thermoonly->ThermoField()).Integrate();

  // perform the result test if required
  DRT::Problem::Instance()->AddFieldTest(thermoonly->ThermoField().CreateFieldTest());
  DRT::Problem::Instance()->TestAll(thermodis->Comm());

  // done
  return;

}  // thr_dyn_drt()


/*----------------------------------------------------------------------*/

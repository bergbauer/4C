/*!------------------------------------------------------------------------------------------------*
 \file poroelast.cpp

 \brief control routine of poroelasticity problems

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15264
 </pre>
 *------------------------------------------------------------------------------------------------*/


/*----------------------------------------------------------------------*
 |  headers                                                             |
 *----------------------------------------------------------------------*/
#include "poro_dyn.H"
#include "poro_base.H"
#include "poro_scatra.H"
#include "poroelast_utils.H"
#include "../drt_inpar/inpar_poroelast.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_discret.H"

#include <Teuchos_TimeMonitor.hpp>

/*------------------------------------------------------------------------------------------------*
 | main control routine for poroelasticity problems                                   vuong 01/12 |
 *------------------------------------------------------------------------------------------------*/
void poroelast_drt()
{
  DRT::Problem* problem = DRT::Problem::Instance();

  // create a communicator
  const Epetra_Comm& comm = problem->GetDis("structure")->Comm();

  // setup of the discretizations, including clone strategy
  POROELAST::UTILS::SetupPoro();

  // access the problem-specific parameter list
  const Teuchos::ParameterList& poroelastdyn =
      problem->PoroelastDynamicParams();

  // choose algorithm depending on solution type
  Teuchos::RCP<POROELAST::PoroBase> poroalgo = POROELAST::UTILS::CreatePoroAlgorithm(poroelastdyn, comm);

  // read the restart information, set vectors and variables
  const int restart = problem->Restart();
  poroalgo->ReadRestart(restart);

  // now do the coupling setup and create the combined dofmap
  poroalgo->SetupSystem();

  // solve the whole problem
  poroalgo->TimeLoop();

  // summarize the performance measurements
  Teuchos::TimeMonitor::summarize();

  // perform the result test
  poroalgo->TestResults(comm);

  return;
}//poroelast_drt()

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void poro_scatra_drt()
{
  DRT::Problem* problem = DRT::Problem::Instance();

  //1.- Initialization
  const Epetra_Comm& comm = problem->GetDis("structure")->Comm();

  //2.- Parameter reading
  const Teuchos::ParameterList& poroscatradynparams = problem->PoroScatraControlParams();

  //3.- Creation of Poroelastic + Scalar_Transport problem. (Discretization called inside)
  Teuchos::RCP<PORO_SCATRA::PartPORO_SCATRA> poro_scatra = Teuchos::rcp(
      new PORO_SCATRA::PartPORO_SCATRA(comm, poroscatradynparams));

  //3.1- Read restart if needed. (Discretization called inside)
  poro_scatra->ReadRestart();

  //4.- Run of the actual problem.

  // 4.1.- Some setup needed for the poroelastic subproblem.
  poro_scatra->SetupSystem();

  // 4.2.- Solve the whole problem
  poro_scatra->Timeloop();

  // 4.3.- Summarize the performance measurements
  Teuchos::TimeMonitor::summarize();

  // 5. - perform the result test
  poro_scatra->TestResults(comm);

}


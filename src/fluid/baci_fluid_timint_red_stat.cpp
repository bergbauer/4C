/*-----------------------------------------------------------*/
/*! \file

\brief Stationary driver for reduced models


\level 2

*/
/*-----------------------------------------------------------*/

#include "baci_fluid_timint_red_stat.H"

#include "baci_io.H"


/*----------------------------------------------------------------------*
 |  Constructor (public)                                       bk 11/13 |
 *----------------------------------------------------------------------*/
FLD::TimIntRedModelsStat::TimIntRedModelsStat(const Teuchos::RCP<DRT::Discretization>& actdis,
    const Teuchos::RCP<CORE::LINALG::Solver>& solver,
    const Teuchos::RCP<Teuchos::ParameterList>& params,
    const Teuchos::RCP<IO::DiscretizationWriter>& output, bool alefluid /*= false*/)
    : FluidImplicitTimeInt(actdis, solver, params, output, alefluid),
      TimIntStationary(actdis, solver, params, output, alefluid),
      TimIntRedModels(actdis, solver, params, output, alefluid)
{
  return;
}


/*----------------------------------------------------------------------*
 |  initialize algorithm                                rasthofer 04/14 |
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModelsStat::Init()
{
  // call Init()-functions of base classes
  // note: this order is important
  TimIntStationary::Init();
  TimIntRedModels::Init();

  return;
}


/*----------------------------------------------------------------------*
 |  read restart data                                   rasthofer 12/14 |
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModelsStat::ReadRestart(int step)
{
  // call of base classes
  TimIntStationary::ReadRestart(step);
  TimIntRedModels::ReadRestart(step);

  return;
}
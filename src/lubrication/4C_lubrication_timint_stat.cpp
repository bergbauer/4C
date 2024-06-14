/*--------------------------------------------------------------------------*/
/*! \file

\brief solution algorithm for stationary problems

\level 3


*/
/*--------------------------------------------------------------------------*/

#include "4C_lubrication_timint_stat.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_io.hpp"
#include "4C_lubrication_ele_action.hpp"

#include <Teuchos_TimeMonitor.hpp>

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 |  Constructor (public)                                    wirtz 11/15 |
 *----------------------------------------------------------------------*/
LUBRICATION::TimIntStationary::TimIntStationary(Teuchos::RCP<Core::FE::Discretization> actdis,
    Teuchos::RCP<Core::LinAlg::Solver> solver, Teuchos::RCP<Teuchos::ParameterList> params,
    Teuchos::RCP<Teuchos::ParameterList> extraparams,
    Teuchos::RCP<Core::IO::DiscretizationWriter> output)
    : TimIntImpl(actdis, solver, params, extraparams, output)
{
  // DO NOT DEFINE ANY STATE VECTORS HERE (i.e., vectors based on row or column maps)
  // this is important since we have problems which require an extended ghosting
  // this has to be done before all state vectors are initialized
}



/*----------------------------------------------------------------------*
 |  initialize time integration                             wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntStationary::Init()
{
  // initialize base class
  TimIntImpl::Init();

  // -------------------------------------------------------------------
  // set element parameters
  // -------------------------------------------------------------------
  // note: - this has to be done before element routines are called
  //       - order is important here: for safety checks in set_element_general_parameters(),
  //         we have to know the time-integration parameters
  set_element_time_parameter();
  set_element_general_parameters();
}


/*----------------------------------------------------------------------*
 | set time parameter for element evaluation (usual call)   wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntStationary::set_element_time_parameter() const
{
  Teuchos::ParameterList eleparams;

  eleparams.set<int>("action", LUBRICATION::set_time_parameter);
  eleparams.set<bool>("using generalized-alpha time integration", false);
  eleparams.set<bool>("using stationary formulation", true);
  eleparams.set<double>("time-step length", dta_);
  eleparams.set<double>("total time", time_);
  eleparams.set<double>("time factor", 1.0);
  eleparams.set<double>("alpha_F", 1.0);

  // call standard loop over elements
  discret_->Evaluate(
      eleparams, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);
}


/*----------------------------------------------------------------------*
 | set time for evaluation of Neumann boundary conditions   wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntStationary::set_time_for_neumann_evaluation(Teuchos::ParameterList& params)
{
  params.set("total time", time_);
}


/*----------------------------------------------------------------------*
 | add actual Neumann loads                                 wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntStationary::add_neumann_to_residual()
{
  residual_->Update(1.0, *neumann_loads_, 1.0);
}


/*--------------------------------------------------------------------------*
 | add global state vectors specific for time-integration scheme            |
 |                                                              wirtz 11/15 |
 *--------------------------------------------------------------------------*/
void LUBRICATION::TimIntStationary::add_time_integration_specific_vectors(
    bool forcedincrementalsolver)
{
  discret_->set_state("prenp", prenp_);
}


/*----------------------------------------------------------------------*
 | update of solution at end of time step                   wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntStationary::Update(const int num)
{
  // for the stationary scheme there is nothing to do
}


/*----------------------------------------------------------------------*
 |                                                          wirtz 11/15 |
 -----------------------------------------------------------------------*/
void LUBRICATION::TimIntStationary::read_restart(int step)
{
  Core::IO::DiscretizationReader reader(
      discret_, Global::Problem::Instance()->InputControlFile(), step);
  time_ = reader.read_double("time");
  step_ = reader.read_int("step");

  if (myrank_ == 0)
    std::cout << "Reading Lubrication restart data (time=" << time_ << " ; step=" << step_ << ")"
              << '\n';

  // read state vectors that are needed for restart
  reader.read_vector(prenp_, "prenp");
}


/*----------------------------------------------------------------------*
 | incremental iteration update of state                    wirtz 01/16 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntStationary::update_iter_incrementally()
{
  //! new end-point temperatures
  //! T_{n+1}^{<k+1>} := T_{n+1}^{<k>} + IncT_{n+1}^{<k>}
  prenp_->Update(1.0, *prei_, 1.0);
}  // update_iter_incrementally()

FOUR_C_NAMESPACE_CLOSE
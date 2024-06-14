/*--------------------------------------------------------------------------*/
/*! \file

\brief Associated with control routine for Lubrication solvers,

     including stationary solver.


\level 3

*/
/*--------------------------------------------------------------------------*/

#include "4C_lubrication_timint_implicit.hpp"

#include "4C_global_data.hpp"
#include "4C_inpar_lubrication.hpp"
#include "4C_inpar_validparameters.hpp"
#include "4C_io.hpp"
#include "4C_io_control.hpp"
#include "4C_io_gmsh.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linalg_utils_sparse_algebra_print.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_lubrication_ele_action.hpp"
#include "4C_utils_function.hpp"

#include <Teuchos_TimeMonitor.hpp>

FOUR_C_NAMESPACE_OPEN

/*==========================================================================*/
// Constructors and destructors and related methods
/*==========================================================================*/

/*----------------------------------------------------------------------*
 | constructor                                     (public) wirtz 11/15 |
 *----------------------------------------------------------------------*/
LUBRICATION::TimIntImpl::TimIntImpl(Teuchos::RCP<Core::FE::Discretization> actdis,
    Teuchos::RCP<Core::LinAlg::Solver> solver, Teuchos::RCP<Teuchos::ParameterList> params,
    Teuchos::RCP<Teuchos::ParameterList> extraparams,
    Teuchos::RCP<Core::IO::DiscretizationWriter> output)
    :  // call constructor for "nontrivial" objects
      solver_(solver),
      params_(params),
      myrank_(actdis->Comm().MyPID()),
      isale_(extraparams->get<bool>("isale")),
      incremental_(true),
      modified_reynolds_(Core::UTILS::IntegralValue<int>(*params, "MODIFIED_REYNOLDS_EQU")),
      addsqz_(Core::UTILS::IntegralValue<int>(*params, "ADD_SQUEEZE_TERM")),
      purelub_(Core::UTILS::IntegralValue<int>(*params, "PURE_LUB")),
      outmean_(Core::UTILS::IntegralValue<int>(*params, "OUTMEAN")),
      outputgmsh_(Core::UTILS::IntegralValue<int>(*params, "OUTPUT_GMSH")),
      output_state_matlab_(Core::UTILS::IntegralValue<int>(*params, "MATLAB_STATE_OUTPUT")),
      time_(0.0),
      maxtime_(params->get<double>("MAXTIME")),
      step_(0),
      stepmax_(params->get<int>("NUMSTEP")),
      dta_(params->get<double>("TIMESTEP")),
      dtele_(0.0),
      dtsolve_(0.0),
      iternum_(0),
      nsd_(Global::Problem::Instance()->NDim()),
      // Initialization of degrees of freedom variables
      prenp_(Teuchos::null),
      nds_disp_(-1),
      discret_(actdis),
      output_(output),
      sysmat_(Teuchos::null),
      zeros_(Teuchos::null),
      dbcmaps_(Teuchos::null),
      neumann_loads_(Teuchos::null),
      residual_(Teuchos::null),
      trueresidual_(Teuchos::null),
      increment_(Teuchos::null),
      prei_(Teuchos::null),
      // Initialization of
      upres_(params->get<int>("RESULTSEVRY")),
      uprestart_(params->get<int>("RESTARTEVRY")),

      roughness_deviation_(params->get<double>("ROUGHNESS_STD_DEVIATION"))
{
  // DO NOT DEFINE ANY STATE VECTORS HERE (i.e., vectors based on row or column maps)
  // this is important since we have problems which require an extended ghosting
  // this has to be done before all state vectors are initialized
  return;
}


/*------------------------------------------------------------------------*
 | initialize time integration                                wirtz 11/15 |
 *------------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::Init()
{
  // -------------------------------------------------------------------
  // always nonlinear solver
  // -------------------------------------------------------------------
  incremental_ = true;

  discret_->compute_null_space_if_necessary(solver_->Params(), true);

  // ensure that degrees of freedom in the discretization have been set
  if ((not discret_->Filled()) or (not discret_->HaveDofs()))
    FOUR_C_THROW("discretization not completed");

  // -------------------------------------------------------------------
  // get a vector layout from the discretization to construct matching
  // vectors and matrices: local <-> global dof numbering
  // -------------------------------------------------------------------
  const Epetra_Map* dofrowmap = discret_->dof_row_map();

  // -------------------------------------------------------------------
  // create empty system matrix (27 adjacent nodes as 'good' guess)
  // -------------------------------------------------------------------
  sysmat_ =
      Teuchos::rcp(new Core::LinAlg::SparseMatrix(*(discret_->dof_row_map()), 27, false, true));

  // -------------------------------------------------------------------
  // create vectors containing problem variables
  // -------------------------------------------------------------------
  // solutions at time n+1 and n
  prenp_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  // -------------------------------------------------------------------
  // create vectors associated to boundary conditions
  // -------------------------------------------------------------------
  // a vector of zeros to be used to enforce zero dirichlet boundary conditions
  zeros_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  // object holds maps/subsets for DOFs subjected to Dirichlet BCs and otherwise
  dbcmaps_ = Teuchos::rcp(new Core::LinAlg::MapExtractor());
  {
    Teuchos::ParameterList eleparams;
    // other parameters needed by the elements
    eleparams.set("total time", time_);
    eleparams.set<const Core::UTILS::FunctionManager*>(
        "function_manager", &Global::Problem::Instance()->FunctionManager());
    discret_->evaluate_dirichlet(
        eleparams, zeros_, Teuchos::null, Teuchos::null, Teuchos::null, dbcmaps_);
    zeros_->PutScalar(0.0);  // just in case of change
  }

  // -------------------------------------------------------------------
  // create vectors associated to solution process
  // -------------------------------------------------------------------
  // the vector containing body and surface forces
  neumann_loads_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  // the residual vector --- more or less the rhs
  residual_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  // residual vector containing the normal boundary fluxes
  trueresidual_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  // incremental solution vector
  increment_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  // iterative pressure increments Incp_{n+1}
  // also known as residual pressures
  prei_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  return;
}  // TimIntImpl::Init()



/*========================================================================*/
//! set element parameters
/*========================================================================*/

/*----------------------------------------------------------------------*
 | set all general parameters for element                   wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::set_element_general_parameters() const
{
  Teuchos::ParameterList eleparams;

  eleparams.set<int>("action", LUBRICATION::set_general_lubrication_parameter);

  eleparams.set<bool>("isale", isale_);

  eleparams.set<bool>("ismodifiedrey", modified_reynolds_);

  eleparams.set<bool>("addsqz", addsqz_);

  eleparams.set<bool>("purelub", purelub_);

  eleparams.set("roughnessdeviation", roughness_deviation_);

  // call standard loop over elements
  discret_->Evaluate(
      eleparams, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);

  return;
}


/*==========================================================================*/
// general framework
/*==========================================================================*/

/*--- set, prepare, and predict --------------------------------------------*/

/*----------------------------------------------------------------------*
 | prepare time loop                                        wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::prepare_time_loop()
{
  // provide information about initial field (do not do for restarts!)
  if (step_ == 0)
  {
    // write out initial state
    Output();

    // compute error for problems with analytical solution (initial field!)
    evaluate_error_compared_to_analytical_sol();
  }

  return;
}  // LUBRICATION::TimIntImpl::prepare_time_loop


/*----------------------------------------------------------------------*
 | setup the variables to do a new time step(predictor)  (public) wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::prepare_time_step()
{
  // time measurement: prepare time step
  TEUCHOS_FUNC_TIME_MONITOR("LUBRICATION:    + prepare time step");

  // -------------------------------------------------------------------
  //                       initialization
  // -------------------------------------------------------------------
  if (step_ == 0) prepare_first_time_step();

  // -------------------------------------------------------------------
  //              set time dependent parameters
  // -------------------------------------------------------------------
  // note the order of the following three functions is important
  increment_time_and_step();

  // -------------------------------------------------------------------
  // set part of the rhs vector belonging to the old timestep
  // -------------------------------------------------------------------
  //  set_old_part_of_righthandside();
  // TODO (Thon): We do not really want to call set_element_time_parameter() every time step.
  // But for now we just do it since "total time" has to be changed in the parameter class..
  set_element_time_parameter();

  // -------------------------------------------------------------------
  //         evaluate Dirichlet and Neumann boundary conditions
  // -------------------------------------------------------------------
  // TODO: Dirichlet auch im Fall von genalpha prenp
  // Neumann(n + alpha_f)
  apply_dirichlet_bc(time_, prenp_, Teuchos::null);
  apply_neumann_bc(neumann_loads_);

  return;
}  // TimIntImpl::prepare_time_step


/*------------------------------------------------------------------------------*
 | initialization procedure prior to evaluation of first time step  wirtz 11/15 |
 *------------------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::prepare_first_time_step()
{
  return;
}  // LUBRICATION::TimIntImpl::prepare_first_time_step


/*----------------------------------------------------------------------*
 | update the height field by function                    faraji        |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::set_height_field_pure_lub(const int nds)
{
  // create the parameters for the time
  Teuchos::ParameterList eleparams;
  eleparams.set("total time", time_);

  // initialize height vectors
  Teuchos::RCP<Epetra_Vector> height =
      Core::LinAlg::CreateVector(*discret_->dof_row_map(nds), true);

  int err(0);
  const int heightfuncno = params_->get<int>("HFUNCNO");
  // loop all nodes on the processor
  for (int lnodeid = 0; lnodeid < discret_->NumMyRowNodes(); lnodeid++)
  {
    // get the processor local node
    Core::Nodes::Node* lnode = discret_->lRowNode(lnodeid);

    // get dofs associated with current node
    std::vector<int> nodedofs = discret_->Dof(nds, lnode);

    for (int index = 0; index < nsd_; ++index)
    {
      double heightfuncvalue =
          Global::Problem::Instance()
              ->FunctionById<Core::UTILS::FunctionOfSpaceTime>(heightfuncno - 1)
              .Evaluate(lnode->X().data(), time_, index);

      // get global and local dof IDs
      const int gid = nodedofs[index];
      const int lid = height->Map().LID(gid);

      if (lid < 0) FOUR_C_THROW("Local ID not found in map for given global ID!");
      err = height->ReplaceMyValue(lid, 0, heightfuncvalue);
      if (err != 0) FOUR_C_THROW("error while inserting a value into height");
    }
  }
  // provide lubrication discretization with height
  discret_->set_state(nds, "height", height);

}  // LUBRICATION::TimIntImpl::set_height_field_pure_lub

/*----------------------------------------------------------------------*
 | update the velocity field by function                  faraji       |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::set_average_velocity_field_pure_lub(const int nds)
{
  // create the parameters for the time
  Teuchos::ParameterList eleparams;
  eleparams.set("total time", time_);

  // initialize velocity vectors
  Teuchos::RCP<Epetra_Vector> vel = Core::LinAlg::CreateVector(*discret_->dof_row_map(nds), true);

  int err(0);
  const int velfuncno = params_->get<int>("VELFUNCNO");
  // loop all nodes on the processor
  for (int lnodeid = 0; lnodeid < discret_->NumMyRowNodes(); lnodeid++)
  {
    // get the processor local node
    Core::Nodes::Node* lnode = discret_->lRowNode(lnodeid);

    // get dofs associated with current node
    std::vector<int> nodedofs = discret_->Dof(nds, lnode);

    for (int index = 0; index < nsd_; ++index)
    {
      double velfuncvalue = Global::Problem::Instance()
                                ->FunctionById<Core::UTILS::FunctionOfSpaceTime>(velfuncno - 1)
                                .Evaluate(lnode->X().data(), time_, index);

      // get global and local dof IDs
      const int gid = nodedofs[index];
      const int lid = vel->Map().LID(gid);

      if (lid < 0) FOUR_C_THROW("Local ID not found in map for given global ID!");
      err = vel->ReplaceMyValue(lid, 0, velfuncvalue);
      if (err != 0) FOUR_C_THROW("error while inserting a value into vel");
    }
  }
  // provide lubrication discretization with velocity
  discret_->set_state(nds, "av_tang_vel", vel);

}  // LUBRICATION::TimIntImpl::set_average_velocity_field_pure_lub
/*----------------------------------------------------------------------*
 | contains the time loop                                   wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::TimeLoop()
{
  // time measurement: time loop
  TEUCHOS_FUNC_TIME_MONITOR("LUBRICATION:  + time loop");

  // prepare time loop
  prepare_time_loop();

  while ((step_ < stepmax_) and ((time_ + 1e-12) < maxtime_))
  {
    // -------------------------------------------------------------------
    // prepare time step
    // -------------------------------------------------------------------
    prepare_time_step();

    // -------------------------------------------------------------------
    //                  set the auxiliary dofs
    // -------------------------------------------------------------------
    if (purelub_)
    {
      set_height_field_pure_lub(1);
      set_average_velocity_field_pure_lub(1);
    }
    else
    {
      set_height_field(1, Teuchos::null);
      SetHeightDotField(1, Teuchos::null);
      set_relative_velocity_field(1, Teuchos::null);
      set_average_velocity_field(1, Teuchos::null);
    }
    // -------------------------------------------------------------------
    //                  solve nonlinear / linear equation
    // -------------------------------------------------------------------
    Solve();

    // -------------------------------------------------------------------
    // evaluate error for problems with analytical solution
    // -------------------------------------------------------------------
    evaluate_error_compared_to_analytical_sol();

    // -------------------------------------------------------------------
    //                         output of solution
    // -------------------------------------------------------------------
    Output();

  }  // while

  // print the results of time measurements
  Teuchos::TimeMonitor::summarize();

  return;
}  // TimIntImpl::TimeLoop


/*----------------------------------------------------------------------*
 | contains the call of linear/nonlinear solver             wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::Solve()
{
  // -----------------------------------------------------------------
  //                    always solve nonlinear equation
  // -----------------------------------------------------------------
  nonlinear_solve();
  // that's all

  return;
}


/*----------------------------------------------------------------------*
 | apply moving mesh data                                     gjb 05/09 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::ApplyMeshMovement(Teuchos::RCP<const Epetra_Vector> dispnp, int nds)
{
  //---------------------------------------------------------------------------
  // only required in ALE case
  //---------------------------------------------------------------------------
  if (isale_)
  {
    TEUCHOS_FUNC_TIME_MONITOR("LUBRICATION: apply mesh movement");

    // check existence of displacement vector
    if (dispnp == Teuchos::null) FOUR_C_THROW("Got null pointer for displacements!");

    // store number of dofset associated with displacement related dofs
    nds_disp_ = nds;

    // provide lubrication discretization with displacement field
    discret_->set_state(nds_disp_, "dispnp", dispnp);
  }  // if (isale_)

  return;
}  // TimIntImpl::ApplyMeshMovement


/*----------------------------------------------------------------------*
 |  print information about current time step to screen     wirtz 11/15 |
 *----------------------------------------------------------------------*/
inline void LUBRICATION::TimIntImpl::print_time_step_info()
{
  if (myrank_ == 0)
    printf("TIME: %11.4E/%11.4E  DT = %11.4E  Stationary  STEP = %4d/%4d \n", time_, maxtime_, dta_,
        step_, stepmax_);
}  // LUBRICATION::TimIntImpl::print_time_step_info


/*----------------------------------------------------------------------*
 | output of solution vector to BINIO                       wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::Output(const int num)
{
  // time measurement: output of solution
  TEUCHOS_FUNC_TIME_MONITOR("LUBRICATION:    + output of solution");

  // solution output and potentially restart data and/or flux data
  if (do_output())
  {
    // step number and time (only after that data output is possible)
    output_->new_step(step_, time_);

    // write domain decomposition for visualization (only once at step "upres"!)
    if (step_ == upres_) output_->write_element_data(true);

    // write state vectors
    output_state();

    // write output to Gmsh postprocessing files
    if (outputgmsh_) output_to_gmsh(step_, time_);

    // write mean values of pressure(s)
    OutputMeanPressures(num);
  }

  if ((step_ != 0) and (output_state_matlab_))
  {
    std::ostringstream filename;
    filename << "Result_Step" << step_ << ".m";
    Core::LinAlg::PrintVectorInMatlabFormat(filename.str(), *prenp_);
  }
  // NOTE:
  // statistics output for normal fluxes at boundaries was already done during Update()

  return;
}  // TimIntImpl::Output


/*==========================================================================*
 |                                                                          |
 | protected:                                                               |
 |                                                                          |
 *==========================================================================*/

/*==========================================================================*/
// general framework
/*==========================================================================*/


/*----------------------------------------------------------------------*
 | evaluate Dirichlet boundary conditions at t_{n+1}        wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::apply_dirichlet_bc(
    const double time, Teuchos::RCP<Epetra_Vector> prenp, Teuchos::RCP<Epetra_Vector> predt)
{
  // time measurement: apply Dirichlet conditions
  TEUCHOS_FUNC_TIME_MONITOR("LUBRICATION:      + apply dirich cond.");

  // Todo: what happens in the case of generalized alpha
  // needed parameters
  Teuchos::ParameterList p;
  p.set("total time", time);  // actual time t_{n+1}
  p.set<const Core::UTILS::FunctionManager*>(
      "function_manager", &Global::Problem::Instance()->FunctionManager());

  // predicted Dirichlet values
  // \c  prenp then also holds prescribed new Dirichlet values
  discret_->ClearState();
  discret_->evaluate_dirichlet(p, prenp, predt, Teuchos::null, Teuchos::null, dbcmaps_);
  discret_->ClearState();

  return;
}  // LUBRICATION::TimIntImpl::apply_dirichlet_bc


/*----------------------------------------------------------------------*
 | contains the residual scaling and addition of Neumann terms          |
 |                                                          wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::scaling_and_neumann()
{
  // scaling to get true residual vector for all time integration schemes
  // in incremental case: boundary flux values can be computed from trueresidual
  if (incremental_) trueresidual_->Update(residual_scaling(), *residual_, 0.0);

  // add Neumann b.c. scaled with a factor due to time discretization
  add_neumann_to_residual();

  return;
}  // TimIntImpl::scaling_and_neumann


/*----------------------------------------------------------------------*
 | evaluate Neumann boundary conditions                     wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::apply_neumann_bc(
    const Teuchos::RCP<Epetra_Vector>& neumann_loads  //!< Neumann loads
)
{
  // prepare load vector
  neumann_loads->PutScalar(0.0);

  // create parameter list
  Teuchos::ParameterList condparams;

  // action for elements
  condparams.set<int>("action", LUBRICATION::bd_calc_Neumann);

  // set time for evaluation of point Neumann conditions as parameter depending on time integration
  // scheme line/surface/volume Neumann conditions use the time stored in the time parameter class
  set_time_for_neumann_evaluation(condparams);

  // provide displacement field in case of ALE
  if (isale_) condparams.set<int>("ndsdisp", nds_disp_);

  // evaluate Neumann boundary conditions at time t_{n+alpha_F} (generalized alpha) or time t_{n+1}
  // (otherwise)
  discret_->evaluate_neumann(condparams, *neumann_loads);
  discret_->ClearState();

  return;
}  // LUBRICATION::TimIntImpl::apply_neumann_bc

/*----------------------------------------------------------------------*
 | add cavitation penalty contribution to matrix and rhs    seitz 12/17 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::add_cavitation_penalty()
{
  const double penalty_param = params_->get<double>("PENALTY_CAVITATION");
  for (int i = 0; i < dof_row_map()->NumMyElements(); ++i)
  {
    const double pressure = Prenp()->operator[](i);
    if (pressure >= 0.) continue;

    const int gid = dof_row_map()->GID(i);
    sysmat_->Assemble(-penalty_param, gid, gid);
    residual_->operator[](i) += penalty_param * pressure;
  }
}


/*----------------------------------------------------------------------*
 | contains the assembly process for matrix and rhs
  for elements                                              wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::assemble_mat_and_rhs()
{
  // time measurement: element calls
  TEUCHOS_FUNC_TIME_MONITOR("LUBRICATION:       + element calls");

  // get cpu time
  const double tcpuele = Teuchos::Time::wallTime();

  // zero out matrix entries
  sysmat_->Zero();

  // reset the residual vector
  residual_->PutScalar(0.0);

  // create parameter list for elements
  Teuchos::ParameterList eleparams;

  // action for elements
  eleparams.set<int>("action", LUBRICATION::calc_mat_and_rhs);

  // time step set up
  eleparams.set<double>("delta time", dta_);

  // provide bool whether ale or not, i.e. if the mesh is displaced
  eleparams.set<bool>("isale", isale_);

  // provide displacement field in case of ALE
  if (isale_) eleparams.set<int>("ndsdisp", nds_disp_);

  // set vector values needed by elements
  discret_->ClearState();

  // add state vectors according to time-integration scheme
  add_time_integration_specific_vectors();

  // call loop over elements
  discret_->Evaluate(eleparams, sysmat_, residual_);
  discret_->ClearState();

  // add cavitation penalty
  add_cavitation_penalty();

  // potential residual scaling and potential addition of Neumann terms
  scaling_and_neumann();  // TODO: do we have to call this function twice??

  // finalize assembly of system matrix
  sysmat_->Complete();

  // end time measurement for element
  dtele_ = Teuchos::Time::wallTime() - tcpuele;

  return;
}  // TimIntImpl::assemble_mat_and_rhs


/*----------------------------------------------------------------------*
 | contains the nonlinear iteration loop                    wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::nonlinear_solve()
{
  // time measurement: nonlinear iteration
  TEUCHOS_FUNC_TIME_MONITOR("LUBRICATION:   + nonlin. iteration/lin. solve");

  // out to screen
  print_time_step_info();

  // print header of convergence table to screen
  print_convergence_header();

  // ---------------------------------------------- nonlinear iteration
  // stop nonlinear iteration when increment-norm is below this bound
  const double ittol = params_->get<double>("CONVTOL");

  //------------------------------ turn adaptive solver tolerance on/off
  const bool isadapttol = (Core::UTILS::IntegralValue<int>(*params_, "ADAPTCONV"));
  const double adaptolbetter = params_->get<double>("ADAPTCONV_BETTER");
  const double abstolres = params_->get<double>("ABSTOLRES");
  double actresidual(0.0);

  // prepare Newton-Raphson iteration
  iternum_ = 0;
  int itemax = params_->get<int>("ITEMAX");

  // start Newton-Raphson iteration
  while (true)
  {
    iternum_++;

    // call elements to calculate system matrix and rhs and assemble
    assemble_mat_and_rhs();

    // Apply Dirichlet boundary conditions to system of equations
    // residual values are supposed to be zero at Dirichlet boundaries
    {
      // time measurement: application of DBC to system
      TEUCHOS_FUNC_TIME_MONITOR("LUBRICATION:       + apply DBC to system");

      Core::LinAlg::apply_dirichlet_to_system(
          *sysmat_, *increment_, *residual_, *zeros_, *(dbcmaps_->CondMap()));

      // additionally apply Dirichlet condition to unprojectable nodes
      // (gap undefined, i.e. no reasonalbe Reynolds equation to be solved)
      if (inf_gap_toggle_lub_ != Teuchos::null)
        Core::LinAlg::apply_dirichlet_to_system(
            *sysmat_, *increment_, *residual_, *zeros_, *inf_gap_toggle_lub_);
    }

    // abort nonlinear iteration if desired
    if (abort_nonlin_iter(iternum_, itemax, ittol, abstolres, actresidual)) break;

    // initialize increment vector
    increment_->PutScalar(0.0);

    {
      // get cpu time
      const double tcpusolve = Teuchos::Time::wallTime();

      // time measurement: call linear solver
      TEUCHOS_FUNC_TIME_MONITOR("LUBRICATION:       + call linear solver");

      // do adaptive linear solver tolerance (not in first solve)
      Core::LinAlg::SolverParams solver_params;
      if (isadapttol && iternum_ > 1)
      {
        solver_params.nonlin_tolerance = ittol;
        solver_params.nonlin_residual = actresidual;
        solver_params.lin_tol_better = adaptolbetter;
      }

      // strategy_->Solve(solver_,sysmat_,increment_,residual_,prenp_,iternum_,projector_);
      solver_params.refactor = true;
      solver_params.reset = true;
      solver_->Solve(sysmat_->EpetraOperator(), increment_, residual_, solver_params);

      solver_->ResetTolerance();

      // end time measurement for solver
      dtsolve_ = Teuchos::Time::wallTime() - tcpusolve;
    }

    //------------------------------------------------ update solution vector
    prenp_->Update(1.0, *increment_, 1.0);

  }  // nonlinear iteration

  return;
}  // TimIntImpl::nonlinear_solve


/*----------------------------------------------------------------------*
 | check if to stop the nonlinear iteration                 wirtz 11/15 |
 *----------------------------------------------------------------------*/
bool LUBRICATION::TimIntImpl::abort_nonlin_iter(const int itnum, const int itemax,
    const double ittol, const double abstolres, double& actresidual)
{
  //----------------------------------------------------- compute norms
  double incprenorm_L2(0.0);

  double prenorm_L2(0.0);

  double preresnorm(0.0);

  double preresnorminf(0.0);

  // Calculate problem-specific norms
  calc_problem_specific_norm(preresnorm, incprenorm_L2, prenorm_L2, preresnorminf);

  // care for the case that nothing really happens in the pressure
  if (prenorm_L2 < 1e-5)
  {
    prenorm_L2 = 1.0;
  }

  //-------------------------------------------------- output to screen
  // special case of very first iteration step: solution increment is not yet available
  if (itnum == 1)
    // print first line of convergence table to screen
    print_convergence_values_first_iter(itnum, itemax, ittol, preresnorm, preresnorminf);

  // ordinary case later iteration steps: solution increment can be printed and convergence check
  // should be done
  else
  {
    // print current line of convergence table to screen
    print_convergence_values(
        itnum, itemax, ittol, preresnorm, incprenorm_L2, prenorm_L2, preresnorminf);

    // convergence check
    if (preresnorm <= ittol and incprenorm_L2 / prenorm_L2 <= ittol)
    {
      // print finish line of convergence table to screen
      print_convergence_finish_line();

      return true;
    }
  }

  // abort iteration, when there's nothing more to do! -> more robustness
  // absolute tolerance for deciding if residual is (already) zero
  // prevents additional solver calls that will not improve the residual anymore
  if ((preresnorm < abstolres))
  {
    // print finish line of convergence table to screen
    print_convergence_finish_line();

    return true;
  }

  // warn if itemax is reached without convergence, but proceed to
  // next timestep...
  if ((itnum == itemax))
  {
    if (myrank_ == 0)
    {
      std::cout << "+---------------------------------------------------------------+" << std::endl;
      std::cout << "|            >>>>>> not converged in itemax steps!              |" << std::endl;
      std::cout << "+---------------------------------------------------------------+" << std::endl
                << std::endl;
    }
    // yes, we stop the iteration
    return true;
  }

  // return the maximum residual value -> used for adaptivity of linear solver tolerance
  actresidual = std::max(preresnorm, incprenorm_L2 / prenorm_L2);

  // check for INF's and NaN's before going on...
  if (std::isnan(incprenorm_L2) or std::isnan(prenorm_L2) or std::isnan(preresnorm))
    FOUR_C_THROW("calculated vector norm is NaN.");

  if (std::isinf(incprenorm_L2) or std::isinf(prenorm_L2) or std::isinf(preresnorm))
    FOUR_C_THROW("calculated vector norm is INF.");

  return false;
}  // TimIntImpl::abort_nonlin_iter

/*----------------------------------------------------------------------*
 | Set the nodal film height at time n+1                    Seitz 12/17 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::set_height_field(const int nds, Teuchos::RCP<const Epetra_Vector> gap)
{
  if (gap == Teuchos::null) FOUR_C_THROW("Gap vector is empty.");
  discret_->set_state(nds, "height", gap);

  return;
}

/*----------------------------------------------------------------------*
 | Set nodal value of film height time derivative(hdot)    Faraji 03/18 |
   at time n+1
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::SetHeightDotField(
    const int nds, Teuchos::RCP<const Epetra_Vector> heightdot)
{
  if (heightdot == Teuchos::null) FOUR_C_THROW("hdot vector is empty.");
  discret_->set_state(nds, "heightdot", heightdot);

  return;
}

/*----------------------------------------------------------------------*
 | Set nodal value of Relative Velocity at time n+1        Faraji 02/19 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::set_relative_velocity_field(
    const int nds, Teuchos::RCP<const Epetra_Vector> rel_vel)
{
  if (nds >= discret_->NumDofSets()) FOUR_C_THROW("Too few dofsets on lubrication discretization!");
  if (rel_vel == Teuchos::null) FOUR_C_THROW("no velocity provided.");
  discret_->set_state(nds, "rel_tang_vel", rel_vel);
}

/*----------------------------------------------------------------------*
 | Set the nodal average tangential velocity at time n+1    Seitz 12/17 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::set_average_velocity_field(
    const int nds, Teuchos::RCP<const Epetra_Vector> av_vel)
{
  if (nds >= discret_->NumDofSets()) FOUR_C_THROW("Too few dofsets on lubrication discretization!");
  if (av_vel == Teuchos::null) FOUR_C_THROW("no velocity provided");

  discret_->set_state(nds, "av_tang_vel", av_vel);
}

/*----------------------------------------------------------------------*
 | Calculate problem specific norm                          wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::calc_problem_specific_norm(
    double& preresnorm, double& incprenorm_L2, double& prenorm_L2, double& preresnorminf)
{
  residual_->Norm2(&preresnorm);
  increment_->Norm2(&incprenorm_L2);
  prenp_->Norm2(&prenorm_L2);
  residual_->NormInf(&preresnorminf);

  return;
}


/*----------------------------------------------------------------------*
 | print header of convergence table to screen              wirtz 11/15 |
 *----------------------------------------------------------------------*/
inline void LUBRICATION::TimIntImpl::print_convergence_header()
{
  if (myrank_ == 0)
    std::cout
        << "+------------+-------------------+--------------+--------------+------------------+\n"
        << "|- step/max -|- tol      [norm] -|-- pre-res ---|-- pre-inc ---|-- pre-res-inf ---|"
        << std::endl;

  return;
}  // LUBRICATION::TimIntImpl::print_convergence_header


/*----------------------------------------------------------------------*
 | print first line of convergence table to screen          wirtz 11/15 |
 *----------------------------------------------------------------------*/
inline void LUBRICATION::TimIntImpl::print_convergence_values_first_iter(
    const int& itnum,            //!< current Newton-Raphson iteration step
    const int& itemax,           //!< maximum number of Newton-Raphson iteration steps
    const double& ittol,         //!< relative tolerance for Newton-Raphson scheme
    const double& preresnorm,    //!< L2 norm of pressure residual
    const double& preresnorminf  //!< infinity norm of pressure residual
)
{
  if (myrank_ == 0)
    std::cout << "|  " << std::setw(3) << itnum << "/" << std::setw(3) << itemax << "   | "
              << std::setw(10) << std::setprecision(3) << std::scientific << ittol << "[L_2 ]  | "
              << std::setw(10) << std::setprecision(3) << std::scientific << preresnorm
              << "   |      --      | " << std::setw(10) << std::setprecision(3) << std::scientific
              << preresnorminf << "       | (      --     ,te=" << std::setw(10)
              << std::setprecision(3) << std::scientific << dtele_ << ")" << std::endl;

  return;
}  // LUBRICATION::TimIntImpl::print_convergence_values_first_iter


/*----------------------------------------------------------------------*
 | print current line of convergence table to screen        wirtz 11/15 |
 *----------------------------------------------------------------------*/
inline void LUBRICATION::TimIntImpl::print_convergence_values(
    const int& itnum,             //!< current Newton-Raphson iteration step
    const int& itemax,            //!< maximum number of Newton-Raphson iteration steps
    const double& ittol,          //!< relative tolerance for Newton-Raphson scheme
    const double& preresnorm,     //!< L2 norm of pressure residual
    const double& incprenorm_L2,  //!< L2 norm of pressure increment
    const double& prenorm_L2,     //!< L2 norm of pressure state vector
    const double& preresnorminf   //!< infinity norm of pressure residual
)
{
  if (myrank_ == 0)
    std::cout << "|  " << std::setw(3) << itnum << "/" << std::setw(3) << itemax << "   | "
              << std::setw(10) << std::setprecision(3) << std::scientific << ittol << "[L_2 ]  | "
              << std::setw(10) << std::setprecision(3) << std::scientific << preresnorm << "   | "
              << std::setw(10) << std::setprecision(3) << std::scientific
              << incprenorm_L2 / prenorm_L2 << "   | " << std::setw(10) << std::setprecision(3)
              << std::scientific << preresnorminf << "       | (ts=" << std::setw(10)
              << std::setprecision(3) << std::scientific << dtsolve_ << ",te=" << std::setw(10)
              << std::setprecision(3) << std::scientific << dtele_ << ")" << std::endl;

  return;
}  // LUBRICATION::TimIntImpl::print_convergence_values


/*----------------------------------------------------------------------*
 | print finish line of convergence table to screen         wirtz 11/15 |
 *----------------------------------------------------------------------*/
inline void LUBRICATION::TimIntImpl::print_convergence_finish_line()
{
  if (myrank_ == 0)
    std::cout
        << "+------------+-------------------+--------------+--------------+------------------+"
        << std::endl
        << std::endl;

  return;
}  // LUBRICATION::TimIntImpl::print_convergence_finish_line


/*----------------------------------------------------------------------*
 |  write current state to BINIO                            wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::output_state()
{
  // solution
  output_->write_vector("prenp", prenp_);

  // displacement field
  if (isale_)
  {
    Teuchos::RCP<const Epetra_Vector> dispnp = discret_->GetState(nds_disp_, "dispnp");
    if (dispnp == Teuchos::null)
      FOUR_C_THROW("Cannot extract displacement field from discretization");

    // convert dof-based Epetra vector into node-based Epetra multi-vector for postprocessing
    Teuchos::RCP<Epetra_MultiVector> dispnp_multi =
        Teuchos::rcp(new Epetra_MultiVector(*discret_->NodeRowMap(), nsd_, true));
    for (int inode = 0; inode < discret_->NumMyRowNodes(); ++inode)
    {
      Core::Nodes::Node* node = discret_->lRowNode(inode);
      for (int idim = 0; idim < nsd_; ++idim)
        (*dispnp_multi)[idim][discret_->NodeRowMap()->LID(node->Id())] =
            (*dispnp)[dispnp->Map().LID(discret_->Dof(nds_disp_, node, idim))];
    }

    output_->write_vector("dispnp", dispnp_multi, Core::IO::nodevector);
  }

  return;
}  // TimIntImpl::output_state


/*----------------------------------------------------------------------*
 | increment time and step for next iteration               wirtz 11/15 |
 *----------------------------------------------------------------------*/
inline void LUBRICATION::TimIntImpl::increment_time_and_step()
{
  step_ += 1;
  time_ += dta_;
}


/*----------------------------------------------------------------------*
 |  calculate error compared to analytical solution         wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::evaluate_error_compared_to_analytical_sol()
{
  const Inpar::LUBRICATION::CalcError calcerr =
      Core::UTILS::IntegralValue<Inpar::LUBRICATION::CalcError>(*params_, "CALCERROR");

  if (calcerr == Inpar::LUBRICATION::calcerror_no)  // do nothing (the usual case))
    return;

  // create the parameters for the error calculation
  Teuchos::ParameterList eleparams;
  eleparams.set<int>("action", LUBRICATION::calc_error);
  eleparams.set("total time", time_);
  eleparams.set<int>("calcerrorflag", calcerr);

  switch (calcerr)
  {
    case Inpar::LUBRICATION::calcerror_byfunction:
    {
      const int errorfunctnumber = params_->get<int>("CALCERRORNO");
      if (errorfunctnumber < 1)
        FOUR_C_THROW("invalid value of paramter CALCERRORNO for error function evaluation!");

      eleparams.set<int>("error function number", errorfunctnumber);
      break;
    }
    default:
      FOUR_C_THROW("Cannot calculate error. Unknown type of analytical test problem");
      break;
  }

  // provide displacement field in case of ALE
  if (isale_) eleparams.set<int>("ndsdisp", nds_disp_);

  // set vector values needed by elements
  discret_->ClearState();
  discret_->set_state("prenp", prenp_);

  // get (squared) error values
  Teuchos::RCP<Core::LinAlg::SerialDenseVector> errors =
      Teuchos::rcp(new Core::LinAlg::SerialDenseVector(4));
  discret_->EvaluateScalars(eleparams, errors);
  discret_->ClearState();

  // std::vector containing
  // [0]: relative L2 pressure error
  // [1]: relative H1 pressure error
  Teuchos::RCP<std::vector<double>> relerror = Teuchos::rcp(new std::vector<double>(2));

  if (std::abs((*errors)[2]) > 1e-14)
    (*relerror)[0] = sqrt((*errors)[0]) / sqrt((*errors)[2]);
  else
    (*relerror)[0] = sqrt((*errors)[0]);
  if (std::abs((*errors)[2]) > 1e-14)
    (*relerror)[1] = sqrt((*errors)[1]) / sqrt((*errors)[3]);
  else
    (*relerror)[1] = sqrt((*errors)[1]);

  if (myrank_ == 0)
  {
    // print last error in a separate file

    const std::string simulation = Global::Problem::Instance()->OutputControlFile()->file_name();
    const std::string fname = simulation + "_pressure_time.relerror";

    if (step_ == 0)
    {
      std::ofstream f;
      f.open(fname.c_str());
      f << "#| Step | Time | rel. L2-error  | rel. H1-error  |\n";
      f << std::setprecision(10) << step_ << " " << std::setw(1) << std::setprecision(5) << time_
        << std::setw(1) << std::setprecision(6) << " " << (*relerror)[0] << std::setw(1)
        << std::setprecision(6) << " " << (*relerror)[1] << "\n";

      f.flush();
      f.close();
    }
    else
    {
      std::ofstream f;
      f.open(fname.c_str(), std::fstream::ate | std::fstream::app);
      f << std::setprecision(10) << step_ << " " << std::setw(3) << std::setprecision(5) << time_
        << std::setw(1) << std::setprecision(6) << " " << (*relerror)[0] << std::setw(1)
        << std::setprecision(6) << " " << (*relerror)[1] << "\n";

      f.flush();
      f.close();
    }
  }

}  // LUBRICATION::TimIntImpl::evaluate_error_compared_to_analytical_sol

/*----------------------------------------------------------------------*
 | write state vectors to Gmsh postprocessing files         wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::output_to_gmsh(const int step, const double time) const
{
  // turn on/off screen output for writing process of Gmsh postprocessing file
  const bool screen_out = true;

  // create Gmsh postprocessing file
  const std::string filename = Core::IO::Gmsh::GetNewFileNameAndDeleteOldFiles(
      "solution_field_pressure", discret_->Writer()->output()->file_name(), step, 500, screen_out,
      discret_->Comm().MyPID());
  std::ofstream gmshfilecontent(filename.c_str());
  {
    // add 'View' to Gmsh postprocessing file
    gmshfilecontent << "View \" "
                    << "Prenp \" {" << std::endl;
    // draw pressure field 'Prenp' for every element
    Core::IO::Gmsh::ScalarFieldToGmsh(discret_, prenp_, gmshfilecontent);
    gmshfilecontent << "};" << std::endl;
  }

  gmshfilecontent.close();
  if (screen_out) std::cout << " done" << std::endl;
}  // TimIntImpl::output_to_gmsh


/*----------------------------------------------------------------------*
 | output mean values of pressure(s)                          wirtz 11/15 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::OutputMeanPressures(const int num)
{
  if (outmean_)
  {
    // set pressure values needed by elements
    discret_->ClearState();
    discret_->set_state("prenp", prenp_);
    // set action for elements
    Teuchos::ParameterList eleparams;
    eleparams.set<int>("action", LUBRICATION::calc_mean_pressures);
    eleparams.set("inverting", false);

    // provide displacement field in case of ALE
    if (isale_) eleparams.set<int>("ndsdisp", nds_disp_);

    // evaluate integrals of pressure(s) and domain
    Teuchos::RCP<Core::LinAlg::SerialDenseVector> pressures =
        Teuchos::rcp(new Core::LinAlg::SerialDenseVector(2));
    discret_->EvaluateScalars(eleparams, pressures);
    discret_->ClearState();  // clean up

    // extract domain integral
    const double domint = (*pressures)[1];

    // print out results to screen and file
    if (myrank_ == 0)
    {
      // screen output
      std::cout << "Mean pressure values:" << std::endl;
      std::cout << "+-------------------------------+" << std::endl;
      std::cout << "| Mean pressure:   " << std::setprecision(6) << (*pressures)[0] / domint << " |"
                << std::endl;
      std::cout << "+-------------------------------+" << std::endl << std::endl;

      // file output
      std::stringstream number;
      number << num;
      const std::string fname = Global::Problem::Instance()->OutputControlFile()->file_name() +
                                number.str() + ".meanvalues.txt";

      std::ofstream f;
      if (Step() <= 1)
      {
        f.open(fname.c_str(), std::fstream::trunc);
        f << "#| Step | Time | Domain integral |";
        f << " Total pressure |";
        f << " Mean pressure |";
        f << "\n";
      }
      else
        f.open(fname.c_str(), std::fstream::ate | std::fstream::app);

      f << Step() << " " << Time() << " " << std::setprecision(9) << domint;
      f << " " << std::setprecision(9) << (*pressures)[0];
      f << " " << std::setprecision(9) << (*pressures)[0] / domint;
      f << "\n";
      f.flush();
      f.close();
    }

  }  // if(outmean_)

}  // LUBRICATION::TimIntImpl::OutputMeanPressures

/*----------------------------------------------------------------------*
 | return system matrix downcasted as sparse matrix         wirtz 01/16 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Core::LinAlg::SparseMatrix> LUBRICATION::TimIntImpl::SystemMatrix()
{
  return Teuchos::rcp_dynamic_cast<Core::LinAlg::SparseMatrix>(sysmat_);
}


/*----------------------------------------------------------------------*
 | build linear system tangent matrix, rhs/force residual   wirtz 01/16 |
 | Monolithic EHL accesses the linearised lubrication problem           |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::Evaluate()
{
  // put zero pressure value, where no gap is defined
  if (inf_gap_toggle_lub_ != Teuchos::null)
    Core::LinAlg::apply_dirichlet_to_system(*prenp_, *residual_, *zeros_, *inf_gap_toggle_lub_);

  // call elements to calculate system matrix and rhs and assemble
  assemble_mat_and_rhs();

  // Apply Dirichlet boundary conditions to system of equations
  // residual values are supposed to be zero at Dirichlet boundaries
  Core::LinAlg::apply_dirichlet_to_system(
      *sysmat_, *increment_, *residual_, *zeros_, *(dbcmaps_->CondMap()));

  // additionally apply Dirichlet condition to unprojectable nodes
  // (gap undefined, i.e. no reasonalbe Reynolds equation to be solved)
  if (inf_gap_toggle_lub_ != Teuchos::null)
    Core::LinAlg::apply_dirichlet_to_system(
        *sysmat_, *increment_, *residual_, *zeros_, *inf_gap_toggle_lub_);
}

/*----------------------------------------------------------------------*
 | Update iteration incrementally with prescribed           wirtz 01/16 |
 | residual pressures                                                   |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::update_iter_incrementally(
    const Teuchos::RCP<const Epetra_Vector> prei  //!< input residual pressures
)
{
  // select residual pressures
  if (prei != Teuchos::null)
    // tempi_ = \f$\Delta{T}^{<k>}_{n+1}\f$
    prei_->Update(1.0, *prei, 0.0);  // set the new solution we just got
  else
    prei_->PutScalar(0.0);

  // Update using #prei_
  update_iter_incrementally();

  // leave this place
  return;
}  // update_iter_incrementally()

/*----------------------------------------------------------------------*
 | update Newton step                                       wirtz 01/16 |
 *----------------------------------------------------------------------*/
void LUBRICATION::TimIntImpl::UpdateNewton(Teuchos::RCP<const Epetra_Vector> prei)
{
  // Yes, this is complicated. But we have to be very careful
  // here. The field solver always expects an increment only. And
  // there are Dirichlet conditions that need to be preserved. So take
  // the sum of increments we get from NOX and apply the latest
  // increment only.
  update_iter_incrementally(prei);
  return;

}  // UpdateNewton()

FOUR_C_NAMESPACE_CLOSE
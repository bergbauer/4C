/*----------------------------------------------------------------------*/
/*! \file

 \brief monolithic coupling algorithm for scalar transport within porous medium

\level 2


 *----------------------------------------------------------------------*/

#include "4C_poroelast_scatra_monolithic.hpp"

#include "4C_adapter_fld_poro.hpp"
#include "4C_adapter_str_fpsiwrapper.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_fem_general_assemblestrategy.hpp"
#include "4C_fluid_ele_action.hpp"
#include "4C_global_data.hpp"
#include "4C_io_control.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_linear_solver_method.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_poroelast_scatra_utils.hpp"
#include "4C_scatra_ele_action.hpp"
#include "4C_scatra_timint_implicit.hpp"
#include "4C_utils_parameter_list.hpp"

#include <Teuchos_TimeMonitor.hpp>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
PoroElastScaTra::PoroScatraMono::PoroScatraMono(
    const Epetra_Comm& comm, const Teuchos::ParameterList& timeparams)
    : PoroScatraBase(comm, timeparams),
      printscreen_(true),  // ADD INPUT PARAMETER
      printiter_(true),    // ADD INPUT PARAMETER
      timer_("PoroScatraMonoSolve", true),
      iterinc_(Teuchos::null),
      zeros_(Teuchos::null),
      systemmatrix_(Teuchos::null),
      rhs_(Teuchos::null),
      blockrowdofmap_(Teuchos::null),
      directsolve_(true)
{
  const Teuchos::ParameterList& sdynparams =
      Global::Problem::Instance()->structural_dynamic_params();

  // some solver paramaters are red form the structure dynamic list (this is not the best way to do
  // it ...)
  solveradapttol_ = (Core::UTILS::IntegralValue<int>(sdynparams, "ADAPTCONV") == 1);
  solveradaptolbetter_ = (sdynparams.get<double>("ADAPTCONV_BETTER"));

  blockrowdofmap_ = Teuchos::rcp(new Core::LinAlg::MultiMapExtractor);
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::Timeloop()
{
  while (NotFinished())
  {
    do_time_step();
  }
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::do_time_step()
{
  // counter and print header
  // predict solution of both field (call the adapter)
  prepare_time_step();

  // Newton-Raphson iteration
  Solve();

  // calculate stresses, strains, energies
  prepare_output();

  // update all single field solvers
  update();

  // write output to screen and files
  output();
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::read_restart(int restart)
{
  // read restart information, set vectors and variables
  // (Note that dofmaps might have changed in a redistribution call!)
  if (restart)
  {
    SetScatraSolution();
    SetPoroSolution();

    poro_field()->read_restart(restart);
    ScaTraField()->read_restart(restart);

    // in case of submeshes, we need to rebuild the subproxies, also (they are reset during restart)
    if (poro_field()->HasSubmeshes())
      replace_dof_sets(structure_field()->discretization(), fluid_field()->discretization(),
          ScaTraField()->discretization());

    // the variables need to be set on other field
    SetScatraSolution();
    SetPoroSolution();

    // second restart needed due to two way coupling.
    ScaTraField()->read_restart(restart);
    poro_field()->read_restart(restart);

    // in case of submeshes, we need to rebuild the subproxies, also (they are reset during restart)
    if (poro_field()->HasSubmeshes())
      replace_dof_sets(structure_field()->discretization(), fluid_field()->discretization(),
          ScaTraField()->discretization());

    SetTimeStep(poro_field()->Time(), restart);

    // Material pointers to other field were deleted during read_restart().
    // They need to be reset.
    PoroElast::UTILS::SetMaterialPointersMatchingGrid(
        poro_field()->structure_field()->discretization(), ScaTraField()->discretization());
    PoroElast::UTILS::SetMaterialPointersMatchingGrid(
        poro_field()->fluid_field()->discretization(), ScaTraField()->discretization());
  }
}

/*----------------------------------------------------------------------*/
// prepare time step
/*----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::prepare_time_step(bool printheader)
{
  // the global control routine has its own time_ and step_ variables, as well as the single fields
  // keep them in sinc!
  increment_time_and_step();

  if (printheader) print_header();

  SetPoroSolution();
  ScaTraField()->prepare_time_step();
  // set structure-based scalar transport values
  SetScatraSolution();

  poro_field()->prepare_time_step();
  SetPoroSolution();
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::prepare_output()
{
  constexpr bool force_prepare = false;
  poro_field()->prepare_output(force_prepare);
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::output()
{
  poro_field()->Output();
  ScaTraField()->check_and_write_output_and_restart();
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::update()
{
  poro_field()->Update();

  ScaTraField()->Update();
  ScaTraField()->evaluate_error_compared_to_analytical_sol();
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::Solve()
{
  // we do a Newton-Raphson iteration here.
  // the specific time integration has set the following
  // --> On #rhs_ is the positive force residuum
  // --> On #systemmatrix_ is the effective dynamic tangent matrix

  //-------------------------------------- initialize variables needed in newton loop
  iter_ = 1;
  normrhs_ = 0.0;
  norminc_ = 0.0;

  // incremental solution vector with length of all dofs
  iterinc_ = Core::LinAlg::CreateVector(*dof_row_map(), true);
  iterinc_->PutScalar(0.0);

  // a zero vector of full length
  zeros_ = Core::LinAlg::CreateVector(*dof_row_map(), true);
  zeros_->PutScalar(0.0);

  //---------------------------------------------- iteration loop

  // equilibrium iteration loop (loop over k)
  while (((not Converged()) and (iter_ <= itermax_)) or (iter_ <= itermin_))
  {
    timer_.reset();
    Teuchos::Time timer("PoroScatraMonoSolve", true);

    // compute residual forces #rhs_ and tangent #tang_
    // whose components are globally oriented
    // build linear system stiffness matrix and rhs/force residual for each
    // field, here e.g. for structure field: field want the iteration increment
    // 1.) Update(iterinc_),
    // 2.) evaluate_force_stiff_residual(),
    // 3.) prepare_system_for_newton_solve()
    Evaluate(iterinc_);

    // check whether we have a sanely filled tangent matrix
    if (not systemmatrix_->Filled())
    {
      FOUR_C_THROW("Effective tangent matrix must be filled here");
    }

    // create full monolithic rhs vector
    setup_rhs(iter_ == 1);

    // fd_check();

    // build norms
    build_convergence_norms();

    if ((not Converged()) or combincfres_ == Inpar::PoroElast::bop_or)
    {
      // (Newton-ready) residual with blanked Dirichlet DOFs (see adapter_timint!)
      // is done in prepare_system_for_newton_solve() within Evaluate(iterinc_)
      linear_solve();
      // cout << "  time for Evaluate linear_solve: " << timer.totalElapsedTime(true) << "\n";
      // timer.reset();

      // reset solver tolerance
      solver_->ResetTolerance();

      // build norms
      build_convergence_norms();
    }

    // print stuff
    print_newton_iter();

    // increment equilibrium loop index
    iter_ += 1;
  }  // end equilibrium loop

  //---------------------------------------------- iteration loop

  // correct iteration counter
  iter_ -= 1;

  // test whether max iterations was hit
  if ((Converged()) and (Comm().MyPID() == 0))
  {
    print_newton_conv();
  }
  else if (iter_ >= itermax_)
  {
    FOUR_C_THROW("Newton unconverged in %d iterations", iter_);
  }

}  // Solve()

/*----------------------------------------------------------------------*
 | evaluate the single fields                              vuong 01/12   |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::Evaluate(Teuchos::RCP<const Epetra_Vector> stepinc)
{
  TEUCHOS_FUNC_TIME_MONITOR("PoroScatraMono::Monolithic::Evaluate");

  // displacement and fluid velocity & pressure incremental vector
  Teuchos::RCP<const Epetra_Vector> porostructinc;
  Teuchos::RCP<const Epetra_Vector> porofluidinc;
  Teuchos::RCP<const Epetra_Vector> scatrainc;

  // if an increment vector exists
  if (stepinc != Teuchos::null)
  {
    // process structure unknowns of the first field
    porostructinc = extractor()->ExtractVector(stepinc, 0);
    porofluidinc = extractor()->ExtractVector(stepinc, 1);

    // process fluid unknowns of the second field
    scatrainc = extractor()->ExtractVector(stepinc, 2);
  }

  // Newton update of the fluid field
  // update velocities and pressures before passed to the structural field
  //  update_iter_incrementally(fx),
  ScaTraField()->UpdateIter(scatrainc);

  // call all elements and assemble rhs and matrices
  /// poro field

  // structure Evaluate (builds tangent, residual and applies DBC)
  // apply current velocity and pressures to structure
  SetScatraSolution();

  // access poro problem to build poro-poro block
  poro_field()->Evaluate(porostructinc, porofluidinc, iter_ == 1);

  /// scatra field

  // Scatra Evaluate
  // (builds tangent, residual and applies DBC and recent coupling values)
  SetPoroSolution();

  // access ScaTra problem to build scatra-scatra block
  ScaTraField()->PrepareLinearSolve();

  // fill off diagonal blocks and build monolithic system matrix
  setup_system_matrix();
}

/*----------------------------------------------------------------------*
 | setup system (called in poro_dyn.cpp)                 vuong 01/12    |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::SetupSystem()
{
  // setup the poro subsystem first
  poro_field()->SetupSystem();

  // create combined map
  std::vector<Teuchos::RCP<const Epetra_Map>> vecSpaces;

  {
    // vecSpaces.push_back(poro_field()->dof_row_map());
    vecSpaces.push_back(poro_field()->DofRowMapStructure());
    vecSpaces.push_back(poro_field()->DofRowMapFluid());
    const Epetra_Map* dofrowmapscatra = (ScaTraField()->discretization())->dof_row_map(0);
    vecSpaces.push_back(Teuchos::rcp(dofrowmapscatra, false));
  }

  if (vecSpaces[0]->NumGlobalElements() == 0) FOUR_C_THROW("No poro structure equation. Panic.");
  if (vecSpaces[1]->NumGlobalElements() == 0) FOUR_C_THROW("No poro fluid equation. Panic.");
  if (vecSpaces[2]->NumGlobalElements() == 0) FOUR_C_THROW("No scatra equation. Panic.");

  // build dof row map of monolithic system
  set_dof_row_maps(vecSpaces);

  // build dbc map of monolithic system
  {
    const Teuchos::RCP<const Epetra_Map> porocondmap = poro_field()->combined_dbc_map();
    const Teuchos::RCP<const Epetra_Map> scatracondmap = ScaTraField()->DirichMaps()->CondMap();
    Teuchos::RCP<const Epetra_Map> dbcmap =
        Core::LinAlg::MergeMap(porocondmap, scatracondmap, false);

    // Finally, create the global FSI Dirichlet map extractor
    dbcmaps_ = Teuchos::rcp(new Core::LinAlg::MapExtractor(*dof_row_map(), dbcmap, true));
    if (dbcmaps_ == Teuchos::null) FOUR_C_THROW("Creation of Dirichlet map extractor failed.");
  }

  // initialize Poroscatra-systemmatrix_
  systemmatrix_ =
      Teuchos::rcp(new Core::LinAlg::BlockSparseMatrix<Core::LinAlg::DefaultBlockMatrixStrategy>(
          *extractor(), *extractor(), 81, false, true));

  {
    std::vector<Teuchos::RCP<const Epetra_Map>> scatravecSpaces;
    const Epetra_Map* dofrowmapscatra = (ScaTraField()->discretization())->dof_row_map(0);
    scatravecSpaces.push_back(Teuchos::rcp(dofrowmapscatra, false));
    scatrarowdofmap_.Setup(*dofrowmapscatra, scatravecSpaces);
  }

  k_pss_ = Teuchos::rcp(
      new Core::LinAlg::SparseMatrix(*(poro_field()->DofRowMapStructure()), 81, true, true));
  k_pfs_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*(poro_field()->DofRowMapFluid()),
      //*(fluid_field()->dof_row_map()),
      81, true, true));

  k_sps_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(
      *(ScaTraField()->discretization()->dof_row_map()), 81, true, true));
  k_spf_ =
      Teuchos::rcp(new Core::LinAlg::SparseMatrix(*(ScaTraField()->discretization()->dof_row_map()),
          //*(fluid_field()->dof_row_map()),
          81, true, true));
}

/*----------------------------------------------------------------------*
 | setup RHS (like fsimon)                                 vuong 01/12  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::setup_rhs(bool firstcall)
{
  // create full monolithic rhs vector
  if (rhs_ == Teuchos::null) rhs_ = Teuchos::rcp(new Epetra_Vector(*dof_row_map(), true));

  // fill the Poroelasticity rhs vector rhs_ with the single field rhss
  setup_vector(*rhs_, poro_field()->RHS(), ScaTraField()->Residual());
}

/*----------------------------------------------------------------------*
 | setup vector of the structure and fluid field            vuong 01/12|
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::setup_vector(
    Epetra_Vector& f, Teuchos::RCP<const Epetra_Vector> pv, Teuchos::RCP<const Epetra_Vector> sv)
{
  // extract dofs of the two fields
  // and put the poro/scatra field vector into the global vector f
  // noticing the block number

  //  Teuchos::RCP<const Epetra_Vector> psx;
  //  Teuchos::RCP<const Epetra_Vector> pfx;

  extractor()->InsertVector(*(poro_field()->Extractor()->ExtractVector(pv, 0)), 0, f);
  extractor()->InsertVector(*(poro_field()->Extractor()->ExtractVector(pv, 1)), 1, f);
  extractor()->InsertVector(*sv, 2, f);
}

/*----------------------------------------------------------------------*
 | setup system matrix of poroelasticity                   vuong 01/12  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::setup_system_matrix()
{
  // set loma block matrix to zero
  systemmatrix_->Zero();

  //----------------------------------------------------------------------
  // 1st diagonal block (upper left): poro weighting - poro solution
  //----------------------------------------------------------------------
  // get matrix block
  Teuchos::RCP<Core::LinAlg::BlockSparseMatrixBase> mat_pp = poro_field()->block_system_matrix();

  // uncomplete matrix block (appears to be required in certain cases)
  mat_pp->UnComplete();

  // assign matrix block
  systemmatrix_->Assign(0, 0, Core::LinAlg::View, mat_pp->Matrix(0, 0));
  systemmatrix_->Assign(0, 1, Core::LinAlg::View, mat_pp->Matrix(0, 1));
  systemmatrix_->Assign(1, 0, Core::LinAlg::View, mat_pp->Matrix(1, 0));
  systemmatrix_->Assign(1, 1, Core::LinAlg::View, mat_pp->Matrix(1, 1));

  //----------------------------------------------------------------------
  // 2nd diagonal block (lower right): scatra weighting - scatra solution
  //----------------------------------------------------------------------
  // get matrix block
  Teuchos::RCP<Core::LinAlg::SparseMatrix> mat_ss = ScaTraField()->SystemMatrix();

  // uncomplete matrix block (appears to be required in certain cases)
  mat_ss->UnComplete();

  // assign matrix block
  systemmatrix_->Assign(2, 2, Core::LinAlg::View, *mat_ss);

  // complete scatra block matrix
  systemmatrix_->Complete();

  //----------------------------------------------------------------------
  // 1st off-diagonal block (upper right): poro weighting - scatra solution
  //----------------------------------------------------------------------

  // evaluate off-diagonal matrix block in fluid
  evaluate_od_block_mat_poro();

  // k_ps_->Complete(mat_pp->DomainMap(),mat_ss->RangeMap());
  //  k_ps_->Complete();
  //
  //  Teuchos::RCP<Core::LinAlg::SparseMatrix> k_ps_sparse = k_ps_->Merge();

  // uncomplete matrix block (appears to be required in certain cases)
  // k_ps_sparse->UnComplete();
  k_pss_->UnComplete();
  k_pfs_->UnComplete();

  // assign matrix block
  systemmatrix_->Assign(0, 2, Core::LinAlg::View, *(k_pss_));
  systemmatrix_->Assign(1, 2, Core::LinAlg::View, *(k_pfs_));

  //----------------------------------------------------------------------
  // 2nd off-diagonal block (lower left): scatra weighting - poro solution
  //----------------------------------------------------------------------

  // evaluate off-diagonal matrix block in scatra
  evaluate_od_block_mat_scatra();

  //  k_sp_->Complete();
  //
  //  Teuchos::RCP<Core::LinAlg::SparseMatrix> k_sp_sparse = k_sp_->Merge();

  // uncomplete matrix block (appears to be required in certain cases)
  // k_sp_sparse->UnComplete();
  k_sps_->UnComplete();
  k_spf_->UnComplete();

  // assign matrix block
  systemmatrix_->Assign(2, 0, Core::LinAlg::View, *(k_sps_));
  systemmatrix_->Assign(2, 1, Core::LinAlg::View, *(k_spf_));

  // complete block matrix
  systemmatrix_->Complete();
}

/*----------------------------------------------------------------------*
 | Solve linear Poroelast_SCATRAicity system                      vuong 01/12   |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::linear_solve()
{
  // Solve for inc_ = [disi_,tempi_]
  // Solve K_Teffdyn . IncX = -R  ===>  IncX_{n+1} with X=[d,T]
  // \f$x_{i+1} = x_i + \Delta x_i\f$
  Core::LinAlg::SolverParams solver_params;
  if (solveradapttol_ and (iter_ > 1))
  {
    solver_params.nonlin_tolerance = tolfres_;
    solver_params.nonlin_residual = normrhs_;
    solver_params.lin_tol_better = solveradaptolbetter_;
  }
  // apply Dirichlet BCs to system of equations
  iterinc_->PutScalar(0.0);  // Useful? depends on solver and more

  if (directsolve_)
  {
    // merge blockmatrix to SparseMatrix and solve
    Teuchos::RCP<Core::LinAlg::SparseMatrix> sparse = systemmatrix_->Merge();

    Core::LinAlg::apply_dirichlet_to_system(
        *sparse, *iterinc_, *rhs_, *zeros_, *combined_dbc_map());
    //  if ( Comm().MyPID()==0 ) { cout << " DBC applied to system" << endl; }

    // standard solver call
    solver_params.refactor = true;
    solver_params.reset = iter_ == 1;
    solver_->Solve(sparse->EpetraOperator(), iterinc_, rhs_, solver_params);
    //  if ( Comm().MyPID()==0 ) { cout << " Solved" << endl; }
  }
  else
  {
    // in case of inclined boundary conditions
    // rotate systemmatrix_ using get_loc_sys_trafo()!=Teuchos::null
    Core::LinAlg::apply_dirichlet_to_system(
        *systemmatrix_, *iterinc_, *rhs_, *zeros_, *combined_dbc_map());
    solver_params.refactor = true;
    solver_params.reset = iter_ == 1;
    solver_->Solve(systemmatrix_->EpetraOperator(), iterinc_, rhs_, solver_params);
  }
}

/*----------------------------------------------------------------------*
 | setup solver for monolithic system                    vuong 01/12     |
 *----------------------------------------------------------------------*/
bool PoroElastScaTra::PoroScatraMono::SetupSolver()
{
  //  solver
  // create a linear solver
  // get dynamic section of poroelasticity
  const Teuchos::ParameterList& poroscatradyn =
      Global::Problem::Instance()->poro_scatra_control_params();
  // get the solver number used for linear poroelasticity solver
  const int linsolvernumber = poroscatradyn.get<int>("LINEAR_SOLVER");
  // check if the poroelasticity solver has a valid solver number
  if (linsolvernumber == (-1))
    FOUR_C_THROW(
        "no linear solver defined for scalar transport in porous media. Please set LINEAR_SOLVER "
        "in POROSCATRA CONTROL to a valid number!");
  const Teuchos::ParameterList& solverparams =
      Global::Problem::Instance()->SolverParams(linsolvernumber);
  const auto solvertype =
      Teuchos::getIntegralValue<Core::LinearSolver::SolverType>(solverparams, "SOLVER");

  directsolve_ = (solvertype == Core::LinearSolver::SolverType::umfpack);

  if (directsolve_)
  {
    solver_ = Teuchos::rcp(new Core::LinAlg::Solver(solverparams, Comm()));
  }
  else
    // create a linear solver
    // create_linear_solver();
    FOUR_C_THROW("no implicit solver supported yet!");

  // Get the parameters for the Newton iteration
  itermax_ = poroscatradyn.get<int>("ITEMAX");
  itermin_ = poroscatradyn.get<int>("ITEMIN");
  normtypeinc_ = Core::UTILS::IntegralValue<Inpar::PoroElast::ConvNorm>(poroscatradyn, "NORM_INC");
  normtypeinc_ = Core::UTILS::IntegralValue<Inpar::PoroElast::ConvNorm>(poroscatradyn, "NORM_INC");
  normtypefres_ =
      Core::UTILS::IntegralValue<Inpar::PoroElast::ConvNorm>(poroscatradyn, "NORM_RESF");
  combincfres_ =
      Core::UTILS::IntegralValue<Inpar::PoroElast::BinaryOp>(poroscatradyn, "NORMCOMBI_RESFINC");
  vectornormfres_ =
      Core::UTILS::IntegralValue<Inpar::PoroElast::VectorNorm>(poroscatradyn, "VECTORNORM_RESF");
  vectornorminc_ =
      Core::UTILS::IntegralValue<Inpar::PoroElast::VectorNorm>(poroscatradyn, "VECTORNORM_INC");

  tolinc_ = poroscatradyn.get<double>("TOLINC_GLOBAL");
  tolfres_ = poroscatradyn.get<double>("TOLRES_GLOBAL");

  tolinc_struct_ = poroscatradyn.get<double>("TOLINC_DISP");
  tolinc_velocity_ = poroscatradyn.get<double>("TOLINC_VEL");
  tolinc_pressure_ = poroscatradyn.get<double>("TOLINC_PRES");
  tolinc_scalar_ = poroscatradyn.get<double>("TOLINC_SCALAR");
  // tolinc_porosity_= poroscatradyn.get<double> ("TOLINC_PORO");

  tolfres_struct_ = poroscatradyn.get<double>("TOLRES_DISP");
  tolfres_velocity_ = poroscatradyn.get<double>("TOLRES_VEL");
  tolfres_pressure_ = poroscatradyn.get<double>("TOLRES_PRES");
  tolfres_scalar_ = poroscatradyn.get<double>("TOLINC_SCALAR");
  // tolfres_porosity_= poroscatradyn.get<double> ("TOLRES_PORO");

  return true;
}

/*----------------------------------------------------------------------*
 | check convergence of Newton iteration (public)         vuong 01/12   |
 *----------------------------------------------------------------------*/
bool PoroElastScaTra::PoroScatraMono::Converged()
{
  // check for single norms
  bool convinc = false;
  bool convfres = false;

  // residual increments
  switch (normtypeinc_)
  {
    case Inpar::PoroElast::convnorm_abs_global:
      convinc = norminc_ < tolinc_;
      break;
    case Inpar::PoroElast::convnorm_abs_singlefields:
      convinc = (normincstruct_ < tolinc_struct_ and normincfluidvel_ < tolinc_velocity_ and
                 normincfluidpres_ < tolinc_pressure_ and normincscalar_ < tolinc_scalar_
          //                  normincporo_      < tolinc_porosity_
      );
      break;
    default:
      FOUR_C_THROW("Cannot check for convergence of residual values!");
      break;
  }

  // residual forces
  switch (normtypefres_)
  {
    case Inpar::PoroElast::convnorm_abs_global:
      convfres = normrhs_ < tolfres_;
      break;
    case Inpar::PoroElast::convnorm_abs_singlefields:
      convfres = (normrhsstruct_ < tolfres_struct_ and normrhsfluidvel_ < tolfres_velocity_ and
                  normrhsfluidpres_ < tolfres_pressure_ and normrhsscalar_ < tolfres_scalar_
          //                   normrhsporo_      < tolfres_porosity_
      );
      break;
    default:
      FOUR_C_THROW("Cannot check for convergence of residual forces!");
      break;
  }

  // combine increments and forces
  bool conv = false;
  switch (combincfres_)
  {
    case Inpar::PoroElast::bop_and:
      conv = convinc and convfres;
      break;
    case Inpar::PoroElast::bop_or:
      conv = convinc or convfres;
      break;
    default:
      FOUR_C_THROW("Something went terribly wrong with binary operator!");
      break;
  }

  // return things
  return conv;
}  // Converged()

/*----------------------------------------------------------------------*
 | print Newton-Raphson iteration to screen and error file    vuong 08/13   |
 | originally by lw 12/07, tk 01/08                                     |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::print_newton_iter()
{
  // print to standard out
  // replace myrank_ here general by Comm().MyPID()
  if ((Comm().MyPID() == 0) and printscreen_ and (Step() % printscreen_ == 0) and printiter_)
  {
    if (iter_ == 1) print_newton_iter_header(stdout);
    print_newton_iter_text(stdout);
  }

}  // print_newton_iter()


/*----------------------------------------------------------------------*
 | print Newton-Raphson iteration to screen and error file    vuong 08/13  |
 | originally by lw 12/07, tk 01/08                                     |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::print_newton_iter_header(FILE* ofile)
{
  // open outstringstream
  std::ostringstream oss;

  oss << "------------------------------------------------------------" << std::endl;
  oss << "                   Newton-Raphson Scheme                    " << std::endl;
  oss << "                NormRES " << VectorNormString(vectornormfres_);
  oss << "     NormINC " << VectorNormString(vectornorminc_) << "                    " << std::endl;
  oss << "------------------------------------------------------------" << std::endl;

  // enter converged state etc
  oss << "numiter";

  // different style due relative or absolute error checking

  // --------------------------------------------------------global system test
  // residual forces
  switch (normtypefres_)
  {
    case Inpar::PoroElast::convnorm_abs_global:
      oss << std::setw(15) << "abs-res"
          << "(" << std::setw(5) << std::setprecision(2) << tolfres_ << ")";
      break;
      //    case Inpar::PoroElastScaTra::convnorm_rel_global:
      //      oss << std::setw(18) << "rel-res";
      break;
    case Inpar::PoroElast::convnorm_abs_singlefields:
      //  case Inpar::PoroElastScaTra::convnorm_rel_singlefields:
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  switch (normtypeinc_)
  {
    case Inpar::PoroElast::convnorm_abs_global:
      oss << std::setw(15) << "abs-inc"
          << "(" << std::setw(5) << std::setprecision(2) << tolinc_ << ")";
      break;
      //    case Inpar::PoroElastScaTra::convnorm_rel_global:
      //      oss << std::setw(18) << "rel-inc";
      break;
    case Inpar::PoroElast::convnorm_abs_singlefields:
      // case Inpar::PoroElastScaTra::convnorm_rel_singlefields:
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  // --------------------------------------------------------single field test
  switch (normtypefres_)
  {
    case Inpar::PoroElast::convnorm_abs_global:
      break;
    case Inpar::PoroElast::convnorm_abs_singlefields:
      oss << std::setw(15) << "abs-s-res"
          << "(" << std::setw(5) << std::setprecision(2) << tolfres_struct_ << ")";
      //      if(porositydof_)
      //        oss <<std::setw(15)<< "abs-poro-res"<<"("<<std::setprecision(2)
      //        <<tolfres_porosity_<<")";
      oss << std::setw(15) << "abs-fvel-res"
          << "(" << std::setw(5) << std::setprecision(2) << tolfres_velocity_ << ")";
      oss << std::setw(15) << "abs-fpres-res"
          << "(" << std::setw(5) << std::setprecision(2) << tolfres_pressure_ << ")";
      oss << std::setw(15) << "abs-sca-res"
          << "(" << std::setw(5) << std::setprecision(2) << tolfres_scalar_ << ")";
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  switch (normtypeinc_)
  {
    case Inpar::PoroElast::convnorm_abs_global:
      break;
    case Inpar::PoroElast::convnorm_abs_singlefields:
      oss << std::setw(15) << "abs-s-inc"
          << "(" << std::setw(5) << std::setprecision(2) << tolinc_struct_ << ")";
      //      if(porositydof_)
      //        oss <<std::setw(15)<< "abs-poro-inc"<<"("<<std::setprecision(2)
      //        <<tolinc_porosity_<<")";
      oss << std::setw(15) << "abs-fvel-inc"
          << "(" << std::setw(5) << std::setprecision(2) << tolinc_velocity_ << ")";
      oss << std::setw(15) << "abs-fpres-inc"
          << "(" << std::setw(5) << std::setprecision(2) << tolinc_pressure_ << ")";
      oss << std::setw(15) << "abs-sca-inc"
          << "(" << std::setw(5) << std::setprecision(2) << tolinc_scalar_ << ")";
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  // add solution time
  oss << std::setw(14) << "wct";

  // finish oss
  oss << std::ends;

  // print to screen (could be done differently...)
  if (ofile == nullptr) FOUR_C_THROW("no ofile available");
  fprintf(ofile, "%s\n", oss.str().c_str());

  // print it, now
  fflush(ofile);

  // nice to have met you
  return;
}  // print_newton_iter_header()


/*----------------------------------------------------------------------*
 | print Newton-Raphson iteration to screen              vuong 08/13 |
 | originally by lw 12/07, tk 01/08                                     |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::print_newton_iter_text(FILE* ofile)
{
  // open outstringstream
  std::ostringstream oss;

  // enter converged state etc
  oss << std::setw(7) << iter_;

  // different style due relative or absolute error checking

  // --------------------------------------------------------global system test
  // residual forces
  switch (normtypefres_)
  {
    case Inpar::PoroElast::convnorm_abs_global:
      oss << std::setw(22) << std::setprecision(5) << std::scientific << normrhs_;
      break;
    case Inpar::PoroElast::convnorm_abs_singlefields:
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }
  // increments
  switch (normtypeinc_)
  {
    case Inpar::PoroElast::convnorm_abs_global:
      oss << std::setw(22) << std::setprecision(5) << std::scientific << norminc_;
      break;
    case Inpar::PoroElast::convnorm_abs_singlefields:
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  // --------------------------------------------------------single field test
  switch (normtypefres_)
  {
    case Inpar::PoroElast::convnorm_abs_singlefields:
      oss << std::setw(22) << std::setprecision(5) << std::scientific << normrhsstruct_;
      //      if(porositydof_)
      //        oss << std::setw(22) << std::setprecision(5) << std::scientific << normrhsporo_;
      oss << std::setw(22) << std::setprecision(5) << std::scientific << normrhsfluidvel_;
      oss << std::setw(22) << std::setprecision(5) << std::scientific << normrhsfluidpres_;
      oss << std::setw(22) << std::setprecision(5) << std::scientific << normrhsscalar_;
      break;
    case Inpar::PoroElast::convnorm_abs_global:
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  switch (normtypeinc_)
  {
    case Inpar::PoroElast::convnorm_abs_singlefields:
      oss << std::setw(22) << std::setprecision(5) << std::scientific << normincstruct_;
      //      if(porositydof_)
      //        oss << std::setw(22) << std::setprecision(5) << std::scientific << normincporo_;
      oss << std::setw(22) << std::setprecision(5) << std::scientific << normincfluidvel_;
      oss << std::setw(22) << std::setprecision(5) << std::scientific << normincfluidpres_;
      oss << std::setw(22) << std::setprecision(5) << std::scientific << normincscalar_;
      break;
    case Inpar::PoroElast::convnorm_abs_global:
      break;
    default:
      FOUR_C_THROW("You should not turn up here.");
      break;
  }

  // add solution time
  oss << std::setw(14) << std::setprecision(2) << std::scientific << timer_.totalElapsedTime(true);

  // finish oss
  oss << std::ends;

  // print to screen (could be done differently...)
  if (ofile == nullptr) FOUR_C_THROW("no ofile available");
  fprintf(ofile, "%s\n", oss.str().c_str());

  // print it, now
  fflush(ofile);

  // nice to have met you
  return;

}  // print_newton_iter_text

/*----------------------------------------------------------------------*
 | print statistics of converged NRI                      vuong 08/13    |
 | orignially by bborn 08/09                                            |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::print_newton_conv()
{
  // somebody did the door
  return;
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::build_convergence_norms()
{
  //------------------------------------------------------------ build residual force norms

  // global norm
  normrhs_ = PoroElast::UTILS::calculate_vector_norm(vectornormfres_, rhs_);

  // split vectors
  Teuchos::RCP<const Epetra_Vector> rhs_s;
  Teuchos::RCP<const Epetra_Vector> rhs_f;
  Teuchos::RCP<const Epetra_Vector> rhs_fvel;
  Teuchos::RCP<const Epetra_Vector> rhs_fpres;
  Teuchos::RCP<const Epetra_Vector> rhs_scalar;

  // process structure unknowns of the first field
  rhs_s = extractor()->ExtractVector(rhs_, 0);

  // process fluid unknowns of the second field
  rhs_f = extractor()->ExtractVector(rhs_, 1);
  rhs_fvel = poro_field()->fluid_field()->ExtractVelocityPart(rhs_f);
  rhs_fpres = poro_field()->fluid_field()->ExtractPressurePart(rhs_f);

  // process scalar unknowns of the third field
  rhs_scalar = extractor()->ExtractVector(rhs_, 2);

  //  if(porositydof_)
  //  {
  //    Teuchos::RCP<const Epetra_Vector> rhs_poro = porositysplitter_->ExtractCondVector(rhs_s);
  //    Teuchos::RCP<const Epetra_Vector> rhs_sdisp = porositysplitter_->ExtractOtherVector(rhs_s);
  //
  //    normrhsstruct_ = UTILS::calculate_vector_norm(vectornormfres_,rhs_sdisp);
  //    normrhsporo_ = UTILS::calculate_vector_norm(vectornormfres_,rhs_poro);
  //  }
  //  else
  normrhsstruct_ = PoroElast::UTILS::calculate_vector_norm(vectornormfres_, rhs_s);

  normrhsfluid_ = PoroElast::UTILS::calculate_vector_norm(vectornormfres_, rhs_f);
  normrhsfluidvel_ = PoroElast::UTILS::calculate_vector_norm(vectornormfres_, rhs_fvel);
  normrhsfluidpres_ = PoroElast::UTILS::calculate_vector_norm(vectornormfres_, rhs_fpres);

  normrhsscalar_ = PoroElast::UTILS::calculate_vector_norm(vectornormfres_, rhs_scalar);


  //------------------------------------------------------------- build residual increment norms
  iterinc_->Norm2(&norminc_);

  // displacement and fluid velocity & pressure incremental vector
  Teuchos::RCP<const Epetra_Vector> interincs;
  Teuchos::RCP<const Epetra_Vector> interincf;
  Teuchos::RCP<const Epetra_Vector> interincfvel;
  Teuchos::RCP<const Epetra_Vector> interincfpres;
  Teuchos::RCP<const Epetra_Vector> interincscalar;

  // process structure unknowns of the first field
  interincs = extractor()->ExtractVector(iterinc_, 0);

  // process fluid unknowns of the second field
  interincf = extractor()->ExtractVector(iterinc_, 1);
  interincfvel = poro_field()->fluid_field()->ExtractVelocityPart(interincf);
  interincfpres = poro_field()->fluid_field()->ExtractPressurePart(interincf);

  // process scalar unknowns of the third field
  interincscalar = extractor()->ExtractVector(iterinc_, 2);

  //  if(porositydof_)
  //  {
  //    Teuchos::RCP<const Epetra_Vector> interincporo =
  //    porositysplitter_->ExtractCondVector(interincs); Teuchos::RCP<const Epetra_Vector>
  //    interincsdisp = porositysplitter_->ExtractOtherVector(interincs);
  //
  //    normincstruct_     = UTILS::calculate_vector_norm(vectornorminc_,interincsdisp);
  //    normincporo_       = UTILS::calculate_vector_norm(vectornorminc_,interincporo);
  //  }
  //  else
  normincstruct_ = PoroElast::UTILS::calculate_vector_norm(vectornorminc_, interincs);

  normincfluid_ = PoroElast::UTILS::calculate_vector_norm(vectornorminc_, interincf);
  normincfluidvel_ = PoroElast::UTILS::calculate_vector_norm(vectornorminc_, interincfvel);
  normincfluidpres_ = PoroElast::UTILS::calculate_vector_norm(vectornorminc_, interincfpres);

  normincscalar_ = PoroElast::UTILS::calculate_vector_norm(vectornorminc_, interincscalar);

  return;
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> PoroElastScaTra::PoroScatraMono::dof_row_map() const
{
  return blockrowdofmap_->FullMap();
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> PoroElastScaTra::PoroScatraMono::combined_dbc_map() const
{
  return dbcmaps_->CondMap();
}

/*----------------------------------------------------------------------*
 |                                                         vuong 08/13  |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Core::LinAlg::SparseMatrix> PoroElastScaTra::PoroScatraMono::SystemMatrix()
{
  return systemmatrix_->Merge();
}

/*----------------------------------------------------------------------*
 | put the single maps to one full                                      |
 | Poroelast_SCATRAicity map together                              vuong 01/12 |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::set_dof_row_maps(
    const std::vector<Teuchos::RCP<const Epetra_Map>>& maps)
{
  Teuchos::RCP<Epetra_Map> fullmap = Core::LinAlg::MultiMapExtractor::MergeMaps(maps);

  // full monolithic-blockmap
  blockrowdofmap_->Setup(*fullmap, maps);
}


/*----------------------------------------------------------------------*
 |  Evaluate off diagonal matrix in poro row                  vuong 08/13   |
 *---------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::evaluate_od_block_mat_poro()
{
  k_pfs_->Zero();

  // create the parameters for the discretization
  Teuchos::ParameterList fparams;
  // action for elements
  fparams.set<int>("action", FLD::calc_poroscatra_mono_odblock);
  // physical type
  fparams.set<int>("Physical Type", poro_field()->fluid_field()->PhysicalType());

  // other parameters that might be needed by the elements
  fparams.set("delta time", Dt());
  fparams.set("total time", Time());

  const Teuchos::RCP<Core::FE::Discretization>& porofluiddis =
      poro_field()->fluid_field()->discretization();
  porofluiddis->ClearState();

  // set general vector values needed by elements
  porofluiddis->set_state(0, "dispnp", poro_field()->fluid_field()->Dispnp());
  porofluiddis->set_state(0, "gridv", poro_field()->fluid_field()->GridVel());
  porofluiddis->set_state(0, "veln", poro_field()->fluid_field()->Veln());
  porofluiddis->set_state(0, "accnp", poro_field()->fluid_field()->Accnp());
  porofluiddis->set_state(0, "hist", poro_field()->fluid_field()->Hist());

  poro_field()->fluid_field()->discretization()->set_state(
      0, "scaaf", poro_field()->fluid_field()->Scaaf());

  porofluiddis->set_state(0, "velaf", poro_field()->fluid_field()->Velnp());
  porofluiddis->set_state(0, "velnp", poro_field()->fluid_field()->Velnp());

  // build specific assemble strategy for the fluid-mechanical system matrix
  // from the point of view of fluid_field:
  // fluiddofset = 0, structdofset = 1
  Core::FE::AssembleStrategy fluidstrategy(0,  // fluiddofset for row
      2,                                       // scatradofset for column
      k_pfs_,                                  // scatra-mechanical matrix
      Teuchos::null,                           // no other matrix or vectors
      Teuchos::null, Teuchos::null, Teuchos::null);

  // evaluate the fluid-mechanical system matrix on the fluid element
  poro_field()->fluid_field()->discretization()->evaluate_condition(
      fparams, fluidstrategy, "PoroCoupling");
  // poro_field()->fluid_field()->discretization()->Evaluate( fparams, fluidstrategy );

  porofluiddis->ClearState(true);

  //************************************************************************************
  //************************************************************************************

  k_pss_->Zero();


  // create the parameters for the discretization
  Teuchos::ParameterList sparams;

  const std::string action = "struct_poro_calc_scatracoupling";
  sparams.set("action", action);
  // other parameters that might be needed by the elements
  sparams.set("delta time", Dt());
  sparams.set("total time", Time());

  poro_field()->structure_field()->discretization()->ClearState();
  poro_field()->structure_field()->discretization()->set_state(
      0, "displacement", poro_field()->structure_field()->Dispnp());
  poro_field()->structure_field()->discretization()->set_state(
      0, "velocity", poro_field()->structure_field()->Velnp());

  // build specific assemble strategy for mechanical-fluid system matrix
  // from the point of view of structure_field:
  // structdofset = 0, fluiddofset = 1
  Core::FE::AssembleStrategy structuralstrategy(0,  // structdofset for row
      2,                                            // scatradofset for column
      k_pss_,                                       // mechanical-scatra coupling matrix
      Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);

  // evaluate the mechanical-fluid system matrix on the structural element
  poro_field()->structure_field()->discretization()->evaluate_condition(
      sparams, structuralstrategy, "PoroCoupling");
  // structure_field()->discretization()->Evaluate( sparams, structuralstrategy);
  poro_field()->structure_field()->discretization()->ClearState(true);

  return;
}

/*----------------------------------------------------------------------*
 |  Evaluate off diagonal matrix in scatra row                    |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::evaluate_od_block_mat_scatra()
{
  // create the parameters for the discretization
  Teuchos::ParameterList sparams_struct;

  k_sps_->Zero();

  Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
      "action", ScaTra::Action::calc_scatra_mono_odblock_mesh, sparams_struct);
  // other parameters that might be needed by the elements
  sparams_struct.set("delta time", Dt());
  sparams_struct.set("total time", Time());

  ScaTraField()->discretization()->ClearState();
  ScaTraField()->discretization()->set_state(0, "hist", ScaTraField()->Hist());
  ScaTraField()->discretization()->set_state(0, "phinp", ScaTraField()->Phinp());

  // build specific assemble strategy for mechanical-fluid system matrix
  // from the point of view of structure_field:
  // structdofset = 0, fluiddofset = 1
  Core::FE::AssembleStrategy scatrastrategy_struct(0,  // scatradofset for row
      1,                                               // structuredofset for column
      k_sps_,                                          // scatra-structure coupling matrix
      Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);

  // evaluate the mechanical-fluid system matrix on the structural element
  ScaTraField()->discretization()->evaluate_condition(
      sparams_struct, scatrastrategy_struct, "PoroCoupling");
  // structure_field()->discretization()->Evaluate( sparams, structuralstrategy);

  ScaTraField()->discretization()->ClearState();
  // FOUR_C_THROW("stop");
  //************************************************************************************
  //************************************************************************************

  k_spf_->Zero();

  // create the parameters for the discretization
  Teuchos::ParameterList sparams_fluid;

  Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
      "action", ScaTra::Action::calc_scatra_mono_odblock_fluid, sparams_fluid);
  // other parameters that might be needed by the elements
  sparams_fluid.set("delta time", Dt());
  sparams_fluid.set("total time", Time());

  ScaTraField()->discretization()->ClearState();
  ScaTraField()->discretization()->set_state(0, "hist", ScaTraField()->Hist());
  ScaTraField()->discretization()->set_state(0, "phinp", ScaTraField()->Phinp());

  // build specific assemble strategy for mechanical-fluid system matrix
  // from the point of view of structure_field:
  // structdofset = 0, fluiddofset = 1
  Core::FE::AssembleStrategy scatrastrategy_fluid(0,  // scatradofset for row
      2,                                              // fluiddofset for column
      k_spf_,                                         // scatra-fluid coupling matrix
      Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);

  // evaluate the mechanical-fluid system matrix on the structural element
  ScaTraField()->discretization()->evaluate_condition(
      sparams_fluid, scatrastrategy_fluid, "PoroCoupling");
  // structure_field()->discretization()->Evaluate( sparams, structuralstrategy);
  ScaTraField()->discretization()->ClearState();

  return;
}

/*----------------------------------------------------------------------*
 |  check tangent stiffness matrix vie finite differences      vuong 08/13  |
 *----------------------------------------------------------------------*/
void PoroElastScaTra::PoroScatraMono::fd_check()
{
  std::cout << "\n******************finite difference check***************" << std::endl;

  int dof_struct = (poro_field()->structure_field()->discretization()->NumGlobalNodes()) * 3;
  int dof_fluid = (poro_field()->fluid_field()->discretization()->NumGlobalNodes()) * 4;
  int dof_scatra = (ScaTraField()->discretization()->NumGlobalNodes());

  std::cout << "structure field has " << dof_struct << " DOFs" << std::endl;
  std::cout << "fluid field has " << dof_fluid << " DOFs" << std::endl;
  std::cout << "scatra field has " << dof_scatra << " DOFs" << std::endl;

  Teuchos::RCP<Epetra_Vector> iterinc = Teuchos::null;
  iterinc = Core::LinAlg::CreateVector(*dof_row_map(), true);

  const int dofs = iterinc->GlobalLength();
  std::cout << "in total " << dofs << " DOFs" << std::endl;
  const double delta = 1e-8;

  iterinc->PutScalar(0.0);

  iterinc->ReplaceGlobalValue(0, 0, delta);

  Teuchos::RCP<Epetra_CrsMatrix> stiff_approx = Teuchos::null;
  stiff_approx = Core::LinAlg::CreateMatrix(*dof_row_map(), 81);

  Teuchos::RCP<Epetra_Vector> rhs_old = Teuchos::rcp(new Epetra_Vector(*dof_row_map(), true));
  rhs_old->Update(1.0, *rhs_, 0.0);
  Teuchos::RCP<Epetra_Vector> rhs_copy = Teuchos::rcp(new Epetra_Vector(*dof_row_map(), true));

  Teuchos::RCP<Core::LinAlg::SparseMatrix> sparse = systemmatrix_->Merge();
  Teuchos::RCP<Core::LinAlg::SparseMatrix> sparse_copy =
      Teuchos::rcp(new Core::LinAlg::SparseMatrix(sparse->EpetraMatrix(), Core::LinAlg::Copy));

  if (false)
  {
    std::cout << "iterinc_" << std::endl << *iterinc_ << std::endl;
    std::cout << "iterinc" << std::endl << *iterinc << std::endl;
    std::cout << "meshdisp: " << std::endl << *(poro_field()->fluid_field()->Dispnp());
    std::cout << "disp: " << std::endl << *(poro_field()->structure_field()->Dispnp());
    std::cout << "fluid vel" << std::endl << *(poro_field()->fluid_field()->Velnp());
    std::cout << "fluid acc" << std::endl << *(poro_field()->fluid_field()->Accnp());
    std::cout << "gridvel fluid" << std::endl << *(poro_field()->fluid_field()->GridVel());
    std::cout << "gridvel struct" << std::endl << *(poro_field()->structure_field()->Velnp());
  }

  const int zeilennr = -1;
  const int spaltenr = -1;
  for (int i = 0; i < dofs; ++i)
  {
    if (combined_dbc_map()->MyGID(i))
    {
      iterinc->ReplaceGlobalValue(i, 0, 0.0);
    }

    if (i == spaltenr)
      std::cout << "\n******************" << spaltenr + 1 << ". Spalte!!***************"
                << std::endl;

    Evaluate(iterinc);
    setup_rhs();

    rhs_copy->Update(1.0, *rhs_, 0.0);

    iterinc_->PutScalar(0.0);  // Useful? depends on solver and more
    Core::LinAlg::apply_dirichlet_to_system(
        *sparse_copy, *iterinc_, *rhs_copy, *zeros_, *combined_dbc_map());


    if (i == spaltenr)
    {
      std::cout << "rhs_: " << (*rhs_copy)[zeilennr] << std::endl;
      std::cout << "rhs_old: " << (*rhs_old)[zeilennr] << std::endl;
    }

    rhs_copy->Update(-1.0, *rhs_old, 1.0);
    rhs_copy->Scale(-1.0 / delta);

    int* index = &i;
    for (int j = 0; j < dofs; ++j)
    {
      double value = (*rhs_copy)[j];
      stiff_approx->InsertGlobalValues(j, 1, &value, index);

      if ((j == zeilennr) and (i == spaltenr))
      {
        std::cout << "\n******************" << zeilennr + 1 << ". Zeile!!***************"
                  << std::endl;
        std::cout << "iterinc_" << std::endl << *iterinc_ << std::endl;
        std::cout << "iterinc" << std::endl << *iterinc << std::endl;
        std::cout << "meshdisp: " << std::endl << *(poro_field()->fluid_field()->Dispnp());
        std::cout << "meshdisp scatra: " << std::endl
                  << *(ScaTraField()->discretization()->GetState(
                         ScaTraField()->NdsDisp(), "dispnp"));
        std::cout << "disp: " << std::endl << *(poro_field()->structure_field()->Dispnp());
        std::cout << "fluid vel" << std::endl << *(poro_field()->fluid_field()->Velnp());
        std::cout << "scatra vel" << std::endl
                  << *(ScaTraField()->discretization()->GetState(
                         ScaTraField()->NdsVel(), "velocity field"));
        std::cout << "fluid acc" << std::endl << *(poro_field()->fluid_field()->Accnp());
        std::cout << "gridvel fluid" << std::endl << *(poro_field()->fluid_field()->GridVel());
        std::cout << "gridvel struct" << std::endl << *(poro_field()->structure_field()->Velnp());

        std::cout << "stiff_apprx(" << zeilennr << "," << spaltenr << "): " << (*rhs_copy)[zeilennr]
                  << std::endl;

        std::cout << "value(" << zeilennr << "," << spaltenr << "): " << value << std::endl;
        std::cout << "\n******************" << zeilennr + 1 << ". Zeile Ende!!***************"
                  << std::endl;
      }
    }

    if (not combined_dbc_map()->MyGID(i)) iterinc->ReplaceGlobalValue(i, 0, -delta);

    iterinc->ReplaceGlobalValue(i - 1, 0, 0.0);

    if (i != dofs - 1) iterinc->ReplaceGlobalValue(i + 1, 0, delta);

    if (i == spaltenr)
      std::cout << "\n******************" << spaltenr + 1 << ". Spalte Ende!!***************"
                << std::endl;
  }

  Evaluate(iterinc);
  setup_rhs();

  stiff_approx->FillComplete();

  Teuchos::RCP<Core::LinAlg::SparseMatrix> stiff_approx_sparse = Teuchos::null;
  stiff_approx_sparse =
      Teuchos::rcp(new Core::LinAlg::SparseMatrix(stiff_approx, Core::LinAlg::Copy));

  stiff_approx_sparse->Add(*sparse_copy, false, -1.0, 1.0);

  Teuchos::RCP<Epetra_CrsMatrix> sparse_crs = sparse_copy->EpetraMatrix();

  Teuchos::RCP<Epetra_CrsMatrix> error_crs = stiff_approx_sparse->EpetraMatrix();

  error_crs->FillComplete();
  sparse_crs->FillComplete();

  bool success = true;
  double error_max_rel = 0.0;
  double error_max_abs = 0.0;
  for (int i = 0; i < dofs; ++i)
  {
    if (not combined_dbc_map()->MyGID(i))
    {
      for (int j = 0; j < dofs; ++j)
      {
        if (not combined_dbc_map()->MyGID(j))
        {
          double stiff_approx_ij = 0.0;
          double sparse_ij = 0.0;
          double error_ij = 0.0;

          {
            // get error_crs entry ij
            int errornumentries;
            int errorlength = error_crs->NumGlobalEntries(i);
            std::vector<double> errorvalues(errorlength);
            std::vector<int> errorindices(errorlength);
            // int errorextractionstatus =
            error_crs->ExtractGlobalRowCopy(
                i, errorlength, errornumentries, errorvalues.data(), errorindices.data());
            for (int k = 0; k < errorlength; ++k)
            {
              if (errorindices[k] == j)
              {
                error_ij = errorvalues[k];
                break;
              }
              else
                error_ij = 0.0;
            }
          }

          // get sparse_ij entry ij
          {
            int sparsenumentries;
            int sparselength = sparse_crs->NumGlobalEntries(i);
            std::vector<double> sparsevalues(sparselength);
            std::vector<int> sparseindices(sparselength);
            // int sparseextractionstatus =
            sparse_crs->ExtractGlobalRowCopy(
                i, sparselength, sparsenumentries, sparsevalues.data(), sparseindices.data());
            for (int k = 0; k < sparselength; ++k)
            {
              if (sparseindices[k] == j)
              {
                sparse_ij = sparsevalues[k];
                break;
              }
              else
                sparse_ij = 0.0;
            }
          }

          // get stiff_approx entry ij
          {
            int approxnumentries;
            int approxlength = stiff_approx->NumGlobalEntries(i);
            std::vector<double> approxvalues(approxlength);
            std::vector<int> approxindices(approxlength);
            // int approxextractionstatus =
            stiff_approx->ExtractGlobalRowCopy(
                i, approxlength, approxnumentries, approxvalues.data(), approxindices.data());
            for (int k = 0; k < approxlength; ++k)
            {
              if (approxindices[k] == j)
              {
                stiff_approx_ij = approxvalues[k];
                break;
              }
              else
                stiff_approx_ij = 0.0;
            }
          }

          double error = 0.0;
          if (abs(stiff_approx_ij) > 1e-5)
            error = error_ij / (stiff_approx_ij);
          else if (abs(sparse_ij) > 1e-5)
            error = error_ij / (sparse_ij);

          if (abs(error) > abs(error_max_rel)) error_max_rel = abs(error);
          if (abs(error_ij) > abs(error_max_abs)) error_max_abs = abs(error_ij);

          if ((abs(error) > 1e-4))
          {
            if ((abs(error_ij) > 1e-5))
            //  if( (sparse_ij>1e-1) or (stiff_approx_ij>1e-1) )
            {
              std::cout << "finite difference check failed entry (" << i << "," << j
                        << ")! stiff: " << sparse_ij << ", approx: " << stiff_approx_ij
                        << " ,abs. error: " << error_ij << " , rel. error: " << error << std::endl;

              success = false;
            }
          }
        }
      }
    }
  }

  if (success)
  {
    std::cout << "finite difference check successful, max. rel. error: " << error_max_rel
              << " , max. abs. error: " << error_max_abs << std::endl;
    std::cout << "******************finite difference check done***************\n\n" << std::endl;
  }
  else
    FOUR_C_THROW("PoroFDCheck failed");

  return;
}

FOUR_C_NAMESPACE_CLOSE
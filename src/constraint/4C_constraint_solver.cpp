/*----------------------------------------------------------------------*/
/*! \file
\brief Class containing Uzawa algorithm to solve linear system.
\level 2


*----------------------------------------------------------------------*/


#include "4C_constraint_solver.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_linear_solver_method.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_linear_solver_method_parameters.hpp"

#include <Teuchos_ParameterList.hpp>
#include <Teuchos_StandardParameterEntryValidators.hpp>

#include <iostream>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*
 |  ctor (public)                                               tk 11/07|
 *----------------------------------------------------------------------*/
CONSTRAINTS::ConstraintSolver::ConstraintSolver(Teuchos::RCP<Core::FE::Discretization> discr,
    Core::LinAlg::Solver& solver, Teuchos::RCP<Core::LinAlg::MapExtractor> dbcmaps,
    Teuchos::ParameterList params)
    : actdisc_(discr),
      max_iter_(params.get<int>("UZAWAMAXITER", 50)),
      dirichtoggle_(Teuchos::null),
      dbcmaps_(dbcmaps)
{
  setup(discr, solver, dbcmaps, params);
}

/*----------------------------------------------------------------------*
 |  set-up (public)                                             tk 11/07|
 *----------------------------------------------------------------------*/
void CONSTRAINTS::ConstraintSolver::setup(Teuchos::RCP<Core::FE::Discretization> discr,
    Core::LinAlg::Solver& solver, Teuchos::RCP<Core::LinAlg::MapExtractor> dbcmaps,
    Teuchos::ParameterList params)
{
  solver_ = Teuchos::rcp(&solver, false);

  algochoice_ = Teuchos::getIntegralValue<Inpar::Solid::ConSolveAlgo>(params, "UZAWAALGO");

  // different setup for #adapttol_
  isadapttol_ = true;
  isadapttol_ = (params.get<bool>("ADAPTCONV"));

  // simple parameters
  adaptolbetter_ = params.get<double>("ADAPTCONV_BETTER", 0.01);
  iterationparam_ = params.get<double>("UZAWAPARAM", 1);
  minparam_ = iterationparam_ * 1E-3;
  iterationtol_ = params.get<double>("UZAWATOL", 1E-8);


  counter_ = 0;
  return;
}



/*----------------------------------------------------------------------*
|(public)                                                               |
|Solve linear constrained system                                        |
*-----------------------------------------------------------------------*/
void CONSTRAINTS::ConstraintSolver::solve(Teuchos::RCP<Core::LinAlg::SparseMatrix> stiff,
    Teuchos::RCP<Core::LinAlg::SparseMatrix> constr,
    Teuchos::RCP<Core::LinAlg::SparseMatrix> constrT,
    Teuchos::RCP<Core::LinAlg::Vector<double>> dispinc,
    Teuchos::RCP<Core::LinAlg::Vector<double>> lagrinc,
    const Teuchos::RCP<Core::LinAlg::Vector<double>> rhsstand,
    const Teuchos::RCP<Core::LinAlg::Vector<double>> rhsconstr)
{
  switch (algochoice_)
  {
    case Inpar::Solid::consolve_uzawa:
      solve_uzawa(stiff, constr, constrT, dispinc, lagrinc, rhsstand, rhsconstr);
      break;
    case Inpar::Solid::consolve_direct:
      solve_direct(stiff, constr, constrT, dispinc, lagrinc, rhsstand, rhsconstr);
      break;
    case Inpar::Solid::consolve_simple:
      solve_simple(stiff, constr, constrT, dispinc, lagrinc, rhsstand, rhsconstr);
      break;
    default:
      FOUR_C_THROW("Unknown constraint solution technique!");
  }
  return;
}

/*----------------------------------------------------------------------*
|(public)                                                               |
|Solve linear constrained system by iterative Uzawa algorithm           |
*-----------------------------------------------------------------------*/
void CONSTRAINTS::ConstraintSolver::solve_uzawa(Teuchos::RCP<Core::LinAlg::SparseMatrix> stiff,
    Teuchos::RCP<Core::LinAlg::SparseMatrix> constr,
    Teuchos::RCP<Core::LinAlg::SparseMatrix> constrT,
    Teuchos::RCP<Core::LinAlg::Vector<double>> dispinc,
    Teuchos::RCP<Core::LinAlg::Vector<double>> lagrinc,
    const Teuchos::RCP<Core::LinAlg::Vector<double>> rhsstand,
    const Teuchos::RCP<Core::LinAlg::Vector<double>> rhsconstr)
{
  const int myrank = (actdisc_->get_comm().MyPID());
  // For every iteration step an uzawa algorithm is used to solve the linear system.
  // Preparation of uzawa method to solve the linear system.
  double norm_uzawa;
  double norm_uzawa_old;
  double quotient;
  double norm_constr_uzawa;
  int numiter_uzawa = 0;
  // counter used for adaptivity
  const int adaptstep = 2;
  const int minstep = 1;
  int count_paramadapt = 1;

  const double computol = 1E-8;

  Teuchos::RCP<Core::LinAlg::Vector<double>> constrTLagrInc =
      Teuchos::rcp(new Core::LinAlg::Vector<double>(rhsstand->Map()));
  Teuchos::RCP<Core::LinAlg::Vector<double>> constrTDispInc =
      Teuchos::rcp(new Core::LinAlg::Vector<double>(rhsconstr->Map()));
  // Core::LinAlg::SparseMatrix constrT =
  // *(Teuchos::rcp_dynamic_cast<Core::LinAlg::SparseMatrix>(constr));

  // ONLY compatability
  // dirichtoggle_ changed and we need to rebuild associated DBC maps
  if (dirichtoggle_ != Teuchos::null)
    dbcmaps_ = Core::LinAlg::convert_dirichlet_toggle_vector_to_maps(dirichtoggle_);

  Teuchos::RCP<Core::LinAlg::Vector<double>> zeros =
      Teuchos::rcp(new Core::LinAlg::Vector<double>(rhsstand->Map(), true));
  Teuchos::RCP<Core::LinAlg::Vector<double>> dirichzeros = dbcmaps_->extract_cond_vector(*zeros);

  // Compute residual of the uzawa algorithm
  Teuchos::RCP<Core::LinAlg::Vector<double>> fresmcopy =
      Teuchos::rcp(new Core::LinAlg::Vector<double>(*rhsstand));
  Core::LinAlg::Vector<double> uzawa_res(*fresmcopy);
  (*stiff).multiply(false, *dispinc, uzawa_res);
  uzawa_res.Update(1.0, *fresmcopy, -1.0);

  // blank residual DOFs which are on Dirichlet BC
  dbcmaps_->insert_cond_vector(*dirichzeros, uzawa_res);

  uzawa_res.Norm2(&norm_uzawa);
  Core::LinAlg::Vector<double> constr_res(lagrinc->Map());

  constr_res.Update(1.0, *(rhsconstr), 0.0);
  constr_res.Norm2(&norm_constr_uzawa);
  quotient = 1;
  // Solve one iteration step with augmented lagrange
  // Since we calculate displacement norm as well, at least one step has to be taken
  while (((norm_uzawa > iterationtol_ or norm_constr_uzawa > iterationtol_) and
             numiter_uzawa < max_iter_) or
         numiter_uzawa < minstep)
  {
    // solve for disi
    // Solve K . IncD = -R  ===>  IncD_{n+1}
    Core::LinAlg::SolverParams solver_params;
    if (isadapttol_ && counter_ && numiter_uzawa)
    {
      solver_params.nonlin_tolerance = tolres_ / 10.0;
      solver_params.nonlin_residual = norm_uzawa;
      solver_params.lin_tol_better = adaptolbetter_;
    }

    solver_params.refactor = true;
    solver_params.reset = numiter_uzawa == 0 && counter_ == 0;
    solver_->solve(stiff->epetra_operator(), dispinc, fresmcopy, solver_params);
    solver_->reset_tolerance();

    // compute Lagrange multiplier increment
    constrTDispInc->PutScalar(0.0);
    constrT->multiply(true, *dispinc, *constrTDispInc);
    lagrinc->Update(iterationparam_, *constrTDispInc, iterationparam_, *rhsconstr, 1.0);

    // Compute residual of the uzawa algorithm
    constr->multiply(false, *lagrinc, *constrTLagrInc);

    fresmcopy->Update(-1.0, *constrTLagrInc, 1.0, *rhsstand, 0.0);
    Core::LinAlg::Vector<double> uzawa_res(*fresmcopy);
    (*stiff).multiply(false, *dispinc, uzawa_res);
    uzawa_res.Update(1.0, *fresmcopy, -1.0);

    // blank residual DOFs which are on Dirichlet BC
    dbcmaps_->insert_cond_vector(*dirichzeros, uzawa_res);
    norm_uzawa_old = norm_uzawa;
    uzawa_res.Norm2(&norm_uzawa);
    Core::LinAlg::Vector<double> constr_res(lagrinc->Map());

    constr_res.Update(1.0, *constrTDispInc, 1.0, *rhsconstr, 0.0);
    constr_res.Norm2(&norm_constr_uzawa);
    //-------------Adapt Uzawa parameter--------------
    // For a constant parameter the quotient of two successive residual norms
    // stays nearly constant during the computation. So this quotient seems to be a good
    // measure for the parameter choice
    // Adaptivity only takes place every second step. Otherwise the quotient is not significant.
    if (count_paramadapt >= adaptstep)
    {
      double quotient_new = norm_uzawa / norm_uzawa_old;
      // In case of divergence the parameter must be too high
      if (quotient_new > (1. + computol))
      {
        if (iterationparam_ > 2. * minparam_) iterationparam_ = iterationparam_ / 2.;
        quotient = 1;
      }
      else
      {
        // In case the newly computed quotient is better than the one obtained from the
        // previous parameter, the parameter is increased by a factor (1+quotient_new)
        if (quotient >= quotient_new)
        {
          iterationparam_ = iterationparam_ * (1. + quotient_new);
          quotient = quotient_new;
        }
        // In case the newly computed quotient is worse than the one obtained from the
        // previous parameter, the parameter is decreased by a factor 1/(1+quotient_new)
        else
        {
          if (iterationparam_ > 2. * minparam_)
            iterationparam_ = iterationparam_ / (1. + quotient_new);
          quotient = quotient_new;
        }
      }

      if (iterationparam_ <= minparam_)
      {
        if (!myrank)
          std::cout << "leaving uzawa loop since Uzawa parameter is too low" << std::endl;
        iterationparam_ *= 1E2;
        break;
      }
      count_paramadapt = 0;
    }
    count_paramadapt++;
    numiter_uzawa++;
  }  // Uzawa loop

  if (!myrank)
  {
    std::cout << "Uzawa steps " << numiter_uzawa << ", Uzawa parameter: " << iterationparam_;
    std::cout << ", residual norms for linear system: " << norm_constr_uzawa << " and "
              << norm_uzawa << std::endl;
  }
  counter_++;
  return;
}

/*----------------------------------------------------------------------*
|(public)                                                               |
|Solve linear constrained system by iterative Uzawa algorithm           |
*-----------------------------------------------------------------------*/
void CONSTRAINTS::ConstraintSolver::solve_direct(Teuchos::RCP<Core::LinAlg::SparseMatrix> stiff,
    Teuchos::RCP<Core::LinAlg::SparseMatrix> constr,
    Teuchos::RCP<Core::LinAlg::SparseMatrix> constrT,
    Teuchos::RCP<Core::LinAlg::Vector<double>> dispinc,
    Teuchos::RCP<Core::LinAlg::Vector<double>> lagrinc,
    const Teuchos::RCP<Core::LinAlg::Vector<double>> rhsstand,
    const Teuchos::RCP<Core::LinAlg::Vector<double>> rhsconstr)
{
  // define maps of standard dofs and additional lagrange multipliers
  Teuchos::RCP<Epetra_Map> standrowmap = Teuchos::rcp(new Epetra_Map(stiff->row_map()));
  Teuchos::RCP<Epetra_Map> conrowmap = Teuchos::rcp(new Epetra_Map(constr->domain_map()));
  // merge maps to one large map
  Teuchos::RCP<Epetra_Map> mergedmap = Core::LinAlg::merge_map(standrowmap, conrowmap, false);
  // define MapExtractor
  Core::LinAlg::MapExtractor mapext(*mergedmap, standrowmap, conrowmap);

  // initialize large Sparse Matrix and Core::LinAlg::Vectors
  Teuchos::RCP<Core::LinAlg::SparseMatrix> mergedmatrix =
      Teuchos::rcp(new Core::LinAlg::SparseMatrix(*mergedmap, 81));
  Teuchos::RCP<Core::LinAlg::Vector<double>> mergedrhs =
      Teuchos::rcp(new Core::LinAlg::Vector<double>(*mergedmap));
  Teuchos::RCP<Core::LinAlg::Vector<double>> mergedsol =
      Teuchos::rcp(new Core::LinAlg::Vector<double>(*mergedmap));
  // ONLY compatability
  // dirichtoggle_ changed and we need to rebuild associated DBC maps
  if (dirichtoggle_ != Teuchos::null)
    dbcmaps_ = Core::LinAlg::convert_dirichlet_toggle_vector_to_maps(dirichtoggle_);
  // fill merged matrix using Add
  mergedmatrix->add(*stiff, false, 1.0, 1.0);
  mergedmatrix->add(*constr, false, 1.0, 1.0);
  mergedmatrix->add(*constrT, true, 1.0, 1.0);
  mergedmatrix->complete(*mergedmap, *mergedmap);
  // fill merged vectors using Export
  Core::LinAlg::export_to(*rhsconstr, *mergedrhs);
  mergedrhs->Scale(-1.0);
  Core::LinAlg::export_to(*rhsstand, *mergedrhs);

  // solve
  Core::LinAlg::SolverParams solver_params;
  solver_params.refactor = true;
  solver_params.reset = counter_ == 0;
  solver_->solve(mergedmatrix->epetra_operator(), mergedsol, mergedrhs, solver_params);
  solver_->reset_tolerance();
  // store results in smaller vectors
  mapext.extract_cond_vector(*mergedsol, *dispinc);
  mapext.extract_other_vector(*mergedsol, *lagrinc);

  counter_++;
  return;
}

void CONSTRAINTS::ConstraintSolver::solve_simple(Teuchos::RCP<Core::LinAlg::SparseMatrix> stiff,
    Teuchos::RCP<Core::LinAlg::SparseMatrix> constr,
    Teuchos::RCP<Core::LinAlg::SparseMatrix> constrT,
    Teuchos::RCP<Core::LinAlg::Vector<double>> dispinc,
    Teuchos::RCP<Core::LinAlg::Vector<double>> lagrinc,
    const Teuchos::RCP<Core::LinAlg::Vector<double>> rhsstand,
    const Teuchos::RCP<Core::LinAlg::Vector<double>> rhsconstr)
{
  // row maps (assumed to equal to range map) and extractor
  Teuchos::RCP<Epetra_Map> standrowmap = Teuchos::rcp(new Epetra_Map(stiff->row_map()));
  Teuchos::RCP<Epetra_Map> conrowmap = Teuchos::rcp(new Epetra_Map(constr->domain_map()));
  Teuchos::RCP<Epetra_Map> mergedrowmap = Core::LinAlg::merge_map(standrowmap, conrowmap, false);
  Core::LinAlg::MapExtractor rowmapext(*mergedrowmap, conrowmap, standrowmap);

  // domain maps and extractor
  Teuchos::RCP<Epetra_Map> standdommap = Teuchos::rcp(new Epetra_Map(stiff->domain_map()));
  Teuchos::RCP<Epetra_Map> condommap = Teuchos::rcp(new Epetra_Map(constr->domain_map()));
  Teuchos::RCP<Epetra_Map> mergeddommap = Core::LinAlg::merge_map(standdommap, condommap, false);
  Core::LinAlg::MapExtractor dommapext(*mergeddommap, condommap, standdommap);

  // cast constraint operators to matrices and save transpose of constraint matrix
  Core::LinAlg::SparseMatrix constrTrans(*conrowmap, 81, false, true);
  constrTrans.add(*constrT, true, 1.0, 0.0);
  constrTrans.complete(constrT->range_map(), constrT->domain_map());

  // ONLY compatability
  // dirichtoggle_ changed and we need to rebuild associated DBC maps
  if (dirichtoggle_ != Teuchos::null)
    dbcmaps_ = Core::LinAlg::convert_dirichlet_toggle_vector_to_maps(dirichtoggle_);

  // stuff needed for Dirichlet BCs
  Teuchos::RCP<Core::LinAlg::Vector<double>> zeros =
      Teuchos::rcp(new Core::LinAlg::Vector<double>(rhsstand->Map(), true));
  Teuchos::RCP<Core::LinAlg::Vector<double>> dirichzeros = dbcmaps_->extract_cond_vector(*zeros);
  Teuchos::RCP<Core::LinAlg::Vector<double>> rhscopy =
      Teuchos::rcp(new Core::LinAlg::Vector<double>(*rhsstand));

  Teuchos::ParameterList sfparams = solver_->params();
  const Teuchos::ParameterList& mcparams = Global::Problem::instance()->contact_dynamic_params();
  const int linsolvernumber = mcparams.get<int>("LINEAR_SOLVER");

  const Teuchos::ParameterList& solverparams =
      Global::Problem::instance()->solver_params(linsolvernumber);

  solver_->params() = Core::LinAlg::Solver::translate_solver_parameters(solverparams,
      Global::Problem::instance()->solver_params_callback(),
      Teuchos::getIntegralValue<Core::IO::Verbositylevel>(
          Global::Problem::instance()->io_params(), "VERBOSITY"));

  const auto prectype =
      Teuchos::getIntegralValue<Core::LinearSolver::PreconditionerType>(solverparams, "AZPREC");

  switch (prectype)
  {
    case Core::LinearSolver::PreconditionerType::block_teko:
    {
      Core::LinearSolver::Parameters::compute_solver_parameters(
          *actdisc_, solver_->params().sublist("Inverse1"));
      break;
    }
    default:
      FOUR_C_THROW("Expected a block preconditioner for saddle point systems.");
  }

  // build block matrix for SIMPLE
  Teuchos::RCP<Core::LinAlg::BlockSparseMatrix<Core::LinAlg::DefaultBlockMatrixStrategy>> mat =
      Teuchos::rcp(new Core::LinAlg::BlockSparseMatrix<Core::LinAlg::DefaultBlockMatrixStrategy>(
          dommapext, rowmapext, 81, false, false));
  mat->assign(0, 0, Core::LinAlg::View, *stiff);
  mat->assign(0, 1, Core::LinAlg::View, *constr);
  mat->assign(1, 0, Core::LinAlg::View, constrTrans);
  mat->complete();

  // merged rhs using Export
  Teuchos::RCP<Core::LinAlg::Vector<double>> mergedrhs =
      Teuchos::rcp(new Core::LinAlg::Vector<double>(*mergedrowmap));
  Core::LinAlg::export_to(*rhsconstr, *mergedrhs);
  mergedrhs->Scale(-1.0);
  Core::LinAlg::export_to(*rhscopy, *mergedrhs);

  // solution vector
  Teuchos::RCP<Core::LinAlg::Vector<double>> mergedsol =
      Teuchos::rcp(new Core::LinAlg::Vector<double>(*mergedrowmap));

  // solve
  Core::LinAlg::SolverParams solver_params;
  solver_params.refactor = true;
  solver_params.reset = counter_ == 0;
  solver_->solve(mat->epetra_operator(), mergedsol, mergedrhs, solver_params);
  solver_->reset_tolerance();
  solver_->params() = sfparams;  // store back original parameter list

  // store results in smaller vectors
  rowmapext.extract_cond_vector(*mergedsol, *lagrinc);
  rowmapext.extract_other_vector(*mergedsol, *dispinc);

  counter_++;
  return;
}

FOUR_C_NAMESPACE_CLOSE

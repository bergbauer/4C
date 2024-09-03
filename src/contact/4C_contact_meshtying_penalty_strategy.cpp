/*----------------------------------------------------------------------*/
/*! \file
\brief strategy handling mesh tying problems with penalty formulation

\level 1

*/
/*----------------------------------------------------------------------*/

#include "4C_contact_meshtying_penalty_strategy.hpp"

#include "4C_inpar_contact.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_linalg_utils_sparse_algebra_math.hpp"
#include "4C_linear_solver_method.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_mortar_interface.hpp"
#include "4C_mortar_utils.hpp"
#include "4C_utils_parameter_list.hpp"

#include <Teuchos_Time.hpp>
#include <Teuchos_TimeMonitor.hpp>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*
 | ctor (public)                                              popp 05/09|
 *----------------------------------------------------------------------*/
CONTACT::MtPenaltyStrategy::MtPenaltyStrategy(const Epetra_Map* dof_row_map,
    const Epetra_Map* NodeRowMap, Teuchos::ParameterList params,
    std::vector<Teuchos::RCP<Mortar::Interface>> interface, const int spatialDim,
    const Teuchos::RCP<const Epetra_Comm>& comm, const double alphaf, const int maxdof)
    : MtAbstractStrategy(
          dof_row_map, NodeRowMap, params, interface, spatialDim, comm, alphaf, maxdof)
{
  // initialize constraint norm and initial penalty
  constrnorm_ = 0.0;
  initialpenalty_ = MtPenaltyStrategy::params().get<double>("PENALTYPARAM");
}

/*----------------------------------------------------------------------*
 |  do mortar coupling in reference configuration             popp 12/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtPenaltyStrategy::mortar_coupling(const Teuchos::RCP<const Epetra_Vector>& dis)
{
  TEUCHOS_FUNC_TIME_MONITOR("CONTACT::MtPenaltyStrategy::mortar_coupling");

  // print message
  if (get_comm().MyPID() == 0)
  {
    std::cout << "Performing mortar coupling...............";
    fflush(stdout);
  }

  // time measurement
  get_comm().Barrier();
  const double t_start = Teuchos::Time::wallTime();

  // refer call to parent class
  MtAbstractStrategy::mortar_coupling(dis);

  //----------------------------------------------------------------------
  // CHECK IF WE NEED TRANSFORMATION MATRICES FOR SLAVE DISPLACEMENT DOFS
  //----------------------------------------------------------------------
  // Concretely, we apply the following transformations:
  // D         ---->   D * T^(-1)
  // D^(-1)    ---->   T * D^(-1)
  // These modifications are applied once right here, thus the
  // following code (evaluate_meshtying) remains unchanged.
  //----------------------------------------------------------------------
  if (dualquadslavetrafo())
  {
    // type of LM interpolation for quadratic elements
    Inpar::Mortar::LagMultQuad lagmultquad =
        Core::UTILS::integral_value<Inpar::Mortar::LagMultQuad>(params(), "LM_QUAD");

    if (lagmultquad != Inpar::Mortar::lagmult_lin)
    {
      // modify dmatrix_
      Teuchos::RCP<Core::LinAlg::SparseMatrix> temp1 =
          Core::LinAlg::matrix_multiply(*dmatrix_, false, *invtrafo_, false, false, false, true);
      dmatrix_ = temp1;
    }
    else
      FOUR_C_THROW(
          "Locally linear LM interpolation is not yet implemented for meshtying with penalty "
          "strategy!");
  }

  // build mortar matrix products
  mtm_ = Core::LinAlg::matrix_multiply(*mmatrix_, true, *mmatrix_, false, false, false, true);
  mtd_ = Core::LinAlg::matrix_multiply(*mmatrix_, true, *dmatrix_, false, false, false, true);
  dtm_ = Core::LinAlg::matrix_multiply(*dmatrix_, true, *mmatrix_, false, false, false, true);
  dtd_ = Core::LinAlg::matrix_multiply(*dmatrix_, true, *dmatrix_, false, false, false, true);

  // transform rows of mortar matrix products to parallel distribution
  // of the global problem (stored in the "p"-version of dof maps)
  if (par_redist())
  {
    mtm_ = Mortar::matrix_row_transform(mtm_, pgmdofrowmap_);
    mtd_ = Mortar::matrix_row_transform(mtd_, pgmdofrowmap_);
    dtm_ = Mortar::matrix_row_transform(dtm_, pgsdofrowmap_);
    dtd_ = Mortar::matrix_row_transform(dtd_, pgsdofrowmap_);
  }

  // full stiffness matrix
  stiff_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*problem_dofs(), 100, false, true));
  double pp = params().get<double>("PENALTYPARAM");

  // add penalty meshtying stiffness terms
  stiff_->add(*mtm_, false, pp, 1.0);
  stiff_->add(*mtd_, false, -pp, 1.0);
  stiff_->add(*dtm_, false, -pp, 1.0);
  stiff_->add(*dtd_, false, pp, 1.0);
  stiff_->complete();

  // time measurement
  get_comm().Barrier();
  const double t_end = Teuchos::Time::wallTime() - t_start;
  if (get_comm().MyPID() == 0)
    std::cout << "in...." << std::scientific << std::setprecision(6) << t_end << " secs"
              << std::endl;

  return;
}

/*----------------------------------------------------------------------*
 |  mesh initialization for rotational invariance              popp 12/09|
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> CONTACT::MtPenaltyStrategy::mesh_initialization()
{
  TEUCHOS_FUNC_TIME_MONITOR("CONTACT::MtPenaltyStrategy::mesh_initialization");

  // get out of here is NTS algorithm is activated
  if (Core::UTILS::integral_value<Inpar::Mortar::AlgorithmType>(params(), "ALGORITHM") ==
      Inpar::Mortar::algorithm_nts)
    return Teuchos::null;

  // print message
  if (get_comm().MyPID() == 0)
  {
    std::cout << "Performing mesh initialization..........." << std::endl;
  }

  // time measurement
  get_comm().Barrier();
  const double t_start = Teuchos::Time::wallTime();

  //**********************************************************************
  // (1) get master positions on global level
  //**********************************************************************
  // fill Xmaster first
  Teuchos::RCP<Epetra_Vector> Xmaster = Core::LinAlg::create_vector(*gmdofrowmap_, true);
  assemble_coords("master", true, Xmaster);

  //**********************************************************************
  // (2) solve for modified slave positions on global level
  //**********************************************************************
  // create linear problem
  Teuchos::RCP<Epetra_Vector> Xslavemod = Core::LinAlg::create_vector(*gsdofrowmap_, true);
  Teuchos::RCP<Epetra_Vector> rhs = Core::LinAlg::create_vector(*gsdofrowmap_, true);
  mmatrix_->multiply(false, *Xmaster, *rhs);

  // solve with default solver
  Teuchos::ParameterList solvparams;
  Core::UTILS::add_enum_class_to_parameter_list<Core::LinearSolver::SolverType>(
      "SOLVER", Core::LinearSolver::SolverType::umfpack, solvparams);
  Core::LinAlg::Solver solver(solvparams, get_comm(), nullptr, Core::IO::Verbositylevel::standard);

  Core::LinAlg::SolverParams solver_params;
  solver_params.refactor = true;
  solver.solve(dmatrix_->epetra_operator(), Xslavemod, rhs, solver_params);

  //**********************************************************************
  // (3) perform mesh initialization node by node
  //**********************************************************************
  // this can be done in the AbstractStrategy now
  MtAbstractStrategy::mesh_initialization(Xslavemod);

  // time measurement
  get_comm().Barrier();
  const double t_end = Teuchos::Time::wallTime() - t_start;
  if (get_comm().MyPID() == 0)
  {
    std::cout << "in...." << std::scientific << std::setprecision(6) << t_end << " secs"
              << std::endl;
  }

  // return xslavemod for global problem
  return Xslavemod;
}

/*----------------------------------------------------------------------*
 | evaluate meshtying and create linear system                popp 06/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtPenaltyStrategy::evaluate_meshtying(
    Teuchos::RCP<Core::LinAlg::SparseOperator>& kteff, Teuchos::RCP<Epetra_Vector>& feff,
    Teuchos::RCP<Epetra_Vector> dis)
{
  // since we will modify the graph of kteff by adding additional
  // meshtyong stiffness entries, we have to uncomplete it
  kteff->un_complete();

  /**********************************************************************/
  /* Global setup of kteff, feff (including meshtying)                  */
  /**********************************************************************/
  double pp = params().get<double>("PENALTYPARAM");

  // add penalty meshtying stiffness terms
  kteff->add(*mtm_, false, pp, 1.0);
  kteff->add(*mtd_, false, -pp, 1.0);
  kteff->add(*dtm_, false, -pp, 1.0);
  kteff->add(*dtd_, false, pp, 1.0);

  //***************************************************************************
  // build constraint vector
  //***************************************************************************
  // Since we enforce the meshtying constraint for the displacements u,
  // and not for the configurations x (which would also be possible in theory),
  // we avoid artificial initial stresses (+), but we might not guarantee
  // exact rotational invariance (-). However, since we always apply the
  // so-called mesh initialization procedure, we can then also guarantee
  // exact rotational invariance (+).
  //***************************************************************************
  Teuchos::RCP<Epetra_Vector> tempvec1 = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  Teuchos::RCP<Epetra_Vector> tempvec2 = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  Core::LinAlg::export_to(*dis, *tempvec1);
  dmatrix_->multiply(false, *tempvec1, *tempvec2);
  g_->Update(-1.0, *tempvec2, 0.0);

  Teuchos::RCP<Epetra_Vector> tempvec3 = Teuchos::rcp(new Epetra_Vector(*gmdofrowmap_));
  Teuchos::RCP<Epetra_Vector> tempvec4 = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  Core::LinAlg::export_to(*dis, *tempvec3);
  mmatrix_->multiply(false, *tempvec3, *tempvec4);
  g_->Update(1.0, *tempvec4, 1.0);

  // update LM vector
  // (in the pure penalty case, zuzawa is zero)
  z_->Update(1.0, *zuzawa_, 0.0);
  z_->Update(-pp, *g_, 1.0);

  // store updated LM into nodes
  store_nodal_quantities(Mortar::StrategyBase::lmupdate);

  // add penalty meshtying force terms
  Teuchos::RCP<Epetra_Vector> fm = Teuchos::rcp(new Epetra_Vector(*gmdofrowmap_));
  mmatrix_->multiply(true, *z_, *fm);
  Teuchos::RCP<Epetra_Vector> fmexp = Teuchos::rcp(new Epetra_Vector(*problem_dofs()));
  Core::LinAlg::export_to(*fm, *fmexp);
  feff->Update(1.0, *fmexp, 1.0);

  Teuchos::RCP<Epetra_Vector> fs = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  dmatrix_->multiply(true, *z_, *fs);
  Teuchos::RCP<Epetra_Vector> fsexp = Teuchos::rcp(new Epetra_Vector(*problem_dofs()));
  Core::LinAlg::export_to(*fs, *fsexp);
  feff->Update(-1.0, *fsexp, 1.0);

  // add old contact forces (t_n)
  Teuchos::RCP<Epetra_Vector> fsold = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  dmatrix_->multiply(true, *zold_, *fsold);
  Teuchos::RCP<Epetra_Vector> fsoldexp = Teuchos::rcp(new Epetra_Vector(*problem_dofs()));
  Core::LinAlg::export_to(*fsold, *fsoldexp);
  feff->Update(alphaf_, *fsoldexp, 1.0);

  Teuchos::RCP<Epetra_Vector> fmold = Teuchos::rcp(new Epetra_Vector(*gmdofrowmap_));
  mmatrix_->multiply(true, *zold_, *fmold);
  Teuchos::RCP<Epetra_Vector> fmoldexp = Teuchos::rcp(new Epetra_Vector(*problem_dofs()));
  Core::LinAlg::export_to(*fmold, *fmoldexp);
  feff->Update(-alphaf_, *fmoldexp, 1.0);

  return;
}

/*----------------------------------------------------------------------*
 | intialize Uzawa step 2,3...                                popp 12/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtPenaltyStrategy::initialize_uzawa(
    Teuchos::RCP<Core::LinAlg::SparseOperator>& kteff, Teuchos::RCP<Epetra_Vector>& feff)
{
  // remove penalty meshtying force terms
  Teuchos::RCP<Epetra_Vector> fm = Teuchos::rcp(new Epetra_Vector(*gmdofrowmap_));
  mmatrix_->multiply(true, *z_, *fm);
  Teuchos::RCP<Epetra_Vector> fmexp = Teuchos::rcp(new Epetra_Vector(*problem_dofs()));
  Core::LinAlg::export_to(*fm, *fmexp);
  feff->Update(-1.0, *fmexp, 1.0);

  Teuchos::RCP<Epetra_Vector> fs = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  dmatrix_->multiply(false, *z_, *fs);
  Teuchos::RCP<Epetra_Vector> fsexp = Teuchos::rcp(new Epetra_Vector(*problem_dofs()));
  Core::LinAlg::export_to(*fs, *fsexp);
  feff->Update(1.0, *fsexp, 1.0);

  // update LM vector
  double pp = params().get<double>("PENALTYPARAM");
  z_->Update(1.0, *zuzawa_, 0.0);
  z_->Update(-pp, *g_, 1.0);

  // add penalty meshtying force terms
  Teuchos::RCP<Epetra_Vector> fmnew = Teuchos::rcp(new Epetra_Vector(*gmdofrowmap_));
  mmatrix_->multiply(true, *z_, *fmnew);
  Teuchos::RCP<Epetra_Vector> fmexpnew = Teuchos::rcp(new Epetra_Vector(*problem_dofs()));
  Core::LinAlg::export_to(*fmnew, *fmexpnew);
  feff->Update(1.0, *fmexpnew, 1.0);

  Teuchos::RCP<Epetra_Vector> fsnew = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  dmatrix_->multiply(false, *z_, *fsnew);
  Teuchos::RCP<Epetra_Vector> fsexpnew = Teuchos::rcp(new Epetra_Vector(*problem_dofs()));
  Core::LinAlg::export_to(*fsnew, *fsexpnew);
  feff->Update(-1.0, *fsexpnew, 1.0);

  return;
}

/*----------------------------------------------------------------------*
 | reset penalty parameter to intial value                    popp 08/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtPenaltyStrategy::reset_penalty()
{
  // reset penalty parameter in strategy
  params().set<double>("PENALTYPARAM", initial_penalty());


  // reset penalty parameter in all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    interface_[i]->interface_params().set<double>("PENALTYPARAM", initial_penalty());
  }

  return;
}

/*----------------------------------------------------------------------*
 | modify penalty parameter to intial value                    mhv 03/16|
 *----------------------------------------------------------------------*/
void CONTACT::MtPenaltyStrategy::modify_penalty()
{
  // generate random number between 0.95 and 1.05
  double randnum = ((double)rand() / (double)RAND_MAX) * 0.1 + 0.95;
  double pennew = randnum * initial_penalty();

  // modify penalty parameter in strategy
  params().set<double>("PENALTYPARAM", pennew);

  // modify penalty parameter in all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    interface_[i]->interface_params().set<double>("PENALTYPARAM", pennew);
  }

  return;
}

/*----------------------------------------------------------------------*
 | evaluate L2-norm of active constraints                     popp 08/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtPenaltyStrategy::update_constraint_norm(int uzawaiter)
{
  // initialize parameters
  double cnorm = 0.0;
  bool updatepenalty = false;
  double ppcurr = params().get<double>("PENALTYPARAM");

  // compute constraint norm
  g_->Norm2(&cnorm);

  //********************************************************************
  // adaptive update of penalty parameter
  // (only for Uzawa Augmented Lagrange strategy)
  //********************************************************************
  Inpar::CONTACT::SolvingStrategy soltype =
      Core::UTILS::integral_value<Inpar::CONTACT::SolvingStrategy>(params(), "STRATEGY");

  if (soltype == Inpar::CONTACT::solution_uzawa)
  {
    // check convergence of cnorm and update penalty parameter
    // only do this for second, third, ... Uzawa iteration
    // cf. Wriggers, Computational Contact Mechanics, 2nd edition (2006), p. 340
    if ((uzawaiter >= 2) && (cnorm > 0.25 * constraint_norm()))
    {
      updatepenalty = true;

      // update penalty parameter in strategy
      params().set<double>("PENALTYPARAM", 10 * ppcurr);

      // update penalty parameter in all interfaces
      for (int i = 0; i < (int)interface_.size(); ++i)
      {
        double ippcurr = interface_[i]->interface_params().get<double>("PENALTYPARAM");
        if (ippcurr != ppcurr) FOUR_C_THROW("Something wrong with penalty parameter");
        interface_[i]->interface_params().set<double>("PENALTYPARAM", 10 * ippcurr);
      }
    }
  }
  //********************************************************************

  // update constraint norm
  constrnorm_ = cnorm;

  // output to screen
  if (get_comm().MyPID() == 0)
  {
    std::cout << "********************************************\n";
    std::cout << "Constraint Norm: " << cnorm << "\n";
    if (updatepenalty)
      std::cout << "Updated penalty parameter: " << ppcurr << " -> "
                << params().get<double>("PENALTYPARAM") << "\n";
    std::cout << "********************************************\n";
  }
  return;
}

/*----------------------------------------------------------------------*
 | store Lagrange multipliers for next Uzawa step             popp 08/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtPenaltyStrategy::update_uzawa_augmented_lagrange()
{
  // store current LM into Uzawa LM
  // (note that this is also done after the last Uzawa step of one
  // time step and thus also gives the guess for the initial
  // Lagrange multiplier lambda_0 of the next time step)
  zuzawa_ = Teuchos::rcp(new Epetra_Vector(*z_));
  store_nodal_quantities(Mortar::StrategyBase::lmuzawa);

  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool CONTACT::MtPenaltyStrategy::evaluate_force(const Teuchos::RCP<const Epetra_Vector> dis)
{
  if (force_.is_null()) force_ = Teuchos::rcp(new Epetra_Vector(*gdisprowmap_));
  if (stiff_->multiply(false, *dis, *force_)) FOUR_C_THROW("multiply failed");

  return true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool CONTACT::MtPenaltyStrategy::evaluate_stiff(const Teuchos::RCP<const Epetra_Vector> dis)
{
  return true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool CONTACT::MtPenaltyStrategy::evaluate_force_stiff(const Teuchos::RCP<const Epetra_Vector> dis)
{
  bool successForce = evaluate_force(dis);
  bool successStiff = evaluate_stiff(dis);

  return (successForce && successStiff);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> CONTACT::MtPenaltyStrategy::get_rhs_block_ptr(
    const enum CONTACT::VecBlockType& bt) const
{
  Teuchos::RCP<Epetra_Vector> vec_ptr = Teuchos::null;
  switch (bt)
  {
    case CONTACT::VecBlockType::displ:
    {
      if (force_.is_null()) FOUR_C_THROW("force vector not available");
      vec_ptr = force_;
      break;
    }
    default:
    {
      FOUR_C_THROW("Unknown Solid::VecBlockType!");
      break;
    }
  }

  return vec_ptr;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<Core::LinAlg::SparseMatrix> CONTACT::MtPenaltyStrategy::get_matrix_block_ptr(
    const enum CONTACT::MatBlockType& bt) const
{
  Teuchos::RCP<Core::LinAlg::SparseMatrix> mat_ptr = Teuchos::null;
  switch (bt)
  {
    case CONTACT::MatBlockType::displ_displ:
      if (stiff_.is_null()) FOUR_C_THROW("stiffness not available");
      mat_ptr = stiff_;
      break;
    default:
      FOUR_C_THROW("Unknown Solid::MatBlockType!");
      break;
  }
  return mat_ptr;
}

FOUR_C_NAMESPACE_CLOSE

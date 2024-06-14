/*---------------------------------------------------------------------*/
/*! \file

\brief Evaluation and assembly of all constraint terms


\level 3

*/
/*---------------------------------------------------------------------*/

#include "4C_structure_new_model_evaluator_lagpenconstraint.hpp"

#include "4C_constraint_lagpenconstraint_noxinterface.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_io.hpp"
#include "4C_linalg_sparsematrix.hpp"
#include "4C_linalg_sparseoperator.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_structure_new_timint_base.hpp"
#include "4C_structure_new_utils.hpp"
#include "4C_utils_exceptions.hpp"

#include <Epetra_Vector.h>
#include <Teuchos_ParameterList.hpp>

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
STR::MODELEVALUATOR::LagPenConstraint::LagPenConstraint()
    : disnp_ptr_(Teuchos::null),
      stiff_constr_ptr_(Teuchos::null),
      fstrconstr_np_ptr_(Teuchos::null),
      noxinterface_ptr_(Teuchos::null),
      noxinterface_prec_ptr_(Teuchos::null)
{
  // empty
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::Setup()
{
  check_init();

  // build the NOX::Nln::CONSTRAINT::Interface::Required object
  noxinterface_ptr_ = Teuchos::rcp(new LAGPENCONSTRAINT::NoxInterface);
  noxinterface_ptr_->Init(global_state_ptr());
  noxinterface_ptr_->Setup();

  // build the NOX::Nln::CONSTRAINT::Interface::Preconditioner object
  noxinterface_prec_ptr_ = Teuchos::rcp(new LAGPENCONSTRAINT::NoxInterfacePrec());
  noxinterface_prec_ptr_->Init(global_state_ptr());
  noxinterface_prec_ptr_->Setup();

  Teuchos::RCP<Core::FE::Discretization> dis = discret_ptr();

  // setup the displacement pointer
  disnp_ptr_ = global_state().get_dis_np();

  // contributions of constraints to structural rhs and stiffness
  fstrconstr_np_ptr_ = Teuchos::rcp(new Epetra_Vector(*global_state().dof_row_map_view()));
  stiff_constr_ptr_ = Teuchos::rcp(
      new Core::LinAlg::SparseMatrix(*global_state().dof_row_map_view(), 81, true, true));

  // ToDo: we do not want to hand in the structural dynamics parameter list
  // to the manager in the future! -> get rid of it as soon as old
  // time-integration dies ...
  // initialize constraint manager
  constrman_ = Teuchos::rcp(new CONSTRAINTS::ConstrManager());
  constrman_->Init(dis, Global::Problem::Instance()->structural_dynamic_params());
  constrman_->Setup(disnp_ptr_, Global::Problem::Instance()->structural_dynamic_params());

  // set flag
  issetup_ = true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::Reset(const Epetra_Vector& x)
{
  check_init_setup();

  // update the structural displacement vector
  disnp_ptr_ = global_state().get_dis_np();

  fstrconstr_np_ptr_->PutScalar(0.0);
  stiff_constr_ptr_->Zero();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::LagPenConstraint::evaluate_force()
{
  check_init_setup();

  double time_np = global_state().get_time_np();
  Teuchos::ParameterList pcon;  // empty parameter list
  Teuchos::RCP<const Epetra_Vector> disn = global_state().get_dis_n();

  // only forces are evaluated!
  constrman_->evaluate_force_stiff(
      time_np, disn, disnp_ptr_, fstrconstr_np_ptr_, Teuchos::null, pcon);

  return true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::LagPenConstraint::evaluate_stiff()
{
  check_init_setup();

  double time_np = global_state().get_time_np();
  Teuchos::ParameterList pcon;  // empty parameter list
  Teuchos::RCP<const Epetra_Vector> disn = global_state().get_dis_n();

  // only stiffnesses are evaluated!
  constrman_->evaluate_force_stiff(
      time_np, disn, disnp_ptr_, Teuchos::null, stiff_constr_ptr_, pcon);

  if (not stiff_constr_ptr_->Filled()) stiff_constr_ptr_->Complete();

  return true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::LagPenConstraint::evaluate_force_stiff()
{
  check_init_setup();

  double time_np = global_state().get_time_np();
  Teuchos::ParameterList pcon;  // empty parameter list
  Teuchos::RCP<const Epetra_Vector> disn = global_state().get_dis_n();

  constrman_->evaluate_force_stiff(
      time_np, disn, disnp_ptr_, fstrconstr_np_ptr_, stiff_constr_ptr_, pcon);

  if (not stiff_constr_ptr_->Filled()) stiff_constr_ptr_->Complete();

  return true;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::LagPenConstraint::assemble_force(
    Epetra_Vector& f, const double& timefac_np) const
{
  Teuchos::RCP<const Epetra_Vector> block_vec_ptr = Teuchos::null;

  Core::LinAlg::AssembleMyVector(1.0, f, timefac_np, *fstrconstr_np_ptr_);

  if (noxinterface_prec_ptr_->IsSaddlePointSystem())
  {
    // assemble constraint rhs
    block_vec_ptr = constrman_->GetError();

    if (block_vec_ptr.is_null())
      FOUR_C_THROW(
          "The constraint model vector is a nullptr pointer, although \n"
          "the structural part indicates, that constraint contributions \n"
          "are present!");

    const int elements_f = f.Map().NumGlobalElements();
    const int max_gid = get_block_dof_row_map_ptr()->MaxAllGID();
    // only call when f is the rhs of the full problem (not for structural
    // equilibriate initial state call)
    if (elements_f == max_gid + 1)
      Core::LinAlg::AssembleMyVector(1.0, f, timefac_np, *block_vec_ptr);
  }

  return true;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::LagPenConstraint::assemble_jacobian(
    Core::LinAlg::SparseOperator& jac, const double& timefac_np) const
{
  Teuchos::RCP<Core::LinAlg::SparseMatrix> block_ptr = Teuchos::null;

  // --- Kdd - block ---------------------------------------------------
  Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd_ptr = global_state().extract_displ_block(jac);
  jac_dd_ptr->Add(*stiff_constr_ptr_, false, timefac_np, 1.0);
  // no need to keep it
  stiff_constr_ptr_->Zero();

  if (noxinterface_prec_ptr_->IsSaddlePointSystem())
  {
    // --- Kdz - block - scale with time-integrator dependent value!-----
    block_ptr = (Teuchos::rcp_dynamic_cast<Core::LinAlg::SparseMatrix>(
        constrman_->GetConstrMatrix(), true));
    block_ptr->Scale(timefac_np);
    global_state().assign_model_block(jac, *block_ptr, Type(), STR::MatBlockType::displ_lm);
    // reset the block pointer, just to be on the safe side
    block_ptr = Teuchos::null;

    // --- Kzd - block - no scaling of this block (cf. diss Kloeppel p78)
    block_ptr =
        (Teuchos::rcp_dynamic_cast<Core::LinAlg::SparseMatrix>(constrman_->GetConstrMatrix(), true))
            ->Transpose();
    global_state().assign_model_block(jac, *block_ptr, Type(), STR::MatBlockType::lm_displ);
    // reset the block pointer, just to be on the safe side
    block_ptr = Teuchos::null;
  }

  return true;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::write_restart(
    Core::IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  iowriter.write_vector("lagrmultiplier", constrman_->GetLagrMultVector());
  iowriter.write_vector("refconval", constrman_->GetRefBaseValues());
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::read_restart(Core::IO::DiscretizationReader& ioreader)
{
  double time_n = global_state().get_time_n();
  constrman_->read_restart(ioreader, time_n);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::run_post_compute_x(
    const Epetra_Vector& xold, const Epetra_Vector& dir, const Epetra_Vector& xnew)
{
  check_init_setup();

  Teuchos::RCP<Epetra_Vector> lagmult_incr =
      Teuchos::rcp(new Epetra_Vector(*get_block_dof_row_map_ptr()));

  Core::LinAlg::Export(dir, *lagmult_incr);

  constrman_->UpdateLagrMult(lagmult_incr);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::update_step_state(const double& timefac_n)
{
  constrman_->Update();

  // add the constraint force contributions to the old structural
  // residual state vector
  if (not fstrconstr_np_ptr_.is_null())
  {
    Teuchos::RCP<Epetra_Vector>& fstructold_ptr = global_state().get_fstructure_old();
    fstructold_ptr->Update(timefac_n, *fstrconstr_np_ptr_, 1.0);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::update_step_element()
{
  // empty
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::determine_stress_strain()
{
  // nothing to do
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::determine_energy()
{
  // nothing to do
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::determine_optional_quantity()
{
  // nothing to do
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::output_step_state(
    Core::IO::DiscretizationWriter& iowriter) const
{
  // nothing to do
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::reset_step_state()
{
  check_init_setup();

  FOUR_C_THROW("Not yet implemented");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const Teuchos::RCP<LAGPENCONSTRAINT::NoxInterface>&
STR::MODELEVALUATOR::LagPenConstraint::nox_interface_ptr()
{
  check_init_setup();

  return noxinterface_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const Teuchos::RCP<LAGPENCONSTRAINT::NoxInterfacePrec>&
STR::MODELEVALUATOR::LagPenConstraint::NoxInterfacePrecPtr()
{
  check_init_setup();

  return noxinterface_prec_ptr_;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> STR::MODELEVALUATOR::LagPenConstraint::get_block_dof_row_map_ptr()
    const
{
  check_init_setup();

  if (noxinterface_prec_ptr_->IsSaddlePointSystem())
  {
    return constrman_->GetConstraintMap();
  }
  else
  {
    return global_state().dof_row_map();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> STR::MODELEVALUATOR::LagPenConstraint::get_current_solution_ptr()
    const
{
  // there are no model specific solution entries
  return Teuchos::null;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector>
STR::MODELEVALUATOR::LagPenConstraint::get_last_time_step_solution_ptr() const
{
  // there are no model specific solution entries
  return Teuchos::null;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::LagPenConstraint::post_output()
{
  check_init_setup();
  // empty
}  // post_output()

FOUR_C_NAMESPACE_CLOSE
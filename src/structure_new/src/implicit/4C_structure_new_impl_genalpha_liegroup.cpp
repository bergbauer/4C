/*-----------------------------------------------------------*/
/*! \file

\brief Modified Generalized Alpha time integrator for Lie groups
      (can handle non-additive rotational pseudo-vector DoFs)


\level 3

*/
/*-----------------------------------------------------------*/

#include "4C_structure_new_impl_genalpha_liegroup.hpp"

#include "4C_io.hpp"
#include "4C_linalg_sparsematrix.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_structure_new_dbc.hpp"
#include "4C_structure_new_model_evaluator.hpp"
#include "4C_structure_new_model_evaluator_data.hpp"
#include "4C_structure_new_timint_base.hpp"
#include "4C_structure_new_utils.hpp"
#include "4C_utils_exceptions.hpp"

#include <Epetra_Vector.h>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
STR::IMPLICIT::GenAlphaLieGroup::GenAlphaLieGroup() : accn_mod_(Teuchos::null)
{
  // empty constructor
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::Setup()
{
  check_init();

  // ---------------------------------------------------------------------------
  // setup additional state vectors of modified acceleration
  // ---------------------------------------------------------------------------
  accn_mod_ = Teuchos::rcp(new Epetra_Vector(*global_state().dof_row_map_view(), true));

  // Call the Setup() of the parent GenAlpha class
  GenAlpha::Setup();
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::post_setup()
{
  check_init_setup();

  if (sdyn().get_mass_lin_type() != Inpar::STR::ml_rotations and !sdyn().NeglectInertia())
  {
    /* we can use this method for all elements with additive DoFs,
     * but it won't work like this for non-additive rotation vector DoFs */
    equilibrate_initial_state();
  }
  else
  {
    /* If we are restarting the simulation, we get the acceleration state from the
     * restart file. So we are already done at this point. */
    if (tim_int().is_restarting()) return;

    // so far, we are restricted to vanishing initial accelerations
    Teuchos::RCP<Epetra_Vector> accnp_ptr = global_state().get_acc_np();
    accnp_ptr->PutScalar(0.0);

    // sanity check whether assumption is fulfilled
    /* ToDo tolerance value is experience and based on following consideration:
     * epsilon = O(1e-15) scaled with EA = O(1e8) yields residual contributions in
     * initial, stress free state of order 1e-8 */
    if (not current_state_is_equilibrium(1.0e-6) and global_state().get_my_rank() == 0)
      std::cout << "\nSERIOUS WARNING: Initially non vanishing acceleration states "
                   "in case of ml_rotation = true,\ni.e. an initial state where the system "
                   "is not equilibrated, cannot yet be computed correctly.\nThis means your "
                   "results in the beginning are not physically correct\n"
                << std::endl;

    // call update routines to copy states from t_{n+1} to t_{n}
    // note that the time step is not incremented
    pre_update();
    update_step_state();
    update_step_element();
    post_update();
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::set_state(const Epetra_Vector& x)
{
  check_init_setup();

  if (is_predictor_state()) return;

  update_constant_state_contributions();

  const double& dt = (*global_state().get_delta_time())[0];
  // ---------------------------------------------------------------------------
  // new end-point displacements
  // ---------------------------------------------------------------------------
  Teuchos::RCP<Epetra_Vector> disnp_ptr = global_state().extract_displ_entries(x);
  global_state().get_dis_np()->Scale(1.0, *disnp_ptr);

  /* ToDo in case we want to handle rotation vector DoFs correctly on time
   *      integrator level, the update procedure needs to be adapted here;
   *      use global_state().ExportAdditiveEntries() and ExportRotvecEntries() for
   *      this */

  // ---------------------------------------------------------------------------
  // new end-point velocities
  // ---------------------------------------------------------------------------
  global_state().get_vel_np()->Update(
      1.0, *(*const_vel_acc_update_ptr_)(0), gamma_ / (beta_ * dt), *disnp_ptr, 0.0);

  // ---------------------------------------------------------------------------
  // new end-point accelerations
  // ---------------------------------------------------------------------------
  global_state().get_acc_np()->Update(1.0, *(*const_vel_acc_update_ptr_)(1),
      (1.0 - alpham_) / (beta_ * dt * dt * (1.0 - alphaf_)), *disnp_ptr, 0.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::write_restart(
    Core::IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  check_init_setup();
  // write modified acceleration vector
  iowriter.write_vector("accn_mod", accn_mod_);

  GenAlpha::write_restart(iowriter, forced_writerestart);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::read_restart(Core::IO::DiscretizationReader& ioreader)
{
  check_init_setup();
  ioreader.read_vector(accn_mod_, "accn_mod");

  GenAlpha::read_restart(ioreader);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::update_step_state()
{
  check_init_setup();

  // new at t_{n+1} -> t_n
  //    acc_mod_{n} := -alpha_m/(1-alpha_m) * acc_mod_{n}
  accn_mod_->Scale(-alpham_ / (1.0 - alpham_));
  accn_mod_->Update(alphaf_ / (1.0 - alpham_), *global_state().get_acc_n(), 1.0);
  accn_mod_->Update((1.0 - alphaf_) / (1.0 - alpham_), *global_state().get_acc_np(), 1.0);

  // ---------------------------------------------------------------------------
  // update model specific variables
  // ---------------------------------------------------------------------------
  model_eval().update_step_state(0.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::update_constant_state_contributions()
{
  const double& dt = (*global_state().get_delta_time())[0];

  /* ToDo in case we want to handle rotation vector DoFs correctly on time
   *      integrator level, the update procedure needs to be adapted here;
   *      use global_state().ExportAdditiveEntries() and ExportRotvecEntries() for
   *      this */

  // ---------------------------------------------------------------------------
  // velocity
  // ---------------------------------------------------------------------------
  (*const_vel_acc_update_ptr_)(0)->Scale((1.0 - gamma_ / (2.0 * beta_)) * dt, *accn_mod_);
  (*const_vel_acc_update_ptr_)(0)->Update(1.0 - gamma_ / beta_, *global_state().get_vel_n(), 1.0);
  (*const_vel_acc_update_ptr_)(0)->Update(-gamma_ / (beta_ * dt), *global_state().get_dis_n(), 1.0);

  // ---------------------------------------------------------------------------
  // acceleration
  // ---------------------------------------------------------------------------
  (*const_vel_acc_update_ptr_)(1)->Scale(alphaf_ / (alphaf_ - 1.0), *global_state().get_acc_n());
  (*const_vel_acc_update_ptr_)(1)->Update(
      alpham_ / (1.0 - alphaf_) - (1.0 - alpham_) * (0.5 - beta_) / (beta_ * (1.0 - alphaf_)),
      *accn_mod_, 1.0);
  (*const_vel_acc_update_ptr_)(1)->Update(
      -(1.0 - alpham_) / (beta_ * dt * (1.0 - alphaf_)), *global_state().get_vel_n(), 1.0);
  (*const_vel_acc_update_ptr_)(1)->Update(
      -(1.0 - alpham_) / (beta_ * dt * dt * (1.0 - alphaf_)), *global_state().get_dis_n(), 1.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double STR::IMPLICIT::GenAlphaLieGroup::get_int_param() const
{
  check_init_setup();
  return 0.0;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::add_visco_mass_contributions(Epetra_Vector& f) const
{
  // viscous damping forces at t_{n+1}
  Core::LinAlg::AssembleMyVector(1.0, f, 1.0, *fvisconp_ptr_);
  // inertia forces at t_{n+1}
  Core::LinAlg::AssembleMyVector(1.0, f, 1.0, *finertianp_ptr_);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::add_visco_mass_contributions(
    Core::LinAlg::SparseOperator& jac) const
{
  Teuchos::RCP<Core::LinAlg::SparseMatrix> stiff_ptr = global_state().extract_displ_block(jac);
  const double& dt = (*global_state().get_delta_time())[0];
  // add inertial contributions to structural stiffness block
  stiff_ptr->Add(*global_state().get_mass_matrix(), false,
      (1.0 - alpham_) / (beta_ * dt * dt * (1.0 - alphaf_)), 1.0);
  // add Rayleigh damping contributions
  if (tim_int().get_data_sdyn().get_damping_type() == Inpar::STR::damp_rayleigh)
    stiff_ptr->Add(*global_state().get_damp_matrix(), false, gamma_ / (beta_ * dt), 1.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::predict_const_dis_consist_vel_acc(
    Epetra_Vector& disnp, Epetra_Vector& velnp, Epetra_Vector& accnp) const
{
  check_init_setup();
  Teuchos::RCP<const Epetra_Vector> disn = global_state().get_dis_n();
  Teuchos::RCP<const Epetra_Vector> veln = global_state().get_vel_n();
  Teuchos::RCP<const Epetra_Vector> accn = global_state().get_acc_n();
  const double& dt = (*global_state().get_delta_time())[0];

  // constant predictor: displacement in domain
  disnp.Update(1.0, *disn, 0.0);

  // consistent velocities following Newmark formulas
  velnp.Update(1.0, disnp, -1.0, *disn, 0.0);
  velnp.Update((beta_ - gamma_) / beta_, *veln, (2.0 * beta_ - gamma_) * dt / (2.0 * beta_),
      *accn_mod_, gamma_ / (beta_ * dt));

  // consistent accelerations following Newmark formulas
  accnp.Update(1.0, disnp, -1.0, *disn, 0.0);
  accnp.Update(-(1.0 - alpham_) / (beta_ * dt * (1 - alphaf_)), *veln, -alphaf_ / (1.0 - alphaf_),
      *accn, (1.0 - alpham_) / (beta_ * dt * dt * (1.0 - alphaf_)));
  accnp.Update(
      alpham_ / (1.0 - alphaf_) - (1.0 - alpham_) * (0.5 - beta_) / (beta_ * (1.0 - alphaf_)),
      *accn_mod_, 1.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::GenAlphaLieGroup::predict_const_vel_consist_acc(
    Epetra_Vector& disnp, Epetra_Vector& velnp, Epetra_Vector& accnp) const
{
  check_init_setup();

  FOUR_C_THROW(
      "Predictor ConstVelConsistAcc is not supported in Lie group GenAlpha so"
      " far! Use ConstDisConsistVelAcc!");

  return true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::GenAlphaLieGroup::predict_const_acc(
    Epetra_Vector& disnp, Epetra_Vector& velnp, Epetra_Vector& accnp) const
{
  check_init_setup();

  FOUR_C_THROW(
      "Predictor ConstAcc is not supported in Lie group GenAlpha so far! Use "
      "ConstDisConsistVelAcc!");

  return true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlphaLieGroup::reset_eval_params()
{
  // set the time step dependent parameters for the element evaluation
  GenAlpha::reset_eval_params();

  /* in case we have non-additive rotation (pseudo-)vector DOFs, we need to pass
   * the GenAlpha parameters to the beam elements via beam parameter interface */
  if (tim_int().get_data_sdyn().GetMassLinType() == Inpar::STR::ml_rotations)
  {
    eval_data().GetBeamData().set_beta(beta_);
    eval_data().GetBeamData().set_gamma(gamma_);
    eval_data().GetBeamData().set_alphaf(alphaf_);
    eval_data().GetBeamData().set_alpham(alpham_);
  }
}

FOUR_C_NAMESPACE_CLOSE
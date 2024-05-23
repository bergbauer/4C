/*-----------------------------------------------------------*/
/*! \file

\brief Generalized Alpha time integrator.


\level 3

*/
/*-----------------------------------------------------------*/

#include "4C_structure_new_impl_genalpha.hpp"

#include "4C_global_data.hpp"
#include "4C_inpar_structure.hpp"
#include "4C_io.hpp"
#include "4C_io_pstream.hpp"
#include "4C_linalg_sparsematrix.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_structure_new_dbc.hpp"
#include "4C_structure_new_model_evaluator.hpp"
#include "4C_structure_new_model_evaluator_data.hpp"
#include "4C_structure_new_model_evaluator_structure.hpp"
#include "4C_structure_new_timint_base.hpp"
#include "4C_structure_new_utils.hpp"
#include "4C_utils_exceptions.hpp"
#include "4C_utils_function.hpp"

#include <Epetra_Vector.h>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
STR::IMPLICIT::GenAlpha::GenAlpha()
    : beta_(coeffs_.beta_),
      gamma_(coeffs_.gamma_),
      alphaf_(coeffs_.alphaf_),
      alpham_(coeffs_.alpham_),
      rhoinf_(coeffs_.rhoinf_),
      const_vel_acc_update_ptr_(Teuchos::null),
      fvisconp_ptr_(Teuchos::null),
      fviscon_ptr_(Teuchos::null),
      finertianp_ptr_(Teuchos::null),
      finertian_ptr_(Teuchos::null)
{
  // empty constructor
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::Setup()
{
  CheckInit();
  // Call the Setup() of the abstract base class first.
  Generic::Setup();

  const STR::TIMINT::GenAlphaDataSDyn& genalpha_sdyn =
      dynamic_cast<const STR::TIMINT::GenAlphaDataSDyn&>(TimInt().GetDataSDyn());

  // ---------------------------------------------------------------------------
  // setup time integration parameters
  // ---------------------------------------------------------------------------
  set_time_integration_coefficients(coeffs_);

  // sanity checks and some screen output
  if (GlobalState().GetMyRank() == 0)
  {
    if (rhoinf_ > 0.0) std::cout << "   rho = " << rhoinf_ << std::endl;
    // beta
    if ((beta_ <= 0.0) or (beta_ > 0.5))
      FOUR_C_THROW("beta out of range (0.0,0.5]");
    else
      std::cout << "   beta = " << beta_ << std::endl;
    // gamma
    if ((gamma_ <= 0.0) or (gamma_ > 1.0))
      FOUR_C_THROW("gamma out of range (0.0,1.0]");
    else
      std::cout << "   gamma = " << gamma_ << std::endl;
    // alpha_f
    if ((alphaf_ < 0.0) or (alphaf_ >= 1.0))
      FOUR_C_THROW("alpha_f out of range [0.0,1.0)");
    else
      std::cout << "   alpha_f = " << alphaf_ << std::endl;
    // alpha_m
    if ((alpham_ < -1.0) or (alpham_ >= 1.0))
      FOUR_C_THROW("alpha_m out of range [-1.0,1.0)");
    else
      std::cout << "   alpha_m = " << alpham_ << std::endl;

    /* ------ mid-averaging type -----------------------------------------------
     * In principle, there exist two mid-averaging possibilities, TR-like and
     * IMR-like, where TR-like means trapezoidal rule and IMR-like means implicit
     * mid-point rule. We used to maintain implementations of both variants, but
     * due to its significantly higher complexity, the IMR-like version has been
     * deleted (popp 02/2013). The nice thing about TR-like mid-averaging is that
     * all element (and thus also material) calls are exclusively(!) carried out
     * at the end-point t_{n+1} of each time interval, but never explicitly at
     * some generalized midpoint, such as t_{n+1-\alpha_f}. Thus, any cumbersome
     * extrapolation of history variables, etc. becomes obsolete. */
    const enum INPAR::STR::MidAverageEnum& midavg = genalpha_sdyn.GetMidAverageType();
    if (midavg != INPAR::STR::midavg_trlike)
      FOUR_C_THROW("mid-averaging of internal forces only implemented TR-like");
    else
      std::cout << "   midavg = " << INPAR::STR::MidAverageString(midavg) << std::endl;
  }

  // ---------------------------------------------------------------------------
  // setup mid-point vectors
  // ---------------------------------------------------------------------------
  const_vel_acc_update_ptr_ =
      Teuchos::rcp(new Epetra_MultiVector(*GlobalState().DofRowMapView(), 2, true));

  // ---------------------------------------------------------------------------
  // setup pointers to the force vectors of the global state data container
  // ---------------------------------------------------------------------------
  finertian_ptr_ = GlobalState().GetFinertialN();
  finertianp_ptr_ = GlobalState().GetFinertialNp();

  fviscon_ptr_ = GlobalState().GetFviscoN();
  fvisconp_ptr_ = GlobalState().GetFviscoNp();

  // -------------------------------------------------------------------
  // set initial displacement
  // -------------------------------------------------------------------
  set_initial_displacement(
      TimInt().GetDataSDyn().GetInitialDisp(), TimInt().GetDataSDyn().StartFuncNo());

  // Has to be set before the PostSetup() routine is called!
  issetup_ = true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::PostSetup()
{
  CheckInitSetup();

  // ---------------------------------------------------------------------------
  // check for applicability of classical GenAlpha scheme
  // ---------------------------------------------------------------------------
  // set the constant parameters for the element evaluation
  if (TimInt().GetDataSDyn().GetMassLinType() == INPAR::STR::ml_rotations)
  {
    FOUR_C_THROW(
        "MASSLIN=ml_rotations is not supported by classical GenAlpha! "
        "Choose GenAlphaLieGroup instead!");
  }

  if (not SDyn().NeglectInertia())
  {
    equilibrate_initial_state();
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::set_time_integration_coefficients(Coefficients& coeffs) const
{
  if (IsInit() and IsSetup())
  {
    coeffs = coeffs_;
    return;
  }

  const STR::TIMINT::GenAlphaDataSDyn& genalpha_sdyn =
      dynamic_cast<const STR::TIMINT::GenAlphaDataSDyn&>(TimInt().GetDataSDyn());

  // get a copy of the input parameters
  coeffs.beta_ = genalpha_sdyn.GetBeta();
  coeffs.gamma_ = genalpha_sdyn.GetGamma();
  coeffs.alphaf_ = genalpha_sdyn.GetAlphaF();
  coeffs.alpham_ = genalpha_sdyn.GetAlphaM();
  coeffs.rhoinf_ = genalpha_sdyn.GetRhoInf();

  STR::ComputeGeneralizedAlphaParameters(coeffs);
}



/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double STR::IMPLICIT::GenAlpha::GetModelValue(const Epetra_Vector& x)
{
  // --- kinetic energy increment
  Teuchos::RCP<const Epetra_Vector> accnp_ptr = GlobalState().GetAccNp();
  const Epetra_Vector& accnp = *accnp_ptr;

  Teuchos::RCP<const Epetra_Vector> accn_ptr = GlobalState().GetAccN();
  const Epetra_Vector& accn = *accn_ptr;

  Epetra_Vector accm(accnp);
  accm.Update(alpham_, accn, 1.0 - alpham_);

  const double dt = (*GlobalState().GetDeltaTime())[0];
  Teuchos::RCP<const CORE::LINALG::SparseOperator> mass_ptr = GlobalState().GetMassMatrix();
  const CORE::LINALG::SparseMatrix& mass =
      dynamic_cast<const CORE::LINALG::SparseMatrix&>(*mass_ptr);
  Epetra_Vector tmp(mass.RangeMap(), true);

  double kin_energy_incr = 0.0;
  mass.Multiply(false, accm, tmp);
  tmp.Dot(accm, &kin_energy_incr);

  kin_energy_incr *= 0.5 * beta_ * dt * dt / (1 - alpham_);

  // --- internal energy
  EvalData().clear_values_for_all_energy_types();
  STR::MODELEVALUATOR::Structure& str_model =
      dynamic_cast<STR::MODELEVALUATOR::Structure&>(Evaluator(INPAR::STR::model_structure));

  Teuchos::RCP<const Epetra_Vector> disnp_ptr = GlobalState().ExtractDisplEntries(x);
  const Epetra_Vector& disnp = *disnp_ptr;

  const double af_np = 1.0 - alphaf_;
  str_model.determine_strain_energy(disnp, true);
  const double int_energy_np = af_np * EvalData().GetEnergyData(STR::internal_energy);

  // --- external energy
  double ext_energy_np = 0.0;
  GlobalState().GetFextNp()->Dot(disnp, &ext_energy_np);
  ext_energy_np *= af_np;

  // --- old contributions
  // Note that all gradient/force contributions related to the previous
  // time step are stored in the global state as FstructureOld. This includes
  // the contact forces as well! See UpdateStepState in the different
  // model evaluator classes.
  double disNp_forcesN = 0.0;
  GlobalState().GetFstructureOld()->Dot(disnp, &disNp_forcesN);

  const double total = kin_energy_incr + int_energy_np + disNp_forcesN - ext_energy_np;

  std::ostream& os = IO::cout.os(IO::debug);
  os << __LINE__ << __PRETTY_FUNCTION__ << "\n";
  os << "kin_energy_incr              = " << kin_energy_incr << "\n"
     << "int_energy * (1-af)          = " << int_energy_np << "\n"
     << "ext_energy * (1-af)          = " << ext_energy_np << "\n"
     << "old_gradients * disnp * (af) = " << disNp_forcesN << "\n";
  os << std::string(80, '-') << "\n";
  os << "Total action integral        = " << total << "\n";
  os << std::string(80, '-') << "\n";

  return total;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::SetState(const Epetra_Vector& x)
{
  CheckInitSetup();

  if (IsPredictorState()) return;

  update_constant_state_contributions();

  const double& dt = (*GlobalState().GetDeltaTime())[0];
  // ---------------------------------------------------------------------------
  // new end-point displacements
  // ---------------------------------------------------------------------------
  Teuchos::RCP<Epetra_Vector> disnp_ptr = GlobalState().ExtractDisplEntries(x);
  GlobalState().GetDisNp()->Scale(1.0, *disnp_ptr);

  // ---------------------------------------------------------------------------
  // new end-point velocities
  // ---------------------------------------------------------------------------
  GlobalState().GetVelNp()->Update(
      1.0, *(*const_vel_acc_update_ptr_)(0), gamma_ / (beta_ * dt), *disnp_ptr, 0.0);

  // ---------------------------------------------------------------------------
  // new end-point accelerations
  // ---------------------------------------------------------------------------
  GlobalState().GetAccNp()->Update(
      1.0, *(*const_vel_acc_update_ptr_)(1), 1.0 / (beta_ * dt * dt), *disnp_ptr, 0.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::update_constant_state_contributions()
{
  const double& dt = (*GlobalState().GetDeltaTime())[0];

  // ---------------------------------------------------------------------------
  // velocity
  // ---------------------------------------------------------------------------
  (*const_vel_acc_update_ptr_)(0)->Scale((beta_ - gamma_) / beta_, *GlobalState().GetVelN());
  (*const_vel_acc_update_ptr_)(0)->Update(
      (2.0 * beta_ - gamma_) * dt / (2.0 * beta_), *GlobalState().GetAccN(), 1.0);
  (*const_vel_acc_update_ptr_)(0)->Update(-gamma_ / (beta_ * dt), *GlobalState().GetDisN(), 1.0);

  // ---------------------------------------------------------------------------
  // acceleration
  // ---------------------------------------------------------------------------
  (*const_vel_acc_update_ptr_)(1)->Scale(
      (2.0 * beta_ - 1.0) / (2.0 * beta_), *GlobalState().GetAccN());
  (*const_vel_acc_update_ptr_)(1)->Update(-1.0 / (beta_ * dt), *GlobalState().GetVelN(), 1.0);
  (*const_vel_acc_update_ptr_)(1)->Update(-1.0 / (beta_ * dt * dt), *GlobalState().GetDisN(), 1.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::GenAlpha::ApplyForce(const Epetra_Vector& x, Epetra_Vector& f)
{
  CheckInitSetup();

  // ---------------------------------------------------------------------------
  // evaluate the different model types (static case) at t_{n+1}^{i}
  // ---------------------------------------------------------------------------
  // set the time step dependent parameters for the element evaluation
  ResetEvalParams();
  return ModelEval().ApplyForce(x, f, 1.0 - GetIntParam());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::GenAlpha::ApplyStiff(const Epetra_Vector& x, CORE::LINALG::SparseOperator& jac)
{
  CheckInitSetup();

  // ---------------------------------------------------------------------------
  // evaluate the different model types (static case) at t_{n+1}^{i}
  // ---------------------------------------------------------------------------
  // set the time step dependent parameters for the element evaluation
  ResetEvalParams();
  const bool ok = ModelEval().ApplyStiff(x, jac, 1.0 - GetIntParam());

  if (not ok) return ok;

  jac.Complete();

  return ok;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::GenAlpha::ApplyForceStiff(
    const Epetra_Vector& x, Epetra_Vector& f, CORE::LINALG::SparseOperator& jac)
{
  CheckInitSetup();
  // ---------------------------------------------------------------------------
  // evaluate the different model types (static case) at t_{n+1}^{i}
  // ---------------------------------------------------------------------------
  // set the time step dependent parameters for the element evaluation
  ResetEvalParams();
  const bool ok = ModelEval().ApplyForceStiff(x, f, jac, 1.0 - GetIntParam());

  if (not ok) return ok;

  jac.Complete();

  return ok;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::GenAlpha::AssembleForce(
    Epetra_Vector& f, const std::vector<INPAR::STR::ModelType>* without_these_models) const
{
  CheckInitSetup();

  // set the time step dependent parameters for the assembly
  return ModelEval().AssembleForce(1.0 - GetIntParam(), f, without_these_models);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::GenAlpha::AssembleJac(CORE::LINALG::SparseOperator& jac,
    const std::vector<INPAR::STR::ModelType>* without_these_models) const
{
  CheckInitSetup();

  // set the time step dependent parameters for the assembly
  return ModelEval().AssembleJacobian(1.0 - GetIntParam(), jac, without_these_models);
}


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::add_visco_mass_contributions(Epetra_Vector& f) const
{
  // viscous damping forces at t_{n+1-alpha_f}
  CORE::LINALG::AssembleMyVector(1.0, f, alphaf_, *fviscon_ptr_);
  CORE::LINALG::AssembleMyVector(1.0, f, 1 - alphaf_, *fvisconp_ptr_);
  // inertial forces at t_{n+1-alpha_m}
  CORE::LINALG::AssembleMyVector(1.0, f, 1 - alpham_, *finertianp_ptr_);
  CORE::LINALG::AssembleMyVector(1.0, f, alpham_, *finertian_ptr_);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::add_visco_mass_contributions(CORE::LINALG::SparseOperator& jac) const
{
  Teuchos::RCP<CORE::LINALG::SparseMatrix> stiff_ptr = GlobalState().ExtractDisplBlock(jac);
  const double& dt = (*GlobalState().GetDeltaTime())[0];
  // add inertial contributions and scale the structural stiffness block
  stiff_ptr->Add(*GlobalState().GetMassMatrix(), false, (1.0 - alpham_) / (beta_ * dt * dt), 1.0);
  // add Rayleigh damping contributions
  if (TimInt().GetDataSDyn().GetDampingType() == INPAR::STR::damp_rayleigh)
    stiff_ptr->Add(
        *GlobalState().GetDampMatrix(), false, (1.0 - alphaf_) * gamma_ / (beta_ * dt), 1.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::WriteRestart(
    IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  CheckInitSetup();
  // write dynamic forces
  iowriter.WriteVector("finert", finertian_ptr_);
  iowriter.WriteVector("fvisco", fviscon_ptr_);

  ModelEval().WriteRestart(iowriter, forced_writerestart);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::ReadRestart(IO::DiscretizationReader& ioreader)
{
  CheckInitSetup();
  ioreader.ReadVector(finertian_ptr_, "finert");
  ioreader.ReadVector(fviscon_ptr_, "fvisco");

  ModelEval().ReadRestart(ioreader);
  update_constant_state_contributions();
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double STR::IMPLICIT::GenAlpha::CalcRefNormForce(
    const enum ::NOX::Abstract::Vector::NormType& type) const
{
  CheckInitSetup();
  FOUR_C_THROW("Not yet implemented! (see the Statics integration for an example)");
  return -1.0;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double STR::IMPLICIT::GenAlpha::GetIntParam() const
{
  // access the alphaf value even if the time integrator has not yet been setup
  Coefficients coeffs;
  set_time_integration_coefficients(coeffs);

  return coeffs.alphaf_;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double STR::IMPLICIT::GenAlpha::GetAccIntParam() const
{
  CheckInitSetup();
  return alpham_;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::UpdateStepState()
{
  CheckInitSetup();
  // ---------------------------------------------------------------------------
  // dynamic effects
  // ---------------------------------------------------------------------------
  // new at t_{n+1} -> t_n
  //    finertial_{n} := finertial_{n+1}
  finertian_ptr_->Scale(1.0, *GlobalState().GetFinertialNp());
  // new at t_{n+1} -> t_n
  //    fviscous_{n} := fviscous_{n+1}
  fviscon_ptr_->Scale(1.0, *fvisconp_ptr_);

  // ---------------------------------------------------------------------------
  // update model specific variables
  // ---------------------------------------------------------------------------
  ModelEval().UpdateStepState(alphaf_);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::UpdateStepElement()
{
  CheckInitSetup();
  ModelEval().UpdateStepElement();
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::PostUpdate() { update_constant_state_contributions(); }

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::predict_const_dis_consist_vel_acc(
    Epetra_Vector& disnp, Epetra_Vector& velnp, Epetra_Vector& accnp) const
{
  CheckInitSetup();
  Teuchos::RCP<const Epetra_Vector> disn = GlobalState().GetDisN();
  Teuchos::RCP<const Epetra_Vector> veln = GlobalState().GetVelN();
  Teuchos::RCP<const Epetra_Vector> accn = GlobalState().GetAccN();
  const double& dt = (*GlobalState().GetDeltaTime())[0];

  // constant predictor: displacement in domain
  disnp.Scale(1.0, *disn);

  // consistent velocities following Newmark formulas
  /* Since disnp and disn are equal we can skip the current
   * update part and have to consider only the old state at t_{n}.
   *           disnp-disn = 0.0                                 */
  velnp.Update(
      (beta_ - gamma_) / beta_, *veln, (2.0 * beta_ - gamma_) * dt / (2.0 * beta_), *accn, 0.0);

  // consistent accelerations following Newmark formulas
  /* Since disnp and disn are equal we can skip the current
   * update part and have to consider only the old state at t_{n}.
   *           disnp-disn = 0.0                                 */
  accnp.Update(-1.0 / (beta_ * dt), *veln, (2.0 * beta_ - 1.0) / (2.0 * beta_), *accn, 0.0);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::GenAlpha::predict_const_vel_consist_acc(
    Epetra_Vector& disnp, Epetra_Vector& velnp, Epetra_Vector& accnp) const
{
  CheckInitSetup();
  /* In the general dynamic case there is no need to design a special start-up
   * procedure, since it is possible to prescribe an initial velocity or
   * acceleration. The corresponding accelerations are calculated in the
   * equilibrate_initial_state() routine. */

  Teuchos::RCP<const Epetra_Vector> disn = GlobalState().GetDisN();
  Teuchos::RCP<const Epetra_Vector> veln = GlobalState().GetVelN();
  Teuchos::RCP<const Epetra_Vector> accn = GlobalState().GetAccN();
  const double& dt = (*GlobalState().GetDeltaTime())[0];

  // extrapolated displacements based upon constant velocities
  // d_{n+1} = d_{n} + dt * v_{n}
  disnp.Update(1.0, *disn, dt, *veln, 0.0);

  // consistent velocities following Newmark formulas
  velnp.Update(1.0, disnp, -1.0, *disn, 0.0);
  velnp.Update((beta_ - gamma_) / beta_, *veln, (2. * beta_ - gamma_) * dt / (2. * beta_), *accn,
      gamma_ / (beta_ * dt));

  // consistent accelerations following Newmark formulas
  accnp.Update(1.0, disnp, -1.0, *disn, 0.0);
  accnp.Update(-1.0 / (beta_ * dt), *veln, (2.0 * beta_ - 1.0) / (2.0 * beta_), *accn,
      1. / (beta_ * dt * dt));

  return true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::GenAlpha::PredictConstAcc(
    Epetra_Vector& disnp, Epetra_Vector& velnp, Epetra_Vector& accnp) const
{
  CheckInitSetup();
  /* In the general dynamic case there is no need to design a special start-up
   * procedure, since it is possible to prescribe an initial velocity or
   * acceleration. The corresponding accelerations are calculated in the
   * equilibrate_initial_state() routine. */

  Teuchos::RCP<const Epetra_Vector> disn = GlobalState().GetDisN();
  Teuchos::RCP<const Epetra_Vector> veln = GlobalState().GetVelN();
  Teuchos::RCP<const Epetra_Vector> accn = GlobalState().GetAccN();
  const double& dt = (*GlobalState().GetDeltaTime())[0];

  // extrapolated displacements based upon constant accelerations
  // d_{n+1} = d_{n} + dt * v_{n} + dt^2 / 2 * a_{n}
  disnp.Update(1.0, *disn, dt, *veln, 0.0);
  disnp.Update(0.5 * dt * dt, *accn, 1.0);

  // extrapolated velocities (equal to consistent velocities)
  // v_{n+1} = v_{n} + dt * a_{n}
  velnp.Update(1.0, *veln, dt, *accn, 0.0);

  // constant accelerations (equal to consistent accelerations)
  accnp.Update(1.0, *accn, 0.0);

  return true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::GenAlpha::ResetEvalParams()
{
  // call base class
  STR::IMPLICIT::Generic::ResetEvalParams();

  // set the time step dependent parameters for the element evaluation
  const double& dt = (*GlobalState().GetDeltaTime())[0];
  double timeintfac_dis = beta_ * dt * dt;
  double timeintfac_vel = gamma_ * dt;

  EvalData().SetTimIntFactorDisp(timeintfac_dis);
  EvalData().SetTimIntFactorVel(timeintfac_vel);
}

FOUR_C_NAMESPACE_CLOSE

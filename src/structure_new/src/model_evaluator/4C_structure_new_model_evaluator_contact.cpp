/*---------------------------------------------------------------------*/
/*! \file

\brief Evaluation and assembly of all contact terms


\level 3
*/
/*---------------------------------------------------------------------*/

#include "4C_structure_new_model_evaluator_contact.hpp"

#include "4C_contact_lagrange_strategy_poro.hpp"
#include "4C_contact_strategy_factory.hpp"
#include "4C_global_data.hpp"
#include "4C_io.hpp"
#include "4C_io_control.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_solver_nonlin_nox_group.hpp"
#include "4C_solver_nonlin_nox_group_prepostoperator.hpp"
#include "4C_solver_nonlin_nox_solver_linesearchbased.hpp"
#include "4C_structure_new_dbc.hpp"
#include "4C_structure_new_impl_generic.hpp"
#include "4C_structure_new_model_evaluator_data.hpp"
#include "4C_structure_new_model_evaluator_manager.hpp"
#include "4C_structure_new_timint_base.hpp"
#include "4C_structure_new_utils.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::setup()
{
  check_init();
  eval_contact_ptr_ = eval_data().contact_ptr();

  // ---------------------------------------------------------------------
  // create the contact factory
  // ---------------------------------------------------------------------
  CONTACT::STRATEGY::Factory factory;
  factory.init(global_state_ptr()->get_discret());
  factory.setup(Global::Problem::instance()->n_dim());

  // check the problem dimension
  factory.check_dimension();

  // create some local variables (later to be stored in strategy)
  std::vector<Teuchos::RCP<CONTACT::Interface>> interfaces;
  Teuchos::ParameterList cparams;

  // read and check contact input parameters
  factory.read_and_check_input(cparams);

  // check for fill_complete of discretization
  if (not discret().filled()) FOUR_C_THROW("discretization is not fillcomplete");

  // ---------------------------------------------------------------------
  // build the contact interfaces
  // ---------------------------------------------------------------------
  // FixMe Would be great, if we get rid of these poro parameters...
  bool poroslave = false;
  bool poromaster = false;
  factory.build_interfaces(cparams, interfaces, poroslave, poromaster);

  // ---------------------------------------------------------------------
  // build the solver strategy object
  // ---------------------------------------------------------------------
  eval_contact_ptr_->set(&discret(), 0);
  strategy_ptr_ = factory.build_strategy(
      cparams, poroslave, poromaster, dof_offset(), interfaces, eval_contact_ptr_.get());
  eval_contact_ptr_->clear_entry(Core::Gen::AnyDataContainer::DataType::any, 0);

  // build the search tree
  factory.build_search_tree(interfaces);

  // print final screen output
  factory.print(interfaces, strategy_ptr_, cparams);

  // ---------------------------------------------------------------------
  // final touches to the contact strategy
  // ---------------------------------------------------------------------
  strategy_ptr_->store_dirichlet_status(integrator().get_dbc().get_dbc_map_extractor());
  strategy_ptr_->set_state(Mortar::state_new_displacement, integrator().get_dbc().get_zeros());
  strategy_ptr_->save_reference_state(integrator().get_dbc().get_zeros_ptr());
  strategy_ptr_->evaluate_reference_state();
  strategy_ptr_->inttime_init();
  set_time_integration_info(*strategy_ptr_);
  strategy_ptr_->redistribute_contact(global_state().get_dis_n(), global_state().get_vel_n());

  check_pseudo2d();

  post_setup(cparams);

  issetup_ = true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::post_setup(Teuchos::ParameterList& cparams)
{
  // do nothing
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::check_pseudo2d() const
{
  // print messages for multifield problems (e.g FSI)
  const Core::ProblemType probtype = Global::Problem::instance()->get_problem_type();
  if ((probtype != Core::ProblemType::structure) and (global_state().get_my_rank() == 0))
  {
    // warnings
#ifdef CONTACTPSEUDO2D
    std::cout << "WARNING: The flag CONTACTPSEUDO2D is switched on. If this "
              << "is a real 3D problem, switch it off!" << std::endl;
#else
    std::cout << "Solid::ModelEvaluator::Contact::check_pseudo2d -- "
              << "WARNING: \nThe flag CONTACTPSEUDO2D is switched off. If this "
              << "is a 2D problem modeled pseudo-3D, switch it on!" << std::endl;
#endif
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::set_time_integration_info(
    CONTACT::AbstractStrategy& strategy) const
{
  const Inpar::Solid::DynamicType dyntype = tim_int().get_data_sdyn().get_dynamic_type();
  const double time_fac = integrator().get_int_param();

  strategy.set_time_integration_info(time_fac, dyntype);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::reset(const Core::LinAlg::Vector<double>& x)
{
  check_init_setup();
  std::vector<Teuchos::RCP<const Core::LinAlg::Vector<double>>> eval_vec(2, Teuchos::null);
  eval_vec[0] = global_state().get_dis_np();
  eval_vec[1] = Teuchos::rcpFromRef(x);
  eval_contact().set_action_type(Mortar::eval_reset);

  // reset displacement state and lagrange multiplier values
  strategy().evaluate(eval_data().contact(), &eval_vec);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Contact::evaluate_force()
{
  check_init_setup();
  bool ok = true;
  // --- evaluate contact contributions ---------------------------------
  eval_contact().set_action_type(Mortar::eval_force);
  eval_data().set_model_evaluator(this);
  strategy().evaluate(eval_data().contact());

  return ok;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Contact::evaluate_stiff()
{
  check_init_setup();
  bool ok = true;
  // --- evaluate contact contributions ---------------------------------
  eval_contact().set_action_type(Mortar::eval_force_stiff);
  eval_data().set_model_evaluator(this);
  strategy().evaluate(eval_data().contact());

  return ok;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Contact::evaluate_force_stiff()
{
  check_init_setup();
  bool ok = true;
  // --- evaluate contact contributions ---------------------------------
  eval_contact().set_action_type(Mortar::eval_force_stiff);
  strategy().evaluate(eval_data().contact());

  return ok;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::pre_evaluate()
{
  eval_contact().set_action_type(Mortar::eval_run_pre_evaluate);
  eval_data().set_model_evaluator(this);
  strategy().evaluate(eval_contact());
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::post_evaluate()
{
  eval_contact().set_action_type(Mortar::eval_run_post_evaluate);
  eval_data().set_model_evaluator(this);
  strategy().evaluate(eval_data().contact());
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Contact::assemble_force(
    Core::LinAlg::Vector<double>& f, const double& timefac_np) const
{
  Teuchos::RCP<const Core::LinAlg::Vector<double>> block_vec_ptr = Teuchos::null;

  if (Teuchos::getIntegralValue<Inpar::Mortar::AlgorithmType>(strategy().params(), "ALGORITHM") ==
          Inpar::Mortar::algorithm_gpts ||
      strategy().is_penalty() || strategy().is_condensed_system())
  {
    block_vec_ptr = strategy().get_rhs_block_ptr(CONTACT::VecBlockType::displ);

    // if there are no active contact contributions, we can skip this...
    if (block_vec_ptr.is_null()) return true;

    Core::LinAlg::assemble_my_vector(1.0, f, timefac_np, *block_vec_ptr);
  }
  else
  {
    // --- displ. - block ---------------------------------------------------
    block_vec_ptr = strategy().get_rhs_block_ptr(CONTACT::VecBlockType::displ);
    // if there are no active contact contributions, we can skip this...
    if (block_vec_ptr.is_null()) return true;
    Core::LinAlg::assemble_my_vector(1.0, f, timefac_np, *block_vec_ptr);

    // --- constr. - block --------------------------------------------------
    block_vec_ptr = strategy().get_rhs_block_ptr(CONTACT::VecBlockType::constraint);
    if (block_vec_ptr.is_null()) return true;
    Core::LinAlg::Vector<double> tmp(f.Map());
    Core::LinAlg::export_to(*block_vec_ptr, tmp);
    f.Update(1., tmp, 1.);
  }

  return true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Contact::assemble_jacobian(
    Core::LinAlg::SparseOperator& jac, const double& timefac_np) const
{
  Teuchos::RCP<Core::LinAlg::SparseMatrix> block_ptr = Teuchos::null;
  int err = 0;
  // ---------------------------------------------------------------------
  // Penalty / gpts / Nitsche system: no additional/condensed dofs
  // ---------------------------------------------------------------------
  if (Teuchos::getIntegralValue<Inpar::Mortar::AlgorithmType>(strategy().params(), "ALGORITHM") ==
          Inpar::Mortar::algorithm_gpts ||
      strategy().is_penalty())
  {
    block_ptr =
        strategy().get_matrix_block_ptr(CONTACT::MatBlockType::displ_displ, &eval_contact());
    if (strategy().is_penalty() && block_ptr.is_null()) return true;
    Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd = global_state().extract_displ_block(jac);
    jac_dd->add(*block_ptr, false, timefac_np, 1.0);
  }
  // ---------------------------------------------------------------------
  // condensed system of equations
  // ---------------------------------------------------------------------
  else if (strategy().is_condensed_system())
  {
    // --- Kdd - block ---------------------------------------------------
    block_ptr =
        strategy().get_matrix_block_ptr(CONTACT::MatBlockType::displ_displ, &eval_contact());
    if (not block_ptr.is_null())
    {
      Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd_ptr = global_state().extract_displ_block(jac);
      jac_dd_ptr->add(*block_ptr, false, timefac_np, 1.0);
      // reset the block pointers, just to be on the safe side
      block_ptr = Teuchos::null;
    }
  }
  // ---------------------------------------------------------------------
  // saddle-point system of equations or no contact contributions
  // ---------------------------------------------------------------------
  else if (strategy().system_type() == Inpar::CONTACT::system_saddlepoint)
  {
    // --- Kdd - block ---------------------------------------------------
    block_ptr =
        strategy().get_matrix_block_ptr(CONTACT::MatBlockType::displ_displ, &eval_contact());
    if (not block_ptr.is_null())
    {
      Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd_ptr = global_state().extract_displ_block(jac);
      jac_dd_ptr->add(*block_ptr, false, timefac_np, 1.0);
      // reset the block pointers, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kdz - block ---------------------------------------------------
    block_ptr = strategy().get_matrix_block_ptr(CONTACT::MatBlockType::displ_lm, &eval_contact());
    if (not block_ptr.is_null())
    {
      block_ptr->scale(timefac_np);
      global_state().assign_model_block(jac, *block_ptr, type(), Solid::MatBlockType::displ_lm);
      // reset the block pointer, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kzd - block ---------------------------------------------------
    block_ptr = strategy().get_matrix_block_ptr(CONTACT::MatBlockType::lm_displ, &eval_contact());
    if (not block_ptr.is_null())
    {
      global_state().assign_model_block(jac, *block_ptr, type(), Solid::MatBlockType::lm_displ);
      // reset the block pointer, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kzz - block ---------------------------------------------------
    block_ptr = strategy().get_matrix_block_ptr(CONTACT::MatBlockType::lm_lm, &eval_contact());
    if (not block_ptr.is_null())
    {
      global_state().assign_model_block(jac, *block_ptr, type(), Solid::MatBlockType::lm_lm);
    }
    /* if there are no active contact contributions, we put a identity
     * matrix at the (lm,lm)-block */
    else
    {
      Teuchos::RCP<Core::LinAlg::Vector<double>> ones =
          Teuchos::make_rcp<Core::LinAlg::Vector<double>>(global_state().block_map(type()), false);
      err = ones->PutScalar(1.0);
      block_ptr = Teuchos::make_rcp<Core::LinAlg::SparseMatrix>(*ones);
      global_state().assign_model_block(jac, *block_ptr, type(), Solid::MatBlockType::lm_lm);
    }
    // reset the block pointer, just to be on the safe side
    block_ptr = Teuchos::null;
  }

  return (err == 0);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::write_restart(
    Core::IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  // clear cache of maps due to varying vector size
  iowriter.clear_map_cache();

  // quantities to be written for restart
  std::map<std::string, Teuchos::RCP<Core::LinAlg::Vector<double>>> restart_vectors;

  strategy().do_write_restart(restart_vectors, forced_writerestart);

  // write all vectors specified by used strategy
  std::map<std::string, Teuchos::RCP<Core::LinAlg::Vector<double>>>::const_iterator p;
  for (p = restart_vectors.begin(); p != restart_vectors.end(); ++p)
    iowriter.write_vector(p->first, p->second);

  /* ToDo Move this stuff into the DoWriteRestart() routine of the
   * AbstractStrategy as soon as the old structural time integration
   * is gone! */
  if (strategy().lagrange_multiplier_n(true) != Teuchos::null)
    iowriter.write_vector("lagrmultold", strategy().lagrange_multiplier_n(true));

  // since the global output_step_state() routine is not called, if the
  // restart is written, we have to do it here manually.
  output_step_state(iowriter);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::read_restart(Core::IO::DiscretizationReader& ioreader)
{
  eval_contact().set_action_type(Mortar::eval_force_stiff);
  // reader strategy specific stuff
  strategy().do_read_restart(ioreader, global_state().get_dis_n(), eval_data().contact_ptr());
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::update_step_state(const double& timefac_n)
{
  // add the contact forces to the old structural residual state vector
  Teuchos::RCP<const Core::LinAlg::Vector<double>> strcontactrhs_ptr =
      strategy().get_rhs_block_ptr(CONTACT::VecBlockType::displ);
  if (not strcontactrhs_ptr.is_null())
  {
    Teuchos::RCP<Core::LinAlg::Vector<double>>& fstructold_ptr =
        global_state().get_fstructure_old();
    fstructold_ptr->Update(timefac_n, *strcontactrhs_ptr, 1.0);
  }

  /* Note: DisN() and dis_np() have the same value at this stage, since
   * we call the structural model evaluator always in first place! */
  strategy_ptr_->update(global_state().get_dis_n());

  post_update_step_state();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::post_update_step_state()
{
  // initialize integration time for time measurement
  strategy_ptr_->inttime_init();

  // redistribute contact
  strategy_ptr_->redistribute_contact(global_state().get_dis_n(), global_state().get_vel_n());

  // setup the map extractor, since redistribute calls fill_complete
  // on the structural discretization. Though this only changes the
  // ghosted dofs while keeping row distribution fixed, the map pointers
  // in the global state are no longer valid. So we reset them.
  integrator().model_eval().setup_multi_map_extractor();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::update_step_element()
{ /* empty */
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::run_post_compute_x(const Core::LinAlg::Vector<double>& xold,
    const Core::LinAlg::Vector<double>& dir, const Core::LinAlg::Vector<double>& xnew)
{
  check_init_setup();

  std::vector<Teuchos::RCP<const Core::LinAlg::Vector<double>>> eval_vec(3, Teuchos::null);
  eval_vec[0] = Teuchos::rcpFromRef(xold);
  eval_vec[1] = Teuchos::rcpFromRef(dir);
  eval_vec[2] = Teuchos::rcpFromRef(xnew);

  eval_contact().set_action_type(Mortar::eval_run_post_compute_x);

  strategy().evaluate(eval_data().contact(), &eval_vec);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::determine_stress_strain()
{
  // evaluate contact tractions
  strategy().compute_contact_stresses();

  if (strategy().weighted_wear())
  {
    /* *******************************************************************
     * We do not compute the non-weighted wear here. we just write
     * the output. the non-weighted wear will be used as dirichlet-b.
     * for the ale problem. n.w.wear will be called in
     * stru_ale_algorithm.cpp and computed in strategy.OutputWear();
     *                                                         farah 06/13
     * ******************************************************************/
    // evaluate wear (not weighted)
    strategy().reset_wear();
    strategy().output_wear();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::determine_energy() {}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::determine_optional_quantity() {}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::output_step_state(
    Core::IO::DiscretizationWriter& iowriter) const
{
  // no output in nitsche Strategy
  if (strategy().is_nitsche()) return;

  // *********************************************************************
  // print summary of active set to screen
  // *********************************************************************
  strategy().print_active_set();

  // *********************************************************************
  // active contact set and slip set
  // *********************************************************************

  // evaluate active set and slip set
  Teuchos::RCP<Core::LinAlg::Vector<double>> activeset =
      Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*strategy().active_row_nodes());
  activeset->PutScalar(1.0);
  if (strategy().is_friction())
  {
    Teuchos::RCP<Core::LinAlg::Vector<double>> slipset =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*strategy().slip_row_nodes());
    slipset->PutScalar(1.0);
    Teuchos::RCP<Core::LinAlg::Vector<double>> slipsetexp =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*strategy().active_row_nodes());
    Core::LinAlg::export_to(*slipset, *slipsetexp);
    activeset->Update(1.0, *slipsetexp, 1.0);
  }

  // export to problem node row map
  Teuchos::RCP<const Epetra_Map> problemnodes = strategy().problem_nodes();
  Teuchos::RCP<Core::LinAlg::Vector<double>> activesetexp =
      Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*problemnodes);
  Core::LinAlg::export_to(*activeset, *activesetexp);

  if (strategy().wear_both_discrete())
  {
    Teuchos::RCP<Core::LinAlg::Vector<double>> mactiveset =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*strategy().master_active_nodes());
    mactiveset->PutScalar(1.0);
    Teuchos::RCP<Core::LinAlg::Vector<double>> slipset =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*strategy().master_slip_nodes());
    slipset->PutScalar(1.0);
    Teuchos::RCP<Core::LinAlg::Vector<double>> slipsetexp =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*strategy().master_active_nodes());
    Core::LinAlg::export_to(*slipset, *slipsetexp);
    mactiveset->Update(1.0, *slipsetexp, 1.0);

    Teuchos::RCP<Core::LinAlg::Vector<double>> mactivesetexp =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*problemnodes);
    Core::LinAlg::export_to(*mactiveset, *mactivesetexp);
    activesetexp->Update(1.0, *mactivesetexp, 1.0);
  }

  iowriter.write_vector("activeset", activesetexp, Core::IO::nodevector);

  // *********************************************************************
  // contact tractions
  // *********************************************************************

  // export to problem dof row map
  Teuchos::RCP<const Epetra_Map> problemdofs = strategy().problem_dofs();

  // normal direction
  Teuchos::RCP<const Core::LinAlg::Vector<double>> normalstresses =
      strategy().contact_normal_stress();
  Teuchos::RCP<Core::LinAlg::Vector<double>> normalstressesexp =
      Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*problemdofs);
  Core::LinAlg::export_to(*normalstresses, *normalstressesexp);

  // tangential plane
  Teuchos::RCP<const Core::LinAlg::Vector<double>> tangentialstresses =
      strategy().contact_tangential_stress();
  Teuchos::RCP<Core::LinAlg::Vector<double>> tangentialstressesexp =
      Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*problemdofs);
  Core::LinAlg::export_to(*tangentialstresses, *tangentialstressesexp);

  // write to output
  // contact tractions in normal and tangential direction
  iowriter.write_vector("norcontactstress", normalstressesexp);
  iowriter.write_vector("tancontactstress", tangentialstressesexp);

  // *********************************************************************
  // wear with internal state variable approach
  // *********************************************************************
  if (strategy().weighted_wear())
  {
    // write output
    Teuchos::RCP<const Core::LinAlg::Vector<double>> wearoutput = strategy().contact_wear();
    Teuchos::RCP<Core::LinAlg::Vector<double>> wearoutputexp =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*problemdofs);
    Core::LinAlg::export_to(*wearoutput, *wearoutputexp);
    iowriter.write_vector("wear", wearoutputexp);
  }

  // *********************************************************************
  // poro contact
  // *********************************************************************
  if (strategy().has_poro_no_penetration())
  {
    // output of poro no penetration lagrange multiplier!
    const CONTACT::LagrangeStrategyPoro& poro_strategy =
        dynamic_cast<const CONTACT::LagrangeStrategyPoro&>(strategy());
    Teuchos::RCP<const Core::LinAlg::Vector<double>> lambdaout = poro_strategy.lambda_no_pen();
    Teuchos::RCP<Core::LinAlg::Vector<double>> lambdaoutexp =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*problemdofs);
    Core::LinAlg::export_to(*lambdaout, *lambdaoutexp);
    iowriter.write_vector("poronopen_lambda", lambdaoutexp);
  }

  /// general way to write the output corresponding to the active strategy
  strategy().write_output(iowriter);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::reset_step_state()
{
  check_init_setup();

  FOUR_C_THROW("Not yet implemented");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Solid::ModelEvaluator::ContactData& Solid::ModelEvaluator::Contact::eval_contact()
{
  check_init_setup();
  return *eval_contact_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const Solid::ModelEvaluator::ContactData& Solid::ModelEvaluator::Contact::eval_contact() const
{
  check_init_setup();
  return *eval_contact_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const Teuchos::RCP<CONTACT::AbstractStrategy>& Solid::ModelEvaluator::Contact::strategy_ptr()
{
  check_init_setup();
  return strategy_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
CONTACT::AbstractStrategy& Solid::ModelEvaluator::Contact::strategy()
{
  check_init_setup();
  return *strategy_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const CONTACT::AbstractStrategy& Solid::ModelEvaluator::Contact::strategy() const
{
  check_init_setup();
  return *strategy_ptr_;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> Solid::ModelEvaluator::Contact::get_block_dof_row_map_ptr() const
{
  Global::Problem* problem = Global::Problem::instance();

  check_init_setup();
  if (strategy().lm_dof_row_map_ptr(false) == Teuchos::null)
    return global_state().dof_row_map();
  else
  {
    auto systype = Teuchos::getIntegralValue<Inpar::CONTACT::SystemType>(
        problem->contact_dynamic_params(), "SYSTEM");

    if (systype == Inpar::CONTACT::system_saddlepoint)
      return strategy().lin_system_lm_dof_row_map_ptr();
    else
      return global_state().dof_row_map();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Core::LinAlg::Vector<double>>
Solid::ModelEvaluator::Contact::get_current_solution_ptr() const
{
  // TODO: this should be removed!
  Global::Problem* problem = Global::Problem::instance();
  auto systype = Teuchos::getIntegralValue<Inpar::CONTACT::SystemType>(
      problem->contact_dynamic_params(), "SYSTEM");
  if (systype == Inpar::CONTACT::system_condensed) return Teuchos::null;

  if (strategy().lagrange_multiplier_np(false) != Teuchos::null)
  {
    Teuchos::RCP<Core::LinAlg::Vector<double>> curr_lm_ptr =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*strategy().lagrange_multiplier_np(false));
    if (not curr_lm_ptr.is_null()) curr_lm_ptr->ReplaceMap(strategy().lm_dof_row_map(false));

    extend_lagrange_multiplier_domain(curr_lm_ptr);

    return curr_lm_ptr;
  }
  else
    return Teuchos::null;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Core::LinAlg::Vector<double>>
Solid::ModelEvaluator::Contact::get_last_time_step_solution_ptr() const
{
  Global::Problem* problem = Global::Problem::instance();
  auto systype = Teuchos::getIntegralValue<Inpar::CONTACT::SystemType>(
      problem->contact_dynamic_params(), "SYSTEM");
  if (systype == Inpar::CONTACT::system_condensed) return Teuchos::null;

  if (strategy().lagrange_multiplier_n(false).is_null()) return Teuchos::null;

  Teuchos::RCP<Core::LinAlg::Vector<double>> old_lm_ptr =
      Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*strategy().lagrange_multiplier_n(false));
  if (not old_lm_ptr.is_null()) old_lm_ptr->ReplaceMap(strategy().lm_dof_row_map(false));

  extend_lagrange_multiplier_domain(old_lm_ptr);

  return old_lm_ptr;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::extend_lagrange_multiplier_domain(
    Teuchos::RCP<Core::LinAlg::Vector<double>>& lm_vec) const
{
  // default case: do nothing
  if (strategy().lm_dof_row_map(false).NumGlobalElements() ==
      get_block_dof_row_map_ptr()->NumGlobalElements())
    return;

  if (strategy().lm_dof_row_map(false).NumGlobalElements() <
      get_block_dof_row_map_ptr()->NumGlobalElements())
  {
    Teuchos::RCP<Core::LinAlg::Vector<double>> tmp_ptr =
        Teuchos::make_rcp<Core::LinAlg::Vector<double>>(*get_block_dof_row_map_ptr());
    Core::LinAlg::export_to(*lm_vec, *tmp_ptr);
    lm_vec = tmp_ptr;
  }
  else
    FOUR_C_THROW("Unconsidered case.");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::post_output()
{
  check_init_setup();
  // empty
}  // post_output()

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::run_pre_compute_x(const Core::LinAlg::Vector<double>& xold,
    Core::LinAlg::Vector<double>& dir_mutable, const NOX::Nln::Group& curr_grp)
{
  check_init_setup();

  std::vector<Teuchos::RCP<const Core::LinAlg::Vector<double>>> eval_vec(1, Teuchos::null);
  eval_vec[0] = Teuchos::rcpFromRef(xold);

  std::vector<Teuchos::RCP<Core::LinAlg::Vector<double>>> eval_vec_mutable(1, Teuchos::null);
  eval_vec_mutable[0] = Teuchos::rcpFromRef(dir_mutable);

  eval_contact().set_action_type(Mortar::eval_run_pre_compute_x);
  eval_data().set_model_evaluator(this);

  // augment the search direction
  strategy().evaluate(eval_data().contact(), &eval_vec, &eval_vec_mutable);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::run_post_iterate(const ::NOX::Solver::Generic& solver)
{
  check_init_setup();

  const ::NOX::Epetra::Vector& nox_x =
      dynamic_cast<const ::NOX::Epetra::Vector&>(solver.getSolutionGroup().getX());

  // displacement vector after the predictor call
  Teuchos::RCP<Core::LinAlg::Vector<double>> curr_disp =
      global_state().extract_displ_entries(Core::LinAlg::Vector<double>(nox_x.getEpetraVector()));
  Teuchos::RCP<const Core::LinAlg::Vector<double>> curr_vel = global_state().get_vel_np();

  if (strategy().dyn_redistribute_contact(curr_disp, curr_vel, solver.getNumIterations()))
    evaluate_force();

  eval_contact().set_action_type(Mortar::eval_run_post_iterate);
  strategy().evaluate(eval_data().contact());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::run_pre_apply_jacobian_inverse(
    const Core::LinAlg::Vector<double>& rhs, Core::LinAlg::Vector<double>& result,
    const Core::LinAlg::Vector<double>& xold, const NOX::Nln::Group& grp)
{
  Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd = global_state().jacobian_displ_block();
  const_cast<CONTACT::AbstractStrategy&>(strategy())
      .run_pre_apply_jacobian_inverse(jac_dd, const_cast<Core::LinAlg::Vector<double>&>(rhs));
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::run_pre_solve(const ::NOX::Solver::Generic& solver)
{
  check_init_setup();
  const ::NOX::Epetra::Vector& nox_x =
      dynamic_cast<const ::NOX::Epetra::Vector&>(solver.getSolutionGroup().getX());

  // displacement vector after the predictor call
  Teuchos::RCP<Core::LinAlg::Vector<double>> curr_disp =
      global_state().extract_displ_entries(Core::LinAlg::Vector<double>(nox_x.getEpetraVector()));

  std::vector<Teuchos::RCP<const Core::LinAlg::Vector<double>>> eval_vec(1, Teuchos::null);
  eval_vec[0] = curr_disp;

  eval_contact().set_action_type(Mortar::eval_run_pre_solve);
  strategy().evaluate(eval_data().contact(), &eval_vec);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::run_post_apply_jacobian_inverse(
    const Core::LinAlg::Vector<double>& rhs, Core::LinAlg::Vector<double>& result,
    const Core::LinAlg::Vector<double>& xold, const NOX::Nln::Group& grp)
{
  check_init_setup();

  eval_contact().set(&rhs, 0);
  eval_contact().set(&result, 1);
  eval_contact().set(&xold, 2);
  eval_contact().set(&grp, 3);

  eval_contact().set_action_type(Mortar::eval_run_post_apply_jacobian_inverse);
  eval_data().set_model_evaluator(this);

  // augment the search direction
  strategy().evaluate(eval_data().contact());

  // clear the set any data again
  eval_contact().clear_all(Core::Gen::AnyDataContainer::DataType::any);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<const Core::LinAlg::SparseMatrix> Solid::ModelEvaluator::Contact::get_jacobian_block(
    const MatBlockType bt) const
{
  return global_state().get_jacobian_block(type(), bt);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<Core::LinAlg::Vector<double>> Solid::ModelEvaluator::Contact::assemble_force_of_models(
    const std::vector<Inpar::Solid::ModelType>* without_these_models, const bool apply_dbc) const
{
  Teuchos::RCP<::NOX::Epetra::Vector> force_nox = global_state().create_global_vector();
  {
    Core::LinAlg::VectorView force_view(force_nox->getEpetraVector());
    integrator().assemble_force(force_view, without_these_models);
  }

  // copy the vector, otherwise the storage will be freed at the end of this
  // function, resulting in a segmentation fault
  Teuchos::RCP<Core::LinAlg::Vector<double>> force =
      Teuchos::make_rcp<Core::LinAlg::Vector<double>>(force_nox->getEpetraVector());

  if (apply_dbc) tim_int().get_dbc().apply_dirichlet_to_rhs(force);

  return force;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<Core::LinAlg::SparseOperator> Solid::ModelEvaluator::Contact::get_aux_displ_jacobian()
    const
{
  std::vector<Inpar::Solid::ModelType> g;
  g.push_back(Inpar::Solid::ModelType::model_contact);

  Teuchos::RCP<Core::LinAlg::SparseOperator> jacaux = global_state().create_aux_jacobian();
  bool ok = integrator().assemble_jac(*jacaux, &g);

  if (!ok) FOUR_C_THROW("ERROR: create_aux_jacobian went wrong!");

  return jacaux;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::evaluate_weighted_gap_gradient_error()
{
  eval_contact().set_action_type(Mortar::eval_wgap_gradient_error);
  eval_data().set_model_evaluator(this);

  // augment the search direction
  strategy().evaluate(eval_data().contact());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Contact::evaluate_cheap_soc_rhs()
{
  check_init_setup();

  eval_contact().set_action_type(Mortar::eval_static_constraint_rhs);
  eval_data().set_model_evaluator(this);

  strategy().evaluate(eval_data().contact());

  return true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Contact::assemble_cheap_soc_rhs(
    Core::LinAlg::Vector<double>& f, const double& timefac_np) const
{
  return assemble_force(f, timefac_np);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool Solid::ModelEvaluator::Contact::correct_parameters(NOX::Nln::CorrectionType type)
{
  check_init_setup();

  eval_contact().set_action_type(Mortar::eval_correct_parameters);
  eval_contact().set(&type, 0);

  strategy().evaluate(eval_contact());

  eval_contact().clear_entry(Core::Gen::AnyDataContainer::DataType::any, 0);

  return true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void Solid::ModelEvaluator::Contact::remove_condensed_contributions_from_rhs(
    Core::LinAlg::Vector<double>& rhs)
{
  check_init_setup();
  eval_contact().set_action_type(Mortar::remove_condensed_contributions_from_str_rhs);

  std::vector<Teuchos::RCP<Core::LinAlg::Vector<double>>> mutable_vec(1, Teuchos::rcpFromRef(rhs));
  strategy().evaluate(eval_contact(), nullptr, &mutable_vec);
}

FOUR_C_NAMESPACE_CLOSE

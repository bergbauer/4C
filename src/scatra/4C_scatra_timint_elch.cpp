/*----------------------------------------------------------------------*/
/*! \file

\brief scatra time integration for elch

\level 2

 *------------------------------------------------------------------------------------------------*/
#include "4C_scatra_timint_elch.hpp"

#include "4C_fem_nurbs_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_io.hpp"
#include "4C_io_control.hpp"
#include "4C_linalg_equilibrate.hpp"
#include "4C_linalg_krylov_projector.hpp"
#include "4C_linalg_matrixtransform.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_mat_ion.hpp"
#include "4C_mat_list.hpp"
#include "4C_rebalance_binning_based.hpp"
#include "4C_scatra_ele_action.hpp"
#include "4C_scatra_resulttest_elch.hpp"
#include "4C_scatra_timint_elch_service.hpp"
#include "4C_scatra_timint_meshtying_strategy_fluid_elch.hpp"
#include "4C_scatra_timint_meshtying_strategy_s2i_elch.hpp"
#include "4C_scatra_timint_meshtying_strategy_std_elch.hpp"
#include "4C_utils_function_of_time.hpp"
#include "4C_utils_parameter_list.hpp"

#include <unordered_set>

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
ScaTra::ScaTraTimIntElch::ScaTraTimIntElch(Teuchos::RCP<Core::FE::Discretization> dis,
    Teuchos::RCP<Core::LinAlg::Solver> solver, Teuchos::RCP<Teuchos::ParameterList> params,
    Teuchos::RCP<Teuchos::ParameterList> sctratimintparams,
    Teuchos::RCP<Teuchos::ParameterList> extraparams,
    Teuchos::RCP<Core::IO::DiscretizationWriter> output)
    : ScaTraTimIntImpl(dis, solver, sctratimintparams, extraparams, output),
      elchparams_(params),
      equpot_(Core::UTILS::IntegralValue<Inpar::ElCh::EquPot>(*elchparams_, "EQUPOT")),
      fr_(elchparams_->get<double>("FARADAY_CONSTANT") / elchparams_->get<double>("GAS_CONSTANT")),
      temperature_funct_num_(elchparams_->get<int>("TEMPERATURE_FROM_FUNCT")),
      temperature_(get_current_temperature()),
      gstatnumite_(0),
      gstatincrement_(0.),
      dlcapexists_(false),
      ektoggle_(Teuchos::null),
      dctoggle_(Teuchos::null),
      electrodeinitvols_(),
      electrodesoc_(),
      electrodecrates_(),
      electrodeconc_(),
      electrodeeta_(),
      electrodecurr_(),
      electrodevoltage_(),
      cellvoltage_(0.),
      cellvoltage_old_(-1.),
      cccv_condition_(Teuchos::null),
      cellcrate_(0.),
      cellcrate_old_(-1.0),
      cycling_timestep_(Core::UTILS::IntegralValue<bool>(*params_, "ADAPTIVE_TIMESTEPPING")
                            ? elchparams_->get<double>("CYCLING_TIMESTEP")
                            : 0.0),
      adapted_timestep_active_(false),
      dt_adapted_(-1.0),
      last_dt_change_(0),
      splitter_macro_(Teuchos::null)
{
  // safety check
  if (fr_ <= 0.0) FOUR_C_THROW("Factor F/R is non-positive!");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::Init()
{
  // The diffusion-conduction formulation does not support all options of the Nernst-Planck
  // formulation Let's check for valid options
  if (Core::UTILS::IntegralValue<int>(*elchparams_, "DIFFCOND_FORMULATION"))
    valid_parameter_diff_cond();

  // additional safety checks associated with adaptive time stepping for CCCV cell cycling
  if (cycling_timestep_ > 0.0)
  {
    if (not discret_->GetCondition("CCCVCycling"))
    {
      FOUR_C_THROW(
          "Adaptive time stepping for CCCV cell cycling requires corresponding boundary "
          "condition!");
    }
    if (cycling_timestep_ >= dta_)
    {
      FOUR_C_THROW(
          "Adaptive time stepping for CCCV cell cycling requires that the modified time step size "
          "is smaller than the original time step size!");
    }
  }

  if ((elchparams_->get<double>("TEMPERATURE") != 298.0) and (temperature_funct_num_ != -1))
  {
    FOUR_C_THROW(
        "You set two methods to calculate the temperature in your Input-File. This is not "
        "reasonable! It is only allowed to set either 'TEMPERATURE' or 'TEMPERATURE_FROM_FUNCT'");
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::SetupSplitter()
{
  // set up concentration-potential splitter
  setup_conc_pot_split();

  // set up concentration-potential-potential splitter for macro scale in multi-scale simulations
  if (macro_scale_) setup_conc_pot_pot_split();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::Setup()
{
  // set up concentration-potential splitter
  SetupSplitter();

  // initialize time-dependent electrode kinetics variables (galvanostatic mode or double layer
  // contribution)
  compute_time_deriv_pot0(true);

  // initialize dirichlet toggle:
  // for certain ELCH problem formulations we have to provide
  // additional flux terms / currents across Dirichlet boundaries for the standard element call
  Teuchos::RCP<Epetra_Vector> dirichones =
      Core::LinAlg::CreateVector(*(dbcmaps_->CondMap()), false);
  dirichones->PutScalar(1.0);
  dctoggle_ = Core::LinAlg::CreateVector(*(discret_->dof_row_map()), true);
  dbcmaps_->InsertCondVector(dirichones, dctoggle_);

  // screen output (has to come after SetInitialField)
  // a safety check for the solver type
  if ((NumScal() > 1) && (solvtype_ != Inpar::ScaTra::solvertype_nonlinear))
    FOUR_C_THROW("Solver type has to be set to >>nonlinear<< for ion transport.");

  if (myrank_ == 0)
  {
    std::cout << "\nSetup of splitter: numscal = " << NumScal() << std::endl;
    std::cout << "Constant F/R = " << fr_ << std::endl;
  }

  // initialize vectors for states of charge and C rates of resolved electrodes
  {
    std::vector<Core::Conditions::Condition*> electrode_soc_conditions;
    discret_->GetCondition("ElectrodeSOC", electrode_soc_conditions);
    for (const auto& electrodeSocCondition : electrode_soc_conditions)
    {
      const int cond_id = electrodeSocCondition->parameters().Get<int>("ConditionID");
      auto conditioninitpair = std::make_pair(cond_id, -1.0);
      if (isale_) electrodeinitvols_.insert(conditioninitpair);
      electrodesoc_.insert(conditioninitpair);
      electrodecrates_.insert(conditioninitpair);
      runtime_csvwriter_soc_.insert(std::make_pair(cond_id, std::nullopt));
      runtime_csvwriter_soc_[cond_id].emplace(
          myrank_, *DiscWriter()->output(), "electrode_soc_" + std::to_string(cond_id));
      runtime_csvwriter_soc_[cond_id]->register_data_vector("SOC", 1, 16);
      runtime_csvwriter_soc_[cond_id]->register_data_vector("CRate", 1, 16);

      // safety checks
      const double one_hour = electrodeSocCondition->parameters().Get<double>("one_hour");
      if (one_hour <= 0.0) FOUR_C_THROW("One hour must not be negative");
      if (std::fmod(std::log10(one_hour / 3600.0), 1.0) != 0)
        FOUR_C_THROW("This is not one hour in SI units");
      if (electrode_soc_conditions[0]->parameters().Get<double>("one_hour") != one_hour)
        FOUR_C_THROW("Different definitions of one hour in Electrode STATE OF CHARGE CONDITIONS.");
    }
  }

  // init map for electrode voltage
  {
    std::vector<Core::Conditions::Condition*> conditions;
    discret_->GetCondition("CellVoltage", conditions);
    std::vector<Core::Conditions::Condition*> conditionspoint;
    discret_->GetCondition("CellVoltagePoint", conditionspoint);
    if (!conditions.empty() and !conditionspoint.empty())
    {
      FOUR_C_THROW(
          "Cannot have cell voltage line/surface conditions and cell voltage point conditions at "
          "the same time!");
    }
    else if (!conditionspoint.empty())
      conditions = conditionspoint;

    // perform all following operations only if there is at least one condition for cell voltage
    if (!conditions.empty())
    {
      // safety check
      if (conditions.size() != 2)
        FOUR_C_THROW(
            "Must have exactly two boundary conditions for cell voltage, one per electrode!");

      // loop over both conditions for cell voltage
      for (const auto& condition : conditions)
      {
        // extract condition ID
        const int condid = condition->parameters().Get<int>("ConditionID");
        electrodevoltage_.insert({condid, 0.0});
      }
      // setup csv writer for cell voltage
      runtime_csvwriter_cell_voltage_.emplace(myrank_, *DiscWriter()->output(), "cell_voltage");
      runtime_csvwriter_cell_voltage_->register_data_vector("CellVoltage", 1, 16);
    }
  }

  // initialize vectors for mean reactant concentrations, mean electric overpotentials, and total
  // electric currents at electrode boundaries
  std::vector<Core::Conditions::Condition*> electrodedomainconditions;
  discret_->GetCondition("ElchDomainKinetics", electrodedomainconditions);
  std::vector<Core::Conditions::Condition*> electrodeboundaryconditions;
  discret_->GetCondition("ElchBoundaryKinetics", electrodeboundaryconditions);
  std::vector<Core::Conditions::Condition*> electrodeboundarypointconditions;
  discret_->GetCondition("ElchBoundaryKineticsPoint", electrodeboundarypointconditions);
  if (!electrodedomainconditions.empty() and
      (!electrodeboundaryconditions.empty() or !electrodeboundarypointconditions.empty()))
  {
    FOUR_C_THROW(
        "At the moment, we cannot have electrode domain kinetics conditions and electrode boundary "
        "kinetics conditions at the same time!");
  }
  else if (!electrodeboundaryconditions.empty() and !electrodeboundarypointconditions.empty())
  {
    FOUR_C_THROW(
        "At the moment, we cannot have electrode boundary kinetics line/surface conditions and "
        "electrode boundary kinetics point conditions at the same time!");
  }
  else if (!electrodedomainconditions.empty() or !electrodeboundaryconditions.empty() or
           !electrodeboundarypointconditions.empty())
  {
    // group electrode conditions from all entities into one vector and loop
    std::vector<std::vector<Core::Conditions::Condition*>> electrodeconditions = {
        electrodedomainconditions, electrodeboundaryconditions, electrodeboundarypointconditions};
    for (const auto& electrodeentityconditions : electrodeconditions)
    {
      for (const auto& electrodedomaincondition : electrodeentityconditions)
      {
        auto condition_pair =
            std::make_pair(electrodedomaincondition->parameters().Get<int>("ConditionID"), -1.0);
        electrodeconc_.insert(condition_pair);
        electrodeeta_.insert(condition_pair);
        electrodecurr_.insert(condition_pair);
      }
    }
  }

  // extract constant-current constant-voltage (CCCV) cell cycling and half-cycle boundary
  // conditions
  std::vector<Core::Conditions::Condition*> cccvcyclingconditions;
  discret_->GetCondition("CCCVCycling", cccvcyclingconditions);
  std::vector<Core::Conditions::Condition*> cccvhalfcycleconditions;
  discret_->GetCondition("CCCVHalfCycle", cccvhalfcycleconditions);

  switch (cccvcyclingconditions.size())
  {
    // no cell cycling intended
    case 0:
    {
      // safety check
      if (!cccvhalfcycleconditions.empty())
      {
        FOUR_C_THROW(
            "Found constant-current constant-voltage (CCCV) half-cycle boundary conditions, but no "
            "CCCV cell cycling condition!");
      }

      break;
    }

    // cell cycling intended
    case 1:
    {
      // check if cell voltage condition is given
      std::vector<Core::Conditions::Condition*> cell_voltage_conditions,
          cell_voltage_point_conditions;
      discret_->GetCondition("CellVoltage", cell_voltage_conditions);
      discret_->GetCondition("CellVoltagePoint", cell_voltage_point_conditions);
      if (cell_voltage_conditions.size() == 0 and cell_voltage_point_conditions.size() == 0)
        FOUR_C_THROW(
            "Definition of 'cell voltage' condition required for 'CCCV cell cycling' condition.");

      // extract constant-current constant-voltage (CCCV) cell cycling boundary condition
      const Core::Conditions::Condition& cccvcyclingcondition = *cccvcyclingconditions[0];

      // safety checks
      if (NumDofPerNode() != 2 and NumDofPerNode() != 3)
      {
        FOUR_C_THROW(
            "Must have exactly two (concentration and potential) or three (concentration and "
            "potential, micro potential) degrees of freedom per node .");
      }
      if (cccvhalfcycleconditions.empty())
      {
        FOUR_C_THROW(
            "Found constant-current constant-voltage (CCCV) cell cycling boundary condition, but "
            "no CCCV half-cycle boundary conditions!");
      }
      if (cccvcyclingcondition.parameters().Get<int>("ConditionIDForCharge") < 0 or
          cccvcyclingcondition.parameters().Get<int>("ConditionIDForDischarge") < 0)
      {
        FOUR_C_THROW(
            "Invalid ID of constant-current constant-voltage (CCCV) half-cycle boundary condition "
            "specified in CCCV cell cycling boundary condition!");
      }

      // new cccv condition
      cccv_condition_ = Teuchos::rcp(new ScaTra::CCCVCondition(cccvcyclingcondition,
          cccvhalfcycleconditions,
          Core::UTILS::IntegralValue<bool>(*params_, "ADAPTIVE_TIMESTEPPING"), NumDofPerNode()));

      break;
    }

    // safety check
    default:
    {
      FOUR_C_THROW(
          "More than one constant-current constant-voltage (CCCV) cell cycling boundary "
          "condition is not allowed!");
      break;
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::setup_conc_pot_split()
{
  // prepare sets for concentration (other) and potential (cond) dofs. In case of current as
  // solution variable, the current dofs are also stored in potdofs
  std::vector<int> conc_dofs;
  std::vector<int> pot_dofs;

  for (int inode = 0; inode < discret_->NumMyRowNodes(); ++inode)
  {
    std::vector<int> dofs = discret_->Dof(0, discret_->lRowNode(inode));
    for (unsigned idof = 0; idof < dofs.size(); ++idof)
    {
      if (idof < static_cast<unsigned>(NumScal()))
        conc_dofs.emplace_back(dofs[idof]);
      else
        pot_dofs.emplace_back(dofs[idof]);
    }
  }

  auto concdofmap = Teuchos::rcp(new const Epetra_Map(
      -1, static_cast<int>(conc_dofs.size()), conc_dofs.data(), 0, discret_->Comm()));
  auto potdofmap = Teuchos::rcp(new const Epetra_Map(
      -1, static_cast<int>(pot_dofs.size()), pot_dofs.data(), 0, discret_->Comm()));

  // set up concentration-potential splitter
  splitter_ =
      Teuchos::rcp(new Core::LinAlg::MapExtractor(*discret_->dof_row_map(), potdofmap, concdofmap));
}

/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::setup_conc_pot_pot_split()
{
  // prepare sets for dofs associated with electrolyte concentration, electrolyte potential, and
  // electrode potential
  std::vector<int> conc_dofs;
  std::vector<int> pot_el_dofs;
  std::vector<int> pot_ed_dofs;

  // fill sets
  for (int inode = 0; inode < discret_->NumMyRowNodes(); ++inode)
  {
    std::vector<int> dofs = discret_->Dof(0, discret_->lRowNode(inode));
    for (unsigned idof = 0; idof < dofs.size(); ++idof)
    {
      if (idof < static_cast<unsigned>(NumScal()))
        conc_dofs.emplace_back(dofs[idof]);
      else if (idof == static_cast<unsigned>(NumScal()))
        pot_el_dofs.emplace_back(dofs[idof]);
      else
        pot_ed_dofs.emplace_back(dofs[idof]);
    }
  }

  // transform sets to maps
  std::vector<Teuchos::RCP<const Epetra_Map>> maps(3, Teuchos::null);
  maps[0] = Teuchos::rcp(new Epetra_Map(
      -1, static_cast<int>(conc_dofs.size()), conc_dofs.data(), 0, discret_->Comm()));
  maps[1] = Teuchos::rcp(new Epetra_Map(
      -1, static_cast<int>(pot_el_dofs.size()), pot_el_dofs.data(), 0, discret_->Comm()));
  maps[2] = Teuchos::rcp(new Epetra_Map(
      -1, static_cast<int>(pot_ed_dofs.size()), pot_ed_dofs.data(), 0, discret_->Comm()));

  // set up concentration-potential-potential splitter
  splitter_macro_ =
      Teuchos::rcp(new Core::LinAlg::MultiMapExtractor(*discret_->dof_row_map(), maps));
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::set_element_specific_sca_tra_parameters(
    Teuchos::ParameterList& eleparams) const
{
  // overwrite action type
  if (Core::UTILS::IntegralValue<int>(*elchparams_, "DIFFCOND_FORMULATION"))
  {
    Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
        "action", ScaTra::Action::set_diffcond_scatra_parameter, eleparams);

    // parameters for diffusion-conduction formulation
    eleparams.sublist("DIFFCOND") = elchparams_->sublist("DIFFCOND");
  }
  else
    Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
        "action", ScaTra::Action::set_elch_scatra_parameter, eleparams);

  // general elch parameters
  eleparams.set<double>("faraday", elchparams_->get<double>("FARADAY_CONSTANT"));
  eleparams.set<double>("gas_constant", elchparams_->get<double>("GAS_CONSTANT"));
  eleparams.set<double>("frt", FRT());
  eleparams.set<double>("temperature", temperature_);
  eleparams.set<int>("equpot", equpot_);
  eleparams.set<bool>("boundaryfluxcoupling",
      Core::UTILS::IntegralValue<bool>(*elchparams_, "COUPLE_BOUNDARY_FLUXES"));
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::compute_time_step_size(double& dt)
{
  // call base class routine
  ScaTraTimIntImpl::compute_time_step_size(dt);

  // adaptive time stepping for CCCV if activated
  if (cycling_timestep_ > 0.0)
  {
    // adaptive time stepping for CCCV cell cycling is currently inactive -> Check if it should be
    // activated
    if (!adapted_timestep_active_)
    {
      // check, current phase allows adaptive time stepping
      if (cccv_condition_->is_adaptive_time_stepping_phase())
      {
        // extrapolate step and adapt time step if needed
        double dt_new = extrapolate_state_adapt_time_step(dt);

        // activate adaptive time stepping and set new time step
        if (dt_new != dt)
        {
          // CCCV half cycle was not changed since this time step adaptivity. Thus, reset observer
          // (tracks phase changes)
          cccv_condition_->reset_phase_change_observer();
          adapted_timestep_active_ = true;
          dt_adapted_ = dt = dt_new;
          last_dt_change_ = Step();
        }
      }
    }
    else
    {
      // if time step adaptivity is enabled for more than 3 steps after last change of phase:
      // disable, otherwise keep adapted time step
      if (cccv_condition_->exceed_max_steps_from_last_phase_change(step_))
        adapted_timestep_active_ = false;
      else if (Step() > last_dt_change_ + 3 * std::ceil(static_cast<double>(dt) /
                                                        static_cast<double>(dt_adapted_)))
      {
        adapted_timestep_active_ = false;
        return;
      }
      else
        dt = dt_adapted_;
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
double ScaTra::ScaTraTimIntElch::extrapolate_state_adapt_time_step(double dt)
{
  // new time step;
  double dt_new = dt;

  // extrapolate state depending on current phase and check if it exceeds bounds of current phase.
  // If so, adapt time step
  switch (cccv_condition_->get_cccv_half_cycle_phase())
  {
    case Inpar::ElCh::CCCVHalfCyclePhase::initital_relaxation:
    {
      const double time_new = time_ + 2 * dt;                  // extrapolate
      if (time_new >= cccv_condition_->GetInitialRelaxTime())  // check
      {
        const double timetoend = cccv_condition_->GetInitialRelaxTime() - time_;
        const int stepstoend = std::max(static_cast<int>(std::ceil(timetoend / cycling_timestep_)),
            cccv_condition_->min_time_steps_during_init_relax());
        dt_new = timetoend / stepstoend;  // adapt
      }
      break;
    }
    case Inpar::ElCh::CCCVHalfCyclePhase::constant_current:
    {
      // initialize variable for cell voltage from previous time step
      if (cellvoltage_old_ < 0.0) cellvoltage_old_ = cellvoltage_;

      const double cellvoltage_new =
          cellvoltage_ + 2.0 * (cellvoltage_ - cellvoltage_old_);  // extrapolate
      if (cccv_condition_->ExceedCellVoltage(cellvoltage_new))     // check
      {
        dt_new = cycling_timestep_;  // adapt
        cellvoltage_old_ = -1.0;
      }
      else
        cellvoltage_old_ = cellvoltage_;
      break;
    }
    case Inpar::ElCh::CCCVHalfCyclePhase::constant_voltage:
    {
      if (cellcrate_old_ < 0.0) cellcrate_old_ = cellcrate_;
      const double cellcrate_new = cellcrate_ + 2.0 * (cellcrate_ - cellcrate_old_);  // extrapolate
      if (cccv_condition_->ExceedCellCRate(cellcrate_new))                            // check
      {
        dt_new = cycling_timestep_;  // adapt
        cellcrate_old_ = -1.0;
      }
      else
        cellcrate_old_ = cellcrate_;
      break;
    }
    case Inpar::ElCh::CCCVHalfCyclePhase::relaxation:
    {
      const double time_new = time_ + 2 * dt;              // extrapolate
      if (time_new >= cccv_condition_->GetRelaxEndTime())  // check
      {
        const double timetoend = cccv_condition_->GetRelaxEndTime() - time_;
        const int stepstoend = std::ceil(timetoend / cycling_timestep_);
        dt_new = timetoend / stepstoend;  // adapt
      }
      break;
    }
    default:
    {
      FOUR_C_THROW("Unknown phase of half cycle.");
      break;
    }
  }

  return dt_new;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::add_problem_specific_parameters_and_vectors(
    Teuchos::ParameterList& params  //!< parameter list
)
{
  discret_->set_state("dctoggle", dctoggle_);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::nonlinear_solve()
{
  bool stopgalvanostat(false);
  gstatnumite_ = 1;

  // galvanostatic control (ELCH)
  while (!stopgalvanostat)
  {
    ScaTraTimIntImpl::nonlinear_solve();

    stopgalvanostat = apply_galvanostatic_control();
  }  // end galvanostatic control
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::assemble_mat_and_rhs()
{
  // safety checks
  check_is_init();
  check_is_setup();

  // check for zero or negative concentration values
  check_concentration_values(phinp_);

  // call base class routine
  ScaTraTimIntImpl::assemble_mat_and_rhs();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::prepare_time_loop()
{
  // safety checks
  check_is_init();
  check_is_setup();

  if (step_ == 0)
  {
    // calculate initial electric potential field
    if (Core::UTILS::IntegralValue<int>(*elchparams_, "INITPOTCALC"))
      calc_initial_potential_field();

    // evaluate SOC, c-rate and cell voltage for output
    evaluate_electrode_info_interior();
    evaluate_cell_voltage();
    evaluate_cccv_phase();
  }

  // call base class routine
  ScaTraTimIntImpl::prepare_time_loop();

  // check validity of material and element formulation
  Teuchos::ParameterList eleparams;
  Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
      "action", ScaTra::Action::check_scatra_element_parameter, eleparams);

  discret_->Evaluate(
      eleparams, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);
}

/*------------------------------------------------------------------------------*
 *------------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::prepare_time_step()
{
  // call base class routine
  ScaTraTimIntImpl::prepare_time_step();

  if (temperature_funct_num_ != -1)
  {
    // set the temperature at the beginning of each time step but after the call to the base class
    // as there the time is updated
    temperature_ = compute_temperature_from_function();

    // after the temperature has been adapted, also the scatra element parameters have to be updated
    set_element_general_parameters();
  }
}

/*------------------------------------------------------------------------------*
 *------------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::prepare_first_time_step()
{
  // safety checks
  check_is_init();
  check_is_setup();

  // call base class routine
  ScaTraTimIntImpl::prepare_first_time_step();

  // initialize Nernst boundary conditions
  init_nernst_bc();
}

/*----------------------------------------------------------------------------------------*
 *----------------------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::create_scalar_handler()
{
  scalarhandler_ = Teuchos::rcp(new ScalarHandlerElch());
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::evaluate_error_compared_to_analytical_sol()
{
  switch (calcerror_)
  {
    case Inpar::ScaTra::calcerror_no:  // do nothing (the usual case)
      break;
    case Inpar::ScaTra::calcerror_Kwok_Wu:
    {
      //   References:

      //   Kwok, Yue-Kuen and Wu, Charles C. K.
      //   "Fractional step algorithm for solving a multi-dimensional
      //   diffusion-migration equation"
      //   Numerical Methods for Partial Differential Equations
      //   1995, Vol 11, 389-397

      //   G. Bauer, V. Gravemeier, W.A. Wall, A 3D finite element approach for the coupled
      //   numerical simulation of electrochemical systems and fluid flow,
      //   International Journal for Numerical Methods in Engineering, 86
      //   (2011) 1339-1359. DOI: 10.1002/nme.3107

      // create the parameters for the error calculation
      Teuchos::ParameterList eleparams;
      Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
          "action", ScaTra::Action::calc_error, eleparams);
      eleparams.set("total time", time_);
      eleparams.set<int>("calcerrorflag", calcerror_);

      // set vector values needed by elements
      discret_->set_state("phinp", phinp_);

      // get (squared) error values
      Teuchos::RCP<Core::LinAlg::SerialDenseVector> errors =
          Teuchos::rcp(new Core::LinAlg::SerialDenseVector(3));
      discret_->EvaluateScalars(eleparams, errors);

      double conerr1 = 0.0;
      double conerr2 = 0.0;
      // for the L2 norm, we need the square root
      if (NumScal() == 2)
      {
        conerr1 = sqrt((*errors)[0]);
        conerr2 = sqrt((*errors)[1]);
      }
      else if (NumScal() == 1)
      {
        conerr1 = sqrt((*errors)[0]);
        conerr2 = 0.0;
      }
      else
        FOUR_C_THROW("The analytical solution of Kwok and Wu is only defined for two species");

      double poterr = sqrt((*errors)[2]);

      if (myrank_ == 0)
      {
        printf("\nL2_err for Kwok and Wu (time = %f):\n", time_);
        printf(" concentration1 %15.8e\n concentration2 %15.8e\n potential      %15.8e\n\n",
            conerr1, conerr2, poterr);
      }
    }
    break;
    case Inpar::ScaTra::calcerror_cylinder:
    {
      //   Reference:
      //   G. Bauer, V. Gravemeier, W.A. Wall, A 3D finite element approach for the coupled
      //   numerical simulation of electrochemical systems and fluid flow,
      //   International Journal for Numerical Methods in Engineering, 2011

      // create the parameters for the error calculation
      Teuchos::ParameterList eleparams;
      Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
          "action", ScaTra::Action::calc_error, eleparams);
      eleparams.set("total time", time_);
      eleparams.set<int>("calcerrorflag", calcerror_);

      // set vector values needed by elements
      discret_->set_state("phinp", phinp_);

      // get (squared) error values
      Teuchos::RCP<Core::LinAlg::SerialDenseVector> errors =
          Teuchos::rcp(new Core::LinAlg::SerialDenseVector(3));
      discret_->EvaluateScalars(eleparams, errors);

      // for the L2 norm, we need the square root
      double conerr1 = sqrt((*errors)[0]);
      double conerr2 = sqrt((*errors)[1]);
      double poterr = sqrt((*errors)[2]);

      if (myrank_ == 0)
      {
        printf("\nL2_err for concentric cylinders (time = %f):\n", time_);
        printf(" concentration1 %15.8e\n concentration2 %15.8e\n potential      %15.8e\n\n",
            conerr1, conerr2, poterr);
      }
    }
    break;
    case Inpar::ScaTra::calcerror_electroneutrality:
    {
      // compute L2 norm of electroneutrality condition

      // create the parameters for the error calculation
      Teuchos::ParameterList eleparams;
      Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
          "action", ScaTra::Action::calc_error, eleparams);
      eleparams.set("total time", time_);
      eleparams.set<int>("calcerrorflag", calcerror_);

      // set vector values needed by elements
      discret_->set_state("phinp", phinp_);

      // get (squared) error values
      Teuchos::RCP<Core::LinAlg::SerialDenseVector> errors =
          Teuchos::rcp(new Core::LinAlg::SerialDenseVector(1));
      discret_->EvaluateScalars(eleparams, errors);

      // for the L2 norm, we need the square root
      double err = sqrt((*errors)[0]);

      if (myrank_ == 0)
      {
        printf("\nL2_err for electroneutrality (time = %f):\n", time_);
        printf(" Deviation from ENC: %15.8e\n\n", err);
      }
    }
    break;
    default:
    {
      // call base class routine
      ScaTraTimIntImpl::evaluate_error_compared_to_analytical_sol();
      break;
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::Update()
{
  // perform update of time-dependent electrode variables
  electrode_kinetics_time_update();

  // evaluate SOC, c-rate and cell voltage for output
  evaluate_electrode_info_interior();
  evaluate_cell_voltage();
  evaluate_cccv_phase();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::check_and_write_output_and_restart()
{
  // call base class routine
  ScaTraTimIntImpl::check_and_write_output_and_restart();

  // output electrode interior status information and cell voltage in every time step
  if (Core::UTILS::IntegralValue<int>(*elchparams_, "ELECTRODE_INFO_EVERY_STEP") or IsResultStep())
  {
    // print electrode domain and boundary status information to screen and files
    output_electrode_info_domain();
    output_electrode_info_boundary();

    // print electrode interior status information to screen and files
    output_electrode_info_interior();

    // print cell voltage to screen and file
    OutputCellVoltage();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::output_problem_specific()
{
  // for elch problems with moving boundary
  if (isale_) output_->write_vector("trueresidual", trueresidual_);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::read_restart_problem_specific(
    const int step, Core::IO::DiscretizationReader& reader)
{
  if (isale_) reader.read_vector(trueresidual_, "trueresidual");

  // read restart data associated with electrode state of charge conditions if applicable, needed
  // for correct evaluation of cell C rate at the beginning of the first time step after restart
  if (discret_->GetCondition("ElectrodeSOC"))
  {
    if (isale_)
    {
      // reconstruct map from two vectors (ID of condition [key], volume [value])
      auto conditionid_vec = Teuchos::rcp(new std::vector<int>);
      auto electrodeinitvol_vec = Teuchos::rcp(new std::vector<double>);
      reader.read_redundant_int_vector(conditionid_vec, "electrodeconditionids");
      reader.read_redundant_double_vector(electrodeinitvol_vec, "electrodeinitvols");
      if (conditionid_vec->size() != electrodeinitvol_vec->size())
        FOUR_C_THROW("something went wrong with reading initial volumes of electrodes");
      electrodeinitvols_.clear();
      for (unsigned i = 0; i < conditionid_vec->size(); ++i)
      {
        auto condition_volume = std::make_pair(conditionid_vec->at(i), electrodeinitvol_vec->at(i));
        electrodeinitvols_.insert(condition_volume);
      }
    }
  }

  // extract constant-current constant-voltage (CCCV) cell cycling boundary condition if available
  const Core::Conditions::Condition* const cccvcyclingcondition =
      discret_->GetCondition("CCCVCycling");

  // read restart data associated with constant-current constant-voltage (CCCV) cell cycling if
  // applicable
  if (cccvcyclingcondition)
  {
    cellvoltage_ = reader.read_double("cellvoltage");
    cellcrate_ = reader.read_double("cellcrate");
    adapted_timestep_active_ = static_cast<bool>(reader.read_int("adapted_timestep_active"));
    dt_adapted_ = reader.read_double("dt_adapted");
    last_dt_change_ = reader.read_int("last_dt_change");

    // read restart of cccv condition
    cccv_condition_->read_restart(reader);
  }

  std::vector<Core::Conditions::Condition*> s2ikinetics_conditions(0, nullptr);
  discretization()->GetCondition("S2IKinetics", s2ikinetics_conditions);
  for (auto* s2ikinetics_cond : s2ikinetics_conditions)
  {
    // only slave side has relevant information
    if (s2ikinetics_cond->parameters().Get<int>("interface side") ==
            static_cast<int>(Inpar::S2I::side_slave) and
        s2ikinetics_cond->parameters().Get<int>("kinetic model") ==
            static_cast<int>(Inpar::S2I::kinetics_butlervolmerreducedcapacitance))
    {
      reader.read_vector(phidtnp_, "phidtnp");
      break;
    }
  }
}

/*------------------------------------------------------------------------------*
 *------------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::output_electrode_info_boundary()
{
  // extract electrode boundary kinetics conditions from discretization
  std::vector<Teuchos::RCP<Core::Conditions::Condition>> cond;
  discret_->GetCondition("ElchBoundaryKinetics", cond);
  std::vector<Teuchos::RCP<Core::Conditions::Condition>> pointcond;
  discret_->GetCondition("ElchBoundaryKineticsPoint", pointcond);

  // safety check
  if (!cond.empty() and !pointcond.empty())
  {
    FOUR_C_THROW(
        "Cannot have electrode boundary kinetics point conditions and electrode boundary "
        "kinetics line/surface conditions at the same time!");
  }
  // process conditions
  else if (!cond.empty() or !pointcond.empty())
  {
    double sum(0.0);

    if (myrank_ == 0)
    {
      std::cout << "Electrode boundary status information:" << std::endl;
      std::cout << "+----+------------------+-------------------+--------------------+-------------"
                   "--------+--------------------+---------------+----------------------+"
                << std::endl;
      std::cout << "| ID | reference domain | boundary integral | mean concentration | electrode "
                   "potential | mean overpotential | total current | mean current density |"
                << std::endl;
    }

    // evaluate the conditions and separate via ConditionID
    for (unsigned icond = 0; icond < cond.size() + pointcond.size(); ++icond)
    {
      // extract condition ID
      int condid(-1);
      if (!cond.empty())
        condid = cond[icond]->parameters().Get<int>("ConditionID");
      else
        condid = pointcond[icond]->parameters().Get<int>("ConditionID");

      // result vector
      // physical meaning of vector components is described in post_process_single_electrode_info
      // routine
      Teuchos::RCP<Core::LinAlg::SerialDenseVector> scalars;

      // electrode boundary kinetics line/surface condition
      if (!cond.empty())
      {
        scalars = evaluate_single_electrode_info(condid, "ElchBoundaryKinetics");
      }
      // electrode boundary kinetics point condition
      else
      {
        scalars = evaluate_single_electrode_info_point(pointcond[icond]);
      }

      // initialize unused dummy variable
      double dummy(0.);

      post_process_single_electrode_info(
          *scalars, condid, true, sum, dummy, dummy, dummy, dummy, dummy);
    }  // loop over conditions

    if (myrank_ == 0)
    {
      std::cout << "+----+------------------+-------------------+--------------------+-------------"
                   "--------+--------------------+---------------+----------------------+"
                << std::endl;
      // print out the net total current for all indicated boundaries
      printf("Net total current over boundary: %10.3E\n\n", sum);
    }
  }
}

/*------------------------------------------------------------------------------*
 *------------------------------------------------------------------------------*/
Teuchos::RCP<Core::LinAlg::SerialDenseVector>
ScaTra::ScaTraTimIntElch::evaluate_single_electrode_info(
    const int condid, const std::string& condstring)
{
  // set vector values needed by elements
  discret_->set_state("phinp", phinp_);
  // needed for double-layer capacity!
  discret_->set_state("phidtnp", phidtnp_);

  // create parameter list
  Teuchos::ParameterList eleparams;

  // set action for elements depending on type of condition to be evaluated
  if (condstring == "ElchDomainKinetics")
    Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
        "action", ScaTra::Action::calc_elch_domain_kinetics, eleparams);
  else if (condstring == "ElchBoundaryKinetics")
    Core::UTILS::AddEnumClassToParameterList<ScaTra::BoundaryAction>(
        "action", ScaTra::BoundaryAction::calc_elch_boundary_kinetics, eleparams);
  else
    FOUR_C_THROW("Invalid action " + condstring + " for output of electrode status information!");

  eleparams.set("calc_status", true);  // just want to have a status output!

  // Since we just want to have the status output for t_{n+1},
  // we have to take care for Gen.Alpha!
  // add_time_integration_specific_vectors cannot be used since we do not want
  // an evaluation for t_{n+\alpha_f} !!!

  // Warning:
  // Specific time integration parameter are set in the following function.
  // In the case of a genalpha-time integration scheme the solution vector phiaf_ at time n+af
  // is passed to the element evaluation routine. Therefore, the electrode status is evaluate at a
  // different time (n+af) than our output routine (n+1), resulting in slightly different values
  // at the electrode. A different approach is not possible (without major hacks) since the
  // time-integration scheme is necessary to perform galvanostatic simulations, for instance.
  // Think about: double layer effects for genalpha time-integration scheme

  // add element parameters according to time-integration scheme
  add_time_integration_specific_vectors();

  // initialize result vector
  // physical meaning of vector components is described in post_process_single_electrode_info
  // routine
  Teuchos::RCP<Core::LinAlg::SerialDenseVector> scalars =
      Teuchos::rcp(new Core::LinAlg::SerialDenseVector(11));

  // evaluate relevant boundary integrals
  discret_->EvaluateScalars(eleparams, scalars, condstring, condid);

  return scalars;
}

/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/
Teuchos::RCP<Core::LinAlg::SerialDenseVector>
ScaTra::ScaTraTimIntElch::evaluate_single_electrode_info_point(
    Teuchos::RCP<Core::Conditions::Condition> condition)
{
  // add state vectors to discretization
  discret_->set_state("phinp", phinp_);
  discret_->set_state("phidtnp", phidtnp_);  // needed for double layer capacity

  // add state vectors according to time integration scheme
  add_time_integration_specific_vectors();

  // determine number of scalar quantities to be computed
  const int numscalars = 11;

  // initialize result vector
  // physical meaning of vector components is described in post_process_single_electrode_info
  // routine
  Teuchos::RCP<Core::LinAlg::SerialDenseVector> scalars =
      Teuchos::rcp(new Core::LinAlg::SerialDenseVector(numscalars));

  // extract nodal cloud of current condition
  const std::vector<int>* nodeids = condition->GetNodes();

  // safety checks
  if (!nodeids)
    FOUR_C_THROW("Electrode kinetics point boundary condition doesn't have nodal cloud!");
  if (nodeids->size() != 1)
    FOUR_C_THROW(
        "Electrode kinetics point boundary condition must be associated with exactly one node!");

  // extract global ID of conditioned node
  const int nodeid = (*nodeids)[0];

  // initialize variable for number of processor owning conditioned node
  int procid(-1);

  // consider node only if it is owned by current processor
  if (discret_->NodeRowMap()->MyGID(nodeid))
  {
    // extract number of processor owning conditioned node
    procid = discret_->Comm().MyPID();

    // create parameter list
    Teuchos::ParameterList condparams;

    // set action for elements
    Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
        "action", ScaTra::Action::calc_elch_boundary_kinetics_point, condparams);

    // set flag for evaluation of status information
    condparams.set<bool>("calc_status", true);

    // equip element parameter list with current condition
    condparams.set<Teuchos::RCP<Core::Conditions::Condition>>("condition", condition);

    // get node
    Core::Nodes::Node* node = discret_->gNode(nodeid);

    // safety checks
    if (node == nullptr)
      FOUR_C_THROW("Cannot find node with global ID %d on discretization!", nodeid);
    if (node->NumElement() != 1)
    {
      FOUR_C_THROW(
          "Electrode kinetics point boundary condition must be specified on boundary node with "
          "exactly one attached element!");
    }

    // get element attached to node
    Core::Elements::Element* element = node->Elements()[0];

    // determine location information
    Core::Elements::Element::LocationArray la(discret_->NumDofSets());
    element->LocationVector(*discret_, la, false);

    // dummy matrix and right-hand side vector
    Core::LinAlg::SerialDenseMatrix elematrix_dummy;
    Core::LinAlg::SerialDenseVector elevector_dummy;

    // evaluate electrode kinetics point boundary conditions
    const int error = element->Evaluate(condparams, *discret_, la, elematrix_dummy, elematrix_dummy,
        *scalars, elevector_dummy, elevector_dummy);

    // safety check
    if (error)
      FOUR_C_THROW("Element with global ID %d returned error code %d on processor %d!",
          element->Id(), error, discret_->Comm().MyPID());
  }

  // communicate number of processor owning conditioned node
  int ownerid(-1);
  discret_->Comm().MaxAll(&procid, &ownerid, 1);

  // broadcast results from processor owning conditioned node to all other processors
  discret_->Comm().Broadcast(scalars->values(), numscalars, ownerid);

  return scalars;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::post_process_single_electrode_info(
    Core::LinAlg::SerialDenseVector& scalars, const int id, const bool print, double& currentsum,
    double& currtangent, double& currresidual, double& electrodeint, double& electrodepot,
    double& meanoverpot)
{
  // get total integral of current
  double currentintegral = scalars(0);
  // get total integral of double layer current
  double currentdlintegral = scalars(1);
  // get total domain or boundary integral
  double boundaryint = scalars(2);
  // get total integral of electric potential
  double electpotentialint = scalars(3);
  // get total integral of electric overpotential
  double overpotentialint = scalars(4);
  // get total integral of electric potential difference
  double epdint = scalars(5);
  // get total integral of open circuit electric potential
  double ocpint = scalars(6);
  // get total integral of reactant concentration
  double cint = scalars(7);
  // get derivative of integrated current with respect to electrode potential
  double currderiv = scalars(8);
  // get negative current residual (right-hand side of galvanostatic balance equation)
  double currentresidual = scalars(9);
  // get total domain integral scaled with volumetric electrode surface area total boundary
  // integral scaled with boundary porosity
  double boundaryint_porous = scalars(10);

  // specify some return values
  currentsum += currentintegral;  // sum of currents
  currtangent = currderiv;        // tangent w.r.t. electrode potential on metal side
  currresidual = currentresidual;
  electrodeint = boundaryint;
  electrodepot = electpotentialint / boundaryint;
  meanoverpot = overpotentialint / boundaryint;

  // print out results to screen/file if desired
  if (myrank_ == 0 and print)
  {
    // print out results to screen
    printf(
        "| %2d |      total       |    %10.3E     |     %10.3E     |     %10.3E      |     "
        "%10.3E     |  %10.3E   |      %10.3E      |\n",
        id, boundaryint, cint / boundaryint, electrodepot, overpotentialint / boundaryint,
        currentintegral + currentdlintegral,
        currentintegral / boundaryint + currentdlintegral / boundaryint);
    printf(
        "| %2d |   electrolyte    |    %10.3E     |     %10.3E     |     %10.3E      |     "
        "%10.3E     |  %10.3E   |      %10.3E      |\n",
        id, boundaryint_porous, cint / boundaryint_porous, electrodepot,
        overpotentialint / boundaryint, currentintegral + currentdlintegral,
        currentintegral / boundaryint_porous + currentdlintegral / boundaryint_porous);

    // write results to file
    std::ostringstream temp;
    temp << id;
    const std::string fname =
        problem_->OutputControlFile()->file_name() + ".electrode_status_" + temp.str() + ".txt";

    std::ofstream f;
    if (Step() == 0)
    {
      f.open(fname.c_str(), std::fstream::trunc);
      f << "#ID,Step,Time,Total_current,Boundary_integral,Mean_current_density_electrode_"
           "kinetics,Mean_current_density_dl,Mean_overpotential,Mean_electrode_pot_diff,Mean_"
           "opencircuit_pot,Electrode_pot,Mean_concentration,Boundary_integral_porous\n";
    }
    else
      f.open(fname.c_str(), std::fstream::ate | std::fstream::app);

    f << id << "," << Step() << "," << Time() << "," << currentintegral + currentdlintegral << ","
      << boundaryint << "," << currentintegral / boundaryint << ","
      << currentdlintegral / boundaryint << "," << overpotentialint / boundaryint << ","
      << epdint / boundaryint << "," << ocpint / boundaryint << "," << electrodepot << ","
      << cint / boundaryint << "," << boundaryint_porous << "\n";
    f.flush();
    f.close();
  }  // if (myrank_ == 0)

  // galvanostatic simulations:
  // add the double layer current to the Butler-Volmer current
  currentsum += currentdlintegral;

  // update vectors
  electrodeconc_[id] = cint / boundaryint;
  electrodeeta_[id] = overpotentialint / boundaryint;
  electrodecurr_[id] = currentintegral + currentdlintegral;
}

/*-------------------------------------------------------------------------*
 *-------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::output_electrode_info_domain()
{
  // extract electrode domain kinetics conditions from discretization
  std::string condstring("ElchDomainKinetics");
  std::vector<Core::Conditions::Condition*> conditions;
  discret_->GetCondition(condstring, conditions);

  // output electrode domain status information to screen if applicable
  if (!conditions.empty())
  {
    // initialize variable for total current
    double currentsum(0.);

    // print header of output table
    if (myrank_ == 0)
    {
      std::cout << "Status of '" << condstring << "':" << std::endl;
      std::cout << "+----+--------------------+---------------------+------------------+-----------"
                   "-----------+--------------------+----------------+----------------+"
                << std::endl;
      std::cout << "| ID | Bound/domain ratio |    Total current    | Domain integral  | Mean "
                   "current density | Mean overpotential | Electrode pot. | Mean Concentr. |"
                << std::endl;
    }

    // evaluate electrode domain kinetics conditions
    for (const auto& condition : conditions)
    {
      // extract condition ID
      const int condid = condition->parameters().Get<int>("ConditionID");

      Teuchos::RCP<Core::LinAlg::SerialDenseVector> scalars =
          evaluate_single_electrode_info(condid, condstring);

      // initialize unused dummy variable
      double dummy(0.);

      post_process_single_electrode_info(
          *scalars, condid, true, currentsum, dummy, dummy, dummy, dummy, dummy);
    }  // loop over electrode domain kinetics conditions

    if (myrank_ == 0)
    {
      // print finish line of output table
      std::cout << "+----+--------------------+----------------------+-----------------+-----------"
                   "-----------+--------------------+----------------+----------------+"
                << std::endl
                << std::endl;

      // print net total current
      printf("Net total current: %10.3E\n\n", currentsum);
    }
  }
}

/*-------------------------------------------------------------------------------*
 *-------------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::output_electrode_info_interior()
{
  std::vector<Core::Conditions::Condition*> conditions;
  discret_->GetCondition("ElectrodeSOC", conditions);

  if (!conditions.empty())
  {
    if (myrank_ == 0)
    {
      std::cout << std::endl << "Electrode state of charge and related:" << std::endl;
      std::cout << "+----+-----------------+----------------+----------------+" << std::endl;
      std::cout << "| ID | state of charge |     C rate     | operation mode |" << std::endl;
    }

    for (const auto& condition : conditions)
    {
      const int cond_id = condition->parameters().Get<int>("ConditionID");

      const double soc = electrodesoc_[cond_id];
      const double c_rate = electrodecrates_[cond_id];

      // print results to screen and files
      if (myrank_ == 0)
      {
        // determine operation mode based on c rate
        std::string mode;
        if (std::abs(c_rate) < 1.0e-16)
          mode = " at rest ";
        else if (c_rate < 0.0)
          mode = "discharge";
        else
          mode = " charge  ";

        // print results to screen
        std::cout << "| " << std::setw(2) << cond_id << " |   " << std::setw(7)
                  << std::setprecision(2) << std::fixed << soc * 100.0 << " %     |     "
                  << std::setw(5) << std::abs(c_rate) << "      |   " << mode << "    |"
                  << std::endl;
      }

      FOUR_C_ASSERT(runtime_csvwriter_soc_[cond_id].has_value(),
          "internal error: runtime csv writer not created.");
      std::map<std::string, std::vector<double>> output_data;
      output_data["SOC"] = {soc};
      output_data["CRate"] = {c_rate};
      runtime_csvwriter_soc_[cond_id]->WriteDataToFile(Time(), Step(), output_data);
    }

    if (myrank_ == 0)
      std::cout << "+----+-----------------+----------------+----------------+" << std::endl;
  }
}

/*-------------------------------------------------------------------------------*
 *-------------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::evaluate_electrode_info_interior()
{
  // extract conditions for electrode state of charge
  std::vector<Core::Conditions::Condition*> conditions;
  discret_->GetCondition("ElectrodeSOC", conditions);

  // perform all following operations only if there is at least one condition for electrode state
  // of charge
  if (!conditions.empty())
  {
    // loop over conditions for electrode state of charge
    for (const auto& condition : conditions)
    {
      // extract condition ID
      const int condid = condition->parameters().Get<int>("ConditionID");

      // add state vectors to discretization
      discret_->set_state("phinp", phinp_);
      discret_->set_state("phidtnp", phidtnp_);

      // create parameter list
      Teuchos::ParameterList condparams;

      // action for elements
      Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
          "action", ScaTra::Action::calc_elch_electrode_soc_and_c_rate, condparams);

      // initialize result vector
      // first component  = integral of concentration
      // second component = integral of time derivative of concentration
      // third component  = integral of domain
      // fourth component = integral of velocity divergence (ALE only)
      // fifth component  = integral of concentration times velocity divergence (ALE only)
      // sixth component  = integral of velocity times concentration gradient (ALE only)
      auto scalars = Teuchos::rcp(new Core::LinAlg::SerialDenseVector(isale_ ? 6 : 3));

      // evaluate current condition for electrode state of charge
      discret_->EvaluateScalars(condparams, scalars, "ElectrodeSOC", condid);

      // extract integral of domain
      const double intdomain = (*scalars)(2);

      // store initial volume of current electrode
      if (isale_ and step_ == 0) electrodeinitvols_[condid] = intdomain;

      // extract reference concentrations at 0% and 100% state of charge
      const double volratio = isale_ ? electrodeinitvols_[condid] / intdomain : 1.0;
      const double c_0 = condition->parameters().Get<double>("c_0%") * volratio;
      const double c_100 = condition->parameters().Get<double>("c_100%") * volratio;
      const double c_delta_inv = 1.0 / (c_100 - c_0);

      // get one hour for c_rate
      const double one_hour = condition->parameters().Get<double>("one_hour");

      // compute state of charge and C rate for current electrode
      const double c_avg = (*scalars)(0) / intdomain;
      const double soc = (c_avg - c_0) * c_delta_inv;
      double c_rate = (*scalars)(1);
      if (isale_)  // ToDo: The ALE case is still doing some weird stuff (strong temporal
                   // oscillations of C rate), so one should have a closer look at that...
        c_rate += (*scalars)(4) + (*scalars)(5) - c_avg * (*scalars)(3);
      c_rate *= c_delta_inv * one_hour / intdomain;

      // update state of charge and C rate for current electrode
      electrodesoc_[condid] = soc;
      electrodecrates_[condid] = c_rate;
    }

    cellcrate_ = std::abs(std::max_element(electrodecrates_.begin(), electrodecrates_.end(),
        [](const auto& a, const auto& b) {
          return std::abs(a.second) < std::abs(b.second);
        })->second);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::OutputCellVoltage()
{
  // extract conditions for cell voltage
  std::vector<Core::Conditions::Condition*> conditions;
  discret_->GetCondition("CellVoltage", conditions);
  std::vector<Core::Conditions::Condition*> conditionspoint;
  discret_->GetCondition("CellVoltagePoint", conditionspoint);
  if (!conditionspoint.empty()) conditions = conditionspoint;

  // perform all following operations only if there is at least one condition for cell voltage
  if (!conditions.empty())
  {
    if (myrank_ == 0)
    {
      std::cout << std::endl << "Electrode potentials and cell voltage:" << std::endl;
      std::cout << "+----+-------------------------+" << std::endl;
      std::cout << "| ID | mean electric potential |" << std::endl;
      for (const auto& condition : conditions)
      {
        const int cond_id = condition->parameters().Get<int>("ConditionID");
        std::cout << "| " << std::setw(2) << cond_id << " |         " << std::setw(6)
                  << std::setprecision(3) << std::fixed << electrodevoltage_[cond_id]
                  << "          |" << std::endl;
      }

      std::cout << "+----+-------------------------+" << std::endl;
      std::cout << "| cell voltage: " << std::setw(6) << cellvoltage_ << "         |" << std::endl;
      std::cout << "+----+-------------------------+" << std::endl;
    }

    FOUR_C_ASSERT(runtime_csvwriter_cell_voltage_.has_value(),
        "internal error: runtime csv writer not created.");
    std::map<std::string, std::vector<double>> output_data;
    output_data["CellVoltage"] = {cellvoltage_};
    runtime_csvwriter_cell_voltage_->WriteDataToFile(Time(), Step(), output_data);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::evaluate_cell_voltage()
{
  // extract conditions for cell voltage
  std::vector<Core::Conditions::Condition*> conditions;
  discret_->GetCondition("CellVoltage", conditions);
  std::vector<Core::Conditions::Condition*> conditionspoint;
  discret_->GetCondition("CellVoltagePoint", conditionspoint);
  if (!conditionspoint.empty()) conditions = conditionspoint;

  // perform all following operations only if there is at least one condition for cell voltage
  if (!conditions.empty())
  {
    // loop over both conditions for cell voltage
    for (const auto& condition : conditions)
    {
      // extract condition ID
      const int condid = condition->parameters().Get<int>("ConditionID");

      // process line and surface conditions
      if (conditionspoint.empty())
      {
        // add state vector to discretization
        discret_->set_state("phinp", phinp_);

        // create parameter list
        Teuchos::ParameterList condparams;

        // action for elements
        Core::UTILS::AddEnumClassToParameterList<ScaTra::BoundaryAction>(
            "action", ScaTra::BoundaryAction::calc_elch_cell_voltage, condparams);

        // initialize result vector
        // first component = electric potential integral, second component = domain integral
        Teuchos::RCP<Core::LinAlg::SerialDenseVector> scalars =
            Teuchos::rcp(new Core::LinAlg::SerialDenseVector(2));

        // evaluate current condition for electrode state of charge
        discret_->EvaluateScalars(condparams, scalars, "CellVoltage", condid);

        // extract concentration and domain integrals
        double intpotential = (*scalars)(0);
        double intdomain = (*scalars)(1);

        // compute mean electric potential of current electrode
        electrodevoltage_[condid] = intpotential / intdomain;
      }

      // process point conditions
      else
      {
        // initialize local variable for electric potential of current electrode
        double potential(0.0);

        // extract nodal cloud
        const std::vector<int>* const nodeids = condition->GetNodes();
        if (nodeids == nullptr)
          FOUR_C_THROW("Cell voltage point condition does not have nodal cloud!");
        if (nodeids->size() != 1)
          FOUR_C_THROW("Nodal cloud of cell voltage point condition must have exactly one node!");

        // extract node ID
        const int nodeid = (*nodeids)[0];

        // process row nodes only
        if (discret_->NodeRowMap()->MyGID(nodeid))
        {
          // extract node
          Core::Nodes::Node* node = discret_->gNode(nodeid);
          if (node == nullptr)
            FOUR_C_THROW(
                "Cannot extract node with global ID %d from scalar transport discretization!",
                nodeid);

          // extract degrees of freedom from node
          const std::vector<int> dofs = discret_->Dof(0, node);

          // extract local ID of degree of freedom associated with electrode potential
          const int lid = discret_->dof_row_map()->LID(*dofs.rbegin());
          if (lid < 0)
            FOUR_C_THROW("Cannot extract degree of freedom with global ID %d!", *dofs.rbegin());

          // extract electrode potential
          potential = (*phinp_)[lid];
        }

        // communicate electrode potential
        discret_->Comm().SumAll(&potential, &electrodevoltage_[condid], 1);
      }

    }  // loop over conditions

    // compute cell voltage
    cellvoltage_ = std::abs(electrodevoltage_[0] - electrodevoltage_[1]);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::write_restart() const
{
  // output restart data associated with electrode state of charge conditions if applicable,
  // needed for correct evaluation of cell C rate at the beginning of the first time step after
  // restart
  if (discret_->GetCondition("ElectrodeSOC"))
  {
    // output volumes of resolved electrodes
    if (isale_)
    {
      // extract condition ID and volume into two separate std vectors and write out
      auto conditionid_vec = Teuchos::rcp(new std::vector<int>);
      auto electrodeinitvol_vec = Teuchos::rcp(new std::vector<double>);
      for (const auto& electrodeinitvol : electrodeinitvols_)
      {
        conditionid_vec->push_back(electrodeinitvol.first);
        electrodeinitvol_vec->push_back(electrodeinitvol.second);
      }
      output_->write_redundant_int_vector("electrodeconditionids", conditionid_vec);
      output_->write_redundant_double_vector("electrodeinitvols", electrodeinitvol_vec);
    }
  }

  // output restart data associated with constant-current constant-voltage (CCCV) cell cycling if
  // applicable
  if (discret_->GetCondition("CCCVCycling"))
  {
    // output number of current charge or discharge half-cycle
    output_->write_int("ihalfcycle", cccv_condition_->get_num_current_half_cycle());

    // output cell voltage
    output_->write_double("cellvoltage", cellvoltage_);

    // output cell C rate
    output_->write_double("cellcrate", cellcrate_);

    // was the phase changed since last time step adaptivity?
    output_->write_int("phasechanged", static_cast<int>(cccv_condition_->IsPhaseChanged()));

    // are we in initial phase relaxation?
    output_->write_int(
        "phaseinitialrelaxation", static_cast<int>(cccv_condition_->is_phase_initial_relaxation()));

    // end time of current relaxation phase
    output_->write_double("relaxendtime", cccv_condition_->GetRelaxEndTime());

    // current phase of half cycle
    output_->write_int(
        "phase_cccv", static_cast<int>(cccv_condition_->get_cccv_half_cycle_phase()));

    // when was the phase change last time?
    output_->write_int("steplastphasechange", cccv_condition_->get_step_last_phase_change());

    // adapted time step
    output_->write_double("dt_adapted", dt_adapted_);

    output_->write_int("last_dt_change", last_dt_change_);

    // is time step adaptivity activated?
    output_->write_int("adapted_timestep_active", adapted_timestep_active_);
  }

  std::vector<Core::Conditions::Condition*> s2ikinetics_conditions(0, nullptr);
  discretization()->GetCondition("S2IKinetics", s2ikinetics_conditions);
  for (auto* s2ikinetics_cond : s2ikinetics_conditions)
  {
    // only slave side has relevant information
    if (s2ikinetics_cond->parameters().Get<int>("interface side") ==
            static_cast<int>(Inpar::S2I::side_slave) and
        s2ikinetics_cond->parameters().Get<int>("kinetic model") ==
            static_cast<int>(Inpar::S2I::kinetics_butlervolmerreducedcapacitance))
    {
      output_->write_vector("phidtnp", phidtnp_);
      break;
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::SetupNatConv()
{
  // calculate the initial mean concentration value
  if (NumScal() < 1) FOUR_C_THROW("Error since numscal = %d. Not allowed since < 1", NumScal());
  c0_.resize(NumScal());

  discret_->set_state("phinp", phinp_);

  // set action for elements
  Teuchos::ParameterList eleparams;
  Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
      "action", ScaTra::Action::calc_total_and_mean_scalars, eleparams);
  eleparams.set("inverting", false);
  eleparams.set("calc_grad_phi", false);

  // evaluate integrals of concentrations and domain
  Teuchos::RCP<Core::LinAlg::SerialDenseVector> scalars =
      Teuchos::rcp(new Core::LinAlg::SerialDenseVector(NumDofPerNode() + 1));
  discret_->EvaluateScalars(eleparams, scalars);

  // calculate mean concentration
  const double domint = (*scalars)[NumDofPerNode()];

  if (std::abs(domint) < 1e-15) FOUR_C_THROW("Division by zero!");

  for (int k = 0; k < NumScal(); k++) c0_[k] = (*scalars)[k] / domint;

  // initialization of the densification coefficient vector
  densific_.resize(NumScal());
  Core::Elements::Element* element = discret_->lRowElement(0);
  Teuchos::RCP<Core::Mat::Material> mat = element->Material();

  if (mat->MaterialType() == Core::Materials::m_matlist)
  {
    Teuchos::RCP<const Mat::MatList> actmat = Teuchos::rcp_static_cast<const Mat::MatList>(mat);

    for (int k = 0; k < NumScal(); ++k)
    {
      const int matid = actmat->MatID(k);
      Teuchos::RCP<const Core::Mat::Material> singlemat = actmat->MaterialById(matid);

      if (singlemat->MaterialType() == Core::Materials::m_ion)
      {
        Teuchos::RCP<const Mat::Ion> actsinglemat =
            Teuchos::rcp_static_cast<const Mat::Ion>(singlemat);

        densific_[k] = actsinglemat->Densification();

        if (densific_[k] < 0.0) FOUR_C_THROW("received negative densification value");
      }
      else
        FOUR_C_THROW("Material type is not allowed!");
    }
  }

  // for a single species calculation
  else if (mat->MaterialType() == Core::Materials::m_ion)
  {
    Teuchos::RCP<const Mat::Ion> actmat = Teuchos::rcp_static_cast<const Mat::Ion>(mat);

    densific_[0] = actmat->Densification();

    if (densific_[0] < 0.0) FOUR_C_THROW("received negative densification value");
    if (NumScal() > 1) FOUR_C_THROW("Single species calculation but numscal = %d > 1", NumScal());
  }
  else
    FOUR_C_THROW("Material type is not allowed!");
}

/*-------------------------------------------------------------------------*
 *-------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::valid_parameter_diff_cond()
{
  if (myrank_ == 0)
  {
    if (Core::UTILS::IntegralValue<Inpar::ElCh::ElchMovingBoundary>(
            *elchparams_, "MOVINGBOUNDARY") != Inpar::ElCh::elch_mov_bndry_no)
      FOUR_C_THROW(
          "Moving boundaries are not supported in the ELCH diffusion-conduction framework!!");

    if (Core::UTILS::IntegralValue<int>(*params_, "NATURAL_CONVECTION"))
      FOUR_C_THROW(
          "Natural convection is not supported in the ELCH diffusion-conduction framework!!");

    if (Core::UTILS::IntegralValue<Inpar::ScaTra::SolverType>(*params_, "SOLVERTYPE") !=
            Inpar::ScaTra::solvertype_nonlinear and
        Core::UTILS::IntegralValue<Inpar::ScaTra::SolverType>(*params_, "SOLVERTYPE") !=
            Inpar::ScaTra::solvertype_nonlinear_multiscale_macrotomicro and
        Core::UTILS::IntegralValue<Inpar::ScaTra::SolverType>(*params_, "SOLVERTYPE") !=
            Inpar::ScaTra::solvertype_nonlinear_multiscale_macrotomicro_aitken and
        Core::UTILS::IntegralValue<Inpar::ScaTra::SolverType>(*params_, "SOLVERTYPE") !=
            Inpar::ScaTra::solvertype_nonlinear_multiscale_macrotomicro_aitken_dofsplit and
        Core::UTILS::IntegralValue<Inpar::ScaTra::SolverType>(*params_, "SOLVERTYPE") !=
            Inpar::ScaTra::solvertype_nonlinear_multiscale_microtomacro)
    {
      FOUR_C_THROW(
          "The only solvertype supported by the ELCH diffusion-conduction framework is the "
          "non-linear solver!!");
    }

    if (problem_->GetProblemType() != Core::ProblemType::ssi and
        problem_->GetProblemType() != Core::ProblemType::ssti and
        Core::UTILS::IntegralValue<Inpar::ScaTra::ConvForm>(*params_, "CONVFORM") !=
            Inpar::ScaTra::convform_convective)
      FOUR_C_THROW("Only the convective formulation is supported so far!!");

    if ((static_cast<bool>(Core::UTILS::IntegralValue<int>(*params_, "NEUMANNINFLOW"))))
      FOUR_C_THROW(
          "Neuman inflow BC's are not supported by the ELCH diffusion-conduction framework!!");

    if ((static_cast<bool>(Core::UTILS::IntegralValue<int>(*params_, "CONV_HEAT_TRANS"))))
    {
      FOUR_C_THROW(
          "Convective heat transfer BC's are not supported by the ELCH diffusion-conduction "
          "framework!!");
    }

    if ((Core::UTILS::IntegralValue<Inpar::ScaTra::FSSUGRDIFF>(*params_, "FSSUGRDIFF")) !=
        Inpar::ScaTra::fssugrdiff_no)
      FOUR_C_THROW(
          "Subgrid diffusivity is not supported by the ELCH diffusion-conduction framework!!");

    if ((static_cast<bool>(Core::UTILS::IntegralValue<int>(*elchparams_, "BLOCKPRECOND"))))
      FOUR_C_THROW("Block preconditioner is not supported so far!!");

    // Parameters defined in "SCALAR TRANSPORT DYNAMIC"
    Teuchos::ParameterList& scatrastabparams = params_->sublist("STABILIZATION");

    if ((Core::UTILS::IntegralValue<Inpar::ScaTra::StabType>(scatrastabparams, "STABTYPE")) !=
        Inpar::ScaTra::stabtype_no_stabilization)
      FOUR_C_THROW(
          "No stabilization is necessary for solving the ELCH diffusion-conduction framework!!");

    if ((Core::UTILS::IntegralValue<Inpar::ScaTra::TauType>(scatrastabparams, "DEFINITION_TAU")) !=
        Inpar::ScaTra::tau_zero)
      FOUR_C_THROW(
          "No stabilization is necessary for solving the ELCH diffusion-conduction framework!!");

    if ((Core::UTILS::IntegralValue<Inpar::ScaTra::EvalTau>(scatrastabparams, "EVALUATION_TAU")) !=
        Inpar::ScaTra::evaltau_integration_point)
      FOUR_C_THROW("Evaluation of stabilization parameter only at Gauss points!!");

    if ((Core::UTILS::IntegralValue<Inpar::ScaTra::EvalMat>(scatrastabparams, "EVALUATION_MAT")) !=
        Inpar::ScaTra::evalmat_integration_point)
      FOUR_C_THROW("Evaluation of material only at Gauss points!!");

    if ((Core::UTILS::IntegralValue<Inpar::ScaTra::Consistency>(scatrastabparams, "CONSISTENCY")) !=
        Inpar::ScaTra::consistency_no)
      FOUR_C_THROW("Consistence formulation is not in the ELCH diffusion-conduction framework!!");

    if (static_cast<bool>(Core::UTILS::IntegralValue<int>(scatrastabparams, "SUGRVEL")))
      FOUR_C_THROW(
          "Subgrid velocity is not incorporated in the ELCH diffusion-conduction framework!!");

    if (static_cast<bool>(Core::UTILS::IntegralValue<int>(scatrastabparams, "ASSUGRDIFF")))
      FOUR_C_THROW(
          "Subgrid diffusivity is not incorporated in the ELCH diffusion-conduction framework!!");
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::init_nernst_bc()
{
  // access electrode kinetics condition
  std::vector<Core::Conditions::Condition*> Elchcond;
  discret_->GetCondition("ElchBoundaryKinetics", Elchcond);
  if (Elchcond.empty()) discret_->GetCondition("ElchBoundaryKineticsPoint", Elchcond);

  for (unsigned icond = 0; icond < Elchcond.size(); ++icond)
  {
    // check if Nernst-BC is defined on electrode kinetics condition
    if (Elchcond[icond]->parameters().Get<int>("kinetic model") == Inpar::ElCh::nernst)
    {
      // safety check
      if (!Elchcond[icond]->GeometryDescription())
        FOUR_C_THROW("Nernst boundary conditions not implemented for one-dimensional domains yet!");

      if (Core::UTILS::IntegralValue<int>(*elchparams_, "DIFFCOND_FORMULATION"))
      {
        if (icond == 0) ektoggle_ = Core::LinAlg::CreateVector(*(discret_->dof_row_map()), true);

        // 1.0 for electrode-kinetics toggle
        const double one = 1.0;

        // global node id's which are part of the Nernst-BC
        const std::vector<int>* nodegids = Elchcond[icond]->GetNodes();

        // loop over all global nodes part of the Nernst-BC
        for (int ii = 0; ii < (int(nodegids->size())); ++ii)
        {
          if (discret_->NodeRowMap()->MyGID((*nodegids)[ii]))
          {
            // get node with global node id (*nodegids)[ii]
            Core::Nodes::Node* node = discret_->gNode((*nodegids)[ii]);

            // get global dof ids of all dof's with global node id (*nodegids)[ii]
            std::vector<int> nodedofs = discret_->Dof(0, node);

            // define electrode kinetics toggle
            // later on this toggle is used to blanck the sysmat and rhs
            ektoggle_->ReplaceGlobalValues(1, &one, &nodedofs[NumScal()]);
          }
        }
      }
      else
        FOUR_C_THROW("Nernst BC is only available for diffusion-conduction formulation!");
    }
  }

  // At element level the Nernst condition has to be handled like a DC
  if (ektoggle_ != Teuchos::null) dctoggle_->Update(1.0, *ektoggle_, 1.0);
}

/*----------------------------------------------------------------------------------------*
 *----------------------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::create_meshtying_strategy()
{
  // fluid meshtying
  if (msht_ != Inpar::FLUID::no_meshtying)
  {
    strategy_ = Teuchos::rcp(new MeshtyingStrategyFluidElch(this));
  }
  // scatra-scatra interface coupling
  else if (S2IMeshtying())
  {
    strategy_ = Teuchos::rcp(new MeshtyingStrategyS2IElch(this, *params_));
  }
  // ScaTra-ScaTra interface contact
  else if (S2IKinetics() and !S2IMeshtying())
  {
    strategy_ = Teuchos::rcp(new MeshtyingStrategyStd(this));
  }
  // standard case without meshtying
  else
  {
    strategy_ = Teuchos::rcp(new MeshtyingStrategyStdElch(this));
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::calc_initial_potential_field()
{
  pre_calc_initial_potential_field();

  // time measurement
  TEUCHOS_FUNC_TIME_MONITOR("SCATRA:       + calc initial potential field");

  // safety checks
  FOUR_C_ASSERT(step_ == 0, "Step counter is not zero!");
  switch (equpot_)
  {
    case Inpar::ElCh::equpot_divi:
    case Inpar::ElCh::equpot_enc_pde:
    case Inpar::ElCh::equpot_enc_pde_elim:
    {
      // These stationary closing equations for the electric potential are OK, since they
      // explicitly contain the electric potential as variable and therefore can be solved for the
      // initial electric potential.
      break;
    }
    default:
    {
      // If the stationary closing equation for the electric potential does not explicitly contain
      // the electric potential as variable, we obtain a zero block associated with the electric
      // potential on the main diagonal of the global system matrix used below. This zero block
      // makes the entire global system matrix singular! In this case, it would be possible to
      // temporarily change the type of closing equation used, e.g., from Inpar::ElCh::equpot_enc
      // to Inpar::ElCh::equpot_enc_pde. This should work, but has not been implemented yet.
      FOUR_C_THROW(
          "Initial potential field cannot be computed for chosen closing equation for electric "
          "potential!");
      break;
    }
  }

  // screen output
  if (myrank_ == 0)
  {
    std::cout << "SCATRA: calculating initial field for electric potential" << std::endl;
    print_time_step_info();
    std::cout << "+------------+-------------------+--------------+--------------+" << std::endl;
    std::cout << "|- step/max -|- tol      [norm] -|-- pot-res ---|-- pot-inc ---|" << std::endl;
  }

  // prepare Newton-Raphson iteration
  iternum_ = 0;
  const int itermax = params_->sublist("NONLINEAR").get<int>("ITEMAX");
  const double itertol = params_->sublist("NONLINEAR").get<double>("CONVTOL");
  const double restol = params_->sublist("NONLINEAR").get<double>("ABSTOLRES");

  // start Newton-Raphson iteration
  while (true)
  {
    // update iteration counter
    iternum_++;

    // check for non-positive concentration values
    check_concentration_values(phinp_);

    // assemble global system matrix and residual vector
    assemble_mat_and_rhs();
    strategy_->CondenseMatAndRHS(sysmat_, residual_);

    // project residual, such that only part orthogonal to nullspace is considered
    if (projector_ != Teuchos::null) projector_->ApplyPT(*residual_);

    // apply actual Dirichlet boundary conditions to system of equations
    Core::LinAlg::apply_dirichlet_to_system(
        *sysmat_, *increment_, *residual_, *zeros_, *(dbcmaps_->CondMap()));

    // apply artificial Dirichlet boundary conditions to system of equations
    // to hold initial concentrations constant when solving for initial potential field
    Core::LinAlg::apply_dirichlet_to_system(
        *sysmat_, *increment_, *residual_, *zeros_, *(splitter_->OtherMap()));

    // compute L2 norm of electric potential state vector
    Teuchos::RCP<Epetra_Vector> pot_vector = splitter_->ExtractCondVector(phinp_);
    double pot_state_L2(0.0);
    pot_vector->Norm2(&pot_state_L2);

    // compute L2 norm of electric potential residual vector
    splitter_->ExtractCondVector(residual_, pot_vector);
    double pot_res_L2(0.);
    pot_vector->Norm2(&pot_res_L2);

    // compute L2 norm of electric potential increment vector
    splitter_->ExtractCondVector(increment_, pot_vector);
    double pot_inc_L2(0.);
    pot_vector->Norm2(&pot_inc_L2);

    // care for the case that nothing really happens in the potential field
    if (pot_state_L2 < 1e-5) pot_state_L2 = 1.0;

    // first iteration step: solution increment is not yet available
    if (iternum_ == 1)
    {
      // print first line of convergence table to screen
      if (myrank_ == 0)
      {
        std::cout << "|  " << std::setw(3) << iternum_ << "/" << std::setw(3) << itermax << "   | "
                  << std::setw(10) << std::setprecision(3) << std::scientific << itertol
                  << "[L_2 ]  | " << std::setw(10) << std::setprecision(3) << std::scientific
                  << pot_res_L2 << "   |      --      | (      --     ,te=" << std::setw(10)
                  << std::setprecision(3) << std::scientific << dtele_ << ")" << std::endl;
      }

      // absolute tolerance for deciding if residual is already zero
      // prevents additional solver calls that will not improve the residual anymore
      if (pot_res_L2 < restol)
      {
        // print finish line of convergence table to screen
        if (myrank_ == 0)
        {
          std::cout << "+------------+-------------------+--------------+--------------+"
                    << std::endl
                    << std::endl;
        }

        // abort Newton-Raphson iteration
        break;
      }
    }

    // later iteration steps: solution increment can be printed
    else
    {
      // print current line of convergence table to screen
      if (myrank_ == 0)
      {
        std::cout << "|  " << std::setw(3) << iternum_ << "/" << std::setw(3) << itermax << "   | "
                  << std::setw(10) << std::setprecision(3) << std::scientific << itertol
                  << "[L_2 ]  | " << std::setw(10) << std::setprecision(3) << std::scientific
                  << pot_res_L2 << "   | " << std::setw(10) << std::setprecision(3)
                  << std::scientific << pot_inc_L2 / pot_state_L2 << "   | (ts=" << std::setw(10)
                  << std::setprecision(3) << std::scientific << dtsolve_ << ",te=" << std::setw(10)
                  << std::setprecision(3) << std::scientific << dtele_ << ")" << std::endl;
      }

      // convergence check
      if ((pot_res_L2 <= itertol and pot_inc_L2 / pot_state_L2 <= itertol) or pot_res_L2 < restol)
      {
        // print finish line of convergence table to screen
        if (myrank_ == 0)
        {
          std::cout << "+------------+-------------------+--------------+--------------+"
                    << std::endl
                    << std::endl;
        }

        // abort Newton-Raphson iteration
        break;
      }
    }

    // warn if maximum number of iterations is reached without convergence
    if (iternum_ == itermax)
    {
      if (myrank_ == 0)
      {
        std::cout << "+--------------------------------------------------------------+"
                  << std::endl;
        std::cout << "|            >>>>>> not converged!                             |"
                  << std::endl;
        std::cout << "+--------------------------------------------------------------+" << std::endl
                  << std::endl;
      }

      // abort Newton-Raphson iteration
      break;
    }

    // safety checks
    if (std::isnan(pot_inc_L2) or std::isnan(pot_state_L2) or std::isnan(pot_res_L2))
      FOUR_C_THROW("calculated vector norm is NaN.");
    if (std::isinf(pot_inc_L2) or std::isinf(pot_state_L2) or std::isinf(pot_res_L2))
      FOUR_C_THROW("calculated vector norm is INF.");

    // zero out increment vector
    increment_->PutScalar(0.);

    // store time before solving global system of equations
    const double time = Teuchos::Time::wallTime();

    // reprepare Krylov projection if required
    if (updateprojection_) update_krylov_space_projection();

    Core::LinAlg::SolverParams solver_params;
    solver_params.projector = projector_;

    // solve final system of equations incrementally
    strategy_->Solve(solver_, sysmat_, increment_, residual_, phinp_, 1, solver_params);

    // determine time needed for solving global system of equations
    dtsolve_ = Teuchos::Time::wallTime() - time;

    // update electric potential degrees of freedom in initial state vector
    splitter_->AddCondVector(splitter_->ExtractCondVector(increment_), phinp_);

    // copy initial state vector
    phin_->Update(1., *phinp_, 0.);

    // update state vectors for intermediate time steps (only for generalized alpha)
    compute_intermediate_values();
  }  // Newton-Raphson iteration

  // reset global system matrix and its graph, since we solved a very special problem with a
  // special sparsity pattern
  sysmat_->Reset();

  post_calc_initial_potential_field();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
double ScaTra::ScaTraTimIntElch::compute_conductivity(
    Core::LinAlg::SerialDenseVector& sigma, bool effCond, bool specresist)
{
  // we perform the calculation on element level hiding the material access!
  // the initial concentration distribution has to be uniform to do so!!
  double specific_resistance = 0.0;

  // create the parameters for the elements
  Teuchos::ParameterList eleparams;
  Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
      "action", ScaTra::Action::calc_elch_conductivity, eleparams);

  eleparams.set("effCond", effCond);

  eleparams.set("specresist", specresist);

  // set vector values needed by elements
  add_time_integration_specific_vectors();

  // evaluate integrals of scalar(s) and domain
  Teuchos::RCP<Core::LinAlg::SerialDenseVector> sigma_domint =
      Teuchos::rcp(new Core::LinAlg::SerialDenseVector(NumScal() + 2));
  discret_->EvaluateScalars(eleparams, sigma_domint);
  const double domint = (*sigma_domint)[NumScal() + 1];

  if (!specresist)
    for (int ii = 0; ii < NumScal() + 1; ++ii) sigma[ii] = (*sigma_domint)[ii] / domint;
  else
    specific_resistance = (*sigma_domint)[NumScal()] / domint;

  return specific_resistance;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool ScaTra::ScaTraTimIntElch::apply_galvanostatic_control()
{
  // for galvanostatic ELCH applications we have to adjust the
  // applied cell voltage and continue Newton-Raphson iterations until
  // we reach the desired value for the electric current.

  if (Core::UTILS::IntegralValue<int>(*elchparams_, "GALVANOSTATIC"))
  {
    // set time derivative parameters of applied voltage for a double layer capacitance current
    // density,
    if (dlcapexists_) compute_time_deriv_pot0(false);

    // extract electrode domain and boundary kinetics conditions from discretization
    std::vector<Teuchos::RCP<Core::Conditions::Condition>> electrodedomainconditions;
    discret_->GetCondition("ElchDomainKinetics", electrodedomainconditions);
    std::vector<Teuchos::RCP<Core::Conditions::Condition>> electrodeboundaryconditions;
    discret_->GetCondition("ElchBoundaryKinetics", electrodeboundaryconditions);
    std::vector<Teuchos::RCP<Core::Conditions::Condition>> electrodeboundarypointconditions;
    discret_->GetCondition("ElchBoundaryKineticsPoint", electrodeboundarypointconditions);

    // safety checks
    if (!electrodedomainconditions.empty() and
        (!electrodeboundaryconditions.empty() or !electrodeboundarypointconditions.empty()))
    {
      FOUR_C_THROW(
          "At the moment, we cannot have electrode domain kinetics conditions and electrode "
          "boundary kinetics conditions at the same time!");
    }
    else if (!electrodeboundaryconditions.empty() and !electrodeboundarypointconditions.empty())
    {
      FOUR_C_THROW(
          "At the moment, we cannot have electrode boundary kinetics line/surface conditions and "
          "electrode boundary kinetics point conditions at the same time!");
    }

    // determine type of electrode kinetics conditions to be evaluated
    std::vector<Teuchos::RCP<Core::Conditions::Condition>> conditions;
    std::string condstring;
    if (!electrodedomainconditions.empty())
    {
      conditions = electrodedomainconditions;
      condstring = "ElchDomainKinetics";
    }
    else if (!electrodeboundaryconditions.empty())
    {
      conditions = electrodeboundaryconditions;
      condstring = "ElchBoundaryKinetics";
    }
    else if (!electrodeboundarypointconditions.empty())
    {
      conditions = electrodeboundarypointconditions;
      condstring = "ElchBoundaryPointKinetics";
    }
    else
      FOUR_C_THROW("Must have electrode kinetics conditions for galvanostatics!");

    // evaluate electrode kinetics conditions if applicable
    if (!conditions.empty())
    {
      const int condid_cathode = elchparams_->get<int>("GSTATCONDID_CATHODE");
      const int condid_anode = elchparams_->get<int>("GSTATCONDID_ANODE");
      int gstatitemax = (elchparams_->get<int>("GSTATITEMAX"));
      double gstatcurrenttol = (elchparams_->get<double>("GSTATCURTOL"));
      const int curvenum = elchparams_->get<int>("GSTATFUNCTNO");
      const double tol = elchparams_->get<double>("GSTATCONVTOL");
      const double effective_length = elchparams_->get<double>("GSTAT_LENGTH_CURRENTPATH");
      if (effective_length < 0.0) FOUR_C_THROW("A negative effective length is not possible!");
      const auto approxelctresist = Core::UTILS::IntegralValue<Inpar::ElCh::ApproxElectResist>(
          *elchparams_, "GSTAT_APPROX_ELECT_RESIST");

      // There are maximal two electrode conditions by definition
      // current flow i at electrodes
      Teuchos::RCP<std::vector<double>> actualcurrent =
          Teuchos::rcp(new std::vector<double>(2, 0.0));
      // residual at electrodes = i*timefac
      Teuchos::RCP<std::vector<double>> currresidual =
          Teuchos::rcp(new std::vector<double>(2, 0.0));
      Teuchos::RCP<std::vector<double>> currtangent = Teuchos::rcp(new std::vector<double>(2, 0.0));
      Teuchos::RCP<std::vector<double>> electrodesurface =
          Teuchos::rcp(new std::vector<double>(2, 0.0));
      Teuchos::RCP<std::vector<double>> electrodepot =
          Teuchos::rcp(new std::vector<double>(2, 0.0));
      Teuchos::RCP<std::vector<double>> meanoverpot = Teuchos::rcp(new std::vector<double>(2, 0.0));
      double meanelectrodesurface(0.0);
      // Assumption: Residual at BV1 is the negative of the value at BV2, therefore only the first
      // residual is calculated
      double residual(0.0);

      // for all time integration schemes, compute the current value for phidtnp
      // this is needed for evaluating charging currents due to double-layer capacity
      // This may only be called here and not inside OutputSingleElectrodeInfoBoundary!!!!
      // Otherwise you modify your output to file called during Output()
      compute_time_derivative();

      double targetcurrent =
          problem_->FunctionById<Core::UTILS::FunctionOfTime>(curvenum - 1).Evaluate(time_);
      double timefacrhs = 1.0 / residual_scaling();

      double currtangent_anode(0.0);
      double currtangent_cathode(0.0);
      double potinc_ohm(0.0);
      double potdiffbulk(0.0);
      double resistance = 0.0;

      if (conditions.size() > 2)
      {
        FOUR_C_THROW(
            "The framework may not work for geometric setups containing more than two "
            "electrodes! \n If you need it, check the framework exactly!!");
      }

      // loop over all BV
      // degenerated to a loop over 2 (user-specified) BV conditions
      // note: only the potential at the boundary with id condid_cathode will be adjusted!
      for (unsigned icond = 0; icond < conditions.size(); ++icond)
      {
        // extract condition ID
        const int condid = conditions[icond]->parameters().Get<int>("ConditionID");

        // result vector
        // physical meaning of vector components is described in post_process_single_electrode_info
        // routine
        Teuchos::RCP<Core::LinAlg::SerialDenseVector> scalars;

        // electrode boundary kinetics line/surface condition
        if (condstring != "ElchBoundaryPointKinetics")
        {
          scalars = evaluate_single_electrode_info(condid, condstring);
        }
        // electrode boundary kinetics point condition
        else
          scalars = evaluate_single_electrode_info_point(conditions[icond]);

        post_process_single_electrode_info(*scalars, condid, false, (*actualcurrent)[condid],
            (*currtangent)[condid], (*currresidual)[condid], (*electrodesurface)[condid],
            (*electrodepot)[condid], (*meanoverpot)[condid]);

        if (conditions.size() == 2)
        {
          // In the case the actual current is zero, we assume that the first electrode is the
          // cathode
          if ((*actualcurrent)[condid] < 0.0 and condid_cathode != condid)
          {
            FOUR_C_THROW(
                "The defined GSTATCONDID_CATHODE does not match the actual current flow "
                "situation!!");
          }
          else if ((*actualcurrent)[condid] > 0.0 and condid_anode != condid)
          {
            FOUR_C_THROW(
                "The defined GSTATCONDID_ANODE does not match the actual current flow "
                "situation!!");
          }
        }
      }  // end loop over electrode kinetics

      if (conditions.size() == 1)
      {
        if (condid_cathode != 0 or condid_anode != 1)
        {
          FOUR_C_THROW(
              "The defined GSTATCONDID_CATHODE and GSTATCONDID_ANODE is wrong for a setup with "
              "only one electrode!!\n Choose: GSTATCONDID_CATHODE=0 and GSTATCONDID_ANODE=1");
        }
      }

      // get the applied electrode potential of the cathode
      Teuchos::RCP<Core::Conditions::Condition> cathode_condition;
      for (const auto& condition : conditions)
      {
        if (condition->parameters().Get<int>("ConditionID") == condid_cathode)
        {
          cathode_condition = condition;
          break;
        }
      }
      const double potold = cathode_condition->parameters().Get<double>("pot");
      double potnew = potold;

      // bulk voltage loss
      // U = V_A - V_C =  eta_A + delta phi_ohm - eta_C
      // -> delta phi_ohm  = V_A - V_C - eta_A + eta_C = V_A - eta_A - (V_C  - eta_C)

      potdiffbulk = ((*electrodepot)[condid_anode] - (*meanoverpot)[condid_anode]) -
                    ((*electrodepot)[condid_cathode] - (*meanoverpot)[condid_cathode]);

      // cell voltage loss = V_A - V_C
      // potdiffcell=(*electrodepot)[condid_anode]-(*electrodepot)[condid_cathode];
      // tanget at anode and cathode
      currtangent_anode = (*currtangent)[condid_anode];
      currtangent_cathode = (*currtangent)[condid_cathode];

      if (conditions.size() == 2)
      {
        // mean electrode surface of the cathode and anode
        meanelectrodesurface = ((*electrodesurface)[0] + (*electrodesurface)[1]) / 2;
      }
      else
        meanelectrodesurface = (*electrodesurface)[condid_cathode];

      // The linearization of potential increment is always based on the cathode side!!

      // Assumption: Residual at BV1 is the negative of the value at BV2, therefore only the first
      // residual is calculated residual := (I - timefacrhs *I_target) I_target is alway negative,
      // since the reference electrode is the cathode
      residual = (*currresidual)[condid_cathode] -
                 (timefacrhs * targetcurrent);  // residual is stored only from cathode!

      // convergence test
      {
        if (myrank_ == 0)
        {
          // format output
          std::cout << "Galvanostatic mode:" << std::endl;
          std::cout << "+-----------------------------------------------------------------------+"
                    << std::endl;
          std::cout << "| Convergence check:                                                    |"
                    << std::endl;
          std::cout << "+-----------------------------------------------------------------------+"
                    << std::endl;
          std::cout << "| iteration:                                " << std::setw(14) << std::right
                    << gstatnumite_ << " / " << gstatitemax << "         |" << std::endl;
          std::cout << "| actual reaction current at cathode:            " << std::setprecision(6)
                    << std::scientific << std::setw(14) << std::right
                    << (*actualcurrent)[condid_cathode] << "         |" << std::endl;
          std::cout << "| required total current at cathode:             " << std::setw(14)
                    << std::right << targetcurrent << "         |" << std::endl;
          std::cout << "| negative residual (rhs):                       " << std::setw(14)
                    << std::right << residual << "         |" << std::endl;
          std::cout << "+-----------------------------------------------------------------------+"
                    << std::endl;
        }

        if (gstatnumite_ > gstatitemax)
        {
          if (myrank_ == 0)
          {
            std::cout << "| --> converged: maximum number iterations reached. Not yet converged!  |"
                      << std::endl;
            std::cout << "+-----------------------------------------------------------------------+"
                      << std::endl
                      << std::endl;
          }
          return true;  // we proceed to next time step
        }
        else if (abs(residual) < gstatcurrenttol)
        {
          if (myrank_ == 0)
          {
            std::cout << "| --> converged: Newton-RHS-Residual is smaller than " << gstatcurrenttol
                      << "!      |" << std::endl;
            std::cout << "+-----------------------------------------------------------------------+"
                      << std::endl
                      << std::endl;
          }
          return true;  // we proceed to next time step
        }
        // electric potential increment of the last iteration
        else if ((gstatnumite_ > 1) and
                 (abs(gstatincrement_) < (1 + abs(potold)) * tol))  // < ATOL + |pot|* RTOL
        {
          if (myrank_ == 0)
          {
            std::cout << "| --> converged: |" << gstatincrement_ << "| < "
                      << (1 + abs(potold)) * tol << std::endl;
            std::cout << "+-----------------------------------------------------------------------+"
                      << std::endl
                      << std::endl;
          }
          return true;  // galvanostatic control has converged
        }

        // safety check
        if (abs((*currtangent)[condid_cathode]) < 1e-13)
          FOUR_C_THROW(
              "Tangent in galvanostatic control is near zero: %lf", (*currtangent)[condid_cathode]);
      }

      // calculate the cell potential increment due to ohmic resistance
      if (approxelctresist == Inpar::ElCh::approxelctresist_effleninitcond)
      {
        // update applied electric potential
        // potential drop ButlerVolmer conditions (surface ovepotential) and in the electrolyte
        // (ohmic overpotential) are conected in parallel:

        // 3 different versions:
        // I_0 = I_BV1 = I_ohmic = I_BV2
        // R(I_target, I) = R_BV1(I_target, I) = R_ohmic(I_target, I) = -R_BV2(I_target, I)
        // delta E_0 = delta U_BV1 + delta U_ohmic - (delta U_BV2)
        // => delta E_0 = (R_BV1(I_target, I)/J) + (R_ohmic(I_target, I)/J) - (-R_BV2(I_target,
        // I)/J) Attention: epsilon and tortuosity are missing in this framework
        //            -> use approxelctresist_efflenintegcond or approxelctresist_relpotcur

        // initialize conductivity vector
        Core::LinAlg::SerialDenseVector sigma(NumDofPerNode());

        // compute conductivity
        compute_conductivity(sigma);

        // print conductivity
        if (myrank_ == 0)
        {
          for (int k = 0; k < NumScal(); ++k)
          {
            std::cout << "| Electrolyte conductivity (species " << k + 1
                      << "):          " << std::setw(14) << std::right << sigma[k] << "         |"
                      << std::endl;
          }

          if (equpot_ == Inpar::ElCh::equpot_enc_pde_elim)
          {
            double diff = sigma[0];

            for (int k = 1; k < NumScal(); ++k) diff += sigma[k];

            std::cout << "| Electrolyte conductivity (species elim) = " << sigma[NumScal()] - diff
                      << "         |" << std::endl;
          }

          std::cout << "| Electrolyte conductivity (all species):        " << std::setw(14)
                    << std::right << sigma[NumScal()] << "         |" << std::endl;
          std::cout << "+-----------------------------------------------------------------------+"
                    << std::endl;
        }

        // compute electrolyte resistance
        resistance = effective_length / (sigma[NumScal()] * meanelectrodesurface);
      }

      else if (approxelctresist == Inpar::ElCh::approxelctresist_relpotcur and
               conditions.size() == 2)
      {
        // actual potential difference is used to calculate the current path length
        // -> it is possible to compute the new ohmic potential step (porous media are
        // automatically included)
        //    without the input parameter GSTAT_LENGTH_CURRENTPATH
        // actual current < 0,  since the reference electrode is the cathode
        // potdiffbulk > 0,     always positive (see definition)
        // -1.0,                resistance has to be positive
        resistance = -1.0 * (potdiffbulk / (*actualcurrent)[condid_cathode]);
        // use of target current for the estimation of the resistance
        // resistance = -1.0*(potdiffbulk/(targetcurrent));
      }

      else if (approxelctresist == Inpar::ElCh::approxelctresist_efflenintegcond and
               conditions.size() == 2)
      {
        // dummy conductivity vector
        Core::LinAlg::SerialDenseVector sigma;

        const double specificresistance = compute_conductivity(sigma, true, true);

        resistance = specificresistance * effective_length / meanelectrodesurface;
        // actual current < 0,  since the reference electrode is the cathode
        // potdiffbulk > 0,     always positive (see definition)
        // -1.0,                resistance has to be positive
      }

      else
      {
        FOUR_C_THROW(
            "The combination of the parameter GSTAT_APPROX_ELECT_RESIST %i and the number of "
            "electrodes %i\n is not valid!",
            approxelctresist, conditions.size());
      }

      // calculate increment due to ohmic resistance
      potinc_ohm = -1.0 * resistance * residual / timefacrhs;

      // Do not update the cell potential for small currents
      if (abs((*actualcurrent)[condid_cathode]) < 1e-10) potinc_ohm = 0.0;

      // the current flow at both electrodes has to be the same within the solution tolerances
      if (abs((*actualcurrent)[condid_cathode] + (*actualcurrent)[condid_anode]) > 1e-8)
      {
        if (myrank_ == 0)
        {
          std::cout << "| WARNING: The difference of the current flow at anode and cathode      |"
                    << std::endl;
          std::cout << "| is "
                    << abs((*actualcurrent)[condid_cathode] + (*actualcurrent)[condid_anode])
                    << " larger than " << 1e-8 << "!                             |" << std::endl;
          std::cout << "+-----------------------------------------------------------------------+"
                    << std::endl;
        }
      }

      // Newton step:  Jacobian * \Delta pot = - Residual
      const double potinc_cathode = residual / ((-1) * currtangent_cathode);
      double potinc_anode = 0.0;
      if (abs(currtangent_anode) > 1e-13)  // anode surface overpotential is optional
      {
        potinc_anode = residual / ((-1) * currtangent_anode);
      }
      gstatincrement_ = (potinc_cathode + potinc_anode + potinc_ohm);
      // update electric potential
      potnew += gstatincrement_;

      if (myrank_ == 0)
      {
        std::cout << "| The ohmic potential increment is calculated based on                  |"
                  << std::endl;
        std::cout << "| the ohmic electrolyte resistance obtained from                        |"
                  << std::endl;

        if (approxelctresist == Inpar::ElCh::approxelctresist_effleninitcond)
        {
          std::cout << "| GSTAT_LENGTH_CURRENTPATH and the averaged electrolyte conductivity.   |"
                    << std::endl;
        }
        else if (approxelctresist == Inpar::ElCh::approxelctresist_relpotcur)
        {
          std::cout << "| the applied potential and the resulting current flow.                 |"
                    << std::endl;
        }
        else
        {
          std::cout << "| GSTAT_LENGTH_CURRENTPATH and the integrated electrolyte conductivity. |"
                    << std::endl;
        }

        std::cout << "+-----------------------------------------------------------------------+"
                  << std::endl;
        std::cout << "| Defined GSTAT_LENGTH_CURRENTPATH:              " << std::setw(14)
                  << std::right << effective_length << "         |" << std::endl;
        std::cout << "| Approximate electrolyte resistance:            " << std::setw(14)
                  << std::right << resistance << "         |" << std::endl;
        std::cout << "| New guess for:                                                        |"
                  << std::endl;
        std::cout << "| - ohmic potential increment:                   " << std::setw(14)
                  << std::right << potinc_ohm << "         |" << std::endl;
        std::cout << "| - overpotential increment cathode (condid " << condid_cathode
                  << "):  " << std::setw(14) << std::right << potinc_cathode << "         |"
                  << std::endl;
        std::cout << "| - overpotential increment anode (condid " << condid_anode
                  << "):    " << std::setw(14) << std::right << potinc_anode << "         |"
                  << std::endl;
        std::cout << "| -> total increment for potential:              " << std::setw(14)
                  << std::right << gstatincrement_ << "         |" << std::endl;
        std::cout << "+-----------------------------------------------------------------------+"
                  << std::endl;
        std::cout << "| old potential at the cathode (condid " << condid_cathode
                  << "):       " << std::setw(14) << std::right << potold << "         |"
                  << std::endl;
        std::cout << "| new potential at the cathode (condid " << condid_cathode
                  << "):       " << std::setw(14) << std::right << potnew << "         |"
                  << std::endl;
        std::cout << "+-----------------------------------------------------------------------+"
                  << std::endl
                  << std::endl;
      }

      //      // print additional information
      //      if (myrank_==0)
      //      {
      //        std::cout<< "  actualcurrent - targetcurrent = " <<
      //        ((*actualcurrent)[condid_cathode]-targetcurrent) << std::endl; std::cout<< "
      //        currtangent_cathode:  " << currtangent_cathode << std::endl; std::cout<< "
      //        currtangent_anode:    " << currtangent_anode << std::endl; std::cout<< "
      //        actualcurrent cathode:    " << (*actualcurrent)[condid_cathode] << std::endl;
      //        std::cout<< "  actualcurrent anode:  " << (*actualcurrent)[condid_anode] <<
      //        std::endl;
      //      }

      // replace potential value of the boundary condition (on all processors)
      cathode_condition->parameters().Add("pot", potnew);
      gstatnumite_++;
      return false;  // not yet converged -> continue Newton iteration with updated potential
    }
  }
  return true;  // default
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::evaluate_electrode_kinetics_conditions(
    Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix, Teuchos::RCP<Epetra_Vector> rhs,
    const std::string& condstring)
{
  // time measurement
  TEUCHOS_FUNC_TIME_MONITOR("SCATRA:       + evaluate condition '" + condstring + "'");

  // create parameter list
  Teuchos::ParameterList condparams;

  // set action for elements depending on type of condition to be evaluated
  if (condstring == "ElchDomainKinetics")
    Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
        "action", ScaTra::Action::calc_elch_domain_kinetics, condparams);
  else if (condstring == "ElchBoundaryKinetics")
    Core::UTILS::AddEnumClassToParameterList<ScaTra::BoundaryAction>(
        "action", ScaTra::BoundaryAction::calc_elch_boundary_kinetics, condparams);
  else
    FOUR_C_THROW("Illegal action for electrode kinetics evaluation!");

  // add element parameters and set state vectors according to time-integration scheme
  add_time_integration_specific_vectors();

  // evaluate electrode kinetics conditions at time t_{n+1} or t_{n+alpha_F}
  discret_->evaluate_condition(
      condparams, systemmatrix, Teuchos::null, rhs, Teuchos::null, Teuchos::null, condstring);

  // add linearization of NernstCondition to system matrix
  if (ektoggle_ != Teuchos::null) linearization_nernst_condition();
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::evaluate_electrode_boundary_kinetics_point_conditions(
    Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix, Teuchos::RCP<Epetra_Vector> rhs)
{
  // time measurement
  TEUCHOS_FUNC_TIME_MONITOR("SCATRA:       + evaluate condition 'ElchBoundaryKineticsPoint'");

  // create parameter list
  Teuchos::ParameterList condparams;

  // set action for elements
  Core::UTILS::AddEnumClassToParameterList<ScaTra::Action>(
      "action", ScaTra::Action::calc_elch_boundary_kinetics_point, condparams);

  // set state vectors according to time-integration scheme
  add_time_integration_specific_vectors();

  // extract electrode kinetics point boundary conditions from discretization
  std::vector<Teuchos::RCP<Core::Conditions::Condition>> conditions;
  discret_->GetCondition("ElchBoundaryKineticsPoint", conditions);

  // loop over all electrode kinetics point boundary conditions
  for (const auto& condition : conditions)
  {
    // extract nodal cloud of current condition
    const std::vector<int>* nodeids = condition->GetNodes();

    // safety checks
    if (!nodeids)
      FOUR_C_THROW("Electrode kinetics point boundary condition doesn't have nodal cloud!");
    if (nodeids->size() != 1)
      FOUR_C_THROW(
          "Electrode kinetics point boundary condition must be associated with exactly one node!");

    // extract global ID of conditioned node
    const int nodeid = (*nodeids)[0];

    // consider node only if it is owned by current processor
    if (discret_->NodeRowMap()->MyGID(nodeid))
    {
      // equip element parameter list with current condition
      condparams.set<Teuchos::RCP<Core::Conditions::Condition>>("condition", condition);

      // get node
      Core::Nodes::Node* node = discret_->gNode(nodeid);

      // safety checks
      if (node == nullptr)
        FOUR_C_THROW("Cannot find node with global ID %d on discretization!", nodeid);
      if (node->NumElement() != 1)
      {
        FOUR_C_THROW(
            "Electrode kinetics point boundary condition must be specified on boundary node with "
            "exactly one attached element!");
      }

      // get element attached to node
      Core::Elements::Element* element = node->Elements()[0];

      // determine location information
      Core::Elements::Element::LocationArray la(discret_->NumDofSets());
      element->LocationVector(*discret_, la, false);

      // initialize element matrix
      const int size = (int)la[0].lm_.size();
      Core::LinAlg::SerialDenseMatrix elematrix;
      if (elematrix.numRows() != size)
        elematrix.shape(size, size);
      else
        elematrix.putScalar(0.0);

      // initialize element right-hand side vector
      Core::LinAlg::SerialDenseVector elevector;
      if (elevector.length() != size)
        elevector.size(size);
      else
        elevector.putScalar(0.0);

      // dummy matrix and right-hand side vector
      Core::LinAlg::SerialDenseMatrix elematrix_dummy;
      Core::LinAlg::SerialDenseVector elevector_dummy;

      // evaluate electrode kinetics point boundary conditions
      const int error = element->Evaluate(condparams, *discret_, la, elematrix, elematrix_dummy,
          elevector, elevector_dummy, elevector_dummy);

      // safety check
      if (error)
        FOUR_C_THROW("Element with global ID %d returned error code %d on processor %d!",
            element->Id(), error, discret_->Comm().MyPID());

      // assemble element matrix and right-hand side vector into global system of equations
      sysmat_->Assemble(element->Id(), la[0].stride_, elematrix, la[0].lm_, la[0].lmowner_);
      Core::LinAlg::Assemble(*residual_, elevector, la[0].lm_, la[0].lmowner_);
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::linearization_nernst_condition()
{
  // Blank rows with Nernst-BC (inclusive diagonal entry)
  // Nernst-BC is a additional constraint coupled to the original system of equation
  if (!sysmat_->Filled()) sysmat_->Complete();
  sysmat_->ApplyDirichlet(*ektoggle_, false);
  Core::LinAlg::apply_dirichlet_to_system(*increment_, *residual_, *zeros_, *ektoggle_);

  // create an parameter list
  Teuchos::ParameterList condparams;
  // update total time for time curve actions
  add_time_integration_specific_vectors();
  // action for elements
  Core::UTILS::AddEnumClassToParameterList<ScaTra::BoundaryAction>(
      "action", ScaTra::BoundaryAction::calc_elch_linearize_nernst, condparams);

  // add element parameters and set state vectors according to time-integration scheme
  // we need here concentration at t+np
  discret_->set_state("phinp", phinp_);

  std::string condstring("ElchBoundaryKinetics");
  // evaluate ElchBoundaryKinetics conditions at time t_{n+1} or t_{n+alpha_F}
  // phinp (view to phinp)
  discret_->evaluate_condition(
      condparams, sysmat_, Teuchos::null, residual_, Teuchos::null, Teuchos::null, condstring);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::evaluate_solution_depending_conditions(
    Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix, Teuchos::RCP<Epetra_Vector> rhs)
{
  // evaluate domain conditions for electrode kinetics
  if (discret_->GetCondition("ElchDomainKinetics") != nullptr)
    evaluate_electrode_kinetics_conditions(systemmatrix, rhs, "ElchDomainKinetics");

  // evaluate boundary conditions for electrode kinetics
  if (discret_->GetCondition("ElchBoundaryKinetics") != nullptr)
    evaluate_electrode_kinetics_conditions(systemmatrix, rhs, "ElchBoundaryKinetics");

  // evaluate point boundary conditions for electrode kinetics
  if (discret_->GetCondition("ElchBoundaryKineticsPoint") != nullptr)
    evaluate_electrode_boundary_kinetics_point_conditions(systemmatrix, rhs);

  // call base class routine
  ScaTraTimIntImpl::evaluate_solution_depending_conditions(systemmatrix, rhs);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::check_concentration_values(Teuchos::RCP<Epetra_Vector> vec)
{
  // action only for ELCH applications

  // for NURBS discretizations we skip the following check.
  // Control points (i.e., the "nodes" and its associated dofs can be located
  // outside the domain of interest. Thus, they can have negative
  // concentration values although the concentration solution is positive
  // in the whole computational domain!
  if (dynamic_cast<Core::FE::Nurbs::NurbsDiscretization*>(discret_.get()) != nullptr) return;

  // this option can be helpful in some rare situations
  bool makepositive(false);

  std::vector<int> numfound(NumScal(), 0);
  for (int i = 0; i < discret_->NumMyRowNodes(); i++)
  {
    Core::Nodes::Node* lnode = discret_->lRowNode(i);
    std::vector<int> dofs;
    dofs = discret_->Dof(0, lnode);

    for (int k = 0; k < NumScal(); k++)
    {
      const int lid = discret_->dof_row_map()->LID(dofs[k]);
      if (((*vec)[lid]) < 1e-13)
      {
        numfound[k]++;
        if (makepositive) ((*vec)[lid]) = 1e-13;
      }
    }
  }

  // print warning to screen
  for (int k = 0; k < NumScal(); k++)
  {
    if (numfound[k] > 0)
    {
      std::cout << "WARNING: PROC " << myrank_ << " has " << numfound[k]
                << " nodes with zero/neg. concentration values for species " << k;
      if (makepositive)
        std::cout << "-> were made positive (set to 1.0e-13)" << std::endl;
      else
        std::cout << std::endl;
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::apply_dirichlet_bc(
    const double time, Teuchos::RCP<Epetra_Vector> phinp, Teuchos::RCP<Epetra_Vector> phidt)
{
  // call base class routine
  ScaTraTimIntImpl::apply_dirichlet_bc(time, phinp, phidt);

  // evaluate Dirichlet boundary condition on electric potential arising from constant-current
  // constant-voltage (CCCV) cell cycling boundary condition during constant-voltage (CV) phase
  if (cccv_condition_ != Teuchos::null)
  {
    if (cccv_condition_->get_cccv_half_cycle_phase() ==
        Inpar::ElCh::CCCVHalfCyclePhase::constant_voltage)
    {
      // initialize set for global IDs of electric potential degrees of freedom affected by
      // constant-current constant-voltage (CCCV) cell cycling boundary condition
      std::set<int> dbcgids;

      // extract constant-current constant-voltage (CCCV) half-cycle boundary conditions
      std::vector<Core::Conditions::Condition*> cccvhalfcycleconditions;
      discret_->GetCondition("CCCVHalfCycle", cccvhalfcycleconditions);

      // loop over all conditions
      for (const auto& cccvhalfcyclecondition : cccvhalfcycleconditions)
      {
        // check relevance of current condition
        if (cccvhalfcyclecondition->parameters().Get<int>("ConditionID") ==
            cccv_condition_->get_half_cycle_condition_id())
        {
          // extract cutoff voltage from condition and perform safety check
          const double cutoff_voltage =
              cccvhalfcyclecondition->parameters().Get<double>("CutoffVoltage");
          if (cutoff_voltage < 0.)
          {
            FOUR_C_THROW(
                "Cutoff voltage for constant-current constant-voltage (CCCV) cell cycling must "
                "not be negative!");
          }

          // extract nodal cloud of current condition and perform safety check
          const auto nodegids = cccvhalfcyclecondition->GetNodes();
          if (nodegids->empty())
          {
            FOUR_C_THROW(
                "Constant-current constant-voltage (CCCV) cell cycling boundary condition does "
                "not have a nodal cloud!");
          }

          // loop over all nodes
          for (int nodegid : *nodegids)
          {
            // consider only nodes stored by current processor
            if (discret_->HaveGlobalNode(nodegid))
            {
              // extract current node
              auto* const node = discret_->gNode(nodegid);

              // consider only nodes owned by current processor
              if (node->Owner() == discret_->Comm().MyPID())
              {
                // extract global ID of electric potential degree of freedom carried by current
                // node
                const int gid = discret_->Dof(0, node, cccv_condition_->NumDofs() - 1);

                // add global ID to set
                dbcgids.insert(gid);

                // apply cutoff voltage as Dirichlet boundary condition
                phinp->ReplaceGlobalValue(gid, 0, cutoff_voltage);
              }
            }
          }  // loop over all nodes

          // leave loop after relevant condition has been processed
          break;
        }  // relevant condition
      }    // loop over all conditions

      // transform set into vector and then into Epetra map
      std::vector<int> dbcgidsvec(dbcgids.begin(), dbcgids.end());
      auto dbcmap = Teuchos::rcp(new const Epetra_Map(-1, static_cast<int>(dbcgids.size()),
          dbcgidsvec.data(), dof_row_map()->IndexBase(), dof_row_map()->Comm()));

      // merge map with existing map for Dirichlet boundary conditions
      // Note: the dbcmaps_ internal member is reset every time evaluate_dirichlet() is called on
      // the discretization (part of ScaTraTimIntImpl::apply_dirichlet_bc(...)) at the beginning of
      // this method, therefore this adaptation has to be performed in each time step during cv
      // phase
      add_dirich_cond(dbcmap);
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::apply_neumann_bc(const Teuchos::RCP<Epetra_Vector>& neumann_loads)
{
  // call base class routine
  ScaTraTimIntImpl::apply_neumann_bc(neumann_loads);

  // evaluate Neumann boundary condition on electric potential arising from constant-current
  // constant-voltage (CCCV) cell cycling boundary condition during constant-current (CC) phase
  if (cccv_condition_ != Teuchos::null)
  {
    if (cccv_condition_->get_cccv_half_cycle_phase() ==
        Inpar::ElCh::CCCVHalfCyclePhase::constant_current)
    {
      // extract constant-current constant-voltage (CCCV) half-cycle boundary conditions
      std::vector<Core::Conditions::Condition*> cccvhalfcycleconditions;
      discret_->GetCondition("CCCVHalfCycle", cccvhalfcycleconditions);

      // loop over all conditions
      for (const auto& condition : cccvhalfcycleconditions)
      {
        // check relevance of current condition
        if (condition->parameters().Get<int>("ConditionID") ==
            cccv_condition_->get_half_cycle_condition_id())
        {
          if (condition->GType() != Core::Conditions::geometry_type_point)
          {
            // To avoid code redundancy, we evaluate the condition using the element-based algorithm
            // for standard Neumann boundary conditions. For this purpose, we must provide the
            // condition with some features to make it look like a standard Neumann boundary
            // condition.
            const std::vector<int> onoff = {0, 1};
            const std::vector<double> val = {0.0, condition->parameters().Get<double>("Current")};
            const std::vector<int> funct = {0, 0};
            condition->parameters().Add("numdof", 2);
            condition->parameters().Add("funct", funct);
            condition->parameters().Add("onoff", onoff);
            condition->parameters().Add("val", val);

            // create parameter list for elements
            Teuchos::ParameterList params;

            // set action for elements
            Core::UTILS::AddEnumClassToParameterList<ScaTra::BoundaryAction>(
                "action", ScaTra::BoundaryAction::calc_Neumann, params);

            // loop over all conditioned elements
            for (const auto& [ele_gid, ele] : condition->Geometry())
            {
              // get location vector of current element
              std::vector<int> lm;
              std::vector<int> lmowner;
              std::vector<int> lmstride;
              ele->LocationVector(*discret_, lm, lmowner, lmstride);

              // initialize element-based vector of Neumann loads
              Core::LinAlg::SerialDenseVector elevector(static_cast<int>(lm.size()));

              // evaluate Neumann boundary condition
              ele->evaluate_neumann(params, *discret_, *condition, lm, elevector);

              // assemble element-based vector of Neumann loads into global vector of Neumann
              // loads
              Core::LinAlg::Assemble(*neumann_loads, elevector, lm, lmowner);
            }  // loop over all conditioned elements
          }
          else
          {
            for (int node_gid : *condition->GetNodes())
            {
              auto* node = discret_->gNode(node_gid);
              auto dofs = discret_->Dof(0, node);
              const int dof_gid = dofs[2];
              const int dof_lid = dof_row_map()->LID(dof_gid);

              const auto neumann_value = condition->parameters().Get<double>("Current");

              constexpr double four_pi = 4.0 * M_PI;
              const double fac =
                  Core::UTILS::IntegralValue<bool>(*ScatraParameterList(), "SPHERICALCOORDS")
                      ? *node->X().data() * *node->X().data() * four_pi
                      : 1.0;

              neumann_loads->SumIntoMyValue(dof_lid, 0, neumann_value * fac);
            }
          }

          // leave loop after relevant condition has been processed
          break;
        }  // relevant condition
      }    // loop over all conditions
    }
  }
}

/*---------------------------------------------------------------------------*
 *---------------------------------------------------------------------------*/
bool ScaTra::ScaTraTimIntElch::NotFinished() const
{
  if (cccv_condition_ == Teuchos::null)
  {
    return ScaTraTimIntImpl::NotFinished();
  }
  else
  {
    return cccv_condition_->NotFinished();
  }
}

/*---------------------------------------------------------------------------*
 *---------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::perform_aitken_relaxation(
    Epetra_Vector& phinp, const Epetra_Vector& phinp_inc_diff)
{
  if (solvtype_ == Inpar::ScaTra::solvertype_nonlinear_multiscale_macrotomicro_aitken_dofsplit)
  {
    // safety checks
    if (splitter_macro_ == Teuchos::null)
      FOUR_C_THROW("Map extractor for macro scale has not been initialized yet!");

    // loop over all degrees of freedom
    for (int idof = 0; idof < splitter_macro_->NumMaps(); ++idof)
    {
      // extract subvectors associated with current degree of freedom
      const Teuchos::RCP<const Epetra_Vector> phinp_inc_dof =
          splitter_macro_->ExtractVector(*phinp_inc_, idof);
      const Teuchos::RCP<const Epetra_Vector> phinp_inc_diff_dof =
          splitter_macro_->ExtractVector(phinp_inc_diff, idof);

      // compute L2 norm of difference between current and previous increments of current degree
      // of freedom
      double phinp_inc_diff_L2(0.);
      phinp_inc_diff_dof->Norm2(&phinp_inc_diff_L2);

      // compute dot product between increment of current degree of freedom and difference between
      // current and previous increments of current degree of freedom
      double phinp_inc_dot_phinp_inc_diff(0.);
      if (phinp_inc_diff_dof->Dot(*phinp_inc_dof, &phinp_inc_dot_phinp_inc_diff))
        FOUR_C_THROW("Couldn't compute dot product!");

      // compute Aitken relaxation factor for current degree of freedom
      if (iternum_outer_ > 1 and phinp_inc_diff_L2 > 1.e-12)
        omega_[idof] *= 1 - phinp_inc_dot_phinp_inc_diff / (phinp_inc_diff_L2 * phinp_inc_diff_L2);

      // perform Aitken relaxation for current degree of freedom
      splitter_macro_->AddVector(*phinp_inc_dof, idof, phinp, omega_[idof]);
    }
  }

  else
    // call base class routine
    ScaTraTimIntImpl::perform_aitken_relaxation(phinp, phinp_inc_diff);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::output_flux(
    Teuchos::RCP<Epetra_MultiVector> flux, const std::string& fluxtype)
{
  // safety check
  if (flux == Teuchos::null) FOUR_C_THROW("Invalid flux vector!");

  if (fluxtype == "domain")
  {
    // In this case, flux output can be straightforwardly performed without additional
    // manipulation.
  }

  else if (fluxtype == "boundary")
  {
    // The closing equation for the electric potential is internally scaled by the factor 1/F for
    // better conditioning. Therefore, the associated boundary flux computed by the function
    // CalcFluxAtBoundary is also scaled by this factor. To avoid confusion, we remove the scaling
    // factor from the boundary flux before outputting it, so that the result can be physically
    // interpreted as the plain boundary current density without any scaling.
    splitter_->Scale(*flux, 1, elchparams_->get<double>("FARADAY_CONSTANT"));
  }

  else
    FOUR_C_THROW("Unknown flux type! Must be either 'domain' or 'boundary'!");

  // perform actual flux output by calling base class routine
  ScaTraTimIntImpl::output_flux(flux, fluxtype);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
ScaTra::ScalarHandlerElch::ScalarHandlerElch() : numscal_() {}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScalarHandlerElch::Setup(const ScaTraTimIntImpl* const scatratimint)
{
  // call base class
  ScalarHandler::Setup(scatratimint);

  // cast to electrochemistry time integrator
  const auto* const elchtimint = dynamic_cast<const ScaTraTimIntElch* const>(scatratimint);
  if (elchtimint == nullptr) FOUR_C_THROW("cast to ScaTraTimIntElch failed!");

  // adapt number of transported scalars if necessary
  // current is a solution variable
  if (Core::UTILS::IntegralValue<int>(
          elchtimint->ElchParameterList()->sublist("DIFFCOND"), "CURRENT_SOLUTION_VAR"))
  {
    // shape of local row element(0) -> number of space dimensions
    int dim = Global::Problem::Instance()->NDim();
    // number of concentrations transported is numdof-1-dim
    numscal_.clear();
    numscal_.insert(NumDofPerNode() - 1 - dim);
  }

  // multi-scale case
  else if (elchtimint->MacroScale())
  {
    // number of transported scalars is 1
    numscal_.clear();
    numscal_.insert(1);
  }

  // standard case
  else
  {
    // number of transported scalars is numdof-1 (last dof = electric potential)
    numscal_.clear();
    numscal_.insert(NumDofPerNode() - 1);
  }
}

/*-------------------------------------------------------------------------*
 *-------------------------------------------------------------------------*/
int ScaTra::ScalarHandlerElch::NumScalInCondition(const Core::Conditions::Condition& condition,
    const Teuchos::RCP<const Core::FE::Discretization>& discret) const
{
  check_is_setup();
  // for now only equal dof numbers are supported
  if (not equalnumdof_)
  {
    FOUR_C_THROW(
        "Different number of DOFs per node within ScaTra discretization! This is not supported for "
        "Elch!");
  }

  return NumScal();
}

/*-----------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::BuildBlockMaps(
    const std::vector<Teuchos::RCP<Core::Conditions::Condition>>& partitioningconditions,
    std::vector<Teuchos::RCP<const Epetra_Map>>& blockmaps) const
{
  if (MatrixType() == Core::LinAlg::MatrixType::block_condition_dof)
  {
    // safety check
    if (Core::UTILS::IntegralValue<int>(
            ElchParameterList()->sublist("DIFFCOND"), "CURRENT_SOLUTION_VAR"))
    {
      FOUR_C_THROW(
          "For chosen type of global block system matrix, current must not constitute solution "
          "variable!");
    }

    for (const auto& cond : partitioningconditions)
    {
      // all dofs that form one block map
      std::vector<std::vector<int>> partitioned_dofs(NumDofPerNode());

      for (const int node_gid : *cond->GetNodes())
      {
        if (discret_->HaveGlobalNode(node_gid) and
            discret_->gNode(node_gid)->Owner() == discret_->Comm().MyPID())
        {
          const std::vector<int> nodedofs = discret_->Dof(0, discret_->gNode(node_gid));
          FOUR_C_ASSERT(NumDofPerNode() == static_cast<int>(nodedofs.size()),
              "Global number of dofs per node is not equal the number of dofs of this node.");

          for (unsigned dof = 0; dof < nodedofs.size(); ++dof)
            partitioned_dofs[dof].emplace_back(nodedofs[dof]);
        }
      }

      for (const auto& dofs : partitioned_dofs)
      {
#ifdef FOUR_C_ENABLE_ASSERTIONS
        std::unordered_set<int> dof_set(dofs.begin(), dofs.end());
        FOUR_C_ASSERT(dof_set.size() == dofs.size(), "The dofs are not unique");
#endif
        blockmaps.emplace_back(Teuchos::rcp(
            new Epetra_Map(-1, static_cast<int>(dofs.size()), dofs.data(), 0, discret_->Comm())));
      }
    }
  }
  else
    ScaTra::ScaTraTimIntImpl::BuildBlockMaps(partitioningconditions, blockmaps);
}

/*-----------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::build_block_null_spaces(
    Teuchos::RCP<Core::LinAlg::Solver> solver, int init_block_number) const
{
  ScaTra::ScaTraTimIntImpl::build_block_null_spaces(solver, init_block_number);

  if (MatrixType() == Core::LinAlg::MatrixType::block_condition_dof)
    reduce_dimension_null_space_blocks(solver, init_block_number);
}

/*-----------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::reduce_dimension_null_space_blocks(
    Teuchos::RCP<Core::LinAlg::Solver> solver, int init_block_number) const
{
  // loop over blocks of global system matrix
  for (int iblock = 0; iblock < BlockMaps()->NumMaps(); ++iblock)
  {
    std::ostringstream iblockstr;
    iblockstr << init_block_number + iblock + 1;

    // access parameter sublist associated with smoother for current matrix block
    Teuchos::ParameterList& mueluparams =
        solver->Params().sublist("Inverse" + iblockstr.str()).sublist("MueLu Parameters");

    // extract already reduced null space associated with current matrix block
    Teuchos::RCP<Epetra_MultiVector> nspVector =
        mueluparams.get<Teuchos::RCP<Epetra_MultiVector>>("nullspace", Teuchos::null);

    const int dimns = mueluparams.get<int>("null space: dimension");
    std::vector<double> nullspace(nspVector->MyLength() * nspVector->NumVectors());
    Core::LinAlg::EpetraMultiVectorToStdVector(nspVector, nullspace, dimns);

    // null space associated with concentration dofs
    if (iblock % 2 == 0)
    {
      // remove zero null space vector associated with electric potential dofs by truncating
      // null space
      nullspace.resize(BlockMaps()->Map(iblock)->NumMyElements());
    }
    // null space associated with electric potential dofs
    else
    {
      // remove zero null space vector(s) associated with concentration dofs and retain only
      // the last null space vector associated with electric potential dofs
      nullspace.erase(
          nullspace.begin(), nullspace.end() - BlockMaps()->Map(iblock)->NumMyElements());
    }

    // decrease null space dimension and number of partial differential equations by one
    --mueluparams.get<int>("null space: dimension");
    --mueluparams.get<int>("PDE equations");

    // TODO:
    // Above a reference is used to directly modify the nullspace vector
    // This can be done more elegant as writing it back in a different container!
    const int dimnsnew = mueluparams.get<int>("null space: dimension");
    Teuchos::RCP<Epetra_MultiVector> nspVectornew =
        Teuchos::rcp(new Epetra_MultiVector(*(BlockMaps()->Map(iblock)), dimnsnew, true));
    Core::LinAlg::StdVectorToEpetraMultiVector(nullspace, nspVectornew, dimnsnew);

    mueluparams.set<Teuchos::RCP<Epetra_MultiVector>>("nullspace", nspVectornew);
  }
}

/*-----------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------*/
double ScaTra::ScaTraTimIntElch::compute_temperature_from_function() const
{
  return problem_->FunctionById<Core::UTILS::FunctionOfTime>(temperature_funct_num_ - 1)
      .Evaluate(time_);
}

/*-----------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------*/
double ScaTra::ScaTraTimIntElch::get_current_temperature() const
{
  double temperature(-1.0);

  // if no function is defined we use the value set in the dat-file
  if (temperature_funct_num_ == -1)
    temperature = elchparams_->get<double>("TEMPERATURE");
  else
    temperature = compute_temperature_from_function();

  return temperature;
}

/*-----------------------------------------------------------------------------*
 *-----------------------------------------------------------------------------*/
Teuchos::RCP<Core::UTILS::ResultTest> ScaTra::ScaTraTimIntElch::create_sca_tra_field_test()
{
  return Teuchos::rcp(new ScaTra::ElchResultTest(Teuchos::rcp(this, false)));
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ScaTra::ScaTraTimIntElch::evaluate_cccv_phase()
{
  // Check and update state of cccv condition
  if (cccv_condition_ != Teuchos::null)
  {
    // only proc 0 should print out information
    const bool do_print = discret_->Comm().MyPID() == 0;

    // which mode was last converged step? Is this phase over? Is the current half cycle over?
    if (cccv_condition_->get_cccv_half_cycle_phase() ==
        Inpar::ElCh::CCCVHalfCyclePhase::initital_relaxation)
    {
      // or-case is required to be independent of the time step size
      if (cccv_condition_->IsInitialRelaxation(time_, Dt()) or (time_ == 0.0))
      {
        // do nothing
      }
      else
      {
        cccv_condition_->set_first_cccv_half_cycle(step_);
      }
    }
    else
    {
      while (cccv_condition_->is_end_of_half_cycle_phase(cellvoltage_, cellcrate_, time_))
        cccv_condition_->NextPhase(step_, time_, do_print);
    }

    // all half cycles completed?
    const bool notfinished = cccv_condition_->NotFinished();

    if (!notfinished and do_print) std::cout << "CCCV cycling is completed.\n";
  }
}
FOUR_C_NAMESPACE_CLOSE
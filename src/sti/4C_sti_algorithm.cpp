/*----------------------------------------------------------------------*/
/*! \file

\brief general coupling algorithm for scatra-thermo interaction

\level 2

*/
/*----------------------------------------------------------------------*/
#include "4C_sti_algorithm.hpp"

#include "4C_coupling_adapter.hpp"
#include "4C_global_data.hpp"
#include "4C_io_control.hpp"
#include "4C_lib_discret.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_scatra_timint_implicit.hpp"
#include "4C_scatra_timint_meshtying_strategy_s2i.hpp"

FOUR_C_NAMESPACE_OPEN

/*--------------------------------------------------------------------------------*
 *--------------------------------------------------------------------------------*/
STI::Algorithm::Algorithm(const Epetra_Comm& comm, const Teuchos::ParameterList& stidyn,
    const Teuchos::ParameterList& scatradyn, const Teuchos::ParameterList& solverparams_scatra,
    const Teuchos::ParameterList& solverparams_thermo)
    :  // instantiate base class
      AlgorithmBase(comm, scatradyn),
      scatra_(Teuchos::null),
      thermo_(Teuchos::null),
      strategyscatra_(Teuchos::null),
      strategythermo_(Teuchos::null),
      fieldparameters_(Teuchos::rcp(new Teuchos::ParameterList(scatradyn))),
      iter_(0),
      itermax_(0),
      itertol_(0.),
      stiparameters_(Teuchos::rcp(new Teuchos::ParameterList(stidyn))),
      timer_(Teuchos::rcp(new Teuchos::Time("STI::ALG", true)))
{
  // check input parameters for scatra and thermo fields
  if (CORE::UTILS::IntegralValue<INPAR::SCATRA::VelocityField>(
          *fieldparameters_, "VELOCITYFIELD") != INPAR::SCATRA::velocity_zero)
    FOUR_C_THROW("Scatra-thermo interaction with convection not yet implemented!");

  // initialize scatra time integrator
  scatra_ = Teuchos::rcp(
      new ADAPTER::ScaTraBaseAlgorithm(*fieldparameters_, *fieldparameters_, solverparams_scatra));
  scatra_->Init();
  scatra_->ScaTraField()->set_number_of_dof_set_velocity(1);
  scatra_->ScaTraField()->set_number_of_dof_set_thermo(2);
  scatra_->Setup();

  // modify field parameters for thermo field
  modify_field_parameters_for_thermo_field();

  // initialize thermo time integrator
  thermo_ = Teuchos::rcp(new ADAPTER::ScaTraBaseAlgorithm(
      *fieldparameters_, *fieldparameters_, solverparams_thermo, "thermo"));
  thermo_->Init();
  thermo_->ScaTraField()->set_number_of_dof_set_velocity(1);
  thermo_->ScaTraField()->set_number_of_dof_set_sca_tra(2);
  thermo_->Setup();

  // check maps from scatra and thermo discretizations
  if (scatra_->ScaTraField()->Discretization()->DofRowMap()->NumGlobalElements() == 0)
    FOUR_C_THROW("Scatra discretization does not have any degrees of freedom!");
  if (thermo_->ScaTraField()->Discretization()->DofRowMap()->NumGlobalElements() == 0)
    FOUR_C_THROW("Thermo discretization does not have any degrees of freedom!");

  // additional safety check
  if (thermo_->ScaTraField()->NumScal() != 1)
    FOUR_C_THROW("Thermo field must involve exactly one transported scalar!");

  // perform initializations associated with scatra-scatra interface mesh tying
  if (scatra_->ScaTraField()->S2IMeshtying())
  {
    // safety check
    if (!thermo_->ScaTraField()->S2IMeshtying())
    {
      FOUR_C_THROW(
          "Can't evaluate scatra-scatra interface mesh tying in scatra field, but not in thermo "
          "field!");
    }

    // extract meshtying strategies for scatra-scatra interface coupling from scatra and thermo time
    // integrators
    strategyscatra_ =
        Teuchos::rcp_dynamic_cast<SCATRA::MeshtyingStrategyS2I>(scatra_->ScaTraField()->Strategy());
    strategythermo_ =
        Teuchos::rcp_dynamic_cast<SCATRA::MeshtyingStrategyS2I>(thermo_->ScaTraField()->Strategy());

    // perform initializations depending on type of meshtying method
    switch (strategyscatra_->CouplingType())
    {
      case INPAR::S2I::coupling_matching_nodes:
      {
        // safety check
        if (strategythermo_->CouplingType() != INPAR::S2I::coupling_matching_nodes)
        {
          FOUR_C_THROW(
              "Must have matching nodes at scatra-scatra coupling interfaces in both the scatra "
              "and the thermo fields!");
        }

        break;
      }

      case INPAR::S2I::coupling_mortar_standard:
      {
        // safety check
        if (strategythermo_->CouplingType() != INPAR::S2I::coupling_mortar_condensed_bubnov)
          FOUR_C_THROW("Invalid type of scatra-scatra interface coupling for thermo field!");

        // extract scatra-scatra interface mesh tying conditions
        std::vector<CORE::Conditions::Condition*> conditions;
        scatra_->ScaTraField()->Discretization()->GetCondition("S2IMeshtying", conditions);

        // loop over all conditions
        for (auto& condition : conditions)
        {
          // consider conditions for slave side only
          if (condition->parameters().Get<int>("interface side") == INPAR::S2I::side_slave)
          {
            // extract ID of current condition
            const int condid = condition->parameters().Get<int>("ConditionID");
            if (condid < 0) FOUR_C_THROW("Invalid condition ID!");

            // extract mortar discretizations associated with current condition
            DRT::Discretization& scatradis = strategyscatra_->mortar_discretization(condid);
            DRT::Discretization& thermodis = strategythermo_->mortar_discretization(condid);

            // exchange dofsets between discretizations
            scatradis.AddDofSet(thermodis.GetDofSetProxy());
            thermodis.AddDofSet(scatradis.GetDofSetProxy());
          }
        }

        break;
      }

      default:
      {
        FOUR_C_THROW("Invalid type of scatra-scatra interface coupling!");
      }
    }
  }
}  // STI::Algorithm::Algorithm

/*--------------------------------------------------------------------------------*
 *--------------------------------------------------------------------------------*/
void STI::Algorithm::modify_field_parameters_for_thermo_field()
{
  // extract parameters for initial temperature field from parameter list for scatra-thermo
  // interaction and overwrite corresponding parameters in parameter list for thermo field
  if (!fieldparameters_->isParameter("INITIALFIELD") or
      !fieldparameters_->isParameter("INITFUNCNO"))
  {
    FOUR_C_THROW(
        "Initial field parameters not properly set in input file section SCALAR TRANSPORT "
        "DYNAMIC!");
  }
  if (!stiparameters_->isParameter("THERMO_INITIALFIELD") or
      !stiparameters_->isParameter("THERMO_INITFUNCNO"))
  {
    FOUR_C_THROW(
        "Initial field parameters not properly set in input file section SCALAR TRANSPORT "
        "DYNAMIC!");
  }
  fieldparameters_->set<std::string>(
      "INITIALFIELD", stiparameters_->get<std::string>("THERMO_INITIALFIELD"));
  fieldparameters_->set<int>("INITFUNCNO", stiparameters_->get<int>("THERMO_INITFUNCNO"));

  // perform additional manipulations associated with scatra-scatra interface mesh tying
  if (scatra_->ScaTraField()->S2IMeshtying())
  {
    // set flag for matrix type associated with thermo field
    fieldparameters_->set<std::string>("MATRIXTYPE", "sparse");

    // set flag in thermo meshtying strategy for evaluation of interface linearizations and
    // residuals on slave side only
    fieldparameters_->sublist("S2I COUPLING").set<std::string>("SLAVEONLY", "Yes");

    // adapt type of meshtying method for thermo field
    if (fieldparameters_->sublist("S2I COUPLING").get<std::string>("COUPLINGTYPE") ==
        "StandardMortar")
    {
      fieldparameters_->sublist("S2I COUPLING")
          .set<std::string>("COUPLINGTYPE", "CondensedMortar_Bubnov");
    }
    else if (fieldparameters_->sublist("S2I COUPLING").get<std::string>("COUPLINGTYPE") !=
             "MatchingNodes")
      FOUR_C_THROW("Invalid type of scatra-scatra interface coupling!");

    // make sure that interface side underlying Lagrange multiplier definition is slave side
    fieldparameters_->sublist("S2I COUPLING").set<std::string>("LMSIDE", "slave");
  }
}  // STI::Algorithm::modify_field_parameters_for_thermo_field()

/*--------------------------------------------------------------------------------*
 *--------------------------------------------------------------------------------*/
void STI::Algorithm::Output()
{
  // output scatra field
  scatra_->ScaTraField()->check_and_write_output_and_restart();

  // output thermo field
  thermo_->ScaTraField()->check_and_write_output_and_restart();
}

/*--------------------------------------------------------------------------------*
 *--------------------------------------------------------------------------------*/
void STI::Algorithm::PrepareTimeStep()
{
  // update time and time step
  increment_time_and_step();

  // provide scatra and thermo fields with velocities
  scatra_->ScaTraField()->SetVelocityField();
  thermo_->ScaTraField()->SetVelocityField();

  // pass thermo degrees of freedom to scatra discretization for preparation of first time step
  // (calculation of initial time derivatives etc.)
  if (Step() == 1) transfer_thermo_to_scatra(thermo_->ScaTraField()->Phiafnp());

  // prepare time step for scatra field
  scatra_->ScaTraField()->PrepareTimeStep();

  // pass scatra degrees of freedom to thermo discretization for preparation of first time step
  // (calculation of initial time derivatives etc.)
  if (Step() == 1) transfer_scatra_to_thermo(scatra_->ScaTraField()->Phiafnp());

  // prepare time step for thermo field
  thermo_->ScaTraField()->PrepareTimeStep();
}  // STI::Algorithm::PrepareTimeStep()

/*--------------------------------------------------------------------------------*
 *--------------------------------------------------------------------------------*/
void STI::Algorithm::ReadRestart(int step  //! time step for restart
)
{
  // read scatra and thermo restart variables
  scatra_->ScaTraField()->ReadRestart(step);
  thermo_->ScaTraField()->ReadRestart(step);

  // set time and time step
  SetTimeStep(scatra_->ScaTraField()->Time(), step);
}  // STI::Algorithm::ReadRestart

/*--------------------------------------------------------------------------------*
 *--------------------------------------------------------------------------------*/
void STI::Algorithm::TimeLoop()
{
  // output initial solution to screen and files
  if (Step() == 0)
  {
    transfer_thermo_to_scatra(thermo_->ScaTraField()->Phiafnp());
    transfer_scatra_to_thermo(scatra_->ScaTraField()->Phiafnp());
    ScaTraField()->PrepareTimeLoop();
    ThermoField()->PrepareTimeLoop();
  }

  // time loop
  while (NotFinished())
  {
    // prepare time step
    PrepareTimeStep();

    // store time before calling nonlinear solver
    double time = timer_->wallTime();

    // evaluate time step
    Solve();

    // determine time spent by nonlinear solver and take maximum over all processors via
    // communication
    double mydtnonlinsolve(timer_->wallTime() - time), dtnonlinsolve(0.);
    Comm().MaxAll(&mydtnonlinsolve, &dtnonlinsolve, 1);

    // output performance statistics associated with nonlinear solver into *.csv file if applicable
    if (CORE::UTILS::IntegralValue<int>(*fieldparameters_, "OUTPUTNONLINSOLVERSTATS"))
      scatra_->ScaTraField()->output_nonlin_solver_stats(
          static_cast<int>(iter_), dtnonlinsolve, Step(), Comm());

    // update scatra and thermo fields
    Update();

    // output solution to screen and files
    Output();
  }  // while(NotFinished())
}  // STI::Algorithm::TimeLoop()

/*--------------------------------------------------------------------------------*
 *--------------------------------------------------------------------------------*/
void STI::Algorithm::transfer_scatra_to_thermo(const Teuchos::RCP<const Epetra_Vector> scatra) const
{
  // pass scatra degrees of freedom to thermo discretization
  thermo_->ScaTraField()->Discretization()->SetState(2, "scatra", scatra);

  // transfer state vector for evaluation of scatra-scatra interface mesh tying
  if (thermo_->ScaTraField()->S2IMeshtying())
  {
    switch (strategythermo_->CouplingType())
    {
      case INPAR::S2I::coupling_matching_nodes:
      {
        // pass master-side scatra degrees of freedom to thermo discretization
        const Teuchos::RCP<Epetra_Vector> imasterphinp = CORE::LINALG::CreateVector(
            *scatra_->ScaTraField()->Discretization()->DofRowMap(), true);
        strategyscatra_->InterfaceMaps()->InsertVector(
            strategyscatra_->CouplingAdapter()->MasterToSlave(
                strategyscatra_->InterfaceMaps()->ExtractVector(*scatra, 2)),
            1, imasterphinp);
        thermo_->ScaTraField()->Discretization()->SetState(2, "imasterscatra", imasterphinp);

        break;
      }

      case INPAR::S2I::coupling_mortar_condensed_bubnov:
      {
        // extract scatra-scatra interface mesh tying conditions
        std::vector<CORE::Conditions::Condition*> conditions;
        thermo_->ScaTraField()->Discretization()->GetCondition("S2IMeshtying", conditions);

        // loop over all conditions
        for (auto& condition : conditions)
        {
          // consider conditions for slave side only
          if (condition->parameters().Get<int>("interface side") == INPAR::S2I::side_slave)
          {
            // extract ID of current condition
            const int condid = condition->parameters().Get<int>("ConditionID");
            if (condid < 0) FOUR_C_THROW("Invalid condition ID!");

            // extract mortar discretization associated with current condition
            DRT::Discretization& thermodis = strategythermo_->mortar_discretization(condid);

            // pass interfacial scatra degrees of freedom to thermo discretization
            const Teuchos::RCP<Epetra_Vector> iscatra =
                Teuchos::rcp(new Epetra_Vector(*thermodis.DofRowMap(1)));
            CORE::LINALG::Export(*scatra, *iscatra);
            thermodis.SetState(1, "scatra", iscatra);
          }
        }

        break;
      }

      default:
      {
        FOUR_C_THROW("You must be kidding me...");
      }
    }
  }
}  // STI::Algorithm::transfer_scatra_to_thermo()

/*--------------------------------------------------------------------------------*
 *--------------------------------------------------------------------------------*/
void STI::Algorithm::transfer_thermo_to_scatra(const Teuchos::RCP<const Epetra_Vector> thermo) const
{
  // pass thermo degrees of freedom to scatra discretization
  scatra_->ScaTraField()->Discretization()->SetState(2, "thermo", thermo);

  // transfer state vector for evaluation of scatra-scatra interface mesh tying
  if (scatra_->ScaTraField()->S2IMeshtying() and
      strategyscatra_->CouplingType() == INPAR::S2I::coupling_mortar_standard)
  {
    // extract scatra-scatra interface mesh tying conditions
    std::vector<CORE::Conditions::Condition*> conditions;
    scatra_->ScaTraField()->Discretization()->GetCondition("S2IMeshtying", conditions);

    // loop over all conditions
    for (auto& condition : conditions)
    {
      // consider conditions for slave side only
      if (condition->parameters().Get<int>("interface side") == INPAR::S2I::side_slave)
      {
        // extract ID of current condition
        const int condid = condition->parameters().Get<int>("ConditionID");
        if (condid < 0) FOUR_C_THROW("Invalid condition ID!");

        // extract mortar discretization associated with current condition
        DRT::Discretization& scatradis = strategyscatra_->mortar_discretization(condid);

        // pass interfacial thermo degrees of freedom to scatra discretization
        const Teuchos::RCP<Epetra_Vector> ithermo =
            Teuchos::rcp(new Epetra_Vector(*scatradis.DofRowMap(1)));
        CORE::LINALG::Export(*thermo, *ithermo);
        scatradis.SetState(1, "thermo", ithermo);
      }
    }
  }
}  // STI::Algorithm::transfer_thermo_to_scatra()

/*--------------------------------------------------------------------------------*
 *--------------------------------------------------------------------------------*/
void STI::Algorithm::Update()
{
  // update scatra field
  scatra_->ScaTraField()->Update();

  // compare scatra field to analytical solution if applicable
  scatra_->ScaTraField()->evaluate_error_compared_to_analytical_sol();

  // update thermo field
  thermo_->ScaTraField()->Update();

  // compare thermo field to analytical solution if applicable
  thermo_->ScaTraField()->evaluate_error_compared_to_analytical_sol();
}  // STI::Algorithm::Update()

FOUR_C_NAMESPACE_CLOSE

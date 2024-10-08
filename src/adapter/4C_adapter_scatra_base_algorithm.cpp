/*----------------------------------------------------------------------*/
/*! \file

\brief scalar transport field base algorithm

\level 1


*/
/*----------------------------------------------------------------------*/

#include "4C_adapter_scatra_base_algorithm.hpp"

#include "4C_global_data.hpp"
#include "4C_inpar_ssi.hpp"
#include "4C_inpar_ssti.hpp"
#include "4C_inpar_sti.hpp"
#include "4C_io.hpp"
#include "4C_io_control.hpp"
#include "4C_levelset_timint_ost.hpp"
#include "4C_levelset_timint_stat.hpp"
#include "4C_linear_solver_method.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_scatra_resulttest_hdg.hpp"
#include "4C_scatra_timint_bdf2.hpp"
#include "4C_scatra_timint_cardiac_monodomain_scheme.hpp"
#include "4C_scatra_timint_cardiac_monodomain_scheme_hdg.hpp"
#include "4C_scatra_timint_elch_scheme.hpp"
#include "4C_scatra_timint_genalpha.hpp"
#include "4C_scatra_timint_loma_genalpha.hpp"
#include "4C_scatra_timint_ost.hpp"
#include "4C_scatra_timint_poromulti.hpp"
#include "4C_scatra_timint_stat.hpp"
#include "4C_scatra_timint_stat_hdg.hpp"

#include <Teuchos_StandardParameterEntryValidators.hpp>

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Adapter::ScaTraBaseAlgorithm::ScaTraBaseAlgorithm(const Teuchos::ParameterList& prbdyn,
    const Teuchos::ParameterList& scatradyn, const Teuchos::ParameterList& solverparams,
    const std::string& disname, const bool isale)
    : scatra_(Teuchos::null), issetup_(false), isinit_(false)
{
  // setup scalar transport algorithm (overriding some dynamic parameters
  // with values specified in given problem-dependent ParameterList prbdyn)

  // -------------------------------------------------------------------
  // what's the current problem type?
  // -------------------------------------------------------------------
  auto probtype = Global::Problem::instance()->get_problem_type();

  // -------------------------------------------------------------------
  // access the discretization
  // -------------------------------------------------------------------
  auto discret = Global::Problem::instance()->get_dis(disname);

  // -------------------------------------------------------------------
  // set degrees of freedom in the discretization
  // -------------------------------------------------------------------
  if (!discret->filled() or !discret->have_dofs()) discret->fill_complete();

  // -------------------------------------------------------------------
  // context for output and restart
  // -------------------------------------------------------------------
  auto output = discret->writer();
  if (discret->num_global_elements() == 0)
    FOUR_C_THROW("No elements in discretization %s", discret->name().c_str());
  output->write_mesh(0, 0.0);

  // -------------------------------------------------------------------
  // create a solver
  // -------------------------------------------------------------------
  // TODO: TAW use of solverparams???
  // change input parameter to solver number instead of parameter list?
  // -> no default paramter possible any more
  auto solver = Teuchos::make_rcp<Core::LinAlg::Solver>(solverparams, discret->get_comm(),
      Global::Problem::instance()->solver_params_callback(),
      Teuchos::getIntegralValue<Core::IO::Verbositylevel>(
          Global::Problem::instance()->io_params(), "VERBOSITY"));

  // -------------------------------------------------------------------
  // set parameters in list required for all schemes
  // -------------------------------------------------------------------
  // make a copy (inside an Teuchos::rcp) containing also all sublists
  auto scatratimeparams = Teuchos::make_rcp<Teuchos::ParameterList>(scatradyn);
  if (scatratimeparams == Teuchos::null)
    FOUR_C_THROW("Instantiation of Teuchos::ParameterList failed!");

  // -------------------------------------------------------------------
  // overrule certain parameters for coupled problems
  // -------------------------------------------------------------------
  // the default time step size
  scatratimeparams->set<double>("TIMESTEP", prbdyn.get<double>("TIMESTEP"));
  // maximum simulation time
  scatratimeparams->set<double>("MAXTIME", prbdyn.get<double>("MAXTIME"));
  // maximum number of timesteps
  scatratimeparams->set<int>("NUMSTEP", prbdyn.get<int>("NUMSTEP"));
  // restart
  scatratimeparams->set<int>("RESTARTEVRY", prbdyn.get<int>("RESTARTEVRY"));
  // solution output
  scatratimeparams->set<int>("RESULTSEVRY", prbdyn.get<int>("RESULTSEVRY"));

  // -------------------------------------------------------------------
  // overrule flags for solid-based scalar transport!
  // (assumed disname = "scatra2" for solid-based scalar transport)
  // -------------------------------------------------------------------
  if (probtype == Core::ProblemType::biofilm_fsi or probtype == Core::ProblemType::gas_fsi or
      probtype == Core::ProblemType::fps3i or probtype == Core::ProblemType::thermo_fsi)
  {
    // scatra1 (=fluid scalar) get's inputs from SCALAR TRANSPORT DYNAMIC/STABILIZATION, hence
    // nothing is to do here
    //    if (disname== "scatra1") //get's inputs from SCALAR TRANSPORT DYNAMIC/STABILIZATION

    if (disname == "scatra2")  // structure_scatra discretisation
    {
      // scatra2 (=structure scalar) get's inputs from FS3I DYNAMIC/STRUCTURE SCALAR STABILIZATION,
      // hence we have to replace it
      scatratimeparams->sublist("STABILIZATION") = prbdyn.sublist("STRUCTURE SCALAR STABILIZATION");
      scatratimeparams->set<Inpar::ScaTra::ConvForm>(
          "CONVFORM", prbdyn.get<Inpar::ScaTra::ConvForm>("STRUCTSCAL_CONVFORM"));

      // scatra2 get's in initial functions from FS3I DYNAMICS
      switch (
          Teuchos::getIntegralValue<Inpar::ScaTra::InitialField>(prbdyn, "STRUCTSCAL_INITIALFIELD"))
      {
        case Inpar::ScaTra::initfield_zero_field:
          scatratimeparams->set<std::string>("INITIALFIELD",
              "zero_field");  // we want zero initial conditions for the structure scalar
          scatratimeparams->set<int>("INITFUNCNO", -1);
          break;
        case Inpar::ScaTra::initfield_field_by_function:
          scatratimeparams->set<std::string>(
              "INITIALFIELD", "field_by_function");  // we want the same initial conditions for
                                                     // structure scalar as for the fluid scalar
          scatratimeparams->set<int>("INITFUNCNO", prbdyn.get<int>("STRUCTSCAL_INITFUNCNO"));
          break;
        default:
          FOUR_C_THROW("Your STRUCTSCAL_INITIALFIELD type is not supported!");
          break;
      }

      // structure scatra does not require any Neumann inflow boundary conditions
      scatratimeparams->set<bool>("NEUMANNINFLOW", false);
    }
    else if (disname == "scatra1")  // fluid_scatra discretisation
    {
      // fluid scatra does not require any convective heat transfer boundary conditions
      scatratimeparams->set<bool>("CONV_HEAT_TRANS", false);
    }
  }

  // -------------------------------------------------------------------
  // list for extra parameters
  // (put here everything that is not available in scatradyn or its sublists)
  // -------------------------------------------------------------------
  auto extraparams = Teuchos::make_rcp<Teuchos::ParameterList>();

  // ----------------Eulerian or ALE formulation of transport equation(s)
  extraparams->set<bool>("isale", isale);

  // ------------------------------------get also fluid turbulence sublist
  const auto& fdyn = Global::Problem::instance()->fluid_dynamic_params();
  extraparams->sublist("TURBULENCE MODEL") = fdyn.sublist("TURBULENCE MODEL");
  extraparams->sublist("SUBGRID VISCOSITY") = fdyn.sublist("SUBGRID VISCOSITY");
  extraparams->sublist("MULTIFRACTAL SUBGRID SCALES") = fdyn.sublist("MULTIFRACTAL SUBGRID SCALES");
  extraparams->sublist("TURBULENT INFLOW") = fdyn.sublist("TURBULENT INFLOW");

  // ------------------------------------get electromagnetic parameters
  extraparams->set<bool>(
      "ELECTROMAGNETICDIFFUSION", scatradyn.get<bool>("ELECTROMAGNETICDIFFUSION"));
  extraparams->set<int>("EMDSOURCE", scatradyn.get<int>("EMDSOURCE"));

  // -------------------------------------------------------------------
  // algorithm construction depending on problem type and
  // time-integration (or stationary) scheme
  // -------------------------------------------------------------------
  auto timintscheme =
      Teuchos::getIntegralValue<Inpar::ScaTra::TimeIntegrationScheme>(scatradyn, "TIMEINTEGR");

  // low Mach number flow
  if (probtype == Core::ProblemType::loma or probtype == Core::ProblemType::thermo_fsi)
  {
    auto lomaparams = Teuchos::make_rcp<Teuchos::ParameterList>(
        Global::Problem::instance()->loma_control_params());
    switch (timintscheme)
    {
      case Inpar::ScaTra::timeint_gen_alpha:
      {
        // create instance of time integration class (call the constructor)
        scatra_ = Teuchos::make_rcp<ScaTra::TimIntLomaGenAlpha>(
            discret, solver, lomaparams, scatratimeparams, extraparams, output);
        break;
      }
      default:
        FOUR_C_THROW("Unknown time integration scheme for loMa!");
        break;
    }
  }

  // electrochemistry
  else if (probtype == Core::ProblemType::elch or
           ((probtype == Core::ProblemType::ssi and
                Teuchos::getIntegralValue<Inpar::SSI::ScaTraTimIntType>(
                    Global::Problem::instance()->ssi_control_params(), "SCATRATIMINTTYPE") ==
                    Inpar::SSI::ScaTraTimIntType::elch) or
               (disname == "scatra" and
                   ((probtype == Core::ProblemType::ssti and
                        Teuchos::getIntegralValue<Inpar::SSTI::ScaTraTimIntType>(
                            Global::Problem::instance()->ssti_control_params(),
                            "SCATRATIMINTTYPE") == Inpar::SSTI::ScaTraTimIntType::elch) or
                       (probtype == Core::ProblemType::sti and
                           Teuchos::getIntegralValue<Inpar::STI::ScaTraTimIntType>(
                               Global::Problem::instance()->sti_dynamic_params(),
                               "SCATRATIMINTTYPE") == Inpar::STI::ScaTraTimIntType::elch)))))
  {
    auto elchparams = Teuchos::make_rcp<Teuchos::ParameterList>(
        Global::Problem::instance()->elch_control_params());

    switch (timintscheme)
    {
      case Inpar::ScaTra::timeint_one_step_theta:
      {
        if (elchparams->sublist("SCL").get<bool>("ADD_MICRO_MACRO_COUPLING"))
        {
          if (disname == "scatra")
          {
            scatra_ = Teuchos::make_rcp<ScaTra::ScaTraTimIntElchSCLOST>(
                discret, solver, elchparams, scatratimeparams, extraparams, output);
          }
          else if (disname == "scatra_micro")
          {
            scatra_ = Teuchos::make_rcp<ScaTra::ScaTraTimIntElchOST>(
                discret, solver, elchparams, scatratimeparams, extraparams, output);
          }
          else
            FOUR_C_THROW("not identified");
        }
        else
        {
          scatra_ = Teuchos::make_rcp<ScaTra::ScaTraTimIntElchOST>(
              discret, solver, elchparams, scatratimeparams, extraparams, output);
        }

        break;
      }
      case Inpar::ScaTra::timeint_bdf2:
      {
        // create instance of time integration class (call the constructor)
        scatra_ = Teuchos::make_rcp<ScaTra::ScaTraTimIntElchBDF2>(
            discret, solver, elchparams, scatratimeparams, extraparams, output);
        break;
      }
      case Inpar::ScaTra::timeint_gen_alpha:
      {
        // create instance of time integration class (call the constructor)
        scatra_ = Teuchos::make_rcp<ScaTra::ScaTraTimIntElchGenAlpha>(
            discret, solver, elchparams, scatratimeparams, extraparams, output);
        break;
      }
      case Inpar::ScaTra::timeint_stationary:
      {
        // create instance of time integration class (call the constructor)
        scatra_ = Teuchos::make_rcp<ScaTra::ScaTraTimIntElchStationary>(
            discret, solver, elchparams, scatratimeparams, extraparams, output);
        break;
      }
      default:
        FOUR_C_THROW("Unknown time integration scheme for electrochemistry!");
        break;
    }
  }

  // levelset
  else if (probtype == Core::ProblemType::level_set or probtype == Core::ProblemType::fluid_xfem_ls)
  {
    Teuchos::RCP<Teuchos::ParameterList> lsparams = Teuchos::null;
    switch (probtype)
    {
      case Core::ProblemType::level_set:
        lsparams = Teuchos::make_rcp<Teuchos::ParameterList>(prbdyn);
        break;
      default:
      {
        if (lsparams.is_null())
          lsparams = Teuchos::make_rcp<Teuchos::ParameterList>(
              Global::Problem::instance()->level_set_control());
        // overrule certain parameters for coupled problems
        // this has already been ensured for scatratimeparams, but has also been ensured for the
        // level-set parameter in a hybrid approach time step size
        lsparams->set<double>("TIMESTEP", prbdyn.get<double>("TIMESTEP"));
        // maximum simulation time
        lsparams->set<double>("MAXTIME", prbdyn.get<double>("MAXTIME"));
        // maximum number of timesteps
        lsparams->set<int>("NUMSTEP", prbdyn.get<int>("NUMSTEP"));
        // restart
        lsparams->set<int>("RESTARTEVRY", prbdyn.get<int>("RESTARTEVRY"));
        // solution output
        lsparams->set<int>("RESULTSEVRY", prbdyn.get<int>("RESULTSEVRY"));

        break;
      }
    }

    switch (timintscheme)
    {
      case Inpar::ScaTra::timeint_one_step_theta:
      {
        // create instance of time integration class (call the constructor)
        scatra_ = Teuchos::make_rcp<ScaTra::LevelSetTimIntOneStepTheta>(
            discret, solver, lsparams, scatratimeparams, extraparams, output);
        break;
      }
      case Inpar::ScaTra::timeint_stationary:
      {
        // create instance of time integration class (call the constructor)
        switch (probtype)
        {
          case Core::ProblemType::level_set:
          {
            FOUR_C_THROW(
                "Stationary time integration scheme only supported for a selection of coupled "
                "level-set problems!");
            exit(EXIT_FAILURE);
          }
          default:
          {
            scatra_ = Teuchos::make_rcp<ScaTra::LevelSetTimIntStationary>(
                discret, solver, lsparams, scatratimeparams, extraparams, output);
            break;
          }
        }
        break;
      }
      case Inpar::ScaTra::timeint_gen_alpha:
      {
        switch (probtype)
        {
          default:
            FOUR_C_THROW("Unknown time-integration scheme for level-set problem");
            exit(EXIT_FAILURE);
        }

        break;
      }
      default:
        FOUR_C_THROW("Unknown time-integration scheme for level-set problem");
        break;
    }  // switch(timintscheme)
  }

  // cardiac monodomain
  else if (probtype == Core::ProblemType::cardiac_monodomain or
           (probtype == Core::ProblemType::ssi and
               Teuchos::getIntegralValue<Inpar::SSI::ScaTraTimIntType>(
                   Global::Problem::instance()->ssi_control_params(), "SCATRATIMINTTYPE") ==
                   Inpar::SSI::ScaTraTimIntType::cardiac_monodomain))
  {
    auto cmonoparams =
        rcp(new Teuchos::ParameterList(Global::Problem::instance()->ep_control_params()));

    // HDG implements all time stepping schemes within gen-alpha
    if (Global::Problem::instance()->spatial_approximation_type() ==
        Core::FE::ShapeFunctionType::hdg)
    {
      scatra_ = Teuchos::make_rcp<ScaTra::TimIntCardiacMonodomainHDG>(
          discret, solver, cmonoparams, scatratimeparams, extraparams, output);
    }
    else
    {
      switch (timintscheme)
      {
        case Inpar::ScaTra::timeint_gen_alpha:
        {
          // create instance of time integration class (call the constructor)
          scatra_ = Teuchos::make_rcp<ScaTra::TimIntCardiacMonodomainGenAlpha>(
              discret, solver, cmonoparams, scatratimeparams, extraparams, output);
          break;
        }
        case Inpar::ScaTra::timeint_one_step_theta:
        {
          // create instance of time integration class (call the constructor)
          scatra_ = Teuchos::make_rcp<ScaTra::TimIntCardiacMonodomainOST>(
              discret, solver, cmonoparams, scatratimeparams, extraparams, output);
          break;
        }
        case Inpar::ScaTra::timeint_bdf2:
        {
          // create instance of time integration class (call the constructor)
          scatra_ = Teuchos::make_rcp<ScaTra::TimIntCardiacMonodomainBDF2>(
              discret, solver, cmonoparams, scatratimeparams, extraparams, output);
          break;
        }
        default:
          FOUR_C_THROW("Unknown time integration scheme for cardiac monodomain problem!");
          break;
      }  // switch(timintscheme)
    }
  }
  else if (probtype == Core::ProblemType::poromultiphasescatra)
  {
    switch (timintscheme)
    {
      case Inpar::ScaTra::timeint_gen_alpha:
      {
        // create instance of time integration class (call the constructor)
        scatra_ = Teuchos::make_rcp<ScaTra::ScaTraTimIntPoroMultiGenAlpha>(
            discret, solver, Teuchos::null, scatratimeparams, extraparams, output);
        break;
      }
      case Inpar::ScaTra::timeint_one_step_theta:
      {
        // create instance of time integration class (call the constructor)
        scatra_ = Teuchos::make_rcp<ScaTra::ScaTraTimIntPoroMultiOST>(
            discret, solver, Teuchos::null, scatratimeparams, extraparams, output);
        break;
      }
      case Inpar::ScaTra::timeint_bdf2:
      {
        // create instance of time integration class (call the constructor)
        scatra_ = Teuchos::make_rcp<ScaTra::ScaTraTimIntPoroMultiBDF2>(
            discret, solver, Teuchos::null, scatratimeparams, extraparams, output);
        break;
      }
      case Inpar::ScaTra::timeint_stationary:
      {
        // create instance of time integration class (call the constructor)
        scatra_ = Teuchos::make_rcp<ScaTra::ScaTraTimIntPoroMultiStationary>(
            discret, solver, Teuchos::null, scatratimeparams, extraparams, output);
        break;
      }
      default:
        FOUR_C_THROW("Unknown time integration scheme for porous medium multiphase problem!");
        break;
    }  // switch(timintscheme)
  }
  // everything else
  else
  {
    // HDG implements all time stepping schemes within gen-alpha
    if (Global::Problem::instance()->spatial_approximation_type() ==
        Core::FE::ShapeFunctionType::hdg)
    {
      switch (timintscheme)
      {
        case Inpar::ScaTra::timeint_one_step_theta:
        case Inpar::ScaTra::timeint_bdf2:
        case Inpar::ScaTra::timeint_gen_alpha:
        {
          scatra_ = Teuchos::make_rcp<ScaTra::TimIntHDG>(
              discret, solver, scatratimeparams, extraparams, output);
          break;
        }
        case Inpar::ScaTra::timeint_stationary:
        {
          scatra_ = Teuchos::make_rcp<ScaTra::TimIntStationaryHDG>(
              discret, solver, scatratimeparams, extraparams, output);
          break;
        }
        default:
        {
          FOUR_C_THROW("Unknown time-integration scheme for HDG scalar transport problem");
          break;
        }
      }
    }
    else
    {
      switch (timintscheme)
      {
        case Inpar::ScaTra::timeint_stationary:
        {
          // create instance of time integration class (call the constructor)
          scatra_ = Teuchos::make_rcp<ScaTra::TimIntStationary>(
              discret, solver, scatratimeparams, extraparams, output);
          break;
        }
        case Inpar::ScaTra::timeint_one_step_theta:
        {
          // create instance of time integration class (call the constructor)
          scatra_ = Teuchos::make_rcp<ScaTra::TimIntOneStepTheta>(
              discret, solver, scatratimeparams, extraparams, output);
          break;
        }
        case Inpar::ScaTra::timeint_bdf2:
        {
          // create instance of time integration class (call the constructor)
          scatra_ = Teuchos::make_rcp<ScaTra::TimIntBDF2>(
              discret, solver, scatratimeparams, extraparams, output);
          break;
        }
        case Inpar::ScaTra::timeint_gen_alpha:
        {
          // create instance of time integration class (call the constructor)
          scatra_ = Teuchos::make_rcp<ScaTra::TimIntGenAlpha>(
              discret, solver, scatratimeparams, extraparams, output);
          break;
        }
        default:
          FOUR_C_THROW("Unknown time-integration scheme for scalar transport problem");
          break;
      }  // switch(timintscheme)
    }
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Adapter::ScaTraBaseAlgorithm::init()
{
  set_is_setup(false);

  // initialize scatra time integrator
  scatra_->init();

  set_is_init(true);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Adapter::ScaTraBaseAlgorithm::setup()
{
  check_is_init();

  // setup the time integrator
  scatra_->setup();

  // get the parameter list
  auto scatradyn = scatra_->scatra_parameter_list();
  // get the discretization
  auto discret = scatra_->discretization();

  // -------------------------------------------------------------------
  // what's the current problem type?
  // -------------------------------------------------------------------
  auto probtype = Global::Problem::instance()->get_problem_type();

  // prepare fixing the null space for electrochemistry and sti
  if (probtype == Core::ProblemType::elch or
      (probtype == Core::ProblemType::sti and discret->name() == "scatra" and
          Teuchos::getIntegralValue<Inpar::STI::ScaTraTimIntType>(
              Global::Problem::instance()->sti_dynamic_params(), "SCATRATIMINTTYPE") ==
              Inpar::STI::ScaTraTimIntType::elch))
  {
    auto elchparams = Teuchos::make_rcp<Teuchos::ParameterList>(
        Global::Problem::instance()->elch_control_params());

    // create a 2nd solver for block-preconditioning if chosen from input
    if (elchparams->get<bool>("BLOCKPRECOND"))
    {
      const auto& solver = scatra_->solver();

      const int linsolvernumber = scatradyn->get<int>("LINEAR_SOLVER");
      const auto prec = Teuchos::getIntegralValue<Core::LinearSolver::PreconditionerType>(
          Global::Problem::instance()->solver_params(linsolvernumber), "AZPREC");
      if (prec != Core::LinearSolver::PreconditionerType::cheap_simple)  // TODO adapt error message
      {
        FOUR_C_THROW(
            "If SIMPLER flag is set to YES you can only use CheapSIMPLE as preconditioner in your "
            "fluid solver. Choose CheapSIMPLE in the SOLVER %i block in your dat file.",
            linsolvernumber);
      }

      solver->params().sublist("CheapSIMPLE Parameters").set("Prec Type", "CheapSIMPLE");
      solver->params().set(
          "ELCH", true);  // internal CheapSIMPLE modus for ML null space computation

      // add Inverse1 block for velocity dofs
      // tell Inverse1 block about nodal_block_information
      // In contrary to contact/meshtying problems this is necessary here, since we originally have
      // built the null space for the whole problem (velocity and pressure dofs). However, if we
      // split the matrix into velocity and pressure block, we have to adapt the null space
      // information for the subblocks. Therefore we need the nodal block information in the first
      // subblock for the velocities. The pressure null space is trivial to be built using a
      // constant vector
      auto& inv1 = solver->params().sublist("CheapSIMPLE Parameters").sublist("Inverse1");
      inv1.sublist("nodal_block_information") = solver->params().sublist("nodal_block_information");
    }
  }

  set_is_setup(true);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Core::UTILS::ResultTest> Adapter::ScaTraBaseAlgorithm::create_scatra_field_test()
{
  return scatra_->create_scatra_field_test();
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Adapter::ScaTraBaseAlgorithm::check_is_setup() const
{
  if (not is_setup()) FOUR_C_THROW("setup() was not called.");
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Adapter::ScaTraBaseAlgorithm::check_is_init() const
{
  if (not is_init()) FOUR_C_THROW("init(...) was not called.");
}

FOUR_C_NAMESPACE_CLOSE

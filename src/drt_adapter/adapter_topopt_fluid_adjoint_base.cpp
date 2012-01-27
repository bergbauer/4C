/*!------------------------------------------------------------------------------------------------*
\file adapter_topopt_fluid_adjoint_base.cpp

\brief 

<pre>
Maintainer: Martin Winklmaier
            winklmaier@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15241
</pre>
 *------------------------------------------------------------------------------------------------*/

#ifdef CCADISCRET

#include "adapter_topopt_fluid_adjoint_base.H"

#include "../drt_fluid/drt_periodicbc.H"
#include "../drt_inpar/drt_validparameters.H"
#include "../drt_inpar/inpar_fluid.H"
#include "../drt_io/io.H"
#include "../drt_io/io_control.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../linalg/linalg_solver.H"

#include "adapter_topopt_fluid_adjoint_impl.H"

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
ADAPTER::TopOptFluidAdjointAlgorithm::TopOptFluidAdjointAlgorithm(
    const Teuchos::ParameterList& prbdyn
    )
{
  SetupAdjointFluid(prbdyn);
  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
ADAPTER::TopOptFluidAdjointAlgorithm::~TopOptFluidAdjointAlgorithm()
{
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::TopOptFluidAdjointAlgorithm::ReadRestart(int step)
{
  AdjointFluidField()->ReadRestart(step);
  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::TopOptFluidAdjointAlgorithm::SetupAdjointFluid(const Teuchos::ParameterList& prbdyn)
{
  Teuchos::RCP<Teuchos::Time> t = Teuchos::TimeMonitor::getNewTimer("ADAPTER::TopOptFluidAdjointAlgorithm::SetupFluid");
  Teuchos::TimeMonitor monitor(*t);

  // -------------------------------------------------------------------
  // access the discretization
  // -------------------------------------------------------------------
  RCP<DRT::Discretization> actdis = DRT::Problem::Instance()->Dis(genprob.numff,0);


  // -------------------------------------------------------------------
  // connect degrees of freedom for periodic boundary conditions
  // -------------------------------------------------------------------
  RCP<map<int,vector<int> > > pbcmapmastertoslave = Teuchos::rcp(new map<int,vector<int> > ());

  PeriodicBoundaryConditions pbc(actdis);
  pbc.UpdateDofsForPeriodicBoundaryConditions();

  pbcmapmastertoslave = pbc.ReturnAllCoupledColNodes();

  // -------------------------------------------------------------------
  // set degrees of freedom in the discretization
  // -------------------------------------------------------------------
  if (!actdis->HaveDofs())
    dserror("adjoint field solved after fluid field");

  // -------------------------------------------------------------------
  // context for output and restart
  // -------------------------------------------------------------------
  RCP<IO::DiscretizationWriter> output =
    rcp(new IO::DiscretizationWriter(actdis));

  // -------------------------------------------------------------------
  // set some pointers and variables
  // -------------------------------------------------------------------
  //const Teuchos::ParameterList& probtype = DRT::Problem::Instance()->ProblemTypeParams();
  const Teuchos::ParameterList& probsize = DRT::Problem::Instance()->ProblemSizeParams();
  const Teuchos::ParameterList& fdyn     = DRT::Problem::Instance()->FluidDynamicParams();

  if (actdis->Comm().MyPID()==0)
    DRT::INPUT::PrintDefaultParameters(std::cout, fdyn);

  // -------------------------------------------------------------------
  // create a solver
  // -------------------------------------------------------------------
  Teuchos::RCP<LINALG::Solver> solver =
      rcp(new LINALG::Solver(DRT::Problem::Instance()->FluidSolverParams(),
          actdis->Comm(),
          DRT::Problem::Instance()->ErrorFile()->Handle()));

  // compute null space information, no block matrix
  actdis->ComputeNullSpaceIfNecessary(solver->Params(),true);

  // -------------------------------------------------------------------
  // create a second solver for SIMPLER preconditioner if chosen from input
  // -------------------------------------------------------------------
  if (DRT::INPUT::IntegralValue<int>(fdyn,"SIMPLER"))
  {
    dserror("not handled for adjoints until now");
    // add Inverse1 block for velocity dofs
    Teuchos::ParameterList& inv1 = solver->Params().sublist("Inverse1");
    inv1 = solver->Params();
    inv1.remove("SIMPLER",false); // not necessary
    inv1.remove("Inverse1",false);
    // add Inverse2 block for pressure dofs
    solver->PutSolverParamsToSubParams("Inverse2", DRT::Problem::Instance()->FluidPressureSolverParams());
    // use CheapSIMPLE preconditioner (hardwired, change me for others)
    solver->Params().sublist("CheapSIMPLE Parameters").set("Prec Type","CheapSIMPLE");
    solver->Params().set("FLUID",true);
  }

  // -------------------------------------------------------------------
  // set parameters in list required for all schemes
  // -------------------------------------------------------------------
  RCP<ParameterList> fluidadjointtimeparams = rcp(new ParameterList());

  // --------------------provide info about periodic boundary conditions
  fluidadjointtimeparams->set<RCP<map<int,vector<int> > > >("periodic bc",pbcmapmastertoslave);

  fluidadjointtimeparams->set<int>("Simple Preconditioner",DRT::INPUT::IntegralValue<int>(fdyn,"SIMPLER"));
  fluidadjointtimeparams->set<int>("AMG(BS) Preconditioner",DRT::INPUT::IntegralValue<INPAR::SOLVER::AzPrecType>(DRT::Problem::Instance()->FluidSolverParams(),"AZPREC"));

  // -------------------------------------- number of degrees of freedom
  // number of degrees of freedom
  fluidadjointtimeparams->set<int>("number of velocity degrees of freedom" ,probsize.get<int>("DIM"));

  // -------------------------------------------------- time integration
  // note: here, the values are taken out of the problem-dependent ParameterList prbdyn
  // (which also can be fluiddyn itself!)

  // the default time step size
  fluidadjointtimeparams->set<double> ("time step size"      ,-1.0*prbdyn.get<double>("TIMESTEP"));
  // maximum simulation time
  fluidadjointtimeparams->set<double> ("total time"          ,prbdyn.get<double>("MAXTIME"));
  // maximum number of timesteps
  fluidadjointtimeparams->set<int>    ("max number timesteps",prbdyn.get<int>("NUMSTEP"));

  // -------- additional parameters in list for generalized-alpha scheme
#if 1
  // parameter alpha_M
  fluidadjointtimeparams->set<double> ("alpha_M", fdyn.get<double>("ALPHA_M"));
  // parameter alpha_F
  fluidadjointtimeparams->set<double> ("alpha_F", fdyn.get<double>("ALPHA_F"));
  // parameter gamma
  fluidadjointtimeparams->set<double> ("gamma",   fdyn.get<double>("GAMMA"));
#else
  // parameter alpha_M
  fluidtimeparams->set<double> ("alpha_M", 1.-prbdyn.get<double>("ALPHA_M"));
  // parameter alpha_F
  fluidtimeparams->set<double> ("alpha_F", 1.-prbdyn.get<double>("ALPHA_F"));
  // parameter gamma
  fluidtimeparams->set<double> ("gamma",   prbdyn.get<double>("GAMMA"));
#endif

  // ---------------------------------------------- nonlinear iteration
  // type of predictor
  fluidadjointtimeparams->set<string>          ("predictor"                 ,fdyn.get<string>("PREDICTOR"));
  // set linearisation scheme
  fluidadjointtimeparams->set<int>("Linearisation", DRT::INPUT::IntegralValue<INPAR::FLUID::LinearisationAction>(fdyn,"NONLINITER"));
  // maximum number of nonlinear iteration steps
  fluidadjointtimeparams->set<int>             ("max nonlin iter steps"     ,fdyn.get<int>("ITEMAX"));
  // stop nonlinear iteration when both incr-norms are below this bound
  fluidadjointtimeparams->set<double>          ("tolerance for nonlin iter" ,fdyn.get<double>("CONVTOL"));
  // set convergence check
  fluidadjointtimeparams->set<string>          ("CONVCHECK"  ,fdyn.get<string>("CONVCHECK"));
  // set adaptive linear solver tolerance
  fluidadjointtimeparams->set<bool>            ("ADAPTCONV",DRT::INPUT::IntegralValue<int>(fdyn,"ADAPTCONV")==1);
  fluidadjointtimeparams->set<double>          ("ADAPTCONV_BETTER",fdyn.get<double>("ADAPTCONV_BETTER"));

  // ----------------------------------------------- restart and output
//  // restart
//  fluidadjointtimeparams->set ("write restart every", prbdyn.get<int>("RESTARTEVRY"));
  // solution output
  fluidadjointtimeparams->set ("write solution every", prbdyn.get<int>("UPRES"));

  // -----------evaluate error for test flows with analytical solutions
  INPAR::FLUID::InitialField initfield = DRT::INPUT::IntegralValue<INPAR::FLUID::InitialField>(fdyn,"INITIALFIELD");
//  fluidadjointtimeparams->set<int>("eval err for analyt sol", initfield);

  // ------------------------------------------ form of convective term
  fluidadjointtimeparams->set<string> ("form of convective term", fdyn.get<string>("CONVFORM"));

  // ------------------------------------ potential Neumann inflow terms
  fluidadjointtimeparams->set<string> ("Neumann inflow",fdyn.get<string>("NEUMANNINFLOW"));

  //--------------------------------------analytical error evaluation
  fluidadjointtimeparams->set<int>("calculate error",
      Teuchos::getIntegralValue<int>(fdyn,"CALCERROR"));

  // -----------------------sublist containing stabilization parameters
  fluidadjointtimeparams->sublist("STABILIZATION")=fdyn.sublist("STABILIZATION");

  // -------------------------------------------------------------------
  // additional parameters and algorithm call depending on respective
  // time-integration (or stationary) scheme
  // -------------------------------------------------------------------
  INPAR::FLUID::TimeIntegrationScheme timeint = DRT::INPUT::IntegralValue<INPAR::FLUID::TimeIntegrationScheme>(fdyn,"TIMEINTEGR");

  // -------------------------------------------------------------------
  // additional parameters and algorithm call depending on respective
  // time-integration (or stationary) scheme
  // -------------------------------------------------------------------
  if(timeint == INPAR::FLUID::timeint_stationary or
     timeint == INPAR::FLUID::timeint_one_step_theta or
     timeint == INPAR::FLUID::timeint_bdf2 or
     timeint == INPAR::FLUID::timeint_afgenalpha or
     timeint == INPAR::FLUID::timeint_npgenalpha
    )
  {
    // -----------------------------------------------------------------
    // set additional parameters in list for
    // one-step-theta/BDF2/af-generalized-alpha/stationary scheme
    // -----------------------------------------------------------------
    // type of time-integration (or stationary) scheme
    fluidadjointtimeparams->set<int>("time int algo",timeint);
    // parameter theta for time-integration schemes
    fluidadjointtimeparams->set<double>           ("theta"                    ,fdyn.get<double>("THETA"));
    // number of steps for potential start algorithm
    fluidadjointtimeparams->set<int>              ("number of start steps"    ,fdyn.get<int>("NUMSTASTEPS"));
    // parameter theta for potential start algorithm
    fluidadjointtimeparams->set<double>           ("start theta"              ,fdyn.get<double>("START_THETA"));

    fluidadjointtimeparams->set<FILE*>("err file",DRT::Problem::Instance()->ErrorFile()->Handle());

    //------------------------------------------------------------------
    // create all vectors and variables associated with the time
    // integration (call the constructor);
    // the only parameter from the list required here is the number of
    // velocity degrees of freedom

    if (true)
    {
      int fluidsolver = DRT::INPUT::IntegralValue<int>(fdyn,"FLUID_SOLVER");
      switch(fluidsolver)
      {
      case fluid_solver_implicit:
        adjoint_ = rcp(new ADAPTER::FluidAdjointImpl(actdis,solver,fluidadjointtimeparams,output));
        break;
      case fluid_solver_pressurecorrection:
      case fluid_solver_pressurecorrection_semiimplicit:
        dserror("not implemented for adjoint field");
        break;
      default:
        dserror("fluid solving strategy unknown.");
      }
    }
  }
  else if (timeint == INPAR::FLUID::timeint_gen_alpha)
  {
    dserror("not implemented for adjoint field");
  }
  else
  {
    dserror("Unknown time integration for fluid\n");
  }

  // set initial field by given function
  // we do this here, since we have direct access to all necessary parameters
  if(initfield != INPAR::FLUID::initfield_zero_field)
  {
    int startfuncno = fdyn.get<int>("STARTFUNCNO");
    if (initfield != INPAR::FLUID::initfield_field_by_function and
        initfield != INPAR::FLUID::initfield_disturbed_field_from_function)
    {
      startfuncno=-1;
    }
    adjoint_->SetInitialFlowField(initfield,startfuncno);
  }
  return;
}
#endif  // #ifdef CCADISCRET

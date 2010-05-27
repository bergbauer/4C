
/*!----------------------------------------------------------------------
\file artnetexplicitintegration.cpp
\brief Control routine for artery solvers,

     including solvers based on

     o two-step Taylor-Galerking scheme


<pre>
Maintainer: Mahmoud Ismail
            ismail@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15268
</pre>
*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include <stdio.h>

#include "../drt_art_net/artnetexplicitintegration.H"

#include "../drt_lib/drt_condition_utils.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/linalg_ana.H"
#include "../drt_lib/linalg_utils.H"
#include "../drt_lib/linalg_solver.H"
#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/drt_nodematchingoctree.H"
#include "../drt_lib/drt_function.H"
#include "art_junction.H"

//#include "../drt_fluid/fluidimpedancecondition.H"



//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 |  Constructor (public)                                    ismail 01/09|
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//

ART::ArtNetExplicitTimeInt::ArtNetExplicitTimeInt(RCP<DRT::Discretization>  actdis,
                                                  LINALG::Solver  &         solver,
                                                  ParameterList&            params,
                                                  IO::DiscretizationWriter& output)
  :
  // call constructor for "nontrivial" objects
  discret_(actdis),
  solver_ (solver),
  params_ (params),
  output_ (output),
  time_(0.0),
  step_(0),
  uprestart_(params.get("write restart every", -1)),
  upres_(params.get("write solution every", -1)),
  coupledTo3D_(false)
{

  // -------------------------------------------------------------------
  // get the processor ID from the communicator
  // -------------------------------------------------------------------
  myrank_  = discret_->Comm().MyPID();

  // time measurement: initialization
  if(!coupledTo3D_)
  {
    TEUCHOS_FUNC_TIME_MONITOR(" + initialization");
  }

  // -------------------------------------------------------------------
  // get the basic parameters first
  // -------------------------------------------------------------------
  // time-step size
  dtp_ = dta_ = params_.get<double>("time step size");
  // maximum number of timesteps
  stepmax_  = params_.get<int>   ("max number timesteps");
  // maximum simulation time
  maxtime_  = dtp_*double(stepmax_);


  // ensure that degrees of freedom in the discretization have been set
  if (!discret_->Filled() || !actdis->HaveDofs()) discret_->FillComplete();

  // -------------------------------------------------------------------
  // Force the reducesd 1d arterial network discretization to run on
  // and only one cpu
  // -------------------------------------------------------------------
  // reduce the node row map into processor 0
  const Epetra_Map noderowmap_1_proc = * LINALG::AllreduceEMap(*discret_->NodeRowMap(), 0);
  // update the discetization by redistributing the new row map
  discret_->Redistribute(noderowmap_1_proc,noderowmap_1_proc);

  // -------------------------------------------------------------------
  // get a vector layout from the discretization to construct matching
  // vectors and matrices
  //                 local <-> global dof numbering
  // -------------------------------------------------------------------
  const Epetra_Map* dofrowmap  = discret_->DofRowMap();

  //  const Epetra_Map* dofcolmap  = discret_->DofColMap();

  // -------------------------------------------------------------------
  // get a vector layout from the discretization to construct matching
  // vectors and matrices
  //                 local <-> global node numbering
  // -------------------------------------------------------------------
  const Epetra_Map* noderowmap = discret_->NodeRowMap();


  // -------------------------------------------------------------------
  // get a vector layout from the discretization for a vector which only
  // contains the volumetric flow rate dofs and for one vector which only
  // contains cross-sectional area degrees of freedom.
  // -------------------------------------------------------------------


  // This is a first estimate for the number of non zeros in a row of
  // the matrix. Each node has 3 adjacent nodes (including itself), each
  // with 2 dofs. (3*2=6)
  // We do not need the exact number here, just for performance reasons
  // a 'good' estimate

  // initialize standard (stabilized) system matrix
  sysmat_  = Teuchos::rcp(new LINALG::SparseMatrix(*dofrowmap,6,false,true));


  // Vectors passed to the element
  // -----------------------------
  // Volumetric flow rate at time n+1, n and n-1
  qanp_          = LINALG::CreateVector(*dofrowmap,true);
  qan_           = LINALG::CreateVector(*dofrowmap,true);
  qanm_          = LINALG::CreateVector(*dofrowmap,true);

  qan_3D_        = LINALG::CreateVector(*dofrowmap,true);

  // Vectors associated to boundary conditions
  // -----------------------------------------
  Wfnp_          = LINALG::CreateVector(*noderowmap,true);
  Wfn_           = LINALG::CreateVector(*noderowmap,true);
  Wfnm_          = LINALG::CreateVector(*noderowmap,true);
  Wbnp_          = LINALG::CreateVector(*noderowmap,true);
  Wbn_           = LINALG::CreateVector(*noderowmap,true);
  Wbnm_          = LINALG::CreateVector(*noderowmap,true);


  // a vector of zeros to be used to enforce zero dirichlet boundary conditions
  // This part might be optimized later
  bcval_   = LINALG::CreateVector(*dofrowmap,true);
  dbctog_  = LINALG::CreateVector(*dofrowmap,true);

  // Vectors used for postporcesing visualization
  // --------------------------------------------
  qn_           = LINALG::CreateVector(*noderowmap,true);
  pn_           = LINALG::CreateVector(*noderowmap,true);
  

  // right hand side vector and right hand side corrector
  rhs_     = LINALG::CreateVector(*dofrowmap,true);
  // create the junction boundary conditions
  ParameterList junparams;

  junc_nodal_vals_=rcp(new map<const int, RCP<ART::UTILS::JunctionNodeParams> >);

  junparams.set<RCP<map<const int, RCP<ART::UTILS::JunctionNodeParams> > > >("Junctions Parameters",junc_nodal_vals_);

  artjun_ = rcp(new UTILS::ArtJunctionWrapper(discret_, output_, junparams, dta_) );

  // create the gnuplot export conditions
  artgnu_ = rcp(new ART::UTILS::ArtWriteGnuplotWrapper(discret_,junparams));

  // ---------------------------------------------------------------------------------------
  // Initialize all the arteries' cross-sectional areas to the initial crossectional area Ao
  // and the volumetric flow rate to 0
  // ---------------------------------------------------------------------------------------
  ParameterList eleparams;
  discret_->ClearState();
  discret_->SetState("qanp",qanp_);

  // loop all elements on this proc (including ghosted ones)
  int localNode;                                                

  for (int nele=0;nele<discret_->NumMyColElements();++nele)
  {
    // get the element
    DRT::Element* ele = discret_->lColElement(nele);

    // get element location vector, dirichlet flags and ownerships
    vector<int> lm;
    //vector<int> lmowner;
    RCP<vector<int> > lmowner = rcp(new vector<int>);
    ele->LocationVector(*discret_,lm,*lmowner);

    // loop all nodes of this element, add values to the global vectors
    eleparams.set("qa0",qanp_);
    eleparams.set("lmowner",lmowner);
    eleparams.set("action","get_initail_artery_state");
    discret_->Evaluate(eleparams,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null);

    //initialize the characteristic wave maps
    if(nele == discret_->NumMyColElements()-1)
    {
      localNode = 1;
    }
    else
      localNode = 0;
  }


#if 0
  cout<<"|**************************************************************************|"<<endl;
  cout<<"|******************** The Initialize Vector qanp is ***********************|"<<endl;
  cout<<"|**************************************************************************|"<<endl;
  cout<<*qanp_<<endl;
  cout<<"|**************************************************************************|"<<endl;
  cout<<*Wfnp_<<endl;
#endif

} // ArtNetExplicitTimeInt::ArtNetExplicitTimeInt


//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 | Start the time integration.                                          |
 |                                                          ismail 12/09|
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
void ART::ArtNetExplicitTimeInt::Integrate()
{
  RCP<ParameterList> param;
  Integrate(false, param);
} // ArtNetExplicitTimeInt::Integrate


//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 | Start the time integration.                                          |
 |                                                          ismail 06/09|
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
void ART::ArtNetExplicitTimeInt::Integrate(bool CoupledTo3D,
                                           RCP<ParameterList> CouplingParams)
{
  coupledTo3D_ = CoupledTo3D;
  if (CoupledTo3D && CouplingParams.get() == NULL)
  {
    dserror("Coupling parameter list is not allowed to be empty, If a 3-D/reduced-D coupling is defined\n");
  }

  TimeLoop(CoupledTo3D,CouplingParams);

  // print the results of time measurements
  if (!coupledTo3D_)
  {
    TimeMonitor::summarize();
  }

  return;
} // ArtNetExplicitTimeInt::Integrate



//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 | contains the time loop                                   ismail 06/09|
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
void ART::ArtNetExplicitTimeInt::TimeLoop(bool CoupledTo3D,
                                          RCP<ParameterList> CouplingTo3DParams)
{
  coupledTo3D_ = CoupledTo3D;
  // time measurement: time loop
  if(!coupledTo3D_)
  {
    TEUCHOS_FUNC_TIME_MONITOR(" + time loop");
  }
  
  while (step_<stepmax_ and time_<maxtime_)
  {
    PrepareTimeStep();
    // -------------------------------------------------------------------
    //                       output to screen
    // -------------------------------------------------------------------
    if (myrank_==0)
    {
      if(!coupledTo3D_)
      {
        printf("TIME: %11.4E/%11.4E  DT = %11.4E   Solving Artery    STEP = %4d/%4d \n",
               time_,maxtime_,dta_,step_,stepmax_);
      }
      else
      {
         printf("SUBSCALE_TIME: %11.4E/%11.4E  SUBSCALE_DT = %11.4E   Solving Artery    SUBSCALE_STEP = %4d/%4d \n",
               time_,maxtime_,dta_,step_,stepmax_);
      }
    }

    Solve(CouplingTo3DParams);

    // -------------------------------------------------------------------
    //                         update solution
    //        current solution becomes old solution of next timestep
    // -------------------------------------------------------------------
    TimeUpdate();

    // -------------------------------------------------------------------
    //  lift'n'drag forces, statistics time sample and output of solution
    //  and statistics
    // -------------------------------------------------------------------
    if (!CoupledTo3D)
    {
      Output(CoupledTo3D,CouplingTo3DParams);
    }

    // -------------------------------------------------------------------
    //                       update time step sizes
    // -------------------------------------------------------------------
    dtp_ = dta_;

    // -------------------------------------------------------------------
    //                    stop criterium for timeloop
    // -------------------------------------------------------------------
    if (CoupledTo3D)
    {
      break;
    }
  }

} // ArtNetExplicitTimeInt::TimeLoop


//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 | setup the variables to do a new time step                ismail 06/09|
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
void ART::ArtNetExplicitTimeInt::PrepareTimeStep()
{
  // -------------------------------------------------------------------
  //              set time dependent parameters
  // -------------------------------------------------------------------
  step_ += 1;
  time_ += dta_;
}


//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 | the solver for artery                                   ismail 06/09 |
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*
Some detials!!
*/
void ART::ArtNetExplicitTimeInt::Solve(Teuchos::RCP<ParameterList> CouplingTo3DParams)
{
  // time measurement: Artery
  if(!coupledTo3D_)
  {
    TEUCHOS_FUNC_TIME_MONITOR("   + solving artery");
  }


  // -------------------------------------------------------------------
  // call elements to calculate system matrix
  // -------------------------------------------------------------------

  // get cpu time
  //  const double tcpuele = Teuchos::Time::wallTime();

  {
    // time measurement: element
    if(!coupledTo3D_)
    {
      TEUCHOS_FUNC_TIME_MONITOR("      + element calls");
    }

    // set both system matrix and rhs vector to zero
    sysmat_->Zero();
    rhs_->PutScalar(0.0);


    // create the parameters for the discretization
    ParameterList eleparams;

    // action for elements
    eleparams.set("action","calc_sys_matrix_rhs");
    eleparams.set("time step size",dta_);

    // other parameters that might be needed by the elements
    eleparams.set("total time",time_);

    // set vector values needed by elements
    discret_->ClearState();
    discret_->SetState("qanp",qanp_);


    // call standard loop over all elements
    discret_->Evaluate(eleparams,sysmat_,rhs_);
    discret_->ClearState();

    // finalize the complete matrix
    sysmat_->Complete();
  }
  // end time measurement for element

  // -------------------------------------------------------------------
  // call elements to calculate the Riemann problem
  // -------------------------------------------------------------------
  {
    // create the parameters for the discretization
    ParameterList eleparams;

    // action for elements
    eleparams.set("action","solve_riemann_problem");

    // set vecotr values needed by elements
    discret_->ClearState();
    discret_->SetState("qanp",qanp_);

    eleparams.set("time step size",dta_);
    eleparams.set("Wfnp",Wfnp_);
    eleparams.set("Wbnp",Wbnp_);

    eleparams.set("total time",time_);
    eleparams.set<RCP<map<const int, RCP<ART::UTILS::JunctionNodeParams> > > >("Junctions Parameters",junc_nodal_vals_);

    // call standard loop over all elements
    discret_->Evaluate(eleparams,sysmat_,rhs_);
  }

  // -------------------------------------------------------------------
  // Solve the boundary conditions 
  // -------------------------------------------------------------------
  bcval_->PutScalar(0.0);
  dbctog_->PutScalar(0.0);
  // Solve terminal BCs

  {
    // create the parameters for the discretization
    ParameterList eleparams;

    // action for elements
    eleparams.set("action","set_term_bc");

    // set vecotr values needed by elements
    discret_->ClearState();
    discret_->SetState("qanp",qanp_);

    eleparams.set("time step size",dta_);
    eleparams.set("total time",time_);
    eleparams.set("bcval",bcval_);
    eleparams.set("dbctog",dbctog_);
    eleparams.set("Wfnp",Wfnp_);
    eleparams.set("Wbnp",Wbnp_);
    eleparams.set<RCP<map<const int, RCP<ART::UTILS::JunctionNodeParams> > > >("Junctions Parameters",junc_nodal_vals_);

    // Add the parameters to solve terminal BCs coupled to 3D fluid boundary
    eleparams.set("coupling with 3D fluid params",CouplingTo3DParams);
    
    // solve junction boundary conditions
    artjun_->Solve(eleparams);

    // call standard loop over all elements
    discret_->Evaluate(eleparams,sysmat_,rhs_);
  }


  // -------------------------------------------------------------------
  // Apply the BCs to the system matrix and rhs
  // -------------------------------------------------------------------
  {
    // time measurement: application of dbc
  if(!coupledTo3D_)
  {
    TEUCHOS_FUNC_TIME_MONITOR("      + apply DBC");
  }

#if 0
    cout<<"Boundary values are: "<<endl<<*bcval_<<endl;
    cout<<"Boundary toggels are: "<<endl<<*dbctog_<<endl;
#endif

    LINALG::ApplyDirichlettoSystem(sysmat_,qanp_,rhs_,bcval_,dbctog_);
  }

  //-------solve for total new velocities and pressures
  // get cpu time
  const double tcpusolve = Teuchos::Time::wallTime();
  {
    // time measurement: solver
    if(!coupledTo3D_)
    {
      TEUCHOS_FUNC_TIME_MONITOR("      + solver calls");
    }

#if 0  // Exporting some values for debugging purposes


    RCP<LINALG::SparseMatrix> A_debug = Teuchos::rcp_dynamic_cast<LINALG::SparseMatrix>(sysmat_);
    if (A_debug != Teuchos::null)
    {
      // print to screen
      (A_debug->EpetraMatrix())->Print(cout);
    }

#endif 
    // call solver
    solver_.Solve(sysmat_->EpetraOperator(),qanp_,rhs_,true,true);
  }


  // end time measurement for solver
  dtsolve_ = Teuchos::Time::wallTime() - tcpusolve;

  if (myrank_ == 0)
    cout << "te=" << dtele_ << ", ts=" << dtsolve_ << "\n\n" ;

} // ArtNetExplicitTimeInt:Solve




//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 | call elements to calculate system matrix/rhs and assemble            |
 | this function will be kept empty untill further use     ismail 07/09 |
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
void ART::ArtNetExplicitTimeInt::AssembleMatAndRHS()
{

  dtele_    = 0.0;
  dtfilter_ = 0.0;
  // time measurement: element
  if(!coupledTo3D_)
  {
    TEUCHOS_FUNC_TIME_MONITOR("      + element calls");
  }

  // get cpu time
  //  const double tcpu=Teuchos::Time::wallTime();

} // ArtNetExplicitTimeInt::AssembleMatAndRHS




//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 | build system matrix and rhs                              ismail 06/09|
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
void ART::ArtNetExplicitTimeInt::Evaluate(Teuchos::RCP<const Epetra_Vector> qael)
{

}


//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 | current solution becomes most recent solution of next timestep       |
 |                                                                      |
 |  qnm_   =  qn_                                                       |
 |  arean_ = areap_                                                     |
 |                                                                      |
 |                                                          ismail 06/09|
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
void ART::ArtNetExplicitTimeInt::TimeUpdate()
{


  // Volumetric Flow rate/Cross-sectional area of this step become most recent
  qanm_->Update(1.0,*qan_ ,0.0);
  qan_ ->Update(1.0,*qanp_,0.0);

  return;
}// ArtNetExplicitTimeInt::TimeUpdate


//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 | output of solution vector to binio                       ismail 07/09|
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
void ART::ArtNetExplicitTimeInt::Output(bool               CoupledTo3D,
                                        RCP<ParameterList> CouplingParams)
{
  int step, upres, uprestart;
  double time_backup;
  // -------------------------------------------------------------------
  // if coupled to 3D problem, then get the export information from 
  // the 3D problem
  // -------------------------------------------------------------------
  
  if (CoupledTo3D)
  {
    step        = step_;
    upres       = upres_;
    uprestart   = uprestart_;
    time_backup = time_;
    step_      = CouplingParams->get<int>("step");
    upres_     = CouplingParams->get<int>("upres");
    uprestart_ = CouplingParams->get<int>("uprestart");
    time_      = CouplingParams->get<double>("time");
  }

  if (step_%upres_ == 0)
  {
    // step number and time
    output_.NewStep(step_,time_);

    // "volumetric flow rate/cross-sectional area" vector
    output_.WriteVector("qanp",qanp_);

    // write domain decomposition for visualization (only once!)
    if (step_==upres_) output_.WriteElementData();

    if (uprestart_ != 0 && step_%uprestart_ == 0)
    {
      // also write impedance bc information if required
      // Note: this method acts only if there is an impedance BC
      // impedancebc_->WriteRestart(output_);
    }
    //#endif
    // ------------------------------------------------------------------
    // Export gnuplot format arteries
    // ------------------------------------------------------------------

    ParameterList params;
    // other parameters that might be needed by the elements
    params.set("total time",time_);

    // set the dof vector values 
    discret_->ClearState();
    discret_->SetState("qanp",qanp_);

    // call the gnuplot writer
    artgnu_->Write(params);
    discret_->ClearState();

    // Export postpro results
    this->CalcPostprocessingValues();
    output_.WriteVector("one_d_artery_flow",qn_);
    output_.WriteVector("one_d_artery_pressure",pn_);
    
    if (CoupledTo3D)
    {
      output_.WriteDouble("Actual_RedD_step", step);
    }

  }
  // write restart also when uprestart_ is not a integer multiple of upres_
  else if (uprestart_ != 0 && step_%uprestart_ == 0)
  {
    // step number and time
    output_.NewStep(step_,time_);

    // "volumetric flow rate/cross-sectional area" vector
    output_.WriteVector("qanp",qanp_);

    // Export postpro results
    this->CalcPostprocessingValues();
    output_.WriteVector("one_d_artery_flow",qn_);
    output_.WriteVector("one_d_artery_pressure",pn_);
        
    // also write impedance bc information if required
    // Note: this method acts only if there is an impedance BC
    // impedancebc_->WriteRestart(output_);

    // ------------------------------------------------------------------
    // Export gnuplot format arteries
    // ------------------------------------------------------------------
    //#endif
    ParameterList params;
    // other parameters that might be needed by the elements
    params.set("total time",time_);

    // set the dof vector values 
    discret_->ClearState();
    discret_->SetState("qanp",qanp_);

    // call the gnuplot writer
    artgnu_->Write(params);
    discret_->ClearState();

    if (CoupledTo3D)
    {
      output_.WriteDouble("Actual_RedD_step", step);
    }

  }

  // -------------------------------------------------------------------
  // if coupled to 3D problem, then retrieve the old information of the
  // the reduced model problem
  // -------------------------------------------------------------------
  if (CoupledTo3D)
  {
    step_     = step;
    upres_    = upres;
    uprestart_= uprestart;
    time_     = time_backup;
  }

  return;
} // ArteryExplicitTimeInt::Output


//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 | ReadRestart (public)                                     ismail 07/09|
 -----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>// 
void ART::ArtNetExplicitTimeInt::ReadRestart(int step)
{

  IO::DiscretizationReader reader(discret_,step);
  
  time_ = reader.ReadDouble("time");

  if (coupledTo3D_)
  {
    step_ = reader.ReadInt("Actual_RedD_step");
  }
  else
  {
    step_ = reader.ReadInt("step");
  }

  reader.ReadVector(qanp_,"qanp");

  // also read impedance bc information if required
  // Note: this method acts only if there is an impedance BC
  // impedancebc_->ReadRestart(reader);

}


/*----------------------------------------------------------------------*
 | Destructor dtor (public)                                 ismail 01/09|
 *----------------------------------------------------------------------*/
ART::ArtNetExplicitTimeInt::~ArtNetExplicitTimeInt()
{
  return;
}

/*----------------------------------------------------------------------*
 | Calculate the post processing values (public)            ismail 04/10|
 *----------------------------------------------------------------------*/
void ART::ArtNetExplicitTimeInt::CalcPostprocessingValues()
{

  // create the parameters for the discretization
  ParameterList eleparams;
  
  // action for elements
  eleparams.set("action","calc_postprocessing_values");
  
  // set vecotr values needed by elements
  discret_->ClearState();
  discret_->SetState("qanp",qanp_);
  discret_->SetState("Wfnp",Wfnp_);
  discret_->SetState("Wbnp",Wbnp_);
  
  eleparams.set("time step size",dta_);
  eleparams.set("total time",time_);
  eleparams.set("pressure",pn_);
  eleparams.set("flow",qn_);
  
  // call standard loop over all elements
  discret_->Evaluate(eleparams,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null);
  
}//ART::ArtNetExplicitTimeInt::CalcPostprocessingValues

#endif


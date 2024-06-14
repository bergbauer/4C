
/*! \file
\brief Control routine for artery solvers,

     including solvers based on

     o two-step Taylor-Galerking scheme

\level 2



*----------------------------------------------------------------------*/


#include "4C_art_net_explicitintegration.hpp"

#include "4C_art_net_art_junction.hpp"
#include "4C_art_net_artery_ele_action.hpp"
#include "4C_art_net_artery_resulttest.hpp"
#include "4C_fem_condition_utils.hpp"
#include "4C_global_data.hpp"
#include "4C_linalg_utils_densematrix_communication.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_utils_exceptions.hpp"
#include "4C_utils_function.hpp"

#include <stdio.h>

FOUR_C_NAMESPACE_OPEN



//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
/*----------------------------------------------------------------------*
 |  Constructor (public)                                    ismail 01/09|
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//

Arteries::ArtNetExplicitTimeInt::ArtNetExplicitTimeInt(
    Teuchos::RCP<Core::FE::Discretization> actdis, const int linsolvernumber,
    const Teuchos::ParameterList& probparams, const Teuchos::ParameterList& artparams,
    Core::IO::DiscretizationWriter& output)
    : TimInt(actdis, linsolvernumber, probparams, artparams, output)
{
  //  exit(1);

}  // ArtNetExplicitTimeInt::ArtNetExplicitTimeInt



/*----------------------------------------------------------------------*
 | Initialize the time integration.                                     |
 |                                                          ismail 12/09|
 *----------------------------------------------------------------------*/
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
//<><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>//
void Arteries::ArtNetExplicitTimeInt::Init(const Teuchos::ParameterList& globaltimeparams,
    const Teuchos::ParameterList& arteryparams, const std::string& scatra_disname)
{
  // time measurement: initialization
  if (!coupledTo3D_)
  {
    TEUCHOS_FUNC_TIME_MONITOR(" + initialization");
  }

  // call base class
  TimInt::Init(globaltimeparams, arteryparams, scatra_disname);

  // ensure that degrees of freedom in the discretization have been set
  if (!discret_->Filled() || !discret_->HaveDofs()) discret_->fill_complete();

  // -------------------------------------------------------------------
  // Force the reducesd 1d arterial network discretization to run on
  // and only one cpu
  // -------------------------------------------------------------------
  // reduce the node row map into processor 0
  const Epetra_Map noderowmap_1_proc = *Core::LinAlg::AllreduceEMap(*discret_->NodeRowMap(), 0);
  // update the discetization by redistributing the new row map
  discret_->Redistribute(noderowmap_1_proc, noderowmap_1_proc);

  // -------------------------------------------------------------------
  // get a vector layout from the discretization to construct matching
  // vectors and matrices
  //                 local <-> global dof numbering
  // -------------------------------------------------------------------
  const Epetra_Map* dofrowmap = discret_->dof_row_map();

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
  sysmat_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*dofrowmap, 6, false, true));

  // Vectors passed to the element
  // -----------------------------
  // Volumetric flow rate at time n+1, n and n-1
  qanp_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  qan_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  qanm_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  qan_3D_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  // Vectors associated to boundary conditions
  // -----------------------------------------
  Wfo_ = Core::LinAlg::CreateVector(*noderowmap, true);
  Wbo_ = Core::LinAlg::CreateVector(*noderowmap, true);
  Wfnp_ = Core::LinAlg::CreateVector(*noderowmap, true);
  Wfn_ = Core::LinAlg::CreateVector(*noderowmap, true);
  Wfnm_ = Core::LinAlg::CreateVector(*noderowmap, true);
  Wbnp_ = Core::LinAlg::CreateVector(*noderowmap, true);
  Wbn_ = Core::LinAlg::CreateVector(*noderowmap, true);
  Wbnm_ = Core::LinAlg::CreateVector(*noderowmap, true);

  // a vector of zeros to be used to enforce zero dirichlet boundary conditions
  // This part might be optimized later
  bcval_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  dbctog_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  // Vectors used for postporcesing visualization
  // --------------------------------------------
  qn_ = Core::LinAlg::CreateVector(*noderowmap, true);
  pn_ = Core::LinAlg::CreateVector(*noderowmap, true);
  an_ = Core::LinAlg::CreateVector(*noderowmap, true);
  nodeIds_ = Core::LinAlg::CreateVector(*noderowmap, true);

  // right hand side vector and right hand side corrector
  rhs_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  // create the junction boundary conditions
  Teuchos::ParameterList junparams;

  junc_nodal_vals_ =
      Teuchos::rcp(new std::map<const int, Teuchos::RCP<Arteries::UTILS::JunctionNodeParams>>);

  junparams
      .set<Teuchos::RCP<std::map<const int, Teuchos::RCP<Arteries::UTILS::JunctionNodeParams>>>>(
          "Junctions Parameters", junc_nodal_vals_);

  artjun_ = Teuchos::rcp(new UTILS::ArtJunctionWrapper(discret_, output_, junparams, dta_));

  // create the gnuplot export conditions
  artgnu_ = Teuchos::rcp(new Arteries::UTILS::ArtWriteGnuplotWrapper(discret_, junparams));

  // ---------------------------------------------------------------------------------------
  // Initialize all the arteries' cross-sectional areas to the initial crossectional area Ao
  // and the volumetric flow rate to 0
  // ---------------------------------------------------------------------------------------
  Teuchos::ParameterList eleparams;
  discret_->ClearState();
  discret_->set_state("qanp", qanp_);

  // loop all elements on this proc (including ghosted ones)

  //  for (int nele=0;nele<discret_->NumMyColElements();++nele)
  {
    // get the element
    //    Core::Elements::Element* ele = discret_->lColElement(nele);

    // get element location vector, dirichlet flags and ownerships
    //    std::vector<int> lm;
    //    std::vector<int> lmstride;
    //    std::vector<int> lmowner;
    //        Teuchos::RCP<std::vector<int> > lmowner = Teuchos::rcp(new std::vector<int>);
    //    ele->LocationVector(*discret_,lm,*lmowner,lmstride);

    // loop all nodes of this element, add values to the global vectors
    eleparams.set("qa0", qanp_);
    eleparams.set("wfo", Wfo_);
    eleparams.set("wbo", Wbo_);
    Wfn_->Update(1.0, *Wfo_, 0.0);
    Wbn_->Update(1.0, *Wbo_, 0.0);
    // eleparams.set("lmowner",lmowner);
    eleparams.set<int>("action", Arteries::get_initial_artery_state);
    discret_->Evaluate(
        eleparams, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);
  }
  // Fill the NodeId vector
  for (int nele = 0; nele < discret_->NumMyColElements(); ++nele)
  {
    // get the element
    Core::Elements::Element* ele = discret_->lColElement(nele);

    // get element location vector, dirichlet flags and ownerships
    std::vector<int> lm;
    std::vector<int> lmstride;
    // vector<int> lmowner;
    Teuchos::RCP<std::vector<int>> lmowner = Teuchos::rcp(new std::vector<int>);
    ele->LocationVector(*discret_, lm, *lmowner, lmstride);

    // loop all nodes of this element, add values to the global vectors

    if (myrank_ == (*lmowner)[0])
    {
      int gid = lm[0];
      double val = gid;
      nodeIds_->ReplaceGlobalValues(1, &val, &gid);
    }
    if (myrank_ == (*lmowner)[1])
    {
      int gid = lm[1];
      double val = gid;
      nodeIds_->ReplaceGlobalValues(1, &val, &gid);
    }
  }

  // -----------------------------------------------------------------------
  // initialize all scatra related stuff
  // -----------------------------------------------------------------------

  if (solvescatra_)
  {
    // initialize scatra system matrix
    scatra_sysmat_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*dofrowmap, 6, false, true));
    // right hand side vector and right hand side corrector
    scatra_rhs_ = Core::LinAlg::CreateVector(*dofrowmap, true);

    // Scalar transport vector of O2 and CO2
    export_scatra_ = Core::LinAlg::CreateVector(*noderowmap, true);
    scatraO2nm_ = Core::LinAlg::CreateVector(*dofrowmap, true);
    scatraO2n_ = Core::LinAlg::CreateVector(*dofrowmap, true);
    scatraO2np_ = Core::LinAlg::CreateVector(*dofrowmap, true);
    scatraO2wfn_ = Core::LinAlg::CreateVector(*noderowmap, true);
    scatraO2wfnp_ = Core::LinAlg::CreateVector(*noderowmap, true);
    scatraO2wbn_ = Core::LinAlg::CreateVector(*noderowmap, true);
    scatraO2wbnp_ = Core::LinAlg::CreateVector(*noderowmap, true);

    scatraCO2n_ = Core::LinAlg::CreateVector(*dofrowmap, true);
    scatraCO2np_ = Core::LinAlg::CreateVector(*dofrowmap, true);
    scatraCO2wfn_ = Core::LinAlg::CreateVector(*noderowmap, true);
    scatraCO2wfnp_ = Core::LinAlg::CreateVector(*noderowmap, true);
    scatraCO2wbn_ = Core::LinAlg::CreateVector(*noderowmap, true);
    scatraCO2wbnp_ = Core::LinAlg::CreateVector(*noderowmap, true);

    // a vector of zeros to be used to enforce zero dirichlet boundary conditions
    // This part might be optimized later
    scatra_bcval_ = Core::LinAlg::CreateVector(*dofrowmap, true);
    scatra_dbctog_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  }

}  // ArtNetExplicitTimeInt::Init

/*----------------------------------------------------------------------*
 | the solver for artery                                   ismail 06/09 |
 *----------------------------------------------------------------------*/
void Arteries::ArtNetExplicitTimeInt::Solve(Teuchos::RCP<Teuchos::ParameterList> CouplingTo3DParams)
{
  // time measurement: Artery
  if (!coupledTo3D_)
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
    if (!coupledTo3D_)
    {
      TEUCHOS_FUNC_TIME_MONITOR("      + element calls");
    }

    // set both system matrix and rhs vector to zero
    sysmat_->Zero();
    rhs_->PutScalar(0.0);


    // create the parameters for the discretization
    Teuchos::ParameterList eleparams;

    // action for elements
    eleparams.set<int>("action", Arteries::calc_sys_matrix_rhs);
    eleparams.set("time step size", dta_);

    // other parameters that might be needed by the elements
    eleparams.set("total time", time_);

    // set vector values needed by elements
    discret_->ClearState();
    discret_->set_state("qanp", qanp_);


    // call standard loop over all elements
    discret_->Evaluate(eleparams, sysmat_, rhs_);
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
    Teuchos::ParameterList eleparams;

    // action for elements
    eleparams.set<int>("action", Arteries::solve_riemann_problem);

    // set vecotr values needed by elements
    discret_->ClearState();
    discret_->set_state("qanp", qanp_);

    eleparams.set("time step size", dta_);
    eleparams.set("Wfnp", Wfnp_);
    eleparams.set("Wbnp", Wbnp_);

    eleparams.set("total time", time_);
    eleparams
        .set<Teuchos::RCP<std::map<const int, Teuchos::RCP<Arteries::UTILS::JunctionNodeParams>>>>(
            "Junctions Parameters", junc_nodal_vals_);

    // call standard loop over all elements
    discret_->Evaluate(eleparams, sysmat_, rhs_);
  }

  // Solve the boundary conditions
  bcval_->PutScalar(0.0);
  dbctog_->PutScalar(0.0);
  // Solve terminal BCs

  {
    // create the parameters for the discretization
    Teuchos::ParameterList eleparams;

    // action for elements
    eleparams.set<int>("action", Arteries::set_term_bc);

    // set vecotr values needed by elements
    discret_->ClearState();
    discret_->set_state("qanp", qanp_);

    eleparams.set("time step size", dta_);
    eleparams.set("total time", time_);
    eleparams.set("bcval", bcval_);
    eleparams.set("dbctog", dbctog_);
    eleparams.set("Wfnp", Wfnp_);
    eleparams.set("Wbnp", Wbnp_);
    eleparams
        .set<Teuchos::RCP<std::map<const int, Teuchos::RCP<Arteries::UTILS::JunctionNodeParams>>>>(
            "Junctions Parameters", junc_nodal_vals_);

    // Add the parameters to solve terminal BCs coupled to 3D fluid boundary
    eleparams.set("coupling with 3D fluid params", CouplingTo3DParams);

    // solve junction boundary conditions
    artjun_->Solve(eleparams);

    // call standard loop over all elements
    discret_->Evaluate(eleparams, sysmat_, rhs_);
  }


  // -------------------------------------------------------------------
  // Apply the BCs to the system matrix and rhs
  // -------------------------------------------------------------------
  {
    // time measurement: application of dbc
    if (!coupledTo3D_)
    {
      TEUCHOS_FUNC_TIME_MONITOR("      + apply DBC");
    }
    Core::LinAlg::apply_dirichlet_to_system(*sysmat_, *qanp_, *rhs_, *bcval_, *dbctog_);
  }

  //-------solve for total new velocities and pressures
  // get cpu time
  const double tcpusolve = Teuchos::Time::wallTime();
  {
    // time measurement: solver
    if (!coupledTo3D_)
    {
      TEUCHOS_FUNC_TIME_MONITOR("      + solver calls");
    }

    // call solver
    Core::LinAlg::SolverParams solver_params;
    solver_params.refactor = true;
    solver_params.reset = true;
    solver_->Solve(sysmat_->EpetraOperator(), qanp_, rhs_, solver_params);
  }
  // end time measurement for solver
  dtsolve_ = Teuchos::Time::wallTime() - tcpusolve;

  if (myrank_ == 0) std::cout << "te=" << dtele_ << ", ts=" << dtsolve_ << "\n\n";

  // Update Wf and Wb
  {
    // create the parameters for the discretization
    Teuchos::ParameterList eleparams;

    // action for elements
    eleparams.set<int>("action", Arteries::evaluate_wf_wb);

    // set vecotr values needed by elements
    discret_->ClearState();
    discret_->set_state("qanp", qanp_);

    eleparams.set("time step size", dta_);
    eleparams.set("total time", time_);
    eleparams.set("Wfnp", Wfnp_);
    eleparams.set("Wbnp", Wbnp_);

    discret_->Evaluate(
        eleparams, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);
  }
}  // ArtNetExplicitTimeInt:Solve


void Arteries::ArtNetExplicitTimeInt::SolveScatra()
{
  {
    scatraO2np_->PutScalar(0.0);
    // create the parameters for the discretization
    Teuchos::ParameterList eleparams;

    // action for elements
    eleparams.set<int>("action", Arteries::evaluate_scatra_analytically);

    // set vecotr values needed by elements
    discret_->ClearState();

    eleparams.set<Teuchos::RCP<Epetra_Vector>>("Wfn", Wfn_);
    eleparams.set<Teuchos::RCP<Epetra_Vector>>("Wbn", Wbn_);
    eleparams.set<Teuchos::RCP<Epetra_Vector>>("Wfo", Wfo_);
    eleparams.set<Teuchos::RCP<Epetra_Vector>>("Wbo", Wbo_);
    eleparams.set<Teuchos::RCP<Epetra_Vector>>("scatran", scatraO2n_);
    eleparams.set<Teuchos::RCP<Epetra_Vector>>("scatranp", scatraO2np_);

    eleparams.set("time step size", dta_);

    // call standard loop over all elements
    discret_->Evaluate(eleparams, scatra_sysmat_, scatra_rhs_);
  }
  {
    scatra_bcval_->PutScalar(0.0);
    scatra_dbctog_->PutScalar(0.0);
    // create the parameters for the discretization
    Teuchos::ParameterList eleparams;

    // action for elements
    eleparams.set<int>("action", Arteries::set_scatra_term_bc);

    // set vecotr values needed by elements
    discret_->ClearState();
    discret_->set_state("qanp", qanp_);

    eleparams.set("time step size", dta_);
    eleparams.set("time", time_);
    eleparams.set("bcval", scatra_bcval_);
    eleparams.set("dbctog", scatra_dbctog_);

    // call standard loop over all elements
    discret_->Evaluate(eleparams, scatra_sysmat_, scatra_rhs_);
  }
  scatraO2np_->Update(1.0, *scatra_bcval_, 1.0);
}

/*----------------------------------------------------------------------*
 | current solution becomes most recent solution of next timestep       |
 |                                                                      |
 |  qnm_   =  qn_                                                       |
 |  arean_ = areap_                                                     |
 |                                                                      |
 |                                                          ismail 06/09|
 *----------------------------------------------------------------------*/
void Arteries::ArtNetExplicitTimeInt::TimeUpdate()
{
  // Volumetric Flow rate/Cross-sectional area of this step become most recent
  qanm_->Update(1.0, *qan_, 0.0);
  qan_->Update(1.0, *qanp_, 0.0);
  Wfn_->Update(1.0, *Wfnp_, 0.0);
  Wbn_->Update(1.0, *Wbnp_, 0.0);

  if (solvescatra_)
  {
    //    scatraO2wfn_->Update(1.0,*scatraO2wfnp_ ,0.0);
    //    scatraO2wbn_->Update(1.0,*scatraO2wbnp_ ,0.0);
    scatraO2nm_->Update(1.0, *scatraO2n_, 0.0);
    scatraO2n_->Update(1.0, *scatraO2np_, 0.0);
  }

  return;
}  // ArtNetExplicitTimeInt::TimeUpdate


/*----------------------------------------------------------------------*
 | Initializes state saving vectors                                     |
 |                                                                      |
 |  This is currently needed for strongly coupling 3D-1D fields         |
 |                                                                      |
 |                                                          ismail 04/14|
 *----------------------------------------------------------------------*/
void Arteries::ArtNetExplicitTimeInt::InitSaveState()
{
  // get the discretizations DOF row map
  const Epetra_Map* dofrowmap = discret_->dof_row_map();

  // Volumetric Flow rate/Cross-sectional area of this step become most recent
  saved_qanp_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  saved_qan_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  saved_qanm_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  saved_Wfnp_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  saved_Wfn_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  saved_Wfnm_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  saved_Wbnp_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  saved_Wbn_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  saved_Wbnm_ = Core::LinAlg::CreateVector(*dofrowmap, true);

  if (solvescatra_)
  {
    saved_scatraO2np_ = Core::LinAlg::CreateVector(*dofrowmap, true);
    saved_scatraO2n_ = Core::LinAlg::CreateVector(*dofrowmap, true);
    saved_scatraO2nm_ = Core::LinAlg::CreateVector(*dofrowmap, true);
  }

  return;
}  // ArtNetExplicitTimeInt::InitSaveState


/*----------------------------------------------------------------------*
 | Saves and backs up the current state.                                |
 |                                                                      |
 |  This is currently needed for stronly coupling 3D-0D fields          |
 |  example:                                                            |
 |  saved_qanp_ = qanp_                                                 |
 |  saved_Wfnp_ = Wfnp_                                                 |
 |                                                                      |
 |                                                          ismail 04/14|
 *----------------------------------------------------------------------*/
void Arteries::ArtNetExplicitTimeInt::SaveState()
{
  // Volumetric Flow rate/Cross-sectional area of this step become most recent
  saved_qanp_->Update(1.0, *qanp_, 0.0);
  saved_qan_->Update(1.0, *qan_, 0.0);
  saved_qanm_->Update(1.0, *qanm_, 0.0);

  saved_Wfnp_->Update(1.0, *Wfnp_, 0.0);
  saved_Wfn_->Update(1.0, *Wfn_, 0.0);
  saved_Wfnm_->Update(1.0, *Wfnm_, 0.0);

  saved_Wbnp_->Update(1.0, *Wbnp_, 0.0);
  saved_Wbn_->Update(1.0, *Wbn_, 0.0);
  saved_Wbnm_->Update(1.0, *Wbnm_, 0.0);

  if (solvescatra_)
  {
    saved_scatraO2np_->Update(1.0, *scatraO2np_, 0.0);
    saved_scatraO2n_->Update(1.0, *scatraO2n_, 0.0);
    saved_scatraO2nm_->Update(1.0, *scatraO2nm_, 0.0);
  }

  return;
}  // ArtNetExplicitTimeInt::SaveState


/*----------------------------------------------------------------------*
 | Loads backed up states.                                              |
 |                                                                      |
 |  This is currently needed for stronly coupling 3D-0D fields          |
 |  example:                                                            |
 |  qanp_   =  saved_qanp_                                              |
 |  Wfnp_   =  saved_Wfnp_                                              |
 |                                                                      |
 |                                                          ismail 04/14|
 *----------------------------------------------------------------------*/
void Arteries::ArtNetExplicitTimeInt::LoadState()
{
  // Volumetric Flow rate/Cross-sectional area of this step become most recent
  qanp_->Update(1.0, *saved_qanp_, 0.0);
  qan_->Update(1.0, *saved_qan_, 0.0);
  qanm_->Update(1.0, *saved_qanm_, 0.0);

  Wfnp_->Update(1.0, *saved_Wfnp_, 0.0);
  Wfn_->Update(1.0, *saved_Wfn_, 0.0);
  Wfnm_->Update(1.0, *saved_Wfnm_, 0.0);

  Wbnp_->Update(1.0, *saved_Wbnp_, 0.0);
  Wbn_->Update(1.0, *saved_Wbn_, 0.0);
  Wbnm_->Update(1.0, *saved_Wbnm_, 0.0);

  if (solvescatra_)
  {
    scatraO2np_->Update(1.0, *saved_scatraO2np_, 0.0);
    scatraO2n_->Update(1.0, *saved_scatraO2n_, 0.0);
    scatraO2nm_->Update(1.0, *saved_scatraO2nm_, 0.0);
  }

  return;
}  // ArtNetExplicitTimeInt::LoadState


/*----------------------------------------------------------------------*
 | output of solution vector to binio                       ismail 07/09|
 *----------------------------------------------------------------------*/
void Arteries::ArtNetExplicitTimeInt::Output(
    bool CoupledTo3D, Teuchos::RCP<Teuchos::ParameterList> CouplingParams)
{
  int step = 0;
  int upres = 0;
  int uprestart = 0;
  double time_backup = 0.0;

  // -------------------------------------------------------------------
  // if coupled to 3D problem, then get the export information from
  // the 3D problem
  // -------------------------------------------------------------------

  if (CoupledTo3D)
  {
    step = step_;
    upres = upres_;
    uprestart = uprestart_;
    time_backup = time_;
    step_ = CouplingParams->get<int>("step");
    upres_ = CouplingParams->get<int>("upres");
    uprestart_ = CouplingParams->get<int>("uprestart");
    time_ = CouplingParams->get<double>("time");
  }

  if (step_ % upres_ == 0)
  {
    // step number and time
    output_.new_step(step_, time_);
    //    output_.write_vector("NodeIDs",nodeIds_);

    // "volumetric flow rate/cross-sectional area" vector
    output_.write_vector("qanp", qanp_);


    // write domain decomposition for visualization (only once!)
    if (step_ == upres_) output_.write_element_data(true);

    // #endif
    //  ------------------------------------------------------------------
    //  Export gnuplot format arteries
    //  ------------------------------------------------------------------

    Teuchos::ParameterList params;
    // other parameters that might be needed by the elements
    params.set("total time", time_);

    // set the dof vector values
    //    discret_->ClearState();
    //    discret_->set_state("qanp",qanp_);

    // call the gnuplot writer
    //    artgnu_->Write(params);
    //    discret_->ClearState();

    // Export postpro results
    this->calc_postprocessing_values();
    output_.write_vector("one_d_artery_flow", qn_);
    output_.write_vector("one_d_artery_pressure", pn_);
    output_.write_vector("one_d_artery_area", an_);

    if (solvescatra_)
    {
      this->calc_scatra_from_scatra_fw(export_scatra_, scatraO2np_);
      output_.write_vector("one_d_o2_scatra", export_scatra_);
    }

    output_.write_vector("forward_speed", Wfnp_);
    output_.write_vector("forward_speed0", Wfo_);
    output_.write_vector("backward_speed", Wbnp_);
    output_.write_vector("backward_speed0", Wbo_);

    if (CoupledTo3D)
    {
      output_.write_int("Actual_RedD_step", step);
    }
  }
  // write restart also when uprestart_ is not a integer multiple of upres_
  else if (uprestart_ != 0 && step_ % uprestart_ == 0)
  {
    // step number and time
    output_.new_step(step_, time_);

    // "volumetric flow rate/cross-sectional area" vector
    output_.write_vector("qanp", qanp_);

    // Export postpro results
    this->calc_postprocessing_values();
    output_.write_vector("one_d_artery_flow", qn_);
    output_.write_vector("one_d_artery_pressure", pn_);
    output_.write_vector("one_d_artery_area", an_);

    if (solvescatra_)
    {
      this->calc_scatra_from_scatra_fw(export_scatra_, scatraO2np_);
      output_.write_vector("one_d_o2_scatra", export_scatra_);
    }


    output_.write_vector("forward_speed", Wfnp_);
    output_.write_vector("forward_speed0", Wfo_);
    output_.write_vector("backward_speed", Wbnp_);
    output_.write_vector("backward_speed0", Wbo_);

    // ------------------------------------------------------------------
    // Export gnuplot format arteries
    // ------------------------------------------------------------------
    // #endif
    Teuchos::ParameterList params;
    // other parameters that might be needed by the elements
    params.set("total time", time_);

    // set the dof vector values
    //    discret_->ClearState();
    //    discret_->set_state("qanp",qanp_);

    // call the gnuplot writer
    //    artgnu_->Write(params);
    //    discret_->ClearState();

    if (CoupledTo3D)
    {
      output_.write_int("Actual_RedD_step", step);
    }
  }

  // -------------------------------------------------------------------
  // if coupled to 3D problem, then retrieve the old information of the
  // the reduced model problem
  // -------------------------------------------------------------------
  if (CoupledTo3D)
  {
    step_ = step;
    upres_ = upres;
    uprestart_ = uprestart;
    time_ = time_backup;
  }

  return;
}  // ArteryExplicitTimeInt::Output


/*----------------------------------------------------------------------*
 | read_restart (public)                                     ismail 07/09|
 -----------------------------------------------------------------------*/
void Arteries::ArtNetExplicitTimeInt::read_restart(int step, bool coupledTo3D)
{
  coupledTo3D_ = coupledTo3D;
  Core::IO::DiscretizationReader reader(
      discret_, Global::Problem::Instance()->InputControlFile(), step);

  time_ = reader.read_double("time");

  if (coupledTo3D_)
  {
    step_ = reader.read_int("Actual_RedD_step");
  }
  else
  {
    step_ = reader.read_int("step");
  }

  reader.read_vector(qanp_, "qanp");
}



/*----------------------------------------------------------------------*
 | Calculate the post processing values (public)            ismail 04/10|
 *----------------------------------------------------------------------*/
void Arteries::ArtNetExplicitTimeInt::calc_postprocessing_values()
{
  //  std::cout<<"On proc("<<myrank_<<"): "<<"postpro values being calculated"<<std::endl;

  // create the parameters for the discretization
  Teuchos::ParameterList eleparams;

  // action for elements
  eleparams.set<int>("action", Arteries::calc_postpro_vals);

  // set vecotr values needed by elements
  discret_->ClearState();
  //  std::cout<<"On proc("<<myrank_<<"): "<<"postpro setting qanp"<<std::endl;
  discret_->set_state("qanp", qanp_);
  //  std::cout<<"On proc("<<myrank_<<"): "<<"postpro setting wfnp"<<std::endl;
  //  discret_->set_state("Wfnp",Wfnp_);
  //  std::cout<<"On proc("<<myrank_<<"): "<<"postpro setting wbnp"<<std::endl;
  //  discret_->set_state("Wbnp",Wbnp_);

  eleparams.set("time step size", dta_);
  eleparams.set("total time", time_);
  eleparams.set("pressure", pn_);
  eleparams.set("art_area", an_);
  eleparams.set("flow", qn_);
  //  std::cout<<"On proc("<<myrank_<<"): "<<"postpro evaluat disc"<<std::endl;
  // call standard loop over all elements
  discret_->Evaluate(
      eleparams, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);
  //  std::cout<<"On proc("<<myrank_<<"): "<<"postpro done "<<std::endl;
}  // Arteries::ArtNetExplicitTimeInt::calc_postprocessing_values


void Arteries::ArtNetExplicitTimeInt::calc_scatra_from_scatra_fw(
    Teuchos::RCP<Epetra_Vector> scatra, Teuchos::RCP<Epetra_Vector> scatra_fb)
{
  scatra->PutScalar(0.0);

  // create the parameters for the discretization
  Teuchos::ParameterList eleparams;

  // action for elements
  eleparams.set<int>("action", Arteries::calc_scatra_from_scatra_fb);

  // set vecotr values needed by elements
  discret_->ClearState();
  eleparams.set("scatra", scatra);
  eleparams.set("scatra_fb", scatra_fb);

  // call standard loop over all elements
  discret_->Evaluate(
      eleparams, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null, Teuchos::null);
}

void Arteries::ArtNetExplicitTimeInt::TestResults()
{
  Teuchos::RCP<Core::UTILS::ResultTest> resulttest = CreateFieldTest();
  Global::Problem::Instance()->AddFieldTest(resulttest);
  Global::Problem::Instance()->TestAll(discret_->Comm());
}

/*----------------------------------------------------------------------*
 | create result test for this field                   kremheller 03/18 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<Core::UTILS::ResultTest> Arteries::ArtNetExplicitTimeInt::CreateFieldTest()
{
  return Teuchos::rcp(new Arteries::ArteryResultTest(*(this)));
}

FOUR_C_NAMESPACE_CLOSE
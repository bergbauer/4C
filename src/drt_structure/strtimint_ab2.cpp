/*----------------------------------------------------------------------*/
/*!
\file strtimint_ab2.cpp
\brief Structural time integration with Adams-Bashforth 2nd order (explicit)

<pre>
Maintainer: Alexander Popp
            popp@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15238
</pre>
*/

/*----------------------------------------------------------------------*/
/* macros */

/*----------------------------------------------------------------------*/
/* headers */
#include "strtimint_ab2.H"
#include "../drt_mortar/mortar_manager_base.H"
#include "../drt_mortar/mortar_strategy_base.H"
#include "../linalg/linalg_solver.H"
#include "../linalg/linalg_utils.H"
#include "../drt_io/io.H"

/*----------------------------------------------------------------------*/
/* Constructor */
STR::TimIntAB2::TimIntAB2
(
  const Teuchos::ParameterList& ioparams,
  const Teuchos::ParameterList& sdynparams,
  const Teuchos::ParameterList& xparams,
  //const Teuchos::ParameterList& ab2params,
  Teuchos::RCP<DRT::Discretization> actdis,
  Teuchos::RCP<LINALG::Solver> solver,
  Teuchos::RCP<LINALG::Solver> contactsolver,
  Teuchos::RCP<IO::DiscretizationWriter> output
)
: TimIntExpl
  (
    ioparams,
    sdynparams,
    xparams,
    actdis,
    solver,
    contactsolver,
    output
  ),
  fextn_(Teuchos::null),
  fintn_(Teuchos::null),
  fviscn_(Teuchos::null),
  fcmtn_(Teuchos::null),
  frimpn_(Teuchos::null)
{
  // info to user
  if (myrank_ == 0)
  {
    std::cout << "with Adams-Bashforth 2nd order"
              << std::endl;
  }

  // determine mass, damping and initial accelerations
  DetermineMassDampConsistAccel();

  // resize of multi-step quantities
  ResizeMStep();

  // allocate force vectors
  fextn_  = LINALG::CreateVector(*dofrowmap_, true);
  fintn_  = LINALG::CreateVector(*dofrowmap_, true);
  fviscn_ = LINALG::CreateVector(*dofrowmap_, true);
  fcmtn_  = LINALG::CreateVector(*dofrowmap_, true);
  frimpn_ = LINALG::CreateVector(*dofrowmap_, true);

  // let it rain
  return;
}

/*----------------------------------------------------------------------*/
/* Resizing of multi-step quantities */
void STR::TimIntAB2::ResizeMStep()
{
  // resize time and stepsize fields
  time_->Resize(-1, 0, (*time_)[0]);
  dt_->Resize(-1, 0, (*dt_)[0]);

  // resize state vectors, AB2 is a 2-step method, thus we need two
  // past steps at t_{n} and t_{n-1}
  dis_->Resize(-1, 0, dofrowmap_, true);
  vel_->Resize(-1, 0, dofrowmap_, true);
  acc_->Resize(-1, 0, dofrowmap_, true);
}

/*----------------------------------------------------------------------*/
/* Integrate step */
void STR::TimIntAB2::IntegrateStep()
{
  // time this step
  timer_->ResetStartTime();

  const double dt = (*dt_)[0];  // \f$\Delta t_{n}\f$
  const double dto = (*dt_)[-1];  // \f$\Delta t_{n-1}\f$

  // new displacements \f$D_{n+}\f$
  disn_->Update(1.0, *(*dis_)(0), 0.0);
  disn_->Update((2.0*dt*dto+dt*dt)/(2.0*dto), *(*vel_)(0),
                -(dt*dt)/(2.0*dto), *(*vel_)(-1),
                1.0);

  // new velocities \f$V_{n+1}\f$
  veln_->Update(1.0, *(*vel_)(0), 0.0);
  veln_->Update((2.0*dt*dto+dt*dt)/(2.0*dto), *(*acc_)(0),
                -(dt*dt)/(2.0*dto), *(*acc_)(-1),
                1.0);

  // apply Dirichlet BCs
  ApplyDirichletBC(timen_, disn_, veln_, Teuchos::null, false);

  // initialise stiffness matrix to zero
  stiff_->Zero();

  // build new external forces
  fextn_->PutScalar(0.0);
  ApplyForceExternal(timen_, disn_, veln_, fextn_);

  // TIMING
  //double dtcpu = timer_->WallTime();

  // initialise internal forces
  fintn_->PutScalar(0.0);

  // ordinary internal force and stiffness
  {
    // displacement increment in step
    Epetra_Vector disinc = Epetra_Vector(*disn_);
    disinc.Update(-1.0, *(*dis_)(0), 1.0);
    // internal force
    ApplyForceInternal(timen_, dt,
                       disn_, Teuchos::rcp(&disinc,false), veln_,
                       fintn_);
  }

  // TIMING
  //if (!myrank_) cout << "\nT_internal: " << timer_->WallTime() -dtcpu << endl;

  // viscous forces due Rayleigh damping
  if (damping_ == INPAR::STR::damp_rayleigh)
  {
    damp_->Multiply(false, *veln_, *fviscn_);
  }

  // TIMING
  //dtcpu = timer_->WallTime();

  // contact or meshtying forces
  if (HaveContactMeshtying())
  {
    fcmtn_->PutScalar(0.0);
    cmtman_->GetStrategy().ApplyForceStiffCmt(disn_,stiff_,fcmtn_,false);
  }

  // TIMING
  //if (!myrank_) cout << "T_contact:  " << timer_->WallTime() - dtcpu  << endl;
  
  // determine time derivative of linear momentum vector,
  // ie \f$\dot{P} = M \dot{V}_{n=1}\f$
  frimpn_->Update(1.0, *fextn_, -1.0, *fintn_, 0.0);

  if (damping_ == INPAR::STR::damp_rayleigh)
  {
    frimpn_->Update(-1.0, *fviscn_, 1.0);
  }
  
  if (HaveContactMeshtying())
  {
    frimpn_->Update(1.0, *fcmtn_, 1.0);
  }

  // TIMING
  //dtcpu = timer_->WallTime();

  // obtain new accelerations \f$A_{n+1}\f$
  {
    dsassert(mass_->Filled(), "Mass matrix has to be completed");
    // blank linear momentum zero on DOFs subjected to DBCs
    dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), frimpn_);
    // get accelerations
    accn_->PutScalar(0.0);

    // in case of no lumping or if mass matrix is a BlockSparseMatrix, use solver
    if (lumpmass_==false || Teuchos::rcp_dynamic_cast<LINALG::SparseMatrix>(mass_)==Teuchos::null)
    {
      // linear solver call
      // refactor==false: This is not necessary, because we always
      // use the same constant mass matrix, which was firstly factorised
      // in TimInt::DetermineMassDampConsistAccel
      solver_->Solve(mass_->EpetraOperator(), accn_, frimpn_, false, true);
    }

    // direct inversion based on lumped mass matrix
    else
    {
      RCP<LINALG::SparseMatrix> massmatrix = Teuchos::rcp_dynamic_cast<LINALG::SparseMatrix>(mass_);
      RCP<Epetra_Vector> diagonal = LINALG::CreateVector(*dofrowmap_, true);
      int error = massmatrix->ExtractDiagonalCopy(*diagonal);
      if (error!=0) dserror("ERROR: ExtractDiagonalCopy went wrong");
      accn_->ReciprocalMultiply(1.0,*diagonal,*frimpn_,0.0);
    }
  }

  // TIMING
  //if (!myrank_) cout << "T_linsolve: " << timer_->WallTime() - dtcpu << endl;

  // apply Dirichlet BCs on accelerations
  ApplyDirichletBC(timen_, Teuchos::null, Teuchos::null, accn_, false);

  // wassup?
  return;
}

/*----------------------------------------------------------------------*/
/* Update step */
void STR::TimIntAB2::UpdateStepState()
{
  // new displacements at t_{n+1} -> t_n
  //    D_{n} := D_{n+1}, D_{n-1} := D_{n}
  dis_->UpdateSteps(*disn_);
  // new velocities at t_{n+1} -> t_n
  //    V_{n} := V_{n+1}, V_{n-1} := V_{n}
  vel_->UpdateSteps(*veln_);
  // new accelerations at t_{n+1} -> t_n
  //    A_{n} := A_{n+1}, A_{n-1} := A_{n}
  acc_->UpdateSteps(*accn_);

  // update contact and meshtying
  UpdateStepContactMeshtying();

  // bye
  return;
}

/*----------------------------------------------------------------------*/
/* update after time step after output on element level*/
// update anything that needs to be updated at the element level
void STR::TimIntAB2::UpdateStepElement()
{
  // create the parameters for the discretization
  ParameterList p;
  // other parameters that might be needed by the elements
  p.set("total time", timen_);
  p.set("delta time", (*dt_)[0]);
  // action for elements
  p.set("action", "calc_struct_update_istep");
  // go to elements
  discret_->Evaluate(p, Teuchos::null, Teuchos::null,
                     Teuchos::null, Teuchos::null, Teuchos::null);
}

/*----------------------------------------------------------------------*/
/* read restart forces */
void STR::TimIntAB2::ReadRestartForce()
{
  dserror("No restart ability Adams-Bashforth 2nd order time integrator!");
  return;
}

/*----------------------------------------------------------------------*/

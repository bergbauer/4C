/*----------------------------------------------------------------------*/
/*!
\file scatra_timint_implicit_service.cpp
\brief Service routines of the scalar transport time integration class

<pre>
Maintainer: Georg Bauer
            bauer@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15252
</pre>
*/
/*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include "scatra_timint_implicit.H"
#include "../drt_lib/drt_timecurve.H"
#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/linalg_utils.H"
#include "../drt_lib/linalg_solver.H"


/*----------------------------------------------------------------------*
 | calculate initial time derivative of phi at t=t_0           gjb 08/08|
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::CalcInitialPhidt()
{
  // time measurement:
  TEUCHOS_FUNC_TIME_MONITOR("SCATRA:       + calc inital phidt");
  if (myrank_ == 0)
  cout<<"SCATRA: calculating initial time derivative of phi\n"<<endl;

  // are we really at step 0?
  dsassert(step_==0,"Step counter is not 0");

  // call elements to calculate matrix and right-hand-side
  {
    // zero out matrix entries
    sysmat_->Zero();

    // create the parameters for the discretization
    ParameterList eleparams;

    // action for elements
    eleparams.set("action","calc_initial_time_deriv");
    // other parameters that are needed by the elements
    eleparams.set("problem type",prbtype_);
    eleparams.set("incremental solver",incremental_);
    eleparams.set("form of convective term",convform_);
    if (prbtype_=="elch")
      eleparams.set("frt",frt_); // factor F/RT
    else if (prbtype_ == "loma")
      eleparams.set("time derivative of thermodynamic pressure",thermpressdtn_);

    //provide velocity field (export to column map necessary for parallel evaluation)
    //SetState cannot be used since this Multivector is nodebased and not dofbased
    const Epetra_Map* nodecolmap = discret_->NodeColMap();
    RefCountPtr<Epetra_MultiVector> tmp = rcp(new Epetra_MultiVector(*nodecolmap,3));
    LINALG::Export(*convel_,*tmp);
    eleparams.set("velocity field",tmp);

    // set vector values needed by elements
    discret_->ClearState();
    discret_->SetState("phi0",phin_);
    discret_->SetState("dens0",densnp_);
    // call loop over elements
    discret_->Evaluate(eleparams,sysmat_,residual_);
    discret_->ClearState();

    // finalize the complete matrix
    sysmat_->Complete();
  }

  // apply Dirichlet boundary conditions to system matrix
  LINALG::ApplyDirichlettoSystem(sysmat_,phidtn_,residual_,phidtn_,*(dbcmaps_->CondMap()));

  // solve for phidtn
  solver_->Solve(sysmat_->EpetraOperator(),phidtn_,residual_,true,true);

  // reset the matrix (and its graph!) since we solved
  // a very special problem here that has a different sparsity pattern_
  if (params_->get<int>("BLOCKPRECOND") )
    ; //how to reset a block matrix ??
  else
    SystemMatrix()->Reset();

  return;
}

/*----------------------------------------------------------------------*
 | evaluate contribution of electrode kinetics to eq. system  gjb 02/09 |
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::EvaluateElectrodeKinetics(
    RCP<LINALG::SparseOperator>& matrix,
    RCP<Epetra_Vector>&          rhs
)
{
  // time measurement: evaluate condition 'ElectrodeKinetics'
  TEUCHOS_FUNC_TIME_MONITOR("SCATRA:       + evaluate condition 'ElectrodeKinetics'");

  // create an parameter list
  ParameterList condparams;

  // action for elements
  condparams.set("action","calc_elch_electrode_kinetics");
  condparams.set("frt",frt_); // factor F/RT
  condparams.set("total time",time_);
  condparams.set("iselch",(prbtype_=="elch")); // a boolean

  // set vector values needed by elements
  discret_->ClearState();
  discret_->SetState("phinp",phinp_);

  // add element parameters and density state according to time-int. scheme
  AddSpecificTimeIntegrationParameters(condparams);

  std::string condstring("ElectrodeKinetics");
  discret_->EvaluateCondition(condparams,matrix,Teuchos::null,rhs,Teuchos::null,Teuchos::null,condstring);
  discret_->ClearState();

  return;
}


/*----------------------------------------------------------------------*
 | construct toggle vector for Dirichlet dofs                  gjb 11/08|
 | assures backward compatibility for avm3 solver; should go away once  |
 *----------------------------------------------------------------------*/
const Teuchos::RCP<const Epetra_Vector> SCATRA::ScaTraTimIntImpl::DirichletToggle()
{
  if (dbcmaps_ == Teuchos::null)
    dserror("Dirichlet map has not been allocated");
  Teuchos::RCP<Epetra_Vector> dirichones = LINALG::CreateVector(*(dbcmaps_->CondMap()),false);
  dirichones->PutScalar(1.0);
  Teuchos::RCP<Epetra_Vector> dirichtoggle = LINALG::CreateVector(*(discret_->DofRowMap()),true);
  dbcmaps_->InsertCondVector(dirichones, dirichtoggle);
  return dirichtoggle;
}


/*----------------------------------------------------------------------*
 |  prepare AVM3-based scale separation                        vg 10/08 |
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::AVM3Preparation()
{
  // time measurement: avm3
  TEUCHOS_FUNC_TIME_MONITOR("SCATRA:            + avm3");

  // create normalized all-scale subgrid-diffusivity matrix
  sysmat_sd_->Zero();

  // create the parameters for the discretization
  ParameterList eleparams;

  // action for elements, time factor and stationary flag
  eleparams.set("action","calc_subgrid_diffusivity_matrix");

  // add element parameters and density state according to time-int. scheme
  AddSpecificTimeIntegrationParameters(eleparams);

  // call loop over elements
  discret_->Evaluate(eleparams,sysmat_sd_,residual_);
  discret_->ClearState();

  // finalize the normalized all-scale subgrid-diffusivity matrix
  sysmat_sd_->Complete();

  // apply DBC to normalized all-scale subgrid-diffusivity matrix
  LINALG::ApplyDirichlettoSystem(sysmat_sd_,phinp_,residual_,phinp_,*(dbcmaps_->CondMap()));

  // get normalized fine-scale subgrid-diffusivity matrix
  {
    // this is important to have!!!
    MLAPI::Init();

    // extract the ML parameters
    ParameterList&  mlparams = solver_->Params().sublist("ML Parameters");

    // get toggle vector for Dirchlet boundary conditions
    const Epetra_Vector& dbct = *DirichletToggle();

    // get nullspace parameters
    double* nullspace = mlparams.get("null space: vectors",(double*)NULL);
    if (!nullspace) dserror("No nullspace supplied in parameter list");
    int nsdim = mlparams.get("null space: dimension",1);

    // modify nullspace to ensure that DBC are fully taken into account
    if (nullspace)
    {
      const int length = sysmat_sd_->OperatorRangeMap().NumMyElements();
      for (int i=0; i<nsdim; ++i)
        for (int j=0; j<length; ++j)
          if (dbct[j]!=0.0) nullspace[i*length+j] = 0.0;
    }

    // get plain aggregation Ptent
    RCP<Epetra_CrsMatrix> crsPtent;
    GetPtent(*sysmat_sd_->EpetraMatrix(),mlparams,nullspace,crsPtent);
    LINALG::SparseMatrix Ptent(crsPtent);

    // compute scale-separation matrix: S = I - Ptent*Ptent^T
    RCP<LINALG::SparseMatrix> Sep;
    Sep = LINALG::Multiply(Ptent,false,Ptent,true);
    Sep->Scale(-1.0);
    RCP<Epetra_Vector> tmp = LINALG::CreateVector(Sep->RowMap(),false);
    tmp->PutScalar(1.0);
    RCP<Epetra_Vector> diag = LINALG::CreateVector(Sep->RowMap(),false);
    Sep->ExtractDiagonalCopy(*diag);
    diag->Update(1.0,*tmp,1.0);
    Sep->ReplaceDiagonalValues(*diag);

    //complete scale-separation matrix and check maps
    Sep->Complete(Sep->DomainMap(),Sep->RangeMap());
    if (!Sep->RowMap().SameAs(sysmat_sd_->RowMap())) dserror("rowmap not equal");
    if (!Sep->RangeMap().SameAs(sysmat_sd_->RangeMap())) dserror("rangemap not equal");
    if (!Sep->DomainMap().SameAs(sysmat_sd_->DomainMap())) dserror("domainmap not equal");

    // precomputation of unscaled S^T*M*S:
    // pre- and post-multiply M by scale-separating operator matrix Sep
    Mnsv_ = LINALG::Multiply(*sysmat_sd_,false,*Sep,false);
    Mnsv_ = LINALG::Multiply(*Sep,true,*Mnsv_,false);
  }

  return;
}


/*----------------------------------------------------------------------*
 |  scaling of AVM3-based subgrid-diffusivity matrix           vg 10/08 |
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::AVM3Scaling(ParameterList& eleparams)
{
  // time measurement: avm3
  TEUCHOS_FUNC_TIME_MONITOR("SCATRA:            + avm3");

  // some necessary definitions
  int ierr;
  double* sgvsqrt = 0;
  int length = subgrdiff_->MyLength();

  // square-root of subgrid-viscosity-scaling vector for left and right scaling
  sgvsqrt = (double*)subgrdiff_->Values();
  for (int i = 0; i < length; ++i)
  {
    sgvsqrt[i] = sqrt(sgvsqrt[i]);
    subgrdiff_->ReplaceMyValues(1,&sgvsqrt[i],&i);
  }

  // get unscaled S^T*M*S from Sep
  sysmat_sd_ = rcp(new LINALG::SparseMatrix(*Mnsv_));

  // left and right scaling of normalized fine-scale subgrid-viscosity matrix
  ierr = sysmat_sd_->LeftScale(*subgrdiff_);
  if (ierr) dserror("Epetra_CrsMatrix::LeftScale returned err=%d",ierr);
  ierr = sysmat_sd_->RightScale(*subgrdiff_);
  if (ierr) dserror("Epetra_CrsMatrix::RightScale returned err=%d",ierr);

  // add the subgrid-viscosity-scaled fine-scale matrix to obtain complete matrix
  Teuchos::RCP<LINALG::SparseMatrix> sysmat = SystemMatrix();
  sysmat->Add(*sysmat_sd_,false,1.0,1.0);

  // set subgrid-diffusivity vector to zero after scaling procedure
  subgrdiff_->PutScalar(0.0);

  return;
}


/*----------------------------------------------------------------------*
 | set initial thermodynamic pressure and time derivative      vg 12/08 |
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::SetInitialThermPressure(const double thermpress)
{
  // set initial thermodynamic pressure
  thermpressn_ = thermpress;

  // set scalar and density vector values needed by elements
  discret_->ClearState();
  discret_->SetState("phinp",phin_);
  discret_->SetState("densnp",densn_);

  // define element parameter list
  ParameterList eleparams;

  // provide velocity field (export to column map necessary for parallel evaluation)
  // SetState cannot be used since this Multivector is nodebased and not dofbased
  const Epetra_Map* nodecolmap = discret_->NodeColMap();
  RefCountPtr<Epetra_MultiVector> tmp = rcp(new Epetra_MultiVector(*nodecolmap,3));
  LINALG::Export(*convel_,*tmp);
  eleparams.set("velocity field",tmp);

  // set action for elements
  eleparams.set("action","calc_domain_and_bodyforce");
  eleparams.set("total time",0.0);

  // variables for integrals of domain and bodyforce
  Teuchos::RCP<Epetra_SerialDenseVector> scalars
    = Teuchos::rcp(new Epetra_SerialDenseVector(2));

  discret_->EvaluateScalars(eleparams, scalars);

  // get global integral values
  double pardomint  = (*scalars)[0];
  double parbofint  = (*scalars)[1];

  // evaluate domain integral
  // set action for elements
  eleparams.set("action","calc_therm_press");

  // variables for integrals of velocity-divergence and diffusive flux
  double divuint = 0.0;
  double diffint = 0.0;
  eleparams.set("velocity-divergence integral",divuint);
  eleparams.set("diffusive-flux integral",     diffint);

  // evaluate velocity-divergence and rhs on boundaries
  // We may use the flux-calculation condition for calculation of fluxes for 
  // thermodynamic pressure, since it is usually at the same boundary.
  vector<std::string> condnames;
  condnames.push_back("FluxCalculation");
  for (unsigned int i=0; i < condnames.size(); i++)
  {
    discret_->EvaluateCondition(eleparams,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,condnames[i]);
  }

  // get integral values on this proc
  divuint = eleparams.get<double>("velocity-divergence integral");
  diffint = eleparams.get<double>("diffusive-flux integral");

  // get integral values in parallel case
  double pardivuint = 0.0;
  double pardiffint = 0.0;
  discret_->Comm().SumAll(&divuint,&pardivuint,1);
  discret_->Comm().SumAll(&diffint,&pardiffint,1);

  // clean up
  discret_->ClearState();

  // compute initial time derivative of thermodynamic pressure
  // (with specific heat ratio fixed to be 1.4)
  const double shr = 1.4;
  thermpressdtn_ = (-shr*thermpressn_*pardivuint
                    + (shr-1.0)*(pardiffint+parbofint))/pardomint;

  return;
}


/*----------------------------------------------------------------------*
 | compute initial total mass in domain                        vg 01/09 |
 *----------------------------------------------------------------------*/
double SCATRA::ScaTraTimIntImpl::ComputeInitialMass(const double thermpress)
{
  // set initial thermodynamic pressure
  thermpressn_ = thermpress;

  // set scalar and density vector values needed by elements
  discret_->ClearState();
  discret_->SetState("phinp",phinp_);
  discret_->SetState("densnp",densnp_);
  // set action for elements
  ParameterList eleparams;
  eleparams.set("action","calc_temp_and_dens");

  // evaluate integral of inverse temperature
  Teuchos::RCP<Epetra_SerialDenseVector> scalars
    = Teuchos::rcp(new Epetra_SerialDenseVector(3));
  discret_->EvaluateScalars(eleparams, scalars);
  discret_->ClearState();   // clean up

  double initialmass = (*scalars)[1];

  // print out initial total mass
  if (myrank_ == 0)
  {
    cout << endl;
    cout << "+--------------------------------------------------------------------------------------------+" << endl;
    cout << "Initial total mass in domain: " << initialmass << endl;
    cout << "+--------------------------------------------------------------------------------------------+" << endl;
  }

  return initialmass;
}


/*----------------------------------------------------------------------*
 | compute thermodynamic pressure from mass conservation       vg 01/09 |
 *----------------------------------------------------------------------*/
double SCATRA::ScaTraTimIntImpl::ComputeThermPressureFromMassCons(
      const double initialmass,
      const double gasconstant)
{
  // provide storage space for inverse temperature and compute
  const Epetra_Map* dofrowmap = discret_->DofRowMap();
  invphinp_ = LINALG::CreateVector(*dofrowmap,true);
  invphinp_->Reciprocal(*phinp_);

  // set scalar and inverse-scalar (on density state) values needed by elements
  discret_->ClearState();
  discret_->SetState("phinp",phinp_);
  discret_->SetState("densnp",invphinp_);
  // set action for elements
  ParameterList eleparams;
  eleparams.set("action","calc_temp_and_dens");

  // evaluate integral of inverse temperature
  Teuchos::RCP<Epetra_SerialDenseVector> scalars
    = Teuchos::rcp(new Epetra_SerialDenseVector(3));
  discret_->EvaluateScalars(eleparams, scalars);
  discret_->ClearState();   // clean up

  // compute thermodynamic pressure: tp = R*M_0/int(1/T)
  thermpressnp_ = gasconstant*initialmass/(*scalars)[1];

  // compute time derivative of thermodynamic pressure: tpdt = (tp(n+1)-tp(n))/dt
  thermpressdtnp_ = (thermpressnp_-thermpressn_)/dta_;

  // print out thermodynamic pressure and its time derivative
  if (myrank_ == 0)
  {
    cout << endl;
    cout << "+--------------------------------------------------------------------------------------------+" << endl;
    cout << "Thermodynamic pressure from mass conservation: " << thermpressnp_ << endl;
    cout << "Time derivative of thermodynamic pressure: " << thermpressdtnp_ << endl;
    cout << "+--------------------------------------------------------------------------------------------+" << endl;
  }

  return thermpressnp_;
}


/*----------------------------------------------------------------------*
 | compute density for low-Mach-number flow                    vg 08/08 |
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::ComputeDensity(const double thermpress,
                                              const double gasconstant)
{
  // compute density based on equation of state:
  // rho = (p_therm/R)*(1/T) = (thermpress/gasconstant)*(1/T)
  densnp_->Reciprocal(*phinp_);
  densnp_->Scale(thermpress/gasconstant);

  return;
}


/*----------------------------------------------------------------------*
 | set velocity field for low-Mach-number flow                 vg 11/08 |
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::SetLomaVelocity(
  RCP<const Epetra_Vector> extvel,
  RCP<const Epetra_Vector> extsubgrvisc,
  RCP<DRT::Discretization> fluiddis)
{
  // store temperature and velocity of previous iteration for convergence check
  tempincnp_->Update(1.0,*phinp_,0.0);
  //velincnp_->Update(1.0,*convel_,0.0);

  // check vector compatibility and determine space dimension
  int numdim =-1;
  if (extvel->MyLength()<= (4* convel_->MyLength()) and
      extvel->MyLength() > (3* convel_->MyLength()))
    numdim = 3;
  else if (extvel->MyLength()<= (3* convel_->MyLength()))
    numdim = 2;
  else
    dserror("fluid velocity vector too large");

  // get noderowmap of scatra discretization
  const Epetra_Map* scatranoderowmap = discret_->NodeRowMap();

  // get dofrowmap of fluid discretization
  const Epetra_Map* fluiddofrowmap = fluiddis->DofRowMap();

  vector<int>    Indices(1);
  vector<double> Values(1);

  // loop over local nodes of scatra discretization
  for(int lnodeid=0;lnodeid<discret_->NumMyRowNodes();lnodeid++)
  {
    // first of all, assume the present node is not a slavenode
    bool slavenode=false;

    // get the processor-local scatra node
    DRT::Node*  scatralnode = discret_->lRowNode(lnodeid);

    // get the processor-local fluid node
    DRT::Node*  fluidlnode = fluiddis->lRowNode(lnodeid);

    // the set of degrees of freedom associated with the fluid node
    vector<int> fluidnodedofset = fluiddis->Dof(fluidlnode);

    // check whether we have a pbc condition on this scatra node
    vector<DRT::Condition*> mypbc;
    scatralnode->GetCondition("SurfacePeriodic",mypbc);

    // yes, we have a periodic boundary condition on this scatra node
    if (mypbc.size()>0)
    {
      // get master and list of all his slavenodes
      map<int, vector<int> >::iterator master = pbcmapmastertoslave_->find(scatralnode->Id());

      // check whether this is a slavenode
      if (master == pbcmapmastertoslave_->end())
      {
        // indeed a slavenode
        slavenode = true;
      }
      else
      {
        // we have a masternode: set values for all slavenodes
        vector<int>::iterator i;
        for(i=(master->second).begin();i!=(master->second).end();++i)
        {
          // global and processor-local scatra node ID for slavenode
          int globalslaveid = *i;
          int localslaveid  = scatranoderowmap->LID(globalslaveid);

          // get the processor-local fluid slavenode
          DRT::Node*  fluidlslavenode = fluiddis->lRowNode(localslaveid);

          // the set of degrees of freedom associated with the node
          vector<int> slavenodedofset = fluiddis->Dof(fluidlslavenode);

          // global and processor-local fluid dof ID
          int fgid = slavenodedofset[0];
          int flid = fluiddofrowmap->LID(fgid);

          // get velocity for this processor-local fluid dof
          double velocity = (*extvel)[flid];
          // insert velocity value in vector
          convel_->ReplaceMyValue(lnodeid, 0, velocity);

          // get subgrid viscosity for this processor-local fluid dof
          // and divide by turbulent Prandtl number to get diffusivity
          Indices[0] = localslaveid;
          Values[0]  = (*extsubgrvisc)[flid]/tpn_;
          subgrdiff_->ReplaceMyValues(1,&Values[0],&Indices[0]);

          for(int index=1;index<numdim;++index)
          {
            // global and processor-local fluid dof ID
            fgid = slavenodedofset[index];
            flid = fluiddofrowmap->LID(fgid);

            // get velocity for this processor-local fluid dof
            double velocity =(*extvel)[flid];
            // insert velocity value in vector
            convel_->ReplaceMyValue(localslaveid, index, velocity);
          }
        }
      }
    }

    // do this for all nodes other than slavenodes
    if (slavenode == false)
    {
      // global and processor-local fluid dof ID
      int fgid = fluidnodedofset[0];
      int flid = fluiddofrowmap->LID(fgid);

      // get velocity for this processor-local fluid dof
      double velocity = (*extvel)[flid];
      // insert velocity value in vector
      convel_->ReplaceMyValue(lnodeid, 0, velocity);

      // get subgrid viscosity for this processor-local fluid dof
      // and divide by turbulent Prandtl number to get diffusivity
      Indices[0] = lnodeid;
      Values[0]  = (*extsubgrvisc)[flid]/tpn_;
      subgrdiff_->ReplaceMyValues(1,&Values[0],&Indices[0]);

      for(int index=1;index<numdim;++index)
      {
        // global and processor-local fluid dof ID
        fgid = fluidnodedofset[index];
        flid = fluiddofrowmap->LID(fgid);

        // get velocity for this processor-local fluid dof
        double velocity = (*extvel)[flid];
        // insert velocity value in vector
        convel_->ReplaceMyValue(lnodeid, index, velocity);
      }
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 | convergence check for low-Mach-number flow                  vg 01/09 |
 *----------------------------------------------------------------------*/
bool SCATRA::ScaTraTimIntImpl::LomaConvergenceCheck(int          itnum,
                                                    int          itmax,
                                                    const double ittol)
{
  bool stopnonliniter = false;

  // compute L2-norm of incremental temperature and temperature
  double tempincnorm_L2;
  double tempnorm_L2;
  tempincnp_->Update(1.0,*phinp_,-1.0);
  tempincnp_->Norm2(&tempincnorm_L2);
  phinp_->Norm2(&tempnorm_L2);

  /*double velincnorm_L2;
  double velnorm_L2;
  velincnp_->Update(1.0,*convel_,-1.0);
  velincnp_->Norm2(&velincnorm_L2);
  convel_->Norm2(&velnorm_L2);*/

  // care for the case that there is (almost) zero temperature or velocity
  // (usually not required for temperature)
  //if (velnorm_L2 < 1e-6) velnorm_L2 = 1.0;
  //if (tempnorm_L2 < 1e-6) tempnorm_L2 = 1.0;

  if (myrank_==0)
  {
    cout<<"\n******************************************\n           OUTER ITERATION STEP\n******************************************\n";
    printf("+------------+-------------------+--------------+\n");
    printf("|- step/max -|- tol      [norm] -|-- temp-inc --|\n");
    printf("|  %3d/%3d   | %10.3E[L_2 ]  | %10.3E   |",
         itnum,itmax,ittol,tempincnorm_L2/tempnorm_L2);
    printf("\n");
    printf("+------------+-------------------+--------------+\n");

    /*printf("+------------+-------------------+--------------+--------------+\n");
    printf("|- step/max -|- tol      [norm] -|-- temp-inc --|-- vel-inc --|\n");
    printf("|  %3d/%3d   | %10.3E[L_2 ]  | %10.3E   | %10.3E   |",
         itnum,itmax,ittol,tempincnorm_L2/tempnorm_L2,velincnorm_L2/velnorm_L2);
    printf("\n");
    printf("+------------+-------------------+--------------+--------------+\n");*/
  }

  /*if ((tempincnorm_L2/tempnorm_L2 <= ittol) and (velincnorm_L2/velnorm_L2 <= ittol))
    stopnonliniter=true;*/
  if ((tempincnorm_L2/tempnorm_L2 <= ittol)) stopnonliniter=true;

  // warn if itemax is reached without convergence, but proceed to next timestep
  /*if ((itnum == itmax) and
      ((tempincnorm_L2/tempnorm_L2 > ittol) or (velincnorm_L2/velnorm_L2 > ittol)))*/
  if ((itnum == itmax) and (tempincnorm_L2/tempnorm_L2 > ittol))
  {
    stopnonliniter=true;
    if (myrank_==0)
    {
      printf("|     >>>>>> not converged in itemax steps!     |\n");
      printf("+-----------------------------------------------+\n");
    }
  }

  return stopnonliniter;
}


/*----------------------------------------------------------------------*
 | update thermodynamic pressure for mass conservation         vg 01/09 |
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::UpdateThermPressureFromMassCons()
{
  thermpressn_   = thermpressnp_;

  return;
}


/*----------------------------------------------------------------------*
 |  output of some mean values                               gjb   01/09|
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::OutputMeanTempAndDens()
{
  // set scalar and density vector values needed by elements
  discret_->ClearState();
  discret_->SetState("phinp",phinp_);
  discret_->SetState("densnp",densnp_);
  // set action for elements
  ParameterList eleparams;
  eleparams.set("action","calc_temp_and_dens");

  // evaluate integrals of temperature/concentrations, density and domain
  Teuchos::RCP<Epetra_SerialDenseVector> scalars
    = Teuchos::rcp(new Epetra_SerialDenseVector(numscal_+2));
  discret_->EvaluateScalars(eleparams, scalars);
  discret_->ClearState();   // clean up

  const double densint = (*scalars)[numscal_];
  const double domint  = (*scalars)[numscal_+1];

  // print out values
  if (myrank_ == 0)
  {
    if (prbtype_=="loma")
    {
      cout << "Mean temperature: " << (*scalars)[0]/domint << endl;
      cout << "Mean density:     " << densint/domint << endl;
    }
    else
    {
        cout << "Domain integral:          " << domint << endl;
      for (int k = 0; k < numscal_; k++)
      {
        //cout << "Total concentration (c_"<<k+1<<"): "<< (*scalars)[k] << endl;
        cout << "Mean concentration (c_"<<k+1<<"): "<< (*scalars)[k]/domint << endl;
      }
        cout << "Mean density:             " << densint/domint << endl;
    }
  }

  return;
}


/*----------------------------------------------------------------------*
 |  output of electrode status information to screen         gjb  01/09 |
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::OutputElectrodeInfo()
{
  // set vector values needed by elements
  discret_->ClearState();
  discret_->SetState("phinp",phinp_);
  // set action for elements
  ParameterList eleparams;
  eleparams.set("action","calc_elch_electrode_kinetics");
  eleparams.set("calc_status",true); // just want to have a status ouput!
  eleparams.set("iselch",(prbtype_=="elch")); // a boolean
  eleparams.set("problem type",prbtype_);
  eleparams.set("frt",frt_);
  // add element parameters and density state according to time-int. scheme
  AddSpecificTimeIntegrationParameters(eleparams);

  // calculate normal flux vector field only for these boundary conditions:
  std::string condname("ElectrodeKinetics");

  double sum(0.0);

  vector<DRT::Condition*> cond;
  discret_->GetCondition(condname,cond);

  // leave method, if there's nothing to do!
  if (!cond.size()) return;

  if (myrank_ == 0)
  {
    cout<<"Status of '"<<condname<<"':\n"
    <<"++----+---------------------+------------------+----------------------+--------------------+----------------+----------------+"<<endl;
    printf("|| ID |    Total current    | Area of boundary | Mean current density | Mean overpotential | Electrode pot. | Mean Concentr. |\n");
  }

  // first, add to all conditions of interest a ConditionID
  for (int condid = 0; condid < (int) cond.size(); condid++)
  {
    // is there already a ConditionID?
    const vector<int>*    CondIDVec  = cond[condid]->Get<vector<int> >("ConditionID");
    if (CondIDVec)
    { 
      if ((*CondIDVec)[0] != condid)
        dserror("Condition %s has non-matching ConditionID",condname.c_str());
    }
    else
    {
      // let's add a ConditionID
      cond[condid]->Add("ConditionID",condid);
    }
  }
  // now we evaluate the conditions and seperate via ConditionID
  for (int condid = 0; condid < (int) cond.size(); condid++)
  {
    // calculate integral of normal fluxes over indicated boundary and it's area
    eleparams.set("currentintegral",0.0);
    eleparams.set("boundaryintegral",0.0);
    eleparams.set("overpotentialintegral",0.0);
    eleparams.set("concentrationintegral",0.0);

    // would be nice to have a EvaluateScalar for conditions!!!
    discret_->EvaluateCondition(eleparams,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,condname,condid);

    // get integral of current on this proc
    double currentintegral = eleparams.get<double>("currentintegral");
    // get area of the boundary on this proc
    double boundaryint = eleparams.get<double>("boundaryintegral");
    // get integral of overpotential on this proc
    double overpotentialint = eleparams.get<double>("overpotentialintegral");
    // get integral of reactant concentration on this proc
    double cint = eleparams.get<double>("concentrationintegral");

    // care for the parallel case
    double parcurrentintegral = 0.0;
    discret_->Comm().SumAll(&currentintegral,&parcurrentintegral,1);
    double parboundaryint = 0.0;
    discret_->Comm().SumAll(&boundaryint,&parboundaryint,1);
    double paroverpotentialint = 0.0;
    discret_->Comm().SumAll(&overpotentialint,&paroverpotentialint,1);
    double parcint = 0.0;
    discret_->Comm().SumAll(&cint,&parcint,1);

    // access some parameters of the actual condition
    double pot0 = cond[condid]->GetDouble("pot0");
    const int curvenum = cond[condid]->Getint("curve");
    if (curvenum>=0)
    {
      const double curvefac = DRT::UTILS::TimeCurveManager::Instance().Curve(curvenum).f(time_);
      // adjust potential at metal side accordingly
      pot0 *= curvefac;
    }

    // print out results
    if (myrank_ == 0)
    {
      printf("|| %2d |     %10.3E      |    %10.3E    |      %10.3E      |     %10.3E     |   %10.3E   |   %10.3E   |\n",
        condid,parcurrentintegral,parboundaryint,parcurrentintegral/parboundaryint,paroverpotentialint/parboundaryint, pot0, parcint/parboundaryint);
    }
    sum+=parcurrentintegral;
  } // loop over condid

  if (myrank_==0)
    cout<<"++----+---------------------+------------------+----------------------+--------------------+----------------+----------------+"<<endl;

  // print out the net total current for all indicated boundaries
  if (myrank_ == 0)
    printf("Net total current over boundary: %10.3E\n\n",sum);

  // clean up
  discret_->ClearState();

  return;
} // ScaTraImplicitTimeInt::OutputElectrodeInfo




/*----------------------------------------------------------------------*
 |  write mass / heat flux vector to BINIO                   gjb   08/08|
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::OutputFlux()
{
  RCP<Epetra_MultiVector> flux = CalcFlux();

  // post_drt_ensight does not support multivectors based on the dofmap
  // for now, I create single vectors that can be handled by the filter

  // get the noderowmap
  const Epetra_Map* noderowmap = discret_->NodeRowMap();
  Teuchos::RCP<Epetra_MultiVector> fluxk = rcp(new Epetra_MultiVector(*noderowmap,3,true));
  for(int k=1;k<=numscal_;++k)
  {
    ostringstream temp;
    temp << k;
    string name = "flux_phi_"+temp.str();
    for (int i = 0;i<fluxk->MyLength();++i)
    {
      DRT::Node* actnode = discret_->lRowNode(i);
      int dofgid = discret_->Dof(actnode,k-1);
      fluxk->ReplaceMyValue(i,0,((*flux)[0])[(flux->Map()).LID(dofgid)]);
      fluxk->ReplaceMyValue(i,1,((*flux)[1])[(flux->Map()).LID(dofgid)]);
      fluxk->ReplaceMyValue(i,2,((*flux)[2])[(flux->Map()).LID(dofgid)]);
    }
    if (numscal_==1)
      output_->WriteVector("flux", fluxk, IO::DiscretizationWriter::nodevector);
    else
      output_->WriteVector(name, fluxk, IO::DiscretizationWriter::nodevector);
  }
  // that's it
  return;
}


/*----------------------------------------------------------------------*
 |  calculate mass / heat flux vector                        gjb   04/08|
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_MultiVector> SCATRA::ScaTraTimIntImpl::CalcFlux()
{
  // get a vector layout from the discretization to construct matching
  // vectors and matrices
  //                 local <-> global dof numbering
  const Epetra_Map* dofrowmap = discret_->DofRowMap();

  // empty vector for (normal) mass or heat flux vectors (always 3D)
  Teuchos::RCP<Epetra_MultiVector> flux = rcp(new Epetra_MultiVector(*dofrowmap,3,true));

  // set control parameters
  string fluxcomputation("nowhere"); // domain / boundary
  string fluxtype("noflux"); // noflux / totalflux / diffusiveflux
  if (writeflux_!="No")
  {
    size_t pos = writeflux_.find("_");    // find position of "_" in str
    fluxcomputation = writeflux_.substr(pos+1);   // get from "_" to the end
    fluxtype = writeflux_.substr(0,pos); // get from beginning to "_"
  }

  // now compute the fluxes
  if (fluxcomputation=="domain")
  {
    // The normal flux calculation is based on the idea proposed in
    // GRESHO ET AL.,
    // "THE CONSISTENT GALERKIN FEM FOR COMPUTING DERIVED BOUNDARY
    // QUANTITIES IN THERMAL AND/OR FLUIDS PROBLEMS",
    // INTERNATIONAL JOURNAL FOR NUMERICAL METHODS IN FLUIDS, VOL. 7, 371-394 (1987)
    // For the moment, we are lumping the 'boundary mass matrix' instead of solving
    // a smal linear system!

    // We have to treat each spatial direction separately
    Teuchos::RCP<Epetra_Vector> fluxx = LINALG::CreateVector(*dofrowmap,true);
    Teuchos::RCP<Epetra_Vector> fluxy = LINALG::CreateVector(*dofrowmap,true);
    Teuchos::RCP<Epetra_Vector> fluxz = LINALG::CreateVector(*dofrowmap,true);

    // set action for elements
    ParameterList params;
    params.set("action","calc_condif_flux");
    params.set("problem type",prbtype_);
    params.set("frt",frt_);

    //provide velocity field (export to column map necessary for parallel evaluation)
    const Epetra_Map* nodecolmap = discret_->NodeColMap();
    RefCountPtr<Epetra_MultiVector> vel = rcp(new Epetra_MultiVector(*nodecolmap,3));
    LINALG::Export(*convel_,*vel);
    params.set("velocity field",vel);
    params.set("fluxtype",fluxtype);

    // set vector values needed by elements
    discret_->ClearState();
    discret_->SetState("phinp",phinp_);
    discret_->SetState("densnp",densnp_);

    // evaluate fluxes in the whole computational domain (e.g., for visualization of particle path-lines)
    discret_->Evaluate(params,Teuchos::null,Teuchos::null,fluxx,fluxy,fluxz);

    // insert values into final flux vector for visualization
    for (int i = 0;i<flux->MyLength();++i)
    {
      flux->ReplaceMyValue(i,0,(*fluxx)[i]);
      flux->ReplaceMyValue(i,1,(*fluxy)[i]);
      flux->ReplaceMyValue(i,2,(*fluxz)[i]);
    }
  }
  if (fluxcomputation=="boundary")
  {
    // calculate normal flux vector field only for these boundary conditions:
    vector<std::string> condnames;
    condnames.push_back("FluxCalculation");
    condnames.push_back("ElectrodeKinetics");
    condnames.push_back("LineNeumann");
    condnames.push_back("SurfaceNeumann");

    // determine the averaged normal vector field for indicated boundaries
    // used for the output of the normal flux as a vector field
    // is computed only once, since there is no ALE support at the moment
    if (normals_ == Teuchos::null) 
      normals_ = ComputeNormalVectors(condnames);

    // was the residual already prepared? (Important only for
    // the result test)
    if ((!nonlinear_) && (lastfluxoutputstep_ != step_))
    {
      lastfluxoutputstep_ = step_;

      // For nonlinear problems we already have the actual residual vector.
      // For linear problems we have to compute this information first:

      // zero out matrix entries
      sysmat_->Zero();

      // zero out residual vector
      residual_->PutScalar(0.0);

      ParameterList eleparams;
      // action for elements
      eleparams.set("action","calc_condif_systemmat_and_residual");

      // other parameters that might be needed by the elements
      eleparams.set("time-step length",dta_);
      eleparams.set("problem type",prbtype_);
      eleparams.set("incremental solver",true); // say yes and you get the residual!!
      eleparams.set("form of convective term",convform_);
      eleparams.set("fs subgrid diffusivity",fssgd_);
      eleparams.set("turbulence model",turbmodel_);
      eleparams.set("frt",frt_);

      //provide velocity field (export to column map necessary for parallel evaluation)
      //SetState cannot be used since this Multivector is nodebased and not dofbased
      const Epetra_Map* nodecolmap = discret_->NodeColMap();
      RefCountPtr<Epetra_MultiVector> tmp = rcp(new Epetra_MultiVector(*nodecolmap,3));
      LINALG::Export(*convel_,*tmp);
      eleparams.set("velocity field",tmp);

      // parameters for stabilization
      eleparams.sublist("STABILIZATION") = params_->sublist("STABILIZATION");

      // set vector values needed by elements
      discret_->ClearState();
      discret_->SetState("hist",hist_);
      if (turbmodel_) discret_->SetState("subgrid diffusivity",subgrdiff_);

      // add element parameters and density state according to time-int. scheme
      AddSpecificTimeIntegrationParameters(eleparams);

      {
        // call standard loop over elements
        discret_->Evaluate(eleparams,sysmat_,null,residual_,null,null);
        discret_->ClearState();
      }

      // scaling to get true residual vector for all time integration schemes
      trueresidual_->Update(ResidualScaling(),*residual_,0.0);

    } // if ((!nonlinear_) && (lastfluxoutputstep_ != step_))

    double normfluxsum(0.0);

    for (unsigned int i=0; i < condnames.size(); i++)
    {
      vector<DRT::Condition*> cond;
      discret_->GetCondition(condnames[i],cond);

      // go to the next condition type, if there's nothing to do!
      if (!cond.size()) continue;

      if (myrank_ == 0)
      {
        cout<<"Normal fluxes at boundary '"<<condnames[i]<<"':\n"
        <<"+----+-------------------------+------------------+--------------------------+"<<endl;
        printf("| ID | Integral of normal flux | Area of boundary | Mean normal flux density |\n");
      }

      // first, add to all conditions of interest a ConditionID
      for (int condid = 0; condid < (int) cond.size(); condid++)
      {
        // is there already a ConditionID?
        const vector<int>*    CondIDVec  = cond[condid]->Get<vector<int> >("ConditionID");
        if (CondIDVec)
        { 
          if ((*CondIDVec)[0] != condid)
            dserror("Condition %s has non-matching ConditionID",condnames[i].c_str());
        }
        else
        {
          // let's add a ConditionID
          cond[condid]->Add("ConditionID",condid);
        }
      }

      // now we evaluate the conditions and seperate via ConditionID
      for (int condid = 0; condid < (int) cond.size(); condid++)
      {
        ParameterList params;

        // calculate integral of shape functions over indicated boundary and it's area
        params.set("boundaryint",0.0);
        params.set("action","integrate_shape_functions");

        // create vector (+ initialization with zeros)
        Teuchos::RCP<Epetra_Vector> integratedshapefunc = LINALG::CreateVector(*dofrowmap,true);

        // call loop over elements
        discret_->ClearState();
        discret_->EvaluateCondition(params,integratedshapefunc,condnames[i],condid);
        discret_->ClearState();

        double normfluxintegral(0.0);

        // insert values into final flux vector for visualization
        int numrownodes = discret_->NumMyRowNodes();
        for (int lnodid = 0; lnodid < numrownodes; ++lnodid )
        {
          DRT::Node* actnode = discret_->lRowNode(lnodid);
          for (int idof = 0; idof < discret_->NumDof(actnode); ++idof)
          {
            int dofgid = discret_->Dof(actnode,idof);
            int doflid = dofrowmap->LID(dofgid);

            if ((*integratedshapefunc)[doflid] != 0.0)
            {
              // this is the value of the normal flux
              double normflux = ((*trueresidual_)[doflid])/(*integratedshapefunc)[doflid];
              if (idof == 0) // integral value only for first scalar!
              {
                normfluxintegral += (*trueresidual_)[doflid];
              }
              // for visualization, we plot the normal flux with 
              // outward pointing normal vector
              for (int idim = 0; idim < 3; idim++)
              {
                Epetra_Vector* normalcomp = (*normals_)(idim);
                double normalveccomp =(*normalcomp)[lnodid];
                flux->ReplaceMyValue(doflid,idim,normflux*normalveccomp);
              }
            }
          }
        }

        // get area of the boundary on this proc
        double boundaryint = params.get<double>("boundaryint");

        // care for the parallel case
        double parnormfluxintegral = 0.0;
        discret_->Comm().SumAll(&normfluxintegral,&parnormfluxintegral,1);
        double parboundaryint = 0.0;
        discret_->Comm().SumAll(&boundaryint,&parboundaryint,1);

        // print out results
        if (myrank_ == 0)
        {
          printf("| %2d |       %10.3E        |    %10.3E    |        %10.3E        |\n",
              condid,parnormfluxintegral,parboundaryint,parnormfluxintegral/parboundaryint);
        }
        normfluxsum+=parnormfluxintegral;

        // statistics section for normfluxintegral
        if (step_>=samstart_ && step_<=samstop_)
        {
          (*sumnormfluxintegral_)[condid] += parnormfluxintegral;
          int samstep = step_-samstart_+1;

          // dump every dumperiod steps
          if (samstep%dumperiod_==0)
          {
            double meannormfluxintegral = (*sumnormfluxintegral_)[condid]/samstep;
            // dump statistical results
            if (myrank_ == 0)
            {
              printf("| %2d | Mean normal-flux integral (step %5d -- step %5d) :   %12.5E |\n", condid,samstart_,step_,meannormfluxintegral);
            }
          }
        }
      } // loop over condid

      if (myrank_==0)
        cout<<"+----+-------------------------+------------------+--------------------------+"<<endl;
    }

    // print out the accumulated normal flux over all indicated boundaries
    if (myrank_ == 0)
      printf("Sum of all normal flux boundary integrals: %10.3E\n\n",normfluxsum);

  } // boundary

  // clean up
  discret_->ClearState();

  return flux;
}


/*----------------------------------------------------------------------*
 | compute outward pointing unit normal vectors at given b.c.  gjb 01/09|
 *----------------------------------------------------------------------*/
RCP<Epetra_MultiVector> SCATRA::ScaTraTimIntImpl::ComputeNormalVectors( 
    const vector<string>& condnames
)
{
  // create vectors for x,y and z component of average normal vector field
  // get noderowmap of discretization
  const Epetra_Map* noderowmap = discret_->NodeRowMap();
  RCP<Epetra_MultiVector> normal = rcp(new Epetra_MultiVector(*noderowmap,3,true));

  discret_->ClearState();

  // set action for elements
  ParameterList eleparams;
  eleparams.set("action","calc_normal_vectors");
  eleparams.set<RCP<Epetra_MultiVector> >("normal vectors",normal);

  // loop over all intended types of conditions
  for (unsigned int i=0; i < condnames.size(); i++)
  {
    discret_->EvaluateCondition(eleparams,condnames[i]);
  }

  // clean up
  discret_->ClearState();

  // the normal vector field is not properly scaled up to now. We do this here
  int numrownodes = discret_->NumMyRowNodes();
  Epetra_Vector* xcomp = (*normal)(0);
  Epetra_Vector* ycomp = (*normal)(1);
  Epetra_Vector* zcomp = (*normal)(2);
  for (int i=0; i<numrownodes; ++i)
  {
    double x = (*xcomp)[i];
    double y = (*ycomp)[i];
    double z = (*zcomp)[i];
    double norm = sqrt(x*x + y*y + z*z);
    // form the unit normal vector
    if (norm > EPS15)
    {
      normal->ReplaceMyValue(i,0,x/norm);
      normal->ReplaceMyValue(i,1,y/norm);
      normal->ReplaceMyValue(i,2,z/norm);
    }
  }

  return normal;
}


/*----------------------------------------------------------------------*
 |  calculate error compared to analytical solution            gjb 10/08|
 *----------------------------------------------------------------------*/
void SCATRA::ScaTraTimIntImpl::EvaluateErrorComparedToAnalyticalSol()
{
  int calcerr = params_->get<int>("CALCERROR");

  switch (calcerr)
  {
  case 0: // do nothing (the usual case)
    break;
  case 1:
  {
    //------------------------------------------------- Kwok et Wu,1995
    //   Reference:
    //   Kwok, Yue-Kuen and Wu, Charles C. K.
    //   "Fractional step algorithm for solving a multi-dimensional diffusion-migration equation"
    //   Numerical Methods for Partial Differential Equations
    //   1995, Vol 11, 389-397

    // create the parameters for the discretization
    ParameterList p;

    // parameters for the elements
    p.set("action","calc_elch_kwok_error");
    p.set("total time",time_);
    p.set("frt",frt_);

    // set vector values needed by elements
    discret_->ClearState();
    discret_->SetState("phinp",phinp_);

    // get (squared) error values
    Teuchos::RCP<Epetra_SerialDenseVector> errors
      = Teuchos::rcp(new Epetra_SerialDenseVector(3));
    discret_->EvaluateScalars(p, errors);
    discret_->ClearState();

    // for the L2 norm, we need the square root
    double conerr1 = sqrt((*errors)[0]);
    double conerr2 = sqrt((*errors)[1]);
    double poterr  = sqrt((*errors)[2]);

    if (myrank_ == 0)
    {
      printf("\nL2_err for Kwok and Wu:\n");
      printf(" concentration1 %15.8e\n concentration2 %15.8e\n potential      %15.8e\n\n",
             conerr1,conerr2,poterr);
    }
  }
  break;
  default:
    dserror("Cannot calculate error. Unknown type of analytical test problem");
  }
  return;
}


#endif /* CCADISCRET       */

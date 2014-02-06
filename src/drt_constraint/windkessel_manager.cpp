/*!----------------------------------------------------------------------
\file windkessel_manager.cpp

\brief Class controlling Windkessel boundary conditions and containing the necessary data

**************************************************************************************************************************
Monolithic coupling of a three-element Windkessel governed by

either the standard linear version in p
(1) c dp/dt - c r2 dq/dt + p/r1 - (1 + r2/r1) q(d) = 0

or a special nonlinear heart version to mimic opened and closed valves
(2) c(p) dp/dt - c(p) r2(p) dq/dt + p/r1(p) - (1 + r2(p)/r1(p)) q(d) = 0

with
c(p) = (c_iso - c_ejec)*0.5*(1.0 - tanh[(p-p_open)/k_p] ) + c_ejec + (c_fill - c_iso)*(1.0 - tanh[(p-p_close)/k_p] )
r1(p) = (r1_iso - r1_ejec)*0.5*(1.0 - tanh[(p-p_open)/k_p] ) + r1_ejec + (r1_fill - r1_iso)*(1.0 - tanh[(p-p_close)/k_p] )
r2(p) = (r2_iso - r2_ejec)*0.5*(1.0 - tanh[(p-p_open)/k_p] ) + r2_ejec + (r2_fill - r2_iso)*(1.0 - tanh[(p-p_close)/k_p] )

[c: compliance, r1: first resistance, r2: second resistance, q = -dV/dt: flux, p: pressure variable]

_iso: values during isovolumic phases
_ejec: values during ejection phases
_fill: values during filling phases
p_open: valve opening pressure
p_close: valve closing pressure

and the standard structural dynamics governing equation

M a + C v + f_int(d) - f_ext(d,p) = 0,

with q being a function of the displacement vector d and f_ext additionally being a function of the Windkessel pressure p.
**************************************************************************************************************************

<pre>
Maintainer: Marc Hirschvogel
            hirschvogel@mhpc.mw.tum.de
            http://www.mhpc.mw.tum.de
            089 - 289-10363
</pre>

*----------------------------------------------------------------------*/

#include <Teuchos_ParameterList.hpp>
#include <Teuchos_StandardParameterEntryValidators.hpp>
#include <stdio.h>
#include <iostream>

#include "../linalg/linalg_utils.H"
#include "../linalg/linalg_solver.H"
#include "../linalg/linalg_mapextractor.H"

#include "../drt_adapter/ad_str_structure.H"
#include "../drt_adapter/ad_str_windkessel_merged.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_condition.H"
#include "windkessel_manager.H"
#include "windkessel.H"
#include "windkesseldofset.H"


/*----------------------------------------------------------------------*
 |  ctor (public)                                              mhv 11/13|
 *----------------------------------------------------------------------*/
UTILS::WindkesselManager::WindkesselManager
(
  RCP<DRT::Discretization> discr,
  RCP<Epetra_Vector> disp,
  Teuchos::ParameterList params,
  LINALG::Solver& solver,
  RCP<LINALG::MapExtractor> dbcmaps):
actdisc_(discr),
myrank_(actdisc_->Comm().MyPID()),
dbcmaps_(Teuchos::rcp(new LINALG::MapExtractor()))
{

	//setup solver
	SolverSetup(discr,solver,dbcmaps,params);

  // a zero vector of full length
  zeros_ = LINALG::CreateVector(*(actdisc_->DofRowMap()), true);

  // Map containing Dirichlet DOFs
  {
    Teuchos::ParameterList p;
    const double time=0.0;
    p.set("total time", time);
    actdisc_->EvaluateDirichlet(p, zeros_, Teuchos::null, Teuchos::null,
                                Teuchos::null, dbcmaps_);
    zeros_->PutScalar(0.0); // just in case of change
  }

  //----------------------------------------------------------------------------
  //---------------------------------------------------------Windkessel Conditions!

  // constructors of Windkessel increment number of Windkessels defined and the minimum
  // ConditionID read so far.
  numWindkesselID_=0;
  WindkesselID_=0;
  offsetID_=10000;
  int maxWindkesselID=0;

  //Check what kind of Windkessel boundary conditions there are
  rcr_=Teuchos::rcp(new Windkessel(actdisc_,"WindkesselStructureCond",offsetID_,maxWindkesselID,currentID));
  rcr_nlnheart_=Teuchos::rcp(new Windkessel(actdisc_,"NonlinHeartWindkesselStructureCond",offsetID_,maxWindkesselID,currentID));

  havewindkessel_= (rcr_->HaveWindkessel()) or (rcr_nlnheart_->HaveWindkessel());
  if (havewindkessel_)
  {
    numWindkesselID_ = std::max(maxWindkesselID-offsetID_+1,0);
    windkesseldofset_ = Teuchos::rcp(new WindkesselDofSet());
    windkesseldofset_ ->AssignDegreesOfFreedom(actdisc_,numWindkesselID_,0);
    offsetID_ -= windkesseldofset_->FirstGID();
    Teuchos::ParameterList p;
    //double time = params.get<double>("total time",0.0);
    double sc_timint = params.get("scale_timint",1.0);
    double gamma = params.get("scale_gamma",1.0);
    double ts_size = params.get("time_step_size",1.0);
    const Epetra_Map* dofrowmap = actdisc_->DofRowMap();
    //build Epetra_Map used as domainmap and rowmap for result vectors
    windkesselmap_=Teuchos::rcp(new Epetra_Map(*(windkesseldofset_->DofRowMap())));
    //build an all reduced version of the windkesselmap, since sometimes all processors
    //have to know all values of the Windkessels and pressures
    redwindkesselmap_ = LINALG::AllreduceEMap(*windkesselmap_);

    // importer
    windkimpo_ = Teuchos::rcp(new Epetra_Export(*redwindkesselmap_,*windkesselmap_));

    //initialize Windkessel stiffness and offdiagonal matrices
    windkesselstiffness_=Teuchos::rcp(new LINALG::SparseMatrix(*windkesselmap_,numWindkesselID_,false,true));
    coupoffdiag_vol_d_=Teuchos::rcp(new LINALG::SparseMatrix(*dofrowmap,numWindkesselID_,false,true));
    coupoffdiag_fext_p_=Teuchos::rcp(new LINALG::SparseMatrix(*dofrowmap,numWindkesselID_,false,true));

    // Initialize vectors
    actdisc_->ClearState();
    pres_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    presn_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    presm_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    presrate_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    presraten_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    presratem_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    vol_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    voln_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    volm_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    flux_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    fluxn_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    fluxm_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    fluxrate_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    fluxraten_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    fluxratem_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    windkesselrhsm_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    windk_rhs_p_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    windk_rhs_dpdt_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    windk_rhs_q_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    windk_rhs_dqdt_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    presnprint_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));
    fluxnprint_=Teuchos::rcp(new Epetra_Vector(*windkesselmap_));

    pres_->PutScalar(0.0);
    presn_->PutScalar(0.0);
    presm_->PutScalar(0.0);
    presrate_->PutScalar(0.0);
    presraten_->PutScalar(0.0);
    presratem_->PutScalar(0.0);
    vol_->PutScalar(0.0);
    voln_->PutScalar(0.0);
    volm_->PutScalar(0.0);
    flux_->PutScalar(0.0);
    fluxn_->PutScalar(0.0);
    fluxm_->PutScalar(0.0);
    fluxrate_->PutScalar(0.0);
    fluxraten_->PutScalar(0.0);
    fluxratem_->PutScalar(0.0);
    windkesselrhsm_->PutScalar(0.0);
    windk_rhs_p_->PutScalar(0.0);
    windk_rhs_dpdt_->PutScalar(0.0);
    windk_rhs_q_->PutScalar(0.0);
    windk_rhs_dqdt_->PutScalar(0.0);
    presnprint_->PutScalar(0.0);
    fluxnprint_->PutScalar(0.0);
    windkesselstiffness_->Zero();

    //p.set("total time",time);
    p.set("OffsetID",offsetID_);
    p.set("NumberofID",numWindkesselID_);
    p.set("scale_timint",sc_timint);
    p.set("scale_gamma",gamma);
    p.set("time_step_size",ts_size);
    actdisc_->SetState("displacement",disp);

    RCP<Epetra_Vector> volredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
    RCP<Epetra_Vector> presredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
    rcr_->Initialize(p,volredundant,presredundant);
    rcr_nlnheart_->Initialize(p,volredundant,presredundant);
    vol_->Export(*volredundant,*windkimpo_,Add);
    pres_->Export(*presredundant,*windkimpo_,Insert);

  }

  return;
}

/*----------------------------------------------------------------------*
|(public)                                                      mhv 11/13|
|Compute difference between current and prescribed values.              |
|Change Stiffnessmatrix and internal force vector                       |
*-----------------------------------------------------------------------*/
void UTILS::WindkesselManager::StiffnessAndInternalForces(
        const double time,
        RCP<Epetra_Vector> displast,
        RCP<Epetra_Vector> disp,
        Teuchos::ParameterList scalelist)
{

  double sc_timint = scalelist.get("scale_timint",1.0);
  double gamma = scalelist.get("scale_gamma",1.0);
  double beta = scalelist.get("scale_beta",1.0);
  double ts_size = scalelist.get("time_step_size",1.0);

  // create the parameters for the discretization
  Teuchos::ParameterList p;
  std::vector<DRT::Condition*> windkesselcond(0);
  const Epetra_Map* dofrowmap = actdisc_->DofRowMap();

  windkesselstiffness_->Zero();
  coupoffdiag_vol_d_->Zero();
  coupoffdiag_fext_p_->Zero();

  // other parameters that might be needed by the elements
  p.set("total time",time);
  p.set("OffsetID",offsetID_);
  p.set("NumberofID",numWindkesselID_);
  p.set("old disp",displast);
  p.set("new disp",disp);
  p.set("scale_timint",sc_timint);
  p.set("scale_gamma",gamma);
  p.set("scale_beta",beta);
  p.set("time_step_size",ts_size);

  Teuchos::RCP<Epetra_Vector> voldummy = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> volnredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> presnredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> presmredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> presratemredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> fluxnredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> fluxmredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> fluxratemredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> windk_rhs_p_red = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> windk_rhs_dpdt_red = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> windk_rhs_q_red = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  Teuchos::RCP<Epetra_Vector> windk_rhs_dqdt_red = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));

  actdisc_->ClearState();
  actdisc_->SetState("displacement",disp);


  // evaluate only current volume
  rcr_->Evaluate(p,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,volnredundant,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null);
  rcr_nlnheart_->Evaluate(p,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,volnredundant,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null);

  voln_->PutScalar(0.0);
  voln_->Export(*volnredundant,*windkimpo_,Add);

  // pressure and volume at generalized midpoint
  presm_->Update(sc_timint, *presn_, (1.-sc_timint), *pres_, 0.0);
  volm_->Update(sc_timint, *voln_, (1.-sc_timint), *vol_, 0.0);

  // update flux - Newmark with second vol derivative
  fluxn_->Update(1.0,*voln_,-1.0,*vol_,0.0);
  fluxn_->Update((beta-gamma)/beta,*flux_,gamma/(beta*ts_size));
  fluxn_->Update((2.*beta-gamma)*ts_size/(2.*beta),*fluxrate_,1.0);
  fluxm_->Update(sc_timint, *fluxn_, (1.-sc_timint), *flux_, 0.0);

  // update flux - Newmark without second vol derivative
  //fluxn_->Update(1.0,*voln_,-1.0,*vol_,0.0);
  //fluxn_->Update((gamma-1.)/gamma,*flux_,1./(gamma*ts_size));
  //fluxm_->Update(sc_timint, *fluxn_, (1.-sc_timint), *flux_, 0.0);

  // update flux rate
  fluxraten_->Update(1.0,*voln_,-1.0,*vol_,0.0);
  fluxraten_->Update(-1./(beta*ts_size),*flux_,1./(beta*ts_size*ts_size));
  fluxraten_->Update((2.*beta-1.)/(2.*beta),*fluxrate_,1.0);
  fluxratem_->Update(sc_timint, *fluxraten_, (1.-sc_timint), *fluxrate_, 0.0);

  // update pressure rate
  presraten_->Update(1.0,*presn_,-1.0,*pres_,0.0);
  presraten_->Update((gamma-1.)/gamma,*presrate_,1./(gamma*ts_size));
  presratem_->Update(sc_timint, *presraten_, (1.-sc_timint), *presrate_, 0.0);


  LINALG::Export(*presratem_,*presratemredundant);
  LINALG::Export(*presm_,*presmredundant);
  LINALG::Export(*fluxratem_,*fluxratemredundant);
  LINALG::Export(*fluxm_,*fluxmredundant);

  // assemble Windkessel stiffness and offdiagonal coupling matrices as well as rhs contributions (of c, r1, r2)
  rcr_->Evaluate(p,windkesselstiffness_,coupoffdiag_vol_d_,Teuchos::null,windk_rhs_p_red,windk_rhs_dpdt_red,windk_rhs_q_red,windk_rhs_dqdt_red,voldummy,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null);
  rcr_->Evaluate(p,Teuchos::null,Teuchos::null,coupoffdiag_fext_p_,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,voldummy,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null);

  rcr_nlnheart_->Evaluate(p,windkesselstiffness_,coupoffdiag_vol_d_,Teuchos::null,windk_rhs_p_red,windk_rhs_dpdt_red,windk_rhs_q_red,windk_rhs_dqdt_red,voldummy,presratemredundant,presmredundant,fluxratemredundant,fluxmredundant);
  rcr_nlnheart_->Evaluate(p,Teuchos::null,Teuchos::null,coupoffdiag_fext_p_,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,voldummy,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null);


  windk_rhs_p_->PutScalar(0.0);
  windk_rhs_p_->Export(*windk_rhs_p_red,*windkimpo_,Insert);
  windk_rhs_dpdt_->PutScalar(0.0);
  windk_rhs_dpdt_->Export(*windk_rhs_dpdt_red,*windkimpo_,Insert);
  windk_rhs_q_->PutScalar(0.0);
  windk_rhs_q_->Export(*windk_rhs_q_red,*windkimpo_,Insert);
  windk_rhs_dqdt_->PutScalar(0.0);
  windk_rhs_dqdt_->Export(*windk_rhs_dqdt_red,*windkimpo_,Insert);

  // Windkessel rhs at generalized midpoint
  windkesselrhsm_->Multiply(1.0,*presm_,*windk_rhs_p_,0.0);
  windkesselrhsm_->Multiply(1.0,*presratem_,*windk_rhs_dpdt_,1.0);
  windkesselrhsm_->Multiply(1.0,*fluxm_,*windk_rhs_q_,1.0);
  windkesselrhsm_->Multiply(1.0,*fluxratem_,*windk_rhs_dqdt_,1.0);

  // finalize the Windkessel stiffness and offdiagonal matrices

  std::string label1(coupoffdiag_vol_d_->Label());
  std::string label2(coupoffdiag_fext_p_->Label());

  // Complete matrices
  windkesselstiffness_->Complete(*windkesselmap_,*windkesselmap_);;

  if (label1 == "LINALG::BlockSparseMatrixBase")
  	coupoffdiag_vol_d_->Complete();
  else
  	coupoffdiag_vol_d_->Complete(*windkesselmap_,*dofrowmap);

  if (label2 == "LINALG::BlockSparseMatrixBase")
  	coupoffdiag_fext_p_->Complete();
  else
  	coupoffdiag_fext_p_->Complete(*windkesselmap_,*dofrowmap);

  LINALG::Export(*fluxn_,*fluxnredundant);
  // ATTENTION: We necessarily need the end-point and NOT the generalized mid-point pressure here
  // since the external load vector will be set to the generalized mid-point by the respective time integrator!
  LINALG::Export(*presn_,*presnredundant);
  EvaluateNeumannWindkesselCoupling(presnredundant);

  return;
}


void UTILS::WindkesselManager::UpdateTimeStep()
{
  pres_->Update(1.0,*presn_,0.0);
  presrate_->Update(1.0,*presraten_,0.0);
  vol_->Update(1.0,*voln_,0.0);
  flux_->Update(1.0,*fluxn_,0.0);
  fluxrate_->Update(1.0,*fluxraten_,0.0);
}



/*----------------------------------------------------------------------*/
/* iterative iteration update of state */
void UTILS::WindkesselManager::UpdatePres(RCP<Epetra_Vector> presincrement)
{
  // new end-point pressures
  // p_{n+1}^{i+1} := p_{n+1}^{i} + Incp_{n+1}^{i}
  presn_->Update(1.0, *presincrement, 1.0);

  return;
}

/*----------------------------------------------------------------------*
|(public)                                                      mhv 12/13|
|Reset reference base values for restart                                |
*-----------------------------------------------------------------------*/
void UTILS::WindkesselManager::SetRefBaseValues(RCP<Epetra_Vector> newrefval,const double& time)
{
  rcr_->Initialize(time);
  rcr_nlnheart_->Initialize(time);

  vol_->Update(1.0, *newrefval,0.0);
  return;
}

/*----------------------------------------------------------------------*/
void UTILS::WindkesselManager::EvaluateNeumannWindkesselCoupling(RCP<Epetra_Vector> actpres)
{

  std::vector<DRT::Condition*> surfneumcond;
  std::vector<int> tmp;
  Teuchos::RCP<DRT::Discretization> structdis = DRT::Problem::Instance()->GetDis("structure");
  if (structdis == Teuchos::null)
    dserror("no structure discretization available");

  // first get all Neumann conditions on structure
  structdis->GetCondition("SurfaceNeumann",surfneumcond);
  unsigned int numneumcond = surfneumcond.size();
  if (numneumcond == 0) dserror("no Neumann conditions on structure");

  // now filter those Neumann conditions that are due to the coupling
  std::vector<DRT::Condition*> coupcond;
  for (unsigned int k = 0; k < numneumcond; ++k)
  {
    DRT::Condition* actcond = surfneumcond[k];
    if (actcond->Type() == DRT::Condition::WindkesselStructureCoupling)
      coupcond.push_back(actcond);
  }
  unsigned int numcond = coupcond.size();
  if (numcond == 0) dserror("no coupling conditions found");


  const Epetra_BlockMap& condmap = actpres->Map();

  for (int i=0; i<condmap.NumMyElements(); ++i)
  {
		DRT::Condition* cond = coupcond[i];
		std::vector<double> newval(6,0.0);
		//make value negative to properly apply via orthopressure routine
		newval[0] = -(*actpres)[i];
		cond->Add("val",newval);
  }

  return;
}

void UTILS::WindkesselManager::PrintPresFlux() const
{
  // prepare stuff for printing to screen
  RCP<Epetra_Vector> presnredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
  RCP<Epetra_Vector> fluxnredundant = Teuchos::rcp(new Epetra_Vector(*redwindkesselmap_));
	LINALG::Export(*presn_,*presnredundant);
	LINALG::Export(*fluxn_,*fluxnredundant);


	if (myrank_ == 0)
	{
		for (int i = 0; i < numWindkesselID_; ++i)
		{
			printf("Windkessel output id%2d:\n",currentID[i]);
			printf("%2d pressure: %10.5e \n",currentID[i],(*presnredundant)[i]);
			printf("%2d flux: %10.5e \n",currentID[i],(*fluxnredundant)[i]);
		}
	}

  return;
}



/*----------------------------------------------------------------------*
 |  set-up (public)                                            mhv 11/13|
 *----------------------------------------------------------------------*/
void UTILS::WindkesselManager::SolverSetup
(
  RCP<DRT::Discretization> discr,
  LINALG::Solver& solver,
  RCP<LINALG::MapExtractor> dbcmaps,
  Teuchos::ParameterList params
)
{

  solver_ = Teuchos::rcp(&solver,false);
  counter_ = 0;

  return;
}



void UTILS::WindkesselManager::Solve
(
  RCP<LINALG::SparseMatrix> stiff,
  RCP<Epetra_Vector> dispinc,
  const RCP<Epetra_Vector> rhsstand
)
{

  // create old style dirichtoggle vector (supposed to go away)
  dirichtoggle_ = Teuchos::rcp(new Epetra_Vector(*(dbcmaps_->FullMap())));
  RCP<Epetra_Vector> temp = Teuchos::rcp(new Epetra_Vector(*(dbcmaps_->CondMap())));
  temp->PutScalar(1.0);
  LINALG::Export(*temp,*dirichtoggle_);

  // allocate additional vectors and matrices
  Teuchos::RCP<Epetra_Vector> rhswindk
	= Teuchos::rcp(new Epetra_Vector(*(GetWindkesselRHS())));
  Teuchos::RCP<Epetra_Vector> presincr
	= Teuchos::rcp(new Epetra_Vector(*(GetWindkesselMap())));
	Teuchos::RCP<LINALG::SparseMatrix> windkstiff =
		(Teuchos::rcp_dynamic_cast<LINALG::SparseMatrix>(GetWindkesselStiffness()));
	Teuchos::RCP<LINALG::SparseMatrix> coupoffdiag_vol_d =
		(Teuchos::rcp_dynamic_cast<LINALG::SparseMatrix>(GetCoupOffdiagVolD()));
	Teuchos::RCP<LINALG::SparseMatrix> coupoffdiag_fext_p =
		(Teuchos::rcp_dynamic_cast<LINALG::SparseMatrix>(GetCoupOffdiagFextP()));

	// prepare residual pressure
	presincr->PutScalar(0.0);


	// apply DBC to additional offdiagonal coupling matrices
	coupoffdiag_vol_d->ApplyDirichlet(*(dbcmaps_->CondMap()),false);
	coupoffdiag_fext_p->ApplyDirichlet(*(dbcmaps_->CondMap()),false);


  // define maps of standard dofs and additional pressures
  RCP<Epetra_Map> standrowmap = Teuchos::rcp(new Epetra_Map(stiff->RowMap()));
  RCP<Epetra_Map> windkrowmap = Teuchos::rcp(new Epetra_Map(windkstiff->RowMap()));

  // merge maps to one large map
  RCP<Epetra_Map> mergedmap = LINALG::MergeMap(standrowmap,windkrowmap,false);
  // define MapExtractor
  //LINALG::MapExtractor mapext(*mergedmap,standrowmap,windkrowmap);

  std::vector<Teuchos::RCP<const Epetra_Map> > myMaps;
  myMaps.push_back(standrowmap);
  myMaps.push_back(windkrowmap);
  LINALG::MultiMapExtractor mapext(*mergedmap, myMaps);

  // initialize large SparseMatrix and Epetra_Vectors
  RCP<LINALG::SparseMatrix> mergedmatrix = Teuchos::rcp(new LINALG::SparseMatrix(*mergedmap,81));
  RCP<Epetra_Vector> mergedrhs = Teuchos::rcp(new Epetra_Vector(*mergedmap));
  RCP<Epetra_Vector> mergedsol = Teuchos::rcp(new Epetra_Vector(*mergedmap));
  // ONLY compatability
  // dirichtoggle_ changed and we need to rebuild associated DBC maps
  if (dirichtoggle_ != Teuchos::null)
    dbcmaps_ = LINALG::ConvertDirichletToggleVectorToMaps(dirichtoggle_);

  // use BlockMatrix
  Teuchos::RCP<LINALG::BlockSparseMatrix<LINALG::DefaultBlockMatrixStrategy> > blockmat = Teuchos::rcp(new LINALG::BlockSparseMatrix<LINALG::DefaultBlockMatrixStrategy>(mapext,mapext,81,false,false));
  blockmat->Assign(0,0,View,*stiff);
  blockmat->Assign(1,0,View,*coupoffdiag_vol_d->Transpose());
  blockmat->Assign(0,1,View,*coupoffdiag_fext_p);
  blockmat->Assign(1,1,View,*windkstiff);
  blockmat->Complete();

  // merge into one, fill merged matrix using Add
  mergedmatrix -> Add(*stiff,false,1.0,1.0);
  mergedmatrix -> Add(*coupoffdiag_vol_d,true,1.0,1.0);
  mergedmatrix -> Add(*coupoffdiag_fext_p,false,1.0,1.0);
  mergedmatrix -> Add(*windkstiff,false,1.0,1.0);
  mergedmatrix -> Complete(*mergedmap,*mergedmap);

  // fill merged vectors using Export
  LINALG::Export(*rhswindk,*mergedrhs);
  mergedrhs -> Scale(-1.0);
  LINALG::Export(*rhsstand,*mergedrhs);

#if 0
    const int myrank=(actdisc_->Comm().MyPID());
    const double cond_number = LINALG::Condest(static_cast<LINALG::SparseMatrix&>(*mergedmatrix),Ifpack_GMRES, 100);
    // computation of significant digits might be completely bogus, so don't take it serious
    const double tmp = std::abs(std::log10(cond_number*1.11022e-16));
    const int sign_digits = (int)floor(tmp);
    if (!myrank)
      std::cout << " cond est: " << std::scientific << cond_number << ", max.sign.digits: " << sign_digits<<std::endl;
#endif

  // solve with merged matrix
  //solver_->Solve(mergedmatrix->EpetraMatrix(),mergedsol,mergedrhs,true,counter_==0);
  // solve with Block matrix
  solver_->Solve(blockmat,mergedsol,mergedrhs,true,counter_==0);
  solver_->ResetTolerance();

  // store results in smaller vectors
  mapext.ExtractVector(mergedsol,0,dispinc);
  mapext.ExtractVector(mergedsol,1,presincr);

  counter_++;

	// update Windkessel pressure
	UpdatePres(presincr);

  return;
}

/*----------------------------------------------------------------------*/
/*!
 \file poro_base.cpp

 \brief  Basis of all porous media algorithms

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15264
 </pre>
 *-----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 | definitions                                                          |
 *----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 | headers                                                  vuong 01/12 |
 *----------------------------------------------------------------------*/
#include "poro_base.H"
#include "poroelast_defines.H"

#include "../drt_adapter/adapter_coupling.H"
#include "../drt_adapter/ad_fld_base_algorithm.H"


#include "../linalg/linalg_utils.H"
#include "../linalg/linalg_solver.H"
#include "../linalg/linalg_mapextractor.H"
#include "../linalg/linalg_sparsematrix.H"

#include "../drt_inpar/inpar_solver.H"

#include <Teuchos_TimeMonitor.hpp>
// needed for PrintNewton
#include <sstream>

// include this header for coupling stiffness terms
#include "../drt_lib/drt_assemblestrategy.H"

#include "../drt_lib/drt_utils.H"

#include "../drt_io/io_control.H"
#include "../drt_io/io_pstream.H"
#include "../drt_lib/drt_condition_utils.H"
#include "../drt_lib/drt_globalproblem.H"

#include "../drt_fluid_ele/fluid_ele.H"
#include "../drt_adapter/ad_fld_poro.H"

#include "../drt_adapter/ad_str_fsiwrapper.H"
#include "../drt_structure/stru_aux.H"

/*----------------------------------------------------------------------*
 | constructor (public)                                    vuong 01/12  |
 *----------------------------------------------------------------------*/
POROELAST::PoroBase::PoroBase(const Epetra_Comm& comm,
                              const Teuchos::ParameterList& timeparams) :
      AlgorithmBase(comm, timeparams)
{
  // access the structural discretization
  Teuchos::RCP<DRT::Discretization> structdis = DRT::Problem::Instance()->GetDis("structure");
  // access structural dynamic params list which will be possibly modified while creating the time integrator
  const Teuchos::ParameterList& sdyn = DRT::Problem::Instance()->StructuralDynamicParams();

  // ask base algorithm for the structural time integrator
  Teuchos::RCP<ADAPTER::StructureBaseAlgorithm> structure =
      Teuchos::rcp(new ADAPTER::StructureBaseAlgorithm(timeparams, const_cast<Teuchos::ParameterList&>(sdyn), structdis));
  structure_ = Teuchos::rcp_dynamic_cast<ADAPTER::FSIStructureWrapper>(structure->StructureFieldrcp());

  if(structure_ == Teuchos::null)
    dserror("cast from ADAPTER::Structure to ADAPTER::FSIStructureWrapper failed");

  // ask base algorithm for the fluid time integrator
  Teuchos::RCP<ADAPTER::FluidBaseAlgorithm> fluid =
      Teuchos::rcp(new ADAPTER::FluidBaseAlgorithm(timeparams,true));
  fluid_ = Teuchos::rcp_dynamic_cast<ADAPTER::FluidPoro>(fluid->FluidFieldrcp());

  if(fluid_ == Teuchos::null)
    dserror("cast from ADAPTER::FluidBaseAlgorithm to ADAPTER::FluidPoro failed");

  //as this is a two way coupled problem, every discretization needs to know the other one.
  // For this we use DofSetProxies and coupling objects which are setup here
  SetupProxiesAndCoupling();

  //extractor for constraints on structure phase
  //
  // when using constraints applied via Lagrange-Multipliers there is a
  // difference between StructureField()->DofRowMap() and StructureField()->DofRowMap(0).
  // StructureField()->DofRowMap(0) returns the DofRowMap
  // known to the discretization (without lagrange multipliers)
  // while StructureField()->DofRowMap() returns the DofRowMap known to
  // the constraint manager (with lagrange multipliers)
  consplitter_ = Teuchos::rcp(new LINALG::MapExtractor(*StructureField()->DofRowMap(),
      StructureField()->DofRowMap(0)));

  //look for special poro conditions and set flags
  CheckForPoroConditions();

  //do some checks
  {
    // access the problem-specific parameter lists
    const Teuchos::ParameterList& fdyn = DRT::Problem::Instance()->FluidDynamicParams();

    std::vector<DRT::Condition*> porocoupl;
    FluidField()->Discretization()->GetCondition("PoroCoupling", porocoupl);
    if ( porocoupl.size() == 0 )
      dserror("no Poro Coupling Condition defined for porous media problem. Fix your input file!");

    // check time integration algo -> currently only one-step-theta scheme supported
    INPAR::STR::DynamicType structtimealgo
    = DRT::INPUT::IntegralValue<INPAR::STR::DynamicType>(sdyn,"DYNAMICTYP");
    INPAR::FLUID::TimeIntegrationScheme fluidtimealgo
    = DRT::INPUT::IntegralValue<INPAR::FLUID::TimeIntegrationScheme>(fdyn,"TIMEINTEGR");

    if ( not (
                   (structtimealgo == INPAR::STR::dyna_onesteptheta and fluidtimealgo == INPAR::FLUID::timeint_one_step_theta)
               or (structtimealgo == INPAR::STR::dyna_statics and fluidtimealgo == INPAR::FLUID::timeint_stationary)
             )
       )
      dserror("porous media problem is limited in functionality (only one-step-theta scheme and stationary case possible)");

    if(structtimealgo == INPAR::STR::dyna_onesteptheta and fluidtimealgo == INPAR::FLUID::timeint_one_step_theta)
    {
      double theta_struct = sdyn.sublist("ONESTEPTHETA").get<double>("THETA");
      double theta_fluid  = fdyn.get<double>("THETA");

      if(theta_struct != theta_fluid)
        dserror("porous media problem is limited in functionality. Only one-step-theta scheme with equal theta for both fields possible");
    }
  }
}

/*----------------------------------------------------------------------*
 | destructor (public)                                    vuong 01/12   |
 *----------------------------------------------------------------------*/
POROELAST::PoroBase::~PoroBase()
{
}

/*----------------------------------------------------------------------*
 | read restart information for given time step (public)   vuong 01/12  |
 *----------------------------------------------------------------------*/
void POROELAST::PoroBase::ReadRestart( int restart)
{
  if (restart)
  {
    // apply current velocity and pressures to structure
    SetFluidSolution();
    // apply current structural displacements to fluid
    SetStructSolution();

    FluidField()->ReadRestart(restart);
    StructureField()->ReadRestart(restart);

    //in case of submeshes, we need to rebuild the subproxies, also (they are reset during restart)
    if(submeshes_)
      AddDofSets(true);

    // apply current velocity and pressures to structure
    SetFluidSolution();
    // apply current structural displacements to fluid
    SetStructSolution();

    // second ReadRestart needed due to the coupling variables
    FluidField()->ReadRestart(restart);
    StructureField()->ReadRestart(restart);

    //in case of submeshes, we need to rebuild the subproxies, also (they are reset during restart)
    if(submeshes_)
      AddDofSets(true);

    SetTimeStep(FluidField()->Time(), restart);
  }

  return;
}

/*----------------------------------------------------------------------*
 | prepare time step (protected)                         vuong 01/12       |
 *----------------------------------------------------------------------*/
void POROELAST::PoroBase::PrepareTimeStep()
{
  // counter and print header
  IncrementTimeAndStep();
  PrintHeader();

  //set fluid velocities and pressures onto the structure
  SetFluidSolution();

  // call the predictor
  StructureField()-> PrepareTimeStep();

  //set structure displacements onto the fluid
  SetStructSolution();

  // call the predictor
  FluidField()->PrepareTimeStep();
}

/*----------------------------------------------------------------------*
 | update (protected)                                     vuong 01/12   |
 *----------------------------------------------------------------------*/
void POROELAST::PoroBase::Update()
{
  StructureField()->Update();
  FluidField()->Update();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void POROELAST::PoroBase::PrepareOutput()
{
  StructureField()->PrepareOutput();
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void POROELAST::PoroBase::TestResults(const Epetra_Comm& comm)
{
  DRT::Problem::Instance()->AddFieldTest(StructureField()->CreateFieldTest());
  DRT::Problem::Instance()->AddFieldTest(FluidField()->CreateFieldTest());
  DRT::Problem::Instance()->TestAll(comm);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> POROELAST::PoroBase::StructureToFluidField(
    Teuchos::RCP<const Epetra_Vector> iv) const
{
  if(submeshes_ )
    return coupfs_->MasterToSlave(psiextractor_->ExtractCondVector(iv));
  else
    return coupfs_->MasterToSlave(iv);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> POROELAST::PoroBase::FluidToStructureField(
    Teuchos::RCP<const Epetra_Vector> iv) const
{
  if(submeshes_)
    return coupfs_->SlaveToMaster(psiextractor_->ExtractCondVector(iv));
  else
    return coupfs_->SlaveToMaster(iv);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::PoroBase::SetStructSolution()
{
  Teuchos::RCP<Epetra_Vector> dispnp;
    // apply current displacements and velocities to the fluid field
  if (StructureField()->HaveConstraint())
    //displacement vector without lagrange-multipliers
    dispnp = consplitter_->ExtractCondVector(StructureField()->Dispnp());
  else
    dispnp = StructureField()->ExtractDispnp();

  Teuchos::RCP<Epetra_Vector> velnp = StructureField()->ExtractVelnp();

  // transfer the current structure displacement to the fluid field
  Teuchos::RCP<Epetra_Vector> structdisp = StructureToFluidField(dispnp);
  FluidField()->ApplyMeshDisplacement(structdisp);

  // transfer the current structure velocity to the fluid field
  Teuchos::RCP<Epetra_Vector> structvel = StructureToFluidField(velnp);
  FluidField()->ApplyMeshVelocity(structvel);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::PoroBase::SetFluidSolution()
{
  StructureField()->ApplyCouplingState(FluidField()->Velnp(),"fluidvel");
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::PoroBase::TimeLoop()
{

  while (NotFinished())
  {
    //solve one time step
    DoTimeStep();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void POROELAST::PoroBase::Output()
{
  // Note: The order is important here! In here control file entries are
  // written. And these entries define the order in which the filters handle
  // the Discretizations, which in turn defines the dof number ordering of the
  // Discretizations.
  FluidField()->Output();
  StructureField()->Output();
} // Monolithic::Output()

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void POROELAST::PoroBase::SetupProxiesAndCoupling()
{

  //get discretizations
  Teuchos::RCP<DRT::Discretization> structdis = StructureField()->Discretization();
  Teuchos::RCP<DRT::Discretization> fluiddis = FluidField()->Discretization();

  // if one discretization is a subset of the other, they will differ in node number (and element number)
  // we assume matching grids for the overlapping part here
  const Epetra_Map* structnodecolmap = structdis->NodeColMap();
  const Epetra_Map* fluidnodecolmap = fluiddis->NodeColMap();

  const int numglobalstructnodes = structnodecolmap->NumGlobalElements();
  const int numglobalfluidnodes = fluidnodecolmap->NumGlobalElements();

  //check for submeshes
  if(numglobalstructnodes != numglobalfluidnodes)
    submeshes_ = true;
  else
    submeshes_ = false;

  // add dof set of structure/fluid discretization to fluid/structure discretization
  AddDofSets();

  const int ndim = DRT::Problem::Instance()->NDim();
  const int numglobalstructdofs = structdis->DofColMap()->NumGlobalElements();
  if(numglobalstructdofs == numglobalstructnodes * ndim)
    porositydof_ = false;
  else
  {
    porositydof_ = true;
    porositysplitter_ = POROELAST::UTILS::BuildPoroSplitter(StructureField()->Discretization());
  }

  // the fluid-structure coupling not always matches
  const Epetra_Map* fluidnoderowmap = fluiddis->NodeRowMap();
  const Epetra_Map* structurenoderowmap= structdis->NodeRowMap();

  coupfs_ = Teuchos::rcp(new ADAPTER::Coupling());
  int ndof = ndim;

  if(porositydof_) ndof++;

  if(submeshes_)
    //for submeshes we only couple a part of the structure disc. with the fluid disc.
    // we use the fact, that we have matching grids and matching gids
    coupfs_->SetupCoupling(*structdis,
                           *fluiddis,
                           *fluidnoderowmap,
                           *fluidnoderowmap,
                           ndof,
                            not submeshes_);
  else
    coupfs_->SetupCoupling(*structdis,
                           *fluiddis,
                           *structurenoderowmap,
                           *fluidnoderowmap,
                           ndof,
                            not submeshes_);

  if(submeshes_)
    psiextractor_ = Teuchos::rcp(new LINALG::MapExtractor(*StructureField()->DofRowMap(), coupfs_->MasterDofMap()));

  FluidField()->SetMeshMap(coupfs_->SlaveDofMap());

  //p1splitter_ = Teuchos::rcp(new LINALG::MapExtractor( *StructureField()->DofRowMap(),coupfs_->MasterDofMap() ));
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void POROELAST::PoroBase::AddDofSets(bool replace)
{
  // the problem is two way coupled, thus each discretization must know the other discretization
  Teuchos::RCP<DRT::DofSet> structdofset = Teuchos::null;
  Teuchos::RCP<DRT::DofSet> fluiddofset = Teuchos::null;

  //get discretizations
  Teuchos::RCP<DRT::Discretization> structdis = StructureField()->Discretization();
  Teuchos::RCP<DRT::Discretization> fluiddis = FluidField()->Discretization();

  /* When coupling porous media with a pure structure we will have two discretizations
   * of different size. In this case we need a special proxy, which can handle submeshes.
   */
  if(submeshes_)
  {
    // build a proxy of the structure discretization for the fluid field (the structure disc. is the bigger one)
    structdofset = structdis->GetDofSetProxy(structdis->NodeColMap(),structdis->ElementColMap());
    // build a proxy of the fluid discretization for the structure field
    fluiddofset = fluiddis->GetDofSetProxy(fluiddis->NodeColMap(),fluiddis->ElementColMap());
  }
  else
  {
    // build a proxy of the structure discretization for the fluid field
    structdofset = structdis->GetDofSetProxy();
    // build a proxy of the fluid discretization for the structure field
    fluiddofset = fluiddis->GetDofSetProxy();
  }

  if(not replace)
  {
    // check if FluidField has 2 discretizations, so that coupling is possible
    if (fluiddis->AddDofSet(structdofset) != 1)
      dserror("unexpected dof sets in fluid field");
    if (structdis->AddDofSet(fluiddofset)!=1)
      dserror("unexpected dof sets in structure field");
  }
  else
  {
    fluiddis->ReplaceDofSet(1,structdofset);
    structdis->ReplaceDofSet(1,fluiddofset);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void POROELAST::PoroBase::CheckForPoroConditions()
{
  std::vector<DRT::Condition*> nopencond;
  FluidField()->Discretization()->GetCondition("NoPenetration", nopencond);
  noPenHandle_ = Teuchos::rcp(new POROELAST::NoPenetrationConditionHandle(nopencond));

  partincond_=false;
  std::vector<DRT::Condition*> poroPartInt;
  FluidField()->Discretization()->GetCondition("PoroPartInt",poroPartInt);
  if(poroPartInt.size())
    partincond_=true;

  presintcond_=false;
  std::vector<DRT::Condition*> poroPresInt;
  FluidField()->Discretization()->GetCondition("PoroPresInt",poroPresInt);
  if(poroPresInt.size())
    presintcond_=true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void POROELAST::PoroBase::CalculateSurfPoro(const string& condstring)
{
  //check if the condition exists
  std::vector<DRT::Condition*> surfporo;
  FluidField()->Discretization()->GetCondition(condstring, surfporo);

  if(surfporo.size())
  {
    //-------------------------------
    // create the parameters for the discretization
    Teuchos::ParameterList p;
    // action for elements
    p.set("action", "calc_struct_area_poro");
    // other parameters that might be needed by the elements
    p.set("total time", Time());
    p.set("delta time", Dt());

    Teuchos::RCP<DRT::Discretization> structdis = StructureField()->Discretization();
    Teuchos::RCP<DRT::Discretization> fluiddis = FluidField()->Discretization();

    // set vector values needed by elements
    structdis->ClearState();
    // extended SetState(0,...) in case of multiple dofsets
    structdis->SetState(0,"displacement", StructureField()->Dispnp());

    structdis->SetState(1,"fluidvel",FluidField()->Velnp());

    structdis->EvaluateCondition(p,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,Teuchos::null,condstring);
    structdis->ClearState();
  }
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::NoPenetrationConditionHandle::BuidNoPenetrationMap(const Epetra_Comm& comm, Teuchos::RCP<const Epetra_Map> dofRowMap)
{
  std::vector<int> condIDs;
  std::set<int>::iterator it;
  for(it=condIDs_->begin();it!=condIDs_->end();it++)
  {
    condIDs.push_back(*it);
  }
  Teuchos::RCP<Epetra_Map> nopendofmap = Teuchos::rcp(new Epetra_Map(-1, condIDs.size(), &condIDs[0], 0, comm));

  nopenetration_ = Teuchos::rcp(new LINALG::MapExtractor(*dofRowMap, nopendofmap));

  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::NoPenetrationConditionHandle::ApplyCondRHS( Teuchos::RCP<Epetra_Vector> iterinc,
                                                            Teuchos::RCP<Epetra_Vector> rhs)
{
  if(hascond_)
  {
    const Teuchos::RCP<const Epetra_Map >& nopenetrationmap = nopenetration_->Map(1);
    LINALG::ApplyDirichlettoSystem(iterinc,rhs,condRHS_,*nopenetrationmap);
  }

  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::NoPenetrationConditionHandle::Clear(POROELAST::coupltype coupltype)
{
  if(hascond_)
  {
    condRHS_->PutScalar(0.0);
    //condVector_->PutScalar(0.0);
    condIDs_->clear();
    switch(coupltype)
    {
    case POROELAST::fluidfluid:
      fluidfluidConstraintMatrix_->Zero();
      condVector_->PutScalar(0.0);
      break;
    case POROELAST::fluidstructure:
      fluidstructureConstraintMatrix_->Zero();
      structVelConstraintMatrix_->Zero();
      break;
    default:
      condVector_->PutScalar(0.0);
      fluidfluidConstraintMatrix_->Zero();
      fluidstructureConstraintMatrix_->Zero();
      structVelConstraintMatrix_->Zero();
      break;
    }
  }
  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void POROELAST::NoPenetrationConditionHandle::Setup(Teuchos::RCP<const Epetra_Map> dofRowMap, const Epetra_Map* dofRowMapFluid)
{
  if(hascond_)
  {
    condRHS_ = Teuchos::rcp(new Epetra_Vector(*dofRowMap, true));

    condVector_ = Teuchos::rcp(new Epetra_Vector(*dofRowMapFluid, true));

    fluidfluidConstraintMatrix_ = Teuchos::rcp(new LINALG::SparseMatrix(
                                                              *dofRowMapFluid,
                                                              81,
                                                              true,
                                                              true)
                                    );

    fluidstructureConstraintMatrix_ = Teuchos::rcp(new LINALG::SparseMatrix(
                                                              *dofRowMapFluid,
                                                              81,
                                                              true,
                                                              true)
                                    );

    structVelConstraintMatrix_ = Teuchos::rcp(new LINALG::SparseMatrix(
                                                                      *dofRowMapFluid,
                                                                      81,
                                                                      true,
                                                                      true)
                                             );
  }
  return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<LINALG::SparseMatrix> POROELAST::NoPenetrationConditionHandle::ConstraintMatrix(POROELAST::coupltype coupltype)
{
  if(hascond_)
  {
    if(coupltype == POROELAST::fluidfluid)
      return fluidfluidConstraintMatrix_;
    else if(coupltype == POROELAST::fluidstructure)
      return fluidstructureConstraintMatrix_;
  }
  return Teuchos::null;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<LINALG::SparseMatrix> POROELAST::NoPenetrationConditionHandle::StructVelConstraintMatrix(POROELAST::coupltype coupltype)
{
  if(hascond_)
  {
    if(coupltype == POROELAST::fluidfluid)
      return Teuchos::null;
    else if(coupltype == POROELAST::fluidstructure)
      return structVelConstraintMatrix_;
  }
  return Teuchos::null;
}


/*----------------------------------------------------------------------*/
/*!
\file adapter_fluid_impl.cpp

\brief Fluid field adapter

<pre>
Maintainer: Ulrich Kuettler
            kuettler@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15238
</pre>
*/
/*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include "adapter_fluid_impl.H"
#include "../linalg/linalg_utils.H"
#include "../drt_fluid/fluidresulttest.H"


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
ADAPTER::FluidImpl::FluidImpl(
        Teuchos::RCP<DRT::Discretization> dis,
        Teuchos::RCP<LINALG::Solver> solver,
        Teuchos::RCP<ParameterList> params,
        Teuchos::RCP<IO::DiscretizationWriter> output,
        bool isale,
        bool dirichletcond)
  : fluid_(dis, *solver, *params, *output, isale),
    dis_(dis),
    solver_(solver),
    params_(params),
    output_(output)
{
  interface_.Setup(*dis);
  fluid_.SetSurfaceSplitter(&interface_);

  // build inner velocity map
  // dofs at the interface are excluded
  // we use only velocity dofs and only those without Dirichlet constraint
  const Teuchos::RCP<const LINALG::MapExtractor> dbcmaps = fluid_.DirichMaps();
  std::vector<Teuchos::RCP<const Epetra_Map> > maps;
  maps.push_back(interface_.OtherMap());
  maps.push_back(dbcmaps->OtherMap());
  innervelmap_ = LINALG::MultiMapExtractor::IntersectMaps(maps);

  if (dirichletcond)
  {
    // mark all interface velocities as dirichlet values
    fluid_.AddDirichCond(interface_.FSICondMap());
  }

  interfaceforcen_ = rcp(new Epetra_Vector(*(interface_.FSICondMap())));
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::InitialGuess()
{
  return fluid_.InitialGuess();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::RHS()
{
  return fluid_.Residual();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::TrueResidual()
{
  return fluid_.TrueResidual();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Velnp()
{
  return fluid_.Velnp();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Velaf()
{
  return fluid_.Velaf();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Veln()
{
  return fluid_.Veln();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Velnm()
{
  return fluid_.Velnm();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Accnp()
{
  return fluid_.Accnp();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Accn()
{
  return fluid_.Accn();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Accam()
{
  return fluid_.Accam();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Scaaf()
{
  return fluid_.Scaaf();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Scaam()
{
  return fluid_.Scaam();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Hist()
{
  return fluid_.Hist();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::Dispnp()
{
  return fluid_.Dispnp();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::ConvectiveVel()
{
  if (fluid_.GridVel() == Teuchos::null)
    return fluid_.Velnp(); // no moving mesh present
  else
  {
    // make an intermediate copy of velnp
    Teuchos::RCP<Epetra_Vector> convel = rcp(new Epetra_Vector(*(fluid_.Velnp())));
    // now subtract the grid velocity
    convel->Update(-1.0,*(fluid_.GridVel()),1.0);

    return convel;
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::FsVel()
{
  return fluid_.FsVel();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> ADAPTER::FluidImpl::DofRowMap()
{
  const Epetra_Map* dofrowmap = dis_->DofRowMap();
  return Teuchos::rcp(dofrowmap, false);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> ADAPTER::FluidImpl::DofRowMap(unsigned nds)
{
  const Epetra_Map* dofrowmap = dis_->DofRowMap(nds);
  return Teuchos::rcp(dofrowmap, false);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<LINALG::SparseMatrix> ADAPTER::FluidImpl::SystemMatrix()
{
  return fluid_.SystemMatrix();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<LINALG::BlockSparseMatrixBase> ADAPTER::FluidImpl::BlockSystemMatrix()
{
  return fluid_.BlockSystemMatrix();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<LINALG::BlockSparseMatrixBase> ADAPTER::FluidImpl::ShapeDerivatives()
{
  return fluid_.ShapeDerivatives();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<DRT::Discretization> ADAPTER::FluidImpl::Discretization()
{
  return fluid_.Discretization();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const LINALG::MapExtractor> ADAPTER::FluidImpl::GetDBCMapExtractor()
{
  return fluid_.DirichMaps();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
// Teuchos::RCP<Epetra_Vector> ADAPTER::FluidImpl::StructCondRHS() const
// {
//   return interface_.ExtractCondVector(Velnp());
// }


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::TimeLoop()
{
   fluid_.Integrate();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::PrepareTimeStep()
{
  fluid_.PrepareTimeStep();

  // we add the whole fluid mesh displacement later on?
  //fluid_.Dispnp()->PutScalar(0.);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::PrepareTimeandStep()
{
  fluid_.UpdateStepandTime();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::PrepareSolve()
{
  fluid_.PrepareSolve();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::Evaluate(Teuchos::RCP<const Epetra_Vector> stepinc)
{
  if (stepinc!=Teuchos::null)
  {
    fluid_.Evaluate(stepinc);
  }
  else
  {
    fluid_.Evaluate(Teuchos::null);
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool ADAPTER::FluidImpl::ConvergenceCheck(int itnum, int itmax, const double ittol)
{
  return fluid_.ConvergenceCheck(itnum, itmax, ittol);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::IterUpdate(const Teuchos::RCP<const Epetra_Vector> increment)
{
  fluid_.IterUpdate(increment);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::Update()
{
  Teuchos::RCP<Epetra_Vector> interfaceforcem = interface_.ExtractFSICondVector(fluid_.TrueResidual());

  interfaceforcen_ = fluid_.ExtrapolateEndPoint(interfaceforcen_,interfaceforcem);

  fluid_.TimeUpdate();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::StatisticsAndOutput()
{
  fluid_.StatisticsAndOutput();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::Output()
{
  fluid_.Output();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::StatisticsOutput()
{
  fluid_.StatisticsOutput();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::NonlinearSolve()
{
  fluid_.NonlinearSolve();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::Predictor()
{
  fluid_.Predictor();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::MultiCorrector()
{
  fluid_.MultiCorrector();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> ADAPTER::FluidImpl::InnerVelocityRowMap()
{
  return innervelmap_;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> ADAPTER::FluidImpl::VelocityRowMap()
{
  return fluid_.VelocityRowMap();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> ADAPTER::FluidImpl::PressureRowMap()
{
  return fluid_.PressureRowMap();
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::SetMeshMap(Teuchos::RCP<const Epetra_Map> mm)
{
  meshmap_.Setup(*dis_->DofRowMap(),mm,LINALG::SplitMap(*dis_->DofRowMap(),*mm));
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
double ADAPTER::FluidImpl::ResidualScaling() const
{
  return fluid_.ResidualScaling();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
double ADAPTER::FluidImpl::TimeScaling() const
{
  if (params_->get<bool>("interface second order"))
  {
    return 2./fluid_.Dt();
  }
  else
    return 1./fluid_.Dt();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::ReadRestart(int step)
{
  fluid_.ReadRestart(step);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::SetRestart(const int step,
                const double time,
                Teuchos::RCP<const Epetra_Vector> readvelnp,
                Teuchos::RCP<const Epetra_Vector> readveln,
                Teuchos::RCP<const Epetra_Vector> readvelnm,
                Teuchos::RCP<const Epetra_Vector> readaccnp,
                Teuchos::RCP<const Epetra_Vector> readaccn)
{
  fluid_.SetRestart(step,time,readvelnp,readveln,readvelnm,readaccnp,readaccn);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
double ADAPTER::FluidImpl::Time() const
{
  return fluid_.Time();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
int ADAPTER::FluidImpl::Step() const
{
  return fluid_.Step();
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
double ADAPTER::FluidImpl::Dt() const
{
  return fluid_.Dt();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::LiftDrag()
{
  fluid_.LiftDrag();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidImpl::ExtractInterfaceForces()
{
  Teuchos::RCP<Epetra_Vector> interfaceforcem = interface_.ExtractFSICondVector(fluid_.TrueResidual());

  return fluid_.ExtrapolateEndPoint(interfaceforcen_,interfaceforcem);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidImpl::ExtractInterfaceForcesRobin()
{
  // Calculate interface force from (externally applied) Robin force and
  // velocity. This assumes the fluid solve results in
  //
  // f_int - alpha_f*u(n+1) + f_robin = 0
  //
  // where f_robin consists of structural interface force and
  // displacement. The point here is to notice non-matching interface
  // displacements in the force vector, so that a testing of interface forces
  // is sufficient as convergence check.

  Teuchos::RCP<Epetra_Vector> robinforce = interface_.ExtractFSICondVector(fluid_.RobinRHS());
  double alphaf = params_->get<double>("alpharobinf",-1.);
  Teuchos::RCP<Epetra_Vector> ivelnp = interface_.ExtractFSICondVector(fluid_.Velnp());

  robinforce->Update(alphaf,*ivelnp,-1.0);

  return robinforce;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidImpl::ExtractInterfaceFluidVelocity()
{
  return interface_.ExtractFSICondVector(fluid_.Velnp());
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidImpl::ExtractInterfaceVeln()
{
  return interface_.ExtractFSICondVector(fluid_.Veln());
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidImpl::ExtractFreeSurfaceVeln()
{
  return Interface().ExtractFSCondVector(fluid_.Veln());
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::ApplyInterfaceVelocities(Teuchos::RCP<Epetra_Vector> ivel)
{
  interface_.InsertFSICondVector(ivel,fluid_.Velnp());
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::ApplyInterfaceRobinValue(Teuchos::RCP<Epetra_Vector> ivel,
                                                  Teuchos::RCP<Epetra_Vector> iforce)
{
  // use the known parts of structure field to create the robin
  // boundary value
  // the robin boundary value consists of a linear combination of
  // interface velocity and interface forces:

  // Robin-RHS = alpha_f * structural interface velocity
  //             - interface force (form structure to fluid)

  // get linear combination parameter
  double alphaf = params_->get<double>("alpharobinf",-1.);
  if (alphaf<0) dserror("falscher alpharobinf-Parameter");

  // robinboundaryvalue vorerst nur interfacegeschwindigkeit
  Teuchos::RCP<Epetra_Vector> robinboundaryvalue = Teuchos::rcp(new Epetra_Vector(*ivel));

  // at the moment iforce is the force to the structure, we have to
  // multiply with -1
  robinboundaryvalue->Update(-1.,*iforce,alphaf);

  // apply robin values to fluid equations RobinRHS vector
  interface_.InsertFSICondVector(robinboundaryvalue,fluid_.RobinRHS());

  // at this point we have to omit the setting of dirichlet values at
  // the interface
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::ApplyMeshDisplacement(Teuchos::RCP<const Epetra_Vector> fluiddisp)
{
  meshmap_.InsertCondVector(fluiddisp,fluid_.Dispnp());

  // new grid velocity
  fluid_.UpdateGridv();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::ApplyMeshVelocity(Teuchos::RCP<const Epetra_Vector> gridvel)
{
  meshmap_.InsertCondVector(gridvel,fluid_.GridVel());
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::DisplacementToVelocity(Teuchos::RCP<Epetra_Vector> fcx)
{
  // get interface velocity at t(n)
  const Teuchos::RCP<Epetra_Vector> veln = Interface().ExtractFSICondVector(Veln());

  // We convert Delta d(n+1,i+1) to Delta u(n+1,i+1) here.
  //
  // Delta d(n+1,i+1) = ( theta Delta u(n+1,i+1) + u(n) ) * dt
  //
  double timescale = TimeScaling();
  fcx->Update(-timescale*fluid_.Dt(),*veln,timescale);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::VelocityToDisplacement(Teuchos::RCP<Epetra_Vector> fcx)
{
  // get interface velocity at t(n)
  const Teuchos::RCP<Epetra_Vector> veln = Interface().ExtractFSICondVector(Veln());

  // We convert Delta u(n+1,i+1) to Delta d(n+1,i+1) here.
  //
  // Delta d(n+1,i+1) = ( theta Delta u(n+1,i+1) + u(n) ) * dt
  //
  double timescale = 1./TimeScaling();
  fcx->Update(fluid_.Dt(),*veln,timescale);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::FreeSurfDisplacementToVelocity(Teuchos::RCP<Epetra_Vector> fcx)
{
  // get interface velocity at t(n)
  const Teuchos::RCP<Epetra_Vector> veln = Interface().ExtractFSCondVector(Veln());

  // We convert Delta d(n+1,i+1) to Delta u(n+1,i+1) here.
  //
  // Delta d(n+1,i+1) = ( theta Delta u(n+1,i+1) + u(n) ) * dt
  //
  double timescale = TimeScaling();
  fcx->Update(-timescale*fluid_.Dt(),*veln,timescale);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::FreeSurfVelocityToDisplacement(Teuchos::RCP<Epetra_Vector> fcx)
{
  // get interface velocity at t(n)
  const Teuchos::RCP<Epetra_Vector> veln = Interface().ExtractFSCondVector(Veln());

  // We convert Delta u(n+1,i+1) to Delta d(n+1,i+1) here.
  //
  // Delta d(n+1,i+1) = ( theta Delta u(n+1,i+1) + u(n) ) * dt
  //
  double timescale = 1./TimeScaling();
  fcx->Update(fluid_.Dt(),*veln,timescale);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
int ADAPTER::FluidImpl::Itemax() const
{
  return fluid_.Itemax();
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::SetItemax(int itemax)
{
  fluid_.SetItemax(itemax);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidImpl::IntegrateInterfaceShape()
{
  return interface_.ExtractFSICondVector(fluid_.IntegrateInterfaceShape("FSICoupling"));
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::UseBlockMatrix(bool splitmatrix)
{
  Teuchos::RCP<std::set<int> > condelements = Interface().ConditionedElementMap(*Discretization());
  fluid_.UseBlockMatrix(condelements,Interface(),Interface(),splitmatrix);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> ADAPTER::FluidImpl::RelaxationSolve(Teuchos::RCP<Epetra_Vector> ivel)
{
  const Epetra_Map* dofrowmap = Discretization()->DofRowMap();
  Teuchos::RCP<Epetra_Vector> relax = LINALG::CreateVector(*dofrowmap,true);
  interface_.InsertFSICondVector(ivel,relax);
  fluid_.LinearRelaxationSolve(relax);
  return ExtractInterfaceForces();
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<DRT::ResultTest> ADAPTER::FluidImpl::CreateFieldTest()
{
  return Teuchos::rcp(new FLD::FluidResultTest(fluid_));
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> ADAPTER::FluidImpl::ExtractVelocityPart(Teuchos::RCP<const Epetra_Vector> velpres)
{
   return (fluid_.VelPresSplitter()).ExtractOtherVector(velpres);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::SetInitialFlowField(const INPAR::FLUID::InitialField initfield,const int startfuncno)
{
   fluid_.SetInitialFlowField(initfield,startfuncno);
   return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::SetIterLomaFields(
    RCP<const Epetra_Vector>          scalaraf,
    RCP<const Epetra_Vector>          scalaram,
    RCP<const Epetra_Vector>          scalardtam,
    const double                      thermpressaf,
    const double                      thermpressam,
    const double                      thermpressdtaf,
    const double                      thermpressdtam,
    Teuchos::RCP<DRT::Discretization> scatradis)
{
   fluid_.SetIterLomaFields(scalaraf,scalaram,scalardtam,thermpressaf,thermpressam,thermpressdtaf,thermpressdtam,scatradis);
   return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::SetTimeLomaFields(
    RCP<const Epetra_Vector> scalarnp,
    const double thermpressnp,
    RCP<const Epetra_Vector> scatraresidual,
    Teuchos::RCP<DRT::Discretization> scatradis,
    const int whichscalar)
{
   fluid_.SetTimeLomaFields(scalarnp,thermpressnp,scatraresidual,scatradis, whichscalar);
   return;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void ADAPTER::FluidImpl::SetVelocityField(
    Teuchos::RCP<const Epetra_Vector> velnp)
{
  fluid_.SetVelocityField(velnp);
  return;
}

#endif  // #ifdef CCADISCRET

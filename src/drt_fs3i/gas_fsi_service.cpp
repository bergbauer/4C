#ifdef CCADISCRET

#include <Teuchos_TimeMonitor.hpp>

#include "../drt_fsi/fsi_dyn.H"
#include "../drt_fsi/fs_monolithic.H"
#include "../drt_fsi/fsi_monolithicfluidsplit.H"
#include "../drt_fsi/fsi_monolithiclagrange.H"
#include "../drt_fsi/fsi_monolithicstructuresplit.H"
#include "../drt_fsi/fsi_utils.H"

#include "../drt_lib/drt_condition_selector.H"
#include "../drt_lib/drt_condition_utils.H"

#include "../drt_lib/drt_globalproblem.H"
#include "../drt_inpar/drt_validparameters.H"

#include "../drt_scatra/scatra_utils.H"

#include "../drt_lib/drt_condition_utils.H"

#ifdef PARALLEL
#include <mpi.h>
#endif

#ifdef PARALLEL
#include <Epetra_MpiComm.h>
#else
#include <Epetra_SerialComm.h>
#endif

#include "gas_fsi.H"


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FS3I::GasFSI::Scatra2ToScatra1(Teuchos::RCP<Epetra_Vector> iv)
{
  return scatracoup_.SlaveToMaster(iv);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> FS3I::GasFSI::Scatra1ToScatra2(Teuchos::RCP<Epetra_Vector> iv)
{
  return scatracoup_.MasterToSlave(iv);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::GasFSI::CheckInterfaceDirichletBC()
{
  Teuchos::RCP<DRT::Discretization> masterdis = scatravec_[0]->ScaTraField().Discretization();
  Teuchos::RCP<DRT::Discretization> slavedis = scatravec_[1]->ScaTraField().Discretization();

  Teuchos::RCP<const Epetra_Map> mastermap = scatracoup_.MasterDofMap();
  Teuchos::RCP<const Epetra_Map> permmastermap = scatracoup_.PermMasterDofMap();
  Teuchos::RCP<const Epetra_Map> slavemap = scatracoup_.SlaveDofMap();
  Teuchos::RCP<const Epetra_Map> permslavemap = scatracoup_.PermSlaveDofMap();

  const Teuchos::RCP<const LINALG::MapExtractor> masterdirichmapex = scatravec_[0]->ScaTraField().DirichMaps();
  const Teuchos::RCP<const Epetra_Map> masterdirichmap = masterdirichmapex->CondMap();

  // filter out master dirichlet dofs associated with the interface
  Teuchos::RCP<Epetra_Vector> masterifdirich = rcp(new Epetra_Vector(*mastermap,true));
  for (int i=0; i<mastermap->NumMyElements(); ++i)
  {
    int gid = mastermap->GID(i);
    if (masterdirichmap->MyGID(gid))
    {
      (*masterifdirich)[i] = 1.0;
    }
  }
  Teuchos::RCP<Epetra_Vector> test_slaveifdirich = scatracoup_.MasterToSlave(masterifdirich);

  const Teuchos::RCP<const LINALG::MapExtractor> slavedirichmapex = scatravec_[1]->ScaTraField().DirichMaps();
  const Teuchos::RCP<const Epetra_Map> slavedirichmap = slavedirichmapex->CondMap();

  // filter out slave dirichlet dofs associated with the interface
  Teuchos::RCP<Epetra_Vector> slaveifdirich = rcp(new Epetra_Vector(*slavemap,true));
  for (int i=0; i<slavemap->NumMyElements(); ++i)
  {
    int gid = slavemap->GID(i);
    if (slavedirichmap->MyGID(gid))
    {
      (*slaveifdirich)[i] = 1.0;
    }
  }
  Teuchos::RCP<Epetra_Vector> test_masterifdirich = scatracoup_.SlaveToMaster(slaveifdirich);

  // check if the locations of non-zero entries do not match
  for (int i=0; i<slavedis->DofRowMap()->NumMyElements(); ++i)
  {
    int gid = slavedis->DofRowMap()->GID(i);
    if (slavemap->MyGID(gid)) // in this case, the current dof is part of the interface
    {
      if ((*test_slaveifdirich)[slavemap->LID(gid)] == 1.0 and (*slaveifdirich)[slavemap->LID(gid)] != 1.0)
      {
        dserror("Dirichlet boundary conditions not matching at the interface");
      }
    }
  }

  for (int i=0; i<masterdis->DofRowMap()->NumMyElements(); ++i)
  {
    int gid = masterdis->DofRowMap()->GID(i);
    if (mastermap->MyGID(gid)) // in this case, the current dof is part of the interface
    {
      if ((*test_masterifdirich)[mastermap->LID(gid)] == 1.0 and (*masterifdirich)[mastermap->LID(gid)] != 1.0)
      {
        dserror("Dirichlet boundary conditions not matching at the interface");
      }
    }
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::GasFSI::ExtractVel(std::vector<Teuchos::RCP<const Epetra_Vector> >& convel,
                              std::vector<Teuchos::RCP<const Epetra_Vector> >& vel)
{
  // extract fluid velocities

  convel.push_back(fsi_->FluidField().ConvectiveVel());
  vel.push_back(fsi_->FluidField().Velnp());

  // extract structure velocities and accelerations

  Teuchos::RCP<Epetra_Vector> velocity = rcp(new Epetra_Vector(*(fsi_->StructureField().ExtractVelnp())));
  vel.push_back(velocity);
  // structure ScaTra: velocity and grid velocity are identical!
  Teuchos::RCP<Epetra_Vector> zeros = rcp(new Epetra_Vector(velocity->Map(),true));
  convel.push_back(zeros);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::GasFSI::SetVelocityFields()
{
  std::vector<Teuchos::RCP<const Epetra_Vector> > convel;
  std::vector<Teuchos::RCP<const Epetra_Vector> > vel;
  ExtractVel(convel, vel);

  std::vector<Teuchos::RCP<DRT::Discretization> > discret;

  discret.push_back(fsi_->FluidAdapter().Discretization());
  discret.push_back(fsi_->StructureAdapter().Discretization());

  for (unsigned i=0; i<scatravec_.size(); ++i)
  {
    Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> scatra = scatravec_[i];
    scatra->ScaTraField().SetVelocityField(convel[i],
                                           Teuchos::null,
                                           vel[i],
                                           Teuchos::null,
                                           Teuchos::null,
                                           discret[i]);
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::GasFSI::SetMeshDisp()
{
  // fluid field
  Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> fluidscatra = scatravec_[0];
  ADAPTER::Fluid& fluidadapter = fsi_->FluidAdapter();
  fluidscatra->ScaTraField().ApplyMeshMovement(fluidadapter.Dispnp(),
                                               fluidadapter.Discretization());

  // structure field
  Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> structscatra = scatravec_[1];
  ADAPTER::Structure& structadapter = fsi_->StructureAdapter();
  structscatra->ScaTraField().ApplyMeshMovement(structadapter.Dispnp(),
                                                structadapter.Discretization());
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool FS3I::GasFSI::AbortScatraNonlinIter(const int itnum)
{
#ifdef PARALLEL
  const Epetra_Comm& comm = scatravec_[0]->ScaTraField().Discretization()->Comm();
#else
  Epetra_SerialComm comm;
#endif

  int myrank = comm.MyPID();

  // some input parameters for the scatra fields
  const Teuchos::ParameterList& scatradyn = DRT::Problem::Instance()->ScalarTransportDynamicParams();
  const int itemax = scatradyn.sublist("NONLINEAR").get<int>("ITEMAX");
  const double ittol = scatradyn.sublist("NONLINEAR").get<double>("CONVTOL");
  const double abstolres = scatradyn.sublist("NONLINEAR").get<double>("ABSTOLRES");

  //----------------------------------------------------- compute norms
  double conresnorm(0.0);
  scatrarhs_->Norm2(&conresnorm);
  // set up vector of absolute concentrations
  double connorm_L2(0.0);
  Teuchos::RCP<Epetra_Vector> con = rcp(new Epetra_Vector(scatraincrement_->Map()));
  Teuchos::RCP<const Epetra_Vector> scatra1 = scatravec_[0]->ScaTraField().Phinp();
  Teuchos::RCP<const Epetra_Vector> scatra2 = scatravec_[1]->ScaTraField().Phinp();
  SetupCoupledScatraVector(con,scatra1,scatra2);
  con->Norm2(&connorm_L2);

  // care for the case that nothing really happens in the concentration field
  if (connorm_L2 < 1e-5)
  {
    connorm_L2 = 1.0;
  }

  // absolute tolerance for deciding if residual is (already) zero
  // prevents additional solver calls that will not improve the residual anymore

  //-------------------------------------------------- output to screen
  /* special case of very first iteration step:
      - solution increment is not yet available
      - do not perform a solver call when the initial residuals are < EPS14*/
  if (itnum == 0)
  {
    if (myrank == 0)
    {
      printf("|  %3d/%3d   | %10.3E[L_2 ]  | %10.3E   |\n",itnum,itemax,ittol,conresnorm);
    }
    // abort iteration, when there's nothing more to do
    if (conresnorm < abstolres)
    {
      // print 'finish line'
      if (myrank == 0)
      {
        printf("+------------+-------------------+--------------+\n");
      }
      return true;
    }
    else
      return false;
  }
  /* ordinary case later iteration steps:
     - solution increment can be printed
     - convergence check should be done*/
  else
  {
    // print the screen info
    if (myrank == 0)
    {
      printf("|  %3d/%3d   | %10.3E[L_2 ]  | %10.3E   |\n",itnum,itemax,ittol,conresnorm);
    }

    // this is the convergence check
    // We always require at least one solve. We test the L_2-norm of the
    // current residual. Norm of residual is just printed for information
    if (conresnorm <= ittol)
    {
      if (myrank == 0)
      {
        // print 'finish line'
        printf("+------------+-------------------+--------------+\n");
      }
      return true;
    }

    // abort iteration, when there's nothing more to do! -> more robustness
    else if (conresnorm < abstolres)
    {
      // print 'finish line'
      if (myrank == 0)
      {
        printf("+------------+-------------------+--------------+\n");
      }
      return true;
    }

    // warn if itemax is reached without convergence, but proceed to
    // next timestep...
    else if (itnum == itemax)
    {
      if (myrank == 0)
      {
        printf("+-----------------------------------------------+\n");
        printf("| >>>>>>> scatra not converged in itemax steps! |\n");
        printf("+-----------------------------------------------+\n");
      }
      // yes, we stop the iteration
      return true;
    }
    else
      return false;
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::GasFSI::TestResults(const Epetra_Comm& comm)
{
  DRT::Problem::Instance()->AddFieldTest(fsi_->FluidField().CreateFieldTest());
  DRT::Problem::Instance()->AddFieldTest(fsi_->StructureField().CreateFieldTest());

  for (unsigned i=0; i<scatravec_.size(); ++i)
  {
    Teuchos::RCP<ADAPTER::ScaTraBaseAlgorithm> scatra = scatravec_[i];
    DRT::Problem::Instance()->AddFieldTest(scatra->CreateScaTraFieldTest());
  }
  DRT::Problem::Instance()->TestAll(comm);
}

#endif

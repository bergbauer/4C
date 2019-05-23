/*!----------------------------------------------------------------------
\file XFAcoupling_manager.cpp
\brief Coupling Manager for eXtended Fluid Ale Coupling

\level 3

\maintainer Christoph Ager
            ager@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289 15249

*----------------------------------------------------------------------*/
#include "XFAcoupling_manager.H"

#include "../drt_xfem/xfem_condition_manager.H"
#include "../drt_fluid_xfluid/xfluid.H"
#include "../drt_adapter/ad_ale_fpsi.H"
#include "../drt_adapter/ad_str_structure.H"
#include "../linalg/linalg_mapextractor.H"

//#include "../drt_lib/drt_globalproblem.H"
//#include "../drt_io/io.H"

/*-----------------------------------------------------------------------------------------*
| Constructor                                                                 ager 06/2016 |
*-----------------------------------------------------------------------------------------*/
XFEM::XFACoupling_Manager::XFACoupling_Manager(Teuchos::RCP<FLD::XFluid> xfluid,
    Teuchos::RCP<ADAPTER::AleFpsiWrapper> ale, std::vector<int> idx,
    Teuchos::RCP<ADAPTER::Structure> structure)
    : Coupling_Comm_Manager(xfluid->Discretization(), ale->Discretization(), "", 0, 3),
      ale_(ale),
      xfluid_(xfluid),
      idx_(idx),
      structure_(structure)
{
  if (idx_.size() != (uint)(2 + (structure_ != Teuchos::null)))
    dserror("XFACoupling_Manager required (two + num coupled block) ( %d != %d)",
        (2 + (structure_ != Teuchos::null)), idx_.size());

  // TODO: Ale_Struct_coupling_
}

void XFEM::XFACoupling_Manager::PredictCouplingStates()
{
  /*
    if (Ale_Struct_coupling_ != Teuchos::null)
    {
      std::cout << "XFEM::XFACoupling_Manager::PredictCouplingStates"<< std::endl;

      //-------------------------------------------
      // Perform a 1st predictor to the ALE field (required, otherwise relaxation solve is bad
    conditioned afterwards) ale_->DoPredictor(); // const vel predictor on dispnp_
      //-------------------------------------------

      //-------------------------------------------
      // Perform a 2nd predictor in the sense of a relaxation solve (required, otherwise the ALE
    displacements are too large at the interface)
      //-------------------------------------------

      // first manipulate the fluid-solid interface, then setup the ale system
      Ale_Struct_coupling_->InsertVector(1,structure_->Dispnp(),0,ale_->WriteAccessDispnp(),XFEM::Coupling_Comm_Manager::full_to_full);

      // apply inner Dirichlet conditions (don't forget to reset the time and the step!)
      ale_->PrepareTimeStep(); // applies DBCs to the current dispnp

      ale_->TimeStep(ALE::UTILS::MapExtractor::dbc_set_structale);

      // Reset the time and dt to be called incremented again in the actual ale->PrepareTimeStep
      ale_->ResetTime(ale_->Dt());
    }
  */
}

/*-----------------------------------------------------------------------------------------*
| Set required displacement & velocity states in the coupling object          ager 06/2016 |
*-----------------------------------------------------------------------------------------*/
void XFEM::XFACoupling_Manager::SetCouplingStates()
{
  // 1 Sets structural conditioned Dispnp onto Ale
  // --> this will be done in one of the following commits!
  // Ale_Struct_coupling_->InsertVector(1,StructureField()->Dispnp(),0,AleField()->WriteAccessDispnp(),XFEM::Coupling_Comm_Manager::full_to_full);
  // 2 Get AleDisplacements
  Teuchos::RCP<Epetra_Vector> aledisplacements =
      Teuchos::rcp(new Epetra_Vector(*GetMapExtractor(0)->Map(1), true));
  InsertVector(1, ale_->Dispnp(), 0, aledisplacements, Coupling_Comm_Manager::partial_to_partial);
  // 3 Set Fluid Dispnp
  GetMapExtractor(0)->InsertVector(aledisplacements, 1, xfluid_->WriteAccessDispnp());

  // 4 new grid velocity
  xfluid_->UpdateGridv();

  // update also ALE vectors w.r.t. current state
  xfluid_->UpdateALEStateVectors();

  //  double max = 0.0;
  //  ale_->WriteAccessDispnp()->MaxValue(&max);
  //  std::cout << "ale_->WriteAccessDispnp() now " << max << std::endl;
  //
  //
  //  xfluid_->WriteAccessDispnp()->MaxValue(&max);
  //  std::cout << "xfluid_->WriteAccessDispnp() now " << max << std::endl;



  return;
}

/*-----------------------------------------------------------------------------------------*
| Add the coupling matrixes to the global systemmatrix                        ager 06/2016 |
*-----------------------------------------------------------------------------------------*/
void XFEM::XFACoupling_Manager::AddCouplingMatrix(
    LINALG::BlockSparseMatrixBase& systemmatrix, double scaling)
{
  // Get Idx of fluid and ale field map extractors
  const int& aidx_other = ALE::UTILS::MapExtractor::cond_other;
  const Teuchos::RCP<LINALG::BlockSparseMatrixBase> a = ale_->BlockSystemMatrix();

  // ALE Condensation
  LINALG::SparseMatrix& aii = a->Matrix(aidx_other, aidx_other);

  systemmatrix.Assign(idx_[1], idx_[1], LINALG::View, aii);

  // TODO: Ale_Struct_coupling_

  //  //////////////////////////////////////////////
  //  //////                                  //////
  //  //////    Linearization of FluidField   //////
  //  //////    with respect to ale mesh      //////
  //  //////             motion               //////
  //  //////                                  //////
  //  //////////////////////////////////////////////

  // TODO: THIS IS STILL MISSING, BUT USUALLY DOES NOT HAVE A BIG INFLUENCE ON THE CONVERGENCE!!!
  return;
}

/*-----------------------------------------------------------------------------------------*
| Add the coupling rhs                                                        ager 06/2016 |
*-----------------------------------------------------------------------------------------*/
void XFEM::XFACoupling_Manager::AddCouplingRHS(
    Teuchos::RCP<Epetra_Vector> rhs, const LINALG::MultiMapExtractor& me, double scaling)
{
  Teuchos::RCP<const Epetra_Vector> av = ale_->RHS();
  Teuchos::RCP<Epetra_Vector> aov = ale_->Interface()->ExtractOtherVector(av);
  me.InsertVector(*aov, idx_[1], *rhs);  // add ALE contributions to 'rhs'
  return;
}

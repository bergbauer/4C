/*---------------------------------------------------------------------*/
/*! \file

\brief Evaluation and assembly of all contact terms


\level 3

*/
/*---------------------------------------------------------------------*/


#include "baci_structure_new_model_evaluator_meshtying.H"

#include "baci_contact_aug_plot.H"
#include "baci_contact_aug_strategy.H"
#include "baci_contact_lagrange_strategy_poro.H"
#include "baci_contact_meshtying_abstract_strategy.H"
#include "baci_contact_meshtying_noxinterface.H"
#include "baci_contact_meshtying_strategy_factory.H"
#include "baci_io.H"
#include "baci_io_control.H"
#include "baci_lib_discret.H"
#include "baci_lib_globalproblem.H"
#include "baci_linalg_sparsematrix.H"
#include "baci_linalg_utils_densematrix_communication.H"
#include "baci_linalg_utils_sparse_algebra_assemble.H"
#include "baci_linalg_utils_sparse_algebra_create.H"
#include "baci_linalg_utils_sparse_algebra_manipulation.H"
#include "baci_solver_nonlin_nox_aux.H"
#include "baci_solver_nonlin_nox_group.H"
#include "baci_solver_nonlin_nox_group_prepostoperator.H"
#include "baci_solver_nonlin_nox_solver_linesearchbased.H"
#include "baci_structure_new_dbc.H"
#include "baci_structure_new_impl_generic.H"
#include "baci_structure_new_model_evaluator.H"
#include "baci_structure_new_model_evaluator_data.H"
#include "baci_structure_new_timint_base.H"
#include "baci_structure_new_utils.H"
#include "baci_utils_exceptions.H"

BACI_NAMESPACE_OPEN


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
STR::MODELEVALUATOR::Meshtying::Meshtying() : strategy_ptr_(Teuchos::null)
{
  // empty
}


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::Init(
    const Teuchos::RCP<STR::MODELEVALUATOR::Data>& eval_data_ptr,
    const Teuchos::RCP<STR::TIMINT::BaseDataGlobalState>& gstate_ptr,
    const Teuchos::RCP<STR::TIMINT::BaseDataIO>& gio_ptr,
    const Teuchos::RCP<STR::Integrator>& int_ptr,
    const Teuchos::RCP<const STR::TIMINT::Base>& timint_ptr, const int& dof_offset)
{
  STR::MODELEVALUATOR::Generic::Init(
      eval_data_ptr, gstate_ptr, gio_ptr, int_ptr, timint_ptr, dof_offset);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::Setup()
{
  CheckInit();

  // create the meshtying factory
  MORTAR::STRATEGY::FactoryMT factory;
  factory.Init(GStatePtr());
  factory.Setup();

  // check the problem dimension
  factory.CheckDimension();

  // create some local variables (later to be stored in strategy)
  std::vector<Teuchos::RCP<MORTAR::Interface>> interfaces;
  Teuchos::ParameterList cparams;

  // read and check contact input parameters
  factory.ReadAndCheckInput(cparams);

  // check for FillComplete of discretization
  if (not Discret().Filled()) dserror("Discretization is not FillComplete.");

  // ---------------------------------------------------------------------
  // build the meshtying interfaces
  // ---------------------------------------------------------------------
  // FixMe Would be great, if we get rid of these poro parameters...
  bool poroslave = false;
  bool poromaster = false;
  factory.BuildInterfaces(cparams, interfaces, poroslave, poromaster);

  // ---------------------------------------------------------------------
  // build the solver strategy object
  // ---------------------------------------------------------------------
  strategy_ptr_ = factory.BuildStrategy(cparams, poroslave, poromaster, DofOffset(), interfaces);

  // build the search tree
  factory.BuildSearchTree(interfaces);

  // ---------------------------------------------------------------------
  // final touches to the meshtying strategy
  // ---------------------------------------------------------------------
  strategy_ptr_->StoreDirichletStatus(Int().GetDbc().GetDBCMapExtractor());
  strategy_ptr_->SetState(MORTAR::state_new_displacement, Int().GetDbc().GetZeros());
  strategy_ptr_->SaveReferenceState(Int().GetDbc().GetZerosPtr());
  strategy_ptr_->EvaluateReferenceState();
  strategy_ptr_->Inttime_init();
  SetTimeIntegrationInfo(*strategy_ptr_);
  strategy_ptr_->RedistributeContact(
      Int().GetDbc().GetZerosPtr(), Int().GetDbc().GetZerosPtr());  // ToDo RedistributeMeshtying??
  strategy_ptr_->MortarCoupling(Int().GetDbc().GetZerosPtr());

  strategy_ptr_->NoxInterfacePtr()->Init(GStatePtr());
  strategy_ptr_->NoxInterfacePtr()->Setup();

  if (!GState().GetRestartStep())
  {
    // perform mesh initialization if required by input parameter MESH_RELOCATION
    auto mesh_relocation_parameter = INPUT::IntegralValue<INPAR::MORTAR::MeshRelocation>(
        DRT::Problem::Instance()->MortarCouplingParams(), "MESH_RELOCATION");

    if (mesh_relocation_parameter == INPAR::MORTAR::relocation_initial)
    {
      Teuchos::RCP<const Epetra_Vector> Xslavemod =
          dynamic_cast<MORTAR::StrategyBase&>(*strategy_ptr_).MeshInitialization();
      if (Xslavemod != Teuchos::null)
      {
        mesh_relocation_ = Teuchos::rcp(new Epetra_Vector(*GState().DofRowMap()));
        for (int i = 0; i < strategy_ptr_->SlaveRowNodes()->NumMyElements(); ++i)
          for (int d = 0; d < strategy_ptr_->Dim(); ++d)
          {
            int gid = GState().GetDiscret()->Dof(
                GState().GetDiscret()->gNode(strategy_ptr_->SlaveRowNodes()->GID(i)), d);
            mesh_relocation_->operator[](mesh_relocation_->Map().LID(gid)) =
                GState().GetDiscret()->gNode(strategy_ptr_->SlaveRowNodes()->GID(i))->X()[d] -
                Xslavemod->operator[](Xslavemod->Map().LID(gid));
          }
        ApplyMeshInitialization(Xslavemod);
      }
    }
    else if (mesh_relocation_parameter == INPAR::MORTAR::relocation_timestep)
    {
      dserror(
          "Meshtying with MESH_RELOCATION every_timestep not permitted. Change to MESH_RELOCATION "
          "initial or MESH_RELOCATION no.");
    }
  }

  issetup_ = true;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::Meshtying::AssembleForce(Epetra_Vector& f, const double& timefac_np) const
{
  Teuchos::RCP<const Epetra_Vector> block_vec_ptr = Teuchos::null;
  if (INPUT::IntegralValue<INPAR::MORTAR::AlgorithmType>(Strategy().Params(), "ALGORITHM") ==
          INPAR::MORTAR::algorithm_gpts ||
      Strategy().IsPenalty())
  {
    block_vec_ptr = Strategy().GetRhsBlockPtr(CONTACT::VecBlockType::displ);
    // if there are no active contact contributions, we can skip this...
    dsassert(!block_vec_ptr.is_null(), "force not available");
    CORE::LINALG::AssembleMyVector(1.0, f, timefac_np, *block_vec_ptr);
  }
  else if (Strategy().IsCondensedSystem())
  {
    // --- displ. - block ---------------------------------------------------
    block_vec_ptr = Strategy().GetRhsBlockPtr(CONTACT::VecBlockType::displ);
    // if there are no active contact contributions, we can skip this...
    if (block_vec_ptr.is_null()) return true;

    CORE::LINALG::AssembleMyVector(1.0, f, timefac_np, *block_vec_ptr);
  }
  else if (Strategy().IsSaddlePointSystem())
  {
    // --- displ. - block ---------------------------------------------------
    block_vec_ptr = Strategy().GetRhsBlockPtr(CONTACT::VecBlockType::displ);
    // if there are no active contact contributions, we can skip this...
    if (block_vec_ptr.is_null()) return true;

    CORE::LINALG::AssembleMyVector(1.0, f, timefac_np, *block_vec_ptr);
  }

  return true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::Meshtying::AssembleJacobian(
    CORE::LINALG::SparseOperator& jac, const double& timefac_np) const
{
  Teuchos::RCP<CORE::LINALG::SparseMatrix> block_ptr = Teuchos::null;
  int err = 0;
  // ---------------------------------------------------------------------
  // Penalty / gpts / Nitsche system: no additional/condensed dofs
  // ---------------------------------------------------------------------
  if (INPUT::IntegralValue<INPAR::MORTAR::AlgorithmType>(Strategy().Params(), "ALGORITHM") ==
          INPAR::MORTAR::algorithm_gpts ||
      Strategy().IsPenalty())
  {
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::displ_displ);
    if (Strategy().IsPenalty() && block_ptr.is_null()) return true;
    Teuchos::RCP<CORE::LINALG::SparseMatrix> jac_dd = GState().ExtractDisplBlock(jac);
    jac_dd->Add(*block_ptr, false, timefac_np, 1.0);
  }
  // ---------------------------------------------------------------------
  // condensed system of equations
  // ---------------------------------------------------------------------
  else if (Strategy().IsCondensedSystem())
  {
    // --- Kdd - block ---------------------------------------------------
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::displ_displ);
    if (not block_ptr.is_null())
    {
      Teuchos::RCP<CORE::LINALG::SparseMatrix> jac_dd_ptr = GState().ExtractDisplBlock(jac);
      jac_dd_ptr->Add(*block_ptr, false, timefac_np, 1.0);
      // reset the block pointers, just to be on the safe side
      block_ptr = Teuchos::null;
    }
  }
  // ---------------------------------------------------------------------
  // saddle-point system of equations or no contact contributions
  // ---------------------------------------------------------------------
  else if (Strategy().SystemType() == INPAR::CONTACT::system_saddlepoint)
  {
    // --- Kdd - block ---------------------------------------------------
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::displ_displ);
    if (not block_ptr.is_null())
    {
      Teuchos::RCP<CORE::LINALG::SparseMatrix> jac_dd_ptr = GState().ExtractDisplBlock(jac);
      jac_dd_ptr->Add(*block_ptr, false, timefac_np, 1.0);
      // reset the block pointers, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kdz - block ---------------------------------------------------
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::displ_lm);
    if (not block_ptr.is_null())
    {
      //      block_ptr->Scale(timefac_np);
      GState().AssignModelBlock(jac, *block_ptr, Type(), STR::MatBlockType::displ_lm);
      // reset the block pointer, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kzd - block ---------------------------------------------------
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::lm_displ);
    if (not block_ptr.is_null())
    {
      GState().AssignModelBlock(jac, *block_ptr, Type(), STR::MatBlockType::lm_displ);
      // reset the block pointer, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kzz - block ---------------------------------------------------
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::lm_lm);
    if (not block_ptr.is_null())
    {
      GState().AssignModelBlock(jac, *block_ptr, Type(), STR::MatBlockType::lm_lm);
      // reset the block pointer, just to be on the safe side
      block_ptr = Teuchos::null;
    }
  }

  return (err == 0);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const Teuchos::RCP<CONTACT::MtAbstractStrategy>& STR::MODELEVALUATOR::Meshtying::StrategyPtr()
{
  CheckInitSetup();
  return strategy_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
CONTACT::MtAbstractStrategy& STR::MODELEVALUATOR::Meshtying::Strategy()
{
  CheckInitSetup();
  return *strategy_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const CONTACT::MtAbstractStrategy& STR::MODELEVALUATOR::Meshtying::Strategy() const
{
  CheckInitSetup();
  return *strategy_ptr_;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> STR::MODELEVALUATOR::Meshtying::GetBlockDofRowMapPtr() const
{
  CheckInitSetup();
  if (Strategy().LMDoFRowMapPtr(true) == Teuchos::null)
    return GState().DofRowMap();
  else
  {
    enum INPAR::CONTACT::SystemType systype =
        INPUT::IntegralValue<INPAR::CONTACT::SystemType>(Strategy().Params(), "SYSTEM");

    if (systype == INPAR::CONTACT::system_saddlepoint)
      return Strategy().LMDoFRowMapPtr(true);
    else
      return GState().DofRowMap();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> STR::MODELEVALUATOR::Meshtying::GetCurrentSolutionPtr() const
{
  //  //TODO: this should be removed!
  //  DRT::Problem* problem = DRT::Problem::Instance();
  //  enum INPAR::CONTACT::SystemType systype =
  //      INPUT::IntegralValue<INPAR::CONTACT::SystemType>(
  //          problem->ContactDynamicParams(),"SYSTEM");
  //  if (systype == INPAR::CONTACT::system_condensed)
  //    return Teuchos::null;
  //
  //  if (Strategy().GetLagrMultNp(false)!=Teuchos::null)
  //  {
  //    Teuchos::RCP<Epetra_Vector> curr_lm_ptr =
  //        Teuchos::rcp(new Epetra_Vector(*Strategy().GetLagrMultNp(false)));
  //    if (not curr_lm_ptr.is_null())
  //      curr_lm_ptr->ReplaceMap(Strategy().LMDoFRowMap(false));
  //
  //    ExtendLagrangeMultiplierDomain( curr_lm_ptr );
  //
  //    return curr_lm_ptr;
  //  }
  //  else
  return Teuchos::null;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> STR::MODELEVALUATOR::Meshtying::GetLastTimeStepSolutionPtr() const
{
  //  DRT::Problem* problem = DRT::Problem::Instance();
  //  enum INPAR::CONTACT::SystemType systype =
  //      INPUT::IntegralValue<INPAR::CONTACT::SystemType>(
  //          problem->ContactDynamicParams(),"SYSTEM");
  //  if (systype == INPAR::CONTACT::system_condensed)
  //    return Teuchos::null;
  //
  //  if (Strategy().GetLagrMultN(false).is_null())
  //    return Teuchos::null;
  //
  //  Teuchos::RCP<Epetra_Vector> old_lm_ptr =
  //      Teuchos::rcp(new Epetra_Vector(*Strategy().GetLagrMultN(false)));
  //  if (not old_lm_ptr.is_null())
  //    old_lm_ptr->ReplaceMap(Strategy().LMDoFRowMap(false));
  //
  //  ExtendLagrangeMultiplierDomain( old_lm_ptr );
  //
  //  return old_lm_ptr;
  return Teuchos::null;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::RunPreApplyJacobianInverse(const Epetra_Vector& rhs,
    Epetra_Vector& result, const Epetra_Vector& xold, const NOX::NLN::Group& grp)
{
  Teuchos::RCP<CORE::LINALG::SparseMatrix> jac_dd = GState().JacobianDisplBlock();
  const_cast<CONTACT::MtAbstractStrategy&>(Strategy())
      .RunPreApplyJacobianInverse(jac_dd, const_cast<Epetra_Vector&>(rhs));
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::RunPostApplyJacobianInverse(const Epetra_Vector& rhs,
    Epetra_Vector& result, const Epetra_Vector& xold, const NOX::NLN::Group& grp)
{
  const_cast<CONTACT::MtAbstractStrategy&>(Strategy()).RunPostApplyJacobianInverse(result);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<const CORE::LINALG::SparseMatrix> STR::MODELEVALUATOR::Meshtying::GetJacobianBlock(
    const STR::MatBlockType bt) const
{
  return GState().GetJacobianBlock(Type(), bt);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::Meshtying::EvaluateForce()
{
  return Strategy().EvaluateForce(GState().GetDisNp());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::Meshtying::EvaluateForceStiff()
{
  return Strategy().EvaluateForceStiff(GState().GetDisNp());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::Meshtying::EvaluateStiff()
{
  return Strategy().EvaluateStiff(GState().GetDisNp());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::ApplyMeshInitialization(
    Teuchos::RCP<const Epetra_Vector> Xslavemod)
{
  // check modified positions vector
  if (Xslavemod == Teuchos::null) return;

  // create fully overlapping slave node map
  Teuchos::RCP<Epetra_Map> slavemap = strategy_ptr_->SlaveRowNodes();
  Teuchos::RCP<Epetra_Map> allreduceslavemap = CORE::LINALG::AllreduceEMap(*slavemap);

  // export modified node positions to column map of problem discretization
  const Epetra_Map* dof_colmap = DiscretPtr()->DofColMap();
  const Epetra_Map* node_colmap = DiscretPtr()->NodeColMap();
  Teuchos::RCP<Epetra_Vector> Xslavemodcol = CORE::LINALG::CreateVector(*dof_colmap, false);
  CORE::LINALG::Export(*Xslavemod, *Xslavemodcol);

  const int numnode = allreduceslavemap->NumMyElements();
  const int numdim = DRT::Problem::Instance()->NDim();
  const Epetra_Vector& gvector = *Xslavemodcol;

  // loop over all slave nodes (for all procs)
  for (int index = 0; index < numnode; ++index)
  {
    int gid = allreduceslavemap->GID(index);

    // only do someting for nodes in my column map
    int ilid = node_colmap->LID(gid);
    if (ilid < 0) continue;

    DRT::Node* mynode = DiscretPtr()->gNode(gid);

    // get degrees of freedom associated with this fluid/structure node
    std::vector<int> nodedofs = DiscretPtr()->Dof(0, mynode);
    std::vector<double> nvector(3, 0.0);

    // create new position vector
    for (int i = 0; i < numdim; ++i)
    {
      const int lid = gvector.Map().LID(nodedofs[i]);

      if (lid < 0)
        dserror("ERROR: Proc %d: Cannot find gid=%d in Epetra_Vector", gvector.Comm().MyPID(),
            nodedofs[i]);

      nvector[i] += gvector[lid];
    }

    // set new reference position
    mynode->SetPos(nvector);
  }

  // re-initialize finite elements
  CORE::COMM::ParObjectFactory::Instance().InitializeElements(Discret());
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::RunPostComputeX(
    const Epetra_Vector& xold, const Epetra_Vector& dir, const Epetra_Vector& xnew)
{
  CheckInitSetup();

  Strategy().RunPostComputeX(xold, dir, xnew);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::RemoveCondensedContributionsFromRhs(Epetra_Vector& rhs)
{
  CheckInitSetup();

  Strategy().RemoveCondensedContributionsFromRhs(rhs);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::WriteRestart(
    IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  if (mesh_relocation_ != Teuchos::null)
    iowriter.WriteVector("mesh_relocation", mesh_relocation_);
  else
  {
    Teuchos::RCP<Epetra_Vector> tmp = Teuchos::rcp(new Epetra_Vector(*Discret().DofRowMap(), true));
    iowriter.WriteVector("mesh_relocation", tmp);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::ReadRestart(IO::DiscretizationReader& ioreader)
{
  mesh_relocation_ = Teuchos::rcp(new Epetra_Vector(*Discret().DofRowMap(), true));
  ioreader.ReadVector(mesh_relocation_, "mesh_relocation");

  strategy_ptr_->SetState(MORTAR::state_new_displacement, *mesh_relocation_);
  strategy_ptr_->MortarCoupling(mesh_relocation_);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::SetTimeIntegrationInfo(
    CONTACT::MtAbstractStrategy& strategy) const
{
  const INPAR::STR::DynamicType dyntype = TimInt().GetDataSDyn().GetDynamicType();
  const double time_fac = Int().GetIntParam();

  strategy.SetTimeIntegrationInfo(time_fac, dyntype);
}

BACI_NAMESPACE_CLOSE

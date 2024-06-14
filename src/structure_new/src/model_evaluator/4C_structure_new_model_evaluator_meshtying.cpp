/*---------------------------------------------------------------------*/
/*! \file

\brief Evaluation and assembly of all contact terms


\level 3

*/
/*---------------------------------------------------------------------*/


#include "4C_structure_new_model_evaluator_meshtying.hpp"

#include "4C_contact_aug_strategy.hpp"
#include "4C_contact_meshtying_abstract_strategy.hpp"
#include "4C_contact_meshtying_noxinterface.hpp"
#include "4C_contact_meshtying_strategy_factory.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_io.hpp"
#include "4C_io_control.hpp"
#include "4C_linalg_sparsematrix.hpp"
#include "4C_linalg_utils_densematrix_communication.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_solver_nonlin_nox_group.hpp"
#include "4C_solver_nonlin_nox_group_prepostoperator.hpp"
#include "4C_solver_nonlin_nox_solver_linesearchbased.hpp"
#include "4C_structure_new_dbc.hpp"
#include "4C_structure_new_impl_generic.hpp"
#include "4C_structure_new_model_evaluator.hpp"
#include "4C_structure_new_model_evaluator_data.hpp"
#include "4C_structure_new_timint_base.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN


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
    const Teuchos::RCP<STR::TimeInt::BaseDataGlobalState>& gstate_ptr,
    const Teuchos::RCP<STR::TimeInt::BaseDataIO>& gio_ptr,
    const Teuchos::RCP<STR::Integrator>& int_ptr,
    const Teuchos::RCP<const STR::TimeInt::Base>& timint_ptr, const int& dof_offset)
{
  STR::MODELEVALUATOR::Generic::Init(
      eval_data_ptr, gstate_ptr, gio_ptr, int_ptr, timint_ptr, dof_offset);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::Setup()
{
  check_init();

  // create the meshtying factory
  Mortar::STRATEGY::FactoryMT factory;
  factory.Init(global_state_ptr()->get_discret());
  factory.Setup();

  // check the problem dimension
  factory.CheckDimension();

  // create some local variables (later to be stored in strategy)
  std::vector<Teuchos::RCP<Mortar::Interface>> interfaces;
  Teuchos::ParameterList cparams;

  // read and check contact input parameters
  factory.read_and_check_input(cparams);

  // check for fill_complete of discretization
  if (not discret().Filled()) FOUR_C_THROW("discretization is not fill_complete.");

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
  strategy_ptr_ = factory.BuildStrategy(cparams, poroslave, poromaster, dof_offset(), interfaces);

  // build the search tree
  factory.BuildSearchTree(interfaces);

  // ---------------------------------------------------------------------
  // final touches to the meshtying strategy
  // ---------------------------------------------------------------------
  strategy_ptr_->store_dirichlet_status(integrator().get_dbc().GetDBCMapExtractor());
  strategy_ptr_->set_state(Mortar::state_new_displacement, integrator().get_dbc().GetZeros());
  strategy_ptr_->SaveReferenceState(integrator().get_dbc().GetZerosPtr());
  strategy_ptr_->evaluate_reference_state();
  strategy_ptr_->Inttime_init();
  set_time_integration_info(*strategy_ptr_);
  strategy_ptr_->RedistributeContact(integrator().get_dbc().GetZerosPtr(),
      integrator().get_dbc().GetZerosPtr());  // ToDo redistribute_meshtying??
  strategy_ptr_->MortarCoupling(integrator().get_dbc().GetZerosPtr());

  strategy_ptr_->nox_interface_ptr()->Init(global_state_ptr());
  strategy_ptr_->nox_interface_ptr()->Setup();

  if (!global_state().get_restart_step())
  {
    // perform mesh initialization if required by input parameter MESH_RELOCATION
    auto mesh_relocation_parameter = Core::UTILS::IntegralValue<Inpar::Mortar::MeshRelocation>(
        Global::Problem::Instance()->mortar_coupling_params(), "MESH_RELOCATION");

    if (mesh_relocation_parameter == Inpar::Mortar::relocation_initial)
    {
      Teuchos::RCP<const Epetra_Vector> Xslavemod =
          dynamic_cast<Mortar::StrategyBase&>(*strategy_ptr_).MeshInitialization();
      if (Xslavemod != Teuchos::null)
      {
        mesh_relocation_ = Teuchos::rcp(new Epetra_Vector(*global_state().dof_row_map()));
        for (int i = 0; i < strategy_ptr_->SlaveRowNodes()->NumMyElements(); ++i)
          for (int d = 0; d < strategy_ptr_->Dim(); ++d)
          {
            int gid = global_state().get_discret()->Dof(
                global_state().get_discret()->gNode(strategy_ptr_->SlaveRowNodes()->GID(i)), d);
            mesh_relocation_->operator[](mesh_relocation_->Map().LID(gid)) =
                global_state()
                    .get_discret()
                    ->gNode(strategy_ptr_->SlaveRowNodes()->GID(i))
                    ->X()[d] -
                Xslavemod->operator[](Xslavemod->Map().LID(gid));
          }
        apply_mesh_initialization(Xslavemod);
      }
    }
    else if (mesh_relocation_parameter == Inpar::Mortar::relocation_timestep)
    {
      FOUR_C_THROW(
          "Meshtying with MESH_RELOCATION every_timestep not permitted. Change to MESH_RELOCATION "
          "initial or MESH_RELOCATION no.");
    }
  }

  issetup_ = true;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::Meshtying::assemble_force(
    Epetra_Vector& f, const double& timefac_np) const
{
  Teuchos::RCP<const Epetra_Vector> block_vec_ptr = Teuchos::null;
  if (Core::UTILS::IntegralValue<Inpar::Mortar::AlgorithmType>(Strategy().Params(), "ALGORITHM") ==
          Inpar::Mortar::algorithm_gpts ||
      Strategy().IsPenalty())
  {
    block_vec_ptr = Strategy().GetRhsBlockPtr(CONTACT::VecBlockType::displ);
    // if there are no active contact contributions, we can skip this...
    FOUR_C_ASSERT(!block_vec_ptr.is_null(), "force not available");
    Core::LinAlg::AssembleMyVector(1.0, f, timefac_np, *block_vec_ptr);
  }
  else if (Strategy().IsCondensedSystem())
  {
    // --- displ. - block ---------------------------------------------------
    block_vec_ptr = Strategy().GetRhsBlockPtr(CONTACT::VecBlockType::displ);
    // if there are no active contact contributions, we can skip this...
    if (block_vec_ptr.is_null()) return true;

    Core::LinAlg::AssembleMyVector(1.0, f, timefac_np, *block_vec_ptr);
  }
  else if (Strategy().IsSaddlePointSystem())
  {
    // --- displ. - block ---------------------------------------------------
    block_vec_ptr = Strategy().GetRhsBlockPtr(CONTACT::VecBlockType::displ);
    // if there are no active contact contributions, we can skip this...
    if (block_vec_ptr.is_null()) return true;

    Core::LinAlg::AssembleMyVector(1.0, f, timefac_np, *block_vec_ptr);
  }

  return true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::Meshtying::assemble_jacobian(
    Core::LinAlg::SparseOperator& jac, const double& timefac_np) const
{
  Teuchos::RCP<Core::LinAlg::SparseMatrix> block_ptr = Teuchos::null;
  int err = 0;
  // ---------------------------------------------------------------------
  // Penalty / gpts / Nitsche system: no additional/condensed dofs
  // ---------------------------------------------------------------------
  if (Core::UTILS::IntegralValue<Inpar::Mortar::AlgorithmType>(Strategy().Params(), "ALGORITHM") ==
          Inpar::Mortar::algorithm_gpts ||
      Strategy().IsPenalty())
  {
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::displ_displ);
    if (Strategy().IsPenalty() && block_ptr.is_null()) return true;
    Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd = global_state().extract_displ_block(jac);
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
      Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd_ptr = global_state().extract_displ_block(jac);
      jac_dd_ptr->Add(*block_ptr, false, timefac_np, 1.0);
      // reset the block pointers, just to be on the safe side
      block_ptr = Teuchos::null;
    }
  }
  // ---------------------------------------------------------------------
  // saddle-point system of equations or no contact contributions
  // ---------------------------------------------------------------------
  else if (Strategy().SystemType() == Inpar::CONTACT::system_saddlepoint)
  {
    // --- Kdd - block ---------------------------------------------------
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::displ_displ);
    if (not block_ptr.is_null())
    {
      Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd_ptr = global_state().extract_displ_block(jac);
      jac_dd_ptr->Add(*block_ptr, false, timefac_np, 1.0);
      // reset the block pointers, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kdz - block ---------------------------------------------------
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::displ_lm);
    if (not block_ptr.is_null())
    {
      //      block_ptr->Scale(timefac_np);
      global_state().assign_model_block(jac, *block_ptr, Type(), STR::MatBlockType::displ_lm);
      // reset the block pointer, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kzd - block ---------------------------------------------------
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::lm_displ);
    if (not block_ptr.is_null())
    {
      global_state().assign_model_block(jac, *block_ptr, Type(), STR::MatBlockType::lm_displ);
      // reset the block pointer, just to be on the safe side
      block_ptr = Teuchos::null;
    }

    // --- Kzz - block ---------------------------------------------------
    block_ptr = Strategy().GetMatrixBlockPtr(CONTACT::MatBlockType::lm_lm);
    if (not block_ptr.is_null())
    {
      global_state().assign_model_block(jac, *block_ptr, Type(), STR::MatBlockType::lm_lm);
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
  check_init_setup();
  return strategy_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
CONTACT::MtAbstractStrategy& STR::MODELEVALUATOR::Meshtying::Strategy()
{
  check_init_setup();
  return *strategy_ptr_;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const CONTACT::MtAbstractStrategy& STR::MODELEVALUATOR::Meshtying::Strategy() const
{
  check_init_setup();
  return *strategy_ptr_;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Map> STR::MODELEVALUATOR::Meshtying::get_block_dof_row_map_ptr() const
{
  check_init_setup();
  if (Strategy().LMDoFRowMapPtr(true) == Teuchos::null)
    return global_state().dof_row_map();
  else
  {
    enum Inpar::CONTACT::SystemType systype =
        Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(Strategy().Params(), "SYSTEM");

    if (systype == Inpar::CONTACT::system_saddlepoint)
      return Strategy().LMDoFRowMapPtr(true);
    else
      return global_state().dof_row_map();
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> STR::MODELEVALUATOR::Meshtying::get_current_solution_ptr() const
{
  //  //TODO: this should be removed!
  //  Global::Problem* problem = Global::Problem::Instance();
  //  enum Inpar::CONTACT::SystemType systype =
  //      Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
  //          problem->contact_dynamic_params(),"SYSTEM");
  //  if (systype == Inpar::CONTACT::system_condensed)
  //    return Teuchos::null;
  //
  //  if (Strategy().GetLagrMultNp(false)!=Teuchos::null)
  //  {
  //    Teuchos::RCP<Epetra_Vector> curr_lm_ptr =
  //        Teuchos::rcp(new Epetra_Vector(*Strategy().GetLagrMultNp(false)));
  //    if (not curr_lm_ptr.is_null())
  //      curr_lm_ptr->ReplaceMap(Strategy().LMDoFRowMap(false));
  //
  //    extend_lagrange_multiplier_domain( curr_lm_ptr );
  //
  //    return curr_lm_ptr;
  //  }
  //  else
  return Teuchos::null;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<const Epetra_Vector> STR::MODELEVALUATOR::Meshtying::get_last_time_step_solution_ptr()
    const
{
  //  Global::Problem* problem = Global::Problem::Instance();
  //  enum Inpar::CONTACT::SystemType systype =
  //      Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(
  //          problem->contact_dynamic_params(),"SYSTEM");
  //  if (systype == Inpar::CONTACT::system_condensed)
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
  //  extend_lagrange_multiplier_domain( old_lm_ptr );
  //
  //  return old_lm_ptr;
  return Teuchos::null;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::run_pre_apply_jacobian_inverse(const Epetra_Vector& rhs,
    Epetra_Vector& result, const Epetra_Vector& xold, const NOX::Nln::Group& grp)
{
  Teuchos::RCP<Core::LinAlg::SparseMatrix> jac_dd = global_state().jacobian_displ_block();
  const_cast<CONTACT::MtAbstractStrategy&>(Strategy())
      .run_pre_apply_jacobian_inverse(jac_dd, const_cast<Epetra_Vector&>(rhs));
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::run_post_apply_jacobian_inverse(const Epetra_Vector& rhs,
    Epetra_Vector& result, const Epetra_Vector& xold, const NOX::Nln::Group& grp)
{
  const_cast<CONTACT::MtAbstractStrategy&>(Strategy()).run_post_apply_jacobian_inverse(result);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<const Core::LinAlg::SparseMatrix> STR::MODELEVALUATOR::Meshtying::GetJacobianBlock(
    const STR::MatBlockType bt) const
{
  return global_state().get_jacobian_block(Type(), bt);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::Meshtying::evaluate_force()
{
  return Strategy().evaluate_force(global_state().get_dis_np());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::Meshtying::evaluate_force_stiff()
{
  return Strategy().evaluate_force_stiff(global_state().get_dis_np());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::MODELEVALUATOR::Meshtying::evaluate_stiff()
{
  return Strategy().evaluate_stiff(global_state().get_dis_np());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::apply_mesh_initialization(
    Teuchos::RCP<const Epetra_Vector> Xslavemod)
{
  // check modified positions vector
  if (Xslavemod == Teuchos::null) return;

  // create fully overlapping slave node map
  Teuchos::RCP<Epetra_Map> slavemap = strategy_ptr_->SlaveRowNodes();
  Teuchos::RCP<Epetra_Map> allreduceslavemap = Core::LinAlg::AllreduceEMap(*slavemap);

  // export modified node positions to column map of problem discretization
  const Epetra_Map* dof_colmap = discret_ptr()->DofColMap();
  const Epetra_Map* node_colmap = discret_ptr()->NodeColMap();
  Teuchos::RCP<Epetra_Vector> Xslavemodcol = Core::LinAlg::CreateVector(*dof_colmap, false);
  Core::LinAlg::Export(*Xslavemod, *Xslavemodcol);

  const int numnode = allreduceslavemap->NumMyElements();
  const int numdim = Global::Problem::Instance()->NDim();
  const Epetra_Vector& gvector = *Xslavemodcol;

  // loop over all slave nodes (for all procs)
  for (int index = 0; index < numnode; ++index)
  {
    int gid = allreduceslavemap->GID(index);

    // only do someting for nodes in my column map
    int ilid = node_colmap->LID(gid);
    if (ilid < 0) continue;

    Core::Nodes::Node* mynode = discret_ptr()->gNode(gid);

    // get degrees of freedom associated with this fluid/structure node
    std::vector<int> nodedofs = discret_ptr()->Dof(0, mynode);
    std::vector<double> nvector(3, 0.0);

    // create new position vector
    for (int i = 0; i < numdim; ++i)
    {
      const int lid = gvector.Map().LID(nodedofs[i]);

      if (lid < 0)
        FOUR_C_THROW("ERROR: Proc %d: Cannot find gid=%d in Epetra_Vector", gvector.Comm().MyPID(),
            nodedofs[i]);

      nvector[i] += gvector[lid];
    }

    // set new reference position
    mynode->SetPos(nvector);
  }

  // re-initialize finite elements
  Core::Communication::ParObjectFactory::Instance().initialize_elements(discret());
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::run_post_compute_x(
    const Epetra_Vector& xold, const Epetra_Vector& dir, const Epetra_Vector& xnew)
{
  check_init_setup();

  Strategy().run_post_compute_x(xold, dir, xnew);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::remove_condensed_contributions_from_rhs(Epetra_Vector& rhs)
{
  check_init_setup();

  Strategy().remove_condensed_contributions_from_rhs(rhs);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::write_restart(
    Core::IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  if (mesh_relocation_ != Teuchos::null)
    iowriter.write_vector("mesh_relocation", mesh_relocation_);
  else
  {
    Teuchos::RCP<Epetra_Vector> tmp =
        Teuchos::rcp(new Epetra_Vector(*discret().dof_row_map(), true));
    iowriter.write_vector("mesh_relocation", tmp);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::read_restart(Core::IO::DiscretizationReader& ioreader)
{
  mesh_relocation_ = Teuchos::rcp(new Epetra_Vector(*discret().dof_row_map(), true));
  ioreader.read_vector(mesh_relocation_, "mesh_relocation");

  strategy_ptr_->set_state(Mortar::state_new_displacement, *mesh_relocation_);
  strategy_ptr_->MortarCoupling(mesh_relocation_);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::Meshtying::set_time_integration_info(
    CONTACT::MtAbstractStrategy& strategy) const
{
  const Inpar::STR::DynamicType dyntype = tim_int().get_data_sdyn().get_dynamic_type();
  const double time_fac = integrator().get_int_param();

  strategy.set_time_integration_info(time_fac, dyntype);
}

FOUR_C_NAMESPACE_CLOSE
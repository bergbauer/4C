/*---------------------------------------------------------------------*/
/*! \file
\brief Nitsche poro contact solving strategy

\level 3


*/
/*---------------------------------------------------------------------*/

#include "4C_contact_nitsche_strategy_poro.hpp"

#include "4C_contact_interface.hpp"
#include "4C_contact_nitsche_utils.hpp"
#include "4C_contact_paramsinterface.hpp"
#include "4C_coupling_adapter.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_fem_general_extract_values.hpp"
#include "4C_global_data.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_so3_plast_ssn.hpp"

#include <Epetra_FEVector.h>
#include <Epetra_Operator.h>

FOUR_C_NAMESPACE_OPEN

void CONTACT::NitscheStrategyPoro::ApplyForceStiffCmt(Teuchos::RCP<Epetra_Vector> dis,
    Teuchos::RCP<Core::LinAlg::SparseOperator>& kt, Teuchos::RCP<Epetra_Vector>& f, const int step,
    const int iter, bool predictor)
{
  if (predictor) return;

  CONTACT::NitscheStrategy::ApplyForceStiffCmt(dis, kt, f, step, iter, predictor);

  // Evaluation for all interfaces
  fp_ = create_rhs_block_ptr(CONTACT::VecBlockType::porofluid);
  kpp_ = create_matrix_block_ptr(CONTACT::MatBlockType::porofluid_porofluid);
  kpd_ = create_matrix_block_ptr(CONTACT::MatBlockType::porofluid_displ);
  kdp_ = create_matrix_block_ptr(CONTACT::MatBlockType::displ_porofluid);
  //    for (int i = 0; i < (int) interface_.size(); ++i)
  //    {
  //      for (int e=0;e<interface_[i]->discret().ElementColMap()->NumMyElements();++e)
  //      {
  //        Mortar::Element* mele
  //        =dynamic_cast<Mortar::Element*>(interface_[i]->discret().gElement(
  //            interface_[i]->discret().ElementColMap()->GID(e)));
  //        mele->GetNitscheContainer().ClearAll();
  //      }
  //    }
}

void CONTACT::NitscheStrategyPoro::set_state(
    const enum Mortar::StateType& statename, const Epetra_Vector& vec)
{
  if (statename == Mortar::state_svelocity)
  {
    SetParentState(statename, vec);
  }
  else
    CONTACT::NitscheStrategy::set_state(statename, vec);
}

void CONTACT::NitscheStrategyPoro::SetParentState(
    const enum Mortar::StateType& statename, const Epetra_Vector& vec)
{
  //
  if (statename == Mortar::state_fvelocity || statename == Mortar::state_fpressure)
  {
    Teuchos::RCP<Core::FE::Discretization> dis = Global::Problem::Instance()->GetDis("porofluid");
    if (dis == Teuchos::null) FOUR_C_THROW("didn't get my discretization");

    Teuchos::RCP<Epetra_Vector> global = Teuchos::rcp(new Epetra_Vector(*dis->DofColMap(), true));
    Core::LinAlg::Export(vec, *global);


    // set state on interfaces
    for (const auto& interface : interface_)
    {
      Core::FE::Discretization& idiscret = interface->Discret();

      for (int j = 0; j < interface->Discret().ElementColMap()->NumMyElements(); ++j)
      {
        const int gid = interface->Discret().ElementColMap()->GID(j);

        auto* ele = dynamic_cast<Mortar::Element*>(idiscret.gElement(gid));

        std::vector<int> lm;
        std::vector<int> lmowner;
        std::vector<int> lmstride;

        if (ele->ParentSlaveElement())  // if this pointer is nullptr, this parent is impermeable
        {
          // this gets values in local order
          ele->ParentSlaveElement()->LocationVector(*dis, lm, lmowner, lmstride);

          std::vector<double> myval;
          Core::FE::ExtractMyValues(*global, myval, lm);

          std::vector<double> vel;
          std::vector<double> pres;

          for (int n = 0; n < ele->ParentSlaveElement()->num_node(); ++n)
          {
            for (unsigned dim = 0; dim < 3; ++dim)
            {
              vel.push_back(myval[n * 4 + dim]);
            }
            pres.push_back(myval[n * 4 + 3]);
          }

          ele->MoData().ParentPFPres() = pres;
          ele->MoData().ParentPFVel() = vel;
          ele->MoData().ParentPFDof() = lm;
        }
      }
    }
  }
  else
    CONTACT::NitscheStrategy::SetParentState(statename, vec);
}

Teuchos::RCP<Epetra_FEVector> CONTACT::NitscheStrategyPoro::setup_rhs_block_vec(
    const enum CONTACT::VecBlockType& bt) const
{
  switch (bt)
  {
    case CONTACT::VecBlockType::porofluid:
      return Teuchos::rcp(
          new Epetra_FEVector(*Global::Problem::Instance()->GetDis("porofluid")->dof_row_map()));
    default:
      return CONTACT::NitscheStrategy::setup_rhs_block_vec(bt);
  }
}

Teuchos::RCP<const Epetra_Vector> CONTACT::NitscheStrategyPoro::GetRhsBlockPtr(
    const enum CONTACT::VecBlockType& bp) const
{
  if (!curr_state_eval_) FOUR_C_THROW("you didn't evaluate this contact state first");

  switch (bp)
  {
    case CONTACT::VecBlockType::porofluid:
      return Teuchos::rcp(new Epetra_Vector(Copy, *(fp_), 0));
    default:
      return CONTACT::NitscheStrategy::GetRhsBlockPtr(bp);
  }
}

Teuchos::RCP<Core::LinAlg::SparseMatrix> CONTACT::NitscheStrategyPoro::setup_matrix_block_ptr(
    const enum CONTACT::MatBlockType& bt)
{
  switch (bt)
  {
    case CONTACT::MatBlockType::displ_porofluid:
      return Teuchos::rcp(new Core::LinAlg::SparseMatrix(
          *Teuchos::rcpFromRef<const Epetra_Map>(
              *Global::Problem::Instance()->GetDis("structure")->dof_row_map()),
          100, true, false, Core::LinAlg::SparseMatrix::FE_MATRIX));
    case CONTACT::MatBlockType::porofluid_displ:
    case CONTACT::MatBlockType::porofluid_porofluid:
      return Teuchos::rcp(new Core::LinAlg::SparseMatrix(
          *Teuchos::rcpFromRef<const Epetra_Map>(
              *Global::Problem::Instance()->GetDis("porofluid")->dof_row_map()),
          100, true, false, Core::LinAlg::SparseMatrix::FE_MATRIX));
    default:
      return CONTACT::NitscheStrategy::setup_matrix_block_ptr(bt);
  }
}

void CONTACT::NitscheStrategyPoro::complete_matrix_block_ptr(
    const enum CONTACT::MatBlockType& bt, Teuchos::RCP<Core::LinAlg::SparseMatrix> kc)
{
  switch (bt)
  {
    case CONTACT::MatBlockType::displ_porofluid:
      if (dynamic_cast<Epetra_FECrsMatrix&>(*kc->EpetraMatrix())
              .GlobalAssemble(
                  *Global::Problem::Instance()->GetDis("porofluid")->dof_row_map(),  // col map
                  *Global::Problem::Instance()->GetDis("structure")->dof_row_map(),  // row map
                  true, Add))
        FOUR_C_THROW("GlobalAssemble(...) failed");
      break;
    case CONTACT::MatBlockType::porofluid_displ:
      if (dynamic_cast<Epetra_FECrsMatrix&>(*kc->EpetraMatrix())
              .GlobalAssemble(
                  *Global::Problem::Instance()->GetDis("structure")->dof_row_map(),  // col map
                  *Global::Problem::Instance()->GetDis("porofluid")->dof_row_map(),  // row map
                  true, Add))
        FOUR_C_THROW("GlobalAssemble(...) failed");
      break;
    case CONTACT::MatBlockType::porofluid_porofluid:
      if (dynamic_cast<Epetra_FECrsMatrix&>(*kc->EpetraMatrix()).GlobalAssemble(true, Add))
        FOUR_C_THROW("GlobalAssemble(...) failed");
      break;
    default:
      CONTACT::NitscheStrategy::complete_matrix_block_ptr(bt, kc);
      break;
  }
}

Teuchos::RCP<Core::LinAlg::SparseMatrix> CONTACT::NitscheStrategyPoro::GetMatrixBlockPtr(
    const enum CONTACT::MatBlockType& bp) const
{
  if (!curr_state_eval_) FOUR_C_THROW("you didn't evaluate this contact state first");

  switch (bp)
  {
    case CONTACT::MatBlockType::porofluid_porofluid:
      return kpp_;
    case CONTACT::MatBlockType::porofluid_displ:
      return kpd_;
    case CONTACT::MatBlockType::displ_porofluid:
      return kdp_;
    default:
      return CONTACT::NitscheStrategy::GetMatrixBlockPtr(bp, nullptr);
  }
}

FOUR_C_NAMESPACE_CLOSE
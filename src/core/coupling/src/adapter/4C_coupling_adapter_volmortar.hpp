/*----------------------------------------------------------------------*/
/*! \file

\brief adapter for the volmortar framework

\level 2


*----------------------------------------------------------------------*/
#ifndef FOUR_C_COUPLING_ADAPTER_VOLMORTAR_HPP
#define FOUR_C_COUPLING_ADAPTER_VOLMORTAR_HPP

/*---------------------------------------------------------------------*
 | headers                                                 farah 10/13 |
 *---------------------------------------------------------------------*/
#include "4C_config.hpp"

#include "4C_coupling_adapter.hpp"
#include "4C_linalg_fixedsizematrix.hpp"

#include <Epetra_Comm.h>
#include <Epetra_Vector.h>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN

/*---------------------------------------------------------------------*
 | forward declarations                                    farah 10/13 |
 *---------------------------------------------------------------------*/
namespace Core::FE
{
  class Discretization;
}  // namespace Core::FE

namespace Core::Elements
{
  class Element;
}

namespace Core::LinAlg
{
  class SparseMatrix;
}

namespace Core::VolMortar
{
  namespace UTILS
  {
    class DefaultMaterialStrategy;
  }
}  // namespace Core::VolMortar

namespace Core::Adapter
{  /// Class for calling volmortar coupling and proper parallel redistr.
  class MortarVolCoupl : public Core::Adapter::CouplingBase
  {
   public:
    /*!
    \brief Empty constructor

    */
    MortarVolCoupl();

    /*!
    \brief Call parallel redistr. and evaluate volmortar coupl.

    */
    void Init(int spatial_dimension, Teuchos::RCP<Core::FE::Discretization> dis1,
        Teuchos::RCP<Core::FE::Discretization> dis2, std::vector<int>* coupleddof12 = nullptr,
        std::vector<int>* coupleddof21 = nullptr, std::pair<int, int>* dofsets12 = nullptr,
        std::pair<int, int>* dofsets21 = nullptr,
        Teuchos::RCP<VolMortar::UTILS::DefaultMaterialStrategy> materialstrategy = Teuchos::null,
        bool createauxdofs = true);

    /*!
    \brief Setup this class based on the @p params.

    */
    void Setup(const Teuchos::ParameterList& params);

    /*!
    \brief Redistribute discretizations to meet needs of volmortar coupling

    \note Call this method in your global control algorithm inbetween \ref Init()
          and \ref Setup(), in case you need parallel redistribution


    \date   09/16
    \author rauch

    */
    void Redistribute();


    /*!
    \brief Get coupling matrices for field 1 and 2

    */
    Teuchos::RCP<const Core::LinAlg::SparseMatrix> GetPMatrix12() const { return p12_; };
    Teuchos::RCP<const Core::LinAlg::SparseMatrix> GetPMatrix21() const { return p21_; };

    /*!
    \brief Mortar mapping for 1 to 2 and 2 to 1 - for vectors

    */
    Teuchos::RCP<const Epetra_Vector> apply_vector_mapping12(
        Teuchos::RCP<const Epetra_Vector> vec) const;
    Teuchos::RCP<const Epetra_Vector> apply_vector_mapping21(
        Teuchos::RCP<const Epetra_Vector> vec) const;

    /*!
    \brief Mortar mapping for 1 to 2 and 2 to 1 - for matrices

    */
    Teuchos::RCP<Core::LinAlg::SparseMatrix> apply_matrix_mapping12(
        Teuchos::RCP<const Core::LinAlg::SparseMatrix> mat) const;
    Teuchos::RCP<Core::LinAlg::SparseMatrix> apply_matrix_mapping21(
        Teuchos::RCP<const Core::LinAlg::SparseMatrix> mat) const;

    //@}

    //! assign materials
    void AssignMaterials(Teuchos::RCP<Core::FE::Discretization> dis1,
        Teuchos::RCP<Core::FE::Discretization> dis2, const Teuchos::ParameterList& volmortar_params,
        Teuchos::RCP<VolMortar::UTILS::DefaultMaterialStrategy> materialstrategy = Teuchos::null);


    /** \name Conversion between master and slave */
    //@{
    /// There are different versions to satisfy all needs. The basic
    /// idea is the same for all of them.

    /// transfer a dof vector from master to slave
    Teuchos::RCP<Epetra_Vector> MasterToSlave(Teuchos::RCP<Epetra_Vector> mv) const override
    {
      return MasterToSlave(Teuchos::rcp_static_cast<const Epetra_Vector>(mv));
    }

    /// transfer a dof vector from slave to master
    Teuchos::RCP<Epetra_Vector> SlaveToMaster(Teuchos::RCP<Epetra_Vector> sv) const override
    {
      return SlaveToMaster(Teuchos::rcp_static_cast<const Epetra_Vector>(sv));
    }

    /// transfer a dof vector from master to slave
    Teuchos::RCP<Epetra_MultiVector> MasterToSlave(
        Teuchos::RCP<Epetra_MultiVector> mv) const override
    {
      return MasterToSlave(Teuchos::rcp_static_cast<const Epetra_MultiVector>(mv));
    }

    /// transfer a dof vector from slave to master
    Teuchos::RCP<Epetra_MultiVector> SlaveToMaster(
        Teuchos::RCP<Epetra_MultiVector> sv) const override
    {
      return SlaveToMaster(Teuchos::rcp_static_cast<const Epetra_MultiVector>(sv));
    }

    /// transfer a dof vector from master to slave
    Teuchos::RCP<Epetra_Vector> MasterToSlave(Teuchos::RCP<const Epetra_Vector> mv) const override;

    /// transfer a dof vector from slave to master
    Teuchos::RCP<Epetra_Vector> SlaveToMaster(Teuchos::RCP<const Epetra_Vector> sv) const override;

    /// transfer a dof vector from master to slave
    Teuchos::RCP<Epetra_MultiVector> MasterToSlave(
        Teuchos::RCP<const Epetra_MultiVector> mv) const override;

    /// transfer a dof vector from slave to master
    Teuchos::RCP<Epetra_MultiVector> SlaveToMaster(
        Teuchos::RCP<const Epetra_MultiVector> sv) const override;

    /// transfer a dof vector from master to slave
    void MasterToSlave(Teuchos::RCP<const Epetra_MultiVector> mv,
        Teuchos::RCP<Epetra_MultiVector> sv) const override;

    /// transfer a dof vector from slave to master
    void SlaveToMaster(Teuchos::RCP<const Epetra_MultiVector> sv,
        Teuchos::RCP<Epetra_MultiVector> mv) const override;

    //@}

    /** \name Coupled maps */
    //@{

    /// the interface dof map of the master side
    Teuchos::RCP<const Epetra_Map> MasterDofMap() const override;

    /// the interface dof map of the slave side
    Teuchos::RCP<const Epetra_Map> SlaveDofMap() const override;

    //@}

   private:
    /*!
    \brief Create auxiliary dofsets for multiphysics if necessary

    */
    void create_aux_dofsets(Teuchos::RCP<Core::FE::Discretization> dis1,
        Teuchos::RCP<Core::FE::Discretization> dis2, std::vector<int>* coupleddof12,
        std::vector<int>* coupleddof21);

    /// check setup call
    const bool& is_setup() const { return issetup_; };

    /// check setup call
    const bool& is_init() const { return isinit_; };

    /// check init and setup call
    void check_setup() const
    {
      if (not is_setup()) FOUR_C_THROW("ERROR: Call Setup() first!");
    }

    /// check init and setup call
    void check_init() const
    {
      if (not is_init()) FOUR_C_THROW("ERROR: Call Init() first!");
    }

   private:
    bool issetup_;  ///< check for setup
    bool isinit_;   ///< check for init

    // mortar matrices and projector
    // s1 = P12 * s2
    // s2 = P21 * s1
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        p12_;  ///< global Mortar projection matrix P Omega_2 -> Omega_1
    Teuchos::RCP<Core::LinAlg::SparseMatrix>
        p21_;  ///< global Mortar projection matrix P Omega_1 -> Omega_2

    Teuchos::RCP<Core::FE::Discretization> masterdis_;
    Teuchos::RCP<Core::FE::Discretization> slavedis_;

    std::vector<int>* coupleddof12_;
    std::vector<int>* coupleddof21_;
    std::pair<int, int>* dofsets12_;
    std::pair<int, int>* dofsets21_;
    Teuchos::RCP<VolMortar::UTILS::DefaultMaterialStrategy> materialstrategy_;

    int spatial_dimension_{};
  };
}  // namespace Core::Adapter

FOUR_C_NAMESPACE_CLOSE

#endif
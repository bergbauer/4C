/*---------------------------------------------------------------------*/
/*! \file

\brief A class to perform integrations of nitsche related terms for the fpi contact case

\level 3


*/
/*---------------------------------------------------------------------*/
#ifndef FOUR_C_CONTACT_NITSCHE_INTEGRATOR_FPI_HPP
#define FOUR_C_CONTACT_NITSCHE_INTEGRATOR_FPI_HPP

#include "4C_config.hpp"

#include "4C_contact_nitsche_integrator_poro.hpp"
#include "4C_linalg_fixedsizematrix.hpp"

FOUR_C_NAMESPACE_OPEN

namespace XFEM
{
  class XFluidContactComm;
}

namespace CONTACT
{
  class Element;

  class IntegratorNitscheFpi : public IntegratorNitschePoro
  {
   public:
    /*!
     \brief Constructor  with shape function specification

     Constructs an instance of this class using a specific type of shape functions.<br>
     Note that this is \b not a collective call as overlaps are
     integrated in parallel by individual processes.<br>
     Note also that this constructor relies heavily on the
     Core::FE::IntegrationPoints structs to get Gauss points
     and corresponding weights.

     */
    IntegratorNitscheFpi(
        Teuchos::ParameterList& params, Core::FE::CellType eletype, const Epetra_Comm& comm);
    //! @name Derived functions
    //! @{

    //! @name currently unsupported derived methods
    //! @{
    void integrate_deriv_segment2_d(Mortar::Element& sele, double& sxia, double& sxib,
        Mortar::Element& mele, double& mxia, double& mxib, const Epetra_Comm& comm,
        const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr) override
    {
      FOUR_C_THROW("Segment based integration is currently unsupported!");
    }

    void IntegrateDerivEle2D(Mortar::Element& sele, std::vector<Mortar::Element*> meles,
        bool* boundary_ele, const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr) override
    {
      FOUR_C_THROW("Element based integration in 2D is currently unsupported!");
    }

    void integrate_deriv_cell3_d_aux_plane(Mortar::Element& sele, Mortar::Element& mele,
        Teuchos::RCP<Mortar::IntCell> cell, double* auxn, const Epetra_Comm& comm,
        const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr) override
    {
      FOUR_C_THROW("The auxiliary plane 3-D coupling integration case is currently unsupported!");
    }
    //! @}

    /*!
     \brief First, reevaluate which gausspoints should be used
     Second, Build all integrals and linearizations without segmentation -- 3D
     (i.e. M, g, LinM, Ling and possibly D, LinD)
     */
    void IntegrateDerivEle3D(Mortar::Element& sele, std::vector<Mortar::Element*> meles,
        bool* boundary_ele, bool* proj_, const Epetra_Comm& comm,
        const Teuchos::RCP<CONTACT::ParamsInterface>& cparams_ptr) override;

    //! @}

   protected:
    /*!
     \brief Perform integration at GP
            This is where the distinction between methods should be,
            i.e. mortar, augmented, gpts,...
     */
    void integrate_gp_3_d(Mortar::Element& sele, Mortar::Element& mele,
        Core::LinAlg::SerialDenseVector& sval, Core::LinAlg::SerialDenseVector& lmval,
        Core::LinAlg::SerialDenseVector& mval, Core::LinAlg::SerialDenseMatrix& sderiv,
        Core::LinAlg::SerialDenseMatrix& mderiv, Core::LinAlg::SerialDenseMatrix& lmderiv,
        Core::Gen::Pairedvector<int, Core::LinAlg::SerialDenseMatrix>& dualmap, double& wgt,
        double& jac, Core::Gen::Pairedvector<int, double>& derivjac, double* normal,
        std::vector<Core::Gen::Pairedvector<int, double>>& dnmap_unit, double& gap,
        Core::Gen::Pairedvector<int, double>& deriv_gap, double* sxi, double* mxi,
        std::vector<Core::Gen::Pairedvector<int, double>>& derivsxi,
        std::vector<Core::Gen::Pairedvector<int, double>>& derivmxi) override;

    /*!
     \brief Perform integration at GP
            This is where the distinction between methods should be,
            i.e. mortar, augmented, gpts,...
     */
    void integrate_gp_2_d(Mortar::Element& sele, Mortar::Element& mele,
        Core::LinAlg::SerialDenseVector& sval, Core::LinAlg::SerialDenseVector& lmval,
        Core::LinAlg::SerialDenseVector& mval, Core::LinAlg::SerialDenseMatrix& sderiv,
        Core::LinAlg::SerialDenseMatrix& mderiv, Core::LinAlg::SerialDenseMatrix& lmderiv,
        Core::Gen::Pairedvector<int, Core::LinAlg::SerialDenseMatrix>& dualmap, double& wgt,
        double& jac, Core::Gen::Pairedvector<int, double>& derivjac, double* normal,
        std::vector<Core::Gen::Pairedvector<int, double>>& dnmap_unit, double& gap,
        Core::Gen::Pairedvector<int, double>& deriv_gap, double* sxi, double* mxi,
        std::vector<Core::Gen::Pairedvector<int, double>>& derivsxi,
        std::vector<Core::Gen::Pairedvector<int, double>>& derivmxi) override
    {
      FOUR_C_THROW("2d problems not available for IntegratorNitscheFsi, as CutFEM is only 3D!");
    }

   private:
    /*!
    \brief evaluate GPTS forces and linearization at this gp
    */
    template <int dim>
    void gpts_forces(Mortar::Element& sele, Mortar::Element& mele,
        const Core::LinAlg::SerialDenseVector& sval, const Core::LinAlg::SerialDenseMatrix& sderiv,
        const std::vector<Core::Gen::Pairedvector<int, double>>& dsxi,
        const Core::LinAlg::SerialDenseVector& mval, const Core::LinAlg::SerialDenseMatrix& mderiv,
        const std::vector<Core::Gen::Pairedvector<int, double>>& dmxi, const double jac,
        const Core::Gen::Pairedvector<int, double>& jacintcellmap, const double wgt,
        const double gap, const Core::Gen::Pairedvector<int, double>& dgapgp, const double* gpn,
        std::vector<Core::Gen::Pairedvector<int, double>>& dnmap_unit, double* sxi, double* mxi);


    template <int dim>
    double get_normal_contact_transition(Mortar::Element& sele, Mortar::Element& mele,
        const Core::LinAlg::SerialDenseVector& sval, const Core::LinAlg::SerialDenseVector& mval,
        const double* sxi, const Core::LinAlg::Matrix<dim, 1>& pxsi,
        const Core::LinAlg::Matrix<dim, 1>& normal, bool& FSI_integrated, bool& gp_on_this_proc);

    /// Update Element contact state -2...not_specified, -1...no_contact, 0...mixed, 1...contact
    void update_ele_contact_state(Mortar::Element& sele, int state);

    /// Element contact state -2...not_specified, -1...no_contact, 0...mixed, 1...contact
    int ele_contact_state_;

    /// Xfluid Contact Communicator
    Teuchos::RCP<XFEM::XFluidContactComm> xf_c_comm_;
  };
}  // namespace CONTACT
FOUR_C_NAMESPACE_CLOSE

#endif
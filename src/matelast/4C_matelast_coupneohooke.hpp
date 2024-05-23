/*----------------------------------------------------------------------*/
/*! \file
\brief Definition of classes for a coupled Neo Hookean material

\level 1
*/
/*----------------------------------------------------------------------*/

#ifndef FOUR_C_MATELAST_COUPNEOHOOKE_HPP
#define FOUR_C_MATELAST_COUPNEOHOOKE_HPP

#include "4C_config.hpp"

#include "4C_matelast_summand.hpp"
#include "4C_material_parameter_base.hpp"

FOUR_C_NAMESPACE_OPEN

namespace MAT
{
  namespace ELASTIC
  {
    namespace PAR
    {
      /*!
       * @brief material parameters for isochoric contribution of a CoupNeoHookean material
       *
       * <h3>Input line</h3>
       * MAT 1 ELAST_CoupNeoHooke YOUNG 1 NUE 1
       */
      class CoupNeoHooke : public CORE::MAT::PAR::Parameter
      {
       public:
        /// standard constructor
        CoupNeoHooke(const Teuchos::RCP<CORE::MAT::PAR::Material>& matdata);

        /// @name material parameters
        //@{

        /// Young's modulus
        double youngs_;
        /// Possion's ratio
        double nue_;

        /// nue \(1-2 nue)
        double beta_;
        /// Shear modulus / 2
        double c_;

        //@}

        /// Override this method and throw error, as the material should be created in within the
        /// Factory method of the elastic summand
        Teuchos::RCP<CORE::MAT::Material> CreateMaterial() override
        {
          FOUR_C_THROW(
              "Cannot create a material from this method, as it should be created in "
              "MAT::ELASTIC::Summand::Factory.");
          return Teuchos::null;
        };
      };  // class CoupNeoHooke

    }  // namespace PAR

    /*!
     * @brief CoupNeoHookean material
     *
     * This is the summand of a hyperelastic, isotropic CoupNeoHookean material depending on the
     * first and the third invariant of the right Cauchy-Green tensor. The formulation is based on
     * [1] p. 247,248 and 263
     *
     * The implemented material is the coupled form of the compressible NeoHooke model. The
     * Parameters read in are the Young's modulus and the Poisson's ratio.
     *
     * Strain energy function is given by
     * \f[
     *   \Psi = c(I_{\boldsymbol{C}}-3)+\frac {c}{\beta} (J^{-2\beta}-1)
     * \f]
     *
     * with
     * \f[
     *   \beta = \frac {\nu}{1-2\nu}
     * \f]
     *
     * \f$ c=\frac {\mu}{2} = \frac {\text{Young's modulus}}{4(1+\nu)} \f$ and \f$\mu\f$ and
     * \f$\nu\f$ denoting the shear modulus and the Poisson's ratio, respectively.
     *
     * [1] Holzapfel, G. A., Nonlinear Solid Mechanics, 2002
     */
    class CoupNeoHooke : public Summand
    {
     public:
      /// constructor with given material parameters
      CoupNeoHooke(MAT::ELASTIC::PAR::CoupNeoHooke* params);


      /// @name Access material constants
      //@{

      /// material type
      CORE::Materials::MaterialType MaterialType() const override
      {
        return CORE::Materials::mes_coupneohooke;
      }

      /// add shear modulus equivalent
      void AddShearMod(bool& haveshearmod,  ///< non-zero shear modulus was added
          double& shearmod                  ///< variable to add upon
      ) const override;

      /// add young's modulus equivalent
      void AddYoungsMod(double& young, double& shear, double& bulk) override { young += YOUNGS(); };

      //@}

      // add strain energy
      void AddStrainEnergy(double& psi,  ///< strain energy function
          const CORE::LINALG::Matrix<3, 1>&
              prinv,  ///< principal invariants of right Cauchy-Green tensor
          const CORE::LINALG::Matrix<3, 1>&
              modinv,  ///< modified invariants of right Cauchy-Green tensor
          const CORE::LINALG::Matrix<6, 1>& glstrain,  ///< Green-Lagrange strain
          int gp,                                      ///< Gauss point
          int eleGID                                   ///< element GID
          ) override;

      void add_derivatives_principal(
          CORE::LINALG::Matrix<3, 1>& dPI,    ///< first derivative with respect to invariants
          CORE::LINALG::Matrix<6, 1>& ddPII,  ///< second derivative with respect to invariants
          const CORE::LINALG::Matrix<3, 1>&
              prinv,  ///< principal invariants of right Cauchy-Green tensor
          int gp,     ///< Gauss point
          int eleGID  ///< element GID
          ) override;

      void add_third_derivatives_principal_iso(
          CORE::LINALG::Matrix<10, 1>&
              dddPIII_iso,  ///< third derivative with respect to invariants
          const CORE::LINALG::Matrix<3, 1>& prinv_iso,  ///< principal isotropic invariants
          int gp,                                       ///< Gauss point
          int eleGID) override;                         ///< element GID

      /// add the derivatives of a coupled strain energy functions associated with a purely
      /// isochoric deformation
      void AddCoupDerivVol(
          const double j, double* dPj1, double* dPj2, double* dPj3, double* dPj4) override;

      /// @name Access methods
      //@{
      double NUE() const { return params_->nue_; }
      double YOUNGS() const { return params_->youngs_; }
      //@}

      /// Indicator for formulation
      void SpecifyFormulation(
          bool& isoprinc,     ///< global indicator for isotropic principal formulation
          bool& isomod,       ///< global indicator for isotropic splitted formulation
          bool& anisoprinc,   ///< global indicator for anisotropic principal formulation
          bool& anisomod,     ///< global indicator for anisotropic splitted formulation
          bool& viscogeneral  ///< global indicator, if one viscoelastic formulation is used
          ) override
      {
        isoprinc = true;
        return;
      };

     private:
      /// my material parameters
      MAT::ELASTIC::PAR::CoupNeoHooke* params_;
    };

  }  // namespace ELASTIC
}  // namespace MAT

FOUR_C_NAMESPACE_CLOSE

#endif

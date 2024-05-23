/*----------------------------------------------------------------------*/
/*! \file
\brief Declaration of a remodel constituent with implicit update rule
\level 3
*/
/*----------------------------------------------------------------------*/

#ifndef FOUR_C_MIXTURE_CONSTITUENT_REMODELFIBER_IMPL_HPP
#define FOUR_C_MIXTURE_CONSTITUENT_REMODELFIBER_IMPL_HPP

#include "4C_config.hpp"

#include "4C_mat_anisotropy_extension_default.hpp"
#include "4C_material_input_base.hpp"
#include "4C_mixture_constituent.hpp"
#include "4C_mixture_constituent_remodelfiber_material.hpp"
#include "4C_mixture_remodelfiber.hpp"

#include <Teuchos_RCPDecl.hpp>

#include <cmath>

FOUR_C_NAMESPACE_OPEN

namespace MIXTURE
{
  class MixtureConstituent;
  template <typename T>
  class RemodelFiberMaterial;

  namespace PAR
  {
    class MixtureConstituentRemodelFiberImpl : public MIXTURE::PAR::MixtureConstituent
    {
     public:
      explicit MixtureConstituentRemodelFiberImpl(
          const Teuchos::RCP<CORE::MAT::PAR::Material>& matdata);
      /// create material instance of matching type with my parameters
      std::unique_ptr<MIXTURE::MixtureConstituent> CreateConstituent(int id) override;

      const int fiber_id_;
      const int init_;

      const int fiber_material_id_;
      const MIXTURE::PAR::RemodelFiberMaterial<double>* fiber_material_;

      const bool growth_enabled_;
      const double poisson_decay_time_;
      const double growth_constant_;

      const double deposition_stretch_;
      const int deposition_stretch_timefunc_num_;
    };
  }  // namespace PAR

  /*!
   * \brief Remodel fiber constituent with an implicit update rule
   */
  class MixtureConstituentRemodelFiberImpl : public MIXTURE::MixtureConstituent
  {
   public:
    explicit MixtureConstituentRemodelFiberImpl(
        MIXTURE::PAR::MixtureConstituentRemodelFiberImpl* params, int id);

    /// Returns the material type enum
    [[nodiscard]] CORE::Materials::MaterialType MaterialType() const override;

    void PackConstituent(CORE::COMM::PackBuffer& data) const override;

    void UnpackConstituent(
        std::vector<char>::size_type& position, const std::vector<char>& data) override;

    void register_anisotropy_extensions(MAT::Anisotropy& anisotropy) override;

    void ReadElement(int numgp, INPUT::LineDefinition* linedef) override;

    void Setup(Teuchos::ParameterList& params, int eleGID) override;

    void Update(const CORE::LINALG::Matrix<3, 3>& F, Teuchos::ParameterList& params, int gp,
        int eleGID) override;

    void Evaluate(const CORE::LINALG::Matrix<3, 3>& F, const CORE::LINALG::Matrix<6, 1>& E_strain,
        Teuchos::ParameterList& params, CORE::LINALG::Matrix<6, 1>& S_stress,
        CORE::LINALG::Matrix<6, 6>& cmat, int gp, int eleGID) override;

    void EvaluateElasticPart(const CORE::LINALG::Matrix<3, 3>& FM,
        const CORE::LINALG::Matrix<3, 3>& iFextin, Teuchos::ParameterList& params,
        CORE::LINALG::Matrix<6, 1>& S_stress, CORE::LINALG::Matrix<6, 6>& cmat, int gp,
        int eleGID) override;

    [[nodiscard]] double GetGrowthScalar(int gp) const override;
    [[nodiscard]] CORE::LINALG::Matrix<1, 6> GetDGrowthScalarDC(int gp, int eleGID) const override;

    void register_output_data_names(
        std::unordered_map<std::string, int>& names_and_size) const override;

    bool EvaluateOutputData(
        const std::string& name, CORE::LINALG::SerialDenseMatrix& data) const override;

   private:
    void integrate_local_evolution_equations(double dt, int gp, int eleGID);
    [[nodiscard]] double EvaluateLambdaf(
        const CORE::LINALG::Matrix<3, 3>& C, int gp, int eleGID) const;
    [[nodiscard]] CORE::LINALG::Matrix<1, 6> evaluate_d_lambdafsq_dc(int gp, int eleGID) const;

    [[nodiscard]] CORE::LINALG::Matrix<6, 1> EvaluateCurrentPK2(int gp, int eleGID) const;
    [[nodiscard]] CORE::LINALG::Matrix<6, 6> EvaluateCurrentCmat(int gp, int eleGID) const;

    [[nodiscard]] double evaluate_deposition_stretch(double time) const;
    void update_homeostatic_values(const Teuchos::ParameterList& params, int eleGID);

    void Initialize();

    /// my material parameters
    MIXTURE::PAR::MixtureConstituentRemodelFiberImpl* params_;

    /// An instance of the remodel fiber
    std::vector<RemodelFiber<2>> remodel_fiber_;

    /// Handler for anisotropic input
    MAT::DefaultAnisotropyExtension<1> anisotropy_extension_;

    /// temporary variables used within one evaluation
    /// @{
    std::vector<CORE::LINALG::Matrix<1, 6>> dgrowthscalard_c_;
    std::vector<CORE::LINALG::Matrix<1, 6>> dlambdard_c_;
    /// @}
  };
}  // namespace MIXTURE

FOUR_C_NAMESPACE_CLOSE

#endif

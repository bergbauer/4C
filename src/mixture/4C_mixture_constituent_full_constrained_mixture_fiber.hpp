/*----------------------------------------------------------------------*/
/*! \file
\brief Declaration of a remodel constituent with implicit update rule
\level 3
*/
/*----------------------------------------------------------------------*/

#ifndef FOUR_C_MIXTURE_CONSTITUENT_FULL_CONSTRAINED_MIXTURE_FIBER_HPP
#define FOUR_C_MIXTURE_CONSTITUENT_FULL_CONSTRAINED_MIXTURE_FIBER_HPP

#include "4C_config.hpp"

#include "4C_mat_anisotropy_extension_default.hpp"
#include "4C_material_parameter_base.hpp"
#include "4C_mixture_constituent.hpp"
#include "4C_mixture_constituent_remodelfiber_material.hpp"
#include "4C_mixture_full_constrained_mixture_fiber.hpp"

#include <Teuchos_RCP.hpp>

#include <cmath>

FOUR_C_NAMESPACE_OPEN

namespace MIXTURE
{
  class MixtureConstituent;
  template <typename T>
  class FullConstrainedMixtureFiber;

  namespace PAR
  {
    class MixtureConstituentFullConstrainedMixtureFiber : public MIXTURE::PAR::MixtureConstituent
    {
     public:
      explicit MixtureConstituentFullConstrainedMixtureFiber(
          const Core::Mat::PAR::Parameter::Data& matdata);
      /// create material instance of matching type with my parameters
      std::unique_ptr<MIXTURE::MixtureConstituent> create_constituent(int id) override;

      const int fiber_id_;
      const int init_;

      const int fiber_material_id_;
      const MIXTURE::PAR::RemodelFiberMaterial<double>* fiber_material_;

      const bool growth_enabled_;
      const double poisson_decay_time_;
      const double growth_constant_;

      const double deposition_stretch_;
      const int initial_deposition_stretch_timefunc_num_;

      const HistoryAdaptionStrategy adaptive_history_strategy_;
      const double adaptive_history_tolerance_;
    };
  }  // namespace PAR

  /*!
   * \brief Full constrained mixture fiber constituent
   */
  class MixtureConstituentFullConstrainedMixtureFiber : public MIXTURE::MixtureConstituent
  {
   public:
    MixtureConstituentFullConstrainedMixtureFiber(
        MIXTURE::PAR::MixtureConstituentFullConstrainedMixtureFiber* params, int id);

    [[nodiscard]] Core::Materials::MaterialType material_type() const override;

    void pack_constituent(Core::Communication::PackBuffer& data) const override;

    void unpack_constituent(
        std::vector<char>::size_type& position, const std::vector<char>& data) override;

    void register_anisotropy_extensions(Mat::Anisotropy& anisotropy) override;

    void read_element(int numgp, const Core::IO::InputParameterContainer& container) override;

    void setup(Teuchos::ParameterList& params, int eleGID) override;

    void update(const Core::LinAlg::Matrix<3, 3>& F, Teuchos::ParameterList& params, int gp,
        int eleGID) override;

    void evaluate(const Core::LinAlg::Matrix<3, 3>& F, const Core::LinAlg::Matrix<6, 1>& E_strain,
        Teuchos::ParameterList& params, Core::LinAlg::Matrix<6, 1>& S_stress,
        Core::LinAlg::Matrix<6, 6>& cmat, int gp, int eleGID) override;

    void evaluate_elastic_part(const Core::LinAlg::Matrix<3, 3>& FM,
        const Core::LinAlg::Matrix<3, 3>& iFextin, Teuchos::ParameterList& params,
        Core::LinAlg::Matrix<6, 1>& S_stress, Core::LinAlg::Matrix<6, 6>& cmat, int gp,
        int eleGID) override;

    [[nodiscard]] double get_growth_scalar(int gp) const override;
    [[nodiscard]] Core::LinAlg::Matrix<1, 6> get_d_growth_scalar_d_cg(
        int gp, int eleGID) const override;

    void register_output_data_names(
        std::unordered_map<std::string, int>& names_and_size) const override;

    bool evaluate_output_data(
        const std::string& name, Core::LinAlg::SerialDenseMatrix& data) const override;

   private:
    [[nodiscard]] double evaluate_lambdaf(
        const Core::LinAlg::Matrix<3, 3>& C, int gp, int eleGID) const;
    [[nodiscard]] Core::LinAlg::Matrix<1, 6> evaluate_d_lambdafsq_dc(int gp, int eleGID) const;

    [[nodiscard]] Core::LinAlg::Matrix<6, 1> evaluate_current_p_k2(int gp, int eleGID) const;
    [[nodiscard]] Core::LinAlg::Matrix<6, 6> evaluate_current_cmat(int gp, int eleGID) const;
    [[nodiscard]] double evaluate_initial_deposition_stretch(double time) const;

    void initialize();

    /// my material parameters
    MIXTURE::PAR::MixtureConstituentFullConstrainedMixtureFiber* params_;

    /// An instance of the full constrained mixture fiber for each Gauss point
    std::vector<FullConstrainedMixtureFiber<double>> full_constrained_mixture_fiber_;

    /// Store the last converged lambda_f for initializing history of full constrained mixture model
    std::vector<double> last_lambda_f_;

    /// Handler for anisotropic input
    Mat::DefaultAnisotropyExtension<1> anisotropy_extension_;
  };
}  // namespace MIXTURE

FOUR_C_NAMESPACE_CLOSE

#endif

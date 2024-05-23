/*----------------------------------------------------------------------*/
/*! \file

\brief Definition of a hyperelastic constituent basis

\level 3


*/
/*----------------------------------------------------------------------*/

#ifndef FOUR_C_MIXTURE_CONSTITUENT_ELASTHYPERBASE_HPP
#define FOUR_C_MIXTURE_CONSTITUENT_ELASTHYPERBASE_HPP

#include "4C_config.hpp"

#include "4C_mat_anisotropy_extension_cylinder_cosy.hpp"
#include "4C_mat_elasthyper_service.hpp"
#include "4C_material_parameter_base.hpp"
#include "4C_mixture_constituent.hpp"
#include "4C_mixture_prestress_strategy.hpp"

FOUR_C_NAMESPACE_OPEN

namespace MAT
{
  class Anisotropy;
}

namespace MIXTURE
{
  class MixtureConstituentElastHyperBase;

  namespace PAR
  {
    class MixtureConstituentElastHyperBase : public MIXTURE::PAR::MixtureConstituent
    {
     public:
      explicit MixtureConstituentElastHyperBase(
          const Teuchos::RCP<CORE::MAT::PAR::Material>& matdata);
      int get_prestressing_mat_id() const { return matid_prestress_strategy_; }

      /// @name material parameters
      /// @{

      /// Material id of the prestress strategy
      const int matid_prestress_strategy_;

      /// number of summands
      const int nummat_;

      /// List of material ids of the summands
      const std::vector<int> matids_;
      /// @}
    };
  }  // namespace PAR

  /*!
   * \brief Constituent for any hyperelastic material
   *
   * This constituent represents any hyperelastic material from the elasthyper toolbox. It has to
   * be paired with the MAT::Mixture material and a MIXTURE::MixtureRule.
   */
  class MixtureConstituentElastHyperBase : public MIXTURE::MixtureConstituent
  {
   public:
    /// Constructor for the material given the material parameters
    explicit MixtureConstituentElastHyperBase(
        MIXTURE::PAR::MixtureConstituentElastHyperBase* params, int id);

    /*!
     * \brief Pack data into a char vector from this class
     *
     * The vector data contains all information to rebuild the exact copy of an instance of a class
     * on a different processor. The first entry in data hast to be an integer which is the unique
     * parobject id defined at the top of the file and delivered by UniqueParObjectId().
     *
     * @param data (in/put) : vector storing all data to be packed into this instance.
     */
    void PackConstituent(CORE::COMM::PackBuffer& data) const override;

    /*!
     * \brief Unpack data from a char vector into this class to be called from a derived class
     *
     * The vector data contains all information to rebuild the exact copy of an instance of a class
     * on a different processor. The first entry in data hast to be an integer which is the unique
     * parobject id defined at the top of the file and delivered by UniqueParObjectId().
     *
     * @param position (in/out) : current position to unpack data
     * @param data (in) : vector storing all data to be unpacked into this instance.
     */
    void UnpackConstituent(
        std::vector<char>::size_type& position, const std::vector<char>& data) override;

    /*!
     * \brief Register all anisotropy extensions also for the sub-summands
     *
     * \param anisotropy Reference to the global anisotropy manager
     */
    void register_anisotropy_extensions(MAT::Anisotropy& anisotropy) override;

    /*!
     * Initialize the constituent with the parameters of the input line
     *
     * @param numgp (in) Number of Gauss-points
     * @param params (in/out) Parameter list for exchange of parameters
     */
    void ReadElement(int numgp, INPUT::LineDefinition* linedef) override;

    /*!
     * \brief Updates the material and all its summands
     *
     * This method is called once between each timestep after convergence.
     *
     * @param defgrd Deformation gradient
     * @param params Container for additional information
     * @param gp Gauss point
     * @param eleGID Global element identifier
     */
    void Update(CORE::LINALG::Matrix<3, 3> const& defgrd, Teuchos::ParameterList& params, int gp,
        int eleGID) override;

    /*!
     * \brief Returns a reference to all summands
     *
     * \return const std::vector<Teuchos::RCP<MAT::ELASTIC::Summand>>& Reference to the summands
     */
    const std::vector<Teuchos::RCP<MAT::ELASTIC::Summand>>& Summands() const { return potsum_; }

    /*!
     * \brief Returns a reference to all summand properties
     *
     * \return const MAT::SummandProperties& Reference to the summand properties
     */
    const MAT::SummandProperties& SummandProperties() { return summand_properties_; }

    /*!
     * \brief Method that is called to setup the constituent once before the start of the simulation
     *
     * \param params Container for additional information
     * \param eleGID Global element id
     */
    void Setup(Teuchos::ParameterList& params, int eleGID) override;

    /*!
     * \brief Method that is called once for each Gauss point before the first evaluate call
     *
     * \param mixtureRule Reference to the mixture rule
     * \param params Container for additional information
     * \param gp Gauss point
     * \param eleGID Global element id
     */
    void PreEvaluate(
        MixtureRule& mixtureRule, Teuchos::ParameterList& params, int gp, int eleGID) override;

    void register_output_data_names(
        std::unordered_map<std::string, int>& names_and_size) const override;

    bool EvaluateOutputData(
        const std::string& name, CORE::LINALG::SerialDenseMatrix& data) const override;

   protected:
    /*!
     * \brief Returns a reference to the prestretch tensor at the Gauss point
     *
     * \param gp Gauss point
     * \return const CORE::LINALG::Matrix<3, 3>& Reference to the prestretch tensor
     */
    const CORE::LINALG::Matrix<3, 3>& PrestretchTensor(const int gp) const
    {
      return prestretch_[gp];
    }

    /*!
     * \brief Returns a reference to the cylinder coordinate system
     *
     * \return const MAT::CylinderCoordinateSystemProvider&
     */
    const MAT::CylinderCoordinateSystemAnisotropyExtension&
    cylinder_coordinate_system_anisotropy_extension() const
    {
      return cosy_anisotropy_extension_;
    }

    std::shared_ptr<MIXTURE::PrestressStrategy> PrestressStrategy() { return prestress_strategy_; }

   private:
    /// @name Flags to specify the elastic formulations (initialize with false)
    //@{
    MAT::SummandProperties summand_properties_;  ///< holder for formulation specification
    //@}

    /// my material parameters
    MIXTURE::PAR::MixtureConstituentElastHyperBase* params_;

    /// map to materials/potential summands
    std::vector<Teuchos::RCP<MAT::ELASTIC::Summand>> potsum_;

    /// Prestretch of the constituent
    std::vector<CORE::LINALG::Matrix<3, 3>> prestretch_;

    /// AnisotropyExtension that handles the management of cylinder coordinate systems
    MAT::CylinderCoordinateSystemAnisotropyExtension cosy_anisotropy_extension_;

    /// Strategy for prestressing the constituent
    std::shared_ptr<MIXTURE::PrestressStrategy> prestress_strategy_;
  };

}  // namespace MIXTURE

FOUR_C_NAMESPACE_CLOSE

#endif

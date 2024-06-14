/*----------------------------------------------------------------------*/
/*! \file
\brief Base object to hold 'quick' access to material parameters

\level 1

*/

/*----------------------------------------------------------------------*/

#ifndef FOUR_C_MATERIAL_PARAMETER_BASE_HPP
#define FOUR_C_MATERIAL_PARAMETER_BASE_HPP


/*----------------------------------------------------------------------*/
/* headers */
#include "4C_config.hpp"

#include "4C_io_input_parameter_container.hpp"
#include "4C_legacy_enum_definitions_materials.hpp"

#include <Epetra_Vector.h>
#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/* forward declarations */
namespace Core::Mat
{
  class Material;
  namespace PAR
  {
    class Parameter;
  }
}  // namespace Core::Mat

/*----------------------------------------------------------------------*/
/* declarations */

namespace Core::Mat::PAR
{
  /// Legacy container for read-in materials
  ///
  /// This object stores the validated material parameters as
  /// Core::IO::InputParameterContainer.
  class Material : public Core::IO::InputParameterContainer
  {
   public:
    /// @name life span
    //@{

    /// Standard constructor
    Material(const int id,                        ///< unique material ID
        const Core::Materials::MaterialType type  ///< type of material
    );

    /// Copy the input_data into this object.
    Material(int id, Core::Materials::MaterialType type,
        const Core::IO::InputParameterContainer& input_data);

    /// Default constructor without information from the input lines.
    Material() = default;

    //@}

    /// @name Query methods
    //@{

    /// Return material id
    [[nodiscard]] inline virtual int Id() const { return id_; }

    /// Return type of material
    [[nodiscard]] inline virtual Core::Materials::MaterialType Type() const { return type_; }

    //@}

    /// don't want = operator
    Material operator=(const Material& old) = delete;
    Material(const Core::Mat::PAR::Material& old) = delete;

   protected:
    /// Unique ID of this material, no second material of same ID may exist
    int id_{};

    /// Type of this material
    Core::Materials::MaterialType type_{};
  };

  /*----------------------------------------------------------------------*/
  /// Base object to hold 'quick' access material parameters
  ///
  /// Core::Mat::PAR::Parameters is derived for the various implemented
  /// materials. These provide the 'quick' access to the read-in
  /// material parameters.
  ///
  /// For every read-in material will exist a single instance (of
  /// a derived class) of this object.
  class Parameter
  {
   public:
    /// construct the material object given material parameters
    Parameter(Teuchos::RCP<const Core::Mat::PAR::Material>
            matdata  ///< read and validated material data (of 'slow' access)
    );

    /// destructor
    virtual ~Parameter() = default;

    /// (unique) material ID
    [[nodiscard]] int Id() const { return id_; }

    /// material type
    [[nodiscard]] Core::Materials::MaterialType Type() const { return type_; }

    /// create material instance of matching type with my parameters
    virtual Teuchos::RCP<Core::Mat::Material> create_material() = 0;

    //! \brief return element specific or global material parameter using enum parametername which
    //! is defined in respective Mat::PAR classes
    double GetParameter(int parametername, const int EleId);

    /**
     * Access to the raw input data.
     */
    const Core::IO::InputParameterContainer& raw_parameters() const { return *raw_parameters_; }

   protected:
    /*! \brief
     * data structure to store all material parameters in.
     * By default all elements with the same mat share the same material properties, hence the
     * Epetra_Vector has length 1 However for elementwise material properties the Epetra_Vector
     * has EleColMap layout.
     */
    std::vector<Teuchos::RCP<Epetra_Vector>> matparams_;

   private:
    /// material ID, as defined in input file
    int id_;

    /// material type
    Core::Materials::MaterialType type_;

    /// raw input parameters
    Teuchos::RCP<const Core::Mat::PAR::Material> raw_parameters_;

  };  // class Parameter

}  // namespace Core::Mat::PAR


FOUR_C_NAMESPACE_CLOSE

#endif
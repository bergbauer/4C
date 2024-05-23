/*----------------------------------------------------------------------*/
/*! \file

\brief singleton class holding all static levelset reinitialization parameters required for element
evaluation

This singleton class holds all static levelset reinitialization parameters required for element
evaluation. All parameters are usually set only once at the beginning of a simulation, namely during
initialization of the global time integrator, and then never touched again throughout the
simulation. This parameter class needs to coexist with the general parameter class holding all
general static parameters required for scalar transport element evaluation.


\level 2
*/
/*----------------------------------------------------------------------*/
#ifndef FOUR_C_SCATRA_ELE_PARAMETER_LSREINIT_HPP
#define FOUR_C_SCATRA_ELE_PARAMETER_LSREINIT_HPP

#include "4C_config.hpp"

#include "4C_inpar_levelset.hpp"
#include "4C_scatra_ele_parameter_base.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

namespace DRT
{
  namespace ELEMENTS
  {
    class ScaTraEleParameterLsReinit : public ScaTraEleParameterBase
    {
     protected:
      /// (private) protected constructor, since we are a Singleton.
      /// this constructor is called from a derived class
      /// -> therefore, it has to be protected instead of private
      ScaTraEleParameterLsReinit(const std::string& disname  //!< name of discretization
      );

     public:
      /// Singleton access method
      static ScaTraEleParameterLsReinit* Instance(
          const std::string& disname  //!< name of discretization
      );

      /*========================================================================*/
      //! @name set-routines
      /*========================================================================*/

      //! set parameters
      void SetParameters(Teuchos::ParameterList& parameters  //!< parameter list
          ) override;

      /*========================================================================*/
      //! @name access-routines
      /*========================================================================*/

      INPAR::SCATRA::ReInitialAction ReinitType() const { return reinittype_; };
      INPAR::SCATRA::SmoothedSignType SignType() const { return signtype_; };
      INPAR::SCATRA::CharEleLengthReinit CharEleLengthReinit() const
      {
        return charelelengthreinit_;
      };
      double interface_thickness_fac() const { return interfacethicknessfac_; };
      bool UseProjectedVel() const { return useprojectedreinitvel_; };
      INPAR::SCATRA::LinReinit LinForm() const { return linform_; };
      INPAR::SCATRA::ArtDiff ArtDiff() const { return artdiff_; };

      /// access the penalty parameter for the elliptical reinitialization
      double PenaltyPara() const
      {
        FOUR_C_ASSERT(alphapen_ > 0.0, "The penalty parameter have to be larger than zero!");
        return alphapen_;
      };

      bool Project() const { return project_; };
      bool Lumping() const { return lumping_; };
      double ProjectDiff() const { return projectdiff_; };
      INPAR::SCATRA::DiffFunc DiffFct() const { return difffct_; };

     private:
      // reinit type
      INPAR::SCATRA::ReInitialAction reinittype_;
      // sign function for phi
      INPAR::SCATRA::SmoothedSignType signtype_;
      // element length for smoothing of sign function
      INPAR::SCATRA::CharEleLengthReinit charelelengthreinit_;
      // interface thickness factor (multiple of characteristic element length)
      double interfacethicknessfac_;
      // from of velocity evaluation
      bool useprojectedreinitvel_;
      // form of linearization of nonlinear terms
      INPAR::SCATRA::LinReinit linform_;
      // form of artificial diffusion
      INPAR::SCATRA::ArtDiff artdiff_;
      // penalty parameter of elliptic reinitialization
      double alphapen_;
      // use L2 projection
      bool project_;
      // diffusion for L2 projection
      double projectdiff_;
      // mass lumping for L2 projection
      bool lumping_;
      // function for diffusivity
      INPAR::SCATRA::DiffFunc difffct_;
    };
  }  // namespace ELEMENTS
}  // namespace DRT
FOUR_C_NAMESPACE_CLOSE

#endif

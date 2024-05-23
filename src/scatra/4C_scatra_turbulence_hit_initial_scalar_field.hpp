/*----------------------------------------------------------------------*/
/*! \file


\brief routines to initialize homogeneous isotropic turbulence simulations with passive scalar
transport

\level 2


*----------------------------------------------------------------------*/


#ifndef FOUR_C_SCATRA_TURBULENCE_HIT_INITIAL_SCALAR_FIELD_HPP
#define FOUR_C_SCATRA_TURBULENCE_HIT_INITIAL_SCALAR_FIELD_HPP

#include "4C_config.hpp"

#include "4C_inpar_scatra.hpp"

#include <Epetra_Vector.h>
#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN

// forward declarations
namespace DRT
{
  class Discretization;
}

namespace SCATRA
{
  // forward declarations
  class ScaTraTimIntImpl;

  // inital condition for homogeneous isotropic turbulence
  // based on the Comte-Bellot - Corrsion experiment
  class HomIsoTurbInitialScalarField
  {
   public:
    //! constructor
    HomIsoTurbInitialScalarField(
        ScaTraTimIntImpl& timeint, const INPAR::SCATRA::InitialField initfield);

    //! calculate initial field
    void calculate_initial_field();

   protected:
    //! sort criterium for double values up to a tolerance of 10-9
    class LineSortCriterion
    {
     public:
      bool operator()(const double& p1, const double& p2) const { return (p1 < p2 - 1E-9); }

     protected:
     private:
    };

   private:
    //! estimate energy form given scalar variance spectrum (function for E_phi)
    double calculate_energy_from_spectrum(double k);

    //! scatra discretization
    Teuchos::RCP<DRT::Discretization> discret_;

    //! state vectors to be initialized
    Teuchos::RCP<Epetra_Vector> phinp_;
    Teuchos::RCP<Epetra_Vector> phin_;

    //! type of energy spectrum for initialization
    INPAR::SCATRA::InitialField type_;

    //! number of resolved mode
    int nummodes_;

    //! vector of coordinates in one spatial direction (same for the other two directions)
    Teuchos::RCP<std::vector<double>> coordinates_;
  };

}  // namespace SCATRA

FOUR_C_NAMESPACE_CLOSE

#endif

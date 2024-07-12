/*----------------------------------------------------------------------*/
/*! \file

\brief Basis of all ELCH algorithms

\level 2

*/
/*----------------------------------------------------------------------*/


#ifndef FOUR_C_ELCH_ALGORITHM_HPP
#define FOUR_C_ELCH_ALGORITHM_HPP

#include "4C_config.hpp"

#include "4C_scatra_algorithm.hpp"

FOUR_C_NAMESPACE_OPEN

namespace ElCh
{
  /// ELCH algorithm base
  /*!

    Base class of ELCH algorithms. Derives from ScaTraAlgorithm.

    \author gjb
    \date 03/08
   */
  class Algorithm : public ScaTra::ScaTraAlgorithm
  {
   public:
    /// constructor
    explicit Algorithm(const Epetra_Comm& comm,     ///< communicator
        const Teuchos::ParameterList& elchcontrol,  ///< elch parameter list
        const Teuchos::ParameterList& scatradyn,    ///< scatra parameter list
        const Teuchos::ParameterList& fdyn,         ///< fluid parameter list
        const Teuchos::ParameterList& solverparams  ///< solver parameter list
    );


   protected:
    /// provide information about initial field
    void prepare_time_loop() override;

    /// print scatra solver type to screen
    void print_scatra_solver() override;

    /// convergence check for natural convection solver
    bool convergence_check(int natconvitnum, int natconvitmax, double natconvittol) override;
  };
}  // namespace ElCh

FOUR_C_NAMESPACE_CLOSE

#endif

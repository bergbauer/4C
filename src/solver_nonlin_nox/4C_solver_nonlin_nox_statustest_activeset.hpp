/*-----------------------------------------------------------*/
/*! \file

\brief Check the active set for convergence. Only meaningful for
       inequality constrained problems.



\level 3

*/
/*-----------------------------------------------------------*/

#ifndef FOUR_C_SOLVER_NONLIN_NOX_STATUSTEST_ACTIVESET_HPP
#define FOUR_C_SOLVER_NONLIN_NOX_STATUSTEST_ACTIVESET_HPP

#include "4C_config.hpp"

#include "4C_solver_nonlin_nox_enum_lists.hpp"
#include "4C_solver_nonlin_nox_forward_decl.hpp"

#include <NOX_StatusTest_Generic.H>  // base class
#include <Teuchos_RCP.hpp>

#include <deque>

FOUR_C_NAMESPACE_OPEN

namespace NOX
{
  namespace Nln
  {
    namespace StatusTest
    {
      class ActiveSet : public ::NOX::StatusTest::Generic
      {
       public:
        //! constructor
        ActiveSet(const NOX::Nln::StatusTest::QuantityType& qtype, const int& max_cycle_size);

        ::NOX::StatusTest::StatusType checkStatus(
            const ::NOX::Solver::Generic& problem, ::NOX::StatusTest::CheckType checkType) override;

        //! NOTE: returns the global status of all normF tests
        ::NOX::StatusTest::StatusType getStatus() const override;

        std::ostream& print(std::ostream& stream, int indent = 0) const override;

       private:
        //! current quantity type
        NOX::Nln::StatusTest::QuantityType qtype_;

        //! status of the active set test
        ::NOX::StatusTest::StatusType status_;

        //! maximal cycle size, which is checked
        int max_cycle_size_;

        //! cycle size if zigzagging is checked
        int cycle_size_;

        //! size of the active set
        int activesetsize_;

        std::deque<Teuchos::RCP<const Epetra_Map>> cycling_maps_;
      };  // class ActiveSet
    }     // namespace StatusTest
  }       // namespace Nln
}  // namespace NOX

FOUR_C_NAMESPACE_CLOSE

#endif
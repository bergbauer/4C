/*----------------------------------------------------------------------*/
/*! \file

\brief result testing functionality for scatra-thermo interaction problems

\level 2

*/
/*----------------------------------------------------------------------*/
#include "4C_sti_resulttest.hpp"

#include "4C_io_linedefinition.hpp"
#include "4C_linear_solver_method_linalg.hpp"
#include "4C_sti_monolithic.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 | constructor                                               fang 01/17 |
 *----------------------------------------------------------------------*/
STI::STIResultTest::STIResultTest(const Teuchos::RCP<STI::Algorithm>&
        sti_algorithm  //!< time integrator for scatra-thermo interaction
    )
    // call base class constructor
    : Core::UTILS::ResultTest("STI"),

      // store pointer to time integrator for scatra-thermo interaction
      sti_algorithm_(sti_algorithm)
{
  return;
}


/*-------------------------------------------------------------------------------------*
 | test special quantity not associated with a particular element or node   fang 01/17 |
 *-------------------------------------------------------------------------------------*/
void STI::STIResultTest::TestSpecial(
    Input::LineDefinition& res,  //!< input file line containing result test specification
    int& nerr,                   //!< number of failed result tests
    int& test_count              ///< number of result tests
)
{
  // make sure that quantity is tested only by one processor
  if (sti_algorithm_->Comm().MyPID() == 0)
  {
    // extract name of quantity to be tested
    std::string quantity;
    res.extract_string("QUANTITY", quantity);

    // get result to be tested
    const double result = result_special(quantity);

    // compare values
    const int err = compare_values(result, "SPECIAL", res);
    nerr += err;
    ++test_count;
  }

  return;
}  // STI::STIResultTest::TestSpecial


/*----------------------------------------------------------------------*
 | get special result to be tested                           fang 01/17 |
 *----------------------------------------------------------------------*/
double STI::STIResultTest::result_special(
    const std::string& quantity  //! name of quantity to be tested
) const
{
  // initialize variable for result
  double result(0.);

  // number of Newton-Raphson iterations in last time step
  if (quantity == "numiterlastnonlinearsolve") result = (double)sti_algorithm_->Iter();

  // number of iterations performed by linear solver during last Newton-Raphson iteration
  else if (quantity == "numiterlastlinearsolve")
    result = (double)sti_monolithic().Solver().getNumIters();

  // catch unknown quantity strings
  else
    FOUR_C_THROW(
        "Quantity '%s' not supported by result testing functionality for scatra-thermo "
        "interaction!",
        quantity.c_str());

  return result;
}  // STI::STIResultTest::result_special


/*------------------------------------------------------------------------------*
 | return time integrator for monolithic scatra-thermo interaction   fang 09/17 |
 *------------------------------------------------------------------------------*/
const STI::Monolithic& STI::STIResultTest::sti_monolithic() const
{
  const STI::Monolithic* const sti_monolithic =
      dynamic_cast<const STI::Monolithic* const>(sti_algorithm_.get());
  if (sti_monolithic == nullptr)
    FOUR_C_THROW("Couldn't access time integrator for monolithic scatra-thermo interaction!");
  return *sti_monolithic;
}

FOUR_C_NAMESPACE_CLOSE
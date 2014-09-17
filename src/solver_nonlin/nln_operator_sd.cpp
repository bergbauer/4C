/*----------------------------------------------------------------------------*/
/*!
\file nln_operator_sd.cpp

<pre>
Maintainer: Matthias Mayr
            mayr@mhpc.mw.tum.de
            089 - 289-10362
</pre>
*/

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* headers */

// Epetra
#include <Epetra_Comm.h>
#include <Epetra_MultiVector.h>
#include <Epetra_Operator.h>
#include <Epetra_Vector.h>

// standard
#include <iostream>
#include <vector>

// Teuchos
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_RCP.hpp>

// baci
#include "linesearch_base.H"
#include "linesearch_factory.H"
#include "nln_operator_sd.H"
#include "nln_problem.H"

#include "../drt_io/io_control.H"
#include "../drt_io/io_pstream.H"

#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/drt_globalproblem.H"

#include "../linalg/linalg_solver.H"

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* Constructor (empty) */
NLNSOL::NlnOperatorSD::NlnOperatorSD()
: linesearch_(Teuchos::null)
{
  return;
}

/*----------------------------------------------------------------------------*/
/* Setup of the algorithm  / operator */
void NLNSOL::NlnOperatorSD::Setup()
{
  // Make sure that Init() has been called
  if (not IsInit()) { dserror("Init() has not been called, yet."); }

  SetupLineSearch();

  // Setup() has been called
  SetIsSetup();

  return;
}

/*----------------------------------------------------------------------------*/
/* Setup of line search */
void NLNSOL::NlnOperatorSD::SetupLineSearch()
{
  NLNSOL::LineSearchFactory linesearchfactory;
  linesearch_ = linesearchfactory.Create(Params().sublist("SD: Line Search"));

  return;
}

/*----------------------------------------------------------------------------*/
/* Apply the preconditioner */
int NLNSOL::NlnOperatorSD::ApplyInverse(const Epetra_MultiVector& f,
    Epetra_MultiVector& x) const
{
  int err = 0;

  // Make sure that Init() and Setup() have been called
  if (not IsInit()) { dserror("Init() has not been called, yet."); }
  if (not IsSetup()) { dserror("Setup() has not been called, yet."); }

  // ---------------------------------------------------------------------------
  // initialize stuff for iteration loop
  // ---------------------------------------------------------------------------
  // solution increment vector
  Teuchos::RCP<Epetra_MultiVector> inc = Teuchos::rcp(new Epetra_MultiVector(x.Map(), true));

  // residual vector
  Teuchos::RCP<Epetra_MultiVector> rhs = Teuchos::rcp(new Epetra_MultiVector(x.Map(), true));
  NlnProblem()->Evaluate(x, *rhs);

  // some scalars
  int iter = 0; // iteration counter
  double steplength = 1.0; // line search parameter
  double fnorm2 = 1.0e+12; // residual L2 norm
  bool converged = NlnProblem()->ConvergenceCheck(*rhs, fnorm2); // convergence flag

  if (Params().get<bool>("SD: Print Iterations"))
    PrintIterSummary(iter, fnorm2);

  // ---------------------------------------------------------------------------
  // iteration loop
  // ---------------------------------------------------------------------------
  while (ContinueIterations(iter, converged))
  {
    ++iter;

    // compute the search direction
    ComputeSearchDirection(*rhs, *inc);

    // line search
    steplength = ComputeStepLength(x, *inc, fnorm2);

    // Iterative update
    err = x.Update(steplength, *inc, 1.0);
    if (err != 0) { dserror("Failed."); }

    // evaluate and check for convergence
    NlnProblem()->Evaluate(x, *rhs);

    converged = NlnProblem()->ConvergenceCheck(*rhs, fnorm2);

    PrintIterSummary(iter, fnorm2);
  }

  // return error code
  return (not CheckSuccessfulConvergence(iter, converged));
}

/*----------------------------------------------------------------------------*/
/* Compute the search direction */
const int NLNSOL::NlnOperatorSD::ComputeSearchDirection(
    const Epetra_MultiVector& rhs,
    Epetra_MultiVector& inc
    ) const
{
  // search direction = negative residual (gradient)
  int err = inc.Update(-1.0, rhs, 0.0);
  if (err != 0) { dserror("Update failed."); }

  return err;
}

/*----------------------------------------------------------------------------*/
/* Compute step length parameter */
const double NLNSOL::NlnOperatorSD::ComputeStepLength(
    const Epetra_MultiVector& x,
    const Epetra_MultiVector& inc,
    double fnorm2
    ) const
{
  linesearch_->Init(NlnProblem(), Params().sublist("SD: Line Search"), x, inc, fnorm2);
  linesearch_->Setup();
  return linesearch_->ComputeLSParam();
}
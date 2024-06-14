/*----------------------------------------------------------------------*/
/*! \file

\brief Implementation of NOX::Group for FSI

\level 1

*/

/*----------------------------------------------------------------------*/

#include "4C_fsi_nox_group.hpp"

#include "4C_fsi_monolithicinterface.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
NOX::FSI::Group::Group(FourC::FSI::MonolithicInterface& mfsi, Teuchos::ParameterList& printParams,
    const Teuchos::RCP<::NOX::Epetra::Interface::Required>& i, const ::NOX::Epetra::Vector& x,
    const Teuchos::RCP<::NOX::Epetra::LinearSystem>& linSys)
    : ::NOX::Epetra::Group(printParams, i, x, linSys), mfsi_(mfsi)
{
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void NOX::FSI::Group::CaptureSystemState()
{
  // we know we already have the first linear system calculated

  mfsi_.setup_rhs(RHSVector.getEpetraVector(), true);
  mfsi_.setup_system_matrix();

  sharedLinearSystem.getObject(this);
  isValidJacobian = true;
  isValidRHS = true;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
::NOX::Abstract::Group::ReturnType NOX::FSI::Group::computeF()
{
  ::NOX::Abstract::Group::ReturnType ret = ::NOX::Epetra::Group::computeF();
  if (ret == ::NOX::Abstract::Group::Ok)
  {
    if (not isValidJacobian)
    {
      mfsi_.setup_system_matrix();
      sharedLinearSystem.getObject(this);
      isValidJacobian = true;
    }
  }
  return ret;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
::NOX::Abstract::Group::ReturnType NOX::FSI::Group::computeJacobian()
{
  ::NOX::Abstract::Group::ReturnType ret = ::NOX::Epetra::Group::computeJacobian();
  if (ret == ::NOX::Abstract::Group::Ok)
  {
    if (not isValidRHS)
    {
      mfsi_.setup_rhs(RHSVector.getEpetraVector());
      isValidRHS = true;
    }
  }
  return ret;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
::NOX::Abstract::Group::ReturnType NOX::FSI::Group::computeNewton(Teuchos::ParameterList& p)
{
  mfsi_.scale_system(RHSVector.getEpetraVector());
  ::NOX::Abstract::Group::ReturnType status = ::NOX::Epetra::Group::computeNewton(p);
  mfsi_.unscale_solution(NewtonVector.getEpetraVector(), RHSVector.getEpetraVector());

  // check return value of computeNewton call
  if (status == ::NOX::Abstract::Group::NotConverged || status == ::NOX::Abstract::Group::Failed)
    FOUR_C_THROW("NOX::FSI::Group::computeNewton: linear solver not converged...");

  return status;
}

FOUR_C_NAMESPACE_CLOSE
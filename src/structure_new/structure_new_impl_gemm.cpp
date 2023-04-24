/*-----------------------------------------------------------*/
/*! \file

\brief Generalized Energy Momentum time integrator.


\level 3

*/
/*-----------------------------------------------------------*/

#include "structure_new_impl_gemm.H"

#include "lib_dserror.H"
#include "linalg_sparseoperator.H"

#include <Epetra_Vector.h>

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
STR::IMPLICIT::Gemm::Gemm()
{
  // empty constructor
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::Gemm::Setup()
{
  CheckInit();
  // Call the Setup() of the abstract base class first.
  Generic::Setup();

  issetup_ = true;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::Gemm::SetState(const Epetra_Vector& x)
{
  CheckInitSetup();
  dserror("Not yet implemented! (see the Statics integration for an example)");
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::Gemm::AddViscoMassContributions(Epetra_Vector& f) const
{
  dserror("Not implemented!");
  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::Gemm::AddViscoMassContributions(LINALG::SparseOperator& jac) const
{
  dserror("Not implemented!");
  return;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::Gemm::ApplyForce(const Epetra_Vector& x, Epetra_Vector& f)
{
  CheckInitSetup();
  return false;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::Gemm::AssembleForce(
    Epetra_Vector& f, const std::vector<INPAR::STR::ModelType>* without_these_models) const
{
  CheckInitSetup();
  return false;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::Gemm::ApplyStiff(const Epetra_Vector& x, LINALG::SparseOperator& jac)
{
  CheckInitSetup();
  return false;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::Gemm::ApplyForceStiff(
    const Epetra_Vector& x, Epetra_Vector& f, LINALG::SparseOperator& jac)
{
  CheckInitSetup();
  return false;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::Gemm::WriteRestart(
    IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  CheckInitSetup();
  dserror("Not yet implemented! (see the Statics integration for an example)");
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::Gemm::ReadRestart(IO::DiscretizationReader& ioreader)
{
  CheckInitSetup();
  dserror("Not yet implemented! (see the Statics integration for an example)");
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double STR::IMPLICIT::Gemm::CalcRefNormForce(const enum NOX::Abstract::Vector::NormType& type)
{
  CheckInitSetup();
  dserror("Not yet implemented! (see the Statics integration for an example)");
  return -1.0;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
double STR::IMPLICIT::Gemm::GetIntParam() const
{
  CheckInitSetup();
  dserror("Set the time integration parameter as return value!");
  return -1.0;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::Gemm::UpdateStepState()
{
  CheckInitSetup();
  dserror("Not yet implemented! (see the Statics integration for an example)");
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::Gemm::UpdateStepElement()
{
  CheckInitSetup();
  dserror("Not yet implemented! (see the Statics integration for an example)");
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void STR::IMPLICIT::Gemm::PredictConstDisConsistVelAcc(
    Epetra_Vector& disnp, Epetra_Vector& velnp, Epetra_Vector& accnp) const
{
  CheckInitSetup();
  dserror("Not yet implemented! (see the Statics integration for an example)");
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::Gemm::PredictConstVelConsistAcc(
    Epetra_Vector& disnp, Epetra_Vector& velnp, Epetra_Vector& accnp) const
{
  CheckInitSetup();
  dserror("Not yet implemented! (see the Statics integration for an example)");
  return false;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
bool STR::IMPLICIT::Gemm::PredictConstAcc(
    Epetra_Vector& disnp, Epetra_Vector& velnp, Epetra_Vector& accnp) const
{
  CheckInitSetup();
  dserror("Not yet implemented! (see the Statics integration for an example)");
  return false;
}

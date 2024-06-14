/*----------------------------------------------------------------------*/
/*! \file

\brief Wrapper for the field time integration

\level 2


*/
/*-----------------------------------------------------------------------*/

#include "4C_adapter_field_wrapper.hpp"

FOUR_C_NAMESPACE_OPEN

/*-----------------------------------------------------------------------/
| start new time step                                                    |
/-----------------------------------------------------------------------*/
void Adapter::FieldWrapper::prepare_time_step()
{
  field_->prepare_time_step();
  if (nox_correction_) reset_stepinc();
}


void Adapter::FieldWrapper::update_state_incrementally(Teuchos::RCP<const Epetra_Vector> disiterinc)
{
  if (nox_correction_) get_iterinc(disiterinc);
  field_->update_state_incrementally(disiterinc);
}

/*-----------------------------------------------------------------------/
| update dofs and evaluate elements                                      |
/-----------------------------------------------------------------------*/
void Adapter::FieldWrapper::Evaluate(Teuchos::RCP<const Epetra_Vector> disiterinc)
{
  if (nox_correction_) get_iterinc(disiterinc);
  field_->Evaluate(disiterinc);
}

/*-----------------------------------------------------------------------/
| update dofs and evaluate elements                                      |
/-----------------------------------------------------------------------*/
void Adapter::FieldWrapper::Evaluate(Teuchos::RCP<const Epetra_Vector> disiterinc, bool firstiter)
{
  if (nox_correction_) get_iterinc(disiterinc);
  field_->Evaluate(disiterinc, firstiter);
}

/*-----------------------------------------------------------------------/
| Reset Step Increment                                                   |
/-----------------------------------------------------------------------*/
void Adapter::FieldWrapper::reset_stepinc()
{
  if (stepinc_ != Teuchos::null) stepinc_->PutScalar(0.);
}

/*-----------------------------------------------------------------------/
| Get Iteration Increment from Step Increment                            |
/-----------------------------------------------------------------------*/
void Adapter::FieldWrapper::get_iterinc(Teuchos::RCP<const Epetra_Vector>& stepinc)
{
  // The field solver always expects an iteration increment only. And
  // there are Dirichlet conditions that need to be preserved. So take
  // the sum of increments we get from NOX and apply the latest iteration
  // increment only.
  // Naming:
  //
  // x^n+1_i+1 = x^n+1_i + iterinc  (sometimes referred to as residual increment), and
  //
  // x^n+1_i+1 = x^n     + stepinc
  if (stepinc != Teuchos::null)
  {
    // iteration increments
    Teuchos::RCP<Epetra_Vector> iterinc = Teuchos::rcp(new Epetra_Vector(*stepinc));
    if (stepinc_ != Teuchos::null)
    {
      iterinc->Update(-1.0, *stepinc_, 1.0);

      // update incremental dof member to provided step increments
      // shortly: disinc_^<i> := disp^<i+1>
      stepinc_->Update(1.0, *stepinc, 0.0);
    }
    else
    {
      stepinc_ = Teuchos::rcp(new Epetra_Vector(*stepinc));
    }
    // output is iterinc!
    stepinc = Teuchos::rcp(new const Epetra_Vector(*iterinc));
  }
}

FOUR_C_NAMESPACE_CLOSE
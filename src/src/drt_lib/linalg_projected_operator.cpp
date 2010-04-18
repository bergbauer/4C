/*!----------------------------------------------------------------------
\file  linalg_projected_operator.cpp

<pre>
Maintainer: Peter Gamnitzer
            gamnitzer@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15235
</pre>

*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include "linalg_projected_operator.H"

/* --------------------------------------------------------------------
                          Constructor
   -------------------------------------------------------------------- */
LINALG::LinalgProjectedOperator::LinalgProjectedOperator(
  Teuchos::RCP<Epetra_Operator>         A      ,
  bool                                  project,
  Teuchos::RCP<LINALG::KrylovProjector> projector
  ) :
  project_(project),
  A_      (A)      ,
  projector_(projector)
{
  if (project_ && (projector==Teuchos::null))
    dserror("Kernel projection enabled but got no projector object");

  return;
} // LINALG::LinalgProjectedOperator::LinalgProjectedOperator

/* --------------------------------------------------------------------
                           Destructor
   -------------------------------------------------------------------- */
LINALG::LinalgProjectedOperator::~LinalgProjectedOperator()
{
  return;
} // LINALG::LinalgProjectedOperator::~LinalgProjectedOperator

/* --------------------------------------------------------------------
                      (Modified) Apply call
   -------------------------------------------------------------------- */
int LINALG::LinalgProjectedOperator::Apply(
  const Epetra_MultiVector &X,
  Epetra_MultiVector       &Y
  ) const
{
  int ierr=0;

  // Apply the operator
  ierr=A_->Apply(X,Y);

  // if necessary, project out matrix kernel
  if(project_)
  {
    int ierr2=0;
    ierr2 = projector_->ApplyPT(Y);
  }

  return(ierr);
} // LINALG::LinalgProjectedOperator::Apply

#endif // CCADISCRET

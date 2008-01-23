
#ifdef D_ALE
#ifdef CCADISCRET

#include "ale3.H"
#include "../drt_lib/linalg_utils.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_dserror.H"


int DRT::ELEMENTS::Ale3Surface::EvaluateNeumann(
  ParameterList& params,
  DRT::Discretization&      discretization,
  DRT::Condition&           condition,
  vector<int>&              lm,
  Epetra_SerialDenseVector& elevec1)
{
  return 0;
}

#endif
#endif

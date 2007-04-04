/*----------------------------------------------------------------------*/
/*!
\file post_drt_evaluation.cpp

\brief compatibility definitions

<pre>
Maintainer: Ulrich Kuettler
            kuettler@lnm.mw.tum.de
            http://www.lnm.mw.tum.de/Members/kuettler
            089 - 289-15238
</pre>

Some discretization functions cannot be included in the filter build
because they use ccarat facilities that are not available inside the
filter. But to link the filter stubs of these functions are needed.

*/
/*----------------------------------------------------------------------*/

#ifdef CCADISCRET
#ifdef TRILINOS_PACKAGE

#include "post_drt_common.H"
#include "../discret/shell8.H"
#include "../discret/fluid2.H"
#include "../discret/fluid3.H"


extern "C" void ReadDat()
{
    dserror("This is not supposed to happen");
    return;
}

/*----------------------------------------------------------------------*
 |  compare the integers - qsort routine                  a.lipka 5/01  |
 |                                                                      |
 |  the call for the sorter of an INT vector is then                    |
 |                                                                      |
 |  qsort((INT*) vector, lenght, sizeof(INT), cmp_int);                 |
 |                                                                      |
 *----------------------------------------------------------------------*/
extern "C" INT cmp_int(const void *a, const void *b )
{
    return *(INT *)a - * (INT *)b;
}

/*----------------------------------------------------------------------*
 |  compare the doubles - qsort routine                   a.lipka 5/01  |
 |                                                                      |
 |  the call for the sorter of a DOUBLE vector is then                  |
 |                                                                      |
 |  qsort((DOUBLE*) vector, lenght, sizeof(DOUBLE), cmp_double);        |
 |                                                                      |
 *----------------------------------------------------------------------*/
extern "C" DOUBLE cmp_double(const void *a, const void *b )
{
    return *(DOUBLE *)a - * (DOUBLE *)b;
}


/*----------------------------------------------------------------------*/
/*!
  \brief A Hack.

  This a yet another hack. This function is called by dserror and
  closes all open files --- in ccarat. The filters are not that
  critical. Thus we do nothing here. We just have to have this
  function.

  \author u.kue
  \date 12/04
*/
/*----------------------------------------------------------------------*/
extern "C" void io_emergency_close_files()
{
  // nothing to do!
}


using namespace DRT;

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Discretization::Evaluate(
                              ParameterList&                params,
                              RefCountPtr<Epetra_CrsMatrix> systemmatrix1,
                              RefCountPtr<Epetra_CrsMatrix> systemmatrix2,
                              RefCountPtr<Epetra_Vector>    systemvector1,
                              RefCountPtr<Epetra_Vector>    systemvector2,
                              RefCountPtr<Epetra_Vector>    systemvector3)
{
  dserror("Discretization::Evaluate undefined");
}


void Discretization::EvaluateNeumann(ParameterList& params, Epetra_Vector& systemvector)
{
  dserror("Discretization::EvaluateNeumann undefined");
}


void Discretization::EvaluateDirichlet(ParameterList& params,
                                            Epetra_Vector& systemvector,
                                            Epetra_Vector& toggle)
{
  dserror("Discretization::EvaluateDirichlet undefined");
}


int Elements::Shell8Register::Initialize(Discretization&)
{
  // dserror("Elements::Shell8Register::Initialize undefined");
  return 0;
}

bool Elements::Shell8::ReadElement()
{
  dserror("Elements::Shell8::ReadElement undefined");
  return false;
}

int Elements::Shell8::Evaluate(ParameterList&,
                               Discretization&,
                               vector<int>&,
                               Epetra_SerialDenseMatrix&,
                               Epetra_SerialDenseMatrix&,
                               Epetra_SerialDenseVector&,
                               Epetra_SerialDenseVector&,
                               Epetra_SerialDenseVector&)
{
  dserror("Elements::Shell8::Evaluate undefined");
  return 0;
}

int Elements::Shell8::EvaluateNeumann(ParameterList&, Discretization&, Condition&, vector<int>&, Epetra_SerialDenseVector&)
{
  dserror("Elements::Shell8::EvaluateNeumann undefined");
  return 0;
}

int Elements::Shell8Line::EvaluateNeumann(ParameterList& params,
                                          Discretization&      discretization,
                                          Condition&           condition,
                                          vector<int>&              lm,
                                          Epetra_SerialDenseVector& elevec1)
{
  dserror("Elements::Shell8Line::EvaluateNeumann undefined");
  return 0;
}

int Elements::Fluid3Register::Initialize(Discretization&)
{
  // dserror("Elements::Fluid3Register::Initialize undefined");
  return 0;
}

bool Elements::Fluid3::ReadElement()
{
  dserror("Elements::Fluid3::ReadElement undefined");
  return false;
}

int Elements::Fluid3::Evaluate(ParameterList&,
                               Discretization&,
                               vector<int>&,
                               Epetra_SerialDenseMatrix&,
                               Epetra_SerialDenseMatrix&,
                               Epetra_SerialDenseVector&,
                               Epetra_SerialDenseVector&,
                               Epetra_SerialDenseVector&)
{
  dserror("Elements::Fluid3::Evaluate undefined");
  return 0;
}

int Elements::Fluid3::EvaluateNeumann(ParameterList&, Discretization&, Condition&, vector<int>&, Epetra_SerialDenseVector&)
{
  dserror("Elements::Fluid3::EvaluateNeumann undefined");
  return 0;
}

int Elements::Fluid3Surface::EvaluateNeumann(ParameterList& params,
                                          Discretization&      discretization,
                                          Condition&           condition,
                                          vector<int>&              lm,
                                          Epetra_SerialDenseVector& elevec1)
{
  dserror("Elements::Fluid3Surface::EvaluateNeumann undefined");
  return 0;
}

    
int Elements::Fluid2Register::Initialize(Discretization&)
{
  // dserror("Elements::Fluid2Register::Initialize undefined");
  return 0;
}

bool Elements::Fluid2::ReadElement()
{
  dserror("Elements::Fluid2::ReadElement undefined");
  return false;
}

int Elements::Fluid2::Evaluate(ParameterList&,
                               Discretization&,
                               vector<int>&,
                               Epetra_SerialDenseMatrix&,
                               Epetra_SerialDenseMatrix&,
                               Epetra_SerialDenseVector&,
                               Epetra_SerialDenseVector&,
                               Epetra_SerialDenseVector&)
{
  dserror("Elements::Fluid2::Evaluate undefined");
  return 0;
}

int Elements::Fluid2::EvaluateNeumann(ParameterList&, Discretization&, Condition&, vector<int>&, Epetra_SerialDenseVector&)
{
  dserror("Elements::Fluid2::EvaluateNeumann undefined");
  return 0;
}

int Elements::Fluid2Line::EvaluateNeumann(ParameterList& params,
                                          Discretization&      discretization,
                                          Condition&           condition,
                                          vector<int>&              lm,
                                          Epetra_SerialDenseVector& elevec1)
{
  dserror("Elements::Fluid2Line::EvaluateNeumann undefined");
  return 0;
}
    
#endif
#endif

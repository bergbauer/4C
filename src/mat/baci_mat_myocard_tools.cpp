/*----------------------------------------------------------------------*/
/*! \file
\brief myocard tools

\level 3

*/

/*----------------------------------------------------------------------*
 |  headers                                                  cbert 08/13 |
 *----------------------------------------------------------------------*/
#include "baci_mat_myocard_tools.H"

#include <math.h> /* tanh, log */

/*----------------------------------------------------------------------*
 |  Constructor                                    (public)  cbert 08/13 |
 *----------------------------------------------------------------------*/
Myocard_Tools::Myocard_Tools() {}

/*----------------------------------------------------------------------*
 |                                                           cbert 08/13 |
 *----------------------------------------------------------------------*/
double Myocard_Tools::GatingFunction(const double Gate1, const double Gate2, const double p,
    const double var, const double thresh) const
{
  double Erg = Gate1 + (Gate2 - Gate1) * (1.0 + tanh(p * (var - thresh))) / 2;
  return Erg;
}


/*----------------------------------------------------------------------*
 |                                                           cbert 10/14 |
 *----------------------------------------------------------------------*/
double Myocard_Tools::GatingVarCalc(
    const double dt, double y_0, const double y_inf, const double y_tau) const
{
  // Solve dy/dt = (1/a)*(y_inf-y)
  double y_1 = 1.0 / (1.0 + dt / y_tau) * (y_0 + dt * y_inf / y_tau);
  return y_1;
}
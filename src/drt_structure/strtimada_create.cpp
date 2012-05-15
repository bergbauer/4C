/*----------------------------------------------------------------------*/
/*!
\file strtimada_create.cpp
\brief Structural time integration

<pre>
Maintainer: Alexander Popp
            popp@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15238
</pre>
*/

/*----------------------------------------------------------------------*/
/* macros */

/*----------------------------------------------------------------------*/
/* headers */
#include <ctime>
#include <cstdlib>
#include <iostream>

#include <Teuchos_StandardParameterEntryValidators.hpp>

#ifdef PARALLEL
#include <mpi.h>
#endif

#include "strtimada_create.H"
#include "strtimada_zienxie.H"
#include "strtimada_joint.H"
#include "strtimint_ab2.H"
#include "../linalg/linalg_blocksparsematrix.H"


/*======================================================================*/
/* create auxiliar time integration scheme */
Teuchos::RCP<STR::TimAda> STR::TimAdaCreate
(
  const Teuchos::ParameterList& ioflags,
  const Teuchos::ParameterList& sdyn,
  const Teuchos::ParameterList& xparams,
  const Teuchos::ParameterList& tap,  //!< adaptive input flags
  Teuchos::RCP<STR::TimInt> tis  //!< marching time integrator
)
{
  Teuchos::RCP<STR::TimAda> sta = Teuchos::null;

  // auxiliar time integrator
  switch (DRT::INPUT::IntegralValue<INPAR::STR::TimAdaKind>(tap,"KIND"))
  {

  case INPAR::STR::timada_kind_none :
    // No adaptivity in time
    sta = Teuchos::null;
    break;

  case INPAR::STR::timada_kind_zienxie :
    // Zienkiewivz-Xie error indicator for generalised-alpha
    sta = Teuchos::rcp(new STR::TimAdaZienXie(sdyn, tap, tis));
    break;

  case INPAR::STR::timada_kind_ab2 :
    // Adams-Bashforth 2nd order
    sta = Teuchos::rcp(new STR::TimAdaJoint<STR::TimIntAB2>(ioflags, sdyn, xparams, tap, tis));
    break;

  default :
    dserror("Auxiliar time integrator is not available.");
    break;

  }

  // return the auxiliar integrator
  return sta;
}

/*----------------------------------------------------------------------*/

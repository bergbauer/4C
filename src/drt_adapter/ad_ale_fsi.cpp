/*----------------------------------------------------------------------------*/
/*!
 \file ad_ale_fsi.cpp

 <pre>
Maintainer: Matthias Mayr
            mayr@mhpc.mw.tum.de
            089 - 289-10362
 </pre>
 */
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/* header inclusions */
#include "ad_ale_fsi.H"

#include "../drt_ale_new/ale_utils_mapextractor.H"

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
ADAPTER::AleFsiWrapper::AleFsiWrapper(Teuchos::RCP<Ale> ale)
  : AleWrapper(ale)
{
  // create the FSI interface
  interface_ = Teuchos::rcp(new ALENEW::UTILS::MapExtractor);
  interface_->Setup(*Discretization());

  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<const ALENEW::UTILS::MapExtractor>
ADAPTER::AleFsiWrapper::Interface() const
{
  return interface_;
}

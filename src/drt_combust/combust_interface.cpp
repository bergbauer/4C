/*!-----------------------------------------------------------------------------------------------*
 \file combust_interface.cpp

 \brief interface handle that transports the intersection related things around for combustion problems

  detailed description in header file combust_interface.H

<pre>
Maintainer: Florian Henke
            henke@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15265
</pre>
 *------------------------------------------------------------------------------------------------*/
#ifdef CCADISCRET

#include "combust_interface.H"

#include "../drt_lib/standardtypes_cpp.H"
#include <Teuchos_TimeMonitor.hpp>
#include <Teuchos_StandardParameterEntryValidators.hpp>
#include "../drt_lib/drt_globalproblem.H"
 #include "../drt_lib/drt_utils.H"

// #include "xfem_condition.H"
// #include "../drt_io/io_gmsh.H"
// #include "../drt_io/io_gmsh_xfem_extension.H"
// #include "../drt_geometry/integrationcell.H"

extern struct _FILES  allfiles;

/*------------------------------------------------------------------------------------------------*
 | constructor                                                                         henke 10/08 | 
 *------------------------------------------------------------------------------------------------*/
COMBUST::InterfaceHandleCombust::InterfaceHandleCombust(
    const Teuchos::RCP<DRT::Discretization>  fluiddis,
    const Teuchos::RCP<DRT::Discretization>  gfuncdis
    ) : InterfaceHandle(fluiddis),
        gfuncdis_(gfuncdis)
{
  if (fluiddis->Comm().MyPID() == 0)
    std::cout << "Constructing InterfaceHandle" << std::endl;

  elementalDomainIntCells_.clear();
  elementalBoundaryIntCells_.clear();

  SanityChecks();

//  octTreenp_ = rcp( new GEO::SearchTree(5));
//  octTreenp_->initializeTree(AABB, elementsByLabel_, GEO::TreeType(GEO::OCTTREE));
//  octTreen_ = rcp( new GEO::SearchTree(5));
//  octTreen_->initializeTree(AABB, elementsByLabel_, GEO::TreeType(GEO::OCTTREE));

}
/*------------------------------------------------------------------------------------------------*
 | destructor                                                                         henke 10/08 | 
 *------------------------------------------------------------------------------------------------*/
COMBUST::InterfaceHandleCombust::~InterfaceHandleCombust()
{
  return;
}


//! implement this function if needed for combustion!
void COMBUST::InterfaceHandleCombust::toGmsh(const int step) const
{
  dserror ("not implemented");
  return;
}

//! implement this function if needed for combustion!
int COMBUST::InterfaceHandleCombust::PositionWithinConditionNP(const BlitzVec3& x_in) const
{
  dserror("not implemented");
  return 0;
}

//! implement this function if needed for combustion!
int COMBUST::InterfaceHandleCombust::PositionWithinConditionN(const BlitzVec3& x_in) const
{
  dserror("not implemented");
  return 0;
}

//! implement this function if needed for combustion!
int COMBUST::InterfaceHandleCombust::PositionWithinConditionNP(const BlitzVec3&     x_in,
                              GEO::NearestObject&  nearestobject) const
{
  dserror("not implemented");
  return 0;
}

//! implement this function if needed for combustion!
int COMBUST::InterfaceHandleCombust::PositionWithinConditionN(const BlitzVec3&     x_in,
                             GEO::NearestObject&  nearestobject) const
{
  dserror("not implemented");
  return 0;
}

#endif // #ifdef CCADISCRET

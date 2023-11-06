/*----------------------------------------------------------------------*/
/*! \file

\brief volume element


\level 2
*/
/*----------------------------------------------------------------------*/

#include "baci_bele_vele3.H"
#include "baci_lib_discret.H"
#include "baci_lib_utils_factory.H"
#include "baci_utils_exceptions.H"


DRT::ELEMENTS::Vele3SurfaceType DRT::ELEMENTS::Vele3SurfaceType::instance_;

DRT::ELEMENTS::Vele3SurfaceType& DRT::ELEMENTS::Vele3SurfaceType::Instance() { return instance_; }


/*----------------------------------------------------------------------*
 |  ctor (public)                                            mwgee 05/09|
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Vele3Surface::Vele3Surface(int id, int owner, int nnode, const int* nodeids,
    DRT::Node** nodes, DRT::ELEMENTS::Vele3* parent, const int lsurface)
    : DRT::FaceElement(id, owner)
{
  SetNodeIds(nnode, nodeids);
  BuildNodalPointers(nodes);
  SetParentMasterElement(parent, lsurface);
  return;
}



/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                       mwgee 01/07|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Vele3Surface::Vele3Surface(const DRT::ELEMENTS::Vele3Surface& old)
    : DRT::FaceElement(old)
{
  return;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::Element* DRT::ELEMENTS::Vele3Surface::Clone() const
{
  DRT::ELEMENTS::Vele3Surface* newelement = new DRT::ELEMENTS::Vele3Surface(*this);
  return newelement;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
CORE::FE::CellType DRT::ELEMENTS::Vele3Surface::Shape() const
{
  switch (NumNode())
  {
    case 3:
      return CORE::FE::CellType::tri3;
    case 4:
      return CORE::FE::CellType::quad4;
    case 6:
      return CORE::FE::CellType::tri6;
    case 8:
      return CORE::FE::CellType::quad8;
    case 9:
      return CORE::FE::CellType::quad9;
    default:
      dserror("unexpected number of nodes %d", NumNode());
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Vele3Surface::Pack(DRT::PackBuffer& data) const
{
  dserror("this Vele3Surface element does not support communication");
  return;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Vele3Surface::Unpack(const std::vector<char>& data)
{
  dserror("this Vele3Surface element does not support communication");
  return;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Vele3Surface::Print(std::ostream& os) const
{
  os << "Vele3Surface " << DRT::DistypeToString(Shape());
  Element::Print(os);
  return;
}


/*----------------------------------------------------------------------*
 |  get vector of lines (public)                               gjb 05/08|
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<DRT::Element>> DRT::ELEMENTS::Vele3Surface::Lines()
{
  return DRT::UTILS::ElementBoundaryFactory<Vele3Line, Vele3Surface>(DRT::UTILS::buildLines, *this);
}


/*----------------------------------------------------------------------*
 |  get vector of Surfaces (length 1) (public)               gammi 04/07|
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<DRT::Element>> DRT::ELEMENTS::Vele3Surface::Surfaces()
{
  return {Teuchos::rcpFromRef(*this)};
}



/*----------------------------------------------------------------------*
 |  get optimal gauss rule                                   gammi 04/07|
 *----------------------------------------------------------------------*/
CORE::DRT::UTILS::GaussRule2D DRT::ELEMENTS::Vele3Surface::getOptimalGaussrule(
    const CORE::FE::CellType& distype) const
{
  CORE::DRT::UTILS::GaussRule2D rule = CORE::DRT::UTILS::GaussRule2D::undefined;
  switch (distype)
  {
    case CORE::FE::CellType::quad4:
      rule = CORE::DRT::UTILS::GaussRule2D::quad_4point;
      break;
    case CORE::FE::CellType::quad8:
    case CORE::FE::CellType::quad9:
      rule = CORE::DRT::UTILS::GaussRule2D::quad_9point;
      break;
    case CORE::FE::CellType::tri3:
      rule = CORE::DRT::UTILS::GaussRule2D::tri_3point;
      break;
    case CORE::FE::CellType::tri6:
      rule = CORE::DRT::UTILS::GaussRule2D::tri_6point;
      break;
    default:
      dserror("unknown number of nodes for gaussrule initialization");
  }
  return rule;
}
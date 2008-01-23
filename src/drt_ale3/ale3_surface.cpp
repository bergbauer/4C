
#ifdef D_ALE
#ifdef CCADISCRET

#include "ale3.H"
#include "../drt_lib/linalg_utils.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_dserror.H"



DRT::ELEMENTS::Ale3Surface::Ale3Surface(int id,
                                        int owner,
                                        int nnode,
                                        const int* nodeids,
                                        DRT::Node** nodes,
                                        DRT::ELEMENTS::Ale3* parent,
                                        const int lsurface)
  : DRT::Element(id,element_ale3surface,owner),
    parent_(parent),
    lsurface_(lsurface)
{
  SetNodeIds(nnode,nodeids);
  BuildNodalPointers(nodes);
}


DRT::ELEMENTS::Ale3Surface::Ale3Surface(const DRT::ELEMENTS::Ale3Surface& old)
  : DRT::Element(old),
    parent_(old.parent_),
    lsurface_(old.lsurface_)
{
}


DRT::Element* DRT::ELEMENTS::Ale3Surface::Clone() const
{
  DRT::ELEMENTS::Ale3Surface* newelement = new DRT::ELEMENTS::Ale3Surface(*this);
  return newelement;
}


DRT::Element::DiscretizationType DRT::ELEMENTS::Ale3Surface::Shape() const
{
  switch (NumNode())
  {
  case 3: return tri3;
  case 4: return quad4;
  case 6: return tri6;
  case 8: return quad8;
  case 9: return quad9;
  default:
    dserror("unexpected number of nodes %d", NumNode());
  }
  return dis_none;
}


void DRT::ELEMENTS::Ale3Surface::Pack(vector<char>& data) const
{
  data.resize(0);
  dserror("this Ale3Surface element does not support communication");
}


void DRT::ELEMENTS::Ale3Surface::Unpack(const vector<char>& data)
{
  dserror("this Ale3Surface element does not support communication");
}


DRT::ELEMENTS::Ale3Surface::~Ale3Surface()
{
}


void DRT::ELEMENTS::Ale3Surface::Print(ostream& os) const
{
  os << "Ale3Surface ";
  Element::Print(os);
}


#endif
#endif

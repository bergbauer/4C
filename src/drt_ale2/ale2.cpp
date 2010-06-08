//-----------------------------------------------------------------------
/*!
\file ale2.cpp

<pre>

</pre>
*/
//-----------------------------------------------------------------------
#ifdef D_ALE
#ifdef CCADISCRET

#include "ale2.H"
#include "../drt_ale2/ale2_nurbs.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_lib/drt_dserror.H"

using namespace DRT::UTILS;

DRT::ELEMENTS::Ale2Type DRT::ELEMENTS::Ale2Type::instance_;

DRT::ParObject* DRT::ELEMENTS::Ale2Type::Create( const std::vector<char> & data )
{
  DRT::ELEMENTS::Ale2* object = new DRT::ELEMENTS::Ale2(-1,-1);
  object->Unpack(data);
  return object;
}


Teuchos::RCP<DRT::Element> DRT::ELEMENTS::Ale2Type::Create( const string eletype,
                                                            const string eledistype,
                                                            const int id,
                                                            const int owner )
{
  Teuchos::RCP<DRT::Element> ele;

  if ( eletype=="ALE2" )
  {
    if (eledistype!="NURBS4" and eledistype!="NURBS9")
    {
      ele = rcp(new DRT::ELEMENTS::Ale2(id,owner));
    }
  }

  return ele;
}


DRT::ELEMENTS::Ale2::Ale2(int id, int owner)
  : DRT::Element(id,element_ale2,owner),
    data_()
{
}


DRT::ELEMENTS::Ale2::Ale2(const DRT::ELEMENTS::Ale2& old)
  : DRT::Element(old),
    data_(old.data_)
{
  return;
}


DRT::Element* DRT::ELEMENTS::Ale2::Clone() const
{
  DRT::ELEMENTS::Ale2* newelement = new DRT::ELEMENTS::Ale2(*this);
  return newelement;
}


DRT::Element::DiscretizationType DRT::ELEMENTS::Ale2::Shape() const
{
  switch (NumNode())
  {
  case  3: return tri3;
  case  4: return quad4;
  case  6: return tri6;
  case  8: return quad8;
  case  9: return quad9;
  default:
    dserror("unexpected number of nodes %d", NumNode());
  }
  return dis_none;
}


void DRT::ELEMENTS::Ale2::Pack(vector<char>& data) const
{
  data.resize(0);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // add base class Element
  vector<char> basedata(0);
  Element::Pack(basedata);
  AddtoPack(data,basedata);
  // data_
  vector<char> tmp(0);
  data_.Pack(tmp);
  AddtoPack(data,tmp);
}


void DRT::ELEMENTS::Ale2::Unpack(const vector<char>& data)
{
	vector<char>::size_type position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");
  // extract base class Element
  vector<char> basedata(0);
  ExtractfromPack(position,data,basedata);
  Element::Unpack(basedata);
  // data_
  vector<char> tmp(0);
  ExtractfromPack(position,data,tmp);
  data_.Unpack(tmp);

  if (position != data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
}


DRT::ELEMENTS::Ale2::~Ale2()
{
}


void DRT::ELEMENTS::Ale2::Print(ostream& os) const
{
  os << "Ale2 ";
  Element::Print(os);
  cout << endl;
  cout << data_;
  return;
}


RefCountPtr<DRT::ElementRegister> DRT::ELEMENTS::Ale2::ElementRegister() const
{
  return Teuchos::null;
}


//
// get vector of lines
//
vector<RCP<DRT::Element> > DRT::ELEMENTS::Ale2::Lines()
{
  // do NOT store line or surface elements inside the parent element
  // after their creation.
  // Reason: if a Redistribute() is performed on the discretization,
  // stored node ids and node pointers owned by these boundary elements might
  // have become illegal and you will get a nice segmentation fault ;-)

  // so we have to allocate new line elements:
  return DRT::UTILS::ElementBoundaryFactory<Ale2Line,Ale2>(DRT::UTILS::buildLines,this);
}


vector<RCP<DRT::Element> > DRT::ELEMENTS::Ale2::Surfaces()
{
  vector<RCP<Element> > surfaces(1);
  surfaces[0]= rcp(this, false);
  return surfaces;
}


GaussRule2D DRT::ELEMENTS::Ale2::getOptimalGaussrule(const DiscretizationType& distype)
{
    GaussRule2D rule = intrule2D_undefined;
    switch (distype)
    {
    case quad4: case nurbs4:
        rule = intrule_quad_4point;
        break;
    case quad8: case quad9: case nurbs9:
        rule = intrule_quad_9point;
        break;
    case tri3:
        rule = intrule_tri_3point;
        break;
    case tri6:
        rule = intrule_tri_6point;
        break;
    default:
        dserror("unknown number of nodes for gaussrule initialization");
  }
  return rule;
}


#endif
#endif

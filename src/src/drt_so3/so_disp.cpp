/*!----------------------------------------------------------------------
\file so_disp.cpp
\brief

<pre>
Maintainer: Axel Gerstenberger
            gerstenberger@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15236
</pre>

*----------------------------------------------------------------------*/
#ifdef D_SOLID3
#ifdef CCADISCRET

#include "so_disp.H"
#include "so_line.H"
#include "so_surface.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_lib/drt_dserror.H"
#include "../drt_fem_general/drt_utils_fem_shapefunctions.H"

using namespace DRT::UTILS;


/*----------------------------------------------------------------------*
 |  ctor (public)                                              maf 04/07|
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::SoDisp::SoDisp(int id, int owner) :
DRT::Element(id,element_sodisp,owner),
kintype_(sodisp_totlag),
stresstype_(sodisp_stress_none),
gaussrule_(intrule3D_undefined),
numnod_disp_(-1),
numdof_disp_(-1),
numgpt_disp_(-1)
{
  return;
}

/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                         maf 04/07|
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::SoDisp::SoDisp(const DRT::ELEMENTS::SoDisp& old) :
DRT::Element(old),
kintype_(old.kintype_),
stresstype_(old.stresstype_),
gaussrule_(old.gaussrule_),
numnod_disp_(old.numnod_disp_),
numdof_disp_(old.numdof_disp_),
numgpt_disp_(old.numgpt_disp_)
{
  return;
}

/*----------------------------------------------------------------------*
 |  Deep copy this instance of Solid3 and return pointer to it (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
DRT::Element* DRT::ELEMENTS::SoDisp::Clone() const
{
  DRT::ELEMENTS::SoDisp* newelement = new DRT::ELEMENTS::SoDisp(*this);
  return newelement;
}

/*----------------------------------------------------------------------*
 |                                                             (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
DRT::Element::DiscretizationType DRT::ELEMENTS::SoDisp::Shape() const
{
    switch (NumNode())
    {
    case  4: return tet4;
    case  8: return hex8;
    case 10: return tet10;
    case 20: return hex20;
    case 27: return hex27;
    case  6: return wedge6;
    case 15: return wedge15;
    case  5: return pyramid5;
    default:
      dserror("unexpected number of nodes %d", NumNode());
    }
    return dis_none;
}

/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoDisp::Pack(vector<char>& data) const
{
  data.resize(0);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // add base class Element
  vector<char> basedata(0);
  Element::Pack(basedata);
  AddtoPack(data,basedata);

  AddtoPack(data,stresstype_);
  AddtoPack(data,kintype_);
  AddtoPack(data,gaussrule_); //implicit conversion from enum to integer
  AddtoPack(data,numnod_disp_);
  AddtoPack(data,numdof_disp_);
  AddtoPack(data,numgpt_disp_);

  return;
}


/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoDisp::Unpack(const vector<char>& data)
{
  int position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");
  // extract base class Element
  vector<char> basedata(0);
  ExtractfromPack(position,data,basedata);
  Element::Unpack(basedata);

  ExtractfromPack(position,data,stresstype_);
  ExtractfromPack(position,data,kintype_);

  int gausrule_integer;
  ExtractfromPack(position,data,gausrule_integer);
  gaussrule_ = GaussRule3D(gausrule_integer); //explicit conversion from integer to enum

  ExtractfromPack(position,data,numnod_disp_);
  ExtractfromPack(position,data,numdof_disp_);
  ExtractfromPack(position,data,numgpt_disp_);

  if (position != (int)data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
  return;
}


/*----------------------------------------------------------------------*
 |  dtor (public)                                              maf 04/07|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::SoDisp::~SoDisp()
{
  return;
}


/*----------------------------------------------------------------------*
 |  print this element (public)                                maf 04/07|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoDisp::Print(ostream& os) const
{
  os << "SoDisp ";
  Element::Print(os);
  cout << endl;
  return;
}

/*----------------------------------------------------------------------*
 |  allocate and return SoDispRegister (public)                maf 04/07|
 *----------------------------------------------------------------------*/
RefCountPtr<DRT::ElementRegister> DRT::ELEMENTS::SoDisp::ElementRegister() const
{
  return rcp(new DRT::ELEMENTS::SoDispRegister(Type()));
}


/*----------------------------------------------------------------------*
 |  get vector of lines              (public)                  gjb 03/07|
 *----------------------------------------------------------------------*/
vector<RCP<DRT::Element> > DRT::ELEMENTS::SoDisp::Lines()
{
  // do NOT store line or surface elements inside the parent element
  // after their creation.
  // Reason: if a Redistribute() is performed on the discretization,
  // stored node ids and node pointers owned by these boundary elements might
  // have become illegal and you will get a nice segmentation fault ;-)

  // so we have to allocate new line elements:
  return DRT::UTILS::ElementBoundaryFactory<StructuralLine,DRT::Element>(DRT::UTILS::buildLines,this);
}


/*----------------------------------------------------------------------*
 |  get vector of surfaces (public)                            gjb 05/08|
 *----------------------------------------------------------------------*/
vector<RCP<DRT::Element> > DRT::ELEMENTS::SoDisp::Surfaces()
{
  // do NOT store line or surface elements inside the parent element
  // after their creation.
  // Reason: if a Redistribute() is performed on the discretization,
  // stored node ids and node pointers owned by these boundary elements might
  // have become illegal and you will get a nice segmentation fault ;-)

  // so we have to allocate new surface elements:
  return DRT::UTILS::ElementBoundaryFactory<StructuralSurface,DRT::Element>(DRT::UTILS::buildSurfaces,this);
}


/*----------------------------------------------------------------------*
 |  get vector of volumes (length 1) (public)                  maf 04/07|
 *----------------------------------------------------------------------*/
vector<RCP<DRT::Element> > DRT::ELEMENTS::SoDisp::Volumes()
{
  vector<RCP<Element> > volumes(1);
  volumes[0]= rcp(this, false);
  return volumes;
}


//=======================================================================
//=======================================================================
//=======================================================================
//=======================================================================

/*----------------------------------------------------------------------*
 |  ctor (public)                                              maf 04/07|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::SoDispRegister::SoDispRegister(DRT::Element::ElementType etype) :
ElementRegister(etype)
{
  return;
}

/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                         maf 04/07|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::SoDispRegister::SoDispRegister(
                               const DRT::ELEMENTS::SoDispRegister& old) :
ElementRegister(old)
{
  return;
}

/*----------------------------------------------------------------------*
 |  Deep copy this instance return pointer to it               (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::SoDispRegister* DRT::ELEMENTS::SoDispRegister::Clone() const
{
  return new DRT::ELEMENTS::SoDispRegister(*this);
}

/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoDispRegister::Pack(vector<char>& data) const
{
  data.resize(0);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // add base class ElementRegister
  vector<char> basedata(0);
  ElementRegister::Pack(basedata);
  AddtoPack(data,basedata);

  return;
}


/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 |                                                            maf 04/07 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoDispRegister::Unpack(const vector<char>& data)
{
  int position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");
  // base class ElementRegister
  vector<char> basedata(0);
  ExtractfromPack(position,data,basedata);
  ElementRegister::Unpack(basedata);

  if (position != (int)data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
  return;
}


/*----------------------------------------------------------------------*
 |  dtor (public)                                              maf 04/07|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::SoDispRegister::~SoDispRegister()
{
  return;
}

/*----------------------------------------------------------------------*
 |  print (public)                                             maf 04/07|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::SoDispRegister::Print(ostream& os) const
{
  os << "SoDispRegister ";
  ElementRegister::Print(os);
  return;
}

/*----------------------------------------------------------------------*
 |  init the element (public)                                      12/06|
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::SoDispRegister::Initialize(DRT::Discretization& dis)
{
  return 0;
}

#endif  // #ifdef CCADISCRET
#endif  // #ifdef D_SOLID3

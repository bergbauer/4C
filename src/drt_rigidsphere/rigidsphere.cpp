/*!----------------------------------------------------------------------
\file rigidsphere.cpp

\brief spherical particle element for brownian dynamics

<pre>
Maintainer: Christoph Meier
            meier@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15301
</pre>

*----------------------------------------------------------------------*/

#include "rigidsphere.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_inpar/drt_validparameters.H"
#include "../linalg/linalg_fixedsizematrix.H"
#include "../drt_fem_general/largerotations.H"
#include "../drt_lib/drt_linedefinition.H"
#include "../drt_fem_general/drt_utils_fem_shapefunctions.H"
#include "../drt_fem_general/drt_utils_integration.H"
#include "../drt_inpar/inpar_statmech.H"


DRT::ELEMENTS::RigidsphereType DRT::ELEMENTS::RigidsphereType::instance_;


DRT::ParObject* DRT::ELEMENTS::RigidsphereType::Create( const std::vector<char> & data )
{
  DRT::ELEMENTS::Rigidsphere* object = new DRT::ELEMENTS::Rigidsphere(-1,-1);
  object->Unpack(data);
  return (object);
}

Teuchos::RCP<DRT::Element> DRT::ELEMENTS::RigidsphereType::Create(const std::string eletype,
																 															const std::string eledistype,
																 															const int id,
																 															const int owner )
{
  if ( eletype=="RIGIDSPHERE" )
  {
    Teuchos::RCP<DRT::Element> ele = Teuchos::rcp(new DRT::ELEMENTS::Rigidsphere(id,owner));
    return (ele);
  }
  return (Teuchos::null);
}

Teuchos::RCP<DRT::Element> DRT::ELEMENTS::RigidsphereType::Create( const int id, const int owner )

{
  return (Teuchos::rcp( new Rigidsphere( id, owner ) ));
}


void DRT::ELEMENTS::RigidsphereType::NodalBlockInformation( DRT::Element * dwele, int & numdf, int & dimns, int & nv, int & np )
{
  numdf = 3;
  nv = 3;
  dimns = 3;
}

// TODO: the function ComputeNullSpace has still to be implemented
void DRT::ELEMENTS::RigidsphereType::ComputeNullSpace( DRT::Discretization & dis, std::vector<double> & ns, const double * x0, int numdf, int dimns )
{
  dserror("Function not implemented yet.");
  //DRT::UTILS::ComputeXFluid3DNullSpace( dis, ns, x0, numdf, dimns );
}

void DRT::ELEMENTS::RigidsphereType::SetupElementDefinition( std::map<std::string,std::map<std::string,DRT::INPUT::LineDefinition> > & definitions )
{
  std::map<std::string,DRT::INPUT::LineDefinition>& defs = definitions["RIGIDSPHERE"];

  defs["POINT1"]
    .AddIntVector("POINT1",1)
    .AddNamedDouble("RADIUS")
    .AddNamedDouble("DENSITY")
    ;

}


/*----------------------------------------------------------------------*
 |  ctor (public)                                            meier 05/12|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Rigidsphere::Rigidsphere(int id, int owner) :
DRT::Element(id,owner),
radius_(0.0),
rho_(0.0)
{
  return;
}
/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                       meier 05/12|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Rigidsphere::Rigidsphere(const DRT::ELEMENTS::Rigidsphere& old) :
 DRT::Element(old),
 radius_(old.radius_),
 rho_(old.rho_)
{
  return;
}
/*----------------------------------------------------------------------*
 |  Deep copy this instance of Rigidsphere and return pointer to it (public) |
 |                                                            meier 05/12 |
 *----------------------------------------------------------------------*/
DRT::Element* DRT::ELEMENTS::Rigidsphere::Clone() const
{
  DRT::ELEMENTS::Rigidsphere* newelement = new DRT::ELEMENTS::Rigidsphere(*this);
  return (newelement);
}

/*----------------------------------------------------------------------*
 |  dtor (public)                                            meier 05/12 |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Rigidsphere::~Rigidsphere()
{
  return;
}


/*----------------------------------------------------------------------*
 |  print this element (public)                              meier 05/12
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Rigidsphere::Print(std::ostream& os) const
{
  return;
}


/*----------------------------------------------------------------------*
 |                                                             (public) |
 |                                                          meier 05/12 |
 *----------------------------------------------------------------------*/
DRT::Element::DiscretizationType DRT::ELEMENTS::Rigidsphere::Shape() const
{
	return (point1);
}

/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                           meier 05/12/
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Rigidsphere::Pack(DRT::PackBuffer& data) const
{

  DRT::PackBuffer::SizeMarker sm( data );
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // add base class Element
  Element::Pack(data);

  //add all class variables of beam2r element
  AddtoPack(data,radius_);
  AddtoPack(data,rho_);

  return;
}

/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 |                                                           meier 05/12|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Rigidsphere::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");
  // extract base class Element
  std::vector<char> basedata(0);
  ExtractfromPack(position,data,basedata);
  Element::Unpack(basedata);


  //extract all class variables
  ExtractfromPack(position,data,radius_);
  ExtractfromPack(position,data,rho_);

  if (position != data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
  return;
}

/*----------------------------------------------------------------------*
 |  get vector of lines (public)                             meier 02/14|
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<DRT::Element> > DRT::ELEMENTS::Rigidsphere::Lines()
{
  std::vector<Teuchos::RCP<Element> > lines(1);
  lines[0]= Teuchos::rcp(this, false);
  return (lines);
}


/*----------------------------------------------------------------------*
 |  Initialize (public)                                      meier 05/12|
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::RigidsphereType::Initialize(DRT::Discretization& dis)
{
    return 0;
}

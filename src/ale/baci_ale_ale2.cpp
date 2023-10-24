/*----------------------------------------------------------------------------*/
/*! \file

\brief ALE element for 2D case


\level 1
*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#include "baci_ale_ale2.H"

#include "baci_ale_ale2_nurbs.H"
#include "baci_io_linedefinition.H"
#include "baci_lib_discret.H"
#include "baci_lib_utils_factory.H"
#include "baci_so3_nullspace.H"
#include "baci_utils_exceptions.H"

DRT::ELEMENTS::Ale2Type DRT::ELEMENTS::Ale2Type::instance_;

DRT::ELEMENTS::Ale2Type& DRT::ELEMENTS::Ale2Type::Instance() { return instance_; }

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
DRT::ParObject* DRT::ELEMENTS::Ale2Type::Create(const std::vector<char>& data)
{
  DRT::ELEMENTS::Ale2* object = new DRT::ELEMENTS::Ale2(-1, -1);
  object->Unpack(data);
  return object;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::Ale2Type::Create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  Teuchos::RCP<DRT::Element> ele;

  if (eletype == "ALE2")
  {
    if (eledistype != "NURBS4" and eledistype != "NURBS9")
    {
      ele = Teuchos::rcp(new DRT::ELEMENTS::Ale2(id, owner));
    }
  }

  return ele;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::Ale2Type::Create(const int id, const int owner)
{
  return Teuchos::rcp(new DRT::ELEMENTS::Ale2(id, owner));
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void DRT::ELEMENTS::Ale2Type::NodalBlockInformation(
    DRT::Element* dwele, int& numdf, int& dimns, int& nv, int& np)
{
  numdf = 2;
  dimns = 3;
  nv = 2;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
CORE::LINALG::SerialDenseMatrix DRT::ELEMENTS::Ale2Type::ComputeNullSpace(
    DRT::Node& node, const double* x0, const int numdof, const int dimnsp)
{
  return ComputeSolid2DNullSpace(node, x0);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void DRT::ELEMENTS::Ale2Type::SetupElementDefinition(
    std::map<std::string, std::map<std::string, DRT::INPUT::LineDefinition>>& definitions)
{
  std::map<std::string, DRT::INPUT::LineDefinition>& defs = definitions["ALE2"];

  defs["QUAD4"] =
      INPUT::LineDefinition::Builder().AddIntVector("QUAD4", 4).AddNamedInt("MAT").Build();

  defs["QUAD8"] =
      INPUT::LineDefinition::Builder().AddIntVector("QUAD8", 8).AddNamedInt("MAT").Build();

  defs["QUAD9"] =
      INPUT::LineDefinition::Builder().AddIntVector("QUAD9", 9).AddNamedInt("MAT").Build();

  defs["TRI3"] =
      INPUT::LineDefinition::Builder().AddIntVector("TRI3", 3).AddNamedInt("MAT").Build();

  defs["TRI6"] =
      INPUT::LineDefinition::Builder().AddIntVector("TRI6", 6).AddNamedInt("MAT").Build();
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> DRT::ELEMENTS::Ale2LineType::Create(const int id, const int owner)
{
  // return Teuchos::rcp( new Ale2Line( id, owner ) );
  return Teuchos::null;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
DRT::ELEMENTS::Ale2::Ale2(int id, int owner) : DRT::Element(id, owner) {}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
DRT::ELEMENTS::Ale2::Ale2(const DRT::ELEMENTS::Ale2& old) : DRT::Element(old) {}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
DRT::Element* DRT::ELEMENTS::Ale2::Clone() const
{
  DRT::ELEMENTS::Ale2* newelement = new DRT::ELEMENTS::Ale2(*this);
  return newelement;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
DRT::Element::DiscretizationType DRT::ELEMENTS::Ale2::Shape() const
{
  switch (NumNode())
  {
    case 3:
      return DiscretizationType::tri3;
    case 4:
      return DiscretizationType::quad4;
    case 6:
      return DiscretizationType::tri6;
    case 8:
      return DiscretizationType::quad8;
    case 9:
      return DiscretizationType::quad9;
    default:
      dserror("unexpected number of nodes %d", NumNode());
      break;
  }
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void DRT::ELEMENTS::Ale2::Pack(DRT::PackBuffer& data) const
{
  DRT::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data, type);
  // add base class Element
  Element::Pack(data);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void DRT::ELEMENTS::Ale2::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position, data, type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");
  // extract base class Element
  std::vector<char> basedata(0);
  ExtractfromPack(position, data, basedata);
  Element::Unpack(basedata);

  if (position != data.size())
    dserror("Mismatch in size of data %d <-> %d", (int)data.size(), position);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
void DRT::ELEMENTS::Ale2::Print(std::ostream& os) const
{
  os << "Ale2 ";
  Element::Print(os);
  std::cout << std::endl;
  return;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
std::vector<Teuchos::RCP<DRT::Element>> DRT::ELEMENTS::Ale2::Lines()
{
  return DRT::UTILS::ElementBoundaryFactory<Ale2Line, Ale2>(DRT::UTILS::buildLines, *this);
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
std::vector<Teuchos::RCP<DRT::Element>> DRT::ELEMENTS::Ale2::Surfaces()
{
  return {Teuchos::rcpFromRef(*this)};
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
CORE::DRT::UTILS::GaussRule2D DRT::ELEMENTS::Ale2::getOptimalGaussrule(
    const DiscretizationType& distype)
{
  CORE::DRT::UTILS::GaussRule2D rule = CORE::DRT::UTILS::GaussRule2D::undefined;
  switch (distype)
  {
    case DiscretizationType::quad4:
    case DiscretizationType::nurbs4:
      rule = CORE::DRT::UTILS::GaussRule2D::quad_4point;
      break;
    case DiscretizationType::quad8:
    case DiscretizationType::quad9:
    case DiscretizationType::nurbs9:
      rule = CORE::DRT::UTILS::GaussRule2D::quad_9point;
      break;
    case DiscretizationType::tri3:
      rule = CORE::DRT::UTILS::GaussRule2D::tri_3point;
      break;
    case DiscretizationType::tri6:
      rule = CORE::DRT::UTILS::GaussRule2D::tri_6point;
      break;
    default:
      dserror("unknown number of nodes for gaussrule initialization");
      break;
  }
  return rule;
}

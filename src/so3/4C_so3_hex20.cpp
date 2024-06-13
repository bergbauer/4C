/*----------------------------------------------------------------------*/
/*! \file

\brief 3D quadratic serendipity element

\level 1


*----------------------------------------------------------------------*/

#include "4C_so3_hex20.hpp"

#include "4C_comm_utils_factory.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_fem_general_utils_fem_shapefunctions.hpp"
#include "4C_global_data.hpp"
#include "4C_io_linedefinition.hpp"
#include "4C_mat_so3_material.hpp"
#include "4C_so3_line.hpp"
#include "4C_so3_nullspace.hpp"
#include "4C_so3_prestress.hpp"
#include "4C_so3_prestress_service.hpp"
#include "4C_so3_surface.hpp"
#include "4C_so3_utils.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

Discret::ELEMENTS::SoHex20Type Discret::ELEMENTS::SoHex20Type::instance_;

Discret::ELEMENTS::SoHex20Type& Discret::ELEMENTS::SoHex20Type::Instance() { return instance_; }

Core::Communication::ParObject* Discret::ELEMENTS::SoHex20Type::Create(
    const std::vector<char>& data)
{
  auto* object = new Discret::ELEMENTS::SoHex20(-1, -1);
  object->Unpack(data);
  return object;
}


Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::SoHex20Type::Create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  if (eletype == get_element_type_string())
  {
    Teuchos::RCP<Core::Elements::Element> ele =
        Teuchos::rcp(new Discret::ELEMENTS::SoHex20(id, owner));
    return ele;
  }
  return Teuchos::null;
}


Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::SoHex20Type::Create(
    const int id, const int owner)
{
  Teuchos::RCP<Core::Elements::Element> ele =
      Teuchos::rcp(new Discret::ELEMENTS::SoHex20(id, owner));
  return ele;
}


void Discret::ELEMENTS::SoHex20Type::nodal_block_information(
    Core::Elements::Element* dwele, int& numdf, int& dimns, int& nv, int& np)
{
  numdf = 3;
  dimns = 6;
  nv = 3;
}

Core::LinAlg::SerialDenseMatrix Discret::ELEMENTS::SoHex20Type::ComputeNullSpace(
    Core::Nodes::Node& node, const double* x0, const int numdof, const int dimnsp)
{
  return ComputeSolid3DNullSpace(node, x0);
}

void Discret::ELEMENTS::SoHex20Type::setup_element_definition(
    std::map<std::string, std::map<std::string, Input::LineDefinition>>& definitions)
{
  std::map<std::string, Input::LineDefinition>& defs = definitions[get_element_type_string()];

  defs["HEX20"] = Input::LineDefinition::Builder()
                      .add_int_vector("HEX20", 20)
                      .add_named_int("MAT")
                      .add_named_string("KINEM")
                      .add_optional_named_double_vector("RAD", 3)
                      .add_optional_named_double_vector("AXI", 3)
                      .add_optional_named_double_vector("CIR", 3)
                      .add_optional_named_double_vector("FIBER1", 3)
                      .add_optional_named_double_vector("FIBER2", 3)
                      .add_optional_named_double_vector("FIBER3", 3)
                      .add_optional_named_double("STRENGTH")
                      .add_optional_named_double("GROWTHTRIG")
                      .Build();
}


/*----------------------------------------------------------------------*
 |  ctor (public)                                                       |
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
Discret::ELEMENTS::SoHex20::SoHex20(int id, int owner)
    : SoBase(id, owner), pstype_(Inpar::STR::PreStress::none), pstime_(0.0), time_(0.0)
{
  invJ_.resize(NUMGPT_SOH20, Core::LinAlg::Matrix<NUMDIM_SOH20, NUMDIM_SOH20>(true));
  detJ_.resize(NUMGPT_SOH20, 0.0);

  Teuchos::RCP<const Teuchos::ParameterList> params =
      Global::Problem::Instance()->getParameterList();
  if (params != Teuchos::null)
  {
    pstype_ = Prestress::GetType();
    pstime_ = Prestress::GetPrestressTime();

    Discret::ELEMENTS::UTILS::ThrowErrorFDMaterialTangent(
        Global::Problem::Instance()->structural_dynamic_params(), get_element_type_string());
  }
  if (Prestress::IsMulf(pstype_))
    prestress_ = Teuchos::rcp(new Discret::ELEMENTS::PreStress(NUMNOD_SOH20, NUMGPT_SOH20));

  return;
}

/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                                  |
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
Discret::ELEMENTS::SoHex20::SoHex20(const Discret::ELEMENTS::SoHex20& old)
    : SoBase(old), detJ_(old.detJ_), pstype_(old.pstype_), pstime_(old.pstime_), time_(old.time_)
{
  invJ_.resize(old.invJ_.size());
  for (int i = 0; i < (int)invJ_.size(); ++i)
  {
    // can this size be anything but NUMDIM_SOH20 x NUMDIM_SOH20?
    // invJ_[i].Shape(old.invJ_[i].numRows(),old.invJ_[i].numCols());
    invJ_[i] = old.invJ_[i];
  }

  if (Prestress::IsMulf(pstype_))
    prestress_ = Teuchos::rcp(new Discret::ELEMENTS::PreStress(*(old.prestress_)));

  return;
}

/*----------------------------------------------------------------------*
 |  Deep copy this instance of Solid3 and return pointer to it (public) |
 *----------------------------------------------------------------------*/
Core::Elements::Element* Discret::ELEMENTS::SoHex20::Clone() const
{
  auto* newelement = new Discret::ELEMENTS::SoHex20(*this);
  return newelement;
}

/*----------------------------------------------------------------------*
 |                                                             (public) |
 *----------------------------------------------------------------------*/
Core::FE::CellType Discret::ELEMENTS::SoHex20::Shape() const { return Core::FE::CellType::hex20; }

/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::SoHex20::Pack(Core::Communication::PackBuffer& data) const
{
  Core::Communication::PackBuffer::SizeMarker sm(data);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  add_to_pack(data, type);
  // add base class Element
  SoBase::Pack(data);

  // detJ_
  add_to_pack(data, detJ_);

  // invJ_
  const auto size = (int)invJ_.size();
  add_to_pack(data, size);
  for (int i = 0; i < size; ++i) add_to_pack(data, invJ_[i]);
  // Pack prestress
  add_to_pack(data, static_cast<int>(pstype_));
  add_to_pack(data, pstime_);
  add_to_pack(data, time_);
  if (Prestress::IsMulf(pstype_))
  {
    Core::Communication::ParObject::add_to_pack(data, *prestress_);
  }

  return;
}


/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::SoHex20::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  Core::Communication::ExtractAndAssertId(position, data, UniqueParObjectId());

  // extract base class Element
  std::vector<char> basedata(0);
  extract_from_pack(position, data, basedata);
  SoBase::Unpack(basedata);

  // detJ_
  extract_from_pack(position, data, detJ_);
  // invJ_
  int size = 0;
  extract_from_pack(position, data, size);
  invJ_.resize(size, Core::LinAlg::Matrix<NUMDIM_SOH20, NUMDIM_SOH20>(true));
  for (int i = 0; i < size; ++i) extract_from_pack(position, data, invJ_[i]);

  // Extract prestress
  pstype_ = static_cast<Inpar::STR::PreStress>(extract_int(position, data));
  extract_from_pack(position, data, pstime_);
  extract_from_pack(position, data, time_);
  if (Prestress::IsMulf(pstype_))
  {
    std::vector<char> tmpprestress(0);
    extract_from_pack(position, data, tmpprestress);
    if (prestress_ == Teuchos::null)
      prestress_ = Teuchos::rcp(new Discret::ELEMENTS::PreStress(NUMNOD_SOH20, NUMGPT_SOH20));
    prestress_->Unpack(tmpprestress);
  }

  if (position != data.size())
    FOUR_C_THROW("Mismatch in size of data %d <-> %d", (int)data.size(), position);
  return;
}



/*----------------------------------------------------------------------*
 |  print this element (public)                                         |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::SoHex20::Print(std::ostream& os) const
{
  os << "So_hex20 ";
  Element::Print(os);
  std::cout << std::endl;
  return;
}

/*----------------------------------------------------------------------*
|  get vector of surfaces (public)                                      |
|  surface normals always point outward                                 |
*----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::SoHex20::Surfaces()
{
  return Core::Communication::ElementBoundaryFactory<StructuralSurface, Core::Elements::Element>(
      Core::Communication::buildSurfaces, *this);
}

/*----------------------------------------------------------------------*
 |  get vector of lines (public)                                        |
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::SoHex20::Lines()
{
  return Core::Communication::ElementBoundaryFactory<StructuralLine, Core::Elements::Element>(
      Core::Communication::buildLines, *this);
}

/*----------------------------------------------------------------------*
 |  Return names of visualization data (public)                         |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::SoHex20::VisNames(std::map<std::string, int>& names)
{
  SolidMaterial()->VisNames(names);
  return;
}

/*----------------------------------------------------------------------*
 |  Return visualization data (public)                                  |
 *----------------------------------------------------------------------*/
bool Discret::ELEMENTS::SoHex20::VisData(const std::string& name, std::vector<double>& data)
{
  // Put the owner of this element into the file (use base class method for this)
  if (Core::Elements::Element::VisData(name, data)) return true;

  return SolidMaterial()->VisData(name, data, NUMGPT_SOH20, this->Id());
}

FOUR_C_NAMESPACE_CLOSE

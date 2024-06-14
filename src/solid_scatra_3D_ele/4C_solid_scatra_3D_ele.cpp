/*! \file

\brief Implementation of the solid-scatra element

\level 1
*/

#include "4C_solid_scatra_3D_ele.hpp"

#include "4C_fem_general_cell_type.hpp"
#include "4C_fem_general_cell_type_traits.hpp"
#include "4C_inpar_scatra.hpp"
#include "4C_so3_line.hpp"
#include "4C_so3_nullspace.hpp"
#include "4C_so3_surface.hpp"
#include "4C_solid_scatra_3D_ele_factory.hpp"
#include "4C_solid_scatra_3D_ele_lib.hpp"

FOUR_C_NAMESPACE_OPEN

namespace
{
  template <Core::FE::CellType celltype>
  Input::LineDefinition::Builder GetDefaultLineDefinitionBuilder()
  {
    return Input::LineDefinition::Builder()
        .add_int_vector(Core::FE::CellTypeToString(celltype), Core::FE::num_nodes<celltype>)
        .add_named_int("MAT")
        .add_named_string("KINEM")
        .add_named_string("TYPE")
        .add_optional_named_string("PRESTRESS_TECH")
        .add_optional_named_double_vector("RAD", 3)
        .add_optional_named_double_vector("AXI", 3)
        .add_optional_named_double_vector("CIR", 3)
        .add_optional_named_double_vector("FIBER1", 3)
        .add_optional_named_double_vector("FIBER2", 3)
        .add_optional_named_double_vector("FIBER3", 3);
  }
}  // namespace

Discret::ELEMENTS::SolidScatraType Discret::ELEMENTS::SolidScatraType::instance_;

Discret::ELEMENTS::SolidScatraType& Discret::ELEMENTS::SolidScatraType::Instance()
{
  return instance_;
}

void Discret::ELEMENTS::SolidScatraType::setup_element_definition(
    std::map<std::string, std::map<std::string, Input::LineDefinition>>& definitions)
{
  std::map<std::string, Input::LineDefinition>& defsgeneral = definitions["SOLIDSCATRA"];

  defsgeneral[Core::FE::CellTypeToString(Core::FE::CellType::hex8)] =
      GetDefaultLineDefinitionBuilder<Core::FE::CellType::hex8>()
          .add_optional_named_string("TECH")
          .build();

  defsgeneral[Core::FE::CellTypeToString(Core::FE::CellType::hex27)] =
      GetDefaultLineDefinitionBuilder<Core::FE::CellType::hex27>().build();

  defsgeneral[Core::FE::CellTypeToString(Core::FE::CellType::tet4)] =
      GetDefaultLineDefinitionBuilder<Core::FE::CellType::tet4>().build();

  defsgeneral[Core::FE::CellTypeToString(Core::FE::CellType::tet10)] =
      GetDefaultLineDefinitionBuilder<Core::FE::CellType::tet10>().build();
}

Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::SolidScatraType::Create(
    const std::string eletype, const std::string elecelltype, const int id, const int owner)
{
  if (eletype == "SOLIDSCATRA") return Create(id, owner);
  return Teuchos::null;
}

Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::SolidScatraType::Create(
    const int id, const int owner)
{
  return Teuchos::rcp(new Discret::ELEMENTS::SolidScatra(id, owner));
}

Core::Communication::ParObject* Discret::ELEMENTS::SolidScatraType::Create(
    const std::vector<char>& data)
{
  auto* object = new Discret::ELEMENTS::SolidScatra(-1, -1);
  object->Unpack(data);
  return object;
}

void Discret::ELEMENTS::SolidScatraType::nodal_block_information(
    Core::Elements::Element* dwele, int& numdf, int& dimns, int& nv, int& np)
{
  STR::UTILS::nodal_block_information_solid(dwele, numdf, dimns, nv, np);
}

Core::LinAlg::SerialDenseMatrix Discret::ELEMENTS::SolidScatraType::ComputeNullSpace(
    Core::Nodes::Node& node, const double* x0, const int numdof, const int dimnsp)
{
  return ComputeSolid3DNullSpace(node, x0);
}

Discret::ELEMENTS::SolidScatra::SolidScatra(int id, int owner) : Core::Elements::Element(id, owner)
{
}

Core::Elements::Element* Discret::ELEMENTS::SolidScatra::Clone() const
{
  return new Discret::ELEMENTS::SolidScatra(*this);
}

int Discret::ELEMENTS::SolidScatra::NumLine() const
{
  return Core::FE::getNumberOfElementLines(celltype_);
}

int Discret::ELEMENTS::SolidScatra::NumSurface() const
{
  return Core::FE::getNumberOfElementSurfaces(celltype_);
}

int Discret::ELEMENTS::SolidScatra::NumVolume() const
{
  return Core::FE::getNumberOfElementVolumes(celltype_);
}

std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::SolidScatra::Lines()
{
  return Core::Communication::GetElementLines<StructuralLine, SolidScatra>(*this);
}

std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::SolidScatra::Surfaces()
{
  return Core::Communication::GetElementSurfaces<StructuralSurface, SolidScatra>(*this);
}

void Discret::ELEMENTS::SolidScatra::set_params_interface_ptr(const Teuchos::ParameterList& p)
{
  if (p.isParameter("interface"))
  {
    interface_ptr_ = Teuchos::rcp_dynamic_cast<STR::ELEMENTS::ParamsInterface>(
        p.get<Teuchos::RCP<Core::Elements::ParamsInterface>>("interface"));
  }
  else
    interface_ptr_ = Teuchos::null;
}

bool Discret::ELEMENTS::SolidScatra::ReadElement(
    const std::string& eletype, const std::string& celltype, Input::LineDefinition* linedef)
{
  // read base element
  // set cell type
  celltype_ = Core::FE::StringToCellType(celltype);

  // read number of material model
  SetMaterial(0, Mat::Factory(STR::UTILS::ReadElement::read_element_material(linedef)));

  // read scalar transport implementation type
  properties_.impltype = ReadScatraImplType(*linedef);


  properties_.solid = STR::UTILS::ReadElement::read_solid_element_properties(linedef);

  solid_scatra_calc_variant_ =
      create_solid_scatra_calculation_interface(celltype_, properties_.solid);

  // setup solid material
  std::visit([&](auto& solid_scatra) { solid_scatra->Setup(SolidMaterial(), linedef); },
      solid_scatra_calc_variant_);

  return true;
}

void Discret::ELEMENTS::SolidScatra::Pack(Core::Communication::PackBuffer& data) const
{
  Core::Communication::PackBuffer::SizeMarker sm(data);

  add_to_pack(data, UniqueParObjectId());

  // add base class Element
  Core::Elements::Element::Pack(data);

  add_to_pack(data, (int)celltype_);
  Discret::ELEMENTS::add_to_pack(data, properties_);

  data.add_to_pack(material_post_setup_);

  // optional data, e.g., EAS data
  Discret::ELEMENTS::Pack(solid_scatra_calc_variant_, data);
}

void Discret::ELEMENTS::SolidScatra::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  if (extract_int(position, data) != UniqueParObjectId()) FOUR_C_THROW("wrong instance type data");

  // extract base class Element
  std::vector<char> basedata(0);
  extract_from_pack(position, data, basedata);
  Core::Elements::Element::Unpack(basedata);

  celltype_ = static_cast<Core::FE::CellType>(extract_int(position, data));

  Discret::ELEMENTS::extract_from_pack(position, data, properties_);

  Core::Communication::ParObject::extract_from_pack(position, data, material_post_setup_);

  // reset solid and scatra interfaces
  solid_scatra_calc_variant_ =
      create_solid_scatra_calculation_interface(celltype_, properties_.solid);

  Discret::ELEMENTS::Unpack(solid_scatra_calc_variant_, position, data);

  if (position != data.size())
    FOUR_C_THROW("Mismatch in size of data %d <-> %d", (int)data.size(), position);
}

void Discret::ELEMENTS::SolidScatra::VisNames(std::map<std::string, int>& names)
{
  Core::Elements::Element::VisNames(names);
  SolidMaterial().VisNames(names);
}

bool Discret::ELEMENTS::SolidScatra::VisData(const std::string& name, std::vector<double>& data)
{
  // Put the owner of this element into the file (use base class method for this)
  if (Core::Elements::Element::VisData(name, data)) return true;

  return SolidMaterial().VisData(name, data, Id());
}

Mat::So3Material& Discret::ELEMENTS::SolidScatra::SolidMaterial(int nummat) const
{
  return *Teuchos::rcp_dynamic_cast<Mat::So3Material>(
      Core::Elements::Element::Material(nummat), true);
}

FOUR_C_NAMESPACE_CLOSE
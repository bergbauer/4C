/*----------------------------------------------------------------------*/
/*! \file

\brief Solid Tet10 Element

\level 1


*----------------------------------------------------------------------*/

#include "4C_so3_tet10.hpp"

#include "4C_comm_utils_factory.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_fem_general_fiber_node.hpp"
#include "4C_fem_general_fiber_node_holder.hpp"
#include "4C_fem_general_fiber_node_utils.hpp"
#include "4C_fem_general_utils_fem_shapefunctions.hpp"
#include "4C_fem_general_utils_integration.hpp"
#include "4C_global_data.hpp"
#include "4C_io_linedefinition.hpp"
#include "4C_mat_so3_material.hpp"
#include "4C_so3_element_service.hpp"
#include "4C_so3_line.hpp"
#include "4C_so3_nullspace.hpp"
#include "4C_so3_prestress.hpp"
#include "4C_so3_prestress_service.hpp"
#include "4C_so3_surface.hpp"
#include "4C_so3_utils.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN
// remove later


Discret::ELEMENTS::SoTet10Type Discret::ELEMENTS::SoTet10Type::instance_;

Discret::ELEMENTS::SoTet10Type& Discret::ELEMENTS::SoTet10Type::Instance() { return instance_; }

Core::Communication::ParObject* Discret::ELEMENTS::SoTet10Type::Create(
    const std::vector<char>& data)
{
  auto* object = new Discret::ELEMENTS::SoTet10(-1, -1);
  object->Unpack(data);
  return object;
}


Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::SoTet10Type::Create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  if (eletype == get_element_type_string())
  {
    Teuchos::RCP<Core::Elements::Element> ele =
        Teuchos::rcp(new Discret::ELEMENTS::SoTet10(id, owner));
    return ele;
  }
  return Teuchos::null;
}


Teuchos::RCP<Core::Elements::Element> Discret::ELEMENTS::SoTet10Type::Create(
    const int id, const int owner)
{
  Teuchos::RCP<Core::Elements::Element> ele =
      Teuchos::rcp(new Discret::ELEMENTS::SoTet10(id, owner));
  return ele;
}


void Discret::ELEMENTS::SoTet10Type::nodal_block_information(
    Core::Elements::Element* dwele, int& numdf, int& dimns, int& nv, int& np)
{
  numdf = 3;
  dimns = 6;
  nv = 3;
}

Core::LinAlg::SerialDenseMatrix Discret::ELEMENTS::SoTet10Type::ComputeNullSpace(
    Core::Nodes::Node& node, const double* x0, const int numdof, const int dimnsp)
{
  return ComputeSolid3DNullSpace(node, x0);
}

void Discret::ELEMENTS::SoTet10Type::setup_element_definition(
    std::map<std::string, std::map<std::string, Input::LineDefinition>>& definitions)
{
  std::map<std::string, Input::LineDefinition>& defs = definitions[get_element_type_string()];

  defs["TET10"] = Input::LineDefinition::Builder()
                      .add_int_vector("TET10", 10)
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
                      .build();
}


/*----------------------------------------------------------------------***
 |  ctor (public)                                                       |
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
Discret::ELEMENTS::SoTet10::SoTet10(int id, int owner)
    : SoBase(id, owner), pstype_(Inpar::STR::PreStress::none), pstime_(0.0), time_(0.0)
{
  invJ_.resize(NUMGPT_SOTET10, Core::LinAlg::Matrix<NUMDIM_SOTET10, NUMDIM_SOTET10>(true));
  detJ_.resize(NUMGPT_SOTET10, 0.0);
  invJ_mass_.resize(
      NUMGPT_MASS_SOTET10, Core::LinAlg::Matrix<NUMDIM_SOTET10, NUMDIM_SOTET10>(true));
  detJ_mass_.resize(NUMGPT_MASS_SOTET10, 0.0);

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
    prestress_ = Teuchos::rcp(new Discret::ELEMENTS::PreStress(NUMNOD_SOTET10, NUMGPT_SOTET10));

  return;
}

/*----------------------------------------------------------------------***
 |  copy-ctor (public)                                                  |
 |  id             (in)  this element's global id                       |
 *----------------------------------------------------------------------*/
Discret::ELEMENTS::SoTet10::SoTet10(const Discret::ELEMENTS::SoTet10& old)
    : SoBase(old),
      detJ_(old.detJ_),
      detJ_mass_(old.detJ_mass_),
      pstype_(old.pstype_),
      pstime_(old.pstime_),
      time_(old.time_)
// try out later detJ_(old.detJ_)
{
  invJ_.resize(old.invJ_.size());
  for (int i = 0; i < (int)invJ_.size(); ++i)
  {
    invJ_[i] = old.invJ_[i];
  }

  invJ_mass_.resize(old.invJ_mass_.size());
  for (int i = 0; i < (int)invJ_mass_.size(); ++i)
  {
    invJ_mass_[i] = old.invJ_mass_[i];
  }

  if (Prestress::IsMulf(pstype_))
    prestress_ = Teuchos::rcp(new Discret::ELEMENTS::PreStress(*(old.prestress_)));

  return;
}

/*----------------------------------------------------------------------***
 |  Deep copy this instance of Solid3 and return pointer to it (public) |
 |                                                                      |
 *----------------------------------------------------------------------*/
Core::Elements::Element* Discret::ELEMENTS::SoTet10::Clone() const
{
  auto* newelement = new Discret::ELEMENTS::SoTet10(*this);
  return newelement;
}

/*----------------------------------------------------------------------***
 |                                                             (public) |
 |                                                                      |
 *----------------------------------------------------------------------*/
Core::FE::CellType Discret::ELEMENTS::SoTet10::Shape() const { return Core::FE::CellType::tet10; }

/*----------------------------------------------------------------------***
 |  Pack data                                                  (public) |
 |                                                                      |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::SoTet10::Pack(Core::Communication::PackBuffer& data) const
{
  Core::Communication::PackBuffer::SizeMarker sm(data);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  add_to_pack(data, type);
  // add base class Element
  SoBase::Pack(data);
  // detJ_
  add_to_pack(data, detJ_);
  add_to_pack(data, detJ_mass_);

  // invJ
  const auto size = (int)invJ_.size();
  add_to_pack(data, size);
  for (int i = 0; i < size; ++i) add_to_pack(data, invJ_[i]);

  const auto size_mass = (int)invJ_mass_.size();
  add_to_pack(data, size_mass);
  for (int i = 0; i < size_mass; ++i) add_to_pack(data, invJ_mass_[i]);

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


/*----------------------------------------------------------------------***
 |  Unpack data                                                (public) |
 |                                                                      |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::SoTet10::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  Core::Communication::ExtractAndAssertId(position, data, UniqueParObjectId());

  // extract base class Element
  std::vector<char> basedata(0);
  extract_from_pack(position, data, basedata);
  SoBase::Unpack(basedata);

  // detJ_
  extract_from_pack(position, data, detJ_);
  extract_from_pack(position, data, detJ_mass_);
  // invJ_
  int size = 0;
  extract_from_pack(position, data, size);
  invJ_.resize(size, Core::LinAlg::Matrix<NUMDIM_SOTET10, NUMDIM_SOTET10>(true));
  for (int i = 0; i < size; ++i) extract_from_pack(position, data, invJ_[i]);

  int size_mass = 0;
  extract_from_pack(position, data, size_mass);
  invJ_mass_.resize(size_mass, Core::LinAlg::Matrix<NUMDIM_SOTET10, NUMDIM_SOTET10>(true));
  for (int i = 0; i < size_mass; ++i) extract_from_pack(position, data, invJ_mass_[i]);

  // Unpack prestress
  pstype_ = static_cast<Inpar::STR::PreStress>(extract_int(position, data));
  extract_from_pack(position, data, pstime_);
  extract_from_pack(position, data, time_);
  if (Prestress::IsMulf(pstype_))
  {
    std::vector<char> tmpprestress(0);
    extract_from_pack(position, data, tmpprestress);
    if (prestress_ == Teuchos::null)
      prestress_ = Teuchos::rcp(new Discret::ELEMENTS::PreStress(NUMNOD_SOTET10, NUMGPT_SOTET10));
    prestress_->Unpack(tmpprestress);
  }

  if (position != data.size())
    FOUR_C_THROW("Mismatch in size of data %d <-> %d", (int)data.size(), position);
  return;
}



/*----------------------------------------------------------------------***
 |  print this element (public)                                         |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::SoTet10::Print(std::ostream& os) const
{
  os << "So_tet10 ";
  Element::Print(os);
  std::cout << std::endl;
  return;
}

/*====================================================================*/
/* 10-node tetrahedra node topology*/
/*--------------------------------------------------------------------*/
/* parameter coordinates (ksi1, ksi2, ksi3) of nodes
 * of a common tetrahedron [0,1]x[0,1]x[0,1]
 *  10-node hexahedron: node 0,1,...,9
 *
 * -----------------------
 *- this is the numbering used in GiD & EXODUS!!
 *      3-
 *      |\ ---
 *      |  \    --9
 *      |    \      ---
 *      |      \        -2
 *      |        \       /\
 *      |          \   /   \
 *      7            8      \
 *      |          /   \     \
 *      |        6       \    5
 *      |      /           \   \
 *      |    /               \  \
 *      |  /                   \ \
 *      |/                       \\
 *      0------------4-------------1
 */
/*====================================================================*/

/*----------------------------------------------------------------------**#
|  get vector of surfaces (public)                                     |
|  surface normals always point outward                                |
*----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::SoTet10::Surfaces()
{
  return Core::Communication::ElementBoundaryFactory<StructuralSurface, Core::Elements::Element>(
      Core::Communication::buildSurfaces, *this);
}

/*----------------------------------------------------------------------***++
 |  get vector of lines (public)                                        |
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<Core::Elements::Element>> Discret::ELEMENTS::SoTet10::Lines()
{
  return Core::Communication::ElementBoundaryFactory<StructuralLine, Core::Elements::Element>(
      Core::Communication::buildLines, *this);
}
/*----------------------------------------------------------------------*
 |  get location of element center                              jb 08/11|
 *----------------------------------------------------------------------*/
std::vector<double> Discret::ELEMENTS::SoTet10::element_center_refe_coords()
{
  // update element geometry
  Core::Nodes::Node** nodes = Nodes();
  Core::LinAlg::Matrix<NUMNOD_SOTET10, NUMDIM_SOTET10> xrefe;  // material coord. of element
  for (int i = 0; i < NUMNOD_SOTET10; ++i)
  {
    const auto& x = nodes[i]->X();
    xrefe(i, 0) = x[0];
    xrefe(i, 1) = x[1];
    xrefe(i, 2) = x[2];
  }
  const Core::FE::CellType distype = Shape();
  Core::LinAlg::Matrix<NUMNOD_SOTET10, 1> funct;
  // Centroid of a tet with (0,1)(0,1)(0,1) is (0.25, 0.25, 0.25)
  Core::FE::shape_function_3D(funct, 0.25, 0.25, 0.25, distype);
  Core::LinAlg::Matrix<1, NUMDIM_SOTET10> midpoint;
  midpoint.MultiplyTN(funct, xrefe);
  std::vector<double> centercoords(3);
  centercoords[0] = midpoint(0, 0);
  centercoords[1] = midpoint(0, 1);
  centercoords[2] = midpoint(0, 2);
  return centercoords;
}

/*----------------------------------------------------------------------*
 |  Return names of visualization data (public)                 st 01/10|
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::SoTet10::VisNames(std::map<std::string, int>& names)
{
  SolidMaterial()->VisNames(names);
  return;
}

/*----------------------------------------------------------------------*
 |  Return visualization data (public)                          st 01/10|
 *----------------------------------------------------------------------*/
bool Discret::ELEMENTS::SoTet10::VisData(const std::string& name, std::vector<double>& data)
{
  // Put the owner of this element into the file (use base class method for this)
  if (Core::Elements::Element::VisData(name, data)) return true;

  return SolidMaterial()->VisData(name, data, NUMGPT_SOTET10, this->Id());
}

/*----------------------------------------------------------------------*
 |  Call post setup routine of the materials                            |
 *----------------------------------------------------------------------*/
void Discret::ELEMENTS::SoTet10::material_post_setup(Teuchos::ParameterList& params)
{
  if (Core::Nodes::HaveNodalFibers<Core::FE::CellType::tet10>(Nodes()))
  {
    // This element has fiber nodes.
    // Interpolate fibers to the Gauss points and pass them to the material

    // Get shape functions
    const static std::vector<Core::LinAlg::Matrix<NUMNOD_SOTET10, 1>> shapefcts_4gp =
        so_tet10_4gp_shapefcts();

    // add fibers to the ParameterList
    // ParameterList does not allow to store a std::vector, so we have to add every gp fiber
    // with a separate key. To keep it clean, It is added to a sublist.
    Core::Nodes::NodalFiberHolder fiberHolder;

    // Do the interpolation
    Core::Nodes::ProjectFibersToGaussPoints<Core::FE::CellType::tet10>(
        Nodes(), shapefcts_4gp, fiberHolder);

    params.set("fiberholder", fiberHolder);
  }

  // Call super post setup
  SoBase::material_post_setup(params);

  // Cleanup ParameterList to not carry all fibers the whole simulation
  // do not throw an error if key does not exist.
  params.remove("fiberholder", false);
  // params.remove("gpfiber2", false);
}

FOUR_C_NAMESPACE_CLOSE
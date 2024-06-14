/*-----------------------------------------------------------------------*/
/*! \file
\brief A class for a mortar coupling node

\level 2

*/
/*-----------------------------------------------------------------------*/

#include "4C_mortar_node.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_mortar_element.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN


Mortar::NodeType Mortar::NodeType::instance_;


Core::Communication::ParObject* Mortar::NodeType::Create(const std::vector<char>& data)
{
  std::vector<double> x(3, 0.0);
  std::vector<int> dofs(0);
  auto* node = new Mortar::Node(0, x, 0, dofs, false);
  node->Unpack(data);
  return node;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Mortar::NodeDataContainer::NodeDataContainer()
    : drows_(0), drows_nts_(0), drows_lts_(0), drows_ltl_(0)
{
  for (int i = 0; i < 3; ++i)
  {
    n()[i] = 0.0;
    EdgeTangent()[i] = 0.0;
    lm()[i] = 0.0;
    lmold()[i] = 0.0;
    lmuzawa()[i] = 0.0;
    GetDscale() = 0.0;
  }
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::NodeDataContainer::Pack(Core::Communication::PackBuffer& data) const
{
  // add n_
  Core::Communication::ParObject::add_to_pack(data, n_, 3 * sizeof(double));
  // add edgetangent_
  Core::Communication::ParObject::add_to_pack(data, edgeTangent_, 3 * sizeof(double));
  // add lm_
  Core::Communication::ParObject::add_to_pack(data, lm_, 3 * sizeof(double));
  // add lmold_
  Core::Communication::ParObject::add_to_pack(data, lmold_, 3 * sizeof(double));
  // add lmuzawa_
  Core::Communication::ParObject::add_to_pack(data, lmuzawa_, 3 * sizeof(double));

  // no need to pack drows_, mrows_ and mmodrows_
  // (these will evaluated anew anyway)
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::NodeDataContainer::Unpack(
    std::vector<char>::size_type& position, const std::vector<char>& data)
{
  // n_
  Core::Communication::ParObject::extract_from_pack(position, data, n_, 3 * sizeof(double));
  // edgetangent_
  Core::Communication::ParObject::extract_from_pack(
      position, data, edgeTangent_, 3 * sizeof(double));
  // lm_
  Core::Communication::ParObject::extract_from_pack(position, data, lm_, 3 * sizeof(double));
  // lmold_
  Core::Communication::ParObject::extract_from_pack(position, data, lmold_, 3 * sizeof(double));
  // lmuzawa_
  Core::Communication::ParObject::extract_from_pack(position, data, lmuzawa_, 3 * sizeof(double));
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Mortar::Node::Node(int id, const std::vector<double>& coords, const int owner,
    const std::vector<int>& dofs, const bool isslave)
    : Core::Nodes::Node(id, coords, owner),
      isslave_(isslave),
      istiedslave_(isslave),
      isonbound_(false),
      isonedge_(false),
      isoncorner_(false),
      isdbc_(false),
      dofs_(dofs),
      hasproj_(false),
      hassegment_(false),
      detected_(false),
      dentries_(0),
      modata_(Teuchos::null),
      nurbsw_(-1.0)
{
  for (std::size_t i = 0; i < coords.size(); ++i)
  {
    uold()[i] = 0.0;
    xspatial()[i] = X()[i];
    dbcdofs_[i] = false;
  }
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Mortar::Node::Node(const Mortar::Node& old)
    : Core::Nodes::Node(old),
      isslave_(old.isslave_),
      istiedslave_(old.istiedslave_),
      isonbound_(old.isonbound_),
      isdbc_(old.isdbc_),
      dofs_(old.dofs_),
      hasproj_(old.hasproj_),
      hassegment_(old.hassegment_),
      detected_(false)
{
  for (int i = 0; i < 3; ++i)
  {
    uold()[i] = old.uold_[i];
    xspatial()[i] = old.xspatial_[i];
    dbcdofs_[i] = false;
  }
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Mortar::Node* Mortar::Node::Clone() const
{
  auto* newnode = new Mortar::Node(*this);
  return newnode;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
std::ostream& operator<<(std::ostream& os, const Mortar::Node& mrtrnode)
{
  mrtrnode.Print(os);
  return os;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::Print(std::ostream& os) const
{
  // Print id and coordinates
  os << "Mortar ";
  Core::Nodes::Node::Print(os);

  if (IsSlave())
    os << " Slave  ";
  else
    os << " Master ";

  if (IsOnBound())
    os << " Boundary ";
  else
    os << " Interior ";
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::Pack(Core::Communication::PackBuffer& data) const
{
  Core::Communication::PackBuffer::SizeMarker sm(data);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  add_to_pack(data, type);
  // add base class Core::Nodes::Node
  Core::Nodes::Node::Pack(data);
  // add isslave_
  add_to_pack(data, isslave_);
  // add istiedslave_
  add_to_pack(data, istiedslave_);
  // add isonbound_
  add_to_pack(data, isonbound_);
  // add isonbound_
  add_to_pack(data, isonedge_);
  // add isonbound_
  add_to_pack(data, isoncorner_);
  // add isdbc_
  add_to_pack(data, isdbc_);
  // add dbcdofs_
  add_to_pack(data, dbcdofs_[0]);
  add_to_pack(data, dbcdofs_[1]);
  add_to_pack(data, dbcdofs_[2]);
  // dentries_
  add_to_pack(data, dentries_);
  // add dofs_
  add_to_pack(data, dofs_);
  // add xspatial_
  add_to_pack(data, xspatial_, 3 * sizeof(double));
  // add uold_
  add_to_pack(data, uold_, 3 * sizeof(double));
  // add hasproj_
  add_to_pack(data, hasproj_);
  // add hassegment_
  add_to_pack(data, hassegment_);
  // add nurbsw_
  add_to_pack(data, nurbsw_);

  // add data_
  bool hasdata = (modata_ != Teuchos::null);
  add_to_pack(data, hasdata);
  if (hasdata) modata_->Pack(data);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  Core::Communication::ExtractAndAssertId(position, data, UniqueParObjectId());

  // extract base class Core::Nodes::Node
  std::vector<char> basedata(0);
  extract_from_pack(position, data, basedata);
  Core::Nodes::Node::Unpack(basedata);
  // isslave_
  isslave_ = extract_int(position, data);
  // istiedslave_
  istiedslave_ = extract_int(position, data);
  // isonbound_
  isonbound_ = extract_int(position, data);
  // isonedge_
  isonedge_ = extract_int(position, data);
  // isoncorner_
  isoncorner_ = extract_int(position, data);
  // isdbc_
  isdbc_ = extract_int(position, data);
  // dbcdofs_
  dbcdofs_[0] = extract_int(position, data);
  dbcdofs_[1] = extract_int(position, data);
  dbcdofs_[2] = extract_int(position, data);
  // dentries_
  extract_from_pack(position, data, dentries_);
  // dofs_
  extract_from_pack(position, data, dofs_);
  // xspatial_
  extract_from_pack(position, data, xspatial_, 3 * sizeof(double));
  // uold_
  extract_from_pack(position, data, uold_, 3 * sizeof(double));
  // hasproj_
  hasproj_ = extract_int(position, data);
  // hassegment_
  hassegment_ = extract_int(position, data);
  // nurbsw_
  nurbsw_ = extract_double(position, data);

  // data_
  bool hasdata = extract_int(position, data);
  if (hasdata)
  {
    modata_ = Teuchos::rcp(new Mortar::NodeDataContainer());
    modata_->Unpack(position, data);
  }
  else
  {
    modata_ = Teuchos::null;
  }

  if (position != data.size())
    FOUR_C_THROW("Mismatch in size of data %d <-> %d", (int)data.size(), position);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::AddDValue(const int& colnode, const double& val)
{
  // check if this is a master node or slave boundary node
  if (not IsSlave()) FOUR_C_THROW("AddDValue: function called for master node %i", Id());
  if (IsOnBound()) FOUR_C_THROW("AddDValue: function called for boundary node %i", Id());

  // check if this has been called before
  if ((int)MoData().GetD().size() == 0) MoData().GetD().resize(dentries_);

  // add the pair (col,val) to the given row
  MoData().GetD()[colnode] += val;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::AddDntsValue(const int& colnode, const double& val)
{
  // check if this has been called before
  if ((int)MoData().GetDnts().size() == 0) MoData().GetDnts().resize(dentries_);

  // add the pair (col,val) to the given row
  MoData().GetDnts()[colnode] += val;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::AddDltsValue(const int& colnode, const double& val)
{
  // check if this has been called before
  if ((int)MoData().GetDlts().size() == 0) MoData().GetDlts().resize(dentries_);

  // add the pair (col,val) to the given row
  MoData().GetDlts()[colnode] += val;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::AddDltlValue(const int& colnode, const double& val)
{
  // check if this is a master node or slave boundary node
  if (not IsSlave()) FOUR_C_THROW("AddDValue: function called for master node %i", Id());
  if (not IsOnEdge()) FOUR_C_THROW("function called for non-edge node %i", Id());

  // check if this has been called before
  if (static_cast<int>(MoData().GetDltl().size()) == 0) MoData().GetDltl().resize(dentries_);

  // add the pair (col,val) to the given row
  MoData().GetDltl()[colnode] += val;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::AddMValue(const int& colnode, const double& val)
{
  // check if this is a master node or slave boundary node
  if (not IsSlave()) FOUR_C_THROW("AddMValue: function called for master node %i", Id());
  if (IsOnBoundorCE()) FOUR_C_THROW("AddMValue: function called for boundary node %i", Id());

  // add the pair (col,val) to the given row
  MoData().GetM()[colnode] += val;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::AddMntsValue(const int& colnode, const double& val)
{
  // add the pair (col,val) to the given row
  MoData().GetMnts()[colnode] += val;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::AddMltsValue(const int& colnode, const double& val)
{
  // add the pair (col,val) to the given row
  MoData().GetMlts()[colnode] += val;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::AddMltlValue(const int& colnode, const double& val)
{
  // check if this is a master node or slave boundary node
  if (not IsSlave()) FOUR_C_THROW("AddMValue: function called for master node %i", Id());
  if (not IsOnEdge()) FOUR_C_THROW("function called for non-edge node %i", Id());

  // add the pair (col,val) to the given row
  MoData().GetMltl()[colnode] += val;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::AddMmodValue(const int& colnode, const double& val)
{
  // check if this is a master node or slave boundary node
  if (not IsSlave()) FOUR_C_THROW("AddMmodValue: function called for master node %i", Id());
  if (IsOnBound()) FOUR_C_THROW("AddMmodValue: function called for boundary node %i", Id());

  // add the pair (col,val) to the given row
  MoData().GetMmod()[colnode] += val;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::initialize_data_container()
{
  // get maximum size of nodal D-entries
  dentries_ = 0;
  std::set<int> sIdCheck;
  std::pair<std::set<int>::iterator, bool> check;
  for (int i = 0; i < NumElement(); ++i)
  {
    const int* snodeIds = Elements()[i]->NodeIds();
    for (int j = 0; j < Elements()[i]->num_node(); ++j)
    {
      check = sIdCheck.insert(snodeIds[j]);
      if (check.second) dentries_ += Elements()[i]->NumDofPerNode(*(Elements()[i]->Nodes()[j]));
    }
  }

  // only initialize if not yet done
  if (modata_ == Teuchos::null) modata_ = Teuchos::rcp(new Mortar::NodeDataContainer());
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::ResetDataContainer()
{
  // reset to Teuchos::null
  modata_ = Teuchos::null;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Mortar::Node::BuildAveragedNormal()
{
  // reset normal and tangents when this method is called
  for (int j = 0; j < 3; ++j) MoData().n()[j] = 0.0;

  int nseg = NumElement();
  Core::Elements::Element** adjeles = Elements();

  // we need to store some stuff here
  //**********************************************************************
  // elens(0,i): x-coord of element normal
  // elens(1,i): y-coord of element normal
  // elens(2,i): z-coord of element normal
  // elens(3,i): id of adjacent element i
  // elens(4,i): length of element normal
  // elens(5,i): length/area of element itself
  //**********************************************************************
  Core::LinAlg::SerialDenseMatrix elens(6, nseg);

  // loop over all adjacent elements
  for (int i = 0; i < nseg; ++i)
  {
    auto* adjmrtrele = dynamic_cast<Mortar::Element*>(adjeles[i]);

    // build element normal at current node
    // (we have to pass in the index i to be able to store the
    // normal and other information at the right place in elens)
    adjmrtrele->BuildNormalAtNode(Id(), i, elens);

    // add (weighted) element normal to nodal normal n
    for (int j = 0; j < 3; ++j) MoData().n()[j] += elens(j, i) / elens(4, i);
  }

  // create unit normal vector
  double length = sqrt(MoData().n()[0] * MoData().n()[0] + MoData().n()[1] * MoData().n()[1] +
                       MoData().n()[2] * MoData().n()[2]);
  if (length == 0.0)
    FOUR_C_THROW("Nodal normal length 0, node ID %i", Id());
  else
    for (int j = 0; j < 3; ++j) MoData().n()[j] /= length;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Mortar::Node* Mortar::Node::FindClosestNode(const Teuchos::RCP<Core::FE::Discretization> intdis,
    const Teuchos::RCP<Epetra_Map> nodesearchmap, double& mindist)
{
  Node* closestnode = nullptr;

  // loop over all nodes of the Core::FE::Discretization that are
  // included in the given Epetra_Map ("brute force" search)
  for (int i = 0; i < nodesearchmap->NumMyElements(); ++i)
  {
    int gid = nodesearchmap->GID(i);
    Core::Nodes::Node* node = intdis->gNode(gid);
    if (!node) FOUR_C_THROW("FindClosestNode: Cannot find node with gid %", gid);
    auto* mrtrnode = dynamic_cast<Node*>(node);

    // build distance between the two nodes
    double dist = 0.0;
    const double* p1 = xspatial();
    const double* p2 = mrtrnode->xspatial();

    for (int j = 0; j < 3; ++j) dist += (p1[j] - p2[j]) * (p1[j] - p2[j]);
    dist = sqrt(dist);

    // new closest node found, update
    if (dist <= mindist)
    {
      mindist = dist;
      closestnode = mrtrnode;
    }
  }

  if (!closestnode) FOUR_C_THROW("FindClosestNode: No closest node found at all!");

  return closestnode;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool Mortar::Node::CheckMeshDistortion(double& relocation, const double& limit)
{
  // initialize return parameter
  bool ok = true;

  // loop over all adjacent elements of this node
  for (int i = 0; i < NumElement(); ++i)
  {
    // get the current element
    Core::Elements::Element* ele = Elements()[i];
    if (!ele) FOUR_C_THROW("Cannot find element with lid %", i);
    auto* mrtrele = dynamic_cast<Mortar::Element*>(ele);

    // minimal edge size of the current element
    const double minedgesize = mrtrele->MinEdgeSize();

    // check whether relocation is not too large
    if (relocation > limit * minedgesize)
    {
      // print information to screen
      std::cout << "\n*****************WARNING***********************" << '\n';
      std::cout << "Checking distortion for CNode:     " << Id() << '\n';
      std::cout << "Relocation distance:               " << relocation << '\n';
      std::cout << "AdjEle: " << mrtrele->Id() << "\tLimit*MinEdgeSize: " << limit * minedgesize
                << '\n';
      std::cout << "*****************WARNING***********************" << '\n';

      // set return parameter and stop
      ok = false;
      break;
    }
  }

  return ok;
}

FOUR_C_NAMESPACE_CLOSE
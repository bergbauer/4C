/*----------------------------------------------------------------------*/
/*! \file

\brief   This is basically a (3d-) node with an additional weight.
   The weight is required for the evaluation of the nurbs
   basis functions.

   note that X() is not the coordinate of some grid point
   anymore, it's just the control point position


\level 2

*----------------------------------------------------------------------*/

#include "baci_nurbs_discret_control_point.H"

DRT::NURBS::ControlPointType DRT::NURBS::ControlPointType::instance_;


DRT::ParObject* DRT::NURBS::ControlPointType::Create(const std::vector<char>& data)
{
  double dummycoord[3] = {999., 999., 999.};
  double dummyweight = 999.;
  DRT::NURBS::ControlPoint* object = new DRT::NURBS::ControlPoint(-1, dummycoord, dummyweight, -1);
  object->Unpack(data);
  return object;
}

/*
  Standard ctor
 */
DRT::NURBS::ControlPoint::ControlPoint(
    int id, const double* coords, const double weight, const int owner)
    : DRT::Node(id, coords, owner), w_(weight)
{
  return;
}

/*
  Copy Constructor

  Makes a deep copy of a control point

*/
DRT::NURBS::ControlPoint::ControlPoint(const DRT::NURBS::ControlPoint& old)
    : DRT::Node(old), w_(old.W())
{
  return;
}

/*
  Deep copy the derived class and return pointer to it

*/
DRT::NURBS::ControlPoint* DRT::NURBS::ControlPoint::Clone() const
{
  DRT::NURBS::ControlPoint* newcp = new DRT::NURBS::ControlPoint(*this);

  return newcp;
}


/*
  Pack this class so it can be communicated

  Pack and Unpack are used to communicate this control point

*/
void DRT::NURBS::ControlPoint::Pack(DRT::PackBuffer& data) const
{
  DRT::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  DRT::Node::AddtoPack(data, type);
  // add base class of control point
  DRT::Node::Pack(data);
  // add weight
  DRT::Node::AddtoPack(data, &w_, sizeof(double));

  return;
}

/*
  Unpack data from a char vector into this class

  Pack and Unpack are used to communicate this control point
*/
void DRT::NURBS::ControlPoint::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  CORE::COMM::ExtractAndAssertId(position, data, UniqueParObjectId());

  // extract base class Node
  std::vector<char> basedata(0);
  ExtractfromPack(position, data, basedata);
  DRT::Node::Unpack(basedata);
  // extract weight
  DRT::Node::ExtractfromPack(position, data, w_);

  return;
}

/*
  Print this control point
*/
void DRT::NURBS::ControlPoint::Print(std::ostream& os) const
{
  os << "Control Point :";
  DRT::Node::Print(os);
  os << "\n+ additional weight ";
  os << w_ << "\n";
  return;
}
/*!----------------------------------------------------------------------
\file drt_control_point.cpp

   This is basically a (3d-) node with an additional weight.
   The weight is required for the evaluation of the nurbs
   basis functions.

   note that X() is not the coordinate of some grid point
   anymore, it's just the control point position

<pre>
Maintainer: Peter Gamnitzer
            gamnitzer@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15235
</pre>

*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include "drt_control_point.H"

DRT::NURBS::ControlPointType DRT::NURBS::ControlPointType::instance_;


DRT::ParObject* DRT::NURBS::ControlPointType::Create( const std::vector<char> & data )
{
  double dummycoord[3] = {999.,999.,999.};
  double dummyweight   =  999.;
  DRT::NURBS::ControlPoint* object = new DRT::NURBS::ControlPoint(-1,dummycoord,dummyweight,-1);
  object->Unpack(data);
  return object;
}

/*
  Standard ctor
 */
DRT::NURBS::ControlPoint::ControlPoint(int           id    ,
				       const double* coords,
				       const double  weight,
				       const int     owner)
:
  DRT::Node(id,coords,owner),
  w_(weight)
{
  return;
}

/*
  Copy Constructor

  Makes a deep copy of a control point

*/
DRT::NURBS::ControlPoint::ControlPoint(const DRT::NURBS::ControlPoint& old)
  :
  DRT::Node(old),
  w_(old.W())
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
  Destructor
*/
DRT::NURBS::ControlPoint::~ControlPoint()
{
  return;
}

/*
  Pack this class so it can be communicated

  Pack and Unpack are used to communicate this control point

*/
void DRT::NURBS::ControlPoint::Pack(vector<char>& data) const
{
  data.resize(0);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  DRT::Node::AddtoPack(data,type);
  // add base class of control point
  vector<char> basedata(0);
  DRT::Node::Pack(basedata);
  DRT::Node::AddtoPack(data,basedata);
  // add weight
  DRT::Node::AddtoPack(data,&w_,  sizeof(double));

  return;
}

/*
  Unpack data from a char vector into this class

  Pack and Unpack are used to communicate this control point
*/
void DRT::NURBS::ControlPoint::Unpack(const vector<char>& data)
{
  vector<char>::size_type position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position,data,type);
  dsassert(type == UniqueParObjectId(), "wrong instance type data");
  // extract base class Node
  vector<char> basedata(0);
  ExtractfromPack(position,data,basedata);
  DRT::Node::Unpack(basedata);
  // extract weight
  DRT::Node::ExtractfromPack(position,data,w_);

  return;
}

/*
  Print this control point
*/
void DRT::NURBS::ControlPoint::Print(ostream& os) const
{
  os << "Control Point :";
  DRT::Node::Print(os);
  os << "\n+ additional weight " ;
  os << w_ <<"\n";
  return;

}

#endif // CCADISCRET

/*!----------------------------------------------------------------------
\file spring3.cpp
\brief three dimensional spring element

<pre>
Maintainer: Dhrubajyoti Mukherjee
            mukherjee@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15270
</pre>

*----------------------------------------------------------------------*/

#include "spring3.H"
#include "../drt_beam3eb/beam3eb.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/drt_utils_nullspace.H"
#include "../drt_lib/drt_linedefinition.H"

DRT::ELEMENTS::Spring3Type DRT::ELEMENTS::Spring3Type::instance_;


DRT::ParObject* DRT::ELEMENTS::Spring3Type::Create( const std::vector<char> & data )
{
  DRT::ELEMENTS::Spring3* object = new DRT::ELEMENTS::Spring3(-1,-1);
  object->Unpack(data);
  return object;
}


Teuchos::RCP<DRT::Element> DRT::ELEMENTS::Spring3Type::Create( const std::string eletype,
                                                              const std::string eledistype,
                                                              const int         id,
                                                              const int         owner )
  {
  if ( eletype=="SPRING3" )
  {
    Teuchos::RCP<DRT::Element> ele = Teuchos::rcp(new DRT::ELEMENTS::Spring3(id,owner));
    return ele;
  }
  return Teuchos::null;
}


Teuchos::RCP<DRT::Element> DRT::ELEMENTS::Spring3Type::Create( const int id, const int owner )
{
  Teuchos::RCP<DRT::Element> ele = Teuchos::rcp(new DRT::ELEMENTS::Spring3(id,owner));
  return ele;
}


void DRT::ELEMENTS::Spring3Type::NodalBlockInformation( DRT::Element * dwele, int & numdf, int & dimns, int & nv, int & np )
{
  numdf = 3;
  dimns = 6;
  nv = 3;
}

void DRT::ELEMENTS::Spring3Type::ComputeNullSpace( DRT::Discretization & dis, std::vector<double> & ns, const double * x0, int numdf, int dimns )
{
  DRT::UTILS::ComputeStructure3DNullSpace( dis, ns, x0, numdf, dimns );
}

void DRT::ELEMENTS::Spring3Type::SetupElementDefinition( std::map<std::string,std::map<std::string,DRT::INPUT::LineDefinition> > & definitions )
{
  std::map<std::string,DRT::INPUT::LineDefinition>& defs = definitions["SPRING3"];

  defs["LINE2"]
    .AddIntVector("LINE2",2)
    .AddNamedInt("MAT")
    ;

  defs["LIN2"]
    .AddIntVector("LIN2",2)
    .AddNamedInt("MAT")
    ;
}


/*----------------------------------------------------------------------*
 |  ctor (public)                                            cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Spring3::Spring3(int id, int owner) :
DRT::Element(id,owner),
data_(),
isinit_(false),
diff_disp_ref_(LINALG::Matrix<1,3>(true)),
deltatheta_(LINALG::Matrix<1,3>(true)),
material_(0),
lrefe_(0),
lcurr_(0),
crosssec_(0),
NormMoment(0),
NormForce(0),
RatioNormForceMoment(0),
Theta0_(LINALG::Matrix<3,1>(true)),
Theta_(LINALG::Matrix<3,1>(true))
{
  return;
}
/*----------------------------------------------------------------------*
 |  copy-ctor (public)                                       cyron 08/08|
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Spring3::Spring3(const DRT::ELEMENTS::Spring3& old) :
 DRT::Element(old),
 data_(old.data_),
 isinit_(old.isinit_),
 X_(old.X_),
 trefNode_(old.trefNode_),
 ThetaRef_(old.ThetaRef_),
 diff_disp_ref_(old.diff_disp_ref_),
 deltatheta_(old.deltatheta_),
 material_(old.material_),
 lrefe_(old.lrefe_),
 lcurr_(old.lcurr_),
 jacobimass_(old.jacobimass_),
 jacobinode_(old.jacobinode_),
 crosssec_(old.crosssec_),
 NormMoment(old.NormMoment),
 NormForce(old.NormForce),
 RatioNormForceMoment(old.RatioNormForceMoment),
 Theta0_(old.Theta0_),
 Theta_(old.Theta_)
{
  return;
}
/*----------------------------------------------------------------------*
 |  Deep copy this instance of Spring3 and return pointer to it (public)|
 |                                                       mukherjee 04/15|
 *----------------------------------------------------------------------*/
DRT::Element* DRT::ELEMENTS::Spring3::Clone() const
{
  DRT::ELEMENTS::Spring3* newelement = new DRT::ELEMENTS::Spring3(*this);
  return newelement;
}

/*----------------------------------------------------------------------*
 |  dtor (public)                                       mukherjee 04/15 |
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::Spring3::~Spring3()
{
  return;
}


/*----------------------------------------------------------------------*
 |  print this element (public)                         mukherjee 04/15 |
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Spring3::Print(std::ostream& os) const
{
  os << "Spring3 ";
  Element::Print(os);
  return;
}



/*----------------------------------------------------------------------*
 | Print the change in angle of this element            mukherjee 04/15 |
 *----------------------------------------------------------------------*/
  LINALG::Matrix<1,3> DRT::ELEMENTS::Spring3::DeltaTheta() const
 {
   return deltatheta_;
 }

/*----------------------------------------------------------------------*
 |(public)                                               mukherjee 04/15|
 *----------------------------------------------------------------------*/
DRT::Element::DiscretizationType DRT::ELEMENTS::Spring3::Shape() const
{
  return line2;
}


/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                       mukherjee 04/15|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Spring3::Pack(DRT::PackBuffer& data) const
{
  DRT::PackBuffer::SizeMarker sm( data );
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // add base class Element
  Element::Pack(data);
  AddtoPack(data,isinit_);
  AddtoPack<6,1>(data,X_);
  AddtoPack(data,trefNode_);
  AddtoPack(data,ThetaRef_);
  AddtoPack<1,3>(data,diff_disp_ref_);
  AddtoPack<1,3>(data,deltatheta_);
  AddtoPack(data,material_);
  AddtoPack(data,lrefe_);
  AddtoPack(data,lcurr_);
  AddtoPack(data,jacobimass_);
  AddtoPack(data,jacobinode_);
  AddtoPack(data,crosssec_);
  AddtoPack(data,NormMoment);
  AddtoPack(data,NormForce);
  AddtoPack(data,RatioNormForceMoment);
  AddtoPack<3,1>(data,Theta0_);
  AddtoPack<3,1>(data,Theta_);
  AddtoPack(data,data_);

  return;
}


/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 |                                                       mukherjee 04/15|
 *----------------------------------------------------------------------*/
void DRT::ELEMENTS::Spring3::Unpack(const std::vector<char>& data)
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
  isinit_ = ExtractInt(position,data);
  ExtractfromPack<6,1>(position,data,X_);
  ExtractfromPack(position,data,trefNode_);
  ExtractfromPack(position,data,ThetaRef_);
  ExtractfromPack<1,3>(position,data,diff_disp_ref_);
  ExtractfromPack<1,3>(position,data,deltatheta_);
  ExtractfromPack(position,data,material_);
  ExtractfromPack(position,data,lrefe_);
  ExtractfromPack(position,data,lcurr_);
  ExtractfromPack(position,data,jacobimass_);
  ExtractfromPack(position,data,jacobinode_);
  ExtractfromPack(position,data,crosssec_);
  ExtractfromPack(position,data,NormMoment);
  ExtractfromPack(position,data,NormForce);
  ExtractfromPack(position,data,RatioNormForceMoment);
  ExtractfromPack<3,1>(position,data,Theta0_);
  ExtractfromPack<3,1>(position,data,Theta_);
  // gaussrule_
  int gausrule_integer;
  ExtractfromPack(position,data,gausrule_integer);
  std::vector<char> tmp(0);
  ExtractfromPack(position,data,tmp);
  data_.Unpack(tmp);

  if (position != data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
  return;
}

/*----------------------------------------------------------------------*
 |  get vector of lines (public)                         mukherjee 04/15|
 *----------------------------------------------------------------------*/
std::vector<Teuchos::RCP<DRT::Element> > DRT::ELEMENTS::Spring3::Lines()
{
  std::vector<Teuchos::RCP<Element> > lines(1);
  lines[0]= Teuchos::rcp(this, false);
  return lines;
}


void DRT::ELEMENTS::Spring3::SetUpReferenceGeometry(const std::vector<double>& xrefe, const std::vector<double>& rotrefe, const bool secondinit)
{
  /*this method initializes geometric variables of the element; the initilization can usually be applied to elements only once;
   *therefore after the first initilization the flag isinit is set to true and from then on this method does not take any action
   *when called again unless it is called on purpose with the additional parameter secondinit. If this parameter is passed into
   *the method and is true the element is initialized another time with respective xrefe;
   *note: the isinit_ flag is important for avoiding reinitialization upon restart. However, it should be possible to conduct a
   *second initilization in principle (e.g. for periodic boundary conditions*/
  if(!isinit_ || secondinit)
  {
    isinit_ = true;

    //setting reference coordinates
    for(int i=0;i<6;i++)
      X_(i) = xrefe[i];

    //length in reference configuration
    lrefe_ = std::pow(pow(X_(3)-X_(0),2)+pow(X_(4)-X_(1),2)+pow(X_(5)-X_(2),2),0.5);

    //set jacobi determinants for integration of mass matrix and at nodes
    jacobimass_.resize(2);
    jacobimass_[0] = lrefe_ / 2.0;
    jacobimass_[1] = lrefe_ / 2.0;
    jacobinode_.resize(2);
    jacobinode_[0] = lrefe_ / 2.0;
    jacobinode_[1] = lrefe_ / 2.0;

    double abs_rotrefe=0;
    for (int i=0; i<6; i++)
      abs_rotrefe+= std::pow(rotrefe[i],2);

    if (abs_rotrefe!=0)
    {
      //assign size to the vector
      ThetaRef_.resize(3);
      trefNode_.resize(3);

      for (int node=0; node<2; node++)
      {
        trefNode_[node].Clear();
        for(int dof=0; dof<3; dof++)
          trefNode_[node](dof)=rotrefe[3*node+dof];
      }
      diff_disp_ref_.Clear();

      //Calculate reference directional vector of the truss element
      for (int j=0; j<3; ++j)
      {
        diff_disp_ref_(j) = Nodes()[1]->X()[j]  - Nodes()[0]->X()[j];
      }

      for (int location=0; location<3; location++) // Location of torsional spring. There are three locations
      {
        double dotprod=0.0;
        LINALG::Matrix  <1,3> crossprod(true);
        double CosTheta=0.0;
        double SinTheta=0.0;

        if (location==0)
        {
          double norm_v_ref = diff_disp_ref_.Norm2();
          double norm_t1_ref=trefNode_[location].Norm2();
          for (int j=0; j<3; ++j)
            dotprod +=  trefNode_[location](j) * diff_disp_ref_(j);

          CosTheta = dotprod/(norm_v_ref*norm_t1_ref);

          //Cross Product
          crossprod(0) = trefNode_[location](1)*diff_disp_ref_(2) - trefNode_[location](2)*diff_disp_ref_(1);
          crossprod(1) = trefNode_[location](2)*diff_disp_ref_(0) - trefNode_[location](0)*diff_disp_ref_(2);
          crossprod(2) = trefNode_[location](0)*diff_disp_ref_(1) - trefNode_[location](1)*diff_disp_ref_(0);

          double norm= crossprod.Norm2();
          SinTheta= norm/(norm_v_ref*norm_t1_ref);
        }

        else if (location==1)
        {
          double norm_v_ref = diff_disp_ref_.Norm2();
          double norm_t2_ref= trefNode_[location].Norm2();
          for (int j=0; j<3; ++j)
            dotprod +=  trefNode_[location](j) * diff_disp_ref_(j); // From the opposite direction v_2 =-v_1

          CosTheta = dotprod/(norm_v_ref*norm_t2_ref);

          // cross product
          crossprod(0) = trefNode_[location](1)*diff_disp_ref_(2) - trefNode_[location](2)*diff_disp_ref_(1);
          crossprod(1) = trefNode_[location](2)*diff_disp_ref_(0) - trefNode_[location](0)*diff_disp_ref_(2);
          crossprod(2) = trefNode_[location](0)*diff_disp_ref_(1) - trefNode_[location](1)*diff_disp_ref_(0);
          double norm= crossprod.Norm2();
          SinTheta= norm/(norm_v_ref*norm_t2_ref);
        }

        else // i.e. for calculation of reference angle between t1 & t2
        {
          double norm_t1_ref = trefNode_[location-2].Norm2();
          double norm_t2_ref=trefNode_[location-1].Norm2();
          for (int j=0; j<3; ++j)
            dotprod +=  trefNode_[location-1](j) * trefNode_[location-2](j);

          CosTheta = dotprod/(norm_t1_ref*norm_t2_ref);

          // cross product
          crossprod(0) = trefNode_[location-2](1)*trefNode_[location-1](2) - trefNode_[location-2](2)*trefNode_[location-1](1);
          crossprod(1) = trefNode_[location-2](2)*trefNode_[location-1](0) - trefNode_[location-2](0)*trefNode_[location-1](2);
          crossprod(2) = trefNode_[location-2](0)*trefNode_[location-1](1) - trefNode_[location-2](1)*trefNode_[location-1](0);

          double norm= crossprod.Norm2();
          SinTheta= norm/(norm_t1_ref*norm_t2_ref);
        }

        double ThetaBoundary1=M_PI/4;
        double ThetaBoundary2=3*M_PI/4;

        ThetaRef_[location]=0;
        if (SinTheta>=0)
        {
          if (CosTheta >= cos(ThetaBoundary1))
            ThetaRef_[location]=asin(SinTheta);
          else if (CosTheta <= cos(ThetaBoundary2))
            ThetaRef_[location]=M_PI-asin(SinTheta);
          else
            ThetaRef_[location]=acos(CosTheta);
        }
        else
          dserror("Angle more than 180 degrees!");

        Theta0_(location)=ThetaRef_[location];
      }
    }
  }

  return;
}


int DRT::ELEMENTS::Spring3Type::Initialize(DRT::Discretization& dis)
{
  //reference node positions
  std::vector<double> xrefe;

  //reference nodal tangent positions
  std::vector<double> rotrefe;
  LINALG::Matrix<3,1> trefNodeAux(true);
  //resize vectors for the number of coordinates we need to store
  xrefe.resize(3*2);
  rotrefe.resize(3*2);
  for(int i=0; i<6; i++)
    rotrefe[i]=0;

  //setting beam reference director correctly
  for (int i=0; i<  dis.NumMyColElements(); ++i)
  {
    //in case that current element is not a truss3 element there is nothing to do and we go back
    //to the head of the loop
    if (dis.lColElement(i)->ElementType() != *this) continue;

    //if we get so far current element is a truss3 element and  we get a pointer at it
    DRT::ELEMENTS::Spring3* currele = dynamic_cast<DRT::ELEMENTS::Spring3*>(dis.lColElement(i));
    if (!currele) dserror("cast to Spring3* failed");

    //getting element's nodal coordinates and treating them as reference configuration
    if (currele->Nodes()[0] == NULL || currele->Nodes()[1] == NULL)
      dserror("Cannot get nodes in order to compute reference configuration'");
    else
    {
      for (int k=0; k<2; k++) //element has two nodes
        for(int l= 0; l < 3; l++)
          xrefe[k*3 + l] = currele->Nodes()[k]->X()[l];
    }

    //ask the spring element about the first element the first node is connected to
    DRT::Element* Element = currele->Nodes()[0]->Elements()[0];
    //Check via dynamic cast, if it's a beam3eb element
    DRT::ELEMENTS::Beam3eb* BeamElement = dynamic_cast<DRT::ELEMENTS::Beam3eb*>(Element);
    if (BeamElement!=NULL)
    {
      for (int k=0; k<2; k++) //element has two nodes
        for(int l= 0; l < 3; l++)
        {
          trefNodeAux=BeamElement->Tref()[k];
          rotrefe[k*3 + l]=trefNodeAux(l);
        }
    }

    currele->SetUpReferenceGeometry(xrefe,rotrefe);


  } //for (int i=0; i<dis_.NumMyColElements(); ++i)


  return 0;
}



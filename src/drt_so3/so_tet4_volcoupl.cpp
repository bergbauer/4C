/*
 * so_tet4_volcoupl.cpp
 *
 *  Created on: Jun 22, 2012
 *      Author: bertoglio
 */

#include "so_tet4_volcoupl.H"

#include "../drt_lib/drt_linedefinition.H"

//template<class coupltype>
template<class coupltype>
DRT::ELEMENTS::So_tet4_volcoupl< coupltype>::So_tet4_volcoupl(int id, int owner) :
DRT::Element(id,owner),
DRT::ELEMENTS::So_tet4(id,owner),
coupltype (id,owner)
//DRT::ELEMENTS::So3_Poro<DRT::Element::tet4>(id,owner)
{
  return;
}

/*!
\brief Copy Constructor

Makes a deep copy of a Element

*/

//template<class coupltype>
template< class coupltype>
DRT::ELEMENTS::So_tet4_volcoupl<coupltype>::So_tet4_volcoupl(const DRT::ELEMENTS::So_tet4_volcoupl<coupltype>& old) :
    DRT::Element(old),
    DRT::ELEMENTS::So_tet4(old),
    coupltype (old)
{
  return;
}

/*----------------------------------------------------------------------*
 |  Deep copy this instance of Solid3 and return pointer to it (public) |
 |                                                            popp 07/10|
 *----------------------------------------------------------------------*/
//template<class coupltype>
template<class coupltype>
DRT::Element* DRT::ELEMENTS::So_tet4_volcoupl<coupltype>::Clone() const
{
  DRT::ELEMENTS::So_tet4_volcoupl<coupltype >* newelement =
      new DRT::ELEMENTS::So_tet4_volcoupl<coupltype >(*this);
  return newelement;
}

/*----------------------------------------------------------------------*
 |  Pack data                                                  (public) |
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
//template<class coupltype>
template< class coupltype>
void DRT::ELEMENTS::So_tet4_volcoupl<coupltype>::Pack(DRT::PackBuffer& data) const
{
  DRT::PackBuffer::SizeMarker sm( data );
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data,type);
  // add base class So_tet4 Element
  DRT::ELEMENTS::So_tet4::Pack(data);
  // add base class So3_poro Element
  coupltype::Pack(data);

  return;
}

/*----------------------------------------------------------------------*
 |  Unpack data                                                (public) |
 |                                                           vuong 03/12|
 *----------------------------------------------------------------------*/
//template<class coupltype>
template< class coupltype>
void DRT::ELEMENTS::So_tet4_volcoupl<coupltype>::Unpack(const vector<char>& data)
{
  vector<char>::size_type position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position,data,type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");
  // extract base class So_tet4 Element
  vector<char> basedata(0);
  ExtractfromPack(position,data,basedata);
  DRT::ELEMENTS::So_tet4::Unpack(basedata);
  // extract base class So3_poro Element
  basedata.clear();
  ExtractfromPack(position,data,basedata);
  coupltype::Unpack(basedata);

  if (position != data.size())
    dserror("Mismatch in size of data %d <-> %d",(int)data.size(),position);
  return;
}

/*----------------------------------------------------------------------*
 |  dtor (public)                                            vuong 03/12|
 *----------------------------------------------------------------------*/
//template<class coupltype>
template<class coupltype>
DRT::ELEMENTS::So_tet4_volcoupl<coupltype>::~So_tet4_volcoupl()
{
  return;
}

/*----------------------------------------------------------------------*
 |  print this element (public)                              vuong 03/12|
 *----------------------------------------------------------------------*/
//template<class coupltype>
template<class coupltype>
void DRT::ELEMENTS::So_tet4_volcoupl<coupltype>::Print(ostream& os) const
{
  os << "So_tet4_volcoupl ";
  coupltype::Print(os);
  Element::Print(os);
  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
//template<class coupltype>
template<class coupltype>
bool DRT::ELEMENTS::So_tet4_volcoupl<coupltype>::ReadElement(const std::string& eletype,
                                             const std::string& eledistype,
                                             DRT::INPUT::LineDefinition* linedef)
{
  return So_tet4::ReadElement(eletype, eledistype, linedef);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
//template<class coupltype>
template<class coupltype>
int DRT::ELEMENTS::So_tet4_volcoupl<coupltype>::NumDofPerNode(const unsigned nds,
                                              const DRT::Node& node) const
{
  if (nds==1)
    return coupltype::NumDofPerNode(nds, node);

  return So_tet4::NumDofPerNode(node);
};

/*----------------------------------------------------------------------*
 |  evaluate the element (public)                                       |
 *----------------------------------------------------------------------*/
//template<class coupltype>
//template< template <DRT::Element::DiscretizationType distype> class coupltype>
template<class coupltype>
int DRT::ELEMENTS::So_tet4_volcoupl<coupltype>::Evaluate(ParameterList& params,
                                    DRT::Discretization&      discretization,
                                    DRT::Element::LocationArray& la,
                                    Epetra_SerialDenseMatrix& elemat1_epetra,
                                    Epetra_SerialDenseMatrix& elemat2_epetra,
                                    Epetra_SerialDenseVector& elevec1_epetra,
                                    Epetra_SerialDenseVector& elevec2_epetra,
                                    Epetra_SerialDenseVector& elevec3_epetra)
{
  // start with "none"
  So_tet4_volcoupl::ActionType act = So_tet4_volcoupl::none;

  // get the required action
  string action = params.get<string>("action","none");
  if (action == "none") dserror("No action supplied");
  //else if (action=="calc_struct_update_istep")          act = So_tet4::calc_struct_update_istep;
  //else if (action=="calc_struct_internalforce")         act = So_tet4::calc_struct_internalforce;
  //else if (action=="calc_struct_nlnstiff")              act = So_tet4::calc_struct_nlnstiff;
  //else if (action=="calc_struct_nlnstiffmass")          act = So_tet4::calc_struct_nlnstiffmass;
  else if (action=="calc_struct_multidofsetcoupling")   act = So_tet4_volcoupl::calc_struct_multidofsetcoupling;
  //else if (action=="postprocess_stress")                act = So_tet4::postprocess_stress;
  //else dserror("Unknown type of action for So_tet4_volcoupl: %s",action.c_str());
  // what should the element do
  switch(act)
  {
  //==================================================================================
  // coupling terms in force-vector and stiffness matrix
  case So_tet4_volcoupl::calc_struct_multidofsetcoupling:
  {
    coupltype::Evaluate(params,
                      discretization,
                      la,
                      elemat1_epetra,
                      elemat2_epetra,
                      elevec1_epetra,
                      elevec2_epetra,
                      elevec3_epetra);
  }
  break;
  //==================================================================================
  default:
  {
    //in some cases we need to write/change some data before evaluating
    coupltype::PreEvaluate(params,
                      discretization,
                      la);

    So_tet4::Evaluate(params,
                      discretization,
                      la[0].lm_,
                      elemat1_epetra,
                      elemat2_epetra,
                      elevec1_epetra,
                      elevec2_epetra,
                      elevec3_epetra);

    coupltype::Evaluate(params,
                      discretization,
                      la,
                      elemat1_epetra,
                      elemat2_epetra,
                      elevec1_epetra,
                      elevec2_epetra,
                      elevec3_epetra);
  }
  } // action

  return 0;
}

template class DRT::ELEMENTS::So_tet4_volcoupl<DRT::ELEMENTS::So3_Poro<DRT::Element::tet4> >;

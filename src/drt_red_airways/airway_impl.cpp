/*----------------------------------------------------------------------*/
/*!
\file airway_impl.cpp

\brief Internal implementation of artery_lin_exp element

<pre>
Maintainer: Mahmoud Ismail
            ismail@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15268
</pre>
*/
/*----------------------------------------------------------------------*/


#ifdef D_RED_AIRWAYS
#ifdef CCADISCRET

#include "airway_impl.H"

#include "../drt_mat/newtonianfluid.H"
#include "../drt_lib/drt_timecurve.H"
#include "../drt_lib/drt_function.H"
#include "../drt_lib/drt_utils.H"
#include "../drt_fem_general/drt_utils_fem_shapefunctions.H"
#include "../drt_fem_general/drt_utils_gder2.H"
#include <fstream>
#include <iomanip>


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::RedAirwayImplInterface* DRT::ELEMENTS::RedAirwayImplInterface::Impl(DRT::ELEMENTS::RedAirway* red_airway)
{
  switch (red_airway->Shape())
  {
  case DRT::Element::line2:
  {
    static AirwayImpl<DRT::Element::line2>* airway;
    if (airway==NULL)
    {
      airway = new AirwayImpl<DRT::Element::line2>;
    }
    return airway;
  }
  default:
    dserror("shape %d (%d nodes) not supported", red_airway->Shape(), red_airway->NumNode());
  }
  return NULL;
}



 /*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::AirwayImpl<distype>::AirwayImpl()
  : qn_(),
    pn_()
{

}

template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::AirwayImpl<distype>::Evaluate(
  RedAirway*                 ele,
  ParameterList&             params,
  DRT::Discretization&       discretization,
  vector<int>&               lm,
  Epetra_SerialDenseMatrix&  elemat1_epetra,
  Epetra_SerialDenseMatrix&  elemat2_epetra,
  Epetra_SerialDenseVector&  elevec1_epetra,
  Epetra_SerialDenseVector&  elevec2_epetra,
  Epetra_SerialDenseVector&  elevec3_epetra,
  RefCountPtr<MAT::Material> mat)
{

  // the number of nodes
  const int numnode = iel;
  vector<int>::iterator it_vcr;

  // construct views
  LINALG::Matrix<1*iel,1*iel> elemat1(elemat1_epetra.A(),true);
  LINALG::Matrix<1*iel,    1> elevec1(elevec1_epetra.A(),true);
  // elemat2, elevec2, and elevec3 are never used anyway

  //----------------------------------------------------------------------
  // get control parameters for time integration
  //----------------------------------------------------------------------

  // get time-step size
  const double dt = params.get<double>("time step size");

  // ---------------------------------------------------------------------
  // get control parameters for stabilization and higher-order elements
  //----------------------------------------------------------------------


  // flag for higher order elements
  //  bool higher_order_ele = ele->isHigherOrderElement(distype);

  // ---------------------------------------------------------------------
  // get all general state vectors: flow, pressure,
  // ---------------------------------------------------------------------

  RefCountPtr<const Epetra_Vector> qnp  = discretization.GetState("qnp");
  RefCountPtr<const Epetra_Vector> pnp  = discretization.GetState("pnp");

  if (qnp==null)
    dserror("Cannot get state vectors 'qnp'");
  if (pnp==null)
    dserror("Cannot get state vectors 'pnp'");

  // extract local values from the global vectors
  vector<double> myqnp(lm.size());
  DRT::UTILS::ExtractMyValues(*qnp,myqnp,lm);

  vector<double> mypnp(lm.size());
  DRT::UTILS::ExtractMyValues(*pnp,mypnp,lm);

  // create objects for element arrays
  LINALG::Matrix<numnode,1> epnp;
  LINALG::Matrix<numnode,1> eqnp;
  for (int i=0;i<numnode;++i)
  {
    // split area and volumetric flow rate, insert into element arrays
    eqnp(i)    = myqnp[i];
    epnp(i)    = mypnp[i];
  }
  // ---------------------------------------------------------------------
  // call routine for calculating element matrix and right hand side
  // ---------------------------------------------------------------------
  Sysmat(ele,
         eqnp,
         epnp,
         elemat1,
         elevec1,
         mat,
         dt);


  return 0;
}


/*----------------------------------------------------------------------*
 |  calculate element matrix and right hand side (private)  ismail 07/09|
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AirwayImpl<distype>::Initial(
  RedAirway*                             ele,
  ParameterList&                         params,
  DRT::Discretization&                   discretization,
  vector<int>&                           lm,
  Teuchos::RCP<const MAT::Material>      material)
{

  RCP<Epetra_Vector> q0    = params.get<RCP<Epetra_Vector> >("q0");
  RCP<Epetra_Vector> p0    = params.get<RCP<Epetra_Vector> >("p0");
  vector<int>        lmown = *(params.get<RCP<vector<int> > >("lmowner"));
  int myrank  = discretization.Comm().MyPID();

  vector<int>::iterator it = lm.begin();

  if(myrank == lmown[0])
  {
    int gid = lm[0];
    double val = 0.0;
    q0->ReplaceGlobalValues(1,&val,&gid);
    p0->ReplaceGlobalValues(1,&val,&gid);
  }
  if(myrank == lmown[1])
  {
    int gid = lm[1];
    double val = 0.0;
    q0->ReplaceGlobalValues(1,&val,&gid);
    p0->ReplaceGlobalValues(1,&val,&gid);
  }


}//ArteryLinExp::Initial

/*----------------------------------------------------------------------*
 |  calculate element matrix and right hand side (private)  ismail 07/09|
 |                                                                      |
 |                                                                      |
 |                                                                      |
 |                               ______                                 |
 |                     _____-----      -----_____                       |
 |           _______---                          ---______              |
 | ->       / \                                         / \   ->        |
 | -->     |   |                                       |   |  -->       |
 | ---->  |     |                                     |     | ---->     |
 | ---->  |     |                                     |     | ---->     |
 | ---->  |     |                                     |     | ---->     |
 | -->     |   |                                       |   |  -->       |
 | ->       \_/_____                                ____\_/   ->        |
 |                  ---_____                _____---                    |
 |                          -----______-----                            |
 |                                                                      |
 |                                                                      |
 |                                                                      |
 |                                                                      |
 |                                                                      |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AirwayImpl<distype>::Sysmat(
  RedAirway*                               ele,
  const LINALG::Matrix<iel,1>&             eqnp,
  const LINALG::Matrix<iel,1>&             epnp,
  LINALG::Matrix<1*iel,1*iel>&             sysmat,
  LINALG::Matrix<1*iel,    1>&             rhs,
  Teuchos::RCP<const MAT::Material>        material,
  double                                   dt)
{

  double dens = 0.0;
  double visc = 0.0;

  if(material->MaterialType() == INPAR::MAT::m_fluid)
  {
    // get actual material
    const MAT::NewtonianFluid* actmat = static_cast<const MAT::NewtonianFluid*>(material.get());

    // get density
    dens = actmat->Density();
    
    // get kinetic viscosity
    visc = actmat->Viscosity();
  }
  else
  {
    dserror("Material law is not a Newtonia fluid");
    exit(1);
  }

  LINALG::Matrix<1*iel,1> qn;
  LINALG::Matrix<1*iel,1> pn;
  for(int i=0; i<iel; i++)
  {
    qn(i,0) = eqnp(i);
    pn(i,0)   = epnp(i);

  }
  // set element data
  const int numnode = iel;
  // get node coordinates and number of elements per node
  DRT::Node** nodes = ele->Nodes();

  LINALG::Matrix<3,iel> xyze;
  for (int inode=0; inode<numnode; inode++)
  {
    const double* x = nodes[inode]->X();
    xyze(0,inode) = x[0];
    xyze(1,inode) = x[1];
    xyze(2,inode) = x[2];
  }

  rhs.Clear();
  sysmat.Clear();


  // check here, if we really have an airway !!

  // Calculate the length of artery element
  const double L=sqrt(
            pow(xyze(0,0) - xyze(0,1),2)
          + pow(xyze(1,0) - xyze(1,1),2)
          + pow(xyze(2,0) - xyze(2,1),2));

  if(ele->type() == "PoiseuilleResistive")
  {
    //------------------------------------------------------------
    //               Calculate the System Matrix
    //------------------------------------------------------------
    const double R = 8.0*PI*visc*dens*L/(pow(ele->getA(),2));
    sysmat(0,0) = -1.0/R  ; sysmat(0,1) =  1.0/R ;
    sysmat(1,0) =  1.0/R  ; sysmat(1,1) = -1.0/R ;
    
    rhs(0) = 0.0;
    rhs(1) = 0.0;

  }
  else if(ele->type() == "InductoResistive")
  {

  }
  else if(ele->type() == "ComplientResistive")
  {
  }
  else if(ele->type() == "RLC")
  {
  }
  else if(ele->type() == "SUKI")
  {

  }
  else
  {
    dserror("[%s] is not an implimented elements yet",(ele->type()).c_str());
    exit(1);
  }


#if 0
  cout<<"+-------------------------!!!!!!!!!!!!!!-------------------------+"<<endl;
  cout<<"+------------------------ THE FINAL R-LHS------------------------+"<<endl;
  cout<<"|+++++++++++++++++++++++++!!!!      !!!!-------------------------|"<<endl;
  cout<<"rhs is: "<<rhs<<endl;
  cout<<"lhs is: "<<sysmat<<endl;
  cout<<"With L= "<<L<<endl;
  cout<<"|+++++++++++++++++++++++++!!!!      !!!!-------------------------|"<<endl;
  cout<<"+-------------------------!!!!!!!!!!!!!!-------------------------+"<<endl;
#endif

}

/*----------------------------------------------------------------------*
 |  Evaluate the values of the degrees of freedom           ismail 07/09|
 |  at terminal nodes.                                                  |
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::AirwayImpl<distype>::EvaluateTerminalBC(
  RedAirway*                   ele,
  ParameterList&               params,
  DRT::Discretization&         discretization,
  vector<int>&                 lm,
  RefCountPtr<MAT::Material>   material)
{
  // get total time
  const double time = params.get<double>("total time");

  // get time-step size
  const double dt = params.get<double>("time step size");

  // the number of nodes
  const int numnode = iel;
  vector<int>::iterator it_vcr;

  RefCountPtr<const Epetra_Vector> pnp  = discretization.GetState("pnp");

  if (pnp==null)
    dserror("Cannot get state vectors 'pnp'");

  // extract local values from the global vectors
  vector<double> mypnp(lm.size());
  DRT::UTILS::ExtractMyValues(*pnp,mypnp,lm);

  // create objects for element arrays
  LINALG::Matrix<numnode,1> epnp;

  //get time step size
  //  const double dt = params.get<double>("time step size");

  //get all values at the last computed time step
  for (int i=0;i<numnode;++i)
  {
    // split area and volumetric flow rate, insert into element arrays
    epnp(i)    = mypnp[i];
  }

  // ---------------------------------------------------------------------------------
  // Resolve the BCs
  // ---------------------------------------------------------------------------------

  for(int i = 0; i<ele->NumNode(); i++)
  {
    DRT::Condition * condition = ele->Nodes()[i]->GetCondition("RedAirwayPrescribedCond");
    if(condition)
    {
      // Get the type of prescribed bc
      string Bc = *(condition->Get<string>("boundarycond"));

      // double get bc value
      double BCin = 0.0;
      
      const  vector<int>*    curve  = condition->Get<vector<int>    >("curve");
      double curvefac = 1.0;
      const  vector<double>* vals   = condition->Get<vector<double> >("val");

      // -----------------------------------------------------------------
      // Read in the value of the applied BC
      // -----------------------------------------------------------------
      if((*curve)[0]>=0)
      {
        curvefac = DRT::Problem::Instance()->Curve((*curve)[0]).f(time);
        BCin = (*vals)[0]*curvefac;
      }
      else
      {
        dserror("no boundary condition defined!");
        exit(1);
      }

      // -----------------------------------------------------------------------------
      // get the local id of the node to whome the bc is prescribed
      // -----------------------------------------------------------------------------
      int local_id =  discretization.NodeRowMap()->LID(ele->Nodes()[i]->Id());
      if (local_id< 0 )
      {
        dserror("node (%d) doesn't exist on proc(%d)",ele->Nodes()[i],discretization.Comm().MyPID());
        exit(1);
      }

      if (Bc == "pressure")
      {
        RefCountPtr<Epetra_Vector> bcval  = params.get<RCP<Epetra_Vector> >("bcval");
        RefCountPtr<Epetra_Vector> dbctog = params.get<RCP<Epetra_Vector> >("dbctog");

        if (bcval==null||dbctog==null)
        {
          dserror("Cannot get state vectors 'bcval' and 'dbctog'");
          exit(1);
        }        
        
               
        // set pressure at node i
        int    gid; 
        double val; 
        
        gid = lm[i];
        val = BCin;
        bcval->ReplaceGlobalValues(1,&val,&gid);
      
        gid = lm[i];
        val = 1;
        dbctog->ReplaceGlobalValues(1,&val,&gid);
      }
      else if (Bc == "flow")
      {
        RefCountPtr<Epetra_Vector> rhs  = params.get<RCP<Epetra_Vector> >("rhs");
        if (rhs==null)
        {
          dserror("Cannot get state vector 'rhs'");
          exit(1);
        }

        // set pressure at node i
        int    gid; 
        double val; 
        
        gid =  lm[i];
        val = -BCin;
        rhs->ReplaceGlobalValues(1,&val,&gid);
      }
      else
      {
        dserror("precribed [%s] is not defined for reduced airways",Bc.c_str());
        exit(1);
      }
        
    }
  } // End of node i has a condition

}

#endif
#endif

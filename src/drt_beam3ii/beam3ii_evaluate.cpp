/*!-----------------------------------------------------------------------------------------------------------
 \file beam3ii_evaluate.cpp
 \brief

<pre>
Maintainer: Christian Cyron
            cyron@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15264
</pre>

 *-----------------------------------------------------------------------------------------------------------*/
#ifdef D_BEAM3II
#ifdef CCADISCRET

#include "beam3ii.H"
#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_exporter.H"
#include "../drt_lib/drt_dserror.H"
#include "../drt_lib/linalg_utils.H"
#include "../drt_lib/drt_timecurve.H"
#include "../drt_fem_general/drt_utils_fem_shapefunctions.H"
#include "../drt_mat/stvenantkirchhoff.H"
#include "../drt_lib/linalg_fixedsizematrix.H"



/*-----------------------------------------------------------------------------------------------------------*
 |  evaluate the element (public)                                                                 cyron 01/08|
 *----------------------------------------------------------------------------------------------------------*/
int DRT::ELEMENTS::Beam3ii::Evaluate(ParameterList& params,
    DRT::Discretization& discretization,
    vector<int>& lm,
    Epetra_SerialDenseMatrix& elemat1,
    Epetra_SerialDenseMatrix& elemat2,
    Epetra_SerialDenseVector& elevec1,
    Epetra_SerialDenseVector& elevec2,
    Epetra_SerialDenseVector& elevec3)
{
  DRT::ELEMENTS::Beam3ii::ActionType act = Beam3ii::calc_none;
  // get the action required
  string action = params.get<string>("action","calc_none");
  if (action == "calc_none") dserror("No action supplied");
  else if (action=="calc_struct_linstiff") act = Beam3ii::calc_struct_linstiff;
  else if (action=="calc_struct_nlnstiff") act = Beam3ii::calc_struct_nlnstiff;
  else if (action=="calc_struct_internalforce") act = Beam3ii::calc_struct_internalforce;
  else if (action=="calc_struct_linstiffmass") act = Beam3ii::calc_struct_linstiffmass;
  else if (action=="calc_struct_nlnstiffmass") act = Beam3ii::calc_struct_nlnstiffmass;
  else if (action=="calc_struct_nlnstifflmass") act = Beam3ii::calc_struct_nlnstifflmass; //with lumped mass matrix
  else if (action=="calc_struct_stress") act = Beam3ii::calc_struct_stress;
  else if (action=="calc_struct_eleload") act = Beam3ii::calc_struct_eleload;
  else if (action=="calc_struct_fsiload") act = Beam3ii::calc_struct_fsiload;
  else if (action=="calc_struct_update_istep") act = Beam3ii::calc_struct_update_istep;
  else if (action=="calc_struct_update_imrlike") act = Beam3ii::calc_struct_update_imrlike;
  else if (action=="calc_struct_reset_istep") act = Beam3ii::calc_struct_reset_istep;
  else if (action=="calc_struct_ptcstiff")        act = Beam3ii::calc_struct_ptcstiff;
  else dserror("Unknown type of action for Beam3ii");

  switch(act)
  {
    case Beam3ii::calc_struct_ptcstiff:
    {
      const int nnode = NumNode();

    	switch(nnode)
   	  {
   	  		case 2:EvaluatePTC<2>(params, elemat1); break;
   	  		case 3:EvaluatePTC<3>(params, elemat1); break;
   	  		case 4:EvaluatePTC<4>(params, elemat1); break;
   	  		case 5:EvaluatePTC<5>(params, elemat1); break;
   	  		default:dserror("Only Line2, Line3, Line4 and Line5 Elements implemented.");
   	  }
    }
    break;
    /*in case that only linear stiffness matrix is required b3_nlstiffmass is called with zero dispalcement and
     residual values*/
    case Beam3ii::calc_struct_linstiff:
    {
      //only nonlinear case implemented!
      dserror("linear stiffness matrix called, but not implemented");

    }
    break;

    //nonlinear stiffness and mass matrix are calculated even if only nonlinear stiffness matrix is required
    case Beam3ii::calc_struct_nlnstiffmass:
    case Beam3ii::calc_struct_nlnstifflmass:
    case Beam3ii::calc_struct_nlnstiff:
    case Beam3ii::calc_struct_internalforce:
    {
      // need current global displacement and residual forces and get them from discretization
      // making use of the local-to-global map lm one can extract current displacemnet and residual values for each degree of freedom
      //
      // get element displcements
      RefCountPtr<const Epetra_Vector> disp = discretization.GetState("displacement");
      if (disp==null) dserror("Cannot get state vectors 'displacement'");
      vector<double> mydisp(lm.size());
      DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);

      // get residual displacements
      RefCountPtr<const Epetra_Vector> res  = discretization.GetState("residual displacement");
      if (res==null) dserror("Cannot get state vectors 'residual displacement'");
      vector<double> myres(lm.size());
      DRT::UTILS::ExtractMyValues(*res,myres,lm);

      //only if random numbers for Brownian dynamics are passed to element, get element velocities
      vector<double> myvel(lm.size());
      if( params.get<  RCP<Epetra_MultiVector> >("RandomNumbers",Teuchos::null) != Teuchos::null)
      {
        RefCountPtr<const Epetra_Vector> vel  = discretization.GetState("velocity");
        DRT::UTILS::ExtractMyValues(*vel,myvel,lm);
      }

      const int nnode = NumNode();

      if (act == Beam3ii::calc_struct_nlnstiffmass)
      {
    	  switch(nnode)
    	  {
  	  		case 2:
  	  		{
  	  			b3_nlnstiffmass<2>(params,myvel,mydisp,&elemat1,&elemat2,&elevec1);
  	  			break;
  	  		}
  	  		case 3:
  	  		{
  	  			b3_nlnstiffmass<3>(params,myvel,mydisp,&elemat1,&elemat2,&elevec1);
  	  			break;
  	  		}
  	  		case 4:
  	  		{
  	  			b3_nlnstiffmass<4>(params,myvel,mydisp,&elemat1,&elemat2,&elevec1);
  	  			break;
  	  		}
  	  		case 5:
  	  		{
  	  			b3_nlnstiffmass<5>(params,myvel,mydisp,&elemat1,&elemat2,&elevec1);
  	  			break;
  	  		}
  	  		default:
  	  			dserror("Only Line2, Line3, Line4 and Line5 Elements implemented.");
    	  }
      }
      else if (act == Beam3ii::calc_struct_nlnstifflmass)
      {
    	  switch(nnode)
    	  {
  	  		case 2:
  	  		{
  	  			b3_nlnstiffmass<2>(params,myvel,mydisp,&elemat1,&elemat2,&elevec1);
  	  			lumpmass<2>(&elemat2);
  	  			break;
  	  		}
  	  		case 3:
  	  		{
  	  			b3_nlnstiffmass<3>(params,myvel,mydisp,&elemat1,&elemat2,&elevec1);
  	  			lumpmass<3>(&elemat2);
  	  			break;
  	  		}
  	  		case 4:
  	  		{
  	  			b3_nlnstiffmass<4>(params,myvel,mydisp,&elemat1,&elemat2,&elevec1);
  	  			lumpmass<4>(&elemat2);
  	  			break;
  	  		}
  	  		case 5:
  	  		{
  	  			b3_nlnstiffmass<5>(params,myvel,mydisp,&elemat1,&elemat2,&elevec1);
  	  			lumpmass<5>(&elemat2);
  	  			break;
  	  		}
  	  		default:
  	  			dserror("Only Line2, Line3, Line4 and Line5 Elements implemented.");
    	  }
      }
      else if (act == Beam3ii::calc_struct_nlnstiff)
      {
    	  switch(nnode)
    	  {
  	  		case 2:
  	  		{
  	  			b3_nlnstiffmass<2>(params,myvel,mydisp,&elemat1,NULL,&elevec1);
  	  			break;
  	  		}
  	  		case 3:
  	  		{
  	  			b3_nlnstiffmass<3>(params,myvel,mydisp,&elemat1,NULL,&elevec1);
  	  			break;
  	  		}
  	  		case 4:
  	  		{
  	  			b3_nlnstiffmass<4>(params,myvel,mydisp,&elemat1,NULL,&elevec1);
  	  			break;
  	  		}
  	  		case 5:
  	  		{
  	  			b3_nlnstiffmass<5>(params,myvel,mydisp,&elemat1,NULL,&elevec1);
  	  			break;
  	  		}
  	  		default:
  	  			dserror("Only Line2, Line3, Line4 and Line5 Elements implemented.");
    	  }
      }

      else if (act == Beam3ii::calc_struct_internalforce)
      {
    	  switch(nnode)
    	  {
  	  		case 2:
  	  		{
  	  			b3_nlnstiffmass<2>(params,myvel,mydisp,NULL,NULL,&elevec1);
  	  			break;
  	  		}
  	  		case 3:
  	  		{
  	  			b3_nlnstiffmass<3>(params,myvel,mydisp,NULL,NULL,&elevec1);
  	  			break;
  	  		}
  	  		case 4:
  	  		{
  	  			b3_nlnstiffmass<4>(params,myvel,mydisp,NULL,NULL,&elevec1);
  	  			break;
  	  		}
  	  		case 5:
  	  		{
  	  			b3_nlnstiffmass<5>(params,myvel,mydisp,NULL,NULL,&elevec1);
  	  			break;
  	  		}
  	  		default:
  	  			dserror("Only Line2, Line3, Line4 and Line5 Elements implemented.");
    	  }
      }

    /*at the end of an iteration step the geometric configuration has to be updated: the starting point for the
     * next iteration step is the configuration at the end of the current step */
    Qold_ = Qnew_;
    dispthetaold_= dispthetanew_;

    
    /*
    //the following code block can be used to check quickly whether the nonlinear stiffness matrix is calculated
    //correctly or not by means of a numerically approximated stiffness matrix
    //The code block will work for all higher order elements.
    if(Id() == 0) //limiting the following tests to certain element numbers
    {
      //variable to store numerically approximated stiffness matrix
      Epetra_SerialDenseMatrix stiff_approx;
      stiff_approx.Shape(6*nnode,6*nnode);


      //relative error of numerically approximated stiffness matrix
      Epetra_SerialDenseMatrix stiff_relerr;
      stiff_relerr.Shape(6*nnode,6*nnode);

      //characteristic length for numerical approximation of stiffness
      double h_rel = 1e-5;

      //flag indicating whether approximation leads to significant relative error
      int outputflag = 0;

      //calculating strains in new configuration
      for(int i=0; i<6; i++) //for all dof
      {
      	for(int k=0; k<nnode; k++)//for all nodes
      	{

      		Epetra_SerialDenseVector force_aux;
      		force_aux.Size(6*nnode);

      		//create new displacement and velocity vectors in order to store artificially modified displacements
      		vector<double> vel_aux(myvel);
      		vector<double> disp_aux(mydisp);

      		//modifying displacement artificially (for numerical derivative of internal forces):
      		disp_aux[6*k + i] += h_rel;
      		vel_aux[6*k + i] += h_rel / params.get<double>("delta time",0.01);

      		//b3_nlnstiffmass is a templated function. therefore we need to point out the number of nodes in advance
      	  switch(nnode)
      	  {
      	  		case 2:
      	  		{
      	  			b3_nlnstiffmass<2>(params,vel_aux,disp_aux,NULL,NULL,&force_aux);
      	  			break;
      	  		}
      	  		case 3:
      	  		{
      	  			b3_nlnstiffmass<3>(params,vel_aux,disp_aux,NULL,NULL,&force_aux);
      	  			break;
      	  		}
      	  		case 4:
      	  		{
      	  			b3_nlnstiffmass<4>(params,vel_aux,disp_aux,NULL,NULL,&force_aux);
      	  			break;
      	  		}
      	  		case 5:
      	  		{
      	  			b3_nlnstiffmass<5>(params,vel_aux,disp_aux,NULL,NULL,&force_aux);
      	  			break;
      	  		}
      	  		default:
      	  			dserror("Only Line2, Line3, Line4 and Line5 Elements implemented.");
      	  }

        	//computing derivative d(fint)/du numerically by finite difference
      		for(int u = 0 ; u < 6*nnode ; u++ )
      			stiff_approx(u,k*6+i)= ( pow(force_aux[u],2) - pow(elevec1(u),2) )/ (h_rel * (force_aux[u] + elevec1(u) ) );

      	} //for(int k=0; k<nnode; k++)//for all nodes

      } //for(int i=0; i<3; i++) //for all dof


      for(int line=0; line<6*nnode; line++)
      {
      	for(int col=0; col<6*nnode; col++)
      	{
      		stiff_relerr(line,col)= fabs( ( pow(elemat1(line,col),2) - pow(stiff_approx(line,col),2) )/ ( (elemat1(line,col) + stiff_approx(line,col)) * elemat1(line,col) ));

      		//suppressing small entries whose effect is only confusing and NaN entires (which arise due to zero entries)
      		if ( fabs( stiff_relerr(line,col) ) < h_rel*500 || isnan( stiff_relerr(line,col)) || elemat1(line,col) == 0) //isnan = is not a number
      			stiff_relerr(line,col) = 0;

      		//if ( stiff_relerr(line,col) > 0)
      			outputflag = 1;
      	} //for(int col=0; col<3*nnode; col++)

      } //for(int line=0; line<3*nnode; line++)

      if(outputflag ==1) 
      {
        
        std::cout<<"\n\n acutally calculated stiffness matrix\n";
        for(int line=0; line<6*nnode; line++)
        {
          for(int col=0; col<6*nnode; col++)
          {
            if(isnan(elemat1(line,col)))
              std::cout<<"     nan   ";
            else if(elemat1(line,col) == 0)
              std::cout<<"     0     ";           
            else if(elemat1(line,col) >= 0)
              std::cout<<"  "<< scientific << setprecision(3)<<elemat1(line,col);
            else
              std::cout<<" "<< scientific << setprecision(3)<<elemat1(line,col);
          }
          std::cout<<"\n";
        }
        
        std::cout<<"\n\n approximated stiffness matrix\n";
        for(int line=0; line<6*nnode; line++)
        {
          for(int col=0; col<6*nnode; col++)
          {
            if(isnan(stiff_approx(line,col)))
              std::cout<<"     nan   ";
            else if(stiff_approx(line,col) == 0)
              std::cout<<"     0     "; 
            else if(stiff_approx(line,col) >= 0)
              std::cout<<"  "<< scientific << setprecision(3)<<stiff_approx(line,col);
            else
              std::cout<<" "<< scientific << setprecision(3)<<stiff_approx(line,col);
          }
          std::cout<<"\n";
        }
        
        std::cout<<"\n\n rel error stiffness matrix\n";
        for(int line=0; line<6*nnode; line++)
        {
          for(int col=0; col<6*nnode; col++)
          {
            if(isnan(stiff_relerr(line,col)))
              std::cout<<"     nan   ";
            else if(stiff_relerr(line,col) == 0)
              std::cout<<"     0     ";
            else if(stiff_relerr(line,col) >= 0)
              std::cout<<"  "<< scientific << setprecision(3)<<stiff_relerr(line,col);
            else
              std::cout<<" "<< scientific << setprecision(3)<<stiff_relerr(line,col);
          }
          std::cout<<"\n";
        }
      }

    } //end of section in which numerical approximation for stiffness matrix is computed
    */


    }
    break;
    case calc_struct_update_istep:
    case calc_struct_update_imrlike:
    {
      /*the action calc_struct_update_istep is called in the very end of a time step when the new dynamic
       * equilibrium has finally been found; this is the point where the variable representing the geometric
       * status of the beam have to be updated; the geometric status is represented by means of the triads Tnew_,
       * the curvatures curvnew_ and the angular values thetaanew_ and thetaprimenew_*/
      Qconv_ = Qnew_;
      dispthetaconv_ = dispthetanew_;
    }
    break;
    case calc_struct_reset_istep:
    {
      /*the action calc_struct_reset_istep is called by the adaptive time step controller; carries out one test
       * step whose purpose is only figuring out a suitabel timestep; thus this step may be a very bad one in order
       * to iterated towards the new dynamic equilibrium and the thereby gained new geometric configuration should
       * not be applied as starting point for any further iteration step; as a consequence the thereby generated change
       * of the geometric configuration should be canceled and the configuration should be reset to the value at the
       * beginning of the time step*/
      Qold_ = Qconv_;
      dispthetaold_ = dispthetaconv_;
    }
    break;
    case calc_struct_stress:
      dserror("No stress output implemented for beam3ii elements");
    default:
      dserror("Unknown type of action for Beam3ii %d", act);
  }
  return 0;

}

/*-----------------------------------------------------------------------------------------------------------*
 |  Integrate a Surface Neumann boundary condition (public)                                       cyron 03/08|
 *----------------------------------------------------------------------------------------------------------*/

int DRT::ELEMENTS::Beam3ii::EvaluateNeumann(ParameterList& params,
                                        DRT::Discretization& discretization,
                                        DRT::Condition& condition,
                                        vector<int>& lm,
                                        Epetra_SerialDenseVector& elevec1,
                                        Epetra_SerialDenseMatrix* elemat1)
{
  // get element displacements
  RefCountPtr<const Epetra_Vector> disp = discretization.GetState("displacement");
  if (disp==null) dserror("Cannot get state vector 'displacement'");
  vector<double> mydisp(lm.size());
  DRT::UTILS::ExtractMyValues(*disp,mydisp,lm);
  // get element velocities (UNCOMMENT IF NEEDED)
  /*
  RefCountPtr<const Epetra_Vector> vel  = discretization.GetState("velocity");
  if (vel==null) dserror("Cannot get state vectors 'velocity'");
  vector<double> myvel(lm.size());
  DRT::UTILS::ExtractMyValues(*vel,myvel,lm);
  */

  // find out whether we will use a time curve
  bool usetime = true;
  const double time = params.get("total time",-1.0);
  if (time<0.0) usetime = false;

  // find out whether we will use a time curve and get the factor
  const vector<int>* curve = condition.Get<vector<int> >("curve");
  int curvenum = -1;
  // number of the load curve related with a specific line Neumann condition called
  if (curve) curvenum = (*curve)[0];
  // amplitude of load curve at current time called
  double curvefac = 1.0;
  if (curvenum>=0 && usetime)//notation for this function similar to Crisfield, Volume 1;
  curvefac = DRT::Problem::Instance()->Curve(curvenum).f(time);

  // no. of nodes on this element; the following line is only valid for elements with constant number of
  // degrees of freedom per node
  const int numdf = 6;
  const DiscretizationType distype = this->Shape();

  // gaussian points
  const DRT::UTILS::IntegrationPoints1D intpoints(MyGaussRule(NumNode(),gaussunderintegration));

  //declaration of variable in order to store shape function
  Epetra_SerialDenseVector funct(NumNode());

  // get values and switches from the condition

  // onoff is related to the first 6 flags of a line Neumann condition in the input file;
  // value 1 for flag i says that condition is active for i-th degree of freedom
  const vector<int>* onoff = condition.Get<vector<int> >("onoff");
  // val is related to the 6 "val" fields after the onoff flags of the Neumann condition
  // in the input file; val gives the values of the force as a multiple of the prescribed load curve
  const vector<double>* val = condition.Get<vector<double> >("val");

  //integration loops
  for (int numgp=0; numgp<intpoints.nquad; ++numgp)
  {
    //integration points in parameter space and weights
    const double xi = intpoints.qxg[numgp][0];
    const double wgt = intpoints.qwgt[numgp];

    //evaluation of shape funcitons at Gauss points
    DRT::UTILS::shape_function_1D(funct,xi,distype);

    double fac=0;
    fac = wgt * jacobi_[numgp];

    // load vector ar
    double ar[numdf];

    // loop the dofs of a node
    for (int dof=0; dof<numdf; ++dof)
      ar[dof] = fac * (*onoff)[dof]*(*val)[dof]*curvefac;


    //sum up load components
    for (int node=0; node<NumNode(); ++node)
      for (int dof=0; dof<numdf; ++dof)
        elevec1[node*numdf+dof] += funct[node] *ar[dof];

  } // for (int numgp=0; numgp<intpoints.nquad; ++numgp)

  return 0;
}

/*---------------------------------------------------------------------------*
 |computes from a quaternion q rodrigues parameters omega (public)cyron02/09|
 *---------------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::quaterniontorodrigues(const LINALG::Matrix<4,1>& q, LINALG::Matrix<3,1>& omega)
{
  /*the Rodrigues parameters are defined only for angles whose absolute valued is smaller than PI, i.e. for which
   * the fourth component of the quaternion is unequal zero; if this is not satisfied for the quaternion passed into
   * this method an error is thrown*/
  if(q(3) == 0)
    dserror("cannot compute Rodrigues parameters for angles with absolute valued PI !!!");

  //in any case except for the one dealt with above the angle can be computed from a quaternion via Crisfield, Vol. 2, eq. (16.79)
  for(int i = 0; i<3; i++)
    omega(i) = q(i)*2/q(3);


  return;
} //DRT::ELEMENTS::Beam3ii::quaterniontorodrigues

/*----------------------------------------------------------------------*
 |computes from a quaternion q the related angle theta (public)cyron10/08|
 *----------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::quaterniontoangle(const LINALG::Matrix<4,1>& q, LINALG::Matrix<3,1>& theta)
{
  /*the following function computes from a quaternion q an angle theta within [-PI; PI]; such an interval is
   * imperative for the use of the resulting angle together with formulae like Crisfield, Vol. 2, equation (16.90);
   * note that these formulae comprise not only trigonometric functions, but rather the angle theta directly. Hence
   * they are not 2*PI-invariant !!! */
    
  //if the rotation angle is pi we have q(3) == 0 and the rotation angle vector can be computed by
  if(q(3) == 0)
  {
    //note that with q(3) == 0 the first three elements of q represent the unit direction vector of the angle
    //according to Crisfield, Vol. 2, equation (16.67)
    for(int i = 0; i<3; i++)
      theta(i) = q(i) * M_PI;
  }
  else
  {
    //otherwise the angle can be computed from a quaternion via Crisfield, Vol. 2, eq. (16.79)
    LINALG::Matrix<3,1> omega;
    for(int i = 0; i<3; i++)
      omega(i) = q(i)*2/q(3);
  
    double tanhalf = omega.Norm2() / 2;
    double thetaabs = atan(tanhalf)*2;
    
    //if the rotation angle is zero we return a zero rotation angle vector at once
    if(omega.Norm2() == 0)
    {
      for(int i = 0; i<3; i++)
        theta(i) = 0;


    }
    else
      for(int i = 0; i<3; i++)
        theta(i) = thetaabs* omega(i) / omega.Norm2();
  }

  return;
} //DRT::ELEMENTS::Beam3ii::quaterniontoangle()


/*---------------------------------------------------------------------------*
 |computes a spin matrix out of a rotation vector 		   (public)cyron02/09|
 *---------------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::computespin(LINALG::Matrix<3,3>& spin, LINALG::Matrix<3,1> rotationangle)
{
  //function based on Crisfield Vol. 2, Section 16 (16.8)
  spin(0,0) = 0;
  spin(0,1) = -rotationangle(2);
  spin(0,2) = rotationangle(1);
  spin(1,0) = rotationangle(2);
  spin(1,1) = 0;
  spin(1,2) = -rotationangle(0);
  spin(2,0) = -rotationangle(1);
  spin(2,1) = rotationangle(0);
  spin(2,2) = 0;

  return;
} // DRT::ELEMENTS::Beam3ii::computespin


/*----------------------------------------------------------------------*
 |computes a rotation matrix R from a quaternion q						|
 |cf. Crisfield, Vol. 2, equation (16.70) 			  (public)cyron10/08|
 *----------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::quaterniontotriad(const LINALG::Matrix<4,1>& q, LINALG::Matrix<3,3>& R)
{
  //separate storage of vector part of q
  LINALG::Matrix<3,1> qvec;
  for(int i = 0; i<3; i++)
    qvec(i) = q(i);

  //setting R to third summand of equation (16.70)
  computespin(R, qvec);
  R.Scale(2*q(3));

  //adding second summand of equation (16.70)
  for(int i = 0; i<3; i++)
    for(int j = 0; j<3; j++)
      R(i,j) += 2*q(i)*q(j);

  //adding diagonal entries according to first summand of equation (16.70)
  R(0,0) = 1 - 2*(q(1)*q(1) + q(2)*q(2));
  R(1,1) = 1 - 2*(q(0)*q(0) + q(2)*q(2));
  R(2,2) = 1 - 2*(q(0)*q(0) + q(1)*q(1));

  return;
} // DRT::ELEMENTS::Beam3ii::quaterniontotriad




/*---------------------------------------------------------------------------*
 |computes a quaternion from an angle vector 		  	   (public)cyron02/09|
 *---------------------------------------------------------------------------*/
void DRT::ELEMENTS::Beam3ii::angletoquaternion(const LINALG::Matrix<3,1>& theta, LINALG::Matrix<4,1>& q)
{
  //absolute value of rotation angle theta
  double abs_theta = theta.Norm2();

  //computing quaterion for rotation by angle theta, Crisfield, Vol. 2, equation (16.67)
  if (abs_theta > 0)
  {
    q(0) = theta(0) * sin(abs_theta / 2) / abs_theta;
    q(1) = theta(1) * sin(abs_theta / 2) / abs_theta;
    q(2) = theta(2) * sin(abs_theta / 2) / abs_theta;
    q(3) = cos(abs_theta / 2);
  }
  else
  {
    q.PutScalar(0.0);
    q(3) = 1;
  }

  return;
}// DRT::ELEMENTS::Beam3ii::angletoquaternion




/*---------------------------------------------------------------------------*
 |computes a quaternion q from a rotation matrix R; all operations are      |
 |performed according to Crisfield, Vol. 2, section 16.10 and the there      |
 |described Spurrier's algorithm		  	   			   (public)cyron02/09|
 *---------------------------------------------------------------------------*/
void DRT::ELEMENTS::Beam3ii::triadtoquaternion(const LINALG::Matrix<3,3>& R, LINALG::Matrix<4,1>& q)
{
  double trace = R(0,0) + R(1,1) + R(2,2);
  if(trace>R(0,0)  && trace>R(1,1) && trace>R(2,2))
  {
    q(3) = 0.5 * pow(1 + trace, 0.5);
    q(0) = (R(2,1) - R(1,2)) / (4*q(3));
    q(1) = (R(0,2) - R(2,0)) / (4*q(3));
    q(2) = (R(1,0) - R(0,1)) / (4*q(3));
  }
  else
  {
    for(int i = 0 ; i<3 ; i++)
    {
      int j = (i+1)% 3;
      int k = (i+2)% 3;

      if(R(i,i) >= R(j,j) && R(i,i) >= R(k,k))
      {
        //equation (16.78a)
        q(i) = pow(0.5*R(i,i) + 0.25*(1 - trace) , 0.5);

        //equation (16.78b)
        q(3) = 0.25*(R(k,j) - R(j,k)) / q(i);

        //equation (16.78c)
        q(j) = 0.25*(R(j,i) + R(i,j)) / q(i);
        q(k) = 0.25*(R(k,i) + R(i,k)) / q(i);
       }
     }
   }
  return;
}// DRT::ELEMENTS::Beam3ii::TriadToQuaternion




/*---------------------------------------------------------------------------*
 |matrix T(\theta) from Jelenic 1999, eq. (2.5)  	       (public) cyron 04/10|
 *---------------------------------------------------------------------------*/
LINALG::Matrix<3,3> DRT::ELEMENTS::Beam3ii::Tmatrix(LINALG::Matrix<3,1> theta)
{
  LINALG::Matrix<3,3> result;
  double theta_abs = pow(theta(0)*theta(0) + theta(1)*theta(1) + theta(2)*theta(2) ,0.5);

  //in case of theta_abs == 0 the following computation has problems with singularities
  if(theta_abs > 0)
  {
    computespin(result, theta);
    result.Scale(-0.5);

    for(int i = 0; i<3; i++)
      result(i,i) += theta_abs/( 2*tan(theta_abs/2) );

    for(int i = 0; i<3; i++)
      for(int j=0; j<3; j++)
        result(i,j) += theta(i) * theta(j) * (1 - theta_abs/(2*tan(theta_abs/2)) )/pow(theta_abs,2);
  }
  //in case of theta_abs == 0 H(theta) is the identity matrix and hence also Hinv
  else
  {
    result.PutScalar(0.0);
    for(int j=0; j<3; j++)
      result(j,j) = 1;
  }

  return result;
}// DRT::ELEMENTS::Beam3ii::Tmatrix




/*---------------------------------------------------------------------------*
 |matrix T(\theta)^{-1} from Jelenic 1999, eq. (2.5)     (public) cyron 04/10|
 *---------------------------------------------------------------------------*/
LINALG::Matrix<3,3> DRT::ELEMENTS::Beam3ii::Tinvmatrix(LINALG::Matrix<3,1> theta)
{

  LINALG::Matrix<3,3> result;
  double theta_abs = pow(theta(0)*theta(0) + theta(1)*theta(1) + theta(2)*theta(2) ,0.5);

  //in case of theta_abs == 0 the following computation has problems with singularities
  if(theta_abs > 0)
  {
    //ultimate term in eq. (2.5)
    computespin(result, theta);
    result.Scale((1-cos(theta_abs)) / pow(theta_abs,2));

    //penultimate term in eq. (2.5)
    for(int i = 0; i<3; i++)
      result(i,i) += sin(theta_abs)/(theta_abs);

    //first term on the right side in eq. (2.5)
    for(int i = 0; i<3; i++)
      for(int j=0; j<3; j++)
        result(i,j) += theta(i) * theta(j) * (1 - sin(theta_abs)/(theta_abs) )/pow(theta_abs,2);
  }
  //in case of theta_abs == 0 H(theta) is the identity matrix and hence also Hinv
  else
  {
    result.PutScalar(0.0);
    for(int j=0; j<3; j++)
      result(j,j) = 1;
  }

  return result;
}// DRT::ELEMENTS::Beam3ii::Tinvmatrix

/*-----------------------------------------------------------------------------------*
 |computes inverse quaternion q^{-1} for input quaternion q 	   (public)cyron02/09|
 *-----------------------------------------------------------------------------------*/
inline LINALG::Matrix<4,1> DRT::ELEMENTS::Beam3ii::inversequaternion(const LINALG::Matrix<4,1>& q)
{
  //square norm ||q||^2 of quaternion q
  double qnormsq = q.Norm2() * q.Norm2();

  //declaration of variable for inverse quaternion
  LINALG::Matrix<4,1> qinv;

  //inverse quaternion q^(-1) = [-q0, -q1, -q2, q3] / ||q||^2;
  for(int i = 0; i<3; i++)
    qinv(i) = -q(i) / qnormsq;

  qinv(3) = q(3) / qnormsq;

  return qinv;

} //DRT::ELEMENTS::Beam3ii::inversequaternion

/*---------------------------------------------------------------------------------------------------*
 |quaternion product q12 = q2*q1, Crisfield, Vol. 2, equation (16.71)		  	   (public)cyron02/09|
 *---------------------------------------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::quaternionproduct(const LINALG::Matrix<4,1>& q1,const LINALG::Matrix<4,1>& q2,LINALG::Matrix<4,1>& q12)
{
  q12(0) = q2(3)*q1(0) + q1(3)*q2(0) + q2(1)*q1(2) - q1(1)*q2(2);
  q12(1) = q2(3)*q1(1) + q1(3)*q2(1) + q2(2)*q1(0) - q1(2)*q2(0);
  q12(2) = q2(3)*q1(2) + q1(3)*q2(2) + q2(0)*q1(1) - q1(0)*q2(1);
  q12(3) = q2(3)*q1(3) - q2(2)*q1(2) - q2(1)*q1(1) - q2(0)*q1(0);
} //DRT::ELEMENTS::Beam3ii::quaternionproduct

/*---------------------------------------------------------------------------------------------------*
 |compute relative rotation qrel from rotation q1 to rotation q1 (all rotations in quaterion format) |
 |                                                                                 (public)cyron02/09|
 *---------------------------------------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::relrot(const LINALG::Matrix<4,1>& q1,const LINALG::Matrix<4,1>& q2,LINALG::Matrix<4,1>& qrel)
{
  quaternionproduct(q2,inversequaternion(q1),qrel);
} //DRT::ELEMENTS::Beam3ii::relrot

/*----------------------------------------------------------------------------------------------------------------------*
 |compute convected stresses from convected strains and returing also constitutive matrix between both according to     |
 |Jelenic 1999, section 2.4)                                                                                 cyron 04/10|
 *----------------------------------------------------------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::strainstress(const LINALG::Matrix<3,1>& gamma, const LINALG::Matrix<3,1>& kappa,
                                                LINALG::Matrix<3,1>& stressN, LINALG::Matrix<3,3>& CN,
                                                LINALG::Matrix<3,1>& stressM, LINALG::Matrix<3,3>& CM)
{
  //first of all we get the material law
  Teuchos::RCP<const MAT::Material> currmat = Material();
  double ym = 0;
  double sm = 0;
  double density = 0;

  //assignment of material parameters; only St.Venant material is accepted for this beam
  switch(currmat->MaterialType())
  {
    case INPAR::MAT::m_stvenant:// only linear elastic material supported
    {
      const MAT::StVenantKirchhoff* actmat = static_cast<const MAT::StVenantKirchhoff*>(currmat.get());
      ym = actmat->Youngs();
      sm = ym / (2*(1 + actmat->PoissonRatio()));
      density = actmat->Density();
    }
    break;
    default:
    dserror("unknown or improper type of material law");
  }
  
  //defining convected constitutive matrix CN between gamma and N according to Jelenic 1999, section 2.4
  CN.PutScalar(0);
  CN(0,0) = ym*crosssec_;
  CN(1,1) = sm*crosssecshear_;
  CN(2,2) = sm*crosssecshear_;

  //defining convected constitutive matrix CM between kappa and M according to Jelenic 1999, section 2.4
  CM.PutScalar(0);
  CM(0,0) = sm*Irr_;
  CM(1,1) = ym*Iyy_;
  CM(2,2) = ym*Iyy_;
  
  //computing stresses by multiplying strains with respective constitutive matrix
  stressN.Multiply(CN,gamma);
  stressM.Multiply(CM,kappa);

  return;
} // DRT::ELEMENTS::Beam3ii::straintostress

/*----------------------------------------------------------------------------------------------------------------------*
 |push forward stresses and constitutive matrix to their spatial counterparts by rotation matrix Lambda according to    |
 |Romero 2004, eq. (3.10)                                                                                    cyron 04/10|
 *----------------------------------------------------------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::pushforward(const LINALG::Matrix<3,3>& Lambda,
                                                const LINALG::Matrix<3,1>& stressN , const LINALG::Matrix<3,3>& CN,
                                                const LINALG::Matrix<3,1>& stressM , const LINALG::Matrix<3,3>& CM,
                                                LINALG::Matrix<3,1>& stressn, LINALG::Matrix<3,3>& cn,
                                                LINALG::Matrix<3,1>& stressm, LINALG::Matrix<3,3>& cm)
{
  //introduce auxiliary variable for pushforward of rotational matrices
  LINALG::Matrix<3,3> temp;
  
  //push forward translational stresses
  stressn.Multiply(Lambda,stressN);
  
  //pushforward translational constitutive matrix CN to matrix cn according to Jelenic 1999, paragraph following to (2.22) on page 148
  temp.Multiply(Lambda,CN);
  cn.MultiplyNT(temp,Lambda);
  
  //push forward rotational stresses stresses
  stressm.Multiply(Lambda,stressM);
  
  //pushforward translational constitutive matrix CM to matrix cm according to Jelenic 1999, paragraph following to (2.22) on page 148
  temp.Multiply(Lambda,CM);
  cm.MultiplyNT(temp,Lambda);

   return;
} // DRT::ELEMENTS::Beam3ii::pushforward

/*----------------------------------------------------------------------------------------------------------------------*
 |compute convected strain at certain Gauss point with triad rotmat according to Crisfield 1999, eq. (3.4) and eq. (4.9)|
 |                                                                                                           cyron 04/10|
 *----------------------------------------------------------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::computestrain(const LINALG::Matrix<3,1>& rprime, const LINALG::Matrix<3,3>& Lambda,
                                                  LINALG::Matrix<3,1>& gamma, LINALG::Matrix<3,1>& kappa)
{

  //convected strain gamma according to Crisfield 1999, eq. (3.4) 
  gamma.MultiplyTN(Lambda,rprime);
  gamma(0) = gamma(0) - 1.0;
  
  /*the below curvature computation is possible for 2-noded elements only; for higher order elements one might replace it by 
   *a computation according to eq. (2.12), Jelenic 1999*/
  if(NumNode()>2)
    dserror("computation of curvature in beam3ii element implemented only for 2 nodes!");
  
  //compute global and local rotational vectors phi according to Crisfield 1999,(4.6) in quaterion form
  LINALG::Matrix<4,1> phi12;
  quaternionproduct(Qnew_[1],inversequaternion(Qnew_[0]),phi12); 
  
  //according o Crisfield 1999, eq. (4.9), kappa equals the vector corresponding to phi12 divided by the element reference length
  quaterniontoangle(phi12,kappa);
  kappa.Scale(0.5/jacobi_[0]);


   return;
} // DRT::ELEMENTS::Beam3ii::computestrain

/*----------------------------------------------------------------------------------------------------------------------*
 |compute d(T^{-1})/dx according to the two-lined equation below (3.19) on page 152 of Jelenic 1999                     |
 |                                                                                                           cyron 04/10|
 *----------------------------------------------------------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::computedTinvdx(const LINALG::Matrix<3,1>& Psil, const LINALG::Matrix<3,1>& Psilprime, LINALG::Matrix<3,3>& dTinvdx)
{
  //auxiliary matrix for storing intermediate results
  LINALG::Matrix<3,3> auxmatrix;
  
  //norm of \Psi^l:
  double normPsil = Psil.Norm2();
  
  //for relative rotations smaller then 1e-12 we use the limit for Psil -> 0 according to the comment above NOTE 4 on page 152, Jelenic 1999
  if(Psil.Norm2() < 1e-12)
  {
    computespin(dTinvdx,Psilprime);
    dTinvdx.Scale(0.5); 
  }
  else
  {  
    //scalarproduct \Psi^{l,t} \cdot \Psi^{l,'}
    double scalarproductPsilPsilprime = 0;
    for(int i=0; i<3; i++)
      for(int j=0; j<3; j++)
        scalarproductPsilPsilprime += Psil(i)*Psilprime(i);
    
    //spin matrices of Psil and Psilprime
    LINALG::Matrix<3,3> spinPsil;
    LINALG::Matrix<3,3> spinPsilprime;
    computespin(spinPsil,Psil);
    computespin(spinPsilprime,Psilprime);
    
    //thrid summand
    dTinvdx.Multiply(spinPsilprime,spinPsil);
    auxmatrix.Multiply(spinPsil,spinPsilprime);
    dTinvdx += auxmatrix;
    dTinvdx.Scale(1-(sin(normPsil)/normPsil)/pow(normPsil,2));
    
    //first summand
    auxmatrix.PutScalar(0);
    auxmatrix += spinPsil;
    auxmatrix.Scale( scalarproductPsilPsilprime*(normPsil*sin(normPsil) - 2*(1-cos(normPsil)))/pow(normPsil,4) ); 
    dTinvdx += auxmatrix;

    //second summand
    auxmatrix.PutScalar(0);
    auxmatrix += spinPsilprime;
    auxmatrix.Scale((1-cos(Psilprime.Norm2()))/pow(normPsil,2));
    dTinvdx += auxmatrix;

    //fourth summand
    auxmatrix.Multiply(spinPsil,spinPsil);
    auxmatrix.Scale( scalarproductPsilPsilprime*(3*sin(normPsil) - normPsil*(2+cos(normPsil)))/pow(normPsil,5) );
    dTinvdx += auxmatrix;

  }

   return;
} // DRT::ELEMENTS::Beam3ii::computedTinvdx

/*----------------------------------------------------------------------------------------------------------------------*
 |compute \tilde{I}^i in (3.18), page 152, Jelenic 1999, for all nodes i at a certaom Gauss point            cyron 04/10|
 *----------------------------------------------------------------------------------------------------------------------*/
template<int nnode>
inline void DRT::ELEMENTS::Beam3ii::computeItilde(const LINALG::Matrix<3,1>& Psil, const LINALG::Matrix<3,1>& Psilprime, vector<LINALG::Matrix<3,3> >& Itilde,
                                                  const LINALG::Matrix<3,1>& phiIJ, const LINALG::Matrix<3,3>& Lambdar,
                                                  const vector<LINALG::Matrix<3,1> >& Psili, const LINALG::Matrix<1,nnode>& funct)
{
  //auxiliary matrices for storing intermediate results 
  LINALG::Matrix<3,3> auxmatrix;
  auxmatrix.PutScalar(0);
  
  //compute squared brackets term in (3.18), Jelenic 1999v
  LINALG::Matrix<3,3> squaredbrackets;
  squaredbrackets.PutScalar(0);
  for(int node=0; node<nnode; ++node)
  {
    auxmatrix = Tmatrix(Psili[node]);
    auxmatrix.Scale(funct(node));
    squaredbrackets -= auxmatrix;
  }
  for(int i=0; i<3; i++)
    squaredbrackets(i,i) += 1;

  //loop through all nodes i
  for (int node=0; node<nnode; ++node)
  {   

    //compute rightmost term in curley brackets in (3.18), Jelenic 1999
    Itilde[node].PutScalar(0);
    Itilde[node].Multiply(Tinvmatrix(Psil),Tmatrix(Psili[node]));
    Itilde[node].Scale(funct(node));
    
    //if node i is node I then add squared bracktets term times v_I
    if(node == nodeI_)
    { 
      auxmatrix.Multiply(squaredbrackets,vI(phiIJ));  
      Itilde[node] += auxmatrix;   
    }
    
    //if node i is node J then add squared bracktets term times v_J
    if(node == nodeJ_)
    { 
      auxmatrix.Multiply(squaredbrackets,vJ(phiIJ));  
      Itilde[node] += auxmatrix;   
    }
    
    //now the term in the curley bracktets has been computed and has to be rotated by \Lambda_r and \Lambda_r^t
    auxmatrix.MultiplyNT(Itilde[node],Lambdar);
    Itilde[node].MultiplyNN(Lambdar,auxmatrix);

  }
  


   return;
} // DRT::ELEMENTS::Beam3ii::computeItilde

/*----------------------------------------------------------------------------------------------------------------------*
 |compute \tilde{I}^{i'} in (3.19), page 152, Jelenic 1999 for all nodes i at a certaom Gauss point          cyron 04/10|
 *----------------------------------------------------------------------------------------------------------------------*/
template<int nnode>
inline void DRT::ELEMENTS::Beam3ii::computeItildeprime(const LINALG::Matrix<3,1>& Psil, const LINALG::Matrix<3,1>& Psilprime, vector<LINALG::Matrix<3,3> >& Itildeprime,
                                                       const LINALG::Matrix<3,1>& phiIJ, const LINALG::Matrix<3,3>& Lambdar,
                                                       const vector<LINALG::Matrix<3,1> >& Psili, const LINALG::Matrix<1,nnode>& funct, const LINALG::Matrix<1,nnode>& deriv)
{

  //auxiliary matrices for storing intermediate results 
  LINALG::Matrix<3,3> auxmatrix;
  
  //matrix d(T^{-1})/dx
  LINALG::Matrix<3,3> dTinvdx;
  computedTinvdx(Psil,Psilprime,dTinvdx);

  //compute T^{~} according to remark subsequent to (3.19), Jelenic 1999
  LINALG::Matrix<3,3> Ttilde;
  Ttilde.PutScalar(0);
  for(int node=0; node<nnode; ++node)
  {
    auxmatrix = Tmatrix(Psili[node]);
    auxmatrix.Scale(funct(node));
    Ttilde += auxmatrix;
  }

  //compute T^{~'} according to remark subsequent to (3.19), Jelenic 1999
  LINALG::Matrix<3,3> Ttildeprime;
  Ttildeprime.PutScalar(0);
  for(int node=0; node<nnode; ++node)
  {
    auxmatrix = Tmatrix(Psili[node]);
    auxmatrix.Scale(deriv(node));
    Ttildeprime += auxmatrix;
  }
  
  //compute first squared brackets term in (3.18), Jelenic 1999v
  LINALG::Matrix<3,3> squaredbrackets;
  squaredbrackets.PutScalar(0);
  squaredbrackets.Multiply(dTinvdx,Ttilde);
  auxmatrix.Multiply(Tinvmatrix(Psil),Ttildeprime);
  squaredbrackets += auxmatrix;
  
  //loop through all nodes i
  for (int node=0; node<nnode; ++node)
  {   
  
    //compute first term in second squared brackets
    Itildeprime[node] = dTinvdx;
    Itildeprime[node].Scale(funct(node));
    
    //compute second term in second squared brackets
    auxmatrix.PutScalar(0);
    auxmatrix += Tinvmatrix(Psil);
    auxmatrix.Scale(deriv(node));
    
    //compute second squared brackets
    auxmatrix += Itildeprime[node];
    
    //compute second squared bracketds time T(\Psi^l_j)
    Itildeprime[node].Multiply(auxmatrix,Tmatrix(Psili[node]));
  
    //if node i is node I then add first squared bracktets term times v_I
    if(node == nodeI_)
    { 
      auxmatrix.Multiply(squaredbrackets,vI(phiIJ));  
      Itildeprime[node] -= auxmatrix;   
    }
    
    //if node i is node J then add first squared bracktets term times v_J
    if(node == nodeJ_)
    { 
      auxmatrix.Multiply(squaredbrackets,vJ(phiIJ));
      Itildeprime[node] -= auxmatrix;   
    }
    
    //now the term in the curley bracktets has been computed and has to be rotated by \Lambda_r and \Lambda_r^t
    auxmatrix.MultiplyNT(Itildeprime[node],Lambdar);
    Itildeprime[node].MultiplyNN(Lambdar,auxmatrix);
  }

   return;
} // DRT::ELEMENTS::Beam3ii::computeItildeprime

/*----------------------------------------------------------------------------------------------------------------------*
 |compute matrix v_I as outlined in the equations above (3.15) on page 152 of Jelenic 1999                   cyron 04/10|
 *----------------------------------------------------------------------------------------------------------------------*/
inline LINALG::Matrix<3,3> DRT::ELEMENTS::Beam3ii::vI(const LINALG::Matrix<3,1>& phiIJ)
{
  //matrix v_I
  LINALG::Matrix<3,3> result;
  
  computespin(result,phiIJ);
  if(phiIJ.Norm2() == 0)
    result.Scale(0.25);
  else
    result.Scale(tan(phiIJ.Norm2()/4.0)/phiIJ.Norm2());
  
  for(int i=0; i<3; i++)
    result(i,i) +=1;
  
  result.Scale(0.5);

  return result;
} // DRT::ELEMENTS::Beam3ii::vI

/*----------------------------------------------------------------------------------------------------------------------*
 |compute matrix v_J as outlined in the equations above (3.15) on page 152 of Jelenic 1999                   cyron 04/10|
 *----------------------------------------------------------------------------------------------------------------------*/
inline LINALG::Matrix<3,3> DRT::ELEMENTS::Beam3ii::vJ(const LINALG::Matrix<3,1>& phiIJ)
{
  //matrix v_J
  LINALG::Matrix<3,3> result;
  
  computespin(result,phiIJ);
  if(phiIJ.Norm2() == 0)
    result.Scale(-0.25);
  else
    result.Scale(-tan(phiIJ.Norm2()/4.0)/phiIJ.Norm2());
  
  for(int i=0; i<3; i++)
    result(i,i) +=1;
  
  result.Scale(0.5);

  return result;
} // DRT::ELEMENTS::Beam3ii::vJ


/*----------------------------------------------------------------------------------------------------------------------*
 |compute derivative r' (rprime) of line of centroids at Gauss point x with respect to Jelenic 1999 , eq. (2.12)        |
 |                                                                                                           cyron 04/10|
 *----------------------------------------------------------------------------------------------------------------------*/
template<int nnode, int dof>
inline void DRT::ELEMENTS::Beam3ii::curvederivative(const vector<double>& disp, const LINALG::Matrix<1,nnode> deriv, LINALG::Matrix<3,1>& rprime, const double& jacobi)
{
  //initialize rprime
  rprime.PutScalar(0);
  
  for (int i=0; i<dof; ++i)
    for (int node=0; node<nnode; ++node)
      rprime(i) += (Nodes()[node]->X()[i]+disp[6*node+i])*deriv(node);
  
  /*so far we have computed the derivative of the curve with respect to the element parameter \xi \in [-1;1];
   *as r' in (2.12) is the derivative with respect to the reference length, we have to divided it by the Jacobi
   *determinant at the respective point*/
  rprime.Scale(1.0/jacobi);

   return;
} // DRT::ELEMENTS::Beam3ii::curvederivative      

/*------------------------------------------------------------------------------------------------------------*
 | nonlinear stiffness and mass matrix (private)                                                   cyron 01/08|
 *-----------------------------------------------------------------------------------------------------------*/
template<int nnode>
void DRT::ELEMENTS::Beam3ii::b3_nlnstiffmass( ParameterList& params,
                                            vector<double>&           vel,
                                            vector<double>&           disp,
                                            Epetra_SerialDenseMatrix* stiffmatrix,
                                            Epetra_SerialDenseMatrix* massmatrix,
                                            Epetra_SerialDenseVector* force)
{
  //variables to store shape functions and its derivatives at a certain Gauss point
  LINALG::Matrix<1,nnode> funct;
  LINALG::Matrix<1,nnode> deriv;
  
  /*variables to store basis function matrices \tilde{I}^i(x) and \tilde{I}^{i'}(x) according to (3.18) and (3.19), Jelenic 1999
   *at some Gauss point for all nodes i*/
  vector<LINALG::Matrix<3,3> > Itilde(nnode);
  vector<LINALG::Matrix<3,3> > Itildeprime(nnode);
  
  //quaternion of relative rotation between node I and J according to (3.10), Jelenic 1999
  LINALG::Matrix<4,1> QIJ;
  //angle of relative rotation between node I and J according to (3.10), Jelenic 1999
  LINALG::Matrix<3,1> phiIJ;
  //quaternion of half relative rotation between node I and J according to (3.9), Jelenic 1999
  LINALG::Matrix<4,1> QIJhalf;
  //quaternion of reference triad \Lambda_r according to (3.9), Jelenic 1999
  LINALG::Matrix<4,1> Qr;
  //matrix of reference triad \Lambda_r according to (3.9), Jelenic 1999
  LINALG::Matrix<3,3> Lambdar; 
  //rotation quaternion between i-th nodal triadsand refenrece triad according to (3.8), Jelenic 1999
  LINALG::Matrix<4,1>  Qli;
  //rotation angles between nodal triads and refenrece triad according to (3.8), Jelenic 1999
  vector<LINALG::Matrix<3,1> > Psili(nnode);
  //interpolated local relative rotation \Psi^l at a certain Gauss point according to (3.11), Jelenic 1999
  LINALG::Matrix<3,1> Psil;
  /*derivative of interpolated local relative rotation \Psi^l at a certain Gauss point according to
   *(3.11), Jelenic 1999, but not with respect to reference length parameter, but with respect to space of integration
   *parameter. Hence, the difference between the variable used in this code and the one used in the 
   *equations of Jelenic 1999 is the Jacobi determinant*/
  LINALG::Matrix<3,1> Psilprime;
  //rotation quaternion between triad at Gauss point and reference triad Qr
  LINALG::Matrix<4,1>  Ql;
  //rotation quaternion at Gauss point 
  LINALG::Matrix<4,1>  Qgauss;
  //rotation matrix at Gauss point 
  LINALG::Matrix<3,3>  Lambda; 
  //r'(x) from (2.1), Jelenic 1999
  LINALG::Matrix<3,1>  rprime;
  //3D vector related to spin matrix \hat{\kappa} from (2.1), Jelenic 1999
  LINALG::Matrix<3,1>  kappa;
  //3D vector of convected axial and shear strains from (2.1), Jelenic 1999
  LINALG::Matrix<3,1>  gamma;
  //rotational displacement at a certain node between this and last iteration step
  LINALG::Matrix<3,1>  deltatheta;
  //rotational displacement at a certain node between this and last iteration step in quaternion form
  LINALG::Matrix<4,1>  deltaQ;
  //spin matrix related to vector rprime at some Gauss point
  LINALG::Matrix<3,3> rprimehat;

  //convected stresses N and M and constitutive matrices C_N and C_M according to section 2.4, Jelenic 1999
  LINALG::Matrix<3,1> stressN;
  LINALG::Matrix<3,1> stressM;
  LINALG::Matrix<3,3> CN;
  LINALG::Matrix<3,3> CM;
  
  //spatial stresses n and m according to (3.10), Romero 2004 and spatial constitutive matrices c_n and c_m according to page 148, Jelenic 1999
  LINALG::Matrix<3,1> stressn;
  LINALG::Matrix<3,1> stressm;
  LINALG::Matrix<3,3> cn;
  LINALG::Matrix<3,3> cm;



  /*first displacement vector is modified for proper element evaluation in case of periodic boundary conditions; in case that
   *no periodic boundary conditions are to be applied the following code line may be ignored or deleted*/
  NodeShift<nnode,3>(params,disp);
  
  
  //Compute current nodal triads
  for (int node=0; node<nnode; ++node)
  {   
    /*rotation increment relative to configuration in last iteration step is difference between current rotation
     *entry in displacement vector minus rotation entry in displacement vector in last iteration step*/
    for(int i=0; i<3; i++)
      dispthetanew_[node](i) = disp[6*node+3+i];

    deltatheta  = dispthetanew_[node];
    deltatheta -= dispthetaold_[node];
      
    //compute quaternion from rotation angle relative to last configuration
    angletoquaternion(deltatheta,deltaQ);
    
    //multiply relative rotation with rotation in last configuration to get rotation in new configuration
    quaternionproduct(Qold_[node],deltaQ,Qnew_[node]);  
    
    //renormalize quaternion to keep its absolute value one even in case of long simulations and intricate calculations
    Qnew_[node].Scale(1/Qnew_[node].Norm2());
  }

  

  //compute reference rotation \Lambda_r according to eq. (3.10) and (3.9), Jelenic 1999
  quaternionproduct(Qnew_[nodeJ_],inversequaternion(Qnew_[nodeI_]),QIJ);
  quaterniontoangle(QIJ,phiIJ);
  phiIJ.Scale(0.5);
  angletoquaternion(phiIJ,QIJhalf);
  phiIJ.Scale(2.0);
  quaternionproduct(QIJhalf,Qnew_[nodeI_],Qr);
  quaterniontotriad(Qr,Lambdar);
  
  //Compute relative rotations \Psi^l_i at all nodes i according to (3.8), Jelenic 1999
  for (int node=0; node<nnode; ++node)
  {   
    quaternionproduct(Qnew_[node],inversequaternion(Qr),Qli);  
    quaterniontoangle(Qli,Psili[node]);
  }

  //Get integrationpoints for Gauss-Legendre underintegration
  DRT::UTILS::IntegrationPoints1D gausspoints(MyGaussRule(nnode,gaussunderintegration));
  
  //Loop through all GP and calculate their contribution to the forcevector and stiffnessmatrix
  for(int numgp=0; numgp < gausspoints.nquad; numgp++)
  {
    //Get location and weight of GP in parameter space
    const double xi = gausspoints.qxg[numgp][0];
    const double wgt = gausspoints.qwgt[numgp];
    
    //evaluate shape functions and its derivatives at xi
    DRT::UTILS::shape_function_1D(funct,xi,this->Shape());
    DRT::UTILS::shape_function_1D_deriv1(deriv,xi,this->Shape());
    
    //compute local relative rotation \Psi^l and its derivative at current Gauss point according to (3.11), Jelenic 1999
    Psil.PutScalar(0);
    Psilprime.PutScalar(0);
    for (int node=0; node<nnode; ++node)
      for(int i=0; i<3; i++)
      {
        Psil(i)      += funct(node)*Psili[node](i);
        Psilprime(i) += deriv(node)*Psili[node](i);
      }
    

    //compute relative rotation between triad at Gauss point and reference triad Qr
    angletoquaternion(Psil,Ql);
    
    //compute rotation at Gauss point, i.e. the quaternion equivalent to \Lambda(s) in Crisfield 1999, eq. (4.7)
    quaternionproduct(Qr,Ql,Qgauss);
    
    //compute rotation matrix at Gauss point, i.e. \Lambda(s) in Crisfield 1999, eq. (4.7)
    quaterniontotriad(Qgauss,Lambda);
    
    
    //compute derivative of line of centroids with respect to curve parameter in reference configuration, i.e. r' from Jelenic 1999, eq. (2.12)
    curvederivative<nnode,3>(disp,deriv,rprime,jacobi_[numgp]);
    
    //compute spin matrix related to vector rprime for later use
    computespin(rprimehat,rprime);
    
    //compute convected strains gamma and kappa according to Jelenic 1999, eq. (2.12)
    computestrain(rprime,Lambda,gamma,kappa);

    //compute convected stress vector from strain vector according to Jelenic 1999, page 147, section 2.4
    strainstress(gamma,kappa,stressN,CN,stressM,CM);   
    
    /*compute spatial stresses and constitutive matrices from convected ones according to Jelenic 1999, page 148, paragraph
     *between (2.22) and (2.23) and Romero 2004, (3.10)*/
    pushforward(Lambda,stressN,CN,stressM,CM,stressn,cn,stressm,cm);
    

    /*computation of internal forces according to Jelenic 1999, eq. (4.3); computation split up with respect
     *to single blocks of matrix in eq. (4.3); note that Jacobi determinantn in diagonal blocks cancels out
     *in implementation, whereas for the lower left block we have to multiply the weight by the jacobi
     *determinant*/
    if (force != NULL)
    {
      for (int node=0; node<nnode; ++node)
      {
        /*upper left block (note: jacobi determinant cancels out as deriv is derivative with respect to
         *parameter in Gauss integration interval and I^{i'} in Jelenic 1999 is derivative with respect to
         *curve length in reference configuration*/
        for (int i=0; i<3; ++i)
          (*force)(6*node+i) += deriv(node)*stressn(i)*wgt;
        
        //lower left block  
        for (int i=0; i<3; ++i)
          for (int j=0; j<3; ++j)
            (*force)(6*node+3+i) -= rprimehat(i,j)*stressn(j)*funct(node)*wgt*jacobi_[numgp];
        
        /*lower right block (note: jacobi determinant cancels out as deriv is derivative with respect to
         *parameter in Gauss integration interval and I^{i'} in Jelenic 1999 is derivative with respect to
         *curve length in reference configuration*/
        for (int j=0; j<3; ++j)
          (*force)(6*node+3+j) += deriv(node)*stressm(j)*wgt;

        }
     }//if (force != NULL)
    

    

    

    //compute at this Gauss point basis functions \tilde{I}^i and \tilde{I}^{i'} in (3.19), page 152, Jelenic 1999, for all nodes
    computeItilde<nnode>(Psil,Psilprime,Itilde,phiIJ,Lambdar,Psili,funct);
    computeItildeprime<nnode>(Psil,Psilprime,Itildeprime,phiIJ,Lambdar,Psili,funct,deriv);

    /*computation of stiffness matrix according to Jelenic 1999, eq. (4.7); computation split up with respect
    *to single blocks of matrix in eq. (4.3)*/
    if (stiffmatrix != NULL)
    {     
      //auxiliary variables for storing intermediate matrices in computation of entries of stiffness matrix
      LINALG::Matrix<3,3> auxmatrix1;
      LINALG::Matrix<3,3> auxmatrix2;
      
      for(int nodei = 0; nodei < nnode; nodei++)
        for(int nodej = 0; nodej < nnode; nodej++)
        {
          //upper left block
          for (int i=0; i<3; ++i)
            for (int j=0; j<3; ++j)
              (*stiffmatrix)(6*nodei+i,6*nodej+j) += deriv(nodei)*deriv(nodej)*cn(i,j)*wgt/jacobi_[numgp];
          
          //upper right block
          auxmatrix2.Multiply(cn,rprimehat);
          computespin(auxmatrix1,stressn);
          auxmatrix2 -= auxmatrix1;
          auxmatrix2.Scale(deriv(nodei));
          auxmatrix1.Multiply(auxmatrix2,Itilde[nodej]);
          for (int i=0; i<3; ++i)
            for (int j=0; j<3; ++j)
              (*stiffmatrix)(6*nodei+i,6*nodej+3+j) += auxmatrix1(i,j)*wgt;

          //lower left block; note: error in eq. (4.7), Jelenic 1999: the first factor should be I^i instead of I^j
          auxmatrix2.Multiply(rprimehat,cn);
          computespin(auxmatrix1,stressn);
          auxmatrix1 -= auxmatrix2;
          auxmatrix1.Scale(funct(nodei)*deriv(nodej));
          for (int i=0; i<3; ++i)
            for (int j=0; j<3; ++j)
              (*stiffmatrix)(6*nodei+3+i,6*nodej+j) += auxmatrix1(i,j)*wgt;
          
          //lower right block 
          //first summand
          auxmatrix1.Multiply(cm,Itildeprime[nodej]);
          auxmatrix1.Scale(deriv(nodei));
          for (int i=0; i<3; ++i)
            for (int j=0; j<3; ++j)
              (*stiffmatrix)(6*nodei+3+i,6*nodej+3+j) += auxmatrix1(i,j)*wgt/jacobi_[numgp];
          
          //second summand
          computespin(auxmatrix2,stressm);
          auxmatrix1.Multiply(auxmatrix2,Itilde[nodej]);
          auxmatrix1.Scale(deriv(nodei));
          for (int i=0; i<3; ++i)
            for (int j=0; j<3; ++j)
              (*stiffmatrix)(6*nodei+3+i,6*nodej+3+j) -= auxmatrix1(i,j)*wgt;
          
          //third summand; note: error in eq. (4.7), Jelenic 1999: the first summand in the parantheses should be \hat{\Lambda N} instead of \Lambda N
          computespin(auxmatrix1,stressn);
          auxmatrix2.Multiply(cn,rprimehat);         
          auxmatrix1 -= auxmatrix2;
          auxmatrix2.Multiply(auxmatrix1,Itilde[nodej]);
          auxmatrix1.Multiply(rprimehat,auxmatrix2);
          auxmatrix1.Scale(funct(nodei));
          for (int i=0; i<3; ++i)
            for (int j=0; j<3; ++j)
              (*stiffmatrix)(6*nodei+3+i,6*nodej+3+j) += auxmatrix1(i,j)*jacobi_[numgp]*wgt;

        } 
    }

    if (massmatrix != NULL)
    {
      //pay attention: no mass matrix has been implemented so far for this elemetn (nor forces owing to inertia)
  
    }//if (massmatrix != NULL)
  
  }


  /*the following function call applied statistical forces and damping matrix according to the fluctuation dissipation theorem;
   * it is dedicated to the application of beam2 elements in the frame of statistical mechanics problems; for these problems a
   * special vector has to be passed to the element packed in the params parameter list; in case that the control routine calling
   * the element does not attach this special vector to params the following method is just doing nothing, which means that for
   * any ordinary problem of structural mechanics it may be ignored*/
   CalcBrownian<nnode,3,6,4>(params,vel,disp,stiffmatrix,force);

  return;

} // DRT::ELEMENTS::Beam3ii::b3_nlnstiffmass

/*------------------------------------------------------------------------------------------------------------*
 | lump mass matrix					   (private)                                                   cyron 01/08|
 *------------------------------------------------------------------------------------------------------------*/
template<int nnode>
void DRT::ELEMENTS::Beam3ii::lumpmass(Epetra_SerialDenseMatrix* emass)
{
  // lump mass matrix
  if (emass != NULL)
  {
	  // we assume #elemat2 is a square matrix
	  for (int c=0; c<(*emass).N(); ++c) // parse columns
	  {
		  double d = 0.0;
		  for (int r=0; r<(*emass).M(); ++r) // parse rows
		  {
			  d += (*emass)(r,c); // accumulate row entries
			  (*emass)(r,c) = 0.0;
		  }

		  (*emass)(c,c) = d; // apply sum of row entries on diagonal
	  }
  }
}

/*-----------------------------------------------------------------------------------------------------------*
 | Evaluate PTC damping (public)                                                                  cyron 10/08|
 *----------------------------------------------------------------------------------------------------------*/
template<int nnode>
void DRT::ELEMENTS::Beam3ii::EvaluatePTC(ParameterList& params,
                                      Epetra_SerialDenseMatrix& elemat1)
{
  /*comment on the calculation of a proper PTC parameter: it is not a priori clear which phyiscal parameters
   * should be incalculated when computing a proper PTC parameter; the reason for instability without PTC is
   * seemingly the vast difference in stiffness of different eigenmodes (namely those related to bending and
   * stretching, respectively). To understand the influence of different parameters to the numerical stability
   * we study a simple model problem: we consider a clamped beam of length L with horizontal and vertical tip
   * loads F_h and F_v, respectively. These tip loads go along horizontal and vertical displacements u and w of
   * the tip point, respectively. Let the beam be undeformed at the beginning of a time step and let the
   * displacements u and w arise at the end of a time step of lenght dt. Then simple calculus shows that with a
   * damping constat gamma ~ eta*L there holds true F_h = EIw/L^3 + gamma*w/dt and F_v = EAu/L + gamma*u/dt.
   * Stability is assumed to be preserved if the ratio between beding u and w is close to one for F_h = F_v.
   * Thus we expect stability if either EI/L^3 ~ EA/L or EI/L^3, EA/L << gamma/dt. In the first case the
   * elastic resistence against bending and streching is comparable, in the second case the problem is dominated
   * by viscous instead of elastic forces. In practice time step size is oriented to either to the bending or
   * stretching time constants tau of the system with dt ~ const1*tau and typically const1 ~ 1e-3. The bending time
   * constants are given by tau_EI = const2*eta*L^4 / EI and the stretching time constants by tau_EA = const3*eta*L^4 / EA,
   * with constant expressions const2, const3 < 1. If dt is chosen according to tau_EI we get gamma /dt ~ const1*const2*EI / L^3,
   * which is always much smaller than the stiffness expression EI/L^3 related to bending and if dt is chosen
   * according to tau_EA the same rationale applies. Therefore EI/L^3, EA/L << gamma/dt never arises in practice
   * and stability depends on the requirement EI/L^3 ~ EA/L. If this requirement is naturally violated an
   * artificial PTC damping has to be employed, which increases the damping stiffness that far that the ratio
   * EI/L^3 ~ EA/L can no longer destabilize the system.
   *
   * The crucial question is obviously how the PTC damping parameter scales with different simulation parameters.
   * In the following we discuss the effect of variations of different parameters:
   *
   * Young's modulus E: As both bending and axial stiffness scale linearly with the Young's modulus E  one may assume
   * that the PTC parameter may be calculated independently on this parameter; this was indeed found in practice:
   * varying E over 3 orders of magnitude upwards and downwards (and scaling time step size by the same factor
   * as all eigenfrequencies depend linearly on Young's modulus) did not affect the PTC parameter required for
   * stabilization.For too small values of E instability was found due to too large curvature in the beam elements,
   * however, this is expected as the beam formulation is valid for moderate curvature only and small values
   * of E naturally admit increasing curvature.
   *
   * Viscosity eta: a similar rationale as for Young's modulus E applies. All the system time constants depend
   * linearly on eta. On the other hand the critical ratio between bending and axial stiffness does not depend
   * on eta. Thus scaling eta and time step size dt by the same factor does not change the PTC factor required
   * for stabilization.
   *
   * Numerical tests revealed that refining the discretization by factor const and at the same time the time
   * step size by a factor const^2 (because the critical axial eigenfrequencies scale with L^2 for element
   * length L) did not change the required PTC parameter. One and the same parameter could be used for a wide
   * range of element lengths up to a scale where the element length became comparable with the persistnece
   * length l_p. For L >= l_p / 2 the simulation became unstable, however, this is supposed to happen not due
   * to an improper PTC parameter, but rather due to the large deformations arsing then, which violated the
   * small strain assumption of this Reissner element. Thus the PTC parameter depends rather on physical parameters
   * than on the choice of the discretization.
   *
   * The above parameter discussion reveals how to adapt the PTC factor in case of changes of the environment of
   * a structure with fixed cross section A, moment of inertia I and length L. However, how to choose the PTC
   * factor and time step size dt for a first discretization and parameter set up has not been discussed so far.
   * Indeed the latter step can be done heuristically once for
   *
   * Cross section A, moment of inertia I: from the above discussed physics one might assume a dependence of the
   * PTC parameter on the ratio of bending and strechting stiffness, i.e. on EI / EA. Such a dependence might
   * considerably exacerbate the application of the PTC algorithm. However, by means of numerical experiments a
   * different rule to deterime the PTC parameter was found: Beyond some ratio EI / EA simulations were found to
   * be unstable without PTC damping. However, a constant PTC damping factor was capable of stabilizing the system
   * over a large range of ratios EI / EA, if the time step size was adopted accordingly. The time step size
   * has to be determined both with respect to bending and stretching time constants. When scaling I by a factor
   * const_I and A by a factor const_A, one first has to decide which of both types of time constants may become
   * critical by the parameter change. Subsequently one has to scale the time step size either by 1/const_A if
   * the stretching time constants are the critical ones or by 1/const_I otherwise.
   *
   *
   * Length L: reduing
   */



  double basisdamp   = (20e-2)*PI*3; // in Actin3D_XXX input files with(!) stochastic torsional moments:: (20e-2)*PI for A = 1.9e-8, (20e-2)*PI*3 for A = 1.9e-6; for input of Thomas Knyrim without(!) stochastic torsional moments: (20e-2)*PI*20
  double anisofactor = 50; //10 for A = 1.9e-8 and A = 1.9e-6


  //Get the applied integrationpoints for underintegration
  DRT::UTILS::IntegrationPoints1D gausspointsptc(MyGaussRule(nnode,gaussunderintegration));
  //Get discretization typ
  const DRT::Element::DiscretizationType distype = Shape();
  //matrix to store Ansatzfunktionen
  LINALG::Matrix<1,nnode> funct;

  for (int gp=0; gp<gausspointsptc.nquad; gp++)
  {

    //Get location and weight of GP in parameter space
    const double xi = gausspointsptc.qxg[gp][0];
    const double wgt = gausspointsptc.qwgt[gp];

    DRT::UTILS::shape_function_1D(funct,xi,distype);

    //computing angle increment from current position in comparison with last converged position for damping
    LINALG::Matrix<4,1> deltaQ;
    quaternionproduct(inversequaternion(Qconv_[gp]),Qnew_[gp],deltaQ);
    LINALG::Matrix<3,1> deltatheta;
    quaterniontoangle(deltaQ,deltatheta);

    //computing special matrix for anisotropic damping
    LINALG::Matrix<3,3> Tconv;
    LINALG::Matrix<3,3> Theta;
    quaterniontotriad(Qconv_[gp],Tconv);
    for(int k=0; k<3; k++)
      for(int j = 0; j<3; j++)
        Theta(k,j) = Tconv(k,0)*Tconv(j,0);

    //isotropic artificial stiffness
    LINALG::Matrix<3,3> artstiff;
    artstiff = Tmatrix(deltatheta);
    artstiff.Scale(basisdamp);

    //anisotropic artificial stiffness
    LINALG::Matrix<3,3> auxstiff;
    auxstiff.Multiply(Theta,Tmatrix(deltatheta));
    auxstiff.Scale(anisofactor*basisdamp);
    artstiff += auxstiff;

    //scale artificial damping with dti parameter for PTC method
    artstiff.Scale( params.get<double>("dti",0.0) );

    for(int i=0; i<nnode; i++)
      for (int j=0; j<nnode; j++)
        for(int k=0; k<3; k++)
          for (int l=0; l<3; l++)
            elemat1(i*6+3+k,j*6+3+l) += artstiff(k,l)*funct(i)*funct(j)*wgt*jacobi_[gp];
  }

  return;
} //DRT::ELEMENTS::Beam3ii::EvaluatePTC

/*-----------------------------------------------------------------------------------------------------------*
 | computes damping coefficients per lengthand stores them in a matrix in the following order: damping of    |
 | translation parallel to filament axis, damping of translation orthogonal to filament axis, damping of     |
 | rotation around filament axis                                             (public)           cyron   10/09|
 *----------------------------------------------------------------------------------------------------------*/
inline void DRT::ELEMENTS::Beam3ii::MyDampingConstants(ParameterList& params,LINALG::Matrix<3,1>& gamma, const INPAR::STATMECH::FrictionModel& frictionmodel)
{
  //translational damping coefficients according to Howard, p. 107, table 6.2;
  gamma(0) = 2*PI*params.get<double>("ETA",0.0);
  gamma(1) = 4*PI*params.get<double>("ETA",0.0);

  /*damping coefficient of rigid straight rod spinning around its own axis according to Howard, p. 107, table 6.2;
   *as this coefficient is very small for thin rods it is increased artificially by a factor for numerical convencience*/
  double rsquare = pow((4*Iyy_/PI),0.5);
  double artificial = 1920;//1920;  //1920 not bad for standard Actin3D_10.dat files; for 40 elements also 1 seems to work really well
  gamma(2) = 4*PI*params.get<double>("ETA",0.0)*rsquare*artificial;


  //in case of an isotropic friction model the same damping coefficients are applied parallel to the polymer axis as perpendicular to it
  if(frictionmodel == INPAR::STATMECH::frictionmodel_isotropicconsistent || frictionmodel == INPAR::STATMECH::frictionmodel_isotropiclumped)
    gamma(0) = gamma(1);


   /* in the following section damping coefficients are replaced by those suggested in Ortega2003, which allows for a
    * comparison of the finite element simulation with the results of that article; note that we assume that the element
    * length is equivalent to the particle length in the following when computing the length to diameter ratio p*/
   /*
   double lrefe=0;
   for (int gp=0; gp<nnode-1; gp++)
     lrefe += gausspointsdamping.qwgt[gp]*jacobi_[gp];

   double p=lrefe/(pow(crosssec_*4.0/PI,0.5));
   double Ct=0.312+0.565/p-0.100/pow(p,2.0);
   double Cr=-0.662+0.917/p-0.05/pow(p,2.0);
   gamma(0) = 2.0*PI*params.get<double>("ETA",0.0)/(log(p) + 2*Ct - Cr);
   gamma(1) = 4.0*PI*params.get<double>("ETA",0.0)/(log(p)+Cr);
   gamma(3) = 4.0*PI*params.get<double>("ETA",0.0)*rsquare*artificial*(0.96 + 0.64992/p - 0.17568/p^2);
   */
}

/*-----------------------------------------------------------------------------------------------------------*
 |computes the number of different random numbers required in each time step for generation of stochastic    |
 |forces;                                                                    (public)           cyron   10/09|
 *----------------------------------------------------------------------------------------------------------*/
int DRT::ELEMENTS::Beam3ii::HowManyRandomNumbersINeed()
{
  /*at each Gauss point one needs as many random numbers as randomly excited degrees of freedom, i.e. three
   *random numbers for the translational degrees of freedom and one random number for the rotation around the element axis*/
  return (4*NumNode());

}

/*-----------------------------------------------------------------------------------------------------------*
 |computes velocity of background fluid and gradient of that velocity at a certain evaluation point in       |
 |the physical space                                                         (public)           cyron   10/09|
 *----------------------------------------------------------------------------------------------------------*/
template<int ndim> //number of dimensions of embedding space
void DRT::ELEMENTS::Beam3ii::MyBackgroundVelocity(ParameterList& params,  //!<parameter list
                                                const LINALG::Matrix<ndim,1>& evaluationpoint,  //!<point at which background velocity and its gradient has to be computed
                                                LINALG::Matrix<ndim,1>& velbackground,  //!< velocity of background fluid
                                                LINALG::Matrix<ndim,ndim>& velbackgroundgrad) //!<gradient of velocity of background fluid
{

  /*note: this function is not yet a general one, but always assumes a shear flow, where the velocity of the
   * background fluid is always directed in direction params.get<int>("OSCILLDIR",0) and orthogonal to z-axis.
   * In 3D the velocity increases linearly in z and equals zero for z = 0.
   * In 2D the velocity increases linearly in y and equals zero for y = 0. */

  //velocity at upper boundary of domain
  double uppervel = 0.0;
  
  //default values for background velocity and its gradient
  velbackground.PutScalar(0);
  velbackgroundgrad.PutScalar(0);

  //oscillations start only at params.get<double>("STARTTIME",0.0)
  if(params.get<double>("total time",0.0) > params.get<double>("STARTTIME",0.0) && params.get<int>("CURVENUMBER",-1) >=  1 && params.get<int>("OSCILLDIR",-1) >= 0 )
  {
    uppervel = (params.get<double>("SHEARAMPLITUDE",0.0)) * (DRT::Problem::Instance()->Curve(params.get<int>("CURVENUMBER",-1)-1).FctDer(params.get<double>("total time",0.0),1))[1];
  
    //compute background velocity
    velbackground(params.get<int>("OSCILLDIR",-1)) = (evaluationpoint(ndim-1) / params.get<double>("PeriodLength",0.0)) * uppervel;
  
    //compute gradient of background velocity
    velbackgroundgrad(params.get<int>("OSCILLDIR",-1),ndim-1) = uppervel / params.get<double>("PeriodLength",0.0);
  }

}
/*-----------------------------------------------------------------------------------------------------------*
 | computes rotational damping forces and stiffness (public)                                    cyron   10/09|
 *----------------------------------------------------------------------------------------------------------*/
template<int nnode> //number of nodes
inline void DRT::ELEMENTS::Beam3ii::MyRotationalDamping(ParameterList& params,  //!<parameter list
                                              const vector<double>&     vel,  //!< element velocity vector
                                              const vector<double>&     disp, //!<element disp vector
                                              Epetra_SerialDenseMatrix* stiffmatrix,  //!< element stiffness matrix
                                              Epetra_SerialDenseVector* force)//!< element internal force vector
{
  //get time step size
  double dt = params.get<double>("delta time",0.0);

  //integration points for underintegration
  DRT::UTILS::IntegrationPoints1D gausspoints(MyGaussRule(nnode,gaussunderintegration));

  //get friction model according to which forces and damping are applied
  INPAR::STATMECH::FrictionModel frictionmodel = Teuchos::get<INPAR::STATMECH::FrictionModel>(params,"FRICTION_MODEL");

  //damping coefficients for translational and rotatinal degrees of freedom
  LINALG::Matrix<3,1> gamma(true);
  MyDampingConstants(params,gamma,frictionmodel);


  //matrix to store basis functions evaluated at a certain Gauss point
  LINALG::Matrix<1,nnode> funct;

  for (int gp=0; gp<gausspoints.nquad; gp++)//loop through Gauss points
  {
    //get evaluated basis functions at current Gauss point
    DRT::UTILS::shape_function_1D(funct,gausspoints.qxg[gp][0],Shape());

    //rotation between last converged position and current position expressend as a quaternion
    LINALG::Matrix<4,1>  deltaQ;
    quaternionproduct(inversequaternion(Qconv_[gp]),Qnew_[gp],deltaQ);

    //rotation between last converged position and current position expressed as a three element rotation vector
    LINALG::Matrix<3,1> deltatheta;
    quaterniontoangle(deltaQ,deltatheta);

    //angular velocity at this Gauss point
    LINALG::Matrix<3,1> omega(true);
    omega += deltatheta;
    omega.Scale(1/dt);

    //compute matrix T*W*T^t
    LINALG::Matrix<3,3> Tnew;
    LINALG::Matrix<3,3> TWTt;
    quaterniontotriad(Qnew_[gp],Tnew);
    for(int k=0; k<3; k++)
      for(int j = 0; j<3; j++)
        TWTt(k,j) = Tnew(k,0)*Tnew(j,0);

    //compute vector T*W*T^t*\omega
    LINALG::Matrix<3,1> TWTtomega;
    TWTtomega.Multiply(TWTt,omega);

    //compute matrix T*W*T^t*H^(-1)
    LINALG::Matrix<3,3> TWTtHinv;
    TWTtHinv.Multiply(TWTt,Tmatrix(deltatheta));

    //compute spin matrix S(\omega)
    LINALG::Matrix<3,3> Sofomega;
    computespin(Sofomega,omega);

    //compute matrix T*W*T^t*S(\omega)
    LINALG::Matrix<3,3> TWTtSofomega;
    TWTtSofomega.Multiply(TWTt,Sofomega);

    //compute spin matrix S(T*W*T^t*\omega)
    LINALG::Matrix<3,3> SofTWTtomega;
    computespin(SofTWTtomega,TWTtomega);

    //loop over all line nodes
    for(int i=0; i<nnode; i++)
      //loop over three dimensions in line direction
      for(int k=0; k<3; k++)
      {
        if(force != NULL)
          (*force)(i*6+3+k) += gamma(2)*TWTtomega(k)*funct(i)*gausspoints.qwgt[gp]*jacobi_[gp];

        if(stiffmatrix != NULL)
          //loop over all column nodes
          for (int j=0; j<nnode; j++)
            //loop over three dimensions in column direction
            for (int l=0; l<3; l++)
              (*stiffmatrix)(i*6+3+k,j*6+3+l) += gamma(2)*( TWTtHinv(k,l) / dt + TWTtSofomega(k,l) - SofTWTtomega(k,l) )*funct(i)*funct(j)*gausspoints.qwgt[gp]*jacobi_[gp];
      }
  }


  return;
}//DRT::ELEMENTS::Beam3ii::MyRotationalDamping(.)

/*-----------------------------------------------------------------------------------------------------------*
 | computes translational damping forces and stiffness (public)                                 cyron   10/09|
 *----------------------------------------------------------------------------------------------------------*/
template<int nnode, int ndim, int dof> //number of nodes, number of dimensions of embedding space, number of degrees of freedom per node
inline void DRT::ELEMENTS::Beam3ii::MyTranslationalDamping(ParameterList& params,  //!<parameter list
                                                  const vector<double>&     vel,  //!< element velocity vector
                                                  const vector<double>&     disp, //!<element disp vector
                                                  Epetra_SerialDenseMatrix* stiffmatrix,  //!< element stiffness matrix
                                                  Epetra_SerialDenseVector* force)//!< element internal force vector
{
  //get time step size
  double dt = params.get<double>("delta time",0.0);

  //velocity and gradient of background velocity field
  LINALG::Matrix<ndim,1> velbackground;
  LINALG::Matrix<ndim,ndim> velbackgroundgrad;

  //evaluation point in physical space corresponding to a certain Gauss point in parameter space
  LINALG::Matrix<ndim,1> evaluationpoint;

  //get friction model according to which forces and damping are applied
  INPAR::STATMECH::FrictionModel frictionmodel = Teuchos::get<INPAR::STATMECH::FrictionModel>(params,"FRICTION_MODEL");

  //damping coefficients for translational and rotatinal degrees of freedom
  LINALG::Matrix<3,1> gamma(true);
  MyDampingConstants(params,gamma,frictionmodel);

  //get vector jacobi with Jacobi determinants at each integration point (gets by default those values required for consistent damping matrix)
  vector<double> jacobi(jacobimass_);

  //determine type of numerical integration performed (lumped damping matrix via lobatto integration!)
  IntegrationType integrationtype = gaussexactintegration;
  if(frictionmodel == INPAR::STATMECH::frictionmodel_isotropiclumped)
  {
    integrationtype = lobattointegration;
    jacobi = jacobinode_;
  }

  //get Gauss points and weights for evaluation of damping matrix
  DRT::UTILS::IntegrationPoints1D gausspoints(MyGaussRule(nnode,integrationtype));

  //matrix to store basis functions and their derivatives evaluated at a certain Gauss point
  LINALG::Matrix<1,nnode> funct;
  LINALG::Matrix<1,nnode> deriv;

  for(int gp=0; gp < gausspoints.nquad; gp++)
  {
    //evaluate basis functions and their derivatives at current Gauss point
    DRT::UTILS::shape_function_1D(funct,gausspoints.qxg[gp][0],Shape());
    DRT::UTILS::shape_function_1D_deriv1(deriv,gausspoints.qxg[gp][0],Shape());

    //compute point in phyiscal space corresponding to Gauss point
    evaluationpoint.PutScalar(0);
    //loop over all line nodes
    for(int i=0; i<nnode; i++)
      //loop over all dimensions
      for(int j=0; j<ndim; j++)
        evaluationpoint(j) += funct(i)*(Nodes()[i]->X()[j]+disp[dof*i+j]);

    //compute velocity and gradient of background flow field at evaluationpoint
    MyBackgroundVelocity<ndim>(params,evaluationpoint,velbackground,velbackgroundgrad);


    //compute tangent vector t_{\par} at current Gauss point
    LINALG::Matrix<ndim,1> tpar(true);
    for(int i=0; i<nnode; i++)
      for(int k=0; k<ndim; k++)
        tpar(k) += deriv(i)*(Nodes()[i]->X()[k]+disp[dof*i+k]) / jacobi[gp];

    //compute velocity vector at this Gauss point
    LINALG::Matrix<ndim,1> velgp(true);
    for(int i=0; i<nnode; i++)
      for(int l=0; l<ndim; l++)
        velgp(l) += funct(i)*vel[dof*i+l];

    //compute matrix product (t_{\par} \otimes t_{\par}) \cdot velbackgroundgrad
    LINALG::Matrix<ndim,ndim> tpartparvelbackgroundgrad(true);
    for(int i=0; i<ndim; i++)
      for(int j=0; j<ndim; j++)
        for(int k=0; k<ndim; k++)
          tpartparvelbackgroundgrad(i,j) += tpar(i)*tpar(k)*velbackgroundgrad(k,j);

    //loop over all line nodes
    for(int i=0; i<nnode; i++)
      //loop over lines of matrix t_{\par} \otimes t_{\par}
      for(int k=0; k<ndim; k++)
        //loop over columns of matrix t_{\par} \otimes t_{\par}
        for(int l=0; l<ndim; l++)
        {
          if(force != NULL)
            (*force)(i*dof+k)+= funct(i)*jacobi[gp]*gausspoints.qwgt[gp]*( (k==l)*gamma(1) + (gamma(0) - gamma(1))*tpar(k)*tpar(l) ) *(velgp(l)- velbackground(l));

          if(stiffmatrix != NULL)
            //loop over all column nodes
            for (int j=0; j<nnode; j++)
            {
              (*stiffmatrix)(i*dof+k,j*dof+l) += gausspoints.qwgt[gp]*funct(i)*funct(j)*jacobi[gp]*(                 (k==l)*gamma(1) + (gamma(0) - gamma(1))*tpar(k)*tpar(l) ) / dt;
              (*stiffmatrix)(i*dof+k,j*dof+l) -= gausspoints.qwgt[gp]*funct(i)*funct(j)*jacobi[gp]*( velbackgroundgrad(k,l)*gamma(1) + (gamma(0) - gamma(1))*tpartparvelbackgroundgrad(k,l) ) ;
              (*stiffmatrix)(i*dof+k,j*dof+k) += gausspoints.qwgt[gp]*funct(i)*deriv(j)*                                                   (gamma(0) - gamma(1))*tpar(l)*(velgp(l) - velbackground(l));
              (*stiffmatrix)(i*dof+k,j*dof+l) += gausspoints.qwgt[gp]*funct(i)*deriv(j)*                                                   (gamma(0) - gamma(1))*tpar(k)*(velgp(l) - velbackground(l));
            }
        }
  }

  return;
}//DRT::ELEMENTS::Beam3ii::MyTranslationalDamping(.)

/*-----------------------------------------------------------------------------------------------------------*
 | computes stochastic forces and resulting stiffness (public)                                  cyron   10/09|
 *----------------------------------------------------------------------------------------------------------*/
template<int nnode, int ndim, int dof, int randompergauss> //number of nodes, number of dimensions of embedding space, number of degrees of freedom per node, number of random numbers required per Gauss point
inline void DRT::ELEMENTS::Beam3ii::MyStochasticForces(ParameterList& params,  //!<parameter list
                                              const vector<double>&     vel,  //!< element velocity vector
                                              const vector<double>&     disp, //!<element disp vector
                                              Epetra_SerialDenseMatrix* stiffmatrix,  //!< element stiffness matrix
                                              Epetra_SerialDenseVector* force)//!< element internal force vector
{
  //get friction model according to which forces and damping are applied
  INPAR::STATMECH::FrictionModel frictionmodel = Teuchos::get<INPAR::STATMECH::FrictionModel>(params,"FRICTION_MODEL");

  //damping coefficients for three translational and one rotatinal degree of freedom
  LINALG::Matrix<3,1> gamma(true);
  MyDampingConstants(params,gamma,frictionmodel);


  //get vector jacobi with Jacobi determinants at each integration point (gets by default those values required for consistent damping matrix)
  vector<double> jacobi(jacobimass_);

  //determine type of numerical integration performed (lumped damping matrix via lobatto integration!)
  IntegrationType integrationtype = gaussexactintegration;
  if(frictionmodel == INPAR::STATMECH::frictionmodel_isotropiclumped)
  {
    integrationtype = lobattointegration;
    jacobi = jacobinode_;
  }

  //get Gauss points and weights for evaluation of damping matrix
  DRT::UTILS::IntegrationPoints1D gausspoints(MyGaussRule(nnode,integrationtype));

  //matrix to store basis functions and their derivatives evaluated at a certain Gauss point
  LINALG::Matrix<1,nnode> funct;
  LINALG::Matrix<1,nnode> deriv;


  /*get pointer at Epetra multivector in parameter list linking to random numbers for stochastic forces with zero mean
   * and standard deviation (2*kT / dt)^0.5; note carefully: a space between the two subsequal ">" signs is mandatory
   * for the C++ parser in order to avoid confusion with ">>" for streams*/
   RCP<Epetra_MultiVector> randomnumbers = params.get<  RCP<Epetra_MultiVector> >("RandomNumbers",Teuchos::null);



  for(int gp=0; gp < gausspoints.nquad; gp++)
  {
    //evaluate basis functions and their derivatives at current Gauss point
    DRT::UTILS::shape_function_1D(funct,gausspoints.qxg[gp][0],Shape());
    DRT::UTILS::shape_function_1D_deriv1(deriv,gausspoints.qxg[gp][0],Shape());

    //compute tangent vector t_{\par} at current Gauss point
    LINALG::Matrix<ndim,1> tpar(true);
    for(int i=0; i<nnode; i++)
      for(int k=0; k<ndim; k++)
        tpar(k) += deriv(i)*(Nodes()[i]->X()[k]+disp[dof*i+k]) / jacobi[gp];


    //loop over all line nodes
    for(int i=0; i<nnode; i++)
      //loop dimensions with respect to lines
      for(int k=0; k<ndim; k++)
        //loop dimensions with respect to columns
        for(int l=0; l<ndim; l++)
        {
          if(force != NULL)
            (*force)(i*dof+k) -= funct(i)*(sqrt(gamma(1))*(k==l) + (sqrt(gamma(0)) - sqrt(gamma(1)))*tpar(k)*tpar(l))*(*randomnumbers)[gp*randompergauss+l][LID()]*sqrt(jacobi[gp]*gausspoints.qwgt[gp]);

          if(stiffmatrix != NULL)
            //loop over all column nodes
            for (int j=0; j<nnode; j++)
            {
              (*stiffmatrix)(i*dof+k,j*dof+k) -= funct(i)*deriv(j)*tpar(l)*(*randomnumbers)[gp*randompergauss+l][LID()]*sqrt(gausspoints.qwgt[gp]/ jacobi[gp])*(sqrt(gamma(0)) - sqrt(gamma(1)));
              (*stiffmatrix)(i*dof+k,j*dof+l) -= funct(i)*deriv(j)*tpar(k)*(*randomnumbers)[gp*randompergauss+l][LID()]*sqrt(gausspoints.qwgt[gp]/ jacobi[gp])*(sqrt(gamma(0)) - sqrt(gamma(1)));
            }
        }
  }



  return;
}//DRT::ELEMENTS::Beam3ii::MyStochasticForces(.)

/*-----------------------------------------------------------------------------------------------------------*
 | computes stochastic moments and (if required) resulting stiffness (public)                   cyron   10/09|
 *----------------------------------------------------------------------------------------------------------*/
template<int nnode, int randompergauss> //number of nodes, number of random numbers required per Gauss point, number of random numbers required per Gauss point
inline void DRT::ELEMENTS::Beam3ii::MyStochasticMoments(ParameterList& params,  //!<parameter list
                                              const vector<double>&     vel,  //!< element velocity vector
                                              const vector<double>&     disp, //!<element disp vector
                                              Epetra_SerialDenseMatrix* stiffmatrix,  //!< element stiffness matrix
                                              Epetra_SerialDenseVector* force)//!< element internal force vector
{

  //get friction model according to which forces and damping are applied
  INPAR::STATMECH::FrictionModel frictionmodel = Teuchos::get<INPAR::STATMECH::FrictionModel>(params,"FRICTION_MODEL");

  //damping coefficients for three translational and one rotatinal degree of freedom
  LINALG::Matrix<3,1> gamma(true);
  MyDampingConstants(params,gamma,frictionmodel);

  //determine type of numerical integration performed (note: underintegration applied as for related points triads already known from elasticity)
  IntegrationType integrationtype = gaussunderintegration;

  //get Gauss points and weights for evaluation of damping matrix
  DRT::UTILS::IntegrationPoints1D gausspoints(MyGaussRule(nnode,integrationtype));

  //matrix to store basis functions and their derivatives evaluated at a certain Gauss point
  LINALG::Matrix<1,nnode> funct;

  /*get pointer at Epetra multivector in parameter list linking to random numbers for stochastic forces with zero mean
   * and standard deviation (2*kT / dt)^0.5; note carefully: a space between the two subsequal ">" signs is mandatory
   * for the C++ parser in order to avoid confusion with ">>" for streams*/
   RCP<Epetra_MultiVector> randomnumbers = params.get<  RCP<Epetra_MultiVector> >("RandomNumbers",Teuchos::null);

  for(int gp=0; gp < gausspoints.nquad; gp++)
  {
    //evaluate basis functions and their derivatives at current Gauss point
    DRT::UTILS::shape_function_1D(funct,gausspoints.qxg[gp][0],Shape());

    //get current triad at this Gauss point:
    LINALG::Matrix<3,3> Tnew;
    quaterniontotriad(Qnew_[gp],Tnew);

    //get first column out of Tnew
    LINALG::Matrix<3,1> t1;
    for(int i=0; i<3; i++)
      t1(i) = Tnew(i,0);

    //compute spin matrix from first column of Tnew times random number
    LINALG::Matrix<3,3> S;
    computespin(S,t1);
    S.Scale((*randomnumbers)[gp*randompergauss+3][LID()]);


    //loop over all line nodes
    for(int i=0; i<nnode; i++)
      //loop over lines of matrix t_{\par} \otimes t_{\par}
      for(int k=0; k<3; k++)
      {
        if(force != NULL)
          (*force)(i*6+3+k) -= funct(i)*t1(k)*(*randomnumbers)[gp*randompergauss+3][LID()]*sqrt(jacobi_[gp]*gausspoints.qwgt[gp]*gamma(2));

        if(stiffmatrix != NULL)
          //loop over all column nodes
          for (int j=0; j<nnode; j++)
            //loop over three dimensions with respect to columns
            for(int l=0; l<3; l++)
              (*stiffmatrix)(i*6+3+k,j*6+3+l) += funct(i)*funct(j)*S(k,l)*sqrt(jacobi_[gp]*gausspoints.qwgt[gp]*gamma(2));

    }
  }
  return;
}//DRT::ELEMENTS::Beam3ii::MyStochasticMoments(.)

/*-----------------------------------------------------------------------------------------------------------*
 | Assemble stochastic and viscous forces and respective stiffness according to fluctuation dissipation      |
 | theorem                                                                               (public) cyron 10/09|
 *----------------------------------------------------------------------------------------------------------*/
template<int nnode, int ndim, int dof, int randompergauss> //number of nodes, number of dimensions of embedding space, number of degrees of freedom per node, number of random numbers required per Gauss point
inline void DRT::ELEMENTS::Beam3ii::CalcBrownian(ParameterList& params,
                                              const vector<double>&           vel,  //!< element velocity vector
                                              const vector<double>&           disp, //!< element displacement vector
                                              Epetra_SerialDenseMatrix* stiffmatrix,  //!< element stiffness matrix
                                              Epetra_SerialDenseVector* force) //!< element internal force vector
{
  //if no random numbers for generation of stochastic forces are passed to the element no Brownian dynamics calculations are conducted
  if( params.get<  RCP<Epetra_MultiVector> >("RandomNumbers",Teuchos::null) == Teuchos::null)
    return;

  //add stiffness and forces due to translational damping effects
  MyTranslationalDamping<nnode,ndim,dof>(params,vel,disp,stiffmatrix,force);

  //add stiffness and forces (i.e. moments) due to rotational damping effects
  MyRotationalDamping<nnode>(params,vel,disp,stiffmatrix,force);

  //add stochastic forces and (if required) resulting stiffness
  MyStochasticForces<nnode,ndim,dof,randompergauss>(params,vel,disp,stiffmatrix,force);

  //add stochastic moments and resulting stiffness
  //MyStochasticMoments<nnode,randompergauss>(params,vel,disp,stiffmatrix,force);


return;

}//DRT::ELEMENTS::Beam3ii::CalcBrownian(.)


/*-----------------------------------------------------------------------------------------------------------*
 | shifts nodes so that proper evaluation is possible even in case of periodic boundary conditions; if two   |
 | nodes within one element are separated by a periodic boundary, one of them is shifted such that the final |
 | distance in R^3 is the same as the initial distance in the periodic space; the shift affects computation  |
 | on element level within that very iteration step, only (no change in global variables performed)          |                                 |
 |                                                                                       (public) cyron 10/09|
 *----------------------------------------------------------------------------------------------------------*/
template<int nnode, int ndim> //number of nodes, number of dimensions
inline void DRT::ELEMENTS::Beam3ii::NodeShift(ParameterList& params,  //!<parameter list
                                            vector<double>& disp) //!<element disp vector
{
  /*get number of degrees of freedom per node; note: the following function assumes the same number of degrees
   *of freedom for each element node*/
  int numdof = NumDofPerNode(*(Nodes()[0]));

  /*only if periodic boundary conditions are in use, i.e. params.get<double>("PeriodLength",0.0) > 0.0, this
   * method has to change the displacement variables*/
  if(params.get<double>("PeriodLength",0.0) > 0.0)
    //loop through all nodes except for the first node which remains fixed as reference node
    for(int i=1;i<nnode;i++)
    {
      for(int dof= ndim - 1; dof > -1; dof--)
      {
        /*if the distance in some coordinate direction between some node and the first node becomes smaller by adding or subtracting
         * the period length, the respective node has obviously been shifted due to periodic boundary conditions and should be shifted
         * back for evaluation of element matrices and vectors; this way of detecting shifted nodes works as long as the element length
         * is smaller than half the periodic length*/
        if( fabs( (Nodes()[i]->X()[dof]+disp[numdof*i+dof]) + params.get<double>("PeriodLength",0.0) - (Nodes()[0]->X()[dof]+disp[numdof*0+dof]) ) < fabs( (Nodes()[i]->X()[dof]+disp[numdof*i+dof]) - (Nodes()[0]->X()[dof]+disp[numdof*0+dof]) ) )
        {
          disp[numdof*i+dof] += params.get<double>("PeriodLength",0.0);

          /*the upper domain surface orthogonal to the z-direction may be subject to shear Dirichlet boundary condition; the lower surface
           *may be fixed by DBC. To avoid problmes when nodes exit the domain through the upper z-surface and reenter through the lower
           *z-surface, the shear has to be substracted from nodal coordinates in that case */
          if(dof == 2 && params.get<int>("CURVENUMBER",-1) >=  1)
            disp[numdof*i+params.get<int>("OSCILLDIR",-1)] += params.get<double>("SHEARAMPLITUDE",0.0)*DRT::Problem::Instance()->Curve(params.get<int>("CURVENUMBER",-1)-1).f(params.get<double>("total time",0.0));
        }

        if( fabs( (Nodes()[i]->X()[dof]+disp[numdof*i+dof]) - params.get<double>("PeriodLength",0.0) - (Nodes()[0]->X()[dof]+disp[numdof*0+dof]) ) < fabs( (Nodes()[i]->X()[dof]+disp[numdof*i+dof]) - (Nodes()[0]->X()[dof]+disp[numdof*0+dof]) ) )
        {
          disp[numdof*i+dof] -= params.get<double>("PeriodLength",0.0);

          /*the upper domain surface orthogonal to the z-direction may be subject to shear Dirichlet boundary condition; the lower surface
           *may be fixed by DBC. To avoid problmes when nodes exit the domain through the lower z-surface and reenter through the upper
           *z-surface, the shear has to be added to nodal coordinates in that case */
          if(dof == 2 && params.get<int>("CURVENUMBER",-1) >=  1)
            disp[numdof*i+params.get<int>("OSCILLDIR",-1)] -= params.get<double>("SHEARAMPLITUDE",0.0)*DRT::Problem::Instance()->Curve(params.get<int>("CURVENUMBER",-1)-1).f(params.get<double>("total time",0.0));
        }
      }
    }

return;

}//DRT::ELEMENTS::Beam3ii::NodeShift



#endif  // #ifdef CCADISCRET
#endif  // #ifdef D_BEAM3

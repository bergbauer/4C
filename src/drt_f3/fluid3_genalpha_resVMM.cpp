/*----------------------------------------------------------------------*/
/*!
\file fluid3_genalpha_resVMM.cpp

\brief Internal implementation of Fluid3 element with a generalised alpha
       time integration.

<pre>
Maintainer: Peter Gamnitzer
            gamnitzer@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15235
</pre>
*/
/*----------------------------------------------------------------------*/
#ifdef D_FLUID3
#ifdef CCADISCRET

#include "fluid3_genalpha_resVMM.H"
#include "../drt_mat/newtonianfluid.H"
#include "../drt_lib/drt_timecurve.H"

#include <Epetra_SerialDenseSolver.h>
#include <Epetra_LAPACK.h>


/*----------------------------------------------------------------------*
  |  constructor allocating arrays whose sizes may depend on the number |
  | of nodes of the element                                             |
  |                            (public)                      gammi 06/07|
  *----------------------------------------------------------------------*/
DRT::Elements::Fluid3GenalphaResVMM::Fluid3GenalphaResVMM(int iel)
  : iel_        (iel),
// nodal data
//-----------------------+------------+------------------------------------
//                  dim  | derivative | node
    xyze_         (  3   ,              iel_,blitz::ColumnMajorArray<2>()),   
    edeadaf_      (  3   ,              iel_,blitz::ColumnMajorArray<2>()),
//-----------------------+------------+------------------------------------
// gausspoint data
//------------------------------------------------------------------------
//                  dim  | derivative | node
//-----------------------+------------+------------------------------------
    funct_        (                     iel_                             ),
    deriv_        (            3      , iel_,blitz::ColumnMajorArray<2>()),
    deriv2_       (            6      , iel_,blitz::ColumnMajorArray<2>()),
    derxy_        (            3      , iel_,blitz::ColumnMajorArray<2>()),
    derxy2_       (            6      , iel_,blitz::ColumnMajorArray<2>()),
    viscs2_       (  3   ,     3      , iel_,blitz::ColumnMajorArray<3>()),
    xjm_          (  3   ,     3            ,blitz::ColumnMajorArray<2>()),
    xji_          (  3   ,     3            ,blitz::ColumnMajorArray<2>()),
    xder2_        (  6   ,     3            ,blitz::ColumnMajorArray<2>()),
    accintam_     (  3                                                   ),
    velintnp_     (  3                                                   ),
    velintaf_     (  3                                                   ),
    pderxynp_     (  3                                                   ),
    vderxynp_     (  3   ,     3            ,blitz::ColumnMajorArray<2>()),
    vderxyaf_     (  3   ,     3            ,blitz::ColumnMajorArray<2>()),
    vderxy2af_    (  3   ,     6            ,blitz::ColumnMajorArray<2>()),
    bodyforceaf_  (  3                                                   ),
    conv_c_af_    (                     iel_                             ),
    conv_r_af_    (  3   ,     3      , iel_,blitz::ColumnMajorArray<3>()),
//----------------------+------------+------------------------------------
// element data
//------------------------------------------------------------------------
    tau_          (3),
#if 0    
    invtauM_lin_  (3),
#endif
    svelaf_       (3),
    convaf_old_   (3),
    convsubaf_old_(3),
    viscaf_old_   (3),
    resM_         (3),
    conv_resM_    (                      iel_),
    conv_subaf_   (                      iel_)
{
}


/*----------------------------------------------------------------------*
  |  calculate system matrix for a generalised alpha time integration   |
  |                            (public)                      gammi 06/07|
  *----------------------------------------------------------------------*/
void DRT::Elements::Fluid3GenalphaResVMM::Sysmat(
  Fluid3*                                               ele,
  Epetra_SerialDenseMatrix&                             elemat,
  Epetra_SerialDenseVector&                             elevec,
  const blitz::Array<double,2>&                         evelnp,
  const blitz::Array<double,1>&                         eprenp,
  const blitz::Array<double,2>&                         eaccam,
  const blitz::Array<double,2>&                         evelaf,
  const struct _MATERIAL*                               material,
  const double                                          alphaM,
  const double                                          alphaF,
  const double                                          gamma,
  const double                                          dt,
  const double                                          time,
  const bool                                            newton,
  const enum Fluid3::StabilisationAction                tds,
  const enum Fluid3::StabilisationAction                inertia,
  const enum Fluid3::StabilisationAction                pspg,
  const enum Fluid3::StabilisationAction                supg,
  const enum Fluid3::StabilisationAction                agls,
  const enum Fluid3::StabilisationAction                cstab,
  const enum Fluid3::StabilisationAction                cross,
  const enum Fluid3::StabilisationAction                reynolds,
  const bool                                            compute_elemat
  )
{
  //------------------------------------------------------------------
  //                     BLITZ CONFIGURATION
  //------------------------------------------------------------------
  //
  // We define the variables i,j,k to be indices to blitz arrays.
  // These are used for array expressions, that is matrix-vector
  // products in the following.

  blitz::firstIndex  i;   // Placeholder for the first index
  blitz::secondIndex j;   // Placeholder for the second index
  blitz::thirdIndex  k;   // Placeholder for the third index
  blitz::fourthIndex l;   // Placeholder for the fourth index

  blitz::Range       _ = blitz::Range::all();

  //------------------------------------------------------------------
  //           SET TIME INTEGRATION SCHEME RELATED DATA
  //------------------------------------------------------------------

  //         n+alpha_F     n+1
  //        t          = t     - (1-alpha_F) * dt

  const double timealphaF = time-(1-alphaF)*dt;

  //------------------------------------------------------------------
  //                      SET MATERIAL DATA
  //------------------------------------------------------------------
  // get viscosity
  // check here, if we really have a fluid !!
  dsassert(material->mattyp == m_fluid, "Material law is not of type m_fluid.");
  const double visc = material->m.fluid->viscosity;


  //------------------------------------------------------------------
  //                      SET ELEMENT DATA
  //------------------------------------------------------------------
  // set element data
  const DRT::Element::DiscretizationType distype = ele->Shape();

  // get node coordinates
  DRT::Node** nodes = ele->Nodes();
  for (int inode=0; inode<iel_; inode++)
  {
    const double* x = nodes[inode]->X();
    xyze_(0,inode) = x[0];
    xyze_(1,inode) = x[1];
    xyze_(2,inode) = x[2];
  }

  // add displacement, when fluid nodes move in the ALE case
  if (ele->is_ale_)
  {
    dserror("no ALE movement for genalpha yet");
  }

  // dead load in element nodes
  GetNodalBodyForce(ele,timealphaF);

  // stabilization parameter
  // This has to be done before anything else is calculated because
  // we use the same arrays internally.

  if(tds == Fluid3::subscales_time_dependent)
  {
#if 0
    if(inertia == Fluid3::inertia_stab_keep)
    {
      // INSTATIONARY FLOW PROBLEM, GENERALISED ALPHA, TIME DEPENDENT SUBSCALES
      //
      // tau_M: modification of
      //
      //    Franca, L.P. and Valentin, F.: On an Improved Unusual Stabilized
      //    Finite Element Method for the Advective-Reactive-Diffusive
      //    Equation. Computer Methods in Applied Mechanics and Enginnering,
      //    Vol. 190, pp. 1785-1800, 2000.
      //    http://www.lncc.br/~valentin/publication.htm                   */
      //
      // tau_Mp: modification of Barrenechea, G.R. and Valentin, F.
      //
      //    Barrenechea, G.R. and Valentin, F.: An unusual stabilized finite
      //    element method for a generalized Stokes problem. Numerische
      //    Mathematik, Vol. 92, pp. 652-677, 2002.
      //    http://www.lncc.br/~valentin/publication.htm
      //
      //
      // tau_C: kept Wall definition
      //
      // for the modifications see Codina, Principe, Guasch, Badia
      //    "Time dependent subscales in the stabilized finite  element
      //     approximation of incompressible flow problems"
      //
      //
      // see also: Codina, R. and Soto, O.: Approximation of the incompressible
      //    Navier-Stokes equations using orthogonal subscale stabilisation
      //    and pressure segregation on anisotropic finite element meshes.
      //    Computer methods in Applied Mechanics and Engineering,
      //    Vol 193, pp. 1403-1419, 2004.
      
      // use one point gauss rule to calculate tau at element center
      DRT::Utils::GaussRule3D integrationrule_stabili=DRT::Utils::intrule_hex_1point;
      switch (distype)
      {
          case hex8: case hex20: case hex27:
            integrationrule_stabili = DRT::Utils::intrule_hex_1point;
            break;
          case tet4: case tet10:
            integrationrule_stabili = DRT::Utils::intrule_tet_1point;
            break;
          default:
            dserror("invalid discretization type for fluid3");
      }
      
      // gaussian points
      const DRT::Utils::IntegrationPoints3D intpoints_onepoint(integrationrule_stabili);
      
      // shape functions and derivs at element center
      const double e1    = intpoints_onepoint.qxg[0][0];
      const double e2    = intpoints_onepoint.qxg[0][1];
      const double e3    = intpoints_onepoint.qxg[0][2];
      const double wquad = intpoints_onepoint.qwgt[0];
      
      DRT::Utils::shape_function_3D       (funct_,e1,e2,e3,distype);
      DRT::Utils::shape_function_3D_deriv1(deriv_,e1,e2,e3,distype);
      
      // get element type constant for tau
      double mk=0.0;
      switch (distype)
      {
          case tet4: case hex8:
            mk = 0.333333333333333333333;
            break;
          case hex20: case hex27: case tet10:
            mk = 0.083333333333333333333;
            break;
          default:
            dserror("type unknown!\n");
      }
      
      // get Jacobian matrix and determinant
      xjm_ = blitz::sum(deriv_(i,k)*xyze_(j,k),k);
      const double det = xjm_(0,0)*xjm_(1,1)*xjm_(2,2)+
        xjm_(0,1)*xjm_(1,2)*xjm_(2,0)+
        xjm_(0,2)*xjm_(1,0)*xjm_(2,1)-
        xjm_(0,2)*xjm_(1,1)*xjm_(2,0)-
        xjm_(0,0)*xjm_(1,2)*xjm_(2,1)-
        xjm_(0,1)*xjm_(1,0)*xjm_(2,2);
      const double vol = wquad*det;
      
      // get element length for tau_Mp/tau_C: volume-equival. diameter/sqrt(3)
      hk = pow((6.*vol/PI),(1.0/3.0))/sqrt(3.0);
      
      // get velocities (n+alpha_F,i) at integration point
      //               
      //                 +-----
      //       n+af       \                  n+af
      //    vel    (x) =   +      N (x) * vel
      //                  /        j         j
      //                 +-----        
      //                 node j
      //
      velintaf_ = blitz::sum(funct_(j)*evelaf(i,j),j);

      // get velocities (n+1,i)  at integration point
      //               
      //                +-----
      //       n+1       \                  n+1
      //    vel   (x) =   +      N (x) * vel
      //                 /        j         j
      //                +-----        
      //                node j
      //
      velintnp_    = blitz::sum(funct_(j)*evelnp(i,j),j);
      
      
      // get velocity norms
      const double vel_normaf = sqrt(blitz::sum(velintaf_*velintaf_));
      const double vel_normnp = sqrt(blitz::sum(velintnp_*velintnp_));
        
      //---------------------------------------------- compute tau_Mu = tau_Mp
      /* convective : viscous forces (element reynolds number)*/
      const double re_convectaf = (vel_normaf * hk / visc ) * (mk/2.0);

      double xi_convectaf;


      /*
        A smoothed version of
               
               xi_convect ^
                          |      /
                          |     /
                          |    /
                        1 +---+
                          |
                          |
                          |
                          +--------------> re_convect
                              1
        namely
                             
                          3  1  4
                          - +- x
                          4  4 
                            |
                            |
                            |    
                            |   x
                            |   
               xi_convect ^ |   |
                          | |   |/
                          | |   /
                          | |  /
                          | | +
                     0.75 ++++
                          |
                          |
                          +--------------> re_convect
                              1
      */
      
      if(re_convectaf >= 1.0)
      {
        // evaluation of (smoothed) xi function, here the classical one
        xi_convectaf=re_convectaf;

        // tauM similar to Codina's definition
        tau_(0) = DSQR(hk) / (4.0 * visc / mk + ( 4.0 * visc/mk) * xi_convectaf);

        /* linearisation of inverse tauM

               /  1.0 \            
            d |  ----  |          2.0 
               \ tauM /                          n+af
            ------------ =  ----------------  * u    
            d   u           ||  n+af ||          dim 
                 dim        || u     || * hk
                            ||       ||
         
        */
       
        invtauM_lin_ = 2./(vel_normaf*hk)*velintaf_;
      }
      else
      {
        // evaluation of smoothed xi function
        xi_convectaf=0.75+0.25*re_convectaf*re_convectaf*re_convectaf*re_convectaf;


        // same definition of tauM using modified xi_convectaf 
        tau_(0) = DSQR(hk) / (4.0 * visc / mk + ( 4.0 * visc/mk) * xi_convectaf);
        
        /* linearisation of inverse tauM

               /  1.0 \            
            d |  ----  |           3                     2
               \ tauM /     1    mk       2   ||  n+af ||    n+af
            ------------ =  - * ----- * hk  * || u     || * u
            d   u           4       3         ||       ||    dim 
                 dim            visc
         
        */
        
        invtauM_lin_ = (mk*mk*mk*hk*hk*vel_normaf*vel_normaf)/(4*visc*visc*visc)*velintaf_;
      }


      
    /*------------------------------------------------------ compute tau_C ---*/

    //-- stability parameter definition according to Wall Diss. 99
    /*
               xi_convect ^
                          |
                        1 |   +-----------
                          |  /
                          | /
                          |/
                          +--------------> Re_convect
                              1
    */
    const double re_convectnp = (vel_normnp * hk / visc ) * (mk/2.0);

    const double xi_tau_c = DMIN(re_convectnp,1.0);
    
    tau_(2) = vel_normnp * hk * 0.5 * xi_tau_c;

    
    /*-- stability parameter definition according to Codina (2002), CMAME 191
     *
     * Analysis of a stabilized finite element approximation of the transient
     * convection-diffusion-reaction equation using orthogonal subscales.
     * Ramon Codina, Jordi Blasco; Comput. Visual. Sci., 4 (3): 167-174, 2002.
     *
     * */
    //tau_(2) = sqrt(DSQR(visc)+DSQR(0.5*vel_normnp*hk));
    //tau_(2) = hk * hk /tau_(0);
        
    }
    else
#endif      
    {

    
    // INSTATIONARY FLOW PROBLEM, GENERALISED ALPHA, TIME DEPENDENT SUBSCALES
    //
    // tau_M: modification of
    //
    //    Franca, L.P. and Valentin, F.: On an Improved Unusual Stabilized
    //    Finite Element Method for the Advective-Reactive-Diffusive
    //    Equation. Computer Methods in Applied Mechanics and Enginnering,
    //    Vol. 190, pp. 1785-1800, 2000.
    //    http://www.lncc.br/~valentin/publication.htm                   */
    //
    // tau_Mp: modification of Barrenechea, G.R. and Valentin, F.
    //
    //    Barrenechea, G.R. and Valentin, F.: An unusual stabilized finite
    //    element method for a generalized Stokes problem. Numerische
    //    Mathematik, Vol. 92, pp. 652-677, 2002.
    //    http://www.lncc.br/~valentin/publication.htm
    //
    //
    // tau_C: kept Wall definition
    //
    // for the modifications see Codina, Principe, Guasch, Badia
    //    "Time dependent subscales in the stabilized finite  element
    //     approximation of incompressible flow problems"
    //
    //
    // see also: Codina, R. and Soto, O.: Approximation of the incompressible
    //    Navier-Stokes equations using orthogonal subscale stabilisation
    //    and pressure segregation on anisotropic finite element meshes.
    //    Computer methods in Applied Mechanics and Engineering,
    //    Vol 193, pp. 1403-1419, 2004.

    // use one point gauss rule to calculate tau at element center
    DRT::Utils::GaussRule3D integrationrule_stabili=DRT::Utils::intrule_hex_1point;
    switch (distype)
    {
    case DRT::Element::hex8:
    case DRT::Element::hex20:
    case DRT::Element::hex27:
          integrationrule_stabili = DRT::Utils::intrule_hex_1point;
          break;
    case DRT::Element::tet4:
    case DRT::Element::tet10:
          integrationrule_stabili = DRT::Utils::intrule_tet_1point;
          break;
        default:
          dserror("invalid discretization type for fluid3");
    }

    // gaussian points
    const DRT::Utils::IntegrationPoints3D intpoints_onepoint(integrationrule_stabili);

    // shape functions and derivs at element center
    const double e1    = intpoints_onepoint.qxg[0][0];
    const double e2    = intpoints_onepoint.qxg[0][1];
    const double e3    = intpoints_onepoint.qxg[0][2];
    const double wquad = intpoints_onepoint.qwgt[0];

    DRT::Utils::shape_function_3D       (funct_,e1,e2,e3,distype);
    DRT::Utils::shape_function_3D_deriv1(deriv_,e1,e2,e3,distype);

    // get element type constant for tau
    double mk=0.0;
    switch (distype)
    {
    case DRT::Element::tet4:
    case DRT::Element::hex8:
          mk = 0.333333333333333333333;
          break;
    case DRT::Element::hex20:
    case DRT::Element::hex27:
    case DRT::Element::tet10:
          mk = 0.083333333333333333333;
          break;
        default:
          dserror("type unknown!\n");
    }

    // get Jacobian matrix and determinant
    xjm_ = blitz::sum(deriv_(i,k)*xyze_(j,k),k);
    const double det = xjm_(0,0)*xjm_(1,1)*xjm_(2,2)+
                       xjm_(0,1)*xjm_(1,2)*xjm_(2,0)+
                       xjm_(0,2)*xjm_(1,0)*xjm_(2,1)-
                       xjm_(0,2)*xjm_(1,1)*xjm_(2,0)-
                       xjm_(0,0)*xjm_(1,2)*xjm_(2,1)-
                       xjm_(0,1)*xjm_(1,0)*xjm_(2,2);
    const double vol = wquad*det;

    // get element length for tau_Mp/tau_C: volume-equival. diameter/sqrt(3)
    const double hk = pow((6.*vol/PI),(1.0/3.0))/sqrt(3.0);

    // get velocities (n+alpha_F,i) at integration point
    //
    //                 +-----
    //       n+af       \                  n+af
    //    vel    (x) =   +      N (x) * vel
    //                  /        j         j
    //                 +-----
    //                 node j
    //
    velintaf_ = blitz::sum(funct_(j)*evelaf(i,j),j);

    // get velocities (n+1,i)  at integration point
    //
    //                +-----
    //       n+1       \                  n+1
    //    vel   (x) =   +      N (x) * vel
    //                 /        j         j
    //                +-----
    //                node j
    //
    velintnp_    = blitz::sum(funct_(j)*evelnp(i,j),j);


    // get velocity norms
    const double vel_normaf = sqrt(blitz::sum(velintaf_*velintaf_));
    const double vel_normnp = sqrt(blitz::sum(velintnp_*velintnp_));

    //---------------------------------------------- compute tau_Mu = tau_Mp
    /* convective : viscous forces (element reynolds number)*/
    const double re_convectaf = (vel_normaf * hk / visc ) * (mk/2.0);

    const double xi_convectaf = DMAX(re_convectaf,1.0);

    /*
               xi_convect ^
                          |      /
                          |     /
                          |    /
                        1 +---+
                          |
                          |
                          |
                          +--------------> re_convect
                              1
    */

    tau_(0) = DSQR(hk) / (4.0 * visc / mk + ( 4.0 * visc/mk) * xi_convectaf);

    /*------------------------------------------------------ compute tau_C ---*/

    //-- stability parameter definition according to Wall Diss. 99
    /*
               xi_convect ^
                          |
                        1 |   +-----------
                          |  /
                          | /
                          |/
                          +--------------> Re_convect
                              1
    */
    const double re_convectnp = (vel_normnp * hk / visc ) * (mk/2.0);

    const double xi_tau_c = DMIN(re_convectnp,1.0);

    tau_(2) = vel_normnp * hk * 0.5 * xi_tau_c;
    //tau_(2) = hk * hk /tau_(0);
    /*-- stability parameter definition according to Codina (2002), CMAME 191
     *
     * Analysis of a stabilized finite element approximation of the transient
     * convection-diffusion-reaction equation using orthogonal subscales.
     * Ramon Codina, Jordi Blasco; Comput. Visual. Sci., 4 (3): 167-174, 2002.
     *
     * */
    //tau(2) = sqrt(DSQR(visc)+DSQR(0.5*vel_normnp*hk));
    }
  }
  else
  {
    // INSTATIONARY FLOW PROBLEM, GENERALISED ALPHA
    // tau_M: Barrenechea, G.R. and Valentin, F.
    // tau_C: Wall

    // use one point gauss rule to calculate tau at element center
    DRT::Utils::GaussRule3D integrationrule_stabili = DRT::Utils::intrule_hex_1point;
    switch (distype)
    {
    case DRT::Element::hex8:
    case DRT::Element::hex20:
    case DRT::Element::hex27:
          integrationrule_stabili = DRT::Utils::intrule_hex_1point;
          break;
    case DRT::Element::tet4:
    case DRT::Element::tet10:
          integrationrule_stabili = DRT::Utils::intrule_tet_1point;
          break;
        default:
          dserror("invalid discretization type for fluid3");
    }

    // gaussian points
    const DRT::Utils::IntegrationPoints3D intpoints_onepoint(integrationrule_stabili);

    // shape functions and derivs at element center
    const double e1    = intpoints_onepoint.qxg[0][0];
    const double e2    = intpoints_onepoint.qxg[0][1];
    const double e3    = intpoints_onepoint.qxg[0][2];
    const double wquad = intpoints_onepoint.qwgt[0];

    DRT::Utils::shape_function_3D       (funct_,e1,e2,e3,distype);
    DRT::Utils::shape_function_3D_deriv1(deriv_,e1,e2,e3,distype);

    // get element type constant for tau
    double mk=0.0;
    switch (distype)
    {
    case DRT::Element::tet4:
    case DRT::Element::hex8:
          mk = 0.333333333333333333333;
          break;
    case DRT::Element::hex20:
    case DRT::Element::hex27:
    case DRT::Element::tet10:
          mk = 0.083333333333333333333;
          break;
        default:
          dserror("type unknown!\n");
    }

    // get Jacobian matrix and determinant
    xjm_ = blitz::sum(deriv_(i,k)*xyze_(j,k),k);
    const double det = xjm_(0,0)*xjm_(1,1)*xjm_(2,2)+
                       xjm_(0,1)*xjm_(1,2)*xjm_(2,0)+
                       xjm_(0,2)*xjm_(1,0)*xjm_(2,1)-
                       xjm_(0,2)*xjm_(1,1)*xjm_(2,0)-
                       xjm_(0,0)*xjm_(1,2)*xjm_(2,1)-
                       xjm_(0,1)*xjm_(1,0)*xjm_(2,2);
    const double vol = wquad*det;

    // get element length for tau_Mp/tau_C: volume-equival. diameter/sqrt(3)
    const double hk = pow((6.*vol/PI),(1.0/3.0))/sqrt(3.0);

    //--------------------------------------------------------------
    //             compute global first derivates
    //--------------------------------------------------------------
    /*
      Use the Jacobian and the known derivatives in element coordinate
      directions on the right hand side to compute the derivatives in
      global coordinate directions

          +-                 -+     +-    -+      +-    -+
          |  dx    dy    dz   |     | dN_k |      | dN_k |
          |  --    --    --   |     | ---- |      | ---- |
          |  dr    dr    dr   |     |  dx  |      |  dr  |
          |                   |     |      |      |      |
          |  dx    dy    dz   |     | dN_k |      | dN_k |
          |  --    --    --   |  *  | ---- |   =  | ---- | for all k
          |  ds    ds    ds   |     |  dy  |      |  ds  |
          |                   |     |      |      |      |
          |  dx    dy    dz   |     | dN_k |      | dN_k |
          |  --    --    --   |     | ---- |      | ---- |
          |  dt    dt    dt   |     |  dz  |      |  dt  |
          +-                 -+     +-    -+      +-    -+

     Do one LU factorisation, everything else is backward substitution!

     */

    {
      // LAPACK solver
      Epetra_LAPACK          solver;

      // this copy of xjm will be used to calculate a in place factorisation
      blitz::Array<double,2> factorU(3,3,blitz::ColumnMajorArray<2>());
      factorU=xjm_.copy();

      // a vector specifying the pivots (reordering)
      int pivot[3];

      // error code
      int ierr = 0;

      // Perform LU factorisation
      solver.GETRF(3,3,factorU.data(),3,&(pivot[0]),&ierr);

      if (ierr!=0)
      {
        dserror("Unable to perform LU factorisation during computation of derxy");
      }

      // backward substitution. The copy is required since GETRS replaces
      // the input with the result
      derxy_ =deriv_.copy();
      solver.GETRS('N',3,iel_,factorU.data(),3,&(pivot[0]),derxy_.data(),3,&ierr);

      if (ierr!=0)
      {
        dserror("Unable to perform backward substitution after factorisation of jacobian");
      }
    }

    // get velocities (n+alpha_F,i) at integration point
    //
    //                 +-----
    //       n+af       \                  n+af
    //    vel    (x) =   +      N (x) * vel
    //                  /        j         j
    //                 +-----
    //                 node j
    //
    velintaf_ = blitz::sum(funct_(j)*evelaf(i,j),j);

    // get velocity norm
    const double vel_norm = sqrt(blitz::sum(velintaf_*velintaf_));

    // normed velocity at element centre
    if (vel_norm>=1e-6)
    {
      velintaf_ = velintaf_/vel_norm;
    }
    else
    {
      velintaf_ = 0.;
      velintaf_(0) = 1;
    }

    // get streamlength
    const double val = blitz::sum(blitz::abs(blitz::sum(velintaf_(j)*derxy_(j,i),j)));
    const double strle = 2.0/val;

    // time factor
    const double timefac = gamma*dt;

    /*----------------------------------------------------- compute tau_Mu ---*/
    /* stability parameter definition according to

              Barrenechea, G.R. and Valentin, F.: An unusual stabilized finite
              element method for a generalized Stokes problem. Numerische
              Mathematik, Vol. 92, pp. 652-677, 2002.
              http://www.lncc.br/~valentin/publication.htm
    and:
              Franca, L.P. and Valentin, F.: On an Improved Unusual Stabilized
              Finite Element Method for the Advective-Reactive-Diffusive
              Equation. Computer Methods in Applied Mechanics and Enginnering,
              Vol. 190, pp. 1785-1800, 2000.
              http://www.lncc.br/~valentin/publication.htm                   */


    const double re1 = 4.0 * timefac * visc / (mk * DSQR(strle)); /* viscous : reactive forces   */
    const double re2 = mk * vel_norm * strle / (2.0 * visc);      /* convective : viscous forces */

    const double xi1 = DMAX(re1,1.0);
    const double xi2 = DMAX(re2,1.0);

    tau_(0) = timefac * DSQR(strle) / (DSQR(strle)*xi1+( 4.0 * timefac*visc/mk)*xi2);

    // compute tau_Mp
    //    stability parameter definition according to Franca and Valentin (2000)
    //                                       and Barrenechea and Valentin (2002)
    const double re_viscous = 4.0 * timefac * visc / (mk * DSQR(hk)); /* viscous : reactive forces   */
    const double re_convect = mk * vel_norm * hk / (2.0 * visc);      /* convective : viscous forces */

    const double xi_viscous = DMAX(re_viscous,1.0);
    const double xi_convect = DMAX(re_convect,1.0);

    /*
                  xi1,xi2 ^
                          |      /
                          |     /
                          |    /
                        1 +---+
                          |
                          |
                          |
                          +--------------> re1,re2
                              1
    */
    tau_(1) = timefac * DSQR(hk) / (DSQR(hk) * xi_viscous + ( 4.0 * timefac * visc/mk) * xi_convect);

    /*------------------------------------------------------ compute tau_C ---*/
    /*-- stability parameter definition according to Codina (2002), CMAME 191
     *
     * Analysis of a stabilized finite element approximation of the transient
     * convection-diffusion-reaction equation using orthogonal subscales.
     * Ramon Codina, Jordi Blasco; Comput. Visual. Sci., 4 (3): 167-174, 2002.
     *
     * */
    //tau(2) = sqrt(DSQR(visc)+DSQR(0.5*vel_norm*hk));

    // Wall Diss. 99
    /*
                      xi2 ^
                          |
                        1 |   +-----------
                          |  /
                          | /
                          |/
                          +--------------> Re2
                              1
    */
    const double xi_tau_c = DMIN(re2,1.0);
    tau_(2) = vel_norm * hk * 0.5 * xi_tau_c;
  }


  // flag for higher order elements
  const bool higher_order_ele = ele->isHigherOrderElement(distype);

  // gaussian points
  const DRT::Utils::IntegrationPoints3D intpoints(ele->gaussrule_);

  // remember whether the subscale quantities have been allocated an set to zero.
  if(tds == Fluid3::subscales_time_dependent)
  {
    // if not available, the arrays for the subscale quantities have to
    // be resized and initialised to zero
    if(ele->sub_acc_old_.extent(blitz::firstDim) != 3 || ele->sub_acc_old_.extent(blitz::secondDim) != intpoints.nquad)
    {
      ele->sub_acc_old_ .resize(3,intpoints.nquad);
      ele->sub_acc_old_  = 0.;
    }
    if(ele->sub_vel_old_.extent(blitz::firstDim) != 3 || ele->sub_vel_old_.extent(blitz::secondDim) != intpoints.nquad)
    {
      ele->sub_vel_old_ .resize(3,intpoints.nquad);
      ele->sub_vel_old_  = 0.;

      ele->sub_vel_.resize(3,intpoints.nquad);
      ele->sub_vel_ = 0.;
    }
    if(ele->sub_pre_old_ .extent(blitz::firstDim) != intpoints.nquad)
    {
      ele->sub_pre_old_ .resize(intpoints.nquad);
      ele->sub_pre_old_ = 0.;

      ele->sub_pre_.resize(intpoints.nquad);
      ele->sub_pre_ = 0.;
    }
  }

  // get subscale information from element --- this is just a reference
  // to the element data
  blitz::Array<double,2> saccn (ele->sub_acc_old_);
  blitz::Array<double,2> sveln (ele->sub_vel_old_);
  blitz::Array<double,2> svelnp(ele->sub_vel_    );
  blitz::Array<double,1> spren (ele->sub_pre_old_);
  blitz::Array<double,1> sprenp(ele->sub_pre_    );

  // just define certain constants for conveniance
  const double afgdt  = alphaF * gamma * dt;


  //------------------------------------------------------------------
  //                       INTEGRATION LOOP
  //------------------------------------------------------------------
  for (int iquad=0;iquad<intpoints.nquad;++iquad)
  {
  
    // set gauss point coordinates
    const double e1 = intpoints.qxg[iquad][0];
    const double e2 = intpoints.qxg[iquad][1];
    const double e3 = intpoints.qxg[iquad][2];

    // get values of shape functions and derivatives in the gausspoint
    DRT::Utils::shape_function_3D(funct_,e1,e2,e3,distype);
    DRT::Utils::shape_function_3D_deriv1(deriv_,e1,e2,e3,distype);
    if (higher_order_ele)
    {
      DRT::Utils::shape_function_3D_deriv2(deriv2_,e1,e2,e3,distype);
    }

    // get transposed Jacobian matrix and determinant
    //
    //        +-            -+ T      +-            -+
    //        | dx   dx   dx |        | dx   dy   dz |
    //        | --   --   -- |        | --   --   -- |
    //        | dr   ds   dt |        | dr   dr   dr |
    //        |              |        |              |
    //        | dy   dy   dy |        | dx   dy   dz |
    //        | --   --   -- |   =    | --   --   -- |
    //        | dr   ds   dt |        | ds   ds   ds |
    //        |              |        |              |
    //        | dz   dz   dz |        | dx   dy   dz |
    //        | --   --   -- |        | --   --   -- |
    //        | dr   ds   dt |        | dt   dt   dt |
    //        +-            -+        +-            -+
    //
    // The Jacobian is computed using the formula
    //
    //            +-----
    //   dx_j(r)   \      dN_k(r)
    //   -------  = +     ------- * (x_j)_k
    //    dr_i     /       dr_i       |
    //            +-----    |         |
    //            node k    |         |
    //                  derivative    |
    //                   of shape     |
    //                   function     |
    //                           component of
    //                          node coordinate
    //
    xjm_ = blitz::sum(deriv_(i,k)*xyze_(j,k),k);
    // The determinant ist computed using Sarrus's rule
    const double det = xjm_(0,0)*xjm_(1,1)*xjm_(2,2)+
                       xjm_(0,1)*xjm_(1,2)*xjm_(2,0)+
                       xjm_(0,2)*xjm_(1,0)*xjm_(2,1)-
                       xjm_(0,2)*xjm_(1,1)*xjm_(2,0)-
                       xjm_(0,0)*xjm_(1,2)*xjm_(2,1)-
                       xjm_(0,1)*xjm_(1,0)*xjm_(2,2);

    // check for degenerated elements
    if (det < 0.0)
    {
      dserror("GLOBAL ELEMENT NO.%i\nNEGATIVE JACOBIAN DETERMINANT: %lf", ele->Id(), det);
    }

    // set total integration factor
    const double fac = intpoints.qwgt[iquad]*det;

    //--------------------------------------------------------------
    //             compute global first derivates
    //--------------------------------------------------------------
    /*
      Use the Jacobian and the known derivatives in element coordinate
      directions on the right hand side to compute the derivatives in
      global coordinate directions

          +-                 -+     +-    -+      +-    -+
          |  dx    dy    dz   |     | dN_k |      | dN_k |
          |  --    --    --   |     | ---- |      | ---- |
          |  dr    dr    dr   |     |  dx  |      |  dr  |
          |                   |     |      |      |      |
          |  dx    dy    dz   |     | dN_k |      | dN_k |
          |  --    --    --   |  *  | ---- |   =  | ---- | for all k
          |  ds    ds    ds   |     |  dy  |      |  ds  |
          |                   |     |      |      |      |
          |  dx    dy    dz   |     | dN_k |      | dN_k |
          |  --    --    --   |     | ---- |      | ---- |
          |  dt    dt    dt   |     |  dz  |      |  dt  |
          +-                 -+     +-    -+      +-    -+

     Do one LU factorisation, everything else is backward substitution!

     */

    {
      // LAPACK solver
      Epetra_LAPACK          solver;

      // this copy of xjm will be used to calculate a in place factorisation
      blitz::Array<double,2> factorU(3,3,blitz::ColumnMajorArray<2>());
      factorU=xjm_.copy();

      // a vector specifying the pivots (reordering)
      int pivot[3];

      // error code
      int ierr = 0;

      // Perform LU factorisation
      solver.GETRF(3,3,factorU.data(),3,&(pivot[0]),&ierr);

      if (ierr!=0)
      {
        dserror("Unable to perform LU factorisation during computation of derxy");
      }

      // backward substitution. The copy is required since GETRS replaces
      // the input with the result
      derxy_ =deriv_.copy();
      solver.GETRS('N',3,iel_,factorU.data(),3,&(pivot[0]),derxy_.data(),3,&ierr);

      if (ierr!=0)
      {
        dserror("Unable to perform backward substitution after factorisation of jacobian");
      }
    }

    //--------------------------------------------------------------
    //             compute second global derivative
    //--------------------------------------------------------------

    /*----------------------------------------------------------------------*
     |  calculate second global derivatives w.r.t. x,y,z at point r,s,t
     |                                            (private)      gammi 07/07
     |
     | From the six equations
     |
     |              +-                     -+
     |  d^2N     d  | dx dN   dy dN   dz dN |
     |  ----   = -- | --*-- + --*-- + --*-- |
     |  dr^2     dr | dr dx   dr dy   dr dz |
     |              +-                     -+
     |
     |              +-                     -+
     |  d^2N     d  | dx dN   dy dN   dz dN |
     |  ------ = -- | --*-- + --*-- + --*-- |
     |  ds^2     ds | ds dx   ds dy   ds dz |
     |              +-                     -+
     |
     |              +-                     -+
     |  d^2N     d  | dx dN   dy dN   dz dN |
     |  ----   = -- | --*-- + --*-- + --*-- |
     |  dt^2     dt | dt dx   dt dy   dt dz |
     |              +-                     -+
     |
     |              +-                     -+
     |  d^2N     d  | dx dN   dy dN   dz dN |
     | -----   = -- | --*-- + --*-- + --*-- |
     | ds dr     ds | dr dx   dr dy   dr dz |
     |              +-                     -+
     |
     |              +-                     -+
     |  d^2N     d  | dx dN   dy dN   dz dN |
     | -----   = -- | --*-- + --*-- + --*-- |
     | dt dr     dt | dr dx   dr dy   dr dz |
     |              +-                     -+
     |
     |              +-                     -+
     |  d^2N     d  | dx dN   dy dN   dz dN |
     | -----   = -- | --*-- + --*-- + --*-- |
     | ds dt     ds | dt dx   dt dy   dt dz |
     |              +-                     -+
     |
     | the matrix (jacobian-bar matrix) system
     |
     | +-                                                                                         -+   +-    -+
     | |   /dx\^2        /dy\^2         /dz\^2           dy dx           dz dx           dy dz     |   | d^2N |
     | |  | -- |        | ---|         | ---|          2*--*--         2*--*--         2*--*--     |   | ---- |
     | |   \dr/          \dr/           \dr/             dr dr           dr dr           dr dr     |   | dx^2 |
     | |                                                                                           |   |      |
     | |   /dx\^2        /dy\^2         /dz\^2           dy dx           dz dx           dy dz     |   | d^2N |
     | |  | -- |        | ---|         | ---|          2*--*--         2*--*--         2*--*--     |   | ---- |
     | |   \ds/          \ds/           \ds/             ds ds           ds ds           ds ds     |   | dy^2 |
     | |                                                                                           |   |      |
     | |   /dx\^2        /dy\^2         /dz\^2           dy dx           dz dx           dy dz     |   | d^2N |
     | |  | -- |        | ---|         | ---|          2*--*--         2*--*--         2*--*--     |   | ---- |
     | |   \dt/          \dt/           \dt/             dt dt           dt dt           dt dt     |   | dz^2 |
     | |                                                                                           | * |      |
     | |   dx dx         dy dy          dz dz        dx dy   dx dy   dx dz   dx dz  dy dz   dy dz  |   | d^2N |
     | |   --*--         --*--          --*--        --*-- + --*--   --*-- + --*--  --*-- + --*--  |   | ---- |
     | |   dr ds         dr ds          dr ds        dr ds   ds dr   dr ds   ds dr  dr ds   ds dr  |   | dxdy |
     | |                                                                                           |   |      |
     | |   dx dx         dy dy          dz dz        dx dy   dx dy   dx dz   dx dz  dy dz   dy dz  |   | d^2N |
     | |   --*--         --*--          --*--        --*-- + --*--   --*-- + --*--  --*-- + --*--  |   | ---- |
     | |   dr dt         dr dt          dr dt        dr dt   dt dr   dr dt   dt dr  dr dt   dt dr  |   | dxdz |
     | |                                                                                           |   |      |
     | |   dx dx         dy dy          dz dz        dx dy   dx dy   dx dz   dx dz  dy dz   dy dz  |   | d^2N |
     | |   --*--         --*--          --*--        --*-- + --*--   --*-- + --*--  --*-- + --*--  |   | ---- |
     | |   dt ds         dt ds          dt ds        dt ds   ds dt   dt ds   ds dt  dt ds   ds dt  |   | dydz |
     | +-                                                                                         -+   +-    -+
     |
     |                  +-    -+     +-                           -+
     |                  | d^2N |     | d^2x dN   d^2y dN   d^2y dN |
     |                  | ---- |     | ----*-- + ----*-- + ----*-- |
     |                  | dr^2 |     | dr^2 dx   dr^2 dy   dr^2 dz |
     |                  |      |     |                             |
     |                  | d^2N |     | d^2x dN   d^2y dN   d^2y dN |
     |                  | ---- |     | ----*-- + ----*-- + ----*-- |
     |                  | ds^2 |     | ds^2 dx   ds^2 dy   ds^2 dz |
     |                  |      |     |                             |
     |                  | d^2N |     | d^2x dN   d^2y dN   d^2y dN |
     |                  | ---- |     | ----*-- + ----*-- + ----*-- |
     |                  | dt^2 |     | dt^2 dx   dt^2 dy   dt^2 dz |
     |              =   |      |  -  |                             |
     |                  | d^2N |     | d^2x dN   d^2y dN   d^2y dN |
     |                  | ---- |     | ----*-- + ----*-- + ----*-- |
     |                  | drds |     | drds dx   drds dy   drds dz |
     |                  |      |     |                             |
     |                  | d^2N |     | d^2x dN   d^2y dN   d^2y dN |
     |                  | ---- |     | ----*-- + ----*-- + ----*-- |
     |                  | drdt |     | drdt dx   drdt dy   drdt dz |
     |                  |      |     |                             |
     |                  | d^2N |     | d^2x dN   d^2y dN   d^2z dN |
     |                  | ---- |     | ----*-- + ----*-- + ----*-- |
     |                  | dtds |     | dtds dx   dtds dy   dtds dz |
     |                  +-    -+     +-                           -+
     |
     |
     | is derived. This is solved for the unknown global derivatives.
     |
     |
     |             jacobian_bar * derxy2 = deriv2 - xder2 * derxy
     |                                              |           |
     |                                              +-----------+
     |                                              'chainrulerhs'
     |                                     |                    |
     |                                     +--------------------+
     |                                          'chainrulerhs'
     |
     *----------------------------------------------------------------------*/
    if (higher_order_ele)
    {
      // initialize and zero out everything
      blitz::Array<double,2> bm(6,6,blitz::ColumnMajorArray<2>());

      // calculate elements of jacobian_bar matrix
      bm(0,0) = xjm_(0,0)*xjm_(0,0);
      bm(1,0) = xjm_(1,0)*xjm_(1,0);
      bm(2,0) = xjm_(2,0)*xjm_(2,0);
      bm(3,0) = xjm_(0,0)*xjm_(1,0);
      bm(4,0) = xjm_(0,0)*xjm_(2,0);
      bm(5,0) = xjm_(2,0)*xjm_(1,0);

      bm(0,1) = xjm_(0,1)*xjm_(0,1);
      bm(1,1) = xjm_(1,1)*xjm_(1,1);
      bm(2,1) = xjm_(2,1)*xjm_(2,1);
      bm(3,1) = xjm_(0,1)*xjm_(1,1);
      bm(4,1) = xjm_(0,1)*xjm_(2,1);
      bm(5,1) = xjm_(2,1)*xjm_(1,1);

      bm(0,2) = xjm_(0,2)*xjm_(0,2);
      bm(1,2) = xjm_(1,2)*xjm_(1,2);
      bm(2,2) = xjm_(2,2)*xjm_(2,2);
      bm(3,2) = xjm_(0,2)*xjm_(1,2);
      bm(4,2) = xjm_(0,2)*xjm_(2,2);
      bm(5,2) = xjm_(2,2)*xjm_(1,2);

      bm(0,3) = 2.*xjm_(0,0)*xjm_(0,1);
      bm(1,3) = 2.*xjm_(1,0)*xjm_(1,1);
      bm(2,3) = 2.*xjm_(2,0)*xjm_(2,1);
      bm(3,3) = xjm_(0,0)*xjm_(1,1)+xjm_(1,0)*xjm_(0,1);
      bm(4,3) = xjm_(0,0)*xjm_(2,1)+xjm_(2,0)*xjm_(0,1);
      bm(5,3) = xjm_(1,0)*xjm_(2,1)+xjm_(2,0)*xjm_(1,1);

      bm(0,4) = 2.*xjm_(0,0)*xjm_(0,2);
      bm(1,4) = 2.*xjm_(1,0)*xjm_(1,2);
      bm(2,4) = 2.*xjm_(2,0)*xjm_(2,2);
      bm(3,4) = xjm_(0,0)*xjm_(1,2)+xjm_(1,0)*xjm_(0,2);
      bm(4,4) = xjm_(0,0)*xjm_(2,2)+xjm_(2,0)*xjm_(0,2);
      bm(5,4) = xjm_(1,0)*xjm_(2,2)+xjm_(2,0)*xjm_(1,2);

      bm(0,5) = 2.*xjm_(0,1)*xjm_(0,2);
      bm(1,5) = 2.*xjm_(1,1)*xjm_(1,2);
      bm(2,5) = 2.*xjm_(2,1)*xjm_(2,2);
      bm(3,5) = xjm_(0,1)*xjm_(1,2)+xjm_(1,1)*xjm_(0,2);
      bm(4,5) = xjm_(0,1)*xjm_(2,2)+xjm_(2,1)*xjm_(0,2);
      bm(5,5) = xjm_(1,1)*xjm_(2,2)+xjm_(2,1)*xjm_(1,2);

      /*------------------ determine 2nd derivatives of coord.-functions */
      /*
       |
       |         0 1 2              0...iel-1
       |        +-+-+-+             +-+-+-+-+        0 1 2
       |        | | | | 0           | | | | | 0     +-+-+-+
       |        +-+-+-+             +-+-+-+-+       | | | | 0
       |        | | | | 1           | | | | | 1     +-+-+-+ .
       |        +-+-+-+             +-+-+-+-+       | | | | .
       |        | | | | 2           | | | | | 2     +-+-+-+
       |        +-+-+-+       =     +-+-+-+-+    *  | | | | .
       |        | | | | 3           | | | | | 3     +-+-+-+ .
       |        +-+-+-+             +-+-+-+-+       | | | | .
       |        | | | | 4           | | | | | 4     +-+-+-+ .
       |        +-+-+-+             +-+-+-+-+       | | | | .
       |        | | | | 5           | | | | | 5     +-+-+-+
       |        +-+-+-+             +-+-+-+-+       | | | | iel-1
       |                                            +-+-+-+
       |
       |        xder2               deriv2          xyze^T
       |
       |
       |                                     +-                  -+
       |  	   	    	    	     | d^2x   d^2y   d^2z |
       |  	   	    	    	     | ----   ----   ---- |
       | 	   	   	   	     | dr^2   dr^2   dr^2 |
       | 	   	   	   	     |                    |
       | 	   	   	   	     | d^2x   d^2y   d^2z |
       |                                     | ----   ----   ---- |
       | 	   	   	   	     | ds^2   ds^2   ds^2 |
       | 	   	   	   	     |                    |
       | 	   	   	   	     | d^2x   d^2y   d^2z |
       | 	   	   	   	     | ----   ----   ---- |
       | 	   	   	   	     | dt^2   dt^2   dt^2 |
       |               yields    xder2  =    |                    |
       |                                     | d^2x   d^2y   d^2z |
       |                                     | ----   ----   ---- |
       |                                     | drds   drds   drds |
       |                                     |                    |
       |                                     | d^2x   d^2y   d^2z |
       |                                     | ----   ----   ---- |
       |                                     | drdt   drdt   drdt |
       |                                     |                    |
       |                                     | d^2x   d^2y   d^2z |
       |                                     | ----   ----   ---- |
       |                                     | dsdt   dsdt   dsdt |
       | 	   	   	   	     +-                  -+
       |
       |
      */
      xder2_ = blitz::sum(deriv2_(i,k)*xyze_(j,k),k);

      /*
       |        0...iel-1             0 1 2
       |        +-+-+-+-+            +-+-+-+
       |        | | | | | 0          | | | | 0
       |        +-+-+-+-+            +-+-+-+            0...iel-1
       |        | | | | | 1          | | | | 1         +-+-+-+-+
       |        +-+-+-+-+            +-+-+-+           | | | | | 0
       |        | | | | | 2          | | | | 2         +-+-+-+-+
       |        +-+-+-+-+       =    +-+-+-+       *   | | | | | 1 * (-1)
       |        | | | | | 3          | | | | 3         +-+-+-+-+
       |        +-+-+-+-+            +-+-+-+           | | | | | 2
       |        | | | | | 4          | | | | 4         +-+-+-+-+
       |        +-+-+-+-+            +-+-+-+
       |        | | | | | 5          | | | | 5          derxy
       |        +-+-+-+-+            +-+-+-+
       |
       |       chainrulerhs          xder2
      */
      derxy2_ = -blitz::sum(xder2_(i,k)*derxy_(k,j),k);

      /*
       |        0...iel-1            0...iel-1         0...iel-1
       |        +-+-+-+-+            +-+-+-+-+         +-+-+-+-+
       |        | | | | | 0          | | | | | 0       | | | | | 0
       |        +-+-+-+-+            +-+-+-+-+         +-+-+-+-+
       |        | | | | | 1          | | | | | 1       | | | | | 1
       |        +-+-+-+-+            +-+-+-+-+         +-+-+-+-+
       |        | | | | | 2          | | | | | 2       | | | | | 2
       |        +-+-+-+-+       =    +-+-+-+-+    +    +-+-+-+-+
       |        | | | | | 3          | | | | | 3       | | | | | 3
       |        +-+-+-+-+            +-+-+-+-+         +-+-+-+-+
       |        | | | | | 4          | | | | | 4       | | | | | 4
       |        +-+-+-+-+            +-+-+-+-+         +-+-+-+-+
       |        | | | | | 5          | | | | | 5       | | | | | 5
       |        +-+-+-+-+            +-+-+-+-+         +-+-+-+-+
       |
       |       chainrulerhs         chainrulerhs        deriv2
      */
      derxy2_ += deriv2_;

      /* make LU decomposition and solve system for all right hand sides
       * (i.e. the components of chainrulerhs)
       |
       |          0  1  2  3  4  5         i        i
       | 	   +--+--+--+--+--+--+       +-+      +-+
       | 	   |  |  |  |  |  |  | 0     | | 0    | | 0
       | 	   +--+--+--+--+--+--+       +-+      +-+
       | 	   |  |  |  |  |  |  | 1     | | 1    | | 1
       | 	   +--+--+--+--+--+--+       +-+      +-+
       | 	   |  |  |  |  |  |  | 2     | | 2    | | 2
       | 	   +--+--+--+--+--+--+    *  +-+   =  +-+      for i=0...iel-1
       |           |  |  |  |  |  |  | 3     | | 3    | | 3
       |           +--+--+--+--+--+--+       +-+      +-+
       |           |  |  |  |  |  |  | 4     | | 4    | | 4
       |           +--+--+--+--+--+--+       +-+      +-+
       |           |  |  |  |  |  |  | 5     | | 5    | | 5
       |           +--+--+--+--+--+--+       +-+      +-+
       |                                      |        |
       |                                      |        |
       |                                      derxy2[i]|
       |		                               |
       |		                               chainrulerhs[i]
       |
       |	  yields
       |
       |                      0...iel-1
       |                      +-+-+-+-+
       |                      | | | | | 0 = drdr
       |                      +-+-+-+-+
       |                      | | | | | 1 = dsds
       |                      +-+-+-+-+
       |                      | | | | | 2 = dtdt
       |            derxy2 =  +-+-+-+-+
       |                      | | | | | 3 = drds
       |                      +-+-+-+-+
       |                      | | | | | 4 = drdt
       |                      +-+-+-+-+
       |                      | | | | | 5 = dsdt
       |    	       	      +-+-+-+-+
      */
      // Use LAPACK
      Epetra_LAPACK          solver;

      // a vector specifying the pivots (reordering)
      int pivot[6];

      // error code
      int ierr = 0;

      // Perform LU factorisation --- this call replaces bm with its factorisation
      solver.GETRF(6,6,bm.data(),6,&(pivot[0]),&ierr);

      if (ierr!=0)
      {
        dserror("Unable to perform LU factorisation during computation of derxy2");
      }

      // backward substitution. GETRS replaces the input (chainrulerhs, currently
      // stored on derxy2) with the result
      solver.GETRS('N',6,iel_,bm.data(),6,&(pivot[0]),derxy2_.data(),6,&ierr);

      if (ierr!=0)
      {
        dserror("Unable to perform backward substitution after factorisation of jacobian");
      }
    }
    else
    {
      derxy2_  = 0.;
    }

    //--------------------------------------------------------------
    //            interpolate nodal values to gausspoint
    //--------------------------------------------------------------

    // get intermediate accelerations (n+alpha_M,i) at integration point
    //
    //                 +-----
    //       n+am       \                  n+am
    //    acc    (x) =   +      N (x) * acc
    //                  /        j         j
    //                 +-----
    //                 node j
    //
    // i         : space dimension u/v/w
    //
    accintam_    = blitz::sum(funct_(j)*eaccam(i,j),j);

    // get velocities (n+alpha_F,i) at integration point
    //
    //                 +-----
    //       n+af       \                  n+af
    //    vel    (x) =   +      N (x) * vel
    //                  /        j         j
    //                 +-----
    //                 node j
    //
    velintaf_    = blitz::sum(funct_(j)*evelaf(i,j),j);

    // get velocity (n+alpha_F,i) derivatives at integration point
    //
    //       n+af      +-----  dN (x)
    //   dvel    (x)    \        k         n+af
    //   ----------- =   +     ------ * vel
    //       dx         /        dx        k
    //         j       +-----      j
    //                 node k
    //
    // j : direction of derivative x/y/z
    //
    vderxyaf_ = blitz::sum(derxy_(j,k)*evelaf(i,k),k);

    // calculate 2nd velocity derivatives at integration point, time(n+alpha_F)
    //
    //    2   n+af       +-----   dN (x)
    //   d vel    (x)     \         k          n+af
    //   ------------  =   +     -------- * vel
    //    dx  dx          /      dx  dx        k
    //      j1  j2       +-----    j1  j2
    //                   node k
    //
    // j=(j1,j2) : direction of derivative x/y/z
    if(higher_order_ele)
    {
      vderxy2af_ = blitz::sum(derxy2_(j,k)*evelaf(i,k),k);
    }
    else
    {
      vderxy2af_ = 0.;
    }

    // get bodyforce in gausspoint, time (n+alpha_F)
    //
    //                 +-----
    //       n+af       \                n+af
    //      f    (x) =   +      N (x) * f
    //                  /        j       j
    //                 +-----
    //                 node j
    //
    bodyforceaf_ = blitz::sum(funct_(j)*edeadaf_(i,j),j);

    // get velocities (n+1,i)  at integration point
    //
    //                +-----
    //       n+1       \                  n+1
    //    vel   (x) =   +      N (x) * vel
    //                 /        j         j
    //                +-----
    //                node j
    //
    velintnp_    = blitz::sum(funct_(j)*evelnp(i,j),j);

    // get velocity (n+1,i) derivatives at integration point
    //
    //       n+1      +-----  dN (x)
    //   dvel   (x)    \        k         n+1
    //   ---------- =   +     ------ * vel
    //       dx        /        dx        k
    //         j      +-----      j
    //                node k
    //
    vderxynp_ = blitz::sum(derxy_(j,k)*evelnp(i,k),k);

    // get pressure (n+1,i) at integration point
    //
    //                +-----
    //       n+1       \                  n+1
    //    pre   (x) =   +      N (x) * pre
    //                 /        i         i
    //                +-----
    //                node i
    //
    prenp_    = blitz::sum(funct_*eprenp);

    // get pressure gradient (n+1,i) at integration point
    //
    //       n+1      +-----  dN (x)
    //   dpre   (x)    \        j         n+1
    //   ---------- =   +     ------ * pre
    //       dx        /        dx        j
    //         i      +-----      i
    //                node j
    //
    // i : direction of derivative
    //
    pderxynp_ = blitz::sum(derxy_(i,j)*eprenp(j),j);


    /*--- convective part u_old * grad (funct) --------------------------*/
    /* u_old_x * N,x  +  u_old_y * N,y + u_old_z * N,z
       with  N .. form function matrix                                   */
    conv_c_af_  = blitz::sum(derxy_(j,i)*velintaf_(j), j);

    /*--- reactive part funct * grad (u_old) ----------------------------*/
    /*        /                                     \
              |  u_old_x,x   u_old_x,y   u_old x,z  |
              |                                     |
              |  u_old_y,x   u_old_y,y   u_old_y,z  | * N
              |                                     |
              |  u_old_z,x   u_old_z,y   u_old_z,z  |
              \                                     /
       with  N .. form function matrix                                   */
    conv_r_af_ = vderxyaf_(i, j)*funct_(k);

    /*--- viscous term  - grad * epsilon(u): ----------------------------*/
    /*   /                                                \
         |  2 N_x,xx + N_x,yy + N_y,xy + N_x,zz + N_z,xz  |
       1 |                                                |
     - - |  N_y,xx + N_x,yx + 2 N_y,yy + N_z,yz + N_y,zz  |
       2 |                                                |
         |  N_z,xx + N_x,zx + N_y,zy + N_z,yy + 2 N_z,zz  |
         \                                                /

         with N_x .. x-line of N
         N_y .. y-line of N                                             */

    viscs2_(0,0,_) = 0.5 * (2.0 * derxy2_(0,_) + derxy2_(1,_) + derxy2_(2,_));
    viscs2_(0,1,_) = 0.5 *  derxy2_(3,_);
    viscs2_(0,2,_) = 0.5 *  derxy2_(4,_);
    viscs2_(1,0,_) = 0.5 *  derxy2_(3,_);
    viscs2_(1,1,_) = 0.5 * (derxy2_(0,_) + 2.0 * derxy2_(1,_) + derxy2_(2,_));
    viscs2_(1,2,_) = 0.5 *  derxy2_(5,_);
    viscs2_(2,0,_) = 0.5 *  derxy2_(4,_);
    viscs2_(2,1,_) = 0.5 *  derxy2_(5,_);
    viscs2_(2,2,_) = 0.5 * (derxy2_(0,_) + derxy2_(1,_) + 2.0 * derxy2_(2,_));

    /* divergence new time step n+1 */
    const double divunp          = (vderxynp_(0,0)+vderxynp_(1,1)+vderxynp_(2,2));

    /* Convective term  u_old * grad u_old: */
    convaf_old_ = blitz::sum(vderxyaf_(i, j)*velintaf_(j), j);

    /* Viscous term  div epsilon(u_old) */
    viscaf_old_(0) = vderxy2af_(0,0) + 0.5 * (vderxy2af_(0,1) + vderxy2af_(1,3) + vderxy2af_(0,2) + vderxy2af_(2,4));
    viscaf_old_(1) = vderxy2af_(1,1) + 0.5 * (vderxy2af_(1,0) + vderxy2af_(0,3) + vderxy2af_(1,2) + vderxy2af_(2,5));
    viscaf_old_(2) = vderxy2af_(2,2) + 0.5 * (vderxy2af_(2,0) + vderxy2af_(0,4) + vderxy2af_(2,1) + vderxy2af_(1,5));

    /* compute residual in gausspoint */
    resM_ = accintam_ + convaf_old_ - 2*visc*viscaf_old_ + pderxynp_ - bodyforceaf_;

#if 0
    if(inertia == Fluid3::inertia_stab_keep)
    {
      // INSTATIONARY FLOW PROBLEM, GENERALISED ALPHA, TIME DEPENDENT SUBSCALES
      //
      // tau_M: modification of
      //
      //    Franca, L.P. and Valentin, F.: On an Improved Unusual Stabilized
      //    Finite Element Method for the Advective-Reactive-Diffusive
      //    Equation. Computer Methods in Applied Mechanics and Enginnering,
      //    Vol. 190, pp. 1785-1800, 2000.
      //    http://www.lncc.br/~valentin/publication.htm                   */
      //
      // tau_Mp: modification of Barrenechea, G.R. and Valentin, F.
      //
      //    Barrenechea, G.R. and Valentin, F.: An unusual stabilized finite
      //    element method for a generalized Stokes problem. Numerische
      //    Mathematik, Vol. 92, pp. 652-677, 2002.
      //    http://www.lncc.br/~valentin/publication.htm
      //
      //
      // tau_C: kept Wall definition
      //
      // for the modifications see Codina, Principe, Guasch, Badia
      //    "Time dependent subscales in the stabilized finite  element
      //     approximation of incompressible flow problems"
      //
      //
      // see also: Codina, R. and Soto, O.: Approximation of the incompressible
      //    Navier-Stokes equations using orthogonal subscale stabilisation
      //    and pressure segregation on anisotropic finite element meshes.
      //    Computer methods in Applied Mechanics and Engineering,
      //    Vol 193, pp. 1403-1419, 2004.

      
      // get element type constant for tau
      double mk=0.0;
      switch (distype)
      {
          case tet4: case hex8:
            mk = 0.333333333333333333333;
            break;
          case hex20: case hex27: case tet10:
            mk = 0.083333333333333333333;
            break;
          default:
            dserror("type unknown!\n");
      }

      // get velocity norms
      const double vel_normaf = sqrt(blitz::sum(velintaf_*velintaf_));
      const double vel_normnp = sqrt(blitz::sum(velintnp_*velintnp_));

      //---------------------------------------------- compute tau_Mu = tau_Mp
      /* convective : viscous forces (element reynolds number)*/
      const double re_convectaf = (vel_normaf * hk / visc ) * (mk/2.0);

      double xi_convectaf;


      /*
        A smoothed version of
               
               xi_convect ^
                          |      /
                          |     /
                          |    /
                        1 +---+
                          |
                          |
                          |
                          +--------------> re_convect
                              1
        namely
                             
                          3  1  4
                          - +- x
                          4  4 
                            |
                            |
                            |    
                            |   x
                            |   
               xi_convect ^ |   |
                          | |   |/
                          | |   /
                          | |  /
                          | | +
                     0.75 ++++
                          |
                          |
                          +--------------> re_convect
                              1
      */
      
      if(re_convectaf >= 1.0)
      {
        // evaluation of (smoothed) xi function, here the classical one
        xi_convectaf=re_convectaf;

        // tauM similar to Codina's definition
        tau_(0) = DSQR(hk) / (4.0 * visc / mk + ( 4.0 * visc/mk) * xi_convectaf);

        /* linearisation of inverse tauM

               /  1.0 \            
            d |  ----  |          2.0 
               \ tauM /                          n+af
            ------------ =  ----------------  * u    
            d   u           ||  n+af ||          dim 
                 dim        || u     || * hk
                            ||       ||               
         
        */
        invtauM_lin_ = 2./(vel_normaf*hk)*velintaf_;
      }
      else
      {
        // evaluation of smoothed xi function
        xi_convectaf=0.75+0.25*re_convectaf*re_convectaf*re_convectaf*re_convectaf;


        // same definition of tauM using modified xi_convectaf 
        tau_(0) = DSQR(hk) / (4.0 * visc / mk + ( 4.0 * visc/mk) * xi_convectaf);
        
        /* linearisation of inverse tauM

               /  1.0 \            
            d |  ----  |           3                     2
               \ tauM /     1    mk       2   ||  n+af ||    n+af
            ------------ =  - * ----- * hk  * || u     || * u
            d   u           4       3         ||       ||    dim 
                 dim            visc
         
        */
        
        invtauM_lin_ = (mk*mk*mk*hk*hk*vel_normaf*vel_normaf)/(4*visc*visc*visc)*velintaf_;
      }

      
    /*------------------------------------------------------ compute tau_C ---*/

    //-- stability parameter definition according to Wall Diss. 99
    /*
               xi_convect ^
                          |
                        1 |   +-----------
                          |  /
                          | /
                          |/
                          +--------------> Re_convect
                              1
    */
    const double re_convectnp = (vel_normnp * hk / visc ) * (mk/2.0);

    const double xi_tau_c = DMIN(re_convectnp,1.0);
    
    tau_(2) = vel_normnp * hk * 0.5 * xi_tau_c;

    /*-- stability parameter definition according to Codina (2002), CMAME 191
     *
     * Analysis of a stabilized finite element approximation of the transient
     * convection-diffusion-reaction equation using orthogonal subscales.
     * Ramon Codina, Jordi Blasco; Comput. Visual. Sci., 4 (3): 167-174, 2002.
     *
     * */
    //tau(2) = sqrt(DSQR(visc)+DSQR(0.5*vel_normnp*hk));
    }
#endif

    
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//
//    ELEMENT FORMULATION BASED ON TIME DEPENDENT SUBSCALES
//
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
    if(tds == Fluid3::subscales_time_dependent)
    {
      const double tauM   = tau_(0);
      const double tauC   = tau_(2);


      // update estimates for the subscale quantities

      const double factauC                  = tauC/(tauC+dt);
      const double facMtau                  = 1./(alphaM*tauM+afgdt);

      /*-------------------------------------------------------------------*
       *                                                                   *
       *                  update of SUBSCALE PRESSURE                      *
       *                                                                   *
       *-------------------------------------------------------------------*/

      /*
        ~n+1      tauC     ~n   tauC * dt            n+1
        p    = --------- * p  - --------- * nabla o u
         (i)   tauC + dt        tauC + dt            (i)
      */
      sprenp(iquad)=(spren(iquad)-dt*divunp)*factauC;

      /*-------------------------------------------------------------------*
       *                                                                   *
       *                  update of SUBSCALE VELOCITY                      *
       *                                                                   *
       *-------------------------------------------------------------------*/

      /*
        ~n+1                1.0
        u    = ----------------------------- *
         (i)   alpha_M*tauM+alpha_F*gamma*dt

                +-
                | +-                                  -+   ~n
               *| |alpha_M*tauM +gamma*dt*(alpha_F-1.0)| * u +
                | +-                                  -+
                +-


                    +-                      -+    ~ n
                  + | dt*tauM*(alphaM-gamma) | * acc -
                    +-                      -+

                                           -+
                                       n+1  |
                  - gamma*dt*tauM * res     |
                                       (i)  |
                                           -+
      */
      svelnp(_,iquad)=((alphaM*tauM+gamma*dt*(alphaF-1.0))*sveln(_,iquad)
                       +
                       (dt*tauM*(alphaM-gamma))           *saccn(_,iquad)
                       -
                       (gamma*dt*tauM)                    *resM_(_)
                      )*facMtau;

      /*-------------------------------------------------------------------*
       *                                                                   *
       *               update of intermediate quantities                   *
       *                                                                   *
       *-------------------------------------------------------------------*/

      /* compute the intermediate value of subscale velocity

              ~n+af            ~n+1                   ~n
              u     = alphaF * u     + (1.0-alphaF) * u
               (i)              (i)

      */
      svelaf_(_) = alphaF*svelnp(_,iquad)+(1.0-alphaF)*sveln(_,iquad);
#if 0
      /* compute the intermediate value of subscale acceleration

             ~ n+am    alphaM     / ~n+1   ~n \    gamma - alphaM    ~ n
            acc     = -------- * |  u    - u   | + -------------- * acc
               (i)    gamma*dt    \  (i)      /         gamma

      */
      saccam_(_) = alphaM/(gamma*dt)*(svelnp(_,iquad)-sveln(_,iquad))
                         +
                         (gamma-alphaM)/gamma*saccn(_,iquad);
#endif


      /*
        This is the operator

                  /~n+af         \
                 | u      o nabla |
                  \   (i)        /

                  required for the cross and reynolds stress calculation

      */
      conv_subaf_ =  blitz::sum(svelaf_(j)*derxy_(j,i),j);

      /* Most recent value for subgrid velocity convective term
                  
                  /~n+af         \   n+af
                 | u      o nabla | u
                  \   (i)        /   (i)
      */
        
      convsubaf_old_ = blitz::sum(vderxyaf_(i, j)*svelaf_(j), j);
     
      //--------------------------------------------------------------
      //--------------------------------------------------------------
      //
      //                       SYSTEM MATRIX
      //
      //--------------------------------------------------------------
      //--------------------------------------------------------------
      if(compute_elemat)
      {
        //---------------------------------------------------------------
        //
        //   GALERKIN PART 1 AND SUBSCALE ACCELERATION STABILISATION
        //
        //---------------------------------------------------------------
        if(inertia == Fluid3::inertia_stab_keep)
        {
          const double fac_alphaM_tauM_facMtau                = fac*alphaM*tauM*facMtau;
          const double fac_two_visc_afgdt_alphaM_tauM_facMtau = fac*2.0*visc*afgdt*alphaM*tauM*facMtau;
          const double fac_afgdt_afgdt_facMtau                = fac*afgdt*afgdt*facMtau;
          const double fac_alphaM_afgdt_facMtau               = fac*alphaM*afgdt*facMtau;



          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            const double fac_alphaM_afgdt_facMtau_funct_ui    = fac_alphaM_afgdt_facMtau*funct_(ui);
            const double fac_afgdt_afgdt_facMtau_conv_c_af_ui = fac_afgdt_afgdt_facMtau*conv_c_af_(ui);
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {

              /*
                inertia term (intermediate)

                factor:

                               alphaF*gamma*dt
                 alphaM*---------------------------
                        alphaM*tauM+alphaF*gamma*dt


                            /          \
                           |            |
                           |  Dacc , v  |
                           |            |
                            \          /
              */

              elemat(vi*4    , ui*4    ) += fac_alphaM_afgdt_facMtau_funct_ui*funct_(vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_alphaM_afgdt_facMtau_funct_ui*funct_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_alphaM_afgdt_facMtau_funct_ui*funct_(vi) ;
              /* convection (intermediate)

              factor:

                                     alphaF*gamma*dt
               +alphaF*gamma*dt*---------------------------
                                alphaM*tauM+alphaF*gamma*dt


                          /                          \
                         |  / n+af       \            |
                         | | u    o nabla | Dacc , v  |
                         |  \            /            |
                          \                          /
              */
              elemat(vi*4    , ui*4    ) += fac_afgdt_afgdt_facMtau_conv_c_af_ui*funct_(vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_afgdt_facMtau_conv_c_af_ui*funct_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_afgdt_facMtau_conv_c_af_ui*funct_(vi) ;

              /* pressure (implicit) */

              /*  factor:
                             alphaM*tauM
                    ---------------------------
                    alphaM*tauM+alphaF*gamma*dt

                 /               \
                |                 |
                |  nabla Dp ,  v  |
                |                 |
                 \               /
              */
              elemat(vi*4    , ui*4 + 3) -= fac_alphaM_tauM_facMtau*derxy_(0,ui)*funct_(vi) ;
              elemat(vi*4 + 1, ui*4 + 3) -= fac_alphaM_tauM_facMtau*derxy_(1,ui)*funct_(vi) ;
              elemat(vi*4 + 2, ui*4 + 3) -= fac_alphaM_tauM_facMtau*derxy_(2,ui)*funct_(vi) ;

              /* viscous term (intermediate) */
              /*  factor:
                                       alphaM*tauM
           2*nu*alphaF*gamma*dt*---------------------------
                                alphaM*tauM+alphaF*gamma*dt


                  /                         \
                 |               /    \      |
                 |  nabla o eps | Dacc | , v |
                 |               \    /      |
                  \                         /

              */
              elemat(vi*4    , ui*4    ) += fac_two_visc_afgdt_alphaM_tauM_facMtau*funct_(vi)*viscs2_(0,0,ui);
              elemat(vi*4    , ui*4 + 1) += fac_two_visc_afgdt_alphaM_tauM_facMtau*funct_(vi)*viscs2_(0,1,ui);
              elemat(vi*4    , ui*4 + 2) += fac_two_visc_afgdt_alphaM_tauM_facMtau*funct_(vi)*viscs2_(0,2,ui);
              elemat(vi*4 + 1, ui*4    ) += fac_two_visc_afgdt_alphaM_tauM_facMtau*funct_(vi)*viscs2_(0,1,ui);
              elemat(vi*4 + 1, ui*4 + 1) += fac_two_visc_afgdt_alphaM_tauM_facMtau*funct_(vi)*viscs2_(1,1,ui);
              elemat(vi*4 + 1, ui*4 + 2) += fac_two_visc_afgdt_alphaM_tauM_facMtau*funct_(vi)*viscs2_(1,2,ui);
              elemat(vi*4 + 2, ui*4    ) += fac_two_visc_afgdt_alphaM_tauM_facMtau*funct_(vi)*viscs2_(0,2,ui);
              elemat(vi*4 + 2, ui*4 + 1) += fac_two_visc_afgdt_alphaM_tauM_facMtau*funct_(vi)*viscs2_(1,2,ui);
              elemat(vi*4 + 2, ui*4 + 2) += fac_two_visc_afgdt_alphaM_tauM_facMtau*funct_(vi)*viscs2_(2,2,ui);
            } // end loop rows (test functions for matrix)
          } // end loop rows (solution for matrix, test function for vector)

#if 0
          /* subscale acceleration: */
          /*  factor:               
                                                   
                                    2.0            n+af 
            alphaF*gamma*dt*  ----------------  * u     
                              ||  n+af ||          dim  
                              || u     || * hk          
                              ||       ||




                              
                  /                                 \
                 |  d    /   1  \         ~n+af      |
                 | ---- |  ----  | Dacc * u      , v |
                 | dacc  \ tauM /          (i)       |
                  \                                 /
                                       
          */
          
          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {
              elemat(vi*4    , ui*4    ) -= fac*afgdt*invtauM_lin_(0)*funct_(ui)*svelaf_(0)*funct_(vi);
              elemat(vi*4    , ui*4 + 1) -= fac*afgdt*invtauM_lin_(1)*funct_(ui)*svelaf_(0)*funct_(vi);
              elemat(vi*4    , ui*4 + 2) -= fac*afgdt*invtauM_lin_(2)*funct_(ui)*svelaf_(0)*funct_(vi);
              elemat(vi*4 + 1, ui*4    ) -= fac*afgdt*invtauM_lin_(0)*funct_(ui)*svelaf_(1)*funct_(vi);
              elemat(vi*4 + 1, ui*4 + 1) -= fac*afgdt*invtauM_lin_(1)*funct_(ui)*svelaf_(1)*funct_(vi);
              elemat(vi*4 + 1, ui*4 + 2) -= fac*afgdt*invtauM_lin_(2)*funct_(ui)*svelaf_(1)*funct_(vi);
              elemat(vi*4 + 2, ui*4    ) -= fac*afgdt*invtauM_lin_(0)*funct_(ui)*svelaf_(2)*funct_(vi);
              elemat(vi*4 + 2, ui*4 + 1) -= fac*afgdt*invtauM_lin_(1)*funct_(ui)*svelaf_(2)*funct_(vi);
              elemat(vi*4 + 2, ui*4 + 2) -= fac*afgdt*invtauM_lin_(2)*funct_(ui)*svelaf_(2)*funct_(vi);
            } // end loop rows (test functions for matrix)
          } // end loop rows (solution for matrix, test function for vector)
#endif
          
          if (newton) // if inertia and newton
          {
            for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
            {
              for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
              {
                /* convection (intermediate)

                factor:

                                     alphaF*gamma*dt
               +alphaF*gamma*dt*---------------------------
                                alphaM*tauM+alphaF*gamma*dt

                         /                            \
                        |  /            \   n+af       |
                        | | Dacc o nabla | u      , v  |
                        |  \            /              |
                         \                            /
                */
                elemat(vi*4    , ui*4    ) += fac_afgdt_afgdt_facMtau*funct_(vi)*conv_r_af_(0, 0, ui) ;
                elemat(vi*4    , ui*4 + 1) += fac_afgdt_afgdt_facMtau*funct_(vi)*conv_r_af_(0, 1, ui) ;
                elemat(vi*4    , ui*4 + 2) += fac_afgdt_afgdt_facMtau*funct_(vi)*conv_r_af_(0, 2, ui) ;
                elemat(vi*4 + 1, ui*4    ) += fac_afgdt_afgdt_facMtau*funct_(vi)*conv_r_af_(1, 0, ui) ;
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_afgdt_facMtau*funct_(vi)*conv_r_af_(1, 1, ui) ;
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_afgdt_facMtau*funct_(vi)*conv_r_af_(1, 2, ui) ;
                elemat(vi*4 + 2, ui*4    ) += fac_afgdt_afgdt_facMtau*funct_(vi)*conv_r_af_(2, 0, ui) ;
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_afgdt_facMtau*funct_(vi)*conv_r_af_(2, 1, ui) ;
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_afgdt_facMtau*funct_(vi)*conv_r_af_(2, 2, ui) ;
              } // end loop rows (test functions for matrix)
            } // end loop columns (solution for matrix, test function for vector)
          } // end if inertia and newton
        } //   end if inertia stabilisation
        else
        { // if no inertia stabilisation
          const double fac_alphaM = fac*alphaM;
          const double fac_afgdt  = fac*afgdt;


          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            const double fac_afgdt_conv_c_af_ui = fac_afgdt*conv_c_af_(ui);
            const double fac_alphaM_funct_ui    = fac_alphaM*funct_(ui);
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {

              /*
                inertia term (intermediate)

                factor: +alphaM

                            /          \
                           |            |
                           |  Dacc , v  |
                           |            |
                            \          /
              */
              elemat(vi*4    , ui*4    ) += fac_alphaM_funct_ui*funct_(vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_alphaM_funct_ui*funct_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_alphaM_funct_ui*funct_(vi) ;


              /*  factor:

               +alphaF*gamma*dt

                          /                          \
                         |  / n+af       \            |
                         | | u    o nabla | Dacc , v  |
                         |  \            /            |
                          \                          /
              */
              elemat(vi*4    , ui*4    ) += fac_afgdt_conv_c_af_ui*funct_(vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_conv_c_af_ui*funct_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_conv_c_af_ui*funct_(vi) ;

            } // end loop rows (test functions for matrix)
          } // end loop columns (solution for matrix, test function for vector)

          if (newton) // if no inertia and newton
          {

            for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
            {
              for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
              {

                /*  factor:

                +alphaF*gamma*dt

                         /                            \
                        |  /            \   n+af       |
                        | | Dacc o nabla | u      , v  |
                        |  \            /              |
                         \                            /
                */
                elemat(vi*4    , ui*4    ) += fac_afgdt*funct_(vi)*conv_r_af_(0, 0, ui) ;
                elemat(vi*4    , ui*4 + 1) += fac_afgdt*funct_(vi)*conv_r_af_(0, 1, ui) ;
                elemat(vi*4    , ui*4 + 2) += fac_afgdt*funct_(vi)*conv_r_af_(0, 2, ui) ;
                elemat(vi*4 + 1, ui*4    ) += fac_afgdt*funct_(vi)*conv_r_af_(1, 0, ui) ;
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt*funct_(vi)*conv_r_af_(1, 1, ui) ;
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt*funct_(vi)*conv_r_af_(1, 2, ui) ;
                elemat(vi*4 + 2, ui*4    ) += fac_afgdt*funct_(vi)*conv_r_af_(2, 0, ui) ;
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt*funct_(vi)*conv_r_af_(2, 1, ui) ;
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt*funct_(vi)*conv_r_af_(2, 2, ui) ;

              } // end loop rows (test functions for matrix)
            } // end loop columns (solution for matrix, test function for vector)
          } // end if no inertia and newton
        } // end if no inertia stabilisation


        const double fac_afgdt_visc           = fac*visc*afgdt;
        const double fac_gamma_dt             = fac*gamma*dt;

        for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
        {
          const double fac_funct_ui=fac*funct_(ui);

          for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
          {
            //---------------------------------------------------------------
            //
            //   GALERKIN PART 2 (REMAINING EXPRESSIONS)
            //
            //---------------------------------------------------------------
            /* pressure (implicit) */

            /*  factor: -1

                 /                \
                |                  |
                |  Dp , nabla o v  |
                |                  |
                 \                /
            */

            elemat(vi*4    , ui*4 + 3) -= fac_funct_ui*derxy_(0, vi) ;
            elemat(vi*4 + 1, ui*4 + 3) -= fac_funct_ui*derxy_(1, vi) ;
            elemat(vi*4 + 2, ui*4 + 3) -= fac_funct_ui*derxy_(2, vi) ;

            /* viscous term (intermediate) */

            /*  factor: +2*nu*alphaF*gamma*dt

                 /                          \
                |       /    \         / \   |
                |  eps | Dacc | , eps | v |  |
                |       \    /         \ /   |
                 \                          /
            */

            elemat(vi*4    , ui*4    ) += fac_afgdt_visc*(2.0*derxy_(0,ui)*derxy_(0,vi)
                                                          +
                                                          derxy_(1,ui)*derxy_(1,vi)
                                                          +
                                                          derxy_(2,ui)*derxy_(2,vi)) ;
            elemat(vi*4    , ui*4 + 1) += fac_afgdt_visc*derxy_(0,ui)*derxy_(1,vi) ;
            elemat(vi*4    , ui*4 + 2) += fac_afgdt_visc*derxy_(0,ui)*derxy_(2,vi) ;
            elemat(vi*4 + 1, ui*4    ) += fac_afgdt_visc*derxy_(1,ui)*derxy_(0,vi) ;
            elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_visc*(derxy_(0,ui)*derxy_(0,vi)
                                                          +
                                                          2.0*derxy_(1,ui)*derxy_(1,vi)
                                                          +
                                                          derxy_(2,ui)*derxy_(2,vi)) ;
            elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_visc*derxy_(1,ui)*derxy_(2,vi) ;
            elemat(vi*4 + 2, ui*4    ) += fac_afgdt_visc*derxy_(2,ui)*derxy_(0,vi) ;
            elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_visc*derxy_(2,ui)*derxy_(1,vi) ;
            elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_visc*(derxy_(0,ui)*derxy_(0,vi)
                                                          +
                                                          derxy_(1,ui)*derxy_(1,vi)
                                                          +
                                                          2.0*derxy_(2,ui)*derxy_(2,vi)) ;

            /* continuity equation (implicit) */

            /*  factor: +gamma*dt

                 /                  \
                |                    |
                | nabla o Dacc  , q  |
                |                    |
                 \                  /
            */

            elemat(vi*4 + 3, ui*4    ) += fac_gamma_dt*derxy_(0,ui)*funct_(vi) ;
            elemat(vi*4 + 3, ui*4 + 1) += fac_gamma_dt*derxy_(1,ui)*funct_(vi) ;
            elemat(vi*4 + 3, ui*4 + 2) += fac_gamma_dt*derxy_(2,ui)*funct_(vi) ;


          } // end loop rows (test functions for matrix)
        } // end loop columns (solution for matrix, test function for vector)
        // end remaining Galerkin terms


        if(pspg == Fluid3::pstab_use_pspg)
        {
          //---------------------------------------------------------------
          //
          //                     STABILISATION PART
          //                    PRESSURE STABILISATION
          //
          //---------------------------------------------------------------

          const double fac_gamma_dt_tauM_facMtau        = fac*gamma*dt*tauM*facMtau;

          const double fac_two_visc_afgdt_gamma_dt_tauM_facMtau = fac*2.0*visc*afgdt*gamma*dt*tauM*facMtau;

          const double fac_afgdt_gamma_dt_tauM_facMtau  = fac*afgdt*gamma*dt*tauM*facMtau;

          const double fac_alphaM_gamma_dt_tauM_facMtau = fac*alphaM*gamma*dt*tauM*facMtau;

          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            const double fac_alphaM_gamma_dt_tauM_facMtau_funct_ui   =fac_alphaM_gamma_dt_tauM_facMtau*funct_(ui);
            const double fac_afgdt_gamma_dt_tauM_facMtau_conv_c_af_ui=fac_afgdt_gamma_dt_tauM_facMtau*conv_c_af_(ui);
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {
              /* pressure stabilisation --- inertia    */

              /*
                           gamma*dt*tau_M
            factor:  ------------------------------ * alpha_M
                     alpha_M*tau_M+alpha_F*gamma*dt


                                /                \
                               |                  |
                               |  Dacc , nabla q  |
                               |                  |
                                \                /
              */
              elemat(vi*4 + 3, ui*4    ) += fac_alphaM_gamma_dt_tauM_facMtau_funct_ui*derxy_(0,vi) ;
              elemat(vi*4 + 3, ui*4 + 1) += fac_alphaM_gamma_dt_tauM_facMtau_funct_ui*derxy_(1,vi) ;
              elemat(vi*4 + 3, ui*4 + 2) += fac_alphaM_gamma_dt_tauM_facMtau_funct_ui*derxy_(2,vi) ;

              /* pressure stabilisation --- convection */

              /*
                           gamma*dt*tau_M
            factor:  ------------------------------ * alpha_F*gamma*dt
                     alpha_M*tau_M+alpha_F*gamma*dt


                        /                                \
                       |  / n+af       \                  |
                       | | u    o nabla | Dacc , nabla q  |
                       |  \            /                  |
                        \                                /


                       /                                  \
                      |  /            \   n+af             |
                      | | Dacc o nabla | u      , nabla q  |
                      |  \            /                    |
                       \                                  /

              */
              elemat(vi*4 + 3, ui*4    ) += fac_afgdt_gamma_dt_tauM_facMtau_conv_c_af_ui*derxy_(0,vi) ;
              elemat(vi*4 + 3, ui*4 + 1) += fac_afgdt_gamma_dt_tauM_facMtau_conv_c_af_ui*derxy_(1,vi) ;
              elemat(vi*4 + 3, ui*4 + 2) += fac_afgdt_gamma_dt_tauM_facMtau_conv_c_af_ui*derxy_(2,vi) ;

              /* pressure stabilisation --- diffusion  */


            /*
                           gamma*dt*tau_M
            factor:  ------------------------------ * alpha_F*gamma*dt * 2 * nu
                     alpha_M*tau_M+alpha_F*gamma*dt


                    /                                \
                   |               /    \             |
                   |  nabla o eps | Dacc | , nabla q  |
                   |               \    /             |
                    \                                /
            */

              elemat(vi*4 + 3, ui*4    ) -= fac_two_visc_afgdt_gamma_dt_tauM_facMtau*
                                            (derxy_(0,vi)*viscs2_(0,0,ui)
                                             +
                                             derxy_(1,vi)*viscs2_(0,1,ui)
                                             +
                                             derxy_(2,vi)*viscs2_(0,2,ui)) ;
              elemat(vi*4 + 3, ui*4 + 1) -= fac_two_visc_afgdt_gamma_dt_tauM_facMtau*
                                            (derxy_(0,vi)*viscs2_(0,1,ui)
                                             +
                                             derxy_(1,vi)*viscs2_(1,1,ui)
                                             +
                                             derxy_(2,vi)*viscs2_(1,2,ui)) ;
              elemat(vi*4 + 3, ui*4 + 2) -= fac_two_visc_afgdt_gamma_dt_tauM_facMtau*
                                            (derxy_(0,vi)*viscs2_(0,2,ui)
                                             +
                                             derxy_(1,vi)*viscs2_(1,2,ui)
                                             +
                                             derxy_(2,vi)*viscs2_(2,2,ui)) ;

              /* pressure stabilisation --- pressure   */

              /*
                          gamma*dt*tau_M
            factor:  ------------------------------
                     alpha_M*tau_M+alpha_F*gamma*dt



                    /                    \
                   |                      |
                   |  nabla Dp , nabla q  |
                   |                      |
                    \                    /
              */

              elemat(vi*4 + 3, ui*4 + 3) += fac_gamma_dt_tauM_facMtau*
                                            (derxy_(0,ui)*derxy_(0,vi)
                                             +
                                             derxy_(1,ui)*derxy_(1,vi)
                                             +
                                             derxy_(2,ui)*derxy_(2,vi)) ;

            } // end loop rows (test functions for matrix)
          } // end loop columns (solution for matrix, test function for vector)

          if (newton) // if pspg and newton
          {

            for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
            {
              for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
              {
                /* pressure stabilisation --- convection */

                /*
                                gamma*dt*tau_M
                factor:  ------------------------------ * alpha_F*gamma*dt
                         alpha_M*tau_M+alpha_F*gamma*dt

                       /                                  \
                      |  /            \   n+af             |
                      | | Dacc o nabla | u      , nabla q  |
                      |  \            /                    |
                       \                                  /

                */

                elemat(vi*4 + 3, ui*4    ) += fac_afgdt_gamma_dt_tauM_facMtau*
                                              (derxy_(0,vi)*conv_r_af_(0,0,ui)
                                               +
                                               derxy_(1,vi)*conv_r_af_(1,0,ui)
                                               +
                                               derxy_(2,vi)*conv_r_af_(2,0,ui)) ;
                elemat(vi*4 + 3, ui*4 + 1) += fac_afgdt_gamma_dt_tauM_facMtau*
                                              (derxy_(0,vi)*conv_r_af_(0,1,ui)
                                               +
                                               derxy_(1,vi)*conv_r_af_(1,1,ui)
                                               +
                                               derxy_(2,vi)*conv_r_af_(2,1,ui)) ;
                elemat(vi*4 + 3, ui*4 + 2) += fac_afgdt_gamma_dt_tauM_facMtau*
                                              (derxy_(0,vi)*conv_r_af_(0,2,ui)
                                               +
                                               derxy_(1,vi)*conv_r_af_(1,2,ui)
                                               +
                                               derxy_(2,vi)*conv_r_af_(2,2,ui)) ;
              } // end loop rows (test functions for matrix)
            } // end loop columns (solution for matrix, test function for vector)
          }// end if pspg and newton
        } // end pressure stabilisation

        if(supg == Fluid3::convective_stab_supg)
        {
          const double fac_alphaM_afgdt_tauM_facMtau         = fac*alphaM*afgdt*facMtau*tauM;
          const double fac_afgdt_tauM_afgdt_facMtau          = fac*afgdt*afgdt*facMtau*tauM;
          const double fac_afgdt_tauM_facMtau                = fac*afgdt*tauM*facMtau;
          const double fac_two_visc_afgdt_afgdt_tauM_facMtau = fac*2.0*visc*afgdt*afgdt*tauM*facMtau;

          //---------------------------------------------------------------
          //
          //                     STABILISATION PART
          //         SUPG STABILISATION FOR CONVECTION DOMINATED FLOWS
          //
          //---------------------------------------------------------------
          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            const double fac_alphaM_afgdt_tauM_facMtau_funct_ui    = fac_alphaM_afgdt_tauM_facMtau*funct_(ui);
            const double fac_afgdt_tauM_afgdt_facMtau_conv_c_af_ui = fac_afgdt_tauM_afgdt_facMtau*conv_c_af_(ui);
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {
              /* SUPG stabilisation --- inertia

               factor:
                           alphaF*gamma*dt*tauM
                        --------------------------- * alphaM
                        alphaM*tauM+alphaF*gamma*dt


                    /                           \
                   |          / n+af       \     |
                   |  Dacc , | u    o nabla | v  |
                   |          \            /     |
                    \                           /
              */
              elemat(vi*4    , ui*4    ) += fac_alphaM_afgdt_tauM_facMtau_funct_ui*conv_c_af_(vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_alphaM_afgdt_tauM_facMtau_funct_ui*conv_c_af_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_alphaM_afgdt_tauM_facMtau_funct_ui*conv_c_af_(vi) ;

              /* SUPG stabilisation --- convection


               factor:
                           alphaF*gamma*dt*tauM
                        --------------------------- * alphaF * gamma * dt
                        alphaM*tauM+alphaF*gamma*dt

                    /                                               \
                   |    / n+af        \          / n+af        \     |
                   |   | u     o nabla | Dacc , | u     o nabla | v  |
                   |    \             /          \             /     |
                    \                                               /
              */

              elemat(vi*4, ui*4)         += fac_afgdt_tauM_afgdt_facMtau_conv_c_af_ui*conv_c_af_(vi);
              elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau_conv_c_af_ui*conv_c_af_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau_conv_c_af_ui*conv_c_af_(vi) ;

              /* SUPG stabilisation --- diffusion

               factor:
                               alphaF*gamma*tauM*dt
                  - 2 * nu  --------------------------- * alphaF * gamma * dt
                            alphaM*tauM+alphaF*gamma*dt


                    /                                            \
                   |               /     \    / n+af        \     |
                   |  nabla o eps | Dacc  |, | u     o nabla | v  |
                   |               \     /    \             /     |
                    \                                            /
              */
              elemat(vi*4, ui*4)         -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscs2_(0, 0, ui)*conv_c_af_(vi) ;
              elemat(vi*4, ui*4 + 1)     -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscs2_(0, 1, ui)*conv_c_af_(vi) ;
              elemat(vi*4, ui*4 + 2)     -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscs2_(0, 2, ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 1, ui*4)     -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscs2_(0, 1, ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 1, ui*4 + 1) -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscs2_(1, 1, ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 1, ui*4 + 2) -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscs2_(1, 2, ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 2, ui*4)     -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscs2_(0, 2, ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 2, ui*4 + 1) -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscs2_(1, 2, ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscs2_(2, 2, ui)*conv_c_af_(vi) ;

              /* SUPG stabilisation --- pressure

               factor:
                               alphaF*gamma*tauM*dt
                            ---------------------------
                            alphaM*tauM+alphaF*gamma*dt


                    /                               \
                   |              / n+af       \     |
                   |  nabla Dp , | u    o nabla | v  |
                   |              \            /     |
                    \                               /
              */

              elemat(vi*4    , ui*4 + 3) += fac_afgdt_tauM_facMtau*derxy_(0,ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 1, ui*4 + 3) += fac_afgdt_tauM_facMtau*derxy_(1,ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 2, ui*4 + 3) += fac_afgdt_tauM_facMtau*derxy_(2,ui)*conv_c_af_(vi) ;

            } // end loop rows (test functions for matrix)
          } // end loop columns (solution for matrix, test function for vector)

          if (newton)
          {
            const double fac_afgdt_svelaf_x               = fac*afgdt*svelaf_(0);
            const double fac_afgdt_svelaf_y               = fac*afgdt*svelaf_(1);
            const double fac_afgdt_svelaf_z               = fac*afgdt*svelaf_(2);


            for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
            {
              for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
              {



                /* SUPG stabilisation --- convection


               factor:
                           alphaF*gamma*dt*tauM
                        --------------------------- * alphaF * gamma * dt
                        alphaM*tauM+alphaF*gamma*dt

                    /                                               \
                   |    /            \   n+af    / n+af        \     |
                   |   | Dacc o nabla | u     , | u     o nabla | v  |
                   |    \            /           \             /     |
                    \                                               /

                */
                elemat(vi*4, ui*4)         += fac_afgdt_tauM_afgdt_facMtau*
                                              (conv_c_af_(vi)*conv_r_af_(0, 0, ui)) ;
                elemat(vi*4, ui*4 + 1)     += fac_afgdt_tauM_afgdt_facMtau*
                                              (conv_c_af_(vi)*conv_r_af_(0, 1, ui)) ;
                elemat(vi*4, ui*4 + 2)     += fac_afgdt_tauM_afgdt_facMtau*
                                              (conv_c_af_(vi)*conv_r_af_(0, 2, ui)) ;
                elemat(vi*4 + 1, ui*4)     += fac_afgdt_tauM_afgdt_facMtau*
                                              (conv_c_af_(vi)*conv_r_af_(1, 0, ui)) ;
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*
                                              (conv_c_af_(vi)*conv_r_af_(1, 1, ui)) ;
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*
                                              (conv_c_af_(vi)*conv_r_af_(1, 2, ui)) ;
                elemat(vi*4 + 2, ui*4)     += fac_afgdt_tauM_afgdt_facMtau*
                                              (conv_c_af_(vi)*conv_r_af_(2, 0, ui)) ;
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*
                                              (conv_c_af_(vi)*conv_r_af_(2, 1, ui)) ;
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*
                                              (conv_c_af_(vi)*conv_r_af_(2, 2, ui)) ;

                /* SUPG stabilisation --- subscale velocity, nonlinear part from testfunction

                factor:
                          alphaF * gamma * dt


                    /                            \
                   |  ~n+af    /            \     |
                   |  u     , | Dacc o nabla | v  |
                   |   (i)     \            /     |
                    \                            /

                */

                elemat(vi*4    , ui*4)     -= fac_afgdt_svelaf_x*funct_(ui)*derxy_(0,vi) ;
                elemat(vi*4    , ui*4 + 1) -= fac_afgdt_svelaf_x*funct_(ui)*derxy_(1,vi) ;
                elemat(vi*4    , ui*4 + 2) -= fac_afgdt_svelaf_x*funct_(ui)*derxy_(2,vi) ;
                elemat(vi*4 + 1, ui*4)     -= fac_afgdt_svelaf_y*funct_(ui)*derxy_(0,vi) ;
                elemat(vi*4 + 1, ui*4 + 1) -= fac_afgdt_svelaf_y*funct_(ui)*derxy_(1,vi) ;
                elemat(vi*4 + 1, ui*4 + 2) -= fac_afgdt_svelaf_y*funct_(ui)*derxy_(2,vi) ;
                elemat(vi*4 + 2, ui*4)     -= fac_afgdt_svelaf_z*funct_(ui)*derxy_(0,vi) ;
                elemat(vi*4 + 2, ui*4 + 1) -= fac_afgdt_svelaf_z*funct_(ui)*derxy_(1,vi) ;
                elemat(vi*4 + 2, ui*4 + 2) -= fac_afgdt_svelaf_z*funct_(ui)*derxy_(2,vi) ;



#if 0
                /* SUPG stabilisation --- inertia, lineariation of testfunction

                factor:
                           alphaF*gamma*dt*tauM
                        --------------------------- * alphaF * gamma * dt
                        alphaM*tauM+alphaF*gamma*dt

                    /                               \
                   |     n+am     /            \     |
                   |  acc      , | Dacc o nabla | v  |
                   |              \            /     |
                    \                               /
                */

                elemat(vi*4    , ui*4    ) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*accintam_(0)*derxy_(0,vi) ;
                elemat(vi*4    , ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*accintam_(0)*derxy_(1,vi) ;
                elemat(vi*4    , ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*accintam_(0)*derxy_(2,vi) ;
                elemat(vi*4 + 1, ui*4    ) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*accintam_(1)*derxy_(0,vi) ;
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*accintam_(1)*derxy_(1,vi) ;
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*accintam_(1)*derxy_(2,vi) ;
                elemat(vi*4 + 2, ui*4)     += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*accintam_(2)*derxy_(0,vi) ;
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*accintam_(2)*derxy_(1,vi) ;
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*accintam_(2)*derxy_(2,vi) ;
#endif


#if 0
                /* SUPG stabilisation --- convection, lineariation of testfunction

                factor:
                           alphaF*gamma*dt*tauM
                        --------------------------- * alphaF * gamma * dt
                        alphaM*tauM+alphaF*gamma*dt

                    /                                               \
                   |    / n+af        \   n+af    /            \     |
                   |   | u     o nabla | u     , | Dacc o nabla | v  |
                   |    \             /           \            /     |
                    \                                               /
                */

                elemat(vi*4    , ui*4    ) += fac_afgdt_tauM_afgdt_facMtau*convaf_old_(0)*funct_(ui)*derxy_(0,vi);
                elemat(vi*4    , ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*convaf_old_(0)*funct_(ui)*derxy_(1,vi);
                elemat(vi*4    , ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*convaf_old_(0)*funct_(ui)*derxy_(2,vi);
                elemat(vi*4 + 1, ui*4    ) += fac_afgdt_tauM_afgdt_facMtau*convaf_old_(1)*funct_(ui)*derxy_(0,vi);
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*convaf_old_(1)*funct_(ui)*derxy_(1,vi);
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*convaf_old_(1)*funct_(ui)*derxy_(2,vi);
                elemat(vi*4 + 2, ui*4    ) += fac_afgdt_tauM_afgdt_facMtau*convaf_old_(2)*funct_(ui)*derxy_(0,vi);
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*convaf_old_(2)*funct_(ui)*derxy_(1,vi);
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*convaf_old_(2)*funct_(ui)*derxy_(2,vi);
#endif



#if 0
                /* SUPG stabilisation ---  pressure, lineariation of testfunction

                factor:
                           alphaF*gamma*dt*tauM
                        --------------------------- * alphaF * gamma * dt
                        alphaM*tauM+alphaF*gamma*dt

                    /                                 \
                   |         n+1    /            \     |
                   |  nabla p    , | Dacc o nabla | v  |
                   |                \            /     |
                    \                                 /
                */

                elemat(vi*4    , ui*4    ) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*pderxynp_(0)*derxy_(0,vi) ;
                elemat(vi*4    , ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*pderxynp_(0)*derxy_(1,vi) ;
                elemat(vi*4    , ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*pderxynp_(0)*derxy_(2,vi) ;
                elemat(vi*4 + 1, ui*4    ) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*pderxynp_(1)*derxy_(0,vi) ;
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*pderxynp_(1)*derxy_(1,vi) ;
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*pderxynp_(1)*derxy_(2,vi) ;
                elemat(vi*4 + 2, ui*4    ) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*pderxynp_(2)*derxy_(0,vi) ;
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*pderxynp_(2)*derxy_(1,vi) ;
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_tauM_afgdt_facMtau*funct_(ui)*pderxynp_(2)*derxy_(2,vi) ;
#endif

#if 0
                /* SUPG stabilisation --- diffusion, lineariation of testfunction
               factor:
                               alphaF*gamma*tauM*dt
                  - 2 * nu  --------------------------- * alphaF * gamma * dt
                            alphaM*tauM+alphaF*gamma*dt

                    /                                            \
                   |               / n+af \    /            \     |
                   |  nabla o eps | u      |, | Dacc o nabla | v  |
                   |               \      /    \            /     |
                    \                                            /
                */
                elemat(vi*4, ui*4)         -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscaf_old_(0)*funct_(ui)*derxy_(0, vi) ;
                elemat(vi*4, ui*4 + 1)     -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscaf_old_(0)*funct_(ui)*derxy_(1, vi) ;
                elemat(vi*4, ui*4 + 2)     -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscaf_old_(0)*funct_(ui)*derxy_(2, vi) ;
                elemat(vi*4 + 1, ui*4)     -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscaf_old_(1)*funct_(ui)*derxy_(0, vi) ;
                elemat(vi*4 + 1, ui*4 + 1) -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscaf_old_(1)*funct_(ui)*derxy_(1, vi) ;
                elemat(vi*4 + 1, ui*4 + 2) -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscaf_old_(1)*funct_(ui)*derxy_(2, vi) ;
                elemat(vi*4 + 2, ui*4)     -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscaf_old_(2)*funct_(ui)*derxy_(0, vi) ;
                elemat(vi*4 + 2, ui*4 + 1) -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscaf_old_(2)*funct_(ui)*derxy_(1, vi) ;
                elemat(vi*4 + 2, ui*4 + 2) -= fac_two_visc_afgdt_afgdt_tauM_facMtau*viscaf_old_(2)*funct_(ui)*derxy_(2, vi) ;

#endif
              } // end loop rows (test functions for matrix)
            } // end loop columns (solution for matrix, test function for vector)
          } // end if newton and supg
        } // end supg stabilisation


        if(agls == Fluid3::viscous_stab_agls)
        {

          const double fac_alphaM_two_visc_afgdt_tauM_facMtau      = fac*alphaM*2.0*visc*afgdt*tauM*facMtau;
          const double fac_afgdt_two_visc_afgdt_tauM_facMtau       = fac*afgdt*2.0*visc*afgdt*tauM*facMtau;
          const double fac_afgdt_four_visc_visc_afgdt_tauM_facMtau = fac*afgdt*4.0*visc*visc*afgdt*tauM*facMtau;
          const double fac_two_visc_afgdt_tauM_facMtau             = fac*2.0*visc*afgdt*tauM*facMtau;

          //---------------------------------------------------------------
          //
          //                     STABILISATION PART
          //            VISCOUS STABILISATION TERMS FOR AGLS
          //
          //---------------------------------------------------------------
          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            const double fac_alphaM_two_visc_afgdt_tauM_facMtau_funct_ui   = fac_alphaM_two_visc_afgdt_tauM_facMtau*funct_(ui);
            const double fac_afgdt_two_visc_afgdt_tauM_facMtau_conv_c_af_ui= fac_afgdt_two_visc_afgdt_tauM_facMtau*conv_c_af_(ui);
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {
              /* viscous stabilisation --- inertia     */

              /* factor:

                             alphaF*gamma*tauM*dt
            alphaM*2*nu* ---------------------------
                         alphaM*tauM+alphaF*gamma*dt

                     /                    \
                    |                      |
                    |  Dacc , div eps (v)  |
                    |                      |
                     \                    /
              */
              elemat(vi*4    , ui*4    ) += fac_alphaM_two_visc_afgdt_tauM_facMtau_funct_ui*viscs2_(0,0,vi);
              elemat(vi*4    , ui*4 + 1) += fac_alphaM_two_visc_afgdt_tauM_facMtau_funct_ui*viscs2_(0,1,vi);
              elemat(vi*4    , ui*4 + 2) += fac_alphaM_two_visc_afgdt_tauM_facMtau_funct_ui*viscs2_(0,2,vi);
              elemat(vi*4 + 1, ui*4    ) += fac_alphaM_two_visc_afgdt_tauM_facMtau_funct_ui*viscs2_(0,1,vi);
              elemat(vi*4 + 1, ui*4 + 1) += fac_alphaM_two_visc_afgdt_tauM_facMtau_funct_ui*viscs2_(1,1,vi);
              elemat(vi*4 + 1, ui*4 + 2) += fac_alphaM_two_visc_afgdt_tauM_facMtau_funct_ui*viscs2_(1,2,vi);
              elemat(vi*4 + 2, ui*4    ) += fac_alphaM_two_visc_afgdt_tauM_facMtau_funct_ui*viscs2_(0,2,vi);
              elemat(vi*4 + 2, ui*4 + 1) += fac_alphaM_two_visc_afgdt_tauM_facMtau_funct_ui*viscs2_(1,2,vi);
              elemat(vi*4 + 2, ui*4 + 2) += fac_alphaM_two_visc_afgdt_tauM_facMtau_funct_ui*viscs2_(2,2,vi);

              /* viscous stabilisation --- convection */
              /*  factor:
                                         alphaF*gamma*dt*tauM
               -alphaF*gamma*dt*2*nu* ---------------------------
                                      alphaM*tauM+alphaF*gamma*dt

                       /                                  \
                      |  / n+af       \                    |
                      | | u    o nabla | Dacc, div eps (v) |
                      |  \            /                    |
                       \                                  /

              */

              elemat(vi*4    , ui*4    ) += fac_afgdt_two_visc_afgdt_tauM_facMtau_conv_c_af_ui*viscs2_(0, 0, vi) ;
              elemat(vi*4    , ui*4 + 1) += fac_afgdt_two_visc_afgdt_tauM_facMtau_conv_c_af_ui*viscs2_(0, 1, vi) ;
              elemat(vi*4    , ui*4 + 2) += fac_afgdt_two_visc_afgdt_tauM_facMtau_conv_c_af_ui*viscs2_(0, 2, vi) ;
              elemat(vi*4 + 1, ui*4    ) += fac_afgdt_two_visc_afgdt_tauM_facMtau_conv_c_af_ui*viscs2_(0, 1, vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_two_visc_afgdt_tauM_facMtau_conv_c_af_ui*viscs2_(1, 1, vi) ;
              elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_two_visc_afgdt_tauM_facMtau_conv_c_af_ui*viscs2_(1, 2, vi) ;
              elemat(vi*4 + 2, ui*4    ) += fac_afgdt_two_visc_afgdt_tauM_facMtau_conv_c_af_ui*viscs2_(0, 2, vi) ;
              elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_two_visc_afgdt_tauM_facMtau_conv_c_af_ui*viscs2_(1, 2, vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_two_visc_afgdt_tauM_facMtau_conv_c_af_ui*viscs2_(2, 2, vi) ;

              /* viscous stabilisation --- diffusion  */

              /* factor:

                                           alphaF*gamma*tauM*dt
               +alphaF*gamma*dt*4*nu*nu ---------------------------
                                        alphaM*tauM+alphaF*gamma*dt

                    /                                   \
                   |               /    \                |
                   |  nabla o eps | Dacc | , div eps (v) |
                   |               \    /                |
                    \                                   /
              */
              elemat(vi*4    , ui*4    ) -= fac_afgdt_four_visc_visc_afgdt_tauM_facMtau*
                                            (viscs2_(0, 0, ui)*viscs2_(0, 0, vi)
                                             +
                                             viscs2_(0, 1, ui)*viscs2_(0, 1, vi)
                                             +
                                             viscs2_(0, 2, ui)*viscs2_(0, 2, vi)) ;
              elemat(vi*4    , ui*4 + 1) -= fac_afgdt_four_visc_visc_afgdt_tauM_facMtau*
                                            (viscs2_(0, 0, vi)*viscs2_(0, 1, ui)
                                             +
                                             viscs2_(0, 1, vi)*viscs2_(1, 1, ui)
                                             +
                                             viscs2_(0, 2, vi)*viscs2_(1, 2, ui)) ;
              elemat(vi*4    , ui*4 + 2) -= fac_afgdt_four_visc_visc_afgdt_tauM_facMtau*
                                            (viscs2_(0, 0, vi)*viscs2_(0, 2, ui)
                                             +
                                             viscs2_(0, 1, vi)*viscs2_(1, 2, ui)
                                             +
                                             viscs2_(0, 2, vi)*viscs2_(2, 2, ui)) ;
              elemat(vi*4 + 1, ui*4    ) -= fac_afgdt_four_visc_visc_afgdt_tauM_facMtau*
                                            (viscs2_(0, 0, ui)*viscs2_(0, 1, vi)
                                             +
                                             viscs2_(0, 1, ui)*viscs2_(1, 1, vi)
                                             +
                                             viscs2_(0, 2, ui)*viscs2_(1, 2, vi)) ;
              elemat(vi*4 + 1, ui*4 + 1) -= fac_afgdt_four_visc_visc_afgdt_tauM_facMtau*
                                            (viscs2_(0, 1, ui)*viscs2_(0, 1, vi)
                                             +
                                             viscs2_(1, 1, ui)*viscs2_(1, 1, vi)
                                             +
                                             viscs2_(1, 2, ui)*viscs2_(1, 2, vi)) ;
              elemat(vi*4 + 1, ui*4 + 2) -= fac_afgdt_four_visc_visc_afgdt_tauM_facMtau*
                                            (viscs2_(0, 1, vi)*viscs2_(0, 2, ui)
                                             +
                                             viscs2_(1, 1, vi)*viscs2_(1, 2, ui)
                                             +
                                             viscs2_(1, 2, vi)*viscs2_(2, 2, ui)) ;
              elemat(vi*4 + 2, ui*4    ) -= fac_afgdt_four_visc_visc_afgdt_tauM_facMtau*
                                            (viscs2_(0, 0, ui)*viscs2_(0, 2, vi)
                                             +
                                             viscs2_(0, 1, ui)*viscs2_(1, 2, vi)
                                             +
                                             viscs2_(0, 2, ui)*viscs2_(2, 2, vi)) ;
              elemat(vi*4 + 2, ui*4 + 1) -= fac_afgdt_four_visc_visc_afgdt_tauM_facMtau*
                                            (viscs2_(0, 1, ui)*viscs2_(0, 2, vi)
                                             +
                                             viscs2_(1, 1, ui)*viscs2_(1, 2, vi)
                                             +
                                             viscs2_(1, 2, ui)*viscs2_(2, 2, vi)) ;
              elemat(vi*4 + 2, ui*4 + 2) -= fac_afgdt_four_visc_visc_afgdt_tauM_facMtau*
                                            (viscs2_(0, 2, ui)*viscs2_(0, 2, vi)
                                             +
                                             viscs2_(1, 2, ui)*viscs2_(1, 2, vi)
                                             +
                                             viscs2_(2, 2, ui)*viscs2_(2, 2, vi)) ;

              /* viscous stabilisation --- pressure   */

              /* factor:

                          alphaF*gamma*tauM*dt
               -2*nu * ---------------------------
                       alphaM*tauM+alphaF*gamma*dt


                    /                        \
                   |                          |
                   |  nabla Dp , div eps (v)  |
                   |                          |
                    \                        /
              */
              elemat(vi*4    , ui*4 + 3) += fac_two_visc_afgdt_tauM_facMtau*
                                            (derxy_(0,ui)*viscs2_(0,0,vi)
                                             +
                                             derxy_(1,ui)*viscs2_(0,1,vi)
                                             +
                                             derxy_(2,ui)*viscs2_(0,2,vi)) ;
              elemat(vi*4 + 1, ui*4 + 3) += fac_two_visc_afgdt_tauM_facMtau*
                                            (derxy_(0,ui)*viscs2_(0,1,vi)
                                             +
                                             derxy_(1,ui)*viscs2_(1,1,vi)
                                             +
                                             derxy_(2,ui)*viscs2_(1,2,vi)) ;
              elemat(vi*4 + 2, ui*4 + 3) += fac_two_visc_afgdt_tauM_facMtau*
                                            (derxy_(0,ui)*viscs2_(0,2,vi)
                                             +
                                             derxy_(1,ui)*viscs2_(1,2,vi)
                                             +
                                             derxy_(2,ui)*viscs2_(2,2,vi)) ;
            } // end loop rows (test functions for matrix)
          } // end loop columns (solution for matrix, test function for vector)

          if (newton)
          {
            for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
            {
              for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
              {

                /* viscous stabilisation --- convection
                   factor:
                                         alphaF*gamma*dt*tauM
               -alphaF*gamma*dt*2*nu* ---------------------------
                                      alphaM*tauM+alphaF*gamma*dt

                     /                                     \
                    |   /            \   n+af               |
                    |  | Dacc o nabla | u     , div eps (v) |
                    |   \            /                      |
                     \                                     /


                */


                elemat(vi*4     , ui*4    )+= fac_afgdt_two_visc_afgdt_tauM_facMtau*
                                              (viscs2_(0, 0, vi)*conv_r_af_(0, 0, ui)
                                               +
                                               viscs2_(0, 1, vi)*conv_r_af_(1, 0, ui)
                                               +
                                               viscs2_(0, 2, vi)*conv_r_af_(2, 0, ui)) ;
                elemat(vi*4     , ui*4 + 1)+= fac_afgdt_two_visc_afgdt_tauM_facMtau*
                                              (viscs2_(0, 0, vi)*conv_r_af_(0, 1, ui)
                                               +
                                               viscs2_(0, 1, vi)*conv_r_af_(1, 1, ui)
                                               +
                                               viscs2_(0, 2, vi)*conv_r_af_(2, 1, ui)) ;
                elemat(vi*4     , ui*4 + 2)+= fac_afgdt_two_visc_afgdt_tauM_facMtau*
                                              (viscs2_(0, 0, vi)*conv_r_af_(0, 2, ui)
                                               +
                                               viscs2_(0, 1, vi)*conv_r_af_(1, 2, ui)
                                               +
                                               viscs2_(0, 2, vi)*conv_r_af_(2, 2, ui)) ;
                elemat(vi*4 + 1, ui*4     )+= fac_afgdt_two_visc_afgdt_tauM_facMtau*
                                              (viscs2_(0, 1, vi)*conv_r_af_(0, 0, ui)
                                               +
                                               viscs2_(1, 1, vi)*conv_r_af_(1, 0, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(2, 0, ui)) ;
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_two_visc_afgdt_tauM_facMtau*
                                              (viscs2_(0, 1, vi)*conv_r_af_(0, 1, ui)
                                               +
                                               viscs2_(1, 1, vi)*conv_r_af_(1, 1, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(2, 1, ui)) ;
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_two_visc_afgdt_tauM_facMtau*
                                              (viscs2_(0, 1, vi)*conv_r_af_(0, 2, ui)
                                               +
                                               viscs2_(1, 1, vi)*conv_r_af_(1, 2, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(2, 2, ui)) ;
                elemat(vi*4 + 2, ui*4    ) += fac_afgdt_two_visc_afgdt_tauM_facMtau*
                                              (viscs2_(0, 2, vi)*conv_r_af_(0, 0, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(1, 0, ui)
                                               +
                                               viscs2_(2, 2, vi)*conv_r_af_(2, 0, ui)) ;
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_two_visc_afgdt_tauM_facMtau*
                                              (viscs2_(0, 2, vi)*conv_r_af_(0, 1, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(1, 1, ui)
                                               +
                                               viscs2_(2, 2, vi)*conv_r_af_(2, 1, ui)) ;
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_two_visc_afgdt_tauM_facMtau*
                                              (viscs2_(0, 2, vi)*conv_r_af_(0, 2, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(1, 2, ui)
                                               +
                                               viscs2_(2, 2, vi)*conv_r_af_(2, 2, ui)) ;

              } // end loop rows (test functions for matrix)
            } // end loop columns (solution for matrix, test function for vector)

          } // end if agls and newton
        } // end agls stabilisation

        if(cstab == Fluid3::continuity_stab_yes)
        {
          //---------------------------------------------------------------
          //
          //                     STABILISATION PART
          //                  CONTINUITY STABILISATION
          //
          //---------------------------------------------------------------

          const double fac_gamma_dt_dt_factauC          = fac*gamma*dt*dt*factauC;

          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {
              /*
                                 tauC * dt
            factor: +gamma* dt * ---------
                                 tauC + dt
                    /                          \
                   |                            |
                   | nabla o Dacc  , nabla o v  |
                   |                            |
                    \                          /
              */

              elemat(vi*4    , ui*4    ) += fac_gamma_dt_dt_factauC*derxy_(0,ui)*derxy_(0,vi) ;
              elemat(vi*4    , ui*4 + 1) += fac_gamma_dt_dt_factauC*derxy_(1,ui)*derxy_(0,vi) ;
              elemat(vi*4    , ui*4 + 2) += fac_gamma_dt_dt_factauC*derxy_(2,ui)*derxy_(0,vi) ;
              elemat(vi*4 + 1, ui*4    ) += fac_gamma_dt_dt_factauC*derxy_(0,ui)*derxy_(1,vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_gamma_dt_dt_factauC*derxy_(1,ui)*derxy_(1,vi) ;
              elemat(vi*4 + 1, ui*4 + 2) += fac_gamma_dt_dt_factauC*derxy_(2,ui)*derxy_(1,vi) ;
              elemat(vi*4 + 2, ui*4    ) += fac_gamma_dt_dt_factauC*derxy_(0,ui)*derxy_(2,vi) ;
              elemat(vi*4 + 2, ui*4 + 1) += fac_gamma_dt_dt_factauC*derxy_(1,ui)*derxy_(2,vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_gamma_dt_dt_factauC*derxy_(2,ui)*derxy_(2,vi) ;

            } // end loop rows (test functions for matrix)
          } // end loop rows (solution for matrix, test function for vector)

        } // end continuity stabilisation

        if(cross == Fluid3::cross_stress_stab)
        {
          //---------------------------------------------------------------
          //
          //                     STABILISATION PART
          //       RESIDUAL BASED VMM STABILISATION --- CROSS STRESS
          //
          //---------------------------------------------------------------

          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {

              /*  factor:
              
               +alphaF*gamma*dt

                          /                          \
                         |  /~n+af       \            |
                         | | u    o nabla | Dacc , v  |
                         |  \            /            |
                          \                          /
              */
              elemat(vi*4    , ui*4    ) += fac*afgdt*conv_subaf_(ui)*funct_(vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac*afgdt*conv_subaf_(ui)*funct_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac*afgdt*conv_subaf_(ui)*funct_(vi) ;
            } // end loop rows (test functions for matrix)
          } // end loop columns (solution for matrix, test function for vector)
        } // end cross
      } // end if compute_elemat

      //---------------------------------------------------------------
      //---------------------------------------------------------------
      //
      //                       RIGHT HAND SIDE
      //
      //---------------------------------------------------------------
      //---------------------------------------------------------------
      if(inertia == Fluid3::inertia_stab_keep)
      {

        const double fac_sacc_plus_resM_not_partially_integrated_x =fac*(-svelaf_(0)/tauM-pderxynp_(0)+2*visc*viscaf_old_(0)) ;
        const double fac_sacc_plus_resM_not_partially_integrated_y =fac*(-svelaf_(1)/tauM-pderxynp_(1)+2*visc*viscaf_old_(1)) ;
        const double fac_sacc_plus_resM_not_partially_integrated_z =fac*(-svelaf_(2)/tauM-pderxynp_(2)+2*visc*viscaf_old_(2)) ;

        for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
        {
          //---------------------------------------------------------------
          //
          //     GALERKIN PART I AND SUBSCALE ACCELERATION STABILISATION
          //
          //---------------------------------------------------------------
          /*  factor: +1

               /             \     /
              |   ~ n+am      |   |     n+am    / n+af        \   n+af
              |  acc     , v  | + |  acc     + | u     o nabla | u     +
              |     (i)       |   |     (i)     \ (i)         /   (i)
               \             /     \

                                                   \
                                        n+af        |
                                     - f       , v  |
                                                    |
                                                   /

             using
                                                        /
                        ~ n+am        1.0      ~n+af   |    n+am
                       acc     = - --------- * u     - | acc     +
                          (i)           n+af    (i)    |    (i)
                                   tau_M                \

                                    / n+af        \   n+af            n+1
                                 + | u     o nabla | u     + nabla o p    -
                                    \ (i)         /   (i)             (i)

                                                            / n+af \
                                 - 2 * nu * grad o epsilon | u      | -
                                                            \ (i)  /
                                         \
                                    n+af  |
                                 - f      |
                                          |
                                         /

          */

          elevec[ui*4    ] -= fac_sacc_plus_resM_not_partially_integrated_x*funct_(ui) ;
          elevec[ui*4 + 1] -= fac_sacc_plus_resM_not_partially_integrated_y*funct_(ui) ;
          elevec[ui*4 + 2] -= fac_sacc_plus_resM_not_partially_integrated_z*funct_(ui) ;
        }

        //---------------------------------------------------------------
        //
        //   GALERKIN PART 2 (REMAINING EXPRESSIONS)
        //
        //---------------------------------------------------------------
        {

          const double fac_divunp = fac*divunp;
          const double fac_visc   = fac*visc;
          const double fac_prenp_ = fac*prenp_ ;

          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            /* pressure */

            /*  factor: -1

               /                  \
              |   n+1              |
              |  p    , nabla o v  |
              |                    |
               \                  /
            */

            elevec[ui*4    ] += fac_prenp_*derxy_(0,ui) ;
            elevec[ui*4 + 1] += fac_prenp_*derxy_(1,ui) ;
            elevec[ui*4 + 2] += fac_prenp_*derxy_(2,ui) ;

            /* viscous term */

            /*  factor: +2*nu

               /                            \
              |       / n+af \         / \   |
              |  eps | u      | , eps | v |  |
              |       \      /         \ /   |
               \                            /
            */

            elevec[ui*4    ] -= fac_visc*
                                (derxy_(0,ui)*vderxyaf_(0,0)*2.0
                                 +
                                 derxy_(1,ui)*vderxyaf_(0,1)
                                 +
                                 derxy_(1,ui)*vderxyaf_(1,0)
                                 +
                                 derxy_(2,ui)*vderxyaf_(0,2)
                                 +
                                 derxy_(2,ui)*vderxyaf_(2,0)) ;
            elevec[ui*4 + 1] -= fac_visc*
                                (derxy_(0,ui)*vderxyaf_(0,1)
                                 +
                                 derxy_(0,ui)*vderxyaf_(1,0)
                                 +
                                 derxy_(1,ui)*vderxyaf_(1,1)*2.0
                                 +
                                 derxy_(2,ui)*vderxyaf_(1,2)
                                 +
                                 derxy_(2,ui)*vderxyaf_(2,1)) ;
            elevec[ui*4 + 2] -= fac_visc*
                                (derxy_(0,ui)*vderxyaf_(0,2)
                                 +
                                 derxy_(0,ui)*vderxyaf_(2,0)
                                 +
                                 derxy_(1,ui)*vderxyaf_(1,2)
                                 +
                                 derxy_(1,ui)*vderxyaf_(2,1)
                                 +
                                 derxy_(2,ui)*vderxyaf_(2,2)*2.0) ;


            /* continuity equation */

            /*  factor: +1

               /                \
              |          n+1     |
              | nabla o u   , q  |
              |                  |
               \                /
            */

            elevec[ui*4 + 3] -= fac_divunp*funct_(ui);

          } // end loop rows (solution for matrix, test function for vector)
        }
      }
      else
      {
      for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
      {

        //---------------------------------------------------------------
        //
        //                       GALERKIN PART
        //
        //---------------------------------------------------------------

        /* inertia terms */

        /*  factor: +1

               /             \
              |     n+am      |
              |  acc     , v  |
              |               |
               \             /
        */

        elevec[ui*4    ] -= fac*funct_(ui)*accintam_(0) ;
        elevec[ui*4 + 1] -= fac*funct_(ui)*accintam_(1) ;
        elevec[ui*4 + 2] -= fac*funct_(ui)*accintam_(2) ;

        /* convection */

        /*  factor: +1

               /                             \
              |  / n+af       \    n+af       |
              | | u    o nabla |  u      , v  |
              |  \            /               |
               \                             /
        */

        elevec[ui*4    ] -= fac*(velintaf_(0)*conv_r_af_(0,0,ui)
                                 +
                                 velintaf_(1)*conv_r_af_(0,1,ui)
                                 +
                                 velintaf_(2)*conv_r_af_(0,2,ui)) ;
        elevec[ui*4 + 1] -= fac*(velintaf_(0)*conv_r_af_(1,0,ui)
                                 +
                                 velintaf_(1)*conv_r_af_(1,1,ui)
                                 +
                                 velintaf_(2)*conv_r_af_(1,2,ui)) ;
        elevec[ui*4 + 2] -= fac*(velintaf_(0)*conv_r_af_(2,0,ui)
                                 +
                                 velintaf_(1)*conv_r_af_(2,1,ui)
                                 +
                                 velintaf_(2)*conv_r_af_(2,2,ui)) ;

        /* pressure */

        /*  factor: -1

               /                  \
              |   n+1              |
              |  p    , nabla o v  |
              |                    |
               \                  /
        */

        elevec[ui*4    ] += fac*prenp_*derxy_(0,ui) ;
        elevec[ui*4 + 1] += fac*prenp_*derxy_(1,ui) ;
        elevec[ui*4 + 2] += fac*prenp_*derxy_(2,ui) ;

        /* viscous term */

        /*  factor: +2*nu

               /                            \
              |       / n+af \         / \   |
              |  eps | u      | , eps | v |  |
              |       \      /         \ /   |
               \                            /
        */

        elevec[ui*4    ] -= visc*fac*
                            (derxy_(0,ui)*vderxyaf_(0,0)*2.0
                             +
                             derxy_(1,ui)*vderxyaf_(0,1)
                             +
                             derxy_(1,ui)*vderxyaf_(1,0)
                             +
                             derxy_(2,ui)*vderxyaf_(0,2)
                             +
                             derxy_(2,ui)*vderxyaf_(2,0)) ;
        elevec[ui*4 + 1] -= visc*fac*
                            (derxy_(0,ui)*vderxyaf_(0,1)
                             +
                             derxy_(0,ui)*vderxyaf_(1,0)
                             +
                             derxy_(1,ui)*vderxyaf_(1,1)*2.0
                             +
                             derxy_(2,ui)*vderxyaf_(1,2)
                             +
                             derxy_(2,ui)*vderxyaf_(2,1)) ;
        elevec[ui*4 + 2] -= visc*fac*
                            (derxy_(0,ui)*vderxyaf_(0,2)
                             +
                             derxy_(0,ui)*vderxyaf_(2,0)
                             +
                             derxy_(1,ui)*vderxyaf_(1,2)
                             +
                             derxy_(1,ui)*vderxyaf_(2,1)
                             +
                             derxy_(2,ui)*vderxyaf_(2,2)*2.0) ;

        /* body force (dead load...) */

        /*  factor: -1

               /           \
              |   n+af      |
              |  f     , v  |
              |             |
               \           /
        */

        elevec[ui*4    ] += fac*funct_(ui)*bodyforceaf_(0);
        elevec[ui*4 + 1] += fac*funct_(ui)*bodyforceaf_(1);
        elevec[ui*4 + 2] += fac*funct_(ui)*bodyforceaf_(2);

        /* continuity equation */

        /*  factor: +1

               /                \
              |          n+1     |
              | nabla o u   , q  |
              |                  |
               \                /
        */

        elevec[ui*4 + 3] -= fac*funct_(ui)*divunp;

      } // end loop rows (solution for matrix, test function for vector)
    }

      if(pspg == Fluid3::pstab_use_pspg)
      {
        const double fac_svelnpx                      = fac*svelnp(0,iquad);
        const double fac_svelnpy                      = fac*svelnp(1,iquad);
        const double fac_svelnpz                      = fac*svelnp(2,iquad);

        for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
        {
          //---------------------------------------------------------------
          //
          //                     STABILISATION PART
          //                    PRESSURE STABILISATION
          //
          //---------------------------------------------------------------
          /* factor: -1

                       /                 \
                      |  ~n+1             |
                      |  u    , nabla  q  |
                      |   (i)             |
                       \                 /
          */

          elevec[ui*4 + 3] += fac_svelnpx*derxy_(0,ui)+fac_svelnpy*derxy_(1,ui)+fac_svelnpz*derxy_(2,ui);

        } // end loop rows (solution for matrix, test function for vector)
      }

      if(supg == Fluid3::convective_stab_supg)
      {

        for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
        {
        //---------------------------------------------------------------
        //
        //                     STABILISATION PART
        //         SUPG STABILISATION FOR CONVECTION DOMINATED FLOWS
        //
        //---------------------------------------------------------------
          /*
                  /                             \
                 |  ~n+af    / n+af        \     |
                 |  u     , | u     o nabla | v  |
                 |           \             /     |
                  \                             /

          */

          elevec[ui*4    ] += fac*conv_c_af_(ui)*svelaf_(0);
          elevec[ui*4 + 1] += fac*conv_c_af_(ui)*svelaf_(1);
          elevec[ui*4 + 2] += fac*conv_c_af_(ui)*svelaf_(2);

        } // end loop rows (solution for matrix, test function for vector)
      }

      if(agls == Fluid3::viscous_stab_agls || agls == Fluid3::viscous_stab_agls_only_rhs)
      {

        const double fac_two_visc_svelaf_x = fac*2.0*visc*svelaf_(0);
        const double fac_two_visc_svelaf_y = fac*2.0*visc*svelaf_(1);
        const double fac_two_visc_svelaf_z = fac*2.0*visc*svelaf_(2);

        for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
        {
          //---------------------------------------------------------------
          //
          //                     STABILISATION PART
          //             VISCOUS STABILISATION (FOR AGLS)
          //
          //---------------------------------------------------------------

          /*
                 /                      \
                |  ~n+af                 |
                |  u      , div eps (v)  |
                |                        |
                 \                      /

          */
          elevec[ui*4    ] += fac_two_visc_svelaf_x*viscs2_(0, 0, ui)
                              +
                              fac_two_visc_svelaf_y*viscs2_(0, 1, ui)
                              +
                              fac_two_visc_svelaf_z*viscs2_(0, 2, ui) ;
          elevec[ui*4 + 1] += fac_two_visc_svelaf_x*viscs2_(0, 1, ui)
                              +
                              fac_two_visc_svelaf_y*viscs2_(1, 1, ui)
                              +
                              fac_two_visc_svelaf_z*viscs2_(1, 2, ui) ;
          elevec[ui*4 + 2] += fac_two_visc_svelaf_x*viscs2_(0, 2, ui)
                              +
                              fac_two_visc_svelaf_y*viscs2_(1, 2, ui)
                              +
                              fac_two_visc_svelaf_z*viscs2_(2, 2, ui) ;
        } // end loop rows (solution for matrix, test function for vector)
      }

      if(cstab == Fluid3::continuity_stab_yes)
      {
        const double fac_sprenp                       = fac*sprenp(iquad);

        for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
        {
          //---------------------------------------------------------------
          //
          //                     STABILISATION PART
          //                  CONTINUITY STABILISATION
          //
          //---------------------------------------------------------------


          /* factor: -1

                       /                  \
                      |  ~n+1              |
                      |  p    , nabla o v  |
                      |   (i)              |
                       \                  /
          */
          elevec[ui*4    ] += fac_sprenp*derxy_(0,ui) ;
          elevec[ui*4 + 1] += fac_sprenp*derxy_(1,ui) ;
          elevec[ui*4 + 2] += fac_sprenp*derxy_(2,ui) ;
        } // end loop rows (solution for matrix, test function for vector)
      }

      if(cross == Fluid3::cross_stress_stab_only_rhs || cross == Fluid3::cross_stress_stab)
      {
        //---------------------------------------------------------------
        //
        //                     STABILISATION PART
        //       RESIDUAL BASED VMM STABILISATION --- CROSS STRESS
        //
        //---------------------------------------------------------------
        for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
        {
          /* factor:

                  /                           \
                 |   ~n+af           n+af      |
                 | ( u    o nabla ) u     , v  |
                 |    (i)            (i)       |
                  \                           /
          */
          elevec[ui*4    ] -= fac*convsubaf_old_(0)*funct_(ui);
          elevec[ui*4 + 1] -= fac*convsubaf_old_(1)*funct_(ui);
          elevec[ui*4 + 2] -= fac*convsubaf_old_(2)*funct_(ui);
        }
      }

      if(reynolds == Fluid3::reynolds_stress_stab_only_rhs)
      {
        //---------------------------------------------------------------
        //
        //                     STABILISATION PART
        //     RESIDUAL BASED VMM STABILISATION --- REYNOLDS STRESS
        //
        //---------------------------------------------------------------

        for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
        {

          /* factor:

                  /                             \
                 |  ~n+af      ~n+af             |
                 |  u      , ( u    o nabla ) v  |
                 |                               |
                  \                             /
          */
          elevec[ui*4    ] += fac*(svelaf_(0)*derxy_(0,ui)
                                   +
                                   svelaf_(1)*derxy_(1,ui)
                                   +
                                   svelaf_(2)*derxy_(2,ui))*svelaf_(0);
          elevec[ui*4 + 1] += fac*(svelaf_(0)*derxy_(0,ui)
                                   +
                                   svelaf_(1)*derxy_(1,ui)
                                   +
                                   svelaf_(2)*derxy_(2,ui))*svelaf_(1);
          elevec[ui*4 + 2] += fac*(svelaf_(0)*derxy_(0,ui)
                                   +
                                   svelaf_(1)*derxy_(1,ui)
                                   +
                                   svelaf_(2)*derxy_(2,ui))*svelaf_(2);

        } // end loop rows (solution for matrix, test function for vector)
      }
    }
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//
//     ELEMENT FORMULATION BASED ON QUASISTATIC SUBSCALES
//
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
//--------------------------------------------------------------
    else
    {
      const double tauM   = tau_(0);
      const double tauMp  = tau_(1);
      const double tauC   = tau_(2);

      /*
        This is the operator

                  /               \
                 | resM    o nabla |
                  \    (i)        /

                  required for the cross and reynolds stress calculation

      */


      conv_resM_ =  blitz::sum(resM_(j)*derxy_(j,i),j);


      //--------------------------------------------------------------
      //--------------------------------------------------------------
      //
      //                       SYSTEM MATRIX
      //
      //--------------------------------------------------------------
      //--------------------------------------------------------------
      if(compute_elemat)
      {
        //---------------------------------------------------------------
        //
        //                       GALERKIN PART
        //
        //---------------------------------------------------------------
        {
          const double fac_alphaM     = fac*alphaM;
          const double fac_afgdt      = fac*afgdt;
          const double fac_visc_afgdt = fac*visc*afgdt;
          const double fac_gamma_dt   = fac*gamma*dt;
          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {

              /*
                inertia term (intermediate)

                factor: +alphaM

                 /          \
                |            |
                |  Dacc , v  |
                |            |
                 \          /
              */
              elemat(vi*4    , ui*4    ) += fac_alphaM*funct_(vi)*funct_(ui) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_alphaM*funct_(vi)*funct_(ui) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_alphaM*funct_(vi)*funct_(ui) ;

              /* convection (intermediate)

               factor:

               +alphaF*gamma*dt

                          /                          \
                         |  / n+af       \            |
                         | | u    o nabla | Dacc , v  |
                         |  \            /            |
                          \                          /
              */
              elemat(vi*4    , ui*4    ) += fac_afgdt*funct_(vi)*conv_c_af_(ui) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt*funct_(vi)*conv_c_af_(ui) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt*funct_(vi)*conv_c_af_(ui) ;

              /* pressure (implicit) */

              /*  factor: -1

                 /                \
                |                  |
                |  Dp , nabla o v  |
                |                  |
                 \                /
              */

              elemat(vi*4    , ui*4 + 3) -= fac*funct_(ui)*derxy_(0, vi) ;
              elemat(vi*4 + 1, ui*4 + 3) -= fac*funct_(ui)*derxy_(1, vi) ;
              elemat(vi*4 + 2, ui*4 + 3) -= fac*funct_(ui)*derxy_(2, vi) ;

              /* viscous term (intermediate) */

              /*  factor: +2*nu*alphaF*gamma*dt

                 /                          \
                |       /    \         / \   |
                |  eps | Dacc | , eps | v |  |
                |       \    /         \ /   |
                 \                          /
              */

              elemat(vi*4    , ui*4    ) += fac_visc_afgdt*(2.0*derxy_(0,ui)*derxy_(0,vi)
                                                            +
                                                            derxy_(1,ui)*derxy_(1,vi)
                                                            +
                                                            derxy_(2,ui)*derxy_(2,vi)) ;
              elemat(vi*4    , ui*4 + 1) += fac_visc_afgdt*derxy_(0,ui)*derxy_(1,vi) ;
              elemat(vi*4    , ui*4 + 2) += fac_visc_afgdt*derxy_(0,ui)*derxy_(2,vi) ;
              elemat(vi*4 + 1, ui*4    ) += fac_visc_afgdt*derxy_(1,ui)*derxy_(0,vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_visc_afgdt*(derxy_(0,ui)*derxy_(0,vi)
                                                            +
                                                            2.0*derxy_(1,ui)*derxy_(1,vi)
                                                            +
                                                            derxy_(2,ui)*derxy_(2,vi)) ;
              elemat(vi*4 + 1, ui*4 + 2) += fac_visc_afgdt*derxy_(1,ui)*derxy_(2,vi) ;
              elemat(vi*4 + 2, ui*4    ) += fac_visc_afgdt*derxy_(2,ui)*derxy_(0,vi) ;
              elemat(vi*4 + 2, ui*4 + 1) += fac_visc_afgdt*derxy_(2,ui)*derxy_(1,vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_visc_afgdt*(derxy_(0,ui)*derxy_(0,vi)
                                                            +
                                                            derxy_(1,ui)*derxy_(1,vi)
                                                            +
                                                            2.0*derxy_(2,ui)*derxy_(2,vi)) ;

              /* continuity equation (implicit) */

              /*  factor: +gamma*dt

                 /                  \
                |                    |
                | nabla o Dacc  , q  |
                |                    |
                 \                  /
              */

              elemat(vi*4 + 3, ui*4    ) += fac_gamma_dt*funct_(vi)*derxy_(0,ui) ;
              elemat(vi*4 + 3, ui*4 + 1) += fac_gamma_dt*funct_(vi)*derxy_(1,ui) ;
              elemat(vi*4 + 3, ui*4 + 2) += fac_gamma_dt*funct_(vi)*derxy_(2,ui) ;

            } // end loop rows (test functions for matrix)
          } // end loop rows (solution for matrix, test function for vector)
          if (newton)
          {
            for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
            {
              for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
              {
                /* convection (intermediate)

                 factor:

                        +alphaF*gamma*dt


                         /                            \
                        |  /            \   n+af       |
                        | | Dacc o nabla | u      , v  |
                        |  \            /              |
                         \                            /
                */
                elemat(vi*4    , ui*4    ) += fac_afgdt*funct_(vi)*conv_r_af_(0, 0, ui) ;
                elemat(vi*4    , ui*4 + 1) += fac_afgdt*funct_(vi)*conv_r_af_(0, 1, ui) ;
                elemat(vi*4    , ui*4 + 2) += fac_afgdt*funct_(vi)*conv_r_af_(0, 2, ui) ;
                elemat(vi*4 + 1, ui*4    ) += fac_afgdt*funct_(vi)*conv_r_af_(1, 0, ui) ;
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt*funct_(vi)*conv_r_af_(1, 1, ui) ;
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt*funct_(vi)*conv_r_af_(1, 2, ui) ;
                elemat(vi*4 + 2, ui*4    ) += fac_afgdt*funct_(vi)*conv_r_af_(2, 0, ui) ;
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt*funct_(vi)*conv_r_af_(2, 1, ui) ;
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt*funct_(vi)*conv_r_af_(2, 2, ui) ;
              } // end loop rows (test functions for matrix)
            } // end loop rows (solution for matrix, test function for vector)
          }
        }

        if(pspg == Fluid3::pstab_use_pspg)
        {
          const double fac_alphaM_tauMp         = fac*alphaM*tauMp;
          const double fac_afgdt_tauMp          = fac*afgdt*tauMp;
          const double fac_two_visc_afgdt_tauMp = fac*2.0*visc*afgdt*tauMp;
          const double fac_tauMp                = fac*tauMp;

          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {
              /* pressure stabilisation --- inertia    */

              /* factor: +alphaM*tauMp

                                /                \
                               |                  |
                               |  Dacc , nabla q  |
                               |                  |
                                \                /
              */

              elemat(vi*4 + 3, ui*4    ) += fac_alphaM_tauMp*funct_(ui)*derxy_(0,vi) ;
              elemat(vi*4 + 3, ui*4 + 1) += fac_alphaM_tauMp*funct_(ui)*derxy_(1,vi) ;
              elemat(vi*4 + 3, ui*4 + 2) += fac_alphaM_tauMp*funct_(ui)*derxy_(2,vi) ;


              /* pressure stabilisation --- convection */

              /*  factor: +alphaF*gamma*dt*tauMp

                        /                                \
                       |  / n+af       \                  |
                       | | u    o nabla | Dacc , nabla q  |
                       |  \            /                  |
                        \                                /

              */

              elemat(vi*4 + 3, ui*4    ) += fac_afgdt_tauMp*conv_c_af_(ui)*derxy_(0,vi) ;
              elemat(vi*4 + 3, ui*4 + 1) += fac_afgdt_tauMp*conv_c_af_(ui)*derxy_(1,vi) ;
              elemat(vi*4 + 3, ui*4 + 2) += fac_afgdt_tauMp*conv_c_af_(ui)*derxy_(2,vi) ;

              /* pressure stabilisation --- diffusion  */

              /* factor: -2*nu*alphaF*gamma*dt*tauMp

                    /                                \
                   |               /    \             |
                   |  nabla o eps | Dacc | , nabla q  |
                   |               \    /             |
                    \                                /
              */

              elemat(vi*4 + 3, ui*4    ) -= fac_two_visc_afgdt_tauMp*
                                            (derxy_(0,vi)*viscs2_(0,0,ui)
                                             +
                                             derxy_(1,vi)*viscs2_(0,1,ui)
                                             +
                                             derxy_(2,vi)*viscs2_(0,2,ui)) ;
              elemat(vi*4 + 3, ui*4 + 1) -= fac_two_visc_afgdt_tauMp*
                                            (derxy_(0,vi)*viscs2_(0,1,ui)
                                             +
                                             derxy_(1,vi)*viscs2_(1,1,ui)
                                             +
                                             derxy_(2,vi)*viscs2_(1,2,ui)) ;
              elemat(vi*4 + 3, ui*4 + 2) -= fac_two_visc_afgdt_tauMp*
                                            (derxy_(0,vi)*viscs2_(0,2,ui)
                                             +
                                             derxy_(1,vi)*viscs2_(1,2,ui)
                                             +
                                             derxy_(2,vi)*viscs2_(2,2,ui)) ;

              /* pressure stabilisation --- pressure   */

              /* factor: +tauMp

                    /                    \
                   |                      |
                   |  nabla Dp , nabla q  |
                   |                      |
                    \                    /
              */

              elemat(vi*4 + 3, ui*4 + 3) += fac_tauMp*
                                            (derxy_(0,ui)*derxy_(0,vi)
                                             +
                                             derxy_(1,ui)*derxy_(1,vi)
                                             +
                                             derxy_(2,ui)*derxy_(2,vi)) ;

            } // end loop rows (test functions for matrix)
          } // end loop rows (solution for matrix, test function for vector)
          if (newton)
          {
            for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
            {
              for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
              {

                /* pressure stabilisation --- convection */

                /*  factor: +alphaF*gamma*dt*tauMp

                       /                                  \
                      |  /            \   n+af             |
                      | | Dacc o nabla | u      , nabla q  |
                      |  \            /                    |
                       \                                  /
                */

                elemat(vi*4 + 3, ui*4    ) += fac_afgdt_tauMp*
                                              (derxy_(0,vi)*conv_r_af_(0,0,ui)
                                               +
                                               derxy_(1,vi)*conv_r_af_(1,0,ui)
                                               +
                                               derxy_(2,vi)*conv_r_af_(2,0,ui)) ;
                elemat(vi*4 + 3, ui*4 + 1) += fac_afgdt_tauMp*
                                              (derxy_(0,vi)*conv_r_af_(0,1,ui)
                                               +
                                               derxy_(1,vi)*conv_r_af_(1,1,ui)
                                               +
                                               derxy_(2,vi)*conv_r_af_(2,1,ui)) ;
                elemat(vi*4 + 3, ui*4 + 2) += fac_afgdt_tauMp*
                                              (derxy_(0,vi)*conv_r_af_(0,2,ui)
                                               +
                                               derxy_(1,vi)*conv_r_af_(1,2,ui)
                                               +
                                               derxy_(2,vi)*conv_r_af_(2,2,ui)) ;
              } // end loop rows (test functions for matrix)
            } // end loop rows (solution for matrix, test function for vector)
          }
        }

        if(supg == Fluid3::convective_stab_supg)
        {
          const double fac_alphaM_tauM         = fac*tauM*alphaM;
          const double fac_afgdt_tauM          = fac*tauM*afgdt;
          const double fac_two_visc_afgdt_tauM = fac*tauM*afgdt*2.0*visc;
          const double fac_tauM                = fac*tauM;

          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {
              /* SUPG stabilisation --- inertia     */

              /* factor: +alphaM*tauM

                    /                           \
                   |          / n+af       \     |
                   |  Dacc , | u    o nabla | v  |
                   |          \            /     |
                    \                           /
              */

              elemat(vi*4    , ui*4    ) += fac_alphaM_tauM*funct_(ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_alphaM_tauM*funct_(ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_alphaM_tauM*funct_(ui)*conv_c_af_(vi) ;

              /* SUPG stabilisation --- convection  */

              /* factor: +alphaF*gamma*dt*tauM


                    /                                               \
                   |    / n+af        \          / n+af        \     |
                   |   | u     o nabla | Dacc , | u     o nabla | v  |
                   |    \             /          \             /     |
                    \                                               /

              */

              elemat(vi*4, ui*4)         += fac_afgdt_tauM*conv_c_af_(ui)*conv_c_af_(vi);
              elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_tauM*conv_c_af_(ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_tauM*conv_c_af_(ui)*conv_c_af_(vi) ;

              /* SUPG stabilisation --- diffusion   */

              /* factor: -2*nu*alphaF*gamma*dt*tauM


                    /                                            \
                   |               /     \    / n+af        \     |
                   |  nabla o eps | Dacc  |, | u     o nabla | v  |
                   |               \     /    \             /     |
                    \                                            /

              */

              elemat(vi*4, ui*4)         -= fac_two_visc_afgdt_tauM*conv_c_af_(vi)*viscs2_(0, 0, ui) ;
              elemat(vi*4, ui*4 + 1)     -= fac_two_visc_afgdt_tauM*conv_c_af_(vi)*viscs2_(0, 1, ui) ;
              elemat(vi*4, ui*4 + 2)     -= fac_two_visc_afgdt_tauM*conv_c_af_(vi)*viscs2_(0, 2, ui) ;
              elemat(vi*4 + 1, ui*4)     -= fac_two_visc_afgdt_tauM*conv_c_af_(vi)*viscs2_(0, 1, ui) ;
              elemat(vi*4 + 1, ui*4 + 1) -= fac_two_visc_afgdt_tauM*conv_c_af_(vi)*viscs2_(1, 1, ui) ;
              elemat(vi*4 + 1, ui*4 + 2) -= fac_two_visc_afgdt_tauM*conv_c_af_(vi)*viscs2_(1, 2, ui) ;
              elemat(vi*4 + 2, ui*4)     -= fac_two_visc_afgdt_tauM*conv_c_af_(vi)*viscs2_(0, 2, ui) ;
              elemat(vi*4 + 2, ui*4 + 1) -= fac_two_visc_afgdt_tauM*conv_c_af_(vi)*viscs2_(1, 2, ui) ;
              elemat(vi*4 + 2, ui*4 + 2) -= fac_two_visc_afgdt_tauM*conv_c_af_(vi)*viscs2_(2, 2, ui) ;

              /* SUPG stabilisation --- pressure    */

              /* factor: +tauM

                    /                               \
                   |              / n+af       \     |
                   |  nabla Dp , | u    o nabla | v  |
                   |              \            /     |
                    \                               /
              */

              elemat(vi*4    , ui*4 + 3) += fac_tauM*derxy_(0,ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 1, ui*4 + 3) += fac_tauM*derxy_(1,ui)*conv_c_af_(vi) ;
              elemat(vi*4 + 2, ui*4 + 3) += fac_tauM*derxy_(2,ui)*conv_c_af_(vi) ;

            } // end loop rows (test functions for matrix)
          } // end loop rows (solution for matrix, test function for vector)
          if (newton)
          {
            for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
            {
              for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
              {
                /* SUPG stabilisation --- inertia     */

                /* factor: +alphaF*gamma*dt*tauM

                    /                               \
                   |     n+am     /            \     |
                   |  acc      , | Dacc o nabla | v  |
                   |              \            /     |
                    \                               /
                */

                elemat(vi*4    , ui*4    ) += fac_afgdt_tauM*funct_(ui)*accintam_(0)*derxy_(0,vi) ;
                elemat(vi*4    , ui*4 + 1) += fac_afgdt_tauM*funct_(ui)*accintam_(0)*derxy_(1,vi) ;
                elemat(vi*4    , ui*4 + 2) += fac_afgdt_tauM*funct_(ui)*accintam_(0)*derxy_(2,vi) ;
                elemat(vi*4 + 1, ui*4    ) += fac_afgdt_tauM*funct_(ui)*accintam_(1)*derxy_(0,vi) ;
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_tauM*funct_(ui)*accintam_(1)*derxy_(1,vi) ;
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_tauM*funct_(ui)*accintam_(1)*derxy_(2,vi) ;
                elemat(vi*4 + 2, ui*4)     += fac_afgdt_tauM*funct_(ui)*accintam_(2)*derxy_(0,vi) ;
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_tauM*funct_(ui)*accintam_(2)*derxy_(1,vi) ;
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_tauM*funct_(ui)*accintam_(2)*derxy_(2,vi) ;


                /* SUPG stabilisation --- convection  */

                /* factor: +alphaF*gamma*dt*tauM

                    /                                               \
                   |    / n+af        \   n+af    /            \     |
                   |   | u     o nabla | u     , | Dacc o nabla | v  |
                   |    \             /           \            /     |
                    \                                               /

                    /                                               \
                   |    /            \   n+af    / n+af        \     |
                   |   | Dacc o nabla | u     , | u     o nabla | v  |
                   |    \            /           \             /     |
                    \                                               /
                */

                elemat(vi*4, ui*4)         += fac_afgdt_tauM*
                                              (conv_c_af_(vi)*conv_r_af_(0, 0, ui)
                                               +
                                               velintaf_(0)*derxy_(0, vi)*conv_r_af_(0, 0, ui)
                                               +
                                               velintaf_(1)*derxy_(0, vi)*conv_r_af_(0, 1, ui)
                                               +
                                               velintaf_(2)*derxy_(0, vi)*conv_r_af_(0, 2, ui)) ;
                elemat(vi*4, ui*4 + 1)     += fac_afgdt_tauM*
                                              (conv_c_af_(vi)*conv_r_af_(0, 1, ui)
                                               +
                                               velintaf_(0)*derxy_(1, vi)*conv_r_af_(0, 0, ui)
                                               +
                                               velintaf_(1)*derxy_(1, vi)*conv_r_af_(0, 1, ui)
                                               +
                                               velintaf_(2)*derxy_(1, vi)*conv_r_af_(0, 2, ui)) ;
                elemat(vi*4, ui*4 + 2)     += fac_afgdt_tauM*
                                              (conv_c_af_(vi)*conv_r_af_(0, 2, ui)
                                               +
                                               velintaf_(0)*derxy_(2, vi)*conv_r_af_(0, 0, ui)
                                               +
                                               velintaf_(1)*derxy_(2, vi)*conv_r_af_(0, 1, ui)
                                               +
                                               velintaf_(2)*derxy_(2, vi)*conv_r_af_(0, 2, ui)) ;
                elemat(vi*4 + 1, ui*4)     += fac_afgdt_tauM*
                                              (conv_c_af_(vi)*conv_r_af_(1, 0, ui)
                                               +
                                               velintaf_(0)*derxy_(0, vi)*conv_r_af_(1, 0, ui)
                                               +
                                               velintaf_(1)*derxy_(0, vi)*conv_r_af_(1, 1, ui)
                                               +
                                               velintaf_(2)*derxy_(0, vi)*conv_r_af_(1, 2, ui)) ;
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_tauM*
                                              (conv_c_af_(vi)*conv_r_af_(1, 1, ui)
                                               +
                                               velintaf_(0)*derxy_(1, vi)*conv_r_af_(1, 0, ui)
                                               +
                                               velintaf_(1)*derxy_(1, vi)*conv_r_af_(1, 1, ui)
                                               +
                                               velintaf_(2)*derxy_(1, vi)*conv_r_af_(1, 2, ui)) ;
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_tauM*
                                              (conv_c_af_(vi)*conv_r_af_(1, 2, ui)
                                               +
                                               velintaf_(0)*derxy_(2, vi)*conv_r_af_(1, 0, ui)
                                               +
                                               velintaf_(1)*derxy_(2, vi)*conv_r_af_(1, 1, ui)
                                               +
                                               velintaf_(2)*derxy_(2, vi)*conv_r_af_(1, 2, ui)) ;
                elemat(vi*4 + 2, ui*4)     += fac_afgdt_tauM*
                                              (conv_c_af_(vi)*conv_r_af_(2, 0, ui)
                                               +
                                               velintaf_(0)*derxy_(0, vi)*conv_r_af_(2, 0, ui)
                                               +
                                               velintaf_(1)*derxy_(0, vi)*conv_r_af_(2, 1, ui)
                                               +
                                               velintaf_(2)*derxy_(0, vi)*conv_r_af_(2, 2, ui)) ;
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_tauM*
                                              (conv_c_af_(vi)*conv_r_af_(2, 1, ui)
                                               +
                                               velintaf_(0)*derxy_(1, vi)*conv_r_af_(2, 0, ui)
                                               +
                                               velintaf_(1)*derxy_(1, vi)*conv_r_af_(2, 1, ui)
                                               +
                                               velintaf_(2)*derxy_(1, vi)*conv_r_af_(2, 2, ui)) ;
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_tauM*
                                              (conv_c_af_(vi)*conv_r_af_(2, 2, ui)
                                               +
                                               velintaf_(0)*derxy_(2, vi)*conv_r_af_(2, 0, ui)
                                               +
                                               velintaf_(1)*derxy_(2, vi)*conv_r_af_(2, 1, ui)
                                               +
                                               velintaf_(2)*derxy_(2, vi)*conv_r_af_(2, 2, ui)) ;


                /* SUPG stabilisation --- diffusion   */

                /* factor: -2*nu*alphaF*gamma*dt*tauM

                    /                                            \
                   |               / n+af \    /            \     |
                   |  nabla o eps | u      |, | Dacc o nabla | v  |
                   |               \      /    \            /     |
                    \                                            /
                */
                elemat(vi*4, ui*4)         -= fac_two_visc_afgdt_tauM*funct_(ui)*viscaf_old_(0)*derxy_(0, vi) ;
                elemat(vi*4, ui*4 + 1)     -= fac_two_visc_afgdt_tauM*funct_(ui)*viscaf_old_(0)*derxy_(1, vi) ;
                elemat(vi*4, ui*4 + 2)     -= fac_two_visc_afgdt_tauM*funct_(ui)*viscaf_old_(0)*derxy_(2, vi) ;
                elemat(vi*4 + 1, ui*4)     -= fac_two_visc_afgdt_tauM*funct_(ui)*viscaf_old_(1)*derxy_(0, vi) ;
                elemat(vi*4 + 1, ui*4 + 1) -= fac_two_visc_afgdt_tauM*funct_(ui)*viscaf_old_(1)*derxy_(1, vi) ;
                elemat(vi*4 + 1, ui*4 + 2) -= fac_two_visc_afgdt_tauM*funct_(ui)*viscaf_old_(1)*derxy_(2, vi) ;
                elemat(vi*4 + 2, ui*4)     -= fac_two_visc_afgdt_tauM*funct_(ui)*viscaf_old_(2)*derxy_(0, vi) ;
                elemat(vi*4 + 2, ui*4 + 1) -= fac_two_visc_afgdt_tauM*funct_(ui)*viscaf_old_(2)*derxy_(1, vi) ;
                elemat(vi*4 + 2, ui*4 + 2) -= fac_two_visc_afgdt_tauM*funct_(ui)*viscaf_old_(2)*derxy_(2, vi) ;

                /* SUPG stabilisation --- pressure    */

                /* factor: +alphaF*gamma*dt*tauM

                    /                                 \
                   |         n+1    /            \     |
                   |  nabla p    , | Dacc o nabla | v  |
                   |                \            /     |
                    \                                 /
                */

                elemat(vi*4    , ui*4    ) += fac_afgdt_tauM*pderxynp_(0)*funct_(ui)*derxy_(0,vi) ;
                elemat(vi*4    , ui*4 + 1) += fac_afgdt_tauM*pderxynp_(0)*funct_(ui)*derxy_(1,vi) ;
                elemat(vi*4    , ui*4 + 2) += fac_afgdt_tauM*pderxynp_(0)*funct_(ui)*derxy_(2,vi) ;
                elemat(vi*4 + 1, ui*4    ) += fac_afgdt_tauM*pderxynp_(1)*funct_(ui)*derxy_(0,vi) ;
                elemat(vi*4 + 1, ui*4 + 1) += fac_afgdt_tauM*pderxynp_(1)*funct_(ui)*derxy_(1,vi) ;
                elemat(vi*4 + 1, ui*4 + 2) += fac_afgdt_tauM*pderxynp_(1)*funct_(ui)*derxy_(2,vi) ;
                elemat(vi*4 + 2, ui*4    ) += fac_afgdt_tauM*pderxynp_(2)*funct_(ui)*derxy_(0,vi) ;
                elemat(vi*4 + 2, ui*4 + 1) += fac_afgdt_tauM*pderxynp_(2)*funct_(ui)*derxy_(1,vi) ;
                elemat(vi*4 + 2, ui*4 + 2) += fac_afgdt_tauM*pderxynp_(2)*funct_(ui)*derxy_(2,vi) ;

                /* SUPG stabilisation --- body force, nonlinear part from testfunction */

                /*  factor: -tauM*alphaF*gamma*dt
                    /                            \
                   |   n+af    /            \     |
                   |  f     , | Dacc o nabla | v  |
                   |           \            /     |
                    \                            /

                */
                elemat(vi*4    , ui*4)     -= fac_afgdt_tauM*edeadaf_(0)*funct_(ui)*derxy_(0,vi) ;
                elemat(vi*4    , ui*4 + 1) -= fac_afgdt_tauM*edeadaf_(0)*funct_(ui)*derxy_(1,vi) ;
                elemat(vi*4    , ui*4 + 2) -= fac_afgdt_tauM*edeadaf_(0)*funct_(ui)*derxy_(2,vi) ;
                elemat(vi*4 + 1, ui*4)     -= fac_afgdt_tauM*edeadaf_(1)*funct_(ui)*derxy_(0,vi) ;
                elemat(vi*4 + 1, ui*4 + 1) -= fac_afgdt_tauM*edeadaf_(1)*funct_(ui)*derxy_(1,vi) ;
                elemat(vi*4 + 1, ui*4 + 2) -= fac_afgdt_tauM*edeadaf_(1)*funct_(ui)*derxy_(2,vi) ;
                elemat(vi*4 + 2, ui*4)     -= fac_afgdt_tauM*edeadaf_(2)*funct_(ui)*derxy_(0,vi) ;
                elemat(vi*4 + 2, ui*4 + 1) -= fac_afgdt_tauM*edeadaf_(2)*funct_(ui)*derxy_(1,vi) ;
                elemat(vi*4 + 2, ui*4 + 2) -= fac_afgdt_tauM*edeadaf_(2)*funct_(ui)*derxy_(2,vi) ;

              } // end loop rows (test functions for matrix)
            } // end loop rows (solution for matrix, test function for vector)
          }
        }

        if(agls == Fluid3::viscous_stab_agls)
        {

          const double fac_two_visc_tauMp             = fac*2.0*visc*tauMp;
          const double fac_two_visc_afgdt_tauMp       = fac*2.0*visc*afgdt*tauMp;
          const double fac_two_visc_alphaM_tauMp      = fac*2.0*visc*alphaM*tauMp;
          const double fac_four_visc_visc_afgdt_tauMp = fac*4.0*visc*visc*afgdt*tauMp;

          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {
              /* viscous stabilisation --- inertia     */

              /* factor: -alphaM*tauMp*2*nu

                    /                    \
                   |                      |
                   |  Dacc , div eps (v)  |
                   |                      |
                    \                    /
              */
              elemat(vi*4    , ui*4    ) -= fac_two_visc_alphaM_tauMp*funct_(ui)*viscs2_(0,0,vi);
              elemat(vi*4    , ui*4 + 1) -= fac_two_visc_alphaM_tauMp*funct_(ui)*viscs2_(0,1,vi);
              elemat(vi*4    , ui*4 + 2) -= fac_two_visc_alphaM_tauMp*funct_(ui)*viscs2_(0,2,vi);
              elemat(vi*4 + 1, ui*4    ) -= fac_two_visc_alphaM_tauMp*funct_(ui)*viscs2_(0,1,vi);
              elemat(vi*4 + 1, ui*4 + 1) -= fac_two_visc_alphaM_tauMp*funct_(ui)*viscs2_(1,1,vi);
              elemat(vi*4 + 1, ui*4 + 2) -= fac_two_visc_alphaM_tauMp*funct_(ui)*viscs2_(1,2,vi);
              elemat(vi*4 + 2, ui*4    ) -= fac_two_visc_alphaM_tauMp*funct_(ui)*viscs2_(0,2,vi);
              elemat(vi*4 + 2, ui*4 + 1) -= fac_two_visc_alphaM_tauMp*funct_(ui)*viscs2_(1,2,vi);
              elemat(vi*4 + 2, ui*4 + 2) -= fac_two_visc_alphaM_tauMp*funct_(ui)*viscs2_(2,2,vi);


              /* viscous stabilisation --- convection */

              /*  factor: -2*nu*alphaF*gamma*dt*tauMp

                       /                                  \
                      |  / n+af       \                    |
                      | | u    o nabla | Dacc, div eps (v) |
                      |  \            /                    |
                       \                                  /

              */
              elemat(vi*4     , ui*4    )-= fac_two_visc_afgdt_tauMp*conv_c_af_(ui)*viscs2_(0, 0, vi) ;
              elemat(vi*4     , ui*4 + 1)-= fac_two_visc_afgdt_tauMp*conv_c_af_(ui)*viscs2_(0, 1, vi) ;
              elemat(vi*4     , ui*4 + 2)-= fac_two_visc_afgdt_tauMp*conv_c_af_(ui)*viscs2_(0, 2, vi) ;
              elemat(vi*4 + 1, ui*4     )-= fac_two_visc_afgdt_tauMp*conv_c_af_(ui)*viscs2_(0, 1, vi) ;
              elemat(vi*4 + 1, ui*4 + 1) -= fac_two_visc_afgdt_tauMp*conv_c_af_(ui)*viscs2_(1, 1, vi) ;
              elemat(vi*4 + 1, ui*4 + 2) -= fac_two_visc_afgdt_tauMp*conv_c_af_(ui)*viscs2_(1, 2, vi) ;
              elemat(vi*4 + 2, ui*4    ) -= fac_two_visc_afgdt_tauMp*conv_c_af_(ui)*viscs2_(0, 2, vi) ;
              elemat(vi*4 + 2, ui*4 + 1) -= fac_two_visc_afgdt_tauMp*conv_c_af_(ui)*viscs2_(1, 2, vi) ;
              elemat(vi*4 + 2, ui*4 + 2) -= fac_two_visc_afgdt_tauMp*conv_c_af_(ui)*viscs2_(2, 2, vi) ;

              /* viscous stabilisation --- diffusion  */

              /* factor: +4*nu*nu*alphaF*gamma*dt*tauMp

                    /                                   \
                   |               /    \                |
                   |  nabla o eps | Dacc | , div eps (v) |
                   |               \    /                |
                    \                                   /
              */

              elemat(vi*4    , ui*4    ) += fac_four_visc_visc_afgdt_tauMp*
                                            (viscs2_(0, 0, ui)*viscs2_(0, 0, vi)
                                             +
                                             viscs2_(0, 1, ui)*viscs2_(0, 1, vi)
                                             +
                                             viscs2_(0, 2, ui)*viscs2_(0, 2, vi)) ;
              elemat(vi*4    , ui*4 + 1) += fac_four_visc_visc_afgdt_tauMp*
                                            (viscs2_(0, 0, vi)*viscs2_(0, 1, ui)
                                             +
                                             viscs2_(0, 1, vi)*viscs2_(1, 1, ui)
                                             +
                                             viscs2_(0, 2, vi)*viscs2_(1, 2, ui)) ;
              elemat(vi*4    , ui*4 + 2) += fac_four_visc_visc_afgdt_tauMp*
                                            (viscs2_(0, 0, vi)*viscs2_(0, 2, ui)
                                             +
                                             viscs2_(0, 1, vi)*viscs2_(1, 2, ui)
                                             +
                                             viscs2_(0, 2, vi)*viscs2_(2, 2, ui)) ;
              elemat(vi*4 + 1, ui*4    ) += fac_four_visc_visc_afgdt_tauMp*
                                            (viscs2_(0, 0, ui)*viscs2_(0, 1, vi)
                                             +
                                             viscs2_(0, 1, ui)*viscs2_(1, 1, vi)
                                             +
                                             viscs2_(0, 2, ui)*viscs2_(1, 2, vi)) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_four_visc_visc_afgdt_tauMp*
                                            (viscs2_(0, 1, ui)*viscs2_(0, 1, vi)
                                             +
                                             viscs2_(1, 1, ui)*viscs2_(1, 1, vi)
                                             +
                                             viscs2_(1, 2, ui)*viscs2_(1, 2, vi)) ;
              elemat(vi*4 + 1, ui*4 + 2) += fac_four_visc_visc_afgdt_tauMp*
                                            (viscs2_(0, 1, vi)*viscs2_(0, 2, ui)
                                             +
                                             viscs2_(1, 1, vi)*viscs2_(1, 2, ui)
                                             +
                                             viscs2_(1, 2, vi)*viscs2_(2, 2, ui)) ;
              elemat(vi*4 + 2, ui*4    ) += fac_four_visc_visc_afgdt_tauMp*
                                            (viscs2_(0, 0, ui)*viscs2_(0, 2, vi)
                                             +
                                             viscs2_(0, 1, ui)*viscs2_(1, 2, vi)
                                             +
                                             viscs2_(0, 2, ui)*viscs2_(2, 2, vi)) ;
              elemat(vi*4 + 2, ui*4 + 1) += fac_four_visc_visc_afgdt_tauMp*
                                            (viscs2_(0, 1, ui)*viscs2_(0, 2, vi)
                                             +
                                             viscs2_(1, 1, ui)*viscs2_(1, 2, vi)
                                             +
                                             viscs2_(1, 2, ui)*viscs2_(2, 2, vi)) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_four_visc_visc_afgdt_tauMp*
                                            (viscs2_(0, 2, ui)*viscs2_(0, 2, vi)
                                             +
                                             viscs2_(1, 2, ui)*viscs2_(1, 2, vi)
                                             +
                                             viscs2_(2, 2, ui)*viscs2_(2, 2, vi)) ;


              /* viscous stabilisation --- pressure   */

              /* factor: -tauMp*2*nu

                    /                        \
                   |                          |
                   |  nabla Dp , div eps (v)  |
                   |                          |
                    \                        /
              */
              elemat(vi*4    , ui*4 + 3) -= fac_two_visc_tauMp*
                                            (derxy_(0,ui)*viscs2_(0,0,vi)
                                             +
                                             derxy_(1,ui)*viscs2_(0,1,vi)
                                             +
                                             derxy_(2,ui)*viscs2_(0,2,vi)) ;
              elemat(vi*4 + 1, ui*4 + 3) -= fac_two_visc_tauMp*
                                            (derxy_(0,ui)*viscs2_(0,1,vi)
                                             +
                                             derxy_(1,ui)*viscs2_(1,1,vi)
                                             +
                                             derxy_(2,ui)*viscs2_(1,2,vi)) ;
              elemat(vi*4 + 2, ui*4 + 3) -= fac_two_visc_tauMp*
                                            (derxy_(0,ui)*viscs2_(0,2,vi)
                                             +
                                             derxy_(1,ui)*viscs2_(1,2,vi)
                                             +
                                             derxy_(2,ui)*viscs2_(2,2,vi)) ;

            } // end loop rows (test functions for matrix)
          } // end loop columns (solution for matrix, test function for vector)
          if (newton)
          {
            for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
            {
              for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
              {
                /* viscous stabilisation --- convection */

                /*  factor: -2*nu*alphaF*gamma*dt*tauMp

                     /                                     \
                    |   /            \   n+af               |
                    |  | Dacc o nabla | u     , div eps (v) |
                    |   \            /                      |
                     \                                     /


                */

                elemat(vi*4     , ui*4    )-= fac_two_visc_afgdt_tauMp*
                                              (viscs2_(0, 0, vi)*conv_r_af_(0, 0, ui)
                                               +
                                               viscs2_(0, 1, vi)*conv_r_af_(1, 0, ui)
                                               +
                                               viscs2_(0, 2, vi)*conv_r_af_(2, 0, ui)) ;
                elemat(vi*4     , ui*4 + 1)-= fac_two_visc_afgdt_tauMp*
                                              (viscs2_(0, 0, vi)*conv_r_af_(0, 1, ui)
                                               +
                                               viscs2_(0, 1, vi)*conv_r_af_(1, 1, ui)
                                               +
                                               viscs2_(0, 2, vi)*conv_r_af_(2, 1, ui)) ;
                elemat(vi*4     , ui*4 + 2)-= fac_two_visc_afgdt_tauMp*
                                              (viscs2_(0, 0, vi)*conv_r_af_(0, 2, ui)
                                               +
                                               viscs2_(0, 1, vi)*conv_r_af_(1, 2, ui)
                                               +
                                               viscs2_(0, 2, vi)*conv_r_af_(2, 2, ui)) ;
                elemat(vi*4 + 1, ui*4     )-= fac_two_visc_afgdt_tauMp*
                                              (viscs2_(0, 1, vi)*conv_r_af_(0, 0, ui)
                                               +
                                               viscs2_(1, 1, vi)*conv_r_af_(1, 0, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(2, 0, ui)) ;
                elemat(vi*4 + 1, ui*4 + 1) -= fac_two_visc_afgdt_tauMp*
                                              (viscs2_(0, 1, vi)*conv_r_af_(0, 1, ui)
                                               +
                                               viscs2_(1, 1, vi)*conv_r_af_(1, 1, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(2, 1, ui)) ;
                elemat(vi*4 + 1, ui*4 + 2) -= fac_two_visc_afgdt_tauMp*
                                              (viscs2_(0, 1, vi)*conv_r_af_(0, 2, ui)
                                               +
                                               viscs2_(1, 1, vi)*conv_r_af_(1, 2, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(2, 2, ui)) ;
                elemat(vi*4 + 2, ui*4    ) -= fac_two_visc_afgdt_tauMp*
                                              (viscs2_(0, 2, vi)*conv_r_af_(0, 0, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(1, 0, ui)
                                               +
                                               viscs2_(2, 2, vi)*conv_r_af_(2, 0, ui)) ;
                elemat(vi*4 + 2, ui*4 + 1) -= fac_two_visc_afgdt_tauMp*
                                              (viscs2_(0, 2, vi)*conv_r_af_(0, 1, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(1, 1, ui)
                                               +
                                               viscs2_(2, 2, vi)*conv_r_af_(2, 1, ui)) ;
                elemat(vi*4 + 2, ui*4 + 2) -= fac_two_visc_afgdt_tauMp*
                                              (viscs2_(0, 2, vi)*conv_r_af_(0, 2, ui)
                                               +
                                               viscs2_(1, 2, vi)*conv_r_af_(1, 2, ui)
                                               +
                                               viscs2_(2, 2, vi)*conv_r_af_(2, 2, ui)) ;
              } // end loop rows (test functions for matrix)
            } // end loop columns (solution for matrix, test function for vector)
          }
        }

        if(cstab == Fluid3::continuity_stab_yes)
        {
          const double fac_gamma_dt_tauC = fac*gamma*dt*tauC;

          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {
              /*  factor: +gamma*dt*tauC

                    /                          \
                   |                            |
                   | nabla o Dacc  , nabla o v  |
                   |                            |
                    \                          /
              */

              elemat(vi*4    , ui*4    ) += fac_gamma_dt_tauC*derxy_(0,ui)*derxy_(0,vi) ;
              elemat(vi*4    , ui*4 + 1) += fac_gamma_dt_tauC*derxy_(1,ui)*derxy_(0,vi) ;
              elemat(vi*4    , ui*4 + 2) += fac_gamma_dt_tauC*derxy_(2,ui)*derxy_(0,vi) ;
              elemat(vi*4 + 1, ui*4    ) += fac_gamma_dt_tauC*derxy_(0,ui)*derxy_(1,vi) ;
              elemat(vi*4 + 1, ui*4 + 1) += fac_gamma_dt_tauC*derxy_(1,ui)*derxy_(1,vi) ;
              elemat(vi*4 + 1, ui*4 + 2) += fac_gamma_dt_tauC*derxy_(2,ui)*derxy_(1,vi) ;
              elemat(vi*4 + 2, ui*4    ) += fac_gamma_dt_tauC*derxy_(0,ui)*derxy_(2,vi) ;
              elemat(vi*4 + 2, ui*4 + 1) += fac_gamma_dt_tauC*derxy_(1,ui)*derxy_(2,vi) ;
              elemat(vi*4 + 2, ui*4 + 2) += fac_gamma_dt_tauC*derxy_(2,ui)*derxy_(2,vi) ;
            } // end loop rows vi (test functions for matrix)
          } // end loop columns ui (solution for matrix, test function for vector)
        } // end cstab

        if(cross == Fluid3::cross_stress_stab)
        {
          //---------------------------------------------------------------
          //
          //                     STABILISATION PART
          //       RESIDUAL BASED VMM STABILISATION --- CROSS STRESS
          //
          //---------------------------------------------------------------

          for (int ui=0; ui<iel_; ++ui) // loop columns (solution for matrix, test function for vector)
          {
            for (int vi=0; vi<iel_; ++vi)  // loop rows (test functions for matrix)
            {

              /*  factor:
              
               -alphaF*gamma*dt*tauM

                          /                          \
                         |  /            \            |
                         | | resM o nabla | Dacc , v  |
                         |  \            /            |
                          \                          /
              */
              elemat(vi*4    , ui*4    ) -= fac*afgdt*tauM*conv_resM_(ui)*funct_(vi) ;
              elemat(vi*4 + 1, ui*4 + 1) -= fac*afgdt*tauM*conv_resM_(ui)*funct_(vi) ;
              elemat(vi*4 + 2, ui*4 + 2) -= fac*afgdt*tauM*conv_resM_(ui)*funct_(vi) ;
            } // end loop rows (test functions for matrix)
          } // end loop columns (solution for matrix, test function for vector)
        } // end cross
        
      } // end if compute_elemat

      //---------------------------------------------------------------
      //---------------------------------------------------------------
      //
      //                       RIGHT HAND SIDE
      //
      //---------------------------------------------------------------
      //---------------------------------------------------------------

      for (int ui=0; ui<iel_; ++ui) // loop rows  (test functions)
      {
        /* inertia terms */

        /*  factor: +1

               /             \
              |     n+am      |
              |  acc     , v  |
              |               |
               \             /
        */

        elevec[ui*4    ] -= fac*funct_(ui)*accintam_(0) ;
        elevec[ui*4 + 1] -= fac*funct_(ui)*accintam_(1) ;
        elevec[ui*4 + 2] -= fac*funct_(ui)*accintam_(2) ;

        /* convection */

        /*  factor: +1

               /                             \
              |  / n+af       \    n+af       |
              | | u    o nabla |  u      , v  |
              |  \            /               |
               \                             /
        */

        elevec[ui*4    ] -= fac*(velintaf_(0)*conv_r_af_(0,0,ui)
                                 +
                                 velintaf_(1)*conv_r_af_(0,1,ui)
                                 +
                                 velintaf_(2)*conv_r_af_(0,2,ui)) ;
        elevec[ui*4 + 1] -= fac*(velintaf_(0)*conv_r_af_(1,0,ui)
                                 +
                                 velintaf_(1)*conv_r_af_(1,1,ui)
                                 +
                                 velintaf_(2)*conv_r_af_(1,2,ui)) ;
        elevec[ui*4 + 2] -= fac*(velintaf_(0)*conv_r_af_(2,0,ui)
                                 +
                                 velintaf_(1)*conv_r_af_(2,1,ui)
                                 +
                                 velintaf_(2)*conv_r_af_(2,2,ui)) ;

        /* pressure */

        /*  factor: -1

               /                  \
              |   n+1              |
              |  p    , nabla o v  |
              |                    |
               \                  /
        */

        elevec[ui*4    ] += fac*prenp_*derxy_(0,ui) ;
        elevec[ui*4 + 1] += fac*prenp_*derxy_(1,ui) ;
        elevec[ui*4 + 2] += fac*prenp_*derxy_(2,ui) ;

        /* viscous term */

        /*  factor: +2*nu

               /                            \
              |       / n+af \         / \   |
              |  eps | u      | , eps | v |  |
              |       \      /         \ /   |
               \                            /
        */

        elevec[ui*4    ] -= visc*fac*
                            (derxy_(0,ui)*vderxyaf_(0,0)*2.0
                             +
                             derxy_(1,ui)*vderxyaf_(0,1)
                             +
                             derxy_(1,ui)*vderxyaf_(1,0)
                             +
                             derxy_(2,ui)*vderxyaf_(0,2)
                             +
                             derxy_(2,ui)*vderxyaf_(2,0)) ;
        elevec[ui*4 + 1] -= visc*fac*
                            (derxy_(0,ui)*vderxyaf_(0,1)
                             +
                             derxy_(0,ui)*vderxyaf_(1,0)
                             +
                             derxy_(1,ui)*vderxyaf_(1,1)*2.0
                             +
                             derxy_(2,ui)*vderxyaf_(1,2)
                             +
                             derxy_(2,ui)*vderxyaf_(2,1)) ;
        elevec[ui*4 + 2] -= visc*fac*
                            (derxy_(0,ui)*vderxyaf_(0,2)
                             +
                             derxy_(0,ui)*vderxyaf_(2,0)
                             +
                             derxy_(1,ui)*vderxyaf_(1,2)
                             +
                             derxy_(1,ui)*vderxyaf_(2,1)
                             +
                             derxy_(2,ui)*vderxyaf_(2,2)*2.0) ;

        /* body force (dead load...) */

        /*  factor: -1

               /           \
              |   n+af      |
              |  f     , v  |
              |             |
               \           /
        */

        elevec[ui*4    ] += fac*funct_(ui)*edeadaf_(0);
        elevec[ui*4 + 1] += fac*funct_(ui)*edeadaf_(1);
        elevec[ui*4 + 2] += fac*funct_(ui)*edeadaf_(2);

        /* continuity equation */

        /*  factor: +1

               /                \
              |          n+1     |
              | nabla o u   , q  |
              |                  |
               \                /
        */

        elevec[ui*4 + 3] -= fac*funct_(ui)*divunp;

      } // end loop rows

      if(pspg == Fluid3::pstab_use_pspg)
      {
        const double fac_tauMp = fac*tauMp;

        for (int ui=0; ui<iel_; ++ui) // loop rows  (test functions)
        {
          /*
            factor: +tauMp

            pressure stabilisation --- inertia


                  /                  \
                 |     n+am           |
                 |  acc    , nabla q  |
                 |                    |
                  \                  /

            pressure stabilisation --- convection


                  /                                   \
                 |  / n+af       \    n+af             |
                 | | u    o nabla |  u      , nabla q  |
                 |  \            /                     |
                  \                                   /


            pressure stabilisation --- diffusion

                  /                                  \
                 |               / n+af \             |
                 |  nabla o eps | u      | , nabla q  |
                 |               \      /             |
                  \                                  /

            pressure stabilisation --- pressure

                  /                      \
                 |         n+1            |
                 |  nabla p    , nabla q  |
                 |                        |
                  \                      /


            pressure stabilisation --- bodyforce
                  /                 \
                 |    n+af           |
                 |  f     , nabla q  |
                 |                   |
                  \                 /
          */
          elevec[ui*4 + 3] -= fac_tauMp*
                              (derxy_(0,ui)*resM_(0)
                               +
                               derxy_(1,ui)*resM_(1)
                               +
                               derxy_(2,ui)*resM_(2));
        } // end loop rows
      }

      if(supg == Fluid3::convective_stab_supg)
      {
        const double fac_tauM = fac*tauM;

        for (int ui=0; ui<iel_; ++ui) // loop rows  (test functions)
        {
          /*
            factor: +tauM

            SUPG stabilisation --- inertia

                  /                              \
                 |     n+am   / n+af        \     |
                 |  acc    , | u     o nabla | v  |
                 |            \             /     |
                  \                              /

           SUPG stabilisation --- convection


                  /                                                \
                 |    / n+af        \   n+af    / n+af        \     |
                 |   | u     o nabla | u     , | u     o nabla | v  |
                 |    \             /           \             /     |
                  \                                                /


           SUPG stabilisation --- diffusion

                  /                                               \
                 |               / n+af \      / n+af        \     |
                 |  nabla o eps | u      |  , | u     o nabla | v  |
                 |               \      /      \             /     |
                  \                                               /

           SUPG stabilisation --- pressure

                  /                                  \
                 |         n+1    / n+af        \     |
                 |  nabla p    , | u     o nabla | v  |
                 |                \             /     |
                  \                                  /

           SUPG stabilisation --- bodyforce


                  /                             \
                 |   n+af    / n+af        \     |
                 |  f     , | u     o nabla | v  |
                 |           \             /     |
                  \                             /
          */

          elevec[ui*4    ] -= fac_tauM*conv_c_af_(ui)*resM_(0) ;
          elevec[ui*4 + 1] -= fac_tauM*conv_c_af_(ui)*resM_(1) ;
          elevec[ui*4 + 2] -= fac_tauM*conv_c_af_(ui)*resM_(2) ;

        } // end loop rows
      }

      if(agls == Fluid3::viscous_stab_agls || agls == Fluid3::viscous_stab_agls_only_rhs)
      {
        const double fac_two_visc_tauMp = fac*2.0*visc*tauMp;

        for (int ui=0; ui<iel_; ++ui) // loop rows  (test functions)
        {
          /*
            factor: -tauMp*2*nu


            viscous stabilisation --- inertia


                 /                         \
                |      n+am                 |
                |  Dacc      , div eps (v)  |
                |                           |
                 \                         /

            viscous stabilisation --- convection

            /                                     \
           |  / n+af       \    n+af               |
           | | u    o nabla |  u     , div eps (v) |
           |  \            /                       |
            \                                     /

            viscous stabilisation --- diffusion

               /                                      \
              |               /  n+af \                |
              |  nabla o eps |  u      | , div eps (v) |
              |               \       /                |
               \                                      /

            viscous stabilisation --- pressure

                 /                           \
                |                             |
                |  nabla p , nabla o eps (v)  |
                |                             |
                 \                           /

           viscous stabilisation --- bodyforce

                  /                         \
                 |    n+af                   |
                 |  f     ,  nabla o eps (v) |
                 |                           |
                  \                         /
          */
          elevec[ui*4    ] += fac_two_visc_tauMp*
                              (resM_(0)*viscs2_(0, 0, ui)
                               +
                               resM_(1)*viscs2_(0, 1, ui)
                               +
                               resM_(2)*viscs2_(0, 2, ui)) ;
          elevec[ui*4 + 1] += fac_two_visc_tauMp*
                              (resM_(0)*viscs2_(0, 1, ui)
                               +
                               resM_(1)*viscs2_(1, 1, ui)
                               +
                               resM_(2)*viscs2_(1, 2, ui)) ;
          elevec[ui*4 + 2] += fac_two_visc_tauMp*
                              (resM_(0)*viscs2_(0, 2, ui)
                               +
                               resM_(1)*viscs2_(1, 2, ui)
                               +
                               resM_(2)*viscs2_(2, 2, ui)) ;
        } // end loop rows ui
      } // end agls

      if(cstab == Fluid3::continuity_stab_yes)
      {
        const double fac_tauC = fac*tauC;
        for (int ui=0; ui<iel_; ++ui) // loop rows  (test functions)
        {
          /* factor: +tauC

                  /                          \
                 |           n+1              |
                 |  nabla o u    , nabla o v  |
                 |                            |
                  \                          /
          */

          elevec[ui*4    ] -= fac_tauC*divunp*derxy_(0,ui) ;
          elevec[ui*4 + 1] -= fac_tauC*divunp*derxy_(1,ui) ;
          elevec[ui*4 + 2] -= fac_tauC*divunp*derxy_(2,ui) ;
        } // end loop rows
      } // end cstab

      if(cross == Fluid3::cross_stress_stab_only_rhs || cross == Fluid3::cross_stress_stab)
      {
        const double fac_tauM = fac*tauM;
        for (int ui=0; ui<iel_; ++ui) // loop rows  (test functions)
        {
          /* factor: +tauM

                  /                            \
                 |                    n+af      |
                 |  ( resM o nabla ) u    ,  v  |
                 |                    (i)       |
                  \                            /
          */

          elevec[ui*4    ] += fac_tauM*(resM_(0)*vderxyaf_(0,0)
                                        +
                                        resM_(1)*vderxyaf_(0,1)
                                        +
                                        resM_(2)*vderxyaf_(0,2))*funct_(ui);
          elevec[ui*4 + 1] += fac_tauM*(resM_(0)*vderxyaf_(1,0)
                                        +
                                        resM_(1)*vderxyaf_(1,1)
                                        +
                                        resM_(2)*vderxyaf_(1,2))*funct_(ui);
          elevec[ui*4 + 2] += fac_tauM*(resM_(0)*vderxyaf_(2,0)
                                        +
                                        resM_(1)*vderxyaf_(2,1)
                                        +
                                        resM_(2)*vderxyaf_(2,2))*funct_(ui);

        } // end loop rows
      }

      if(reynolds == Fluid3::reynolds_stress_stab_only_rhs)
      {
        const double fac_tauM_tauM = fac*tauM*tauM;
        for (int ui=0; ui<iel_; ++ui) // loop rows  (test functions)
        {
          /* factor: -tauM*tauM

                  /                             \
                 |                               |
                 |  resM   , ( resM o nabla ) v  |
                 |                               |
                  \                             /
          */
          elevec[ui*4    ] += fac_tauM_tauM*conv_resM_(ui)*resM_(0);
          elevec[ui*4 + 1] += fac_tauM_tauM*conv_resM_(ui)*resM_(1);
          elevec[ui*4 + 2] += fac_tauM_tauM*conv_resM_(ui)*resM_(2);
        } // end loop rows
      }


    }
  } // end loop iquad
  return;
}



/*----------------------------------------------------------------------*
 |  get the body force in the nodes of the element (private) gammi 04/07|
 |  the Neumann condition associated with the nodes is stored in the    |
 |  array edeadng only if all nodes have a VolumeNeumann condition      |
 *----------------------------------------------------------------------*/
void DRT::Elements::Fluid3GenalphaResVMM::GetNodalBodyForce(Fluid3* ele, const double time)
{
  vector<DRT::Condition*> myneumcond;
  DRT::Node** nodes = ele->Nodes();

  // check whether all nodes have a unique VolumeNeumann condition
  int nodecount = 0;
  for (int inode=0;inode<iel_;inode++)
  {
    nodes[inode]->GetCondition("VolumeNeumann",myneumcond);

    if (myneumcond.size()>1)
    {
      dserror("more than one VolumeNeumann cond on one node");
    }
    if (myneumcond.size()==1)
    {
      nodecount++;
    }
  }

  if (nodecount == iel_)
  {
    // find out whether we will use a time curve
    const vector<int>* curve  = myneumcond[0]->Get<vector<int> >("curve");
    int curvenum = -1;

    if (curve) curvenum = (*curve)[0];

    // initialisation
    double curvefac    = 0.0;

    if (curvenum >= 0) // yes, we have a timecurve
    {
      // time factor for the intermediate step
      if(time >= 0.0)
      {
        curvefac = DRT::Utils::TimeCurveManager::Instance().Curve(curvenum).f(time);
      }
      else
      {
	// do not compute an "alternative" curvefac here since a negative time value
	// indicates an error.
        dserror("Negative time value in body force calculation: time = %f",time);
        //curvefac = DRT::Utils::TimeCurveManager::Instance().Curve(curvenum).f(0.0);
      }
    }
    else // we do not have a timecurve --- timefactors are constant equal 1
    {
      curvefac = 1.0;
    }

    // set this condition to the edeadng array
    for (int jnode=0; jnode<iel_; jnode++)
    {
      nodes[jnode]->GetCondition("VolumeNeumann",myneumcond);

      // get values and switches from the condition
      const vector<int>*    onoff = myneumcond[0]->Get<vector<int> >   ("onoff");
      const vector<double>* val   = myneumcond[0]->Get<vector<double> >("val"  );

      for(int isd=0;isd<3;isd++)
      {
        edeadaf_(isd,jnode) = (*onoff)[isd]*(*val)[isd]*curvefac;
      }
    }
  }
  else
  {
    // we have no dead load
    edeadaf_ = 0.;
  }
  return;
}








#endif
#endif

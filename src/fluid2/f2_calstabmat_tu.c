/*!----------------------------------------------------------------------
\file
\brief stabilisation part of element stiffness matrix for fluid2

------------------------------------------------------------------------*/
#ifdef D_FLUID2 
#include "../headers/standardtypes.h"
#include "fluid2_prototypes.h"
#include "fluid2.h"
#include "fluid2_tu.h"
/*!---------------------------------------------------------------------
\brief evaluate stabilisaton part of Kkapeps

<pre>                                                         he  12/02

In this routine the stabilisation part of matrix Kvv is calculated:

    /
   |  tau_tu * u * grad(kapeps) * grad(psi) * u   d_omega    +  D. C.
  /

    /
   |  -tau_tu * div[(nue+nue_t/sig)*grad(kapeps)] * grad(psi) * u   d_omega  +  D. C.
  /
  
    /
   |  tau_tu * factor * kapeps_old * kapeps * grad(psi) * u    d_omega   +  D. C.
  /  

LOW-REYNOLD's MODEL only for kappa:

    /
   |  tau_tu *2.0*visc*[ 2*grad(k_old)/(4*k_old)  * grad (k) - grad(k_old)*grad(k_old)/(4*k_old^2) * k ] *  grad(psi) * ud_omega  +  D. C.
  /
  

NOTE: there's only one elestif
      --> Kkapeps is stored in estif[0..(iel-1)][0..(iel-1)]
      
</pre>
\param  *ele	     ELEMENT	   (i)	   actual element
\param  *elev	     ELEMENT	   (i)	   actual element for vel.
\param  *dynvar        FLUID_DYN_CALC  (i)
\param **estif          double	   (i/o)   ele stiffness matrix
\param   kapepsint      double	   (i)     kapeps at integr. point
\param  *velint         double	   (i)     vel. at integr. point
\param  *velint_dc      double	   (i)     vel. at integr. point for DISC. CAPT.
\param   eddyint        double	   (i)     eddy at integr. point
\param  *kapepsderxy    double	   (i)     kapeps deriv. at integr. point
\param  *funct          double	   (i)     nat. shape functions
\param **derxy          double	   (i)     global derivatives
\param **derxy2         double	   (i)     2nd global derivatives
\param   fac	      double	   (i)     weighting factor	   
\param   visc	      double	   (i)     fluid viscosity
\param   factor	      double	   (i)     factor
\param   sig	      double	   (i)     factor
\param   iel	      int		   (i)     num. of nodes in ele
\return void                                                                       

------------------------------------------------------------------------*/
void f2_calstabkkapeps(			      
                ELEMENT         *ele,    
		    ELEMENT         *elev,    
                FLUID_DYN_CALC  *dynvar,
		    double         **estif,  
		    double           kapepsint, 
		    double          *velint, 
		    double          *velint_dc, 
                double           eddyint, 
                double          *kapepsderxy, 
                double          *funct,  
		    double         **derxy,  
		    double         **derxy2, 
		    double           fac,    
		    double           visc,   
		    double           factor,
		    double           sig,
                int              iel    
                   )
{
/*----------------------------------------------------------------------*
 | NOTATION:                                                            |
 |   irow - row number in element matrix                                |
 |   icol - column number in element matrix                             |
 |   irn  - row node: number of node considered for matrix-row          |
 |   ird  - row dim.: number of spatial dimension at row node           |  
/*----------------------------------------------------------------------*/
int    irow,icol,irn,icn,ird;
double taumu,taumu_dc;
double c,c_dc;
double auxc,aux;

#ifdef DEBUG 
dstrc_enter("f2_calstabkkapeps");
#endif

/*---------------------------------------- set stabilisation parameter */
taumu     = dynvar->tau_tu;
taumu_dc  = dynvar->tau_tu_dc;
c    = fac*taumu;
c_dc = fac*taumu_dc;
/*----------------------------------------------------------------------*
    /
   | tau_tu * u * grad(kapeps) * grad(psi) * u  d_omega
  /
 *----------------------------------------------------------------------*/
 icol=0;
   for(icn=0;icn<iel;icn++)
   {
     irow=0;
     auxc = velint[0]*derxy[0][icn]+velint[1]*derxy[1][icn];

      for(irn=0;irn<iel;irn++)
      {
         estif[irow][icol] += auxc*c   *(velint[0]   *derxy[0][irn]+velint[1]   *derxy[1][irn]);
         estif[irow][icol] += auxc*c_dc*(velint_dc[0]*derxy[0][irn]+velint_dc[1]*derxy[1][irn]);  
         irow += 1;
      } /* end of loop over irn */
      icol += 1;
   } /* end of loop over icn */

/*----------------------------------------------------------------------*
    /
   |  -tau_tu * div[(nue+nue_t/sig)*grad(kapeps)] * grad(psi) * u   d_omega =
  /
 
    /
   |  -tau_tu *  (nue+nue_t/sig) *  div grad(kapeps) * grad(psi) * u   d_omega
  /                                    
 
 *----------------------------------------------------------------------*/
   icol=0;
      for (icn=0;icn<iel;icn++)
      {
 	 irow = 0;
       auxc = (visc+eddyint/sig)*(derxy2[0][icn]+derxy2[1][icn]);

	 for (irn=0;irn<iel;irn++)
	 {          
          estif[irow][icol] -= auxc*c   *(derxy[0][irn]*velint[0]   +derxy[1][irn]*velint[1]);
          estif[irow][icol] -= auxc*c_dc*(derxy[0][irn]*velint_dc[0]+derxy[1][irn]*velint_dc[1]);  
	    irow += 1;
	 } /* end of loop over irn */
	 icol += 1;
      } /* end of loop over icn */

/*----------------------------------------------------------------------*
    /
   |  tau_tu * factor * kapeps_old * kapeps * grad(psi) * u   d_omega
  /
 *----------------------------------------------------------------------*/
      icol=0;
      for (icn=0;icn<iel;icn++)
      {
	 irow = 0;
       auxc = factor*kapepsint*funct[icn];

	 for (irn=0;irn<iel;irn++)
	 {
          estif[irow][icol]     += auxc*c   *(velint[0]   *derxy[0][irn] + velint[1]   *derxy[1][irn]);
          estif[irow][icol]     += auxc*c_dc*(velint_dc[0]*derxy[0][irn] + velint_dc[1]*derxy[1][irn]); 
          irow += 1;
	 } /* end of loop over irn */
	 icol += 1;
      } /* end of loop over icn */


if(dynvar->kapeps_flag==0) 
{
/*----------------------------------------------------------------------*
LOW-REYNOLD's MODEL:

    /
   |  tau_tu * 2*grad(k_old)/(4*k_old) * 2.0 * visc * grad k* grad(psi) * u  d_omega
  /
*----------------------------------------------------------------------*/
      icol=0;
      for (icn=0;icn<iel;icn++)
      {
	 irow = 0;
       
       auxc  =  2/(4*kapepsint)*
               (kapepsderxy[0]*derxy[0][icn]+kapepsderxy[1]*derxy[1][icn]);
       auxc -= (pow(kapepsderxy[0],2)+pow(kapepsderxy[1],2))/
               (4*kapepsint*kapepsint) * funct[icn];

	 for (irn=0;irn<iel;irn++)
	 {
          estif[irow][icol]   += 2.0*visc*c   *auxc*(velint[0]   *derxy[0][irn] + velint[1]   *derxy[1][irn]);
          estif[irow][icol]   += 2.0*visc*c_dc*auxc*(velint_dc[0]*derxy[0][irn] + velint_dc[1]*derxy[1][irn]); 
          irow += 1;
	 } /* end of loop over irn */
	 icol += 1;
      } /* end of loop over icn */

} /* endif */


/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif

return;
} /* end of f2_calstabkvv */


/*!--------------------------------------------------------------------- 
\brief evaluate stabilisaton part of Mkapeps

<pre>                                                         he  12/02

In this routine the stabilisation part of matrix Mkapeps is calculated:

    /
   |   tau_tu * grad(psi) * u * kapeps  d_omega + D. C.
  /
  
  
NOTE: there's only one elestif  			    
      --> Mkapeps is stored in emass[0..(iel-1)][0..(iel-1)]
      
</pre>
\param  *ele	 ELEMENT	     (i)	   actual element
\param  *dynvar    FLUID_DYN_CALC  (i)
\param **emass     double	   (i/o)   ele mass matrix
\param  *velint    double	   (i)     vel. at integr. point
\param  *velint_dc double	   (i)     vel. at integr. point for D.C.
\param  *funct     double	   (i)     nat. shape functions
\param **derxy     double	   (i)     global derivatives
\param   fac	   double	   (i)     weighting factor
\param   iel	   int  	   (i)	   num. of nodes in ele
\return void                                                                       

------------------------------------------------------------------------*/
void f2_calstabmkapeps(
                    ELEMENT         *ele,     
		        FLUID_DYN_CALC  *dynvar,
		        double         **emass,  
    		        double          *velint, 
    		        double          *velint_dc, 
                    double          *funct,  
		        double         **derxy,  
		        double           fac,    
		        int              iel    
                     )
{
/*----------------------------------------------------------------------*
 | NOTATION:                                                            |
 |   irow - row number in element matrix                                |
 |   icol - column number in element matrix                             |
 |   irn  - row node: number of node considered for matrix-row          |
 |   ird  - row dim.: number of spatial dimension at row node           |   
/*----------------------------------------------------------------------*/
int    irow,icol,irn,icn;
double taumu,taumu_dc;
double c,c_dc;
double auxc;

#ifdef DEBUG 
dstrc_enter("f2_calstabmkapeps");
#endif

/*---------------------------------------- set stabilisation parameter */
taumu     = dynvar->tau_tu;
taumu_dc  = dynvar->tau_tu_dc;

c    = fac * taumu;
c_dc = fac * taumu_dc;

/*----------------------------------------------------------------------*
   Calculate convection stabilisation part:
    /
   |   tau_tu * grad(psi) * u * kapeps  d_omega
  /
 *----------------------------------------------------------------------*/
   icol=0;
   for (icn=0;icn<iel;icn++)
   {
      irow = 0;
      auxc = funct[icn];

      for (irn=0;irn<iel;irn++)
      {
	 emass[irow][icol] += auxc*c   *(velint[0]   *derxy[0][irn]+velint[1]   *derxy[1][irn]);
	 emass[irow][icol] += auxc*c_dc*(velint_dc[0]*derxy[0][irn]+velint_dc[1]*derxy[1][irn]); 
	 irow += 1;
      } /* end loop over irn */
      icol += 1;      
   } /* end loop over icn */

/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif

return;
} /* end of f2_calstabmvv */

#endif

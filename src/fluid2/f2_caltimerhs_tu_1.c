/*!----------------------------------------------------------------------
\file
\brief time RHS for fluid2 element

------------------------------------------------------------------------*/
#ifdef D_FLUID2 
#include "../headers/standardtypes.h"
#include "fluid2_prototypes.h"
#include "fluid2_tu.h"
/*!--------------------------------------------------------------------- 
\brief galerkin part of time forces for kapome dof

<pre>                                                         he  02/03

In this routine the galerkin part of the time forces for kapome dof
is calculated:

        /
   (+) |  kapome * psi     d_omega
      /  
               

                      /
   (-) (1-THETA)*dt  |  u * grad(kapome) * psi     d_omega
                    /
		  
                      /
   (-) (1-THETA)*dt  |  (nue+nue_t*sig) *grad(kapome) * grad(psi)  d_omega
                    /  
		  
                      /
   (-) (1-THETA)*dt  |  factor * kapome^2 * psi d_omega
                    /		  		      

                      /
   (+) (1-THETA)*dt  |  0.5 * factor1* eddyint * ((grad(u) + [grad(u)]^T)^2 * psi d_omega
                    /		  		      

                     / 
   (+) (1-THETA)*dt | factor2 * (kapome)^2 * psi  d_omega
                   /  


</pre>
\param   *dynvar      FLUID_DYN_CALC  (i)
\param   *eforce      double	      (i/o)  element force vector
\param    kapomeint   double	      (i)    kapome at integr. point
\param   *velint      double	      (i)    vel. at integr. point
\param    eddyint     double	      (i)    eddy-visc. at integr. point
\param   *funct       double	      (i)    nat. shape functions      
\param  **derxy       double	      (i)    global derivatives
\param  **vderxy      double	      (i)    global vel. deriv.
\param   *kapomederxy double	      (i)    global kapome deriv.
\param    visc	    double	      (i)    fluid viscosity
\param    fac	    double	      (i)    weighting factor
\param    factor	    double	      (i)    factor
\param    factor1	    double	      (i)    factor
\param    factor2	    double	      (i)    factor
\param    sig	    double	      (i)    factor
\param    production  double	      (i)    factor
\param    iel	    int	      (i)    num. of nodes in ele
\return void                                                                       

------------------------------------------------------------------------*/
void f2_calgaltfkapome(
                  FLUID_DYN_CALC  *dynvar, 
                  double          *eforce,    
		      double           kapomeint,  
                  double          *velint,   
		      double           eddyint,
                  double          *funct,    
		      double         **derxy,    
		      double         **vderxy,   
		      double          *kapomederxy,   
                  double           visc,     
		      double           fac,      
                  double           factor,  
                  double           factor1,  
                  double           factor2,  
                  double           sig,  
                  double           production,  
                  int              iel       
                  )  
{
int    j,irow,isd,inode;  
double c;
double aux;
double facsr;

#ifdef DEBUG 
dstrc_enter("f2_calgaltfkapome");
#endif


/*--------------------------------------------------- set some factors */
facsr = fac * dynvar->thsr;

/*----------------------------------------------------------------------*
   Calculate intertia forces of time force vector:
      /
     |  kapome * psi     d_omega
    /  
 *----------------------------------------------------------------------*/ 
irow=-1;
for (inode=0;inode<iel;inode++)
{
      irow++;
      eforce[irow] += funct[inode]*kapomeint*fac;
} /* end of loop over inode */

/*----------------------------------------------------------------------*
   Calculate  forces of time force vector:
    
                      /
   (-) (1-THETA)*dt  |  u * grad(kapome) * psi     d_omega
                    /
*----------------------------------------------------------------------*/
irow=0;
for (inode=0;inode<iel;inode++)
{
 aux = funct[inode]*facsr;

   for (isd=0;isd<2;isd++)
   {
      eforce[irow] -= aux*kapomederxy[isd]*velint[isd];
   } /* end of loop over isd */
 irow++;
} /* end of loop over irwo */

/*----------------------------------------------------------------------*
   Calculate  forces of time force vector:
                      /
   (-) (1-THETA)*dt  |  (nue+nue_t*sig) *grad(kapome) * grad(psi)  d_omega
                    /  
 *----------------------------------------------------------------------*/ 
irow=0;
for (inode=0;inode<iel;inode++)
{
 aux = (visc+eddyint*sig)*facsr;

   for (isd=0;isd<2;isd++)
   {
      eforce[irow] -= aux*derxy[isd][inode]*kapomederxy[isd];
   } /* end of loop over isd */
 irow++;
} /* end of loop over inode */

/*----------------------------------------------------------------------*
   Calculate  forces of time force vector:
                      /
   (-) (1-THETA)*dt  |  factor * kapome^2 * psi d_omega
                    /		  		      
 *----------------------------------------------------------------------*/ 
  irow=-1;
   for (inode=0;inode<iel;inode++)
   {
      irow++;
      eforce[irow] -= factor*funct[inode]*pow(kapomeint,2)*facsr;
   } /* end of loop over inode */

/*----------------------------------------------------------------------*
   Calculate  forces of time force vector:
                      /
   (+) (1-THETA)*dt  |  0.5 * factor1 * eddyint* ((grad(u) + [grad(u)]^T)^2 * psi d_omega
                    /		  		      
 *----------------------------------------------------------------------*/ 
irow=-1;
for (inode=0;inode<iel;inode++)
{
      irow++;
      eforce[irow] += factor1*eddyint*production*funct[inode]*facsr;
} /* end of loop over inode */

/*----------------------------------------------------------------------*
   Calculate  forces of time force vector:
                      /
   (+) (1-THETA)*dt  |  factor2 * kapome^2 * psi d_omega
                    /		  		      
*----------------------------------------------------------------------*/ 
  irow=-1;
   for (inode=0;inode<iel;inode++)
   {
         irow++;
         eforce[irow] += factor2*funct[inode]*pow(kapomeint,2)*facsr;
   } /* end of loop over inode */

 
/*----------------------------------------------------------------------*/ 
#ifdef DEBUG 
dstrc_exit();
#endif

return;
} /* end of f2_calgaltfv */


/*!--------------------------------------------------------------------- 
\brief stabilisation part of time forces for kapome dof

<pre>                                                         he  02/03

In this routine the stabilisation part of the time forces for kapome dofs
is calculated:
		 		                   	  		      
           
                     /
                (+) | tau_tu * u * kapome * grad(psi)  d_omega + D. C.
                   /  


                     /
   (-) (1-THETA)*dt |  tau_tu * u * grad(kapome) * u * grad(psi) d_omega  + D. C.
                   /          
    
                     /
   (+) (1-THETA)*dt |  tau_tu * div((nue+nue_t*sig)*grad(kapome)) * u * grad(psi)  d_omega  + D. C.
                   /

                     /
   (-) (1-THETA)*dt |  tau_tu * factor * kapome^2 * grad(psi) * u  d_omega  + D. C.
                   /


                     /
   (+) (1-THETA)*dt |  tau_tu * 0.5 * nue_t * factor1 * (grad(u)+[grad(u)^T])^2 *grad(psi) * u   d_omega
                   /   
      	 
                     /
   (+) (1-THETA)*dt |  tau_tu * factor2 * kapome^2 * grad(psi) * u  d_omega
                   /
		 	   		
       
</pre>
\param   *dynvar        FLUID_DYN_CALC    (i)
\param   *ele           ELEMENT	      (i)    actual element
\param   *eforce        double	      (i/o)  element force vector
\param    kapomeint     double	      (i)    kapome at integr. point
\param   *velint        double	      (i)    vel. at integr. point
\param   *velint_dc     double	      (i)    vel. at integr. point for D.C.
\param    eddyint       double	      (i)    eddy-visc. at integr. point
\param  **derxy         double	      (i)    global derivatives
\param   *kapomederxy2  double	      (i)    kapome 2nd derivatives
\param  **vderxy        double	      (i)    global vel. deriv.
\param   *kapomederxy   double	      (i)    global kapome deriv.
\param    visc	    double	      (i)    fluid viscosity
\param    fac	      double	      (i)    weighting factor
\param    factor	      double	      (i)    factor
\param    factor1	      double	      (i)    factor
\param    factor2	      double	      (i)    factor
\param    sig	      double	      (i)    factor
\param    production    double	      (i)    factor
\param    iel	      int	            (i)    num. of nodes in ele
\return void                                                                       

------------------------------------------------------------------------*/
void f2_calstabtfkapome(
                   FLUID_DYN_CALC  *dynvar,
                   ELEMENT         *ele,      
	            double          *eforce,  
	 	      double           kapomeint,  
       	      double          *velint,  
       	      double          *velint_dc,  
		      double           eddyint, 
                  double         **derxy,   
		      double          *kapomederxy2,   
                  double         **vderxy,  
		      double          *kapomederxy,
                  double           visc,     
                  double           fac,     
                  double           factor,
                  double           factor1,
                  double           factor2,
                  double           sig,
                  double           production,  
                  int              iel      
                  )
{
int    j,irow,isd,inode;
double aux,aux_dc;
double taumu,taumu_dc;
double facsr,facsr_dc;

#ifdef DEBUG 
dstrc_enter("f2_calstabtfkapome");
#endif

/*--------------------------------------------------- set some factors */
taumu    = dynvar->tau_tu;
taumu_dc = dynvar->tau_tu_dc;
facsr    = fac * dynvar->thsr * taumu;
facsr_dc = fac * dynvar->thsr * taumu_dc;

/*----------------------------------------------------------------------*
   Calculate forces of time force vector:
                     /
                (+) | tau_tu * u * kapome * grad(psi)     d_omega
                   /  
*----------------------------------------------------------------------*/
   irow=0;
   for (inode=0;inode<iel;inode++)
   {
     aux = kapomeint;
     
      for (isd=0;isd<2;isd++)
      {
	 eforce[irow] += aux*fac*taumu   *derxy[isd][inode]*velint[isd];
	 eforce[irow] += aux*fac*taumu_dc*derxy[isd][inode]*velint_dc[isd]; 
      } /* end loop over isd */
     irow++;
   } /* end loop over inode */

/*----------------------------------------------------------------------*
   Calculate forces of time force vector:
                     /
   (-) (1-THETA)*dt |  tau_tu * u * grad(kapome) * u * grad(psi) d_omega
                   /          
*----------------------------------------------------------------------*/
   irow=0;
   for (inode=0;inode<iel;inode++)
   {
       aux    =(velint[0]*velint[0]*kapomederxy[0]+
                velint[0]*velint[1]*kapomederxy[1])*derxy[0][inode]+
               (velint[1]*velint[0]*kapomederxy[0]+
                velint[1]*velint[1]*kapomederxy[1])*derxy[1][inode]; 

       aux_dc =(velint_dc[0]*velint_dc[0]*kapomederxy[0]+
                velint_dc[0]*velint_dc[1]*kapomederxy[1])*derxy[0][inode]+
               (velint_dc[1]*velint_dc[0]*kapomederxy[0]+
                velint_dc[1]*velint_dc[1]*kapomederxy[1])*derxy[1][inode]; 

	 eforce[irow] -= aux   *facsr;
	 eforce[irow] -= aux_dc*facsr_dc; 
    irow++;
   } /* end loop over inode */
 
/*----------------------------------------------------------------------*
   Calculate forces of time force vector:
                     /
   (+) (1-THETA)*dt |  tau_tu * div((nue+nue_t*sig)*grad(kapome)) * u * grad(psi)  d_omega=
                   /

                     /
   (+) (1-THETA)*dt |  tau_tu *(nue+nue_t*sig) * div grad(kapome)] * u * grad(psi)  d_omega 
                   /   
                  
*----------------------------------------------------------------------*/
  irow = 0;
   for (inode=0;inode<iel;inode++)
   {
    aux = (visc+eddyint*sig)*(kapomederxy2[0]+kapomederxy2[1]);

    for (isd=0;isd<2;isd++)
    {
        eforce[irow] += aux*facsr   *derxy[isd][inode]*velint[isd];			 
        eforce[irow] += aux*facsr_dc*derxy[isd][inode]*velint_dc[isd];			 
    } /* end loop over irn */
    irow ++;
   } /* end loop over inode*/

/*----------------------------------------------------------------------*
   Calculate forces of time force vector:
                     /
   (-) (1-THETA)*dt |  tau_tu * factor * kapome^2 * grad(psi) * u  d_omega
                   /
*----------------------------------------------------------------------*/ 
  irow=0;
   for (inode=0;inode<iel;inode++)
   {
    aux = factor*pow(kapomeint,2);
   
      for (isd=0;isd<2;isd++)
      {
         eforce[irow] -= aux*facsr   *velint[isd]   *derxy[isd][inode];
         eforce[irow] -= aux*facsr_dc*velint_dc[isd]*derxy[isd][inode]; 
      } /* end loop over isd */
   irow++;
   } /* end loop ove inode */

/*----------------------------------------------------------------------*
   Calculate forces of time force vector:
                     /
   (+) (1-THETA)*dt |  tau_tu * 0.5 * nue_t * factor1 * (grad(u)+[grad(u)^T])^2 * grad(psi) * u   d_omega
                   /   
 *----------------------------------------------------------------------*/
   irow = 0;
   for (inode=0;inode<iel;inode++)
   {
    aux = eddyint*factor1*production;

      for (isd=0;isd<2;isd++)
      {
        eforce[irow] += aux*facsr   *velint[isd]   *derxy[isd][inode];
        eforce[irow] += aux*facsr_dc*velint_dc[isd]*derxy[isd][inode];  
      } /* end loop over isd */
  irow ++;
} /* end loop over inode */

/*----------------------------------------------------------------------*
   Calculate forces of time force vector:
                     /
   (+) (1-THETA)*dt |  tau_tu * factor2 * kapome^2 * grad(psi) * u  d_omega
                   /
*----------------------------------------------------------------------*/ 
  irow=0;
   for (inode=0;inode<iel;inode++)
   {
    aux = factor2*pow(kapomeint,2);
    
      for (isd=0;isd<2;isd++)
      {
         eforce[irow] += aux*facsr   *velint[isd]   *derxy[isd][inode];
         eforce[irow] += aux*facsr_dc*velint_dc[isd]*derxy[isd][inode]; 
      } /* end loop over isd */
   irow++;
   } /* end loop ove inode */


/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif

return;
} /* end of f2_calstabtfkapome */




#endif

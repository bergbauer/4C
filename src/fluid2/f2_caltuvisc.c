/*!----------------------------------------------------------------------
\file
\brief evaluate turbulent eddy vicosity

------------------------------------------------------------------------*/
#ifdef D_FLUID2 
#include "../headers/standardtypes.h"
#include "fluid2_prototypes.h"
#include "fluid2.h"
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | vector of material laws                                              |
 | defined in global_control.c
 *----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;

/*!---------------------------------------------------------------------                                         

<pre>                                                         he    11/02

In this routine the algebraic turbulent eddy viscosity is calculated:   

</pre>
\param  *ele	   ELEMENT	   (i)    actual element
\param **vderxy      double	   (i)    global vel. derivatives
----------------------------------------------------------------------*/

double f2_calvisc(
	           ELEMENT    *ele,
                 double     **vderxy
                )
{
double           factor;     /* Faktorisierung der Wurzel */   
double           radiant;   
double           root; 
double           visc;       /* eddy-viscosity */ 
double           density;    /* density */     
int              actmat;     /* material number of the element */

/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_enter("f2_calvisc");
#endif
/*----------------------------------------------------------------------*/
actmat     = ele->mat-1;
density    = mat[actmat].m.fluid->density;

/*----------------------------------------------------------------------*/
factor  = pow(0.15*ele->e.f2->hk[0],2); 

radiant = 2.0 * (pow(vderxy[0][0],2) + pow(vderxy[1][1],2) + 
                 0.5*pow(vderxy[0][1],2) + 0.5*pow(vderxy[1][0],2) +
                 vderxy[1][0]*vderxy[0][1]);
 
root  = sqrt(radiant);

/*----------------------------------- Wirbelviskositaet ----------------*/
visc = 1.0/density*factor*root;   

/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_exit();
#endif
/*----------------------------------------------------------------------*/

return ((double)(visc));
} /*end of f2_calvisc */
 
#endif

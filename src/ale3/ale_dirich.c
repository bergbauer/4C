/*!----------------------------------------------------------------------
\file
\brief contains the routine 'ale_setdirich' and 'ale_caldirich'

*----------------------------------------------------------------------*/
#ifdef D_ALE
#include "../headers/standardtypes.h"
#include "../headers/solution.h"
#include "ale3.h"
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | vector of material laws                                              |
 | defined in global_control.c
 *----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;
/*----------------------------------------------------------------------*
 |                                                       m.gee 02/02    |
 | number of load curves numcurve                                       |
 | vector of structures of curves                                       |
 | defined in input_curves.c                                            |
 | INT                   numcurve;                                      |
 | struct _CURVE      *curve;                                           |
 *----------------------------------------------------------------------*/
extern INT            numcurve;
extern struct _CURVE *curve;

/*! 
\addtogroup Ale 
*//*! @{ (documentation module open)*/

/*!----------------------------------------------------------------------
\brief sets dirichlet boundary conditions on at time t

<pre>                                                              mn 06/02 
This routine reads the initial value for the dirichlet condition from 
actgnode->dirich->dirich_val.a.dv[j], gets the appropriate factor from 
the timecurve and writes the value for the dirichlet conditions at the
time t to actnode->sol.a.da[0][j].

</pre>
\param *actfield  FIELD          (i)  my field
\param *sdyn      STRUCT_DYNAMIK (i)  structure containing time information

\warning For (dirich_val.a.dv == 90) the boundary conditions for a special
         example (rotating hole) are calculated.
\return void                                               
\sa calling: dyn_facfromcurve(); called by: dyn_ale()

*----------------------------------------------------------------------*/
void ale_setdirich(FIELD  *actfield, STRUCT_DYNAMIC *sdyn)
{
GNODE                *actgnode;
NODE                 *actnode;
INT                   i,j;
INT                   numnp_total;
INT                   numele_total;
INT                   actcurve;
DOUBLE                timefac[ALENUMTIMECURVE];
DOUBLE                T;
DOUBLE                acttimefac;
DOUBLE                initval;

DOUBLE                cx,cy,win,wino,dd;

#ifdef DEBUG 
dstrc_enter("ale_setdirich");
#endif 

numnp_total  = actfield->dis[0].numnp;
numele_total = actfield->dis[0].numele;
T            = sdyn->time;

/*------------------------------------------ get values from time curve */
for (actcurve=0;actcurve<numcurve;actcurve++)
{
  dyn_facfromcurve(actcurve,T,&timefac[actcurve]) ;
}

/*------------------------------------------------- loop over all nodes */
for (i=0;i<numnp_total;i++)
{
   actnode  = &(actfield->dis[0].node[i]); 
   actgnode = actnode->gnode;      
   if (actgnode->dirich==NULL)
         continue;
   for (j=0;j<actnode->numdf;j++)
   {
      if (actgnode->dirich->dirich_onoff.a.iv[j]==0)
         continue;
      actcurve = actgnode->dirich->curve.a.iv[j]-1;
      if (actcurve<0)
         acttimefac = 1.0;
      else
         acttimefac = timefac[actcurve];
      initval  = actgnode->dirich->dirich_val.a.dv[j];               
/*=====================================================================*
 |    example: rotating hole (dirich_val.a.dv == 90)                   |
 |    sonst: Normalfall:                                               |
 |     actnode->sol.a.da[0][j] = initval*acttimefac;                   |
 *=====================================================================*/
      if (initval != 90)
        actnode->sol.a.da[0][j] = initval*acttimefac;
      else
      {
	cx = actnode->x[0]; 
	cy = actnode->x[1];
	win = (initval * acttimefac * 3.14159265359)/180.0;
	wino= atan(cy/cx);
	dd = sqrt(cx*cx+cy*cy);
	if(cx < 0.0) wino += 3.14159265359;
        if (j==0)
	  actnode->sol.a.da[0][j] = dd * cos(win+wino) - cx;
        else
	  actnode->sol.a.da[0][j] = dd * sin(win+wino) - cy;
      }
/*=====================================================================*/
   }
}

/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of ale_settdirich*/





/*!----------------------------------------------------------------------
\brief calculates the element dirichlet load vector  

<pre>                                                              mn 06/02 
This routine calculates the element dirichlet load vector, reading the
values of the dirichlet conditions from actnode->sol.a.da[0][j]

</pre>
\param *actele        ELEMENT (i)  my element
\param *fullvec        DOUBLE  (o)  the dirichlet load vector as full vector
\param dim            INT     (i)  dimension
\param *estif_global  ARRAY   (i)  the element stiffness matrix

\return void                                               
\sa calling: ---; called by: ale_rhs()

*----------------------------------------------------------------------*/
void ale_caldirich(
                     ELEMENT   *actele, 
		     DOUBLE    *fullvec,
		     INT        dim,
                     ARRAY     *estif_global
		    )     
{

INT                   i,j;
INT                   dof;
INT                   numdf;
INT                   nd=0;
DOUBLE              **estif;
DOUBLE                dirich[MAXDOFPERELE];
INT                   dirich_onoff[MAXDOFPERELE];
DOUBLE                dforces[MAXDOFPERELE];
GNODE                *actgnode;
NODE                 *actnode;
INT                   lm[MAXDOFPERELE];

#ifdef DEBUG 
dstrc_enter("ale_caldirich");
#endif  
/*----------------------------------------------------------------------*/
estif  = estif_global->a.da;
/*---------------------------------- set number of dofs on this element */
for (i=0; i<actele->numnp; i++) nd += actele->node[i]->numdf;

/*---------------------------- init the vectors dirich and dirich_onoff */
for (i=0; i<nd; i++)
{
   dirich[i] = 0.0;
   dirich_onoff[i] = 0;
   dforces[i] = 0.0;
}
/*-------------------------------- fill vectors dirich and dirich_onoff */
/*                                 dirichlet values at (n) were already *
/*                                     written to the nodes (sol[0][j]) */
for (i=0; i<actele->numnp; i++)
{
   numdf    = actele->node[i]->numdf;
   actnode  = actele->node[i];   
   actgnode = actnode->gnode;
   for (j=0; j<numdf; j++)
   {
      lm[i*numdf+j] = actele->node[i]->dof[j];
      if (actgnode->dirich==NULL) continue;
      dirich_onoff[i*numdf+j] = actgnode->dirich->dirich_onoff.a.iv[j];
      dirich[i*numdf+j] = actnode->sol.a.da[0][j];
   }
}
/*----------------------------------------- loop rows of element matrix */
for (i=0; i<nd; i++)
{
   /*------------------------------------- do nothing for supported row */
   if (dirich_onoff[i]!=0) continue;
   /*---------------------------------- loop columns of unsupported row */
   for (j=0; j<nd; j++)
   {
      /*---------------------------- do nothing for unsupported columns */
      if (dirich_onoff[j]==0) continue;
      dforces[i] -= estif[i][j] * dirich[j];
   }/* loop j over columns */
}/* loop i over rows */
/*-------- now assemble the vector dforces to the global vector fullvec */
for (i=0; i<nd; i++)
{
   if (lm[i] >= dim) continue;
   fullvec[lm[i]] += dforces[i];
}
/*----------------------------------------------------------------------*/
end:
/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of ale_caldirich*/ 
#endif
/*! @} (documentation module close)*/

#include "../headers/standardtypes.h"
#include "wall1.h"
#include "wall1_prototypes.h"
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | vector of material laws                                              |
 | defined in global_control.c
 *----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;
/*----------------------------------------------------------------------*
 | main wall1  control routine                               al 6/01    |
 *----------------------------------------------------------------------*/
void wall1(PARTITION   *actpart,
           INTRA       *actintra,
           ELEMENT     *ele,
           ARRAY       *estif_global,
           ARRAY       *emass_global,
           ARRAY       *intforce_global,
           int          kstep,
           CALC_ACTION *action)
{
int  i;
W1_DATA      actdata;
MATERIAL    *actmat;

int          imyrank;
double      *intforce;

#ifdef DEBUG 
dstrc_enter("wall1");
#endif
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
if (intforce_global)
intforce = intforce_global->a.dv;
/*------------------------------------------------- switch to do option */
switch (*action)
{
/*------------------------------------------- init the element routines */
case calc_struct_init:
   w1init(actpart, mat);
   w1static_ke(NULL,NULL,NULL,NULL,NULL,1);
   w1_cal_stress(NULL,NULL,NULL,NULL,NULL,1);
break;/*----------------------------------------------------------------*/
/*----------------------------------- calculate linear stiffness matrix */
case calc_struct_linstiff:
   actmat = &(mat[ele->mat-1]);
   w1static_ke(ele,&actdata,actmat,estif_global,NULL,0);
break;/*----------------------------------------------------------------*/
/*---------------------------------calculate nonlinear stiffness matrix */
case calc_struct_nlnstiff:
   actmat = &(mat[ele->mat-1]);
   w1static_ke(ele,&actdata,actmat,estif_global,intforce,0);
break;/*----------------------------------------------------------------*/
/*-------------------------- calculate linear stiffness and mass matrix */
case calc_struct_linstiffmass:
break;/*----------------------------------------------------------------*/
/*----------------------- calculate nonlinear stiffness and mass matrix */
case calc_struct_nlnstiffmass:
break;/*----------------------------------------------------------------*/
/*-------------------------------- calculate stresses in a certain step */
case calc_struct_stress:
   actmat = &(mat[ele->mat-1]);
   w1_cal_stress(ele,&actdata,actmat,estif_global,intforce,0);
break;/*----------------------------------------------------------------*/
/*------------------------------ calculate load vector of element loads */
case calc_struct_eleload:
break;/*----------------------------------------------------------------*/
/*--------------------------------------- update after incremental step */
case calc_struct_update_istep:
   actmat = &(mat[ele->mat-1]);
   w1static_ke(ele,&actdata,actmat,estif_global,intforce,2);
break;/*----------------------------------------------------------------*/
default:
   dserror("action unknown");
break;
}
/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
return; 
} /* end of wall1 */

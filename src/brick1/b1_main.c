#include "../headers/standardtypes.h"
#include "brick1.h"
#include "brick1_calc.h"
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | vector of material laws                                              |
 | defined in global_control.c
 *----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;
/*----------------------------------------------------------------------*
 | main brick1  control routine                              al 6/01    |
 |                                                                      |
 *----------------------------------------------------------------------*/
void brick1(      PARTITION *actpart,
                  INTRA     *actintra,
                  ELEMENT   *ele,
                  ARRAY     *estif_global,
                  ARRAY     *emass_global,
            const int        option)
{
int  i;
B1_DATA      actdata;
MATERIAL    *actmat;

#ifdef DEBUG 
dstrc_enter("brick1");
#endif
/*----------------------------------------------------------------------*/
/*------------------------------------------------- switch to do option */
switch (option)
{
case 0:/*------------------------------------ init the element routines */
   b1static_ke(NULL,NULL,NULL,NULL,1);
break;/*----------------------------------------------------------------*/
case 1:/*---------------------------- calculate linear stiffness matrix */
   actmat = &(mat[ele->mat-1]);
   b1static_ke(ele,&actdata,actmat,estif_global,0);
break;/*----------------------------------------------------------------*/
case 2:/*--------------------------calculate nonlinear stiffness matrix */
break;/*----------------------------------------------------------------*/
case 3:/*------------------- calculate linear stiffness and mass matrix */
break;/*----------------------------------------------------------------*/
case 4:/*---------------- calculate nonlinear stiffness and mass matrix */
break;/*----------------------------------------------------------------*/
case 5:/*-------------------------- calculate vector of internal forces */
break;/*----------------------------------------------------------------*/
case 6:/*----------------------- calculate load vector of element loads */
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
} /* end of brick1 */

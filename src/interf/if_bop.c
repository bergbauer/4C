/*!-----------------------------------------------------------------------
\file
\brief contains the routine 'if_bop' which calculates the linear operator
matrix for a interface element at gaussian points
<pre>
Maintainer: Andrea Hund
            hund@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/hund/
            0711 - 685-6122
</pre>

*-----------------------------------------------------------------------*/
#ifndef CCADISCRET
#ifdef D_INTERF
#include "../headers/standardtypes.h"
#include "interf.h"
#include "interf_prototypes.h"

/*!
\addtogroup INTERF
*/
/*! @{ (documentation module open)*/

/*!----------------------------------------------------------------------
\brief calculate operator matrix at gaussian point

<pre>                                                             ah 05/03
This routine calcuates the operator matrix B at the given gaussian point
for an interface element.
</pre>

\param   typ           DIS_TYP (I)   quad4 or quad8
\param **bop           DOUBLE  (O)   B-Operator-Matrix
\param  *funct         DOUBLE  (I)   Ansatz-functions
\param   co            DOUBLE  (I)   cosinus of angle bet x-dir and orient. of IF-ele
\param   si            DOUBLE  (I)   sinus of angle bet x-dir and orient. of IF-ele
\param   flag          INT     (I)   flag for case differentiation

\warning There is nothing special to this routine
\return void

*----------------------------------------------------------------------*/
void if_bop(DIS_TYP    typ,
            DOUBLE   **bop,
            DOUBLE    *funct,
            DOUBLE     co,
            DOUBLE     si,
            INT        flag)
{
/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_enter("if_bop");
#endif

/*----------------------- compute operator B = func*L*transformation -- */
switch(typ)
{
case quad4:
     if(flag == 1)
     {
      bop[0][0] = -funct[0]*co;
      bop[1][0] =  funct[0]*si;
      bop[0][1] = -funct[0]*si;
      bop[1][1] = -funct[0]*co;
      bop[0][2] = -funct[1]*co;
      bop[1][2] =  funct[1]*si;
      bop[0][3] = -funct[1]*si;
      bop[1][3] = -funct[1]*co;
      bop[0][4] =  funct[1]*co;
      bop[1][4] = -funct[1]*si;
      bop[0][5] =  funct[1]*si;
      bop[1][5] =  funct[1]*co;
      bop[0][6] =  funct[0]*co;
      bop[1][6] = -funct[0]*si;
      bop[0][7] =  funct[0]*si;
      bop[1][7] =  funct[0]*co;
      }
      else if (flag == 2)
      {
      bop[0][0] =  funct[0]*co;
      bop[1][0] = -funct[0]*si;
      bop[0][1] =  funct[0]*si;
      bop[1][1] =  funct[0]*co;
      bop[0][2] = -funct[0]*co;
      bop[1][2] =  funct[0]*si;
      bop[0][3] = -funct[0]*si;
      bop[1][3] = -funct[0]*co;
      bop[0][4] = -funct[1]*co;
      bop[1][4] =  funct[1]*si;
      bop[0][5] = -funct[1]*si;
      bop[1][5] = -funct[1]*co;
      bop[0][6] =  funct[1]*co;
      bop[1][6] = -funct[1]*si;
      bop[0][7] =  funct[1]*si;
      bop[1][7] =  funct[1]*co;
      }
break;
/*-----------------------------------------------------------------------*/
case quad8:

     if(flag == 1)
     {
      bop[0][0] = -funct[0]*co;
      bop[1][0] =  funct[0]*si;
      bop[0][1] = -funct[0]*si;
      bop[1][1] = -funct[0]*co;
      bop[0][2] = -funct[1]*co;
      bop[1][2] =  funct[1]*si;
      bop[0][3] = -funct[1]*si;
      bop[1][3] = -funct[1]*co;
      bop[0][4] =  funct[1]*co;
      bop[1][4] = -funct[1]*si;
      bop[0][5] =  funct[1]*si;
      bop[1][5] =  funct[1]*co;
      bop[0][6] =  funct[0]*co;
      bop[1][6] = -funct[0]*si;
      bop[0][7] =  funct[0]*si;
      bop[1][7] =  funct[0]*co;
      bop[0][8] = -funct[2]*co;
      bop[1][8] =  funct[2]*si;
      bop[0][9] = -funct[2]*si;
      bop[1][9] = -funct[2]*co;
      bop[0][10] = 0.0;
      bop[1][10] = 0.0;
      bop[0][11] = 0.0;
      bop[1][11] = 0.0;
      bop[0][12] = funct[2]*co;
      bop[1][12] =-funct[2]*si;
      bop[0][13] = funct[2]*si;
      bop[1][13] = funct[2]*co;
      bop[0][14] = 0.0;
      bop[1][14] = 0.0;
      bop[0][15] = 0.0;
      bop[1][15] = 0.0;
     }
     else if (flag == 2)
     {
      bop[0][0] =  funct[0]*co;
      bop[1][0] = -funct[0]*si;
      bop[0][1] =  funct[0]*si;
      bop[1][1] =  funct[0]*co;
      bop[0][2] = -funct[0]*co;
      bop[1][2] =  funct[0]*si;
      bop[0][3] = -funct[0]*si;
      bop[1][3] = -funct[0]*co;
      bop[0][4] = -funct[1]*co;
      bop[1][4] =  funct[1]*si;
      bop[0][5] = -funct[1]*si;
      bop[1][5] = -funct[1]*co;
      bop[0][6] =  funct[1]*co;
      bop[1][6] = -funct[1]*si;
      bop[0][7] =  funct[1]*si;
      bop[1][7] =  funct[1]*co;
      bop[0][8] =  0.0;
      bop[1][8] =  0.0;
      bop[0][9] =  0.0;
      bop[1][9] =  0.0;
      bop[0][10] =-funct[2]*co;
      bop[1][10] = funct[2]*si;
      bop[0][11] =-funct[2]*si;
      bop[1][11] =-funct[2]*co;
      bop[0][12] = 0.0;
      bop[1][12] = 0.0;
      bop[0][13] = 0.0;
      bop[1][13] = 0.0;
      bop[0][14] = funct[2]*co;
      bop[1][14] =-funct[2]*si;
      bop[0][15] = funct[2]*si;
      bop[1][15] = funct[2]*co;
     }
break;
default:
   dserror("discretisation unknown for Interface");
break;
}

/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_exit();
#endif
return;
} /* end of if_bop */
/*----------------------------------------------------------------------*/

/*! @} (documentation module close)*/

#endif /*D_INTERF*/
#endif

/*!----------------------------------------------------------------------
\file
\brief calculate submesh element size for fluid3

<pre>
Maintainer: Volker Gravemeier
            vgravem@stanford.edu


</pre>

------------------------------------------------------------------------*/
#ifdef FLUID3_ML
#include "../headers/standardtypes.h"
#include "../fluid3/fluid3_prototypes.h"
#include "fluid3ml_prototypes.h"
#include "../fluid3/fluid3.h"
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | pointer to allocate dynamic variables if needed                      |
 | dedfined in global_control.c                                         |
 | ALLDYNA               *alldyn;                                       |
 *----------------------------------------------------------------------*/
extern ALLDYNA      *alldyn;
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | general problem data                                                 |
 | global variable GENPROB genprob is defined in global_control.c       |
 *----------------------------------------------------------------------*/
extern struct _GENPROB     genprob;

static FLUID_DYNAMIC *fdyn;

/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | vector of material laws                                              |
 | defined in global_control.c
 *----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;

/*!---------------------------------------------------------------------
\brief routine to calculate submesh element size for fluid3

<pre>                                                       gravem 07/03

In this routine, the characteristic submesh element length is calculated
and the routine for the calculation of the stabilization parameter or
the subgrid viscosity (depending on the respective flag) is called.

</pre>
\param  *ele       ELEMENT	     (i)   actual element
\param  *data      FLUID_DATA	     (i)
\param  *mlvar     FLUID_DYN_ML      (i)
\param  *funct     DOUBLE 	     (-)   l-s shape functions
\param **deriv     DOUBLE 	     (-)   deriv. of l-s shape funcs
\param **deriv2    DOUBLE 	     (-)   2nd deriv. of l-s sh. funcs
\param  *smfunct   DOUBLE 	     (-)   submesh shape functions
\param **smderiv   DOUBLE 	     (-)   deriv. of submesh shape fun.
\param **smderiv2  DOUBLE 	     (-)   2nd deriv. of sm sh. fun.
\param **derxy     DOUBLE 	     (-)   global l-s sh. fun. deriv.
\param **xjm       DOUBLE 	     (-)   jacobian matrix
\param **evel      DOUBLE 	     (i)   l-s element velocities
\param  *velint    DOUBLE 	     (-)   l-s vel. at integr. point
\param **vderxy    DOUBLE 	     (-)   l-s vel. deriv. at int. point
\param **smxyze    DOUBLE 	     (i)   submesh element coordinates
\param **smxyzep   DOUBLE 	     (i)   sm ele. coord. on parent dom.
\param **wa1       DOUBLE 	     (-)   working array
\return void

------------------------------------------------------------------------*/
void f3_smelesize(ELEMENT         *ele,
		  FLUID_DATA	  *data,
		  FLUID_DYN_ML    *mlvar,
	          DOUBLE	  *funct,
	          DOUBLE	 **deriv,
	          DOUBLE	 **deriv2,
	          DOUBLE	  *smfunct,
	          DOUBLE	 **smderiv,
	          DOUBLE	 **smderiv2,
	          DOUBLE	 **derxy,
		  DOUBLE	 **xjm,
		  DOUBLE	 **evel,
		  DOUBLE	  *velint,
	          DOUBLE	 **vderxy,
	          DOUBLE	 **smxyze,
	          DOUBLE	 **smxyzep,
		  DOUBLE	 **wa1)
{

INT     inod;           /* simply a counter	        		*/
INT     nsmtyp;         /* l-s and sm element type (TRI or QUAD)        */
INT     actmat;         /* number of actual material		        */
INT     iel,smiel;      /* l-s and sm number of nodes of actual element */
DOUBLE  visc;           /* fluid viscosity                              */
DOUBLE  vol;            /* element volume                               */
DOUBLE  det;            /* determinant of jacobian                      */
DOUBLE  val;            /* temporary calculation value                  */
DOUBLE  velno;          /* velocity norm                                */
DOUBLE  strle;          /* streamlength                                 */
DOUBLE  e1,e2,e3;       /* natural coordinates of inegration point      */
DOUBLE  fac,facr;       /* factors                                      */
DOUBLE  facs,fact;      /* factors                                      */
DOUBLE  velino[3];      /* normed velocity vector at integration point  */
DOUBLE  coor[3];        /* coordinates                                  */
DIS_TYP typ,smtyp;

#ifdef DEBUG
dstrc_enter("f3_smelesize");
#endif

/*---------------------------------------------------------- initialize */
nsmtyp = mlvar->submesh.ntyp;
typ    = ele->distyp;
smtyp  = mlvar->submesh.typ;
iel    = ele->numnp;
smiel  = mlvar->submesh.numen;
vol   = ZERO;
strle = ZERO;

actmat=ele->mat-1;
visc = mat[actmat].m.fluid->viscosity;

/*------ get values of integration parameters, shape functions and their
         derivatives for submesh element -------------------------------*/
switch(nsmtyp)
{
case 1:    /* --> hex - element */
   e1	= data->qxg[0][0];
   facr = data->qwgt[0][0];
   e2	= data->qxg[0][0];
   facs = data->qwgt[0][0];
   e3	= data->qxg[0][0];
   fact = data->qwgt[0][0];
   f3_hex(smfunct,smderiv,smderiv2,e1,e2,e3,smtyp,2);
break;
case 2:       /* --> tet - element */
   e1	= data->txgr[0][0];
   facr = data->twgt[0][0];
   e2	= data->txgs[0][0];
   facs = ONE;
   e3	= data->txgs[0][0];
   fact = ONE;
   f3_tet(smfunct,smderiv,smderiv2,e1,e2,e3,smtyp,2);
break;
default:
   dserror("nsmtyp unknown!\n");
} /*end switch(nsmtyp) */

if (mlvar->smesize<4)     /* compute volume */
{
/* -------------------------------------------- compute jacobian matrix */
  f3_mljaco3(smxyze,smfunct,smderiv,xjm,&det,smiel,ele);
  fac=facr*facs*fact*det;
  vol += fac;
}/* endif (mlvar->smesize<4) */

/* compute diagonal based diameter */
if (mlvar->smesize==4) dserror("no diagonal-based diameter in 3D yet!\n");

if (mlvar->smesize==5)    /* compute streamlength based on l-s velocity */
{
  f3_mlgcoor2(smfunct,smxyzep,smiel,coor);
  switch(typ)
  {
  case hex8: case hex20: case hex27:    /* --> quad - element */
    f3_hex(funct,deriv,deriv2,coor[0],coor[1],coor[2],typ,2);
  break;
  case tet4: case tet10:	/* --> tri - element */
    f3_tet(funct,deriv,deriv2,coor[0],coor[1],coor[2],typ,2);
  break;
  default:
    dserror("typ unknown!\n");
  } /*end switch(typ) */
  f3_veci(velint,funct,evel,iel);
  f3_mljaco(funct,deriv,xjm,&det,ele,iel);
  f3_gder(derxy,deriv,xjm,wa1,det,iel);
  val = ZERO;
  velno=sqrt( velint[0]*velint[0] \
  	    + velint[1]*velint[1] \
    	    + velint[2]*velint[2]);
  if(velno>=EPS6)
  {
     velino[0] = velint[0]/velno;
     velino[1] = velint[1]/velno;
     velino[2] = velint[2]/velno;
  }
  else
  {
     velino[0] = ONE;
     velino[1] = ZERO;
     velino[2] = ZERO;
  }
  for (inod=0;inod<iel;inod++) /* loop element nodes */
  {
     val += FABS(velino[0]*derxy[0][inod] \
    		+velino[1]*derxy[1][inod] \
    		+velino[2]*derxy[2][inod]);
  } /* end of loop over element nodes */
  strle=TWO/val;
} /* endif (mlvar->smesize==5) */

/*----------------------------------- set characteristic element length */
if      (mlvar->smesize==1) ele->e.f3->smcml = pow(vol,(ONE/THREE));
else if (mlvar->smesize==2) ele->e.f3->smcml = pow((SIX*vol/PI),(ONE/THREE));
else if (mlvar->smesize==3) ele->e.f3->smcml = pow((SIX*vol/PI),(ONE/THREE))/sqrt(THREE);
else if (mlvar->smesize==5) ele->e.f3->smcml = strle;

if (mlvar->smesize<5)     /* compute l-s velocity */
{
  f3_mlgcoor2(smfunct,smxyzep,smiel,coor);
  switch(typ)
  {
  case hex8: case hex20: case hex27:    /* --> hex - element */
    f3_hex(funct,deriv,deriv2,coor[0],coor[1],coor[2],typ,2);
  break;
  case tet4: case tet10:	/* --> tet - element */
    f3_tet(funct,deriv,deriv2,coor[0],coor[1],coor[2],typ,2);
  break;
  default:
    dserror("typ unknown!\n");
  } /*end switch(typ) */
  f3_veci(velint,funct,evel,iel);
}

/*----------------------------------- calculate stabilization parameter */
if (mlvar->smstabi>0) f3_smstabpar(ele,mlvar,velint,visc,smiel,typ);

/*--------------------------------------------------- subgrid viscosity */
if (mlvar->smsgvi==1 || mlvar->smsgvi==2)
{
  f3_mljaco(funct,deriv,xjm,&det,ele,iel);
/*-------------------------------------------- compute global derivates */
  f3_gder(derxy,deriv,xjm,wa1,det,iel);
/*----------------------- get velocity derivatives at integration point */
  f3_vder(vderxy,derxy,evel,iel);
/*----------------------------------------- calculate subgrid viscosity */
  f3_smsgvisc(ele,mlvar,velint,vderxy,visc,smiel,typ);
}

/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_exit();
#endif

return;
} /* end of f3_smelesize */

/*!---------------------------------------------------------------------
\brief routine to calculate large-scale element size and stab. parameter

<pre>                                                       gravem 05/05

   ele->e.f3->stabi.gls->iadvec: adevction stab.
      0 = no
      1 = yes
   ele->e.f3->stabi.gls->ipres: pressure stab.
      0 = no
      1 = yes
   ele->e.f3->stabi.gls->ivisc: diffusion stab.
      0 = no
      1 = GLS-
      2 = GLS+
   ele->e.f3->stabi.gls->icont: continuity stab.
      0 = no
      1 = yes
   ele->e.f3->stabi.gls->istapa: version of stab. parameter
      35 = diss wall instationary
      36 = diss wall stationanary
   ele->e.f3->stabi.gls->norm_P: p-norm
      p = 1<=p<=oo
      0 = max.-norm (p=oo)
   ele->e.f3->stabi.gls->mk: higher order elements control flag
      0 = mk fixed (--> (bi)linear: 1/3, biquadr.: 1/12)
      1 = min(1/3,2*C)
     -1 = mk=1/3  (--> element order via approx. nodal-distance)
   ele->e.f3->stabi.gls->ihele[]:
      x/y/z = length-def. for velocity/pressure/continuity stab
      0 = don't compute
      1 = sqrt(area)
      2 = area equivalent diameter
      3 = diameter/sqrt(2)
      4 = sqrt(2)*area/diagonal (rectangle) 4*area/s (triangle)
      5 = streamlength (element length in flow direction
   ele->e.f3->stabi.gls->ninths: number of integration points for streamlength
      1 = at center of element
      2 = at every INT pt used for element.-stab.-matrices
   ele->e.f3->stabi.gls->istapc: flag for stabilisation parameter calculation
      1 = at center of element
      2 = at every integration point
   ele->e.f3->stabi.gls->clamb \
   ele->e.f3->c1               |_>> stabilisation constants (input)
   ele->e.f3->c2               |
   ele->e.f3->c3              /
   ele->e.f3->stabi.gls->istrle: has streamlength to be computed
   ele->e.f3->stabi.gls->iarea: calculation of area length
   ele->e.f3->stabi.gls->iduring: calculation during INT.-pt.loop
   ele->e.f3->stabi.gls->itau[0]: flag for tau_mu calc. (-1: before, 1:during)
   ele->e.f3->stabi.gls->itau[1]: flag for tau_mp calc. (-1: before, 1:during)
   ele->e.f3->stabi.gls->itau[2]: flag for tau_c calc. (-1: before, 1:during)
   ele->e.f3->hk[i]: "element sizes" (vel / pre / cont)
   ele->e.f3->stabi.gls->idiaxy: has diagonals to be computed
   fdyn->tau[0]: stability parameter momentum / velocity (tau_mu)
   fdyn->tau[1]: stability parameter momentum / pressure (tau_mp)
   fdyn->tau[2]: stability parameter continuity (tau_c)
</pre>
\param  *ele     ELEMENT         (i)   actual element
\param  *data    FLUID_DATA      (i)   actual element
\param  *funct   DOUBLE          (-)   shape functions
\param **deriv   DOUBLE          (-)   deriv. of shape funcs
\param **deriv2  DOUBLE          (-)   2nd deriv. of sh. funcs
\param **derxy   DOUBLE          (-)   global derivatives
\param **xjm     DOUBLE          (-)   jacobian matrix
\param **evel    DOUBLE          (i)   element velocities
\param  *velint  DOUBLE          (-)   velocity at integr. point
\param **vderxy  DOUBLE          (-)   velocity der. at integr. point
\param **wa1     DOUBLE          (-)   working array
\return void

------------------------------------------------------------------------*/
void f3_lselesize(ELEMENT         *ele,
		  FLUID_DATA      *data,
	          DOUBLE          *funct,
	          DOUBLE         **deriv,
	          DOUBLE         **deriv2,
                  DOUBLE         **derxy,
		  DOUBLE         **xjm,
		  DOUBLE         **evel,
		  DOUBLE          *velint,
	          DOUBLE         **vderxy,
		  DOUBLE         **wa1)
{
INT ieval = 0;       /* evaluation flag			                */
INT ilen,inod;       /* simply two counters	        		*/
INT istrnint;        /* evaluation flag		     	                */
INT ishvol;          /* evaluation flag		        	        */
INT actmat;          /* number of actual material		        */
INT iel;             /* number of nodes of actual element               */
DOUBLE visc;         /* fluid viscosity                                 */
DOUBLE det;          /* determinant of jacobian                         */
DOUBLE vol;          /* element volume                                  */
DOUBLE val;          /* temporary calculation value                     */
DOUBLE velno;        /* velocity norm                                   */
DOUBLE strle;        /* streamlength                                    */
DOUBLE e1,e2,e3;     /* natural coordinates of inegration point         */
DOUBLE fac,facr;     /* factors                                         */
DOUBLE facs,fact;    /* factors                                         */
DOUBLE velino[3];    /* normed velocity vector at integration point     */
DIS_TYP typ;
STAB_PAR_GLS *gls;	/* pointer to GLS stabilisation parameters	*/

#ifdef DEBUG
dstrc_enter("f3_lselesize");
#endif

/*---------------------------------------------------------- initialise */
fdyn = alldyn[genprob.numff].fdyn;

iel    = ele->numnp;
typ    = ele->distyp;
gls    = ele->e.f3->stabi.gls;

if (ele->e.f3->stab_type != stab_gls)
   dserror("routine with no or wrong stabilisation called");

istrnint = gls->istrle * gls->ninths;
ishvol  = fdyn->ishape * gls->iareavol;

/*----------------------------------------------------------------------*
 | calculations at element center: area & streamlength                  |
 | NOTE:                                                                |
 |    volume is always calculated using only 1 integrationpoint         |
 |    --> it may be possible to save some operations here by replacing  |
 |         e1,e2,e3, facr,facs,fact with their constant values in the   |
 |         calls of f3_hex / f3_tet!!!!!!                               |
 *----------------------------------------------------------------------*/

if (ishvol==1)
{
   vol   = ZERO;
   strle = ZERO;
/*------ get values of integration parameters, shape functions and their
         derivatives ---------------------------------------------------*/
   switch(typ)
   {
   case hex8: case hex20: case hex27:   /* --> hex - element */
      e1   = data->qxg[0][0];
      facr = data->qwgt[0][0];
      e2   = data->qxg[0][0];
      facs = data->qwgt[0][0];
      e3   = data->qxg[0][0];
      fact = data->qwgt[0][0];
      f3_hex(funct,deriv,deriv2,e1,e2,e3,typ,2);
   break;
   case tet4: case tet10:  /* --> tet - element */
      e1   = data->txgr[0][0];
      facr = data->twgt[0][0];
      e2   = data->txgs[0][0];
      facs = ONE;
      e3   = data->txgs[0][0];
      fact = ONE;
      f3_tet(funct,deriv,deriv2,e1,e2,e3,typ,2);
   break;
   default:
      dserror("typ unknown!");
   } /*end switch(typ) */
   ieval++;
/* ------------------------------------------- compute jacobian matrix */
   f3_mljaco(funct,deriv,xjm,&det,ele,iel);
   fac=facr*facs*fact*det;
   vol += fac;
   if (istrnint==1)    /* compute streamlength */
   {
      f3_veci(velint,funct,evel,iel);
      f3_gder(derxy,deriv,xjm,wa1,det,iel);
      ieval++;
      val = ZERO;
      velno=sqrt( velint[0]*velint[0] \
                + velint[1]*velint[1] \
		+ velint[2]*velint[2]);
      if(velno>=EPS6)
      {
	 velino[0] = velint[0]/velno;
         velino[1] = velint[1]/velno;
         velino[2] = velint[2]/velno;
      }
      else
      {
         velino[0] = ONE;
	 velino[1] = ZERO;
	 velino[2] = ZERO;
      }
      for (inod=0;inod<iel;inod++) /* loop element nodes */
      {
         val += FABS(velino[0]*derxy[0][inod] \
	            +velino[1]*derxy[1][inod] \
		    +velino[2]*derxy[2][inod]);
      } /* end of loop over elements */
      strle=TWO/val;
   } /* endif (istrnint==1) */
/*--------------------------------------------------- set element sizes *
  ----loop over 3 different element sizes: vel/pre/cont  ---------------*/
   for(ilen=0;ilen<3;ilen++)
   {
      if (gls->ihele[ilen]==1)
         ele->e.f3->hk[ilen] = pow(vol,(ONE/THREE));
      else if (gls->ihele[ilen]==2)
         ele->e.f3->hk[ilen] = pow((SIX*vol/PI),(ONE/THREE));
      else if (gls->ihele[ilen]==3)
         ele->e.f3->hk[ilen] = pow((SIX*vol/PI),(ONE/THREE))/sqrt(THREE);
      else if (gls->ihele[ilen]==4)
         dserror("ihele[i] = 4: calculation of element size not possible!!!");
         else if (gls->ninths==1)
         ele->e.f3->hk[ilen] = strle;
   } /* end of loop over ilen */
} /* endif (ishvol==1) */
/*----------------------------------------------------------------------*
 | calculations at element center: only streamlength                    |
 | NOTE:                                                                |
 |    volume is always calculated using only 1 integrationpoint         |
 |    --> it may be possible to save some operations here by replacing  |
 |         e1,e2,e3, facr,facs,fact with their constant values in the   |
 |         calls of f3_hex / f3_tet!!!!!!                               |
 *----------------------------------------------------------------------*/
else if (istrnint==1 && ishvol !=1)
{
   vol   = ZERO;
   strle = ZERO;
/*------ get values of integration parameters, shape functions and their
         derivatives ---------------------------------------------------*/
   switch(typ)
   {
   case hex8: case hex20: case hex27:   /* --> hex - element */
      e1   = data->qxg[0][0];
      facr = data->qwgt[0][0];
      e2   = data->qxg[0][0];
      facs = data->qwgt[0][0];
      e3   = data->qxg[0][0];
      fact = data->qwgt[0][0];
      f3_hex(funct,deriv,deriv2,e1,e2,e3,typ,2);
   break;
   case tet4: case tet10:  /* --> tet - element */
      e1   = data->txgr[0][0];
      facr = data->twgt[0][0];
      e2   = data->txgs[0][0];
      facs = ONE;
      e3   = data->txgs[0][0];
      fact = ONE;
      f3_tet(funct,deriv,deriv2,e1,e2,e3,typ,2);
   break;
   default:
      dserror("typ unknown!");
   } /*end switch(typ) */
/* ------------------------------------------- compute jacobian matrix */
   f3_mljaco(funct,deriv,xjm,&det,ele,iel);
/* --------------------------------------------- compute stream length */
   f3_veci(velint,funct,evel,iel);
   f3_gder(derxy,deriv,xjm,wa1,det,iel);
   ieval++;
   val = ZERO;
   velno=sqrt( velint[0]*velint[0] \
   	     + velint[1]*velint[1] \
     	     + velint[2]*velint[2]);
   if(velno>=EPS6)
   {
      velino[0] = velint[0]/velno;
      velino[1] = velint[1]/velno;
      velino[2] = velint[2]/velno;
   }
   else
   {
      velino[0] = ONE;
      velino[1] = ZERO;
      velino[2] = ZERO;
   }
   for (inod=0;inod<iel;inod++) /* loop element nodes */
   {
      val += FABS(velino[0]*derxy[0][inod] \
     		 +velino[1]*derxy[1][inod] \
     		 +velino[2]*derxy[2][inod]);
   } /* end of loop over element nodes */
   strle=TWO/val;
/*--------------------------------------------------- set element sizes *
      loop over 3 different element sizes: vel/pre/cont  ---------------*/
   for (ilen=0;ilen<3;ilen++)
   {
      if (gls->ihele[ilen]==5)
         ele->e.f3->hk[ilen] = strle;
   } /* end of loop over ilen */
} /* endif (istrnint==1 && ishvol !=1) */

/*----------------------------------------------------------------------*
  calculate stabilisation parameter
 *----------------------------------------------------------------------*/
if(gls->istapc==1 || istrnint==1)
{
   switch(ieval) /* ival>2: vel at intpoint already available! */
   {
   case 0:
/*------ get only values of integration parameters and shape functions
        + derivatives for Smagorinsky subgrid viscosity --------------*/
      switch(typ)
      {
      case hex8: case hex20: case hex27:    /* --> quad - element */
         e1   = data->qxg[0][0];
         facr = data->qwgt[0][0];
         e2   = data->qxg[0][0];
         facs = data->qwgt[0][0];
         e3   = data->qxg[0][0];
         fact = data->qwgt[0][0];
         f3_hex(funct,deriv,deriv2,e1,e2,e3,typ,2);
      break;
      case tet4: case tet10:       /* --> tet - element */
         e1   = data->txgr[0][0];
         facr = data->twgt[0][0];
         e2   = data->txgs[0][0];
         facs = ONE;
         e3   = data->txgs[0][0];
         fact = ONE;
         f3_tet(funct,deriv,deriv2,e1,e2,e3,typ,2);
      break;
      default:
         dserror("typ unknown!");
      } /* end switch (typ) */
      f3_veci(velint,funct,evel,iel);
      if (fdyn->sgvisc>0) f3_mljaco(funct,deriv,xjm,&det,ele,iel);
   break;
   case 1:
      f3_veci(velint,funct,evel,iel);
   break;
   case 2:
   break;
   default:
      dserror("wrong value for ieval");
   } /* end switch (ieval) */
/*----------------------------------- calculate stabilisation parameter */
   actmat=ele->mat-1;
   visc = mat[actmat].m.fluid->viscosity;
   f3_lsstabpar(ele,velint,visc,iel,typ,-1);
/*--------------------------------------------------- subgrid viscosity */
   if (fdyn->sgvisc>0)
   {
/*------------------------------------------- compute global derivates */
     f3_gder(derxy,deriv,xjm,wa1,det,iel);
/*---------------------- get velocity derivatives at integration point */
     f3_vder(vderxy,derxy,evel,iel);
/*---------------------------------------- calculate subgrid viscosity */
     f3_lssgvisc(ele,velint,vderxy,visc,iel,typ);
   }
} /* endif (ele->e.f3->istapc==1 || istrnint==1) */

/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_exit();
#endif

return;
} /* end of f3_lselesize */


/*!---------------------------------------------------------------------
\brief routine to calculate large-scale element size and stab. parameter

<pre>                                                       gravem 05/05

In this routine, the element size and the stabilisation parameter
is calculated for one element during the integration loop

</pre>
\param  *ele     ELEMENT         (i)    actual element
\param  *velint  DOUBLE          (-)    velocity at integr. point
\param **vderxy  DOUBLE          (-)    velocity der. at integr. point
\param **derxy   DOUBLE          (-)    global derivatives
\param   visc    DOUBLE          (i)    fluid viscosity
\param   iel     INT             (i)    act. num. of ele nodes
\param   typ     DIS_TYP         (i)    element type
\return void
\sa f3_lselesize()

------------------------------------------------------------------------*/
void f3_lselesize2(ELEMENT         *ele,
                   DOUBLE	   *velint,
		   DOUBLE	  **vderxy,
                   DOUBLE	  **derxy,
		   DOUBLE	    visc,
		   INT 	            iel,
		   DIS_TYP 	    typ)
{
INT    ilen, inod; /* simply a counter                                  */
INT    istrnint;   /* evaluation flag                                   */
DOUBLE strle;      /* stream length                                     */
DOUBLE val;	   /* temporary calculation value                       */
DOUBLE velno;	   /* velocity norm                                     */
DOUBLE velino[3];  /* normed velocity vector at integration point       */
STAB_PAR_GLS *gls;	/* pointer to GLS stabilisation parameters	*/

#ifdef DEBUG
dstrc_enter("f3_lselesize2");
#endif

/*---------------------------------------------------------- initialise */
fdyn = alldyn[genprob.numff].fdyn;

gls    = ele->e.f3->stabi.gls;
istrnint = gls->istrle * gls->ninths;
val = ZERO;

if (istrnint==2)
{
/*------------------------------------------------ compute streamlength */
   velno=sqrt( velint[0]*velint[0] \
   	     + velint[1]*velint[1] \
     	     + velint[2]*velint[2]);
   if(velno>=EPS6)
   {
      velino[0] = velint[0]/velno;
      velino[1] = velint[1]/velno;
      velino[2] = velint[2]/velno;
   }
   else
   {
      velino[0] = ONE;
      velino[1] = ZERO;
      velino[2] = ZERO;
   }
   for (inod=0;inod<iel;inod++)
   {
      val += FABS(velino[0]*derxy[0][inod] \
     		 +velino[1]*derxy[1][inod] \
     		 +velino[2]*derxy[2][inod]);
   }
   strle=TWO/val;
/*--------------------------------------------------- set element sizes *
      loop over 3 different element sizes: vel/pre/cont  ---------------*/
   for (ilen=0;ilen<3;ilen++)
   {
      if (gls->ihele[ilen]==5)
         ele->e.f3->hk[ilen] = strle;
   } /* end of loop over ilen */
} /* endif (istrnint==2) */
/*----------------------------------- calculate stabilisation parameter */
f3_lsstabpar(ele,velint,visc,iel,typ,1);

/*----------------------------------------- calculate subgrid viscosity */
if (fdyn->sgvisc>0) f3_lssgvisc(ele,velint,vderxy,visc,iel,typ);

/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_exit();
#endif

return;
} /* end of f3_lselesize2 */

#endif

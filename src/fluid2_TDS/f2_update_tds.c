/*!----------------------------------------------------------------------
\file
\brief evaluate 2D fluid coefficient matrix

<pre>
Maintainer: Peter Gamnitzer
            gamnitzer@lnm.mw.tum.de
            http://www.lnm.mw.tum.de/Members/gammi/
            +49-(0)89-289-15235
</pre>

------------------------------------------------------------------------*/
/*!
\addtogroup FLUID2
*//*! @{ (documentation module open)*/
#ifdef D_FLUID2
#include "../headers/standardtypes.h"
#include "../fluid2/fluid2.h"
#include "../fluid2/fluid2_prototypes.h"

#ifdef D_FLUID2_TDS
#include "fluid2_TDS_prototypes.h"

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

/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | vector of material laws                                              |
 | defined in global_control.c                                          |
 *----------------------------------------------------------------------*/
extern struct _MATERIAL  *mat;

static FLUID_DYNAMIC *fdyn;


/*!---------------------------------------------------------------------
\brief update of time dependent subscales

<pre>                                                        gammi 11/06

The time dependent pressure subscales are updated according to a
one step theta timestepping scheme.

                  dp_sub       1
                  ------ = - ----- * p_sub + res_C(u)
                    dt       tau_C

Here, res_C(u)=div(u) is the residual of the continuity equation.

</pre>
\param  *actpart       PARTITION        (i)   
\param  *actintra      INTRA            (i)   
\param  *actfield      FIELD            (i)   
\param  *ipos          ARRAY_POSITION   (i)
\param   disnum_calc   INT              (i)   
\return void

------------------------------------------------------------------------*/


void f2_update_subscale_pres(
    PARTITION      *actpart,
    INTRA          *actintra,
    FIELD          *actfield,
    ARRAY_POSITION *ipos,
    INT             disnum_calc)
{
/* multi purpose counter */
int      i;

/* counter for elements */
int      nele;

/* element related data */
int      ls,lr;
int      nis=0,nir=0;
int      icode;
int      ihoel;
double   det;

ELEMENT *ele;
NODE    *actnode;

/* material properties */
double   visc;

/* time algorithm */
double   theta;
double   dt;

/* constants containing dt and the stabilisation parameter */
double   facC,facCtau;

/* new and old residual of the coninuity equation */
double   divu,divu_old;

/* the old subscale pressure */
double   sp_old;

/* a constant, */
double   CRHS;

static ARRAY     xyze_a;   /* actual element coordinates                */
static DOUBLE  **xyze;
static ARRAY     xjm_a;    /* jocobian matrix                           */
static DOUBLE  **xjm;
static ARRAY     funct_a;  /* shape functions                           */
static DOUBLE   *funct;
static ARRAY     deriv_a;  /* first natural derivatives                 */
static DOUBLE  **deriv;
static ARRAY     eveln_a;  /* element velocities at (n)                 */
static DOUBLE  **eveln;
static ARRAY     evelng_a; /* element velocities at (n+gamma)           */
static DOUBLE  **evelng;
static ARRAY     derxy_a;  /* coordinate - derivatives                  */
static DOUBLE  **derxy;
static ARRAY     vderxy_a; /* vel - derivatives                         */
static DOUBLE  **vderxy;


/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_enter("f2_update_subscale_pres");
#endif

/*========================== initialisation ============================*/
fdyn = alldyn[genprob.numff].fdyn;

dt   =fdyn->dt;
theta=fdyn->theta;

xyze      = amdef("xyze"     ,&xyze_a     ,2,MAXNOD_F2,"DA");
xjm       = amdef("xjm"      ,&xjm_a      ,2,2        ,"DA");
funct     = amdef("funct"    ,&funct_a    ,MAXNOD_F2,1,"DV");
deriv     = amdef("deriv"    ,&deriv_a    ,2,MAXNOD_F2,"DA");
eveln     = amdef("eveln"    ,&eveln_a    ,NUM_F2_VELDOF,MAXNOD_F2,"DA");
evelng    = amdef("evelng"   ,&evelng_a   ,NUM_F2_VELDOF,MAXNOD_F2,"DA");
vderxy    = amdef("vderxy"   ,&vderxy_a   ,2,2,"DA");
derxy     = amdef("derxy"    ,&derxy_a    ,2,MAXNOD_F2,"DA");
  


for(nele=0;nele<actpart->pdis[disnum_calc].numele;nele++)
{
    ele=actpart->pdis[disnum_calc].element[nele];

/*------- get integraton data and check if elements are "higher order" */
    switch (ele->distyp)
    {
	case quad4: case quad8: case quad9:  /* --> quad - element */
	    icode   = 3; /* flag for higher order elements                 */
	    ihoel   = 1; /* flag for eveluation of shape functions         */
	    nir = ele->e.f2->nGP[0];
	    nis = ele->e.f2->nGP[1];
	    break;
	case tri6: /* --> tri - element */
	    icode   = 3; /* flag for higher order elements                 */
	    ihoel   = 1; /* flag for eveluation of shape functions         */
	    nir  = ele->e.f2->nGP[0];
	    nis  = 1;	    
	    break;
	case tri3:
	    ihoel  =0;  /* flag for higher order elements                 */
	    icode  =2;  /* flag for eveluation of shape functions         */
	    nir  = ele->e.f2->nGP[0];
	    nis  = 1;
	    break;
	default:
	    dserror("typ unknown!");
    } /* end switch(typ) */

  
    /*------------------------------------------ set element coordinates -*/
    for(i=0;i<ele->numnp;i++)
    {
	xyze[0][i]=ele->node[i]->x[0];
	xyze[1][i]=ele->node[i]->x[1];
    }

    /* -> implicit time integration method ---------*/
    for(i=0;i<ele->numnp;i++) /* loop nodes of element */
    {
	actnode=ele->node[i];
        /*----------------------------- set recent element velocities */
	evelng[0][i]=actnode->sol_increment.a.da[ipos->velnp][0];
	evelng[1][i]=actnode->sol_increment.a.da[ipos->velnp][1];
	eveln [0][i]=actnode->sol_increment.a.da[ipos->veln ][0];
	eveln [1][i]=actnode->sol_increment.a.da[ipos->veln ][1];
    } /* end of loop over nodes of element */

    
    /*---------------------------------------------- get viscosity ---*/
    visc = mat[ele->mat-1].m.fluid->viscosity;
    
    /*--------------------------------------------- stab-parameter ---*/
    f2_caltau(ele,xyze,funct,deriv,xjm,evelng,visc);

    facC   =1./(fdyn->tau[2]+theta*dt);
    facCtau=fdyn->tau[2]*facC;
    
/*----------------------------------------------------------------------*
 |               start loop over integration points                     |
 *----------------------------------------------------------------------*/
    for (lr=0;lr<nir;lr++)
    {
	for (ls=0;ls<nis;ls++)
	{
	    
	    /*------------------ compute Jacobian matrix at time n+1 ---*/
	    f2_jaco(xyze,deriv,xjm,&det,ele->numnp,ele);
	    
	    /*----------------------------- compute global derivates ---*/
	    f2_gder(derxy,deriv,xjm,det,ele->numnp);
   
	    /*--- get velocity (n+1,i) derivatives at integration point */
	    f2_vder(vderxy,derxy,evelng,ele->numnp);

	    divu     = vderxy[0][0] + vderxy[1][1];

	    /*--- get velocity (n+1,i) derivatives at integration point */
	    f2_vder(vderxy,derxy,eveln,ele->numnp);

	    divu_old = vderxy[0][0] + vderxy[1][1];

	    sp_old=ele->e.f2->sub_pres.a.dv[lr*nis+ls];
	    
	    CRHS=fdyn->tau[2]*sp_old-(1.-theta)*dt*(sp_old+fdyn->tau[2]*divu_old);

	    ele->e.f2->sub_pres.a.dv[lr*nis+ls]=
		facC*(CRHS-fdyn->tau[2]*theta*dt*divu);

	} /* end of loop over integration points ls*/
    } /* end of loop over integration points lr */
}


/* clean up ------------------------------------------------------------*/

amdel(&xyze_a);
amdel(&xjm_a);
amdel(&funct_a);
amdel(&deriv_a);
amdel(&eveln_a);
amdel(&evelng_a);
amdel(&derxy_a);
amdel(&vderxy_a);

/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_exit();
#endif

return;
}


/*!---------------------------------------------------------------------
\brief update of time dependent subscale velocities

<pre>                                                        gammi 11/06

The time dependent velocity subscales are updated according to a
one step theta time integration scheme of the equation

                  du_sub       1
                  ------ = - ----- * u_sub + res_M(u)
                    dt       tau_M

Here, res_M(u) is the residual of the momentum equation and contains a
time derivative, a convective term, a diffusion term, the pressure
gradient and the volume force.
                    
</pre>
\param  *actpart       PARTITION        (i)   
\param  *actintra      INTRA            (i)   
\param  *actfield      FIELD            (i)   
\param  *ipos          ARRAY_POSITION   (i)
\param   disnum_calc   INT              (i)   
\return void

------------------------------------------------------------------------*/


void f2_update_subscale_vel(
    PARTITION      *actpart,
    INTRA          *actintra,
    FIELD          *actfield,
    ARRAY_POSITION *ipos,
    INT             disnum_calc)
{
/* multi purpose counter */    
int      i;

/* counter for dimensions (x,y) */
int     dim;

/* counter for elements */
int      nele;

/* element related data */
DOUBLE   e1,e2;             /* natural coordinates of integr. point */
int      ls,lr;             /* counters for Gausspoints             */
int      nis=0,nir=0;       /* total number of integration points   */
int      icode=0,intc=0;    /* required for tri elements            */
int      ihoel=0;           /* higher order element flag            */
double   det;               /* Jacobideterminant                    */

ELEMENT *ele;
NODE    *actnode;

FLUID_DATA      *data;      /* Information on Gausspoints etc.      */

/* material properties */
double   visc;

/* time algorithm */
double  theta;
double  dt;

/* constants containing dt and the stabilisation parameter */
double  facM,facMtau;

/* vector of subscale velocities */
double  sv_old[2];
/* residuals of both timesteps (without time derivative) */
double  res_old[2];
double  res_new[2];

/* */
double  time_der[2];

/* factors from curves for timedependent deadload */
double  acttimefacn,acttimefac;

/* information on the use of ele->e.f2->sub_vel.a.d3                    */
int     old=0,new=1;

/* velocity vectors at integration point      */
DOUBLE  velint[2],velint_old[2];
/* pressure and pressure gradients */
DOUBLE  press    ,press_old    ;
DOUBLE  gradp [2],gradp_old [2];
/* higher order terms */
DOUBLE  hot   [2],hot_old   [2];

static ARRAY     xyze_a;   /* actual element coordinates                */
static DOUBLE  **xyze;
static ARRAY     xjm_a;    /* jocobian matrix                           */
static DOUBLE  **xjm;
static ARRAY     funct_a;  /* shape functions                           */
static DOUBLE   *funct;
static ARRAY     deriv_a;  /* first natural derivatives                 */
static DOUBLE  **deriv;
static ARRAY     eveln_a;  /* element velocities at (n)                 */
static DOUBLE  **eveln;
static ARRAY     evelng_a; /* element velocities at (n+gamma)           */
static DOUBLE  **evelng;
static ARRAY     derxy_a;  /* coordinate - derivatives                  */
static DOUBLE  **derxy;
static ARRAY     vderxy_a; /* vel - derivatives                         */
static DOUBLE  **vderxy;
static ARRAY     vderxy_old_a; /* vel - derivatives                     */
static DOUBLE  **vderxy_old;
static ARRAY     epren_a;  /* element pressures at (n)	                */
static DOUBLE   *epren;
static ARRAY     epreng_a; /* element pressures at (n)	                */
static DOUBLE   *epreng;
static ARRAY     edeadn_a; /* element dead load (selfweight)            */
static DOUBLE   *edeadng;
static ARRAY     edeadng_a;/* element dead load (selfweight)            */
static DOUBLE   *edeadn;
static ARRAY     w1_a;     /* working array of arbitrary chosen size    */
static DOUBLE  **wa1;      /* used in different element routines        */
static ARRAY     w2_a;     /* working array of arbitrary chosen size    */
static DOUBLE  **wa2;      /* used in different element routines        */
static ARRAY     vderxy2_a;/* vel - 2nd derivatives                     */
static DOUBLE  **vderxy2;
static ARRAY     vderxy2_old_a;/* vel - 2nd derivatives                     */
static DOUBLE  **vderxy2_old;
static ARRAY     derxy2_a; /* 2nd coordinate - derivatives              */
static DOUBLE  **derxy2;
static ARRAY     deriv2_a; /* second natural derivatives                */
static DOUBLE  **deriv2;

/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_enter("f2_update_subscale_vel");
#endif

/*========================== initialisation ============================*/
fdyn   = alldyn[genprob.numff].fdyn;
data   = fdyn->data;

dt   =fdyn->dt;
theta=fdyn->theta;

xyze       = amdef("xyze"      ,&xyze_a      ,2,MAXNOD_F2,"DA");
xjm        = amdef("xjm"       ,&xjm_a       ,2,2        ,"DA");
funct      = amdef("funct"     ,&funct_a     ,MAXNOD_F2,1,"DV");
deriv      = amdef("deriv"     ,&deriv_a     ,2,MAXNOD_F2,"DA");
deriv2     = amdef("deriv2"   ,&deriv2_a   ,3,MAXNOD_F2,"DA");
eveln      = amdef("eveln"     ,&eveln_a     ,NUM_F2_VELDOF,MAXNOD_F2,"DA");
evelng     = amdef("evelng"     ,&evelng_a   ,NUM_F2_VELDOF,MAXNOD_F2,"DA");
vderxy     = amdef("vderxy"    ,&vderxy_a    ,2,2,"DA");
vderxy_old = amdef("vderxy_old",&vderxy_old_a,2,2,"DA");
vderxy2    = amdef("vderxy2"  ,&vderxy2_a  ,2,3,"DA");
vderxy2_old = amdef("vderxy2_old"  ,&vderxy2_old_a  ,2,3,"DA");
derxy      = amdef("derxy"     ,&derxy_a     ,2,MAXNOD_F2,"DA");
derxy2     = amdef("derxy2"   ,&derxy2_a   ,3,MAXNOD_F2,"DA");
epren      = amdef("epren"     ,&epren_a     ,MAXNOD_F2,1,"DV");
epreng     = amdef("epreng"    ,&epreng_a    ,MAXNOD_F2,1,"DV");
edeadn     = amdef("edeadn"    ,&edeadn_a    ,2,1,"DV");
edeadng    = amdef("edeadng"   ,&edeadng_a   ,2,1,"DV");
wa1        = amdef("wa1"      ,&w1_a       ,MAXDOFPERELE,MAXDOFPERELE,"DA");
wa2        = amdef("wa2"      ,&w2_a       ,MAXDOFPERELE,MAXDOFPERELE,"DA");
 


for(nele=0;nele<actpart->pdis[disnum_calc].numele;nele++)
{
    ele=actpart->pdis[disnum_calc].element[nele];

/*------- get integraton data and check if elements are "higher order" */
    switch (ele->distyp)
    {
	case quad4: case quad8: case quad9:  /* --> quad - element */
	    icode   = 3; /* flag for higher order elements                 */
	    ihoel   = 1; /* flag for eveluation of shape functions         */
	    nir  = ele->e.f2->nGP[0];
	    nis  = ele->e.f2->nGP[1];
	    intc = ele->e.f2->nGP[1];
	    break;
	case tri6: /* --> tri - element */
	    icode   = 3; /* flag for higher order elements                 */
	    ihoel   = 1; /* flag for eveluation of shape functions         */
	    nir  = ele->e.f2->nGP[0];
	    nis  = 1;
	    intc = ele->e.f2->nGP[1];
	    break;
	case tri3:
	    ihoel  =0;  /* flag for higher order elements                 */
	    icode  =2;  /* flag for eveluation of shape functions         */
	    nir  = ele->e.f2->nGP[0];
	    nis  = 1;
	    intc = ele->e.f2->nGP[1];
	    break;
	default:
	    dserror("typ unknown!");
    } /* end switch(typ) */

  
    /*------------------------------------------ set element coordinates -*/
    for(i=0;i<ele->numnp;i++)
    {
	xyze[0][i]=ele->node[i]->x[0];
	xyze[1][i]=ele->node[i]->x[1];
    }

    /* -> implicit time integration method ---------*/
    for(i=0;i<ele->numnp;i++) /* loop nodes of element */
    {
	actnode=ele->node[i];
        /*----------------------------- set recent element velocities */
	evelng[0][i]=actnode->sol_increment.a.da[ipos->velnp][0];
	evelng[1][i]=actnode->sol_increment.a.da[ipos->velnp][1];
	eveln [0][i]=actnode->sol_increment.a.da[ipos->veln ][0];
	eveln [1][i]=actnode->sol_increment.a.da[ipos->veln ][1];
	
	epreng   [i]=actnode->sol_increment.a.da[ipos->velnp][2];
	epren    [i]=actnode->sol_increment.a.da[ipos->veln ][2];
    } /* end of loop over nodes of element */

    if(ele->g.gsurf->neum!=NULL)
    {

	if (ele->g.gsurf->neum->curve<1)
	{
	    acttimefac =ONE;
	    acttimefacn=ONE;
	}
	else
	{
	    dyn_facfromcurve(ele->g.gsurf->neum->curve-1,
			     fdyn->acttime,
			     &acttimefac) ;

	    dyn_facfromcurve(ele->g.gsurf->neum->curve-1,
			     fdyn->acttime-fdyn->dta,
			     &acttimefacn) ;
	}
	
	for (i=0;i<2;i++)
	{
	    if (ele->g.gsurf->neum->neum_onoff.a.iv[i]==0)
	    {
		edeadn[i]  = 0.0;
		edeadng[i] = 0.0;
	    }
	    if (ele->g.gsurf->neum->neum_type==neum_dead  &&
		ele->g.gsurf->neum->neum_onoff.a.iv[i]!=0)
	    {
		edeadn [i] = ele->g.gsurf->neum->neum_val.a.dv[i]*acttimefacn;
		edeadng[i] = ele->g.gsurf->neum->neum_val.a.dv[i]*acttimefac ;
	    }
	}
    }
    else
    {
	for (i=0;i<2;i++)
	{
	    edeadn [i] = 0.0;
	    edeadng[i] = 0.0;
	}
    }
    
    /*---------------------------------------------- get viscosity ---*/
    visc = mat[ele->mat-1].m.fluid->viscosity;
    
    /*--------------------------------------------- stab-parameter ---*/
    f2_caltau(ele,xyze,funct,deriv,xjm,evelng,visc);

    facM   =         1.0/(fdyn->tau[0]+theta*dt);
    facMtau=fdyn->tau[0]/(fdyn->tau[0]+theta*dt);

/*----------------------------------------------------------------------*
 |               start loop over integration points                     |
 *----------------------------------------------------------------------*/
    for (lr=0;lr<nir;lr++)
    {
	for (ls=0;ls<nis;ls++)
	{
	    /*------- get values of  shape functions and their derivatives ---*/
	    switch(ele->distyp)
	    {
		case quad4: case quad8: case quad9:   /* --> quad - element */
		    e1   = data->qxg[lr][nir-1];
		    e2   = data->qxg[ls][nis-1];
		    f2_rec(funct,deriv,deriv2,e1,e2,ele->distyp,icode);
		    break;
		case tri3: case tri6:   /* --> tri - element */
		    e1   = data->txgr[lr][intc];
		    e2   = data->txgs[lr][intc];
		    f2_tri(funct,deriv,deriv2,e1,e2,ele->distyp,icode);
		    break;
		default:
		    dserror("typ unknown!");
	    } /* end switch(typ) */

	    /*-------------- get velocities (n) at integration point ---*/
	    f2_veci(velint_old,funct,eveln ,ele->numnp);
	    /*------------ get velocities (n+1) at integration point ---*/
	    f2_veci(velint    ,funct,evelng,ele->numnp);
	    /*-------------------- get pressure at integration point ---*/
	    press_old = 0;
	    press = 0;
	    for (i=0;i<ele->numnp;i++)
	    {
		actnode=ele->node[i];

		/*--------------------------------- get pressure (n) ---*/
		press_old +=funct[i]*epren[i];
		/*------------------------------- get pressure (n+1) ---*/
		press     +=funct[i]*epreng[i];
	    }
	    	    
	    /*------------------ compute Jacobian matrix at time n+1 ---*/
	    f2_jaco(xyze,deriv,xjm,&det,ele->numnp,ele);
	    
	    /*----------------------------- compute global derivates ---*/
	    f2_gder(derxy,deriv,xjm,det,ele->numnp);
   
	    /*--- get velocity (n+1,i) derivatives at integration point */
	    f2_vder(vderxy,derxy,evelng,ele->numnp);

	    if (ihoel!=0)
	    {
		f2_gder2(xyze,xjm,wa1,wa2,derxy,derxy2,deriv2,ele->numnp);
		f2_vder2(vderxy2,derxy2,evelng,ele->numnp);
	    }

	    /*------- get velocity (n) derivatives at integration point */
	    f2_vder(vderxy_old,derxy,eveln,ele->numnp);

	    if (ihoel!=0)
	    {
		f2_vder2(vderxy2_old,derxy2,eveln,ele->numnp);
	    }

	    /*------------------------------- get pressure gradients ---*/
	    gradp[0] = gradp[1] = 0.0;
	    
	    for (i=0; i<ele->numnp; i++)
	    {
		gradp[0] += derxy[0][i] * epreng[i];
		gradp[1] += derxy[1][i] * epreng[i];
	    }

	    gradp_old[0] = gradp_old[1] = 0.0;

	    for (i=0; i<ele->numnp; i++)
	    {
		gradp_old[0] += derxy[0][i] * epren[i];
		gradp_old[1] += derxy[1][i] * epren[i];
	    }
	    /*------------------------------ get higher order terms ---*/
	    if(ihoel!=0)
	    {
		hot    [0]=0.5 * (2.0*vderxy2[0][0]
				  +
				  (vderxy2[0][1] + vderxy2[1][2]));
		hot    [1]=0.5 * (2.0*vderxy2[1][1]
				  +
				  (vderxy2[1][0] + vderxy2[0][2]));
		
		hot_old[0]=0.5 * (2.0*vderxy2_old[0][0]
			   	  +
			   	  (vderxy2_old[0][1] + vderxy2_old[1][2]));
		hot_old[1]=0.5 * (2.0*vderxy2_old[1][1]
			   	  +
			   	  (vderxy2_old[1][0] + vderxy2_old[0][2]));
	    }	
	    else
	    {
		hot    [0]=0;
		hot    [1]=0;
		hot_old[0]=0;
		hot_old[1]=0;
	    }

	    for (dim=0;dim<2;dim++)
	    {
		/* the current subscales velocities become the most
		 * recent subscale velocities for the next timestep    */
		/* sv_old is just an abbreviation                      */
		sv_old[dim]=ele->e.f2->sub_vel.a.d3[new][dim][lr*nis+ls];
		ele->e.f2->sub_vel.a.d3[old][dim][lr*nis+ls]=sv_old[dim];

		/* calculate new residual without time derivative       */

		res_new[dim]=0;
		res_new[dim]+=(velint[0]*vderxy[dim][0]
			       +
			       velint[1]*vderxy[dim][1]);
		res_new[dim]-=2*visc*hot[dim];
		res_new[dim]+=gradp[dim];
		res_new[dim]-=edeadng[dim];

		/* calculate old residual without time derivative       */
		res_old[dim]=0;
		res_old[dim]+=(velint_old[0]*vderxy_old[dim][0]
			       +
			       velint_old[1]*vderxy_old[dim][1]);
		
		res_old[dim]-=2*visc*hot_old[dim];
		res_old[dim]+=gradp_old[dim];
		res_old[dim]-=edeadn[dim];

		/* calculate the time derivative                        */
		time_der[dim]=velint[dim]-velint_old[dim];


		/* set new subscale velocities                          */
		ele->e.f2->sub_vel.a.d3[new][dim][lr*nis+ls]=
		     facMtau*                sv_old[dim]
		    -facMtau*(               time_der[dim]
			      +theta    *dt* res_new[dim]
			      +(1-theta)*dt* res_old[dim])
		    -facM   *  (1-theta)*dt* sv_old[dim];
	    }
	} /* end of loop over integration points ls*/
    } /* end of loop over integration points lr */
}

/* clean up ------------------------------------------------------------*/
amdel(&xyze_a);
amdel(&xjm_a);
amdel(&funct_a);
amdel(&deriv_a);
amdel(&eveln_a);
amdel(&evelng_a);
amdel(&derxy_a);
amdel(&vderxy_a);
amdel(&vderxy_old_a);
amdel(&epren_a);
amdel(&epreng_a);
amdel(&edeadn_a);
amdel(&edeadng_a);
amdel(&w1_a);
amdel(&w2_a);
amdel(&derxy2_a);
amdel(&deriv2_a);
amdel(&vderxy2_a);
amdel(&vderxy2_old_a);


/*----------------------------------------------------------------------*/
#ifdef DEBUG
dstrc_exit();
#endif

return;
}


#endif /*D_FLUID2_TDS*/
#endif /*D_FLUID2*/
/*! @} (documentation module close)*/

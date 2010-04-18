/*======================================================================*/
/*!
\file
\brief Stiffness matrix (tangent) of THERM2 element

<pre>
Maintainer: Burkhard Bornemann
            bornemann@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089-289-15237
</pre>
*/


/*----------------------------------------------------------------------*/
#ifndef CCADISCRET
#ifdef D_THERM2

/*----------------------------------------------------------------------*/
/* headers */
#include "../headers/standardtypes.h"
#include "therm2.h"

/*!
\addtogroup THERM2
*//*! @{ (documentation module open)*/


/*----------------------------------------------------------------------*/
/*!
\brief General problem data

\author bborn
\date 03/06
*/
extern struct _GENPROB genprob;


/*----------------------------------------------------------------------*/
/*!
\brief Locally global allocatables

\author bborn
\date 03/06
*/
static INT allocated = 0;  /* allocation flag */
static ARRAY    cmat_a;  /* material tensor */
static DOUBLE **cmat;
static ARRAY    shape_a;  /* shape functions */
static DOUBLE  *shape;
static ARRAY    deriv_a;  /* derivatives of shape functions */
static DOUBLE **deriv;
static ARRAY    xjm_a;  /* Jacobian matrix */
static DOUBLE **xjm;
static ARRAY    bop_a;  /* B-operator */
static DOUBLE **bop;
static DOUBLE  *hflux;
static ARRAY    hflux_a;  /* heat flux vector */

/*======================================================================*/
/*!
\brief Initialise locally globals

Allocate locally globals

\author bborn
\date 03/06
*/
void th2_lin_init()
{
  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("th2_lin_init");
#endif

  /*--------------------------------------------------------------------*/
  /* allocation */
  if (allocated == 0)
  {
    shape = amdef("shape", &shape_a, MAXNOD_THERM2, 1, "DV");
    deriv = amdef("deriv", &deriv_a, NDIM_THERM2, MAXNOD_THERM2, "DA");
    cmat = amdef("cmat", &cmat_a, NUMTMGR_THERM2, NUMTMGR_THERM2, "DA");
    xjm = amdef("xjm", &xjm_a, NDIM_THERM2, NDIM_THERM2, "DA");
    bop = amdef("bop", &bop_a, NUMTMGR_THERM2, (NUMDOF_THERM2*MAXNOD_THERM2), "DA");
    hflux = amdef("hflux", &hflux_a, NUMHFLX_THERM2, 1, "DV");
    /* initialisation */
    amzero(&cmat_a);
    /* flag alloaction */
    allocated = 1;
  }

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}  /* end of th2_lin_init */


/*=====================================================================*/
/*!
\brief Finalise locally globals

Deallocate locally globals

\author bborn
\date 03/06
*/
void th2_lin_final()
{

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("th2_lin_init");
#endif

  /*--------------------------------------------------------------------*/
  /* deallocation */
  if (allocated == 1)
  {
    amdel(&shape_a);
    amdel(&deriv_a);
    amdel(&cmat_a);
    amdel(&xjm_a);
    amdel(&bop_a);
    amdel(&hflux_a);
    /* reset allocation flag */
    allocated = 0;
  }

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}  /* end of th2_lin_final */


/*======================================================================*/
/*!
\brief Calculate stiffness matrix of linear heat conduction THERM2
       element

The element stiffness matrix, ie the tangent operator, is determined
for the linear planar heat conduction problem.

\param   *ele           ELEMENT     (i)   pointer to current element
\param   *data          TH2_DATA    (i)
\param   *estif_global  ARRAY       (o)
\param   *emass_global  ARRAY       (o)  global vector for mass
\param   *force         DOUBLE      (o)  global vector for internal
                                           forces (initialized!)
\return void

\author bborn
\date 03/06
*/
void th2_lin_stiff(ELEMENT *ele,
                   TH2_DATA *data,
                   MATERIAL *mat,
                   ARRAY *estif_global,
                   ARRAY *emass_global,
                   DOUBLE *force)
{

  /* general variables/constants */
  /* const INT ndim = genprob.ndim; */  /* ought to be 2 */
  INT nelenod;  /* numnp to this element */
  INT neledof;
  DOUBLE thick;  /* thickness */

  /* integration */
  INT nir = 0;  /* num GP in r-direction */
  INT nis = 0;  /* num GP in s-direction */
  INT intc = 0;  /* "integration case" for tri-element */
  INT lr, ls;   /* loopers over GP */
  INT ip;  /* current total index of Gauss point */
  DOUBLE fac; /* integration factors */
  DOUBLE e1 = 0.0;  /* r-coord current GP */
  DOUBLE e2 = 0.0;  /* s-coord current GP */
  DOUBLE facr = 0.0;  /* weights at GP */
  DOUBLE facs = 0.0;  /* weights at GP */

  DOUBLE det;  /* Jacobi-det */

  /* element quantities */
  static DOUBLE **estif;  /* element stiffness matrix (convenience) */
  /* static DOUBLE **emass; */  /* mass matrix (convenience) */

  /*--------------------------------------------------------------------*/
  /* start */
#ifdef DEBUG
  dstrc_enter("th2_stiff_lin");
#endif


  /*====================================================================*/
  /* initialise data */
  /*--------------------------------------------------------------------*/
  /* check calculation of `mass' matrix */
  /* to be done */

  /*--------------------------------------------------------------------*/
  /* some of the fields have to be reinitialized to zero */
  amzero(estif_global);
  estif = estif_global->a.da;
  /*--------------------------------------------------------------------*/
  /* integration parameters */
  nelenod = ele->numnp;
  neledof = NUMDOF_THERM2 * nelenod;
  /*--------------------------------------------------------------------*/
  thick = ele->e.th2->thick;

  /*====================================================================*/
  /* compute stiffness matrix */
  /*--------------------------------------------------------------------*/
  /* get integraton data */
  switch (ele->distyp)
  {
    case quad4: case quad8: case quad9:  /* --> quad - element */
      nir = ele->e.th2->nGP[0];
      nis = ele->e.th2->nGP[1];
      break;
    case tri3: case tri6:  /* --> tri - element */
      nir  = 1;
      nis  = ele->e.th2->nGP[0];
      intc = ele->e.th2->gpintc;
      break;
    default:
      dserror("ele->distyp unknown!");
  }  /* end of switch(ele->distyp) */
  /*--------------------------------------------------------------------*/
  /* integration loops */
  ip = -1;
  for (lr=0; lr<nir; lr++)
  {
    for (ls=0; ls<nis; ls++)
    {
      /* obtain values of shape functions and their derivatives */
      switch (ele->distyp)
      {
        /* --> quad - element */
        case quad4: case quad8: case quad9:
          e1   = data->gqlc[lr][nir-1];
          facr = data->gqlw[lr][nir-1];
          e2   = data->gqlc[ls][nis-1];
          facs = data->gqlw[ls][nis-1];
          break;
        /* --> tri - element */
        case tri3: case tri6:
          e1   = data->gtdcr[ls][intc];
          facr = 1.0;
          e2   = data->gtdcs[ls][intc];
          facs = data->gtdw[ls][intc];
          break;
        default:
          dserror("ele->distyp unknown!");
      }  /* end of switch (ele->distyp) */
      /* increment Gauss integration point counter */
      ip++;
      /*----------------------------------------------------------------*/
      /* shape functions and their derivatives */
      th2_shape_deriv(shape, deriv, e1, e2, ele->distyp, 1);
      /*----------------------------------------------------------------*/
      /* compute Jacobian matrix and its determinant */
      th2_jaco(deriv, xjm, &det, ele, nelenod);
      /*----------------------------------------------------------------*/
      /* integration (quadrature) factor */
      fac = facr * facs * det * thick;
      /*----------------------------------------------------------------*/
      /* compute `mass' matrix */
      /* to be done */
      /*----------------------------------------------------------------*/
      /* calculate operator B */
      amzero(&bop_a);
      th2_bop(bop, deriv, xjm, det, nelenod);
      /*----------------------------------------------------------------*/
      /* call material law */
      th2_mat_sel(ele, mat, bop, ip, hflux, cmat);
      /*----------------------------------------------------------------*/
      /* element stiffness matrix estif */
      th2_lin_bcb(estif, bop, cmat, fac, neledof, NUMTMGR_THERM2);
      /*----------------------------------------------------------------*/
      /* element nodal forces fi from integration of stresses */
      if (force)
      {
        th2_lin_fint(hflux, fac, bop, neledof, force);
      }
    }  /* end of for (lr=0; lr<nir; lr++) */
  }  /* end of for (ls=0; ls<nis; ls++) */

  /*====================================================================*/
  /* local coordinate system */
  dsassert(ele->locsys == locsys_no,
           "locsys not implemented for this element!\n");

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
} /* end of th2_lin_stiff(...) */


/*======================================================================*/
/*!
\brief Classic B^T C B operation for one Gauss point

\param **s          DOUBLE  (o)    element stiffness matrix increment of
                                     current Gauss point
\param **bs         DOUBLE  (i)    B-operator
\param **d          DOUBLE  (i)    constitutive matrix
\param   fac        DOUBLE  (i)    integration factor of current GP
\param   neledof    INT     (i)    number of element DOFs
\param   ntmgr      INT     (i)    number of temperature gradients
                                     identically number of heat flux
                                     components
\return void

\author bborn
\date 03/06
*/
void th2_lin_bcb(DOUBLE  **s,
                 DOUBLE  **bs,
                 DOUBLE  **d,
                 DOUBLE    fac,
                 INT       neledof,
                 INT       ntmgr)
{
  INT i, j, k, l, m;  /* counters */
  DOUBLE dbk, sij;  /* intermediate sums */
  DOUBLE db[NUMTMGR_THERM2];  /* db_k = cmat_kj * bop_jl for l */

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("th2_lin_bcb");
#endif

  /*--------------------------------------------------------------------*/
  for (j=0; j<neledof; j++)
  {
    for (k=0; k<ntmgr; k++)
    {
      dbk = 0.0;
      for (l=0; l<ntmgr; l++)
      {
        dbk = dbk + d[k][l]*bs[l][j]*fac;
      }
      db[k] = dbk;
    }
    for (i=0; i<neledof; i++)
    {
      sij = 0.0;
      for (m=0; m<ntmgr; m++)
      {
        sij = sij + bs[m][i]*db[m];
      }
      s[i][j] = s[i][j] + sij;
    }
  }

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
   dstrc_exit();
#endif
   return;
} /* end of th2_bcb(...) */




/*======================================================================*/
/*!
\brief Evaluate element nodal forces

The element nodal forces 'fie' are incremented by the contribution
of the current Gauss point

\param   **hflux     DOUBLE   (i)    heat flux for current GP
\param     fac       DOUBLE   (i)    Gauss quadrature factor mult. etc.
\param   **bop       DOUBLE   (i)    B-operator for current GP
\param     neledof   INT      (i)    number of element nodes
\param    *fie       DOUBLE   (io)   element nodal force

\author bborn
\date 03/06
*/
void th2_lin_fint(DOUBLE  *hflux,
                  DOUBLE   fac,
                  DOUBLE **bop,
                  INT      neledof,
                  DOUBLE  *fie)
{
  /*--------------------------------------------------------------------*/
  INT i;  /* counters */
  DOUBLE hfluxfac0, hfluxfac1;  /* heatflux multiplied by 'fac' */

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("th2_lin_fint");
#endif

  /*--------------------------------------------------------------------*/
  hfluxfac0 = hflux[0] * fac;
  hfluxfac1 = hflux[1] * fac;

  /*--------------------------------------------------------------------*/
  /* geometrically linear */
  for (i=1; i<neledof; i++)
  {
    fie[i] += bop[0][i]*hfluxfac0 + bop[1][i]*hfluxfac1;
  }

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}  /* end of th2_lin_fint(...) */


/*======================================================================*/
#endif /* end of #ifdef D_THERM2 */
/*! @} (documentation module close)*/
#endif

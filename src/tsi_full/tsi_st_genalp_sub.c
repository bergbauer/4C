/*======================================================================*/
/*!
\file
\brief TSI - time integration of structure field

<pre>
Maintainer: Burkhard Bornemann
            bornemann@lnm.mw.tum.de
            http://www.lnm.mw.tum.de/Members/bornemann
            089-289-15237
</pre>

\author bborn
\date 03/06
*/

#ifndef CCADISCRET

/*----------------------------------------------------------------------*/
/* header files */
#include "../headers/standardtypes.h"
#include "../solver/solver.h"
#ifdef BINIO
#include "../io/io.h"
#endif
#include "tsi_prototypes.h"


/*----------------------------------------------------------------------*/
/*!
\brief File pointers
       This structure struct _FILES allfiles is defined in
       input_control_global.c and the type is in standardtypes.h
       It holds all file pointers and some variables needed for the FRSYSTEM
\author bborn
\date 03/06
*/
/*----------------------------------------------------------------------*/
extern FILES allfiles;


/*----------------------------------------------------------------------*/
/*!
\brief General problem data
       defined in global_control.c
\author bborn
\date 03/06
*/
extern GENPROB genprob;


/*----------------------------------------------------------------------*/
/*!
\brief Fields
       vector of numfld FIELDs, defined in global_control.c
\author bborn
\date 03/06
*/
extern FIELD *field;


/*----------------------------------------------------------------------*/
/*!
\brief Global nodal solution vectors in solver-specific format
       global variable *solv, vector of lenght numfld of structures SOLVAR
       defined in solver_control.c
\author bborn
\date 03/06
*/
/*----------------------------------------------------------------------*/
extern SOLVAR *solv;


/*----------------------------------------------------------------------*/
/*!
\brief One proc's info about his partition
       - the partition of one proc (all discretizations)
       - the type is in partition.h
\author bborn
\date 03/06
*/
extern PARTITION *partition;


/*----------------------------------------------------------------------*/
/*!
\brief Input/output control flags
       structure of flags to control output defined in out_global.c
\author bborn
\date 03/06
*/
extern IO_FLAGS ioflags;


/*----------------------------------------------------------------------*/
/*!
\brief Rank and communicators (Parallelism!)
       This structure struct _PAR par; is defined in main_ccarat.c
       and the type is in partition.h
\author born
\date 03/06
*/
extern PAR par;


/*----------------------------------------------------------------------*/
/*!
\brief Dynamic control
       pointer to allocate dynamic variables if needed
       dedfined in global_control.c
\author bborn
\date 03/06
*/
extern ALLDYNA *alldyn;


/*----------------------------------------------------------------------*/
/*!
\brief Load curve, load factor function
       number of load curves numcurve
       vector of structures of curves
       defined in input_curves.c
\author bborn
\date 03/06
*/
extern INT numcurve;
extern CURVE *curve;


/*----------------------------------------------------------------------*/
/*!
\brief CALC_ACTIONs
       command passed from control routine to the element level
       to tell element routines what to do
       defined globally in global_calelm.c
\author bborn
\date 03/06
*/
extern CALC_ACTION calc_action[MAXFIELD];


/*---------------------------------------------------------------------*/
/*!
\brief Actual (current) time globally given
\author bborn
\date 03/06
*/
DOUBLE acttime;
DOUBLE deltat;


/*======================================================================*/
/*!
\brief Initialise generalised-alpha time integration of structure field
\param  actpart     PARTITION*      (io)   partition
\param  actintra    INTRA*          (i)    intra-communicator
\param  actfield    FIELD           (i)    thermal field
\param  disnum      INT             (i)    discretisation index
\param  ipos        ARRAY_POSITION* (i)    indices in NODE arrays
\param  isol        ARRAY_POSITION_SOL*(i) indices in NODE sol arrays
\param  isolinc     ARRAY_POSITION_SOLINC*(i) indices in NODE sol_increment
\param  actsolv     SOLVAR*         (io)   solution variables
\param  numeq_total INT*            (o)    total number of equations
\param  numeq       INT*            (o)    number of equations
\param  stiff_array INT*            (o)    tangent array index
\param  mass_array  INT*            (o)    mass array index
\param  damp_array  INT*            (o)    damp array index
\param  actdyn      STRUCT_DYNAMIC* (i)    structure dyn. control
\param  dynvar      STRUCT_DYN_CALC*(i)    dynamic variables
\param  container   CONTAINER*      (i)    container
\param  out_context BIN_OUT_FIELD*  (io)   BINIO context
\param  dispi_num   INT             (i)    number of dispi vectors
\param  dispi       DIST_VECTOR**   (io)   dispi vectors
\param  vel_num     INT             (i)    number of vel vectors
\param  vel         DIST_VECTOR**   (io)   velocity vectors
\param  acc_num     INT             (i)    number of acc vectors
\param  acc         DIST_VECTOR**   (io)   acceleration vectors
\param  fie_num     INT             (i)    number of fie vectors
\param  fie         DIST_VECTOR**   (io)   internal force vectors
\param  work_num    INT             (i)    number of work vectors
\param  work        DIST_VECTOR**   (io)   working vectors
\param  intforce_a  ARRAY*          (io)   global int. force vector
\param  dirich_a    ARRAY*          (io)   Dirichlet RHS contribution
\author bborn
\date 05/07
*/
void tsi_st_genalp_init(PARTITION* actpart,
                        INTRA* actintra,
                        FIELD* actfield,
                        INT disnum,
                        ARRAY_POSITION* ipos,
                        ARRAY_POSITION_SOL* isol,
                        ARRAY_POSITION_SOLINC* isolinc,
                        SOLVAR* actsolv,
                        INT* numeq_total,
                        INT* numeq,
                        INT* stiff_array,
                        INT* mass_array,
                        INT* damp_array,
                        STRUCT_DYNAMIC* actdyn,
                        STRUCT_DYN_CALC* dynvar,
                        CONTAINER* container,
#ifdef BINIO
                        BIN_OUT_FIELD* out_context,
#endif
                        INT vel_num,
                        DIST_VECTOR** vel,
                        INT acc_num,
                        DIST_VECTOR** acc,
                        INT fie_num,
                        DIST_VECTOR** fie,
                        INT dispi_num,
                        DIST_VECTOR** dispi,
                        INT work_num,
                        DIST_VECTOR** work,
                        ARRAY* intforce_a,
                        ARRAY* dirich_a)
{
  const INT numsf = genprob.numsf;  /* index of structure field */
  CALC_ACTION* action = &(calc_action[numsf]);  /* structure cal_action */
  INT i;  /* index */
  INT init;  /* solver init flag */
  INT actsysarray;  /* WHY???? */
  DIST_VECTOR* distemp1;

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("tsi_st_genalp_init");
#endif

  /*--------------------------------------------------------------------*/
  /* init the variables in dynvar to zero */
  /* Set all variables to zero. No matter what changes in future. */
  memset(dynvar, 0, sizeof(STRUCT_DYN_CALC));

  /*--------------------------------------------------------------------*/
  /* check solvar variable */
  if (actsolv->nsysarray == 1)
  {
    actsysarray = 0;
  }
  else
  {
    dserror("More than 1 system arrays (actsolv->nsysarray)!");
  }

  /*--------------------------------------------------------------------*/
  /* damping */
  if (actdyn->damp == 1)
  {
    *stiff_array = 0;
    *mass_array = 1;
    *damp_array = 2;
    actsolv->nsysarray = 3;
  }
  else
  {
    *stiff_array = 0;
    *mass_array = 1;
    *damp_array = -1;
    actsolv->nsysarray = 2;
  }

  /*--------------------------------------------------------------------*/
  /*--------------------------------------------------------------------*/
  /* allocate sparse mass (and damping) matrix */
  /* reallocate the vector of sparse matrices and the vector of
   * their types : formerly lenght 1, now lenght 2 or 3 dependent on
   * presence of damp_array */
  actsolv->sysarray_typ
    = (SPARSE_TYP*) CCAREALLOC(actsolv->sysarray_typ,
                               actsolv->nsysarray*sizeof(SPARSE_TYP));
  if (!actsolv->sysarray_typ)
  {
    dserror("Allocation of memory failed");
  }
  actsolv->sysarray
    = (SPARSE_ARRAY*) CCAREALLOC(actsolv->sysarray,
                               actsolv->nsysarray*sizeof(SPARSE_ARRAY));
  if (!actsolv->sysarray)
  {
    dserror("Allocation of memory failed");
  }
  /* copy the matrices sparsity mask from stiff_array to mass_array */
  solserv_alloc_cp_sparsemask(actintra,
                              &(actsolv->sysarray_typ[*stiff_array]),
                              &(actsolv->sysarray[*stiff_array]),
                              &(actsolv->sysarray_typ[*mass_array]),
                              &(actsolv->sysarray[*mass_array]));

  if (*damp_array > 0)
  {
    solserv_alloc_cp_sparsemask(actintra,
                                &(actsolv->sysarray_typ[*stiff_array]),
                                &(actsolv->sysarray[*stiff_array]),
                                &(actsolv->sysarray_typ[*damp_array]),
                                &(actsolv->sysarray[*damp_array]));
  }

  /*--------------------------------------------------------------------*/
  /* init the dist sparse matrices to zero */
  for (i=0; i<actsolv->nsysarray; i++)
  {
    solserv_zero_mat(actintra,
                     &(actsolv->sysarray[i]),
                     &(actsolv->sysarray_typ[i]));
  }

  /*--------------------------------------------------------------------*/
  /* get global and local number of equations */
  solserv_getmatdims(&(actsolv->sysarray[*stiff_array]),
                     actsolv->sysarray_typ[*stiff_array],
                     numeq, numeq_total);

  /*--------------------------------------------------------------------*/
  /* allocate 4 distributed vectors for RHS */
  /* these hold original load vector,
   *            load vector at time t
   *            load vctor at time at t-dt
   *            and interpolated load vector */
  actsolv->nrhs = 4;
  solserv_create_vec(&(actsolv->rhs),
                     actsolv->nrhs, *numeq_total, *numeq, "DV");
  for (i=0; i<actsolv->nrhs; i++)
  {
    solserv_zero_vec(&(actsolv->rhs[i]));
  }

  /*--------------------------------------------------------------------*/
  /* allocate 2 dist. solution/displacement vectors */
  /* displacement vector at t_{n+1}
   * displacement vector at t_{n} */
  actsolv->nsol = 2;
  solserv_create_vec(&(actsolv->sol),
                     actsolv->nsol, *numeq_total, *numeq, "DV");
  for (i=0; i<actsolv->nsol; i++)
  {
    solserv_zero_vec(&(actsolv->sol[i]));
  }

  /*--------------------------------------------------------------------*/
  /* allocate 1 dist vector for iterative displacements increments */
  solserv_create_vec(dispi, dispi_num, *numeq_total, *numeq, "DV");
  for (i=0; i<dispi_num; i++)
  {
    solserv_zero_vec(dispi[i]);
  }

  /*--------------------------------------------------------------------*/
  /* allocate 1 dist vector for velocities */
  solserv_create_vec(vel, vel_num, *numeq_total, *numeq, "DV");
  for (i=0; i<vel_num; i++)
  {
    solserv_zero_vec(vel[i]);
  }

  /*--------------------------------------------------------------------*/
  /* allocate 1 dist vector for accelerations */
  solserv_create_vec(acc, acc_num, *numeq_total, *numeq, "DV");
  for (i=0; i<acc_num; i++)
  {
    solserv_zero_vec(acc[i]);
  }

  /*--------------------------------------------------------------------*/
  /* create 1 redundant full-length vector for internal forces */
  amdef("intforce_s", intforce_a, *numeq_total, 1, "DV");

  /*--------------------------------------------------------------------*/
  /* create 1 vector of full length for Dirichlet part of RHS */
  amdef("dirich_s", dirich_a, *numeq_total, 1, "DV");

  /*--------------------------------------------------------------------*/
  /* allocate 3 dist. vectors for internal forces */
  /* internal force at t_{n+1}
   * internal force at t_{n}
   * mid-internal force at t_{n+1/2} */ /*fie_x, fie_y, fie_z;*/
#if 1
  solserv_create_vec(fie, fie_num, *numeq_total, *numeq, "DV");
  for (i=0; i<fie_num; i++)
  {
    solserv_zero_vec(fie[i]);
  }
#endif

  /*--------------------------------------------------------------------*/
  /* allocate 3 dist. working vectors */
#if 1
  solserv_create_vec(work, work_num, *numeq_total, *numeq, "DV");
  for (i=0; i<work_num; i++)
  {
    solserv_zero_vec(work[i]);
  }
#endif

  /*--------------------------------------------------------------------*/
  /* b */
  solserv_create_vec(&distemp1, work_num, *numeq_total, *numeq, "DV");
  for (i=0; i<work_num; i++)
  {
    solserv_zero_vec(&(distemp1[i]));
  }

  /*--------------------------------------------------------------------*/
  /* initialize solver on all matrices */
  /* NOTE: solver init phase has to be called with each matrix one wants to
   *       solve with. Solver init phase has to be called with all matrices
   *       one wants to do matrix-vector products and matrix scalar products.
   *       This is not needed by all solver libraries, but the solver-init
   *       phase is cheap in computation (can be costly in memory)
   *       There will be no solver call on mass or damping array. */

  /*--------------------------------------------------------------------*/
  /* initialize solver */
  init = 1;
  solver_control(actfield, disnum, actsolv, actintra,
		 &(actsolv->sysarray_typ[*stiff_array]),
                 &(actsolv->sysarray[*stiff_array]),
                 dispi[0], &(actsolv->rhs[0]), init);
#if 1
  solver_control(actfield, disnum, actsolv, actintra,
                 &(actsolv->sysarray_typ[*mass_array]),
                 &(actsolv->sysarray[*mass_array]),
                 work[0], work[1], init);
  if (*damp_array > 0)
  {
    solver_control(actfield, disnum, actsolv, actintra,
		   &(actsolv->sysarray_typ[*damp_array]),
                   &(actsolv->sysarray[*damp_array]),
                   work[0], work[1], init);
  }
#else
  solver_control(actfield, disnum, actsolv, actintra,
                 &(actsolv->sysarray_typ[*mass_array]),
                 &(actsolv->sysarray[*mass_array]),
                 &(distemp1[0]), &(distemp1[1]), init);
  if (*damp_array > 0)
  {
    solver_control(actfield, disnum, actsolv, actintra,
		   &(actsolv->sysarray_typ[*damp_array]),
                   &(actsolv->sysarray[*damp_array]),
                   &(distemp1[0]), &(distemp1[1]), init);
  }
#endif

  /*--------------------------------------------------------------------*/
  /* init the assembly for stiffness and for mass matrix */
  /* (damping is not assembled) */
  init_assembly(actpart, actsolv, actintra, actfield, *stiff_array, disnum);
  init_assembly(actpart, actsolv, actintra, actfield, *mass_array, disnum);

  /*--------------------------------------------------------------------*/
  /* init the element calculating routines */
  *action = calc_struct_init;
  calinit(actfield, actpart, action, container);

  /*--------------------------------------------------------------------*/
  /* call elements to calculate stiffness and mass */
  if (*damp_array > 0)
  {
  *action = calc_struct_nlnstiffmass;
  container->dvec = NULL;
  container->dirich = NULL;
  container->global_numeq = 0;  /* WHY NOT  *numeq_total */
  container->dirichfacs = NULL;
  container->kstep = 0;
/*   deltat = actdyn->dt;  /\* NECESSARY ???????? *\/ */
  calelm(actfield, actsolv, actpart, actintra,
         *stiff_array, *mass_array, container, action);
  }

  /*--------------------------------------------------------------------*/
  /* calculate damping matrix */
  if (*damp_array > 0)
  {
    /* stiffness proportional contribution */
    solserv_add_mat(actintra,
                    &(actsolv->sysarray_typ[*damp_array]),
                    &(actsolv->sysarray[*damp_array]),
                    &(actsolv->sysarray_typ[*stiff_array]),
                    &(actsolv->sysarray[*stiff_array]),
                    actdyn->k_damp);
    /* mass proportional contribution */
    solserv_add_mat(actintra,
                    &(actsolv->sysarray_typ[*damp_array]),
                    &(actsolv->sysarray[*damp_array]),
                    &(actsolv->sysarray_typ[*mass_array]),
                    &(actsolv->sysarray[*mass_array]),
                    actdyn->m_damp);
    solserv_close_mat(actintra,
		      &(actsolv->sysarray_typ[*damp_array]),
		      &(actsolv->sysarray[*damp_array]));
  }



  /*--------------------------------------------------------------------*/
  /* put a zero to the place ipos->num=12 in node->sol to init the
   * velocities and accels of prescribed displacements */
  /* HINT: This actually redefines/reallocates/enlarges the sol
   *       array of each structure node to dimension 12x2 (or 12x3)
   *       from originally 1x2 (or 1x3) */
  solserv_sol_zero(actfield, disnum, node_array_sol, ipos->num-1);

  /*--------------------------------------------------------------------*/
  /* put a zero to the place ipos->numincr=2 in sol_increment of NODEs */
  /* later this will hold internal forces at t_{n+1} and t_{n} */
  /* HINT: This actually redefines/reallocates/enlarges the sol_increment
   *       array at each structure node to dimension 2x2 (or 2x3)
   *       from originally 1x2 (or 1x3) */
  /* initialise internal forces f_{int;n} to zero */
  solserv_sol_zero(actfield, disnum, node_array_sol_increment,
                   isolinc->fint);
  /* initialise internal forces f_{int;n+1} to zero */
  solserv_sol_zero(actfield, disnum, node_array_sol_increment,
                   isolinc->fintn);


  /*--------------------------------------------------------------------*/
  /* WARNING : BINIO is not available --- work needs to be done */
#ifdef BINIO
  /* initialize binary output
   * It's important to do this only after all the node arrays are set
   * up because their sizes are used to allocate internal memory. */
  init_bin_out_field(out_context,
                     &(actsolv->sysarray_typ[*stiff_array]),
                     &(actsolv->sysarray[*stiff_array]),
                     actfield, actpart, actintra, disnum);
#endif

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}

/*======================================================================*/
/*!
\brief Implicit predictor
\param  actpart     PARTITION*      (io)   partition
\param  actintra    INTRA*          (i)    intra-communicator
\param  actfield    FIELD           (i)    thermal field
\param  disnum      INT             (i)    discretisation index
\param  isol        ARRAY_POSITION_SOL*(i) indices in NODE sol arrays
\param  isolinc     ARRAY_POSITION_SOLINC*(i) indices in NODE sol_increment
\param  isolres     ARRAY_POSITION_SOLRES*(i) indices in NODE sol_residual
\param  actsolv     SOLVAR*         (io)   solution variables
\param  numeq_total INT             (i)    total number of equations
\param  stiff_array INT             (i)    tangent array index
\param  mass_array  INT             (i)    mass array index
\param  damp_array  INT             (i)    damp array index
\param  actdyn      STRUCT_DYNAMIC* (i)    structure dyn. control
\param  dynvar      STRUCT_DYN_CALC*(i)    dynamic variables
\param  container   CONTAINER*      (i)    container
\param  dispi       DIST_VECTOR*    (io)   dispi vectors
\param  vel         DIST_VECTOR*    (io)   velocity vectors
\param  acc         DIST_VECTOR*    (io)   acceleration vectors
\param  fie         DIST_VECTOR*    (io)   internal force vectors
\param  work        DIST_VECTOR*    (io)   working vectors
\param  intforce_a  ARRAY*          (io)   global int. force vector
\param  dirich_a    ARRAY*          (io)   Dirichlet RHS contribution
\author bborn
\date 05/07
*/
void tsi_st_genalp_pred(PARTITION* actpart,
                        INTRA* actintra,
                        FIELD* actfield,
                        INT disnum,
                        ARRAY_POSITION_SOL* isol,
                        ARRAY_POSITION_SOLINC* isolinc,
                        ARRAY_POSITION_SOLRES* isolres,
                        SOLVAR* actsolv,
                        INT numeq_total,
                        INT stiff_array,
                        INT mass_array,
                        INT damp_array,
                        STRUCT_DYNAMIC* actdyn,
                        STRUCT_DYN_CALC* dynvar,
                        CONTAINER* container,
                        DIST_VECTOR* vel,
                        DIST_VECTOR* acc,
                        DIST_VECTOR* fie,
                        DIST_VECTOR* dispi,
                        DIST_VECTOR* work,
                        ARRAY* dirich_a,
                        ARRAY* intforce_a)
{
  DOUBLE dirichfacs[10];  /* factors needed for Dirichlet-part of RHS */
  const INT numsf = genprob.numsf;  /* index of structure field */
  CALC_ACTION* action = &(calc_action[numsf]);  /* structure cal_action */
  /* const INT actcurve = 0; */  /* index of active time curve */
  DOUBLE* intforce = intforce_a->a.dv;
  DOUBLE* dirich = dirich_a->a.dv;
  INT init;

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("tsi_st_genalp_pred");
#endif

  /*--------------------------------------------------------------------*/
  /* set some constants */
  dyn_setconstants(dynvar, actdyn, actdyn->dt);

  /*--------------------------------------------------------------------*/
  /* set incremental displacements dispi[0] to zero */
  solserv_zero_vec(&(dispi[0]));

  /*--------------------------------------------------------------------*/
  /* set residual/iterative displacements in nodes to zero */
  solserv_result_resid(actfield, disnum, actintra, &dispi[0],
                       isolres->disres,
                       &(actsolv->sysarray[stiff_array]),
                       &(actsolv->sysarray_typ[stiff_array]));

  /*--------------------------------------------------------------------*/
  /*  this vector holds loads due to external forces */
  solserv_zero_vec(&(actsolv->rhs[1]));
  container->kstep = 0;
  container->inherit = 1;  /* ??? */
  container->point_neum = 1;  /* ??? */
  *action = calc_struct_eleload;
  calrhs(actfield, actsolv, actpart, actintra, stiff_array,
         &(actsolv->rhs[1]), action, container);
#if 0
  {
    INT i;
    for (i=0; i<numeq_total; i++)
    {
      if (isnan(actsolv->rhs[1].vec.a.dv[i]))
      {
        printf("Load %d: %24.10e\n", i, actsolv->rhs[1].vec.a.dv[i]);
        abort();
      }
    }
  }
#endif

  /*--------------------------------------------------------------------*/
  /* multiply rhs[1] by load factor based on factor rldfac of curve 0 */
  /* WARNING: This control routine at the moment always uses curve 0
   *          for the complete RHS */
  /* Get factor at new time t_{n+1} */
  /* dyn_facfromcurve(actcurve, actdyn->time, &(dynvar->rldfac)); */
  dynvar->rldfac = 1.0;
  solserv_scalarprod_vec(&(actsolv->rhs[1]), dynvar->rldfac);

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement into local system
   * prior to calculation/assignment of locally oriented
   * prescribed DBC values */
#ifdef LOCALSYSTEMS_ST
  solserv_zerodirich(actfield, disnum, node_array_sol, isol->disdn);
#endif

  /*--------------------------------------------------------------------*/
  /* put the scaled prescribed displacements to the nodes in field sol
   * at place 4 separate of the free DOFs
   * These are used to calculate the RHS due to the Dirichlet conditions */
  /* In the case of locally oriented DBCs, these prescribed DOFs are
   * given in the local system. */
  solserv_putdirich_to_dof(actfield, disnum, node_array_sol,
                           isol->disdn, actdyn->time);

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement back into global system
   * post assignment of presc. DBC values;
   * This operation will not only rotate the prescribed DOFs of the relevant
   * Dirichlet node, but all (i.e. prescribed & free) its DOFs. */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->disdn,
                          locsys_trf_xyz_to_XYZ);
#endif

  /*--------------------------------------------------------------------*/
  /* put presdisplacements(t_{n+1}) - presdisplacements(t_n) in place 5 */
  solserv_adddirich(actfield, disnum, node_array_sol,
                    isol->disd, isol->disdn, isol->disdi, -1.0, 1.0);

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement increments into local system
   * these are needed to determine the so-called Dirichlet forces
   * (in calelm) */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->disdi,
                          locsys_trf_XYZ_to_xyz);
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->veldn,
                          locsys_trf_XYZ_to_xyz);
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->accdn,
                          locsys_trf_XYZ_to_xyz);
#endif

  /*--------------------------------------------------------------------*/
  /* set factors needed for prescribed displacement terms on eff RHS */
  /* dirichfacs[0] = -(1.0-alpham)*(1.0/beta)/(DSQR(dt))
   * dirichfacs[1] =  (1.0-alpham)*(1.0/beta)/dt
   * dirichfacs[2] =  (1.0-alpham)/(2*beta) - 1
   * dirichfacs[3] = -(1.0-alphaf)*(gamma/beta)/dt
   * dirichfacs[4] =  (1.0-alphaf)*gamma/beta - 1
   * dirichfacs[5] =  (gamma/(2*beta)-1)*(1.0-alphaf)
   * dirichfacs[6] = -(1.0-alphaf) or 0
   * dirichfacs[7] =  raleigh damping factor for mass
   * dirichfacs[8] =  raleigh damping factor for stiffness
   * dirichfacs[9] =  dt
   * see PhD theses Mok page 165: Generalized-alpha time integration
   *                              with prescribed displ. */
  dirichfacs[0] = -dynvar->constants[0];
  dirichfacs[1] = +dynvar->constants[1];
  dirichfacs[2] = +dynvar->constants[2];
  dirichfacs[3] = -dynvar->constants[3];
  dirichfacs[4] = +dynvar->constants[4];
  dirichfacs[5] = +dynvar->constants[5];
  dirichfacs[6] = -dynvar->constants[6];
  if (damp_array > 0)
  {
    dirichfacs[7] = actdyn->m_damp;
    dirichfacs[8] = actdyn->k_damp;
  }
  else
  {
    dirichfacs[7] = 0.0;
    dirichfacs[8] = 0.0;
  }
  dirichfacs[9] = actdyn->dt;

  /*--------------------------------------------------------------------*/
  /* calculate tangential stiffness/mass
   * and internal forces at time t_{n} */
  solserv_zero_mat(actintra,
                   &(actsolv->sysarray[stiff_array]),
                   &(actsolv->sysarray_typ[stiff_array]));
  solserv_zero_mat(actintra,
                   &(actsolv->sysarray[mass_array]),
                   &(actsolv->sysarray_typ[mass_array]));
  amzero(dirich_a);
  amzero(intforce_a);

  /*--------------------------------------------------------------------*/
  /*  call elements */
  *action = calc_struct_nlnstiffmass;
  container->isdyn = 1;
  container->dvec = intforce;
  container->dirich = dirich;
  container->global_numeq = numeq_total;
  container->dirichfacs = dirichfacs;
  container->kstep = 0;
  calelm(actfield, actsolv, actpart, actintra,
         stiff_array, mass_array, container, action);
#if 0
  {
    INT i;
    for (i=0; i<numeq_total; i++)
    {
      if (isnan(intforce[i]))
      {
        printf("Intforce %d: %24.10e\n", i, intforce[i]);
        exit(0);
      }
      if (isnan(dirich[i]))
      {
        printf("DirichForce %d: %24.10e\n", i, dirich[i]);
        abort();
      }
    }
  }
#endif

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement increments back into global system */
  /* these were needed to determine the so-called Dirichlet forces
   * (in calelm) */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->disdi,
                          locsys_trf_xyz_to_XYZ);
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol,isol->veldn,
                          locsys_trf_xyz_to_XYZ);
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->accdn,
                          locsys_trf_xyz_to_XYZ);
#endif

  /*--------------------------------------------------------------------*/
  /* store positive internal forces on fie[1] */
  solserv_zero_vec(&(fie[1]));
  assemble_vec(actintra,
               &(actsolv->sysarray_typ[stiff_array]),
               &(actsolv->sysarray[stiff_array]),
               &(fie[1]), intforce, 1.0);

  /*--------------------------------------------------------------------*/
  /* determine external mid-force vector by interpolating */
  /* forces rhs[0] = (1-alphaf)*rhs[1] + alphaf*rhs[2] */
  solserv_copy_vec(&(actsolv->rhs[2]), &(actsolv->rhs[0]));
  solserv_scalarprod_vec(&(actsolv->rhs[0]), actdyn->alpha_f);
  solserv_add_vec(&(actsolv->rhs[1]), &(actsolv->rhs[0]),
                  (1.0-actdyn->alpha_f));

  /*--------------------------------------------------------------------*/
  /* subtract internal forces from interpolated external forces */
  solserv_add_vec(&(fie[1]), &(actsolv->rhs[0]), -1.0);

  /*--------------------------------------------------------------------*/
  /* add rhs from prescribed displacements to RHS */
  assemble_vec(actintra, &(actsolv->sysarray_typ[stiff_array]),
               &(actsolv->sysarray[stiff_array]),
               &(actsolv->rhs[0]), dirich, 1.0);

  /*--------------------------------------------------------------------*/
  /* create effective load vector (rhs[0]-fie[2])eff */
  /* Peff = rhs[0] - fie[0]
   *   + M*(-a1*dispi[0]+a2*vel[0]+a3*acc[0])
   *   + D*(-a4*dispi[0]+a5*vel[0]+a6*acc[0]) (if present)
   *
   *   a1 = dynvar.constants[0]
   *      =  (1.0-alpham) * (1.0/beta)/(DSQR(dt))
   *   a2 =
   *      = ((1.0-alpham) * (1.0/beta)/(DSQR(dt)))*dt
   *   a3 = dynvar.constants[2]
   *      =  (1.0-alpham) / (2.0*beta) - 1.0
   *   a4 = dynvar.constants[3]
   *      =  (1.0-alphaf) * ((gamma/beta)/dt)
   *   a5 = dynvar.constants[4]
   *      =  ((1.0-alphaf) * ((gamma/beta)/dt))*dt - 1.0
   *   a6 =
   *      = (gamma/beta)/2.0 - 1.0) * dt * (1.0-alphaf) */
  pefnln_struct(dynvar, actdyn, actfield, actsolv, actintra,
                dispi, vel, acc, work, mass_array, damp_array);

  /*--------------------------------------------------------------------*/
  /* create effective stiffness matrix */
  /* keff = constants[6] * K + constants[0] * M + constants[3] * D
   *   constants[6] =  (1.0-alphaf)
   *   constants[0] =  (1.0-alpham) * (1.0/beta)/(DSQR(dt))
   *   constants[3] =  (1.0-alphaf) * ((gamma/beta)/dt) */
  kefnln_struct(dynvar, actdyn, actfield, actsolv, actintra,
                work, stiff_array, mass_array, damp_array);

  /*--------------------------------------------------------------------*/
  /* call for solution of system dispi[0] = Keff^-1 * rhs[0] */
  init = 0;
  solver_control(actfield, disnum, actsolv, actintra,
                 &(actsolv->sysarray_typ[stiff_array]),
                 &(actsolv->sysarray[stiff_array]),
                 &(dispi[0]), &(actsolv->rhs[0]), init);

  /*--------------------------------------------------------------------*/
  /* blank prior to calculation/assignment of locally oriented
   * residual DBC displacements */
#ifdef LOCALSYSTEMS_ST
  solserv_zerodirich(actfield, disnum, node_array_sol_residual,
                     isolres->disres);
#endif

  /*--------------------------------------------------------------------*/
  /* return residual/iterative displacements \iinc D_{n+1}^<i+1>
   * to the nodes
   * These are needed for updating internal element variables. */
  solserv_result_resid(actfield, disnum, actintra,
                       &(dispi[0]),
                       isolres->disres,
                       &(actsolv->sysarray[stiff_array]),
                       &(actsolv->sysarray_typ[stiff_array]));

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement back into global system */
  /* post return residual displacements to nodes
   * This operation will not only rotate the free DOFs of relevant
   * Dirichlet nodes, but all (i.e. prescribed & free) its DOFs.
   * This should not matter, 'coz the residual displacements are zero
   * at prescribed nodes */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol_residual,
                          isolres->disres, locsys_trf_xyz_to_XYZ);
#endif

  /*====================================================================*/
  /* update */

  /*--------------------------------------------------------------------*/
  /* iterative update of internal variables of elements */
  *action = calc_struct_update_iterstep;
  container->dvec = NULL;
  container->dirich = NULL;
  container->global_numeq = 0;
  container->kstep = 0;
  calelm(actfield, actsolv, actpart, actintra,
         stiff_array, -1, container, action);

  /*--------------------------------------------------------------------*/
  /* update displacements sol[1] = sol[0] + dispi[0] */
  solserv_copy_vec(&(actsolv->sol[0]), &(actsolv->sol[1]));
  solserv_add_vec(&(dispi[0]), &(actsolv->sol[1]), 1.0);

  /*--------------------------------------------------------------------*/
  /* blank prior to calculation/assignment of locally oriented
   * prescribed DBC values */
#ifdef LOCALSYSTEMS_ST
  solserv_zerodirich(actfield, disnum, node_array_sol, isol->disn);
#endif

  /*--------------------------------------------------------------------*/
  /* put the scaled prescribed displacements to the nodes */
  /* in field sol (0) at place 0 together with free displacements
   * these are used to calculate the stiffness matrix */
  /* In the case of locally oriented DBCs, these prescribed DOFs are
   * given in the local system. */
  solserv_putdirich_to_dof(actfield, disnum,
                           node_array_sol, isol->disn, actdyn->time);

  /*--------------------------------------------------------------------*/
  /* return total displacements to the nodes */
  solserv_result_total(actfield, disnum, actintra,
                       &(actsolv->sol[1]),
                       isol->disn,
                       &(actsolv->sysarray[stiff_array]),
                       &(actsolv->sysarray_typ[stiff_array]));

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement back into global system */
  /* post return total displacements to nodes,
   * This operation will not only rotate the prescribed DOFs of the relevant
   * Dirichlet node, but all (i.e. prescribed & free) its DOFs. */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->disn,
                          locsys_trf_xyz_to_XYZ);
#endif

  /*--------------------------------------------------------------------*/
  /* blank prior to calculation/assignment of locally oriented
   * increments of DBC displacements */
#ifdef LOCALSYSTEMS_ST
  solserv_zerodirich(actfield, disnum, node_array_sol_increment,
                     isolinc->disinc);
#endif

  /*--------------------------------------------------------------------*/
  /* return incremental displacements to the nodes */
  solserv_result_incre(actfield, disnum, actintra,
                       &dispi[0],
                       isolinc->disinc,
                       &(actsolv->sysarray[stiff_array]),
                       &(actsolv->sysarray_typ[stiff_array]));

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement back into global system */
  /* post return incremental displacements to nodes
   * This operation will not only rotate the free DOFs of relevant
   * Dirichlet nodes, but all (i.e. prescribed & free) its DOFs.
   * This should not matter, 'coz the increments are zero at prescribed nodes */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol_increment,
                          isolinc->disinc, locsys_trf_xyz_to_XYZ);
#endif

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}

/*======================================================================*/
/*!
\brief Equilibrium iteration
\param  actpart     PARTITION*      (io)   partition
\param  actintra    INTRA*          (i)    intra-communicator
\param  actfield    FIELD           (i)    thermal field
\param  disnum      INT             (i)    discretisation index
\param  isol        ARRAY_POSITION_SOL*(i) indices in NODE sol arrays
\param  isolinc     ARRAY_POSITION_SOLINC*(i) indices in NODE sol_increment
\param  isolres     ARRAY_POSITION_SOLRES*(i) indices in NODE sol_residual
\param  actsolv     SOLVAR*         (io)   solution variables
\param  numeq       INT             (i)    number of equations
\param  stiff_array INT             (i)    tangent array index
\param  mass_array  INT             (i)    mass array index
\param  damp_array  INT             (i)    damp array index
\param  actdyn      STRUCT_DYNAMIC* (i)    structure dyn. control
\param  dynvar      STRUCT_DYN_CALC*(i)    dynamic variables
\param  container   CONTAINER*      (i)    container
\param  dispi       DIST_VECTOR*    (io)   dispi vectors
\param  vel         DIST_VECTOR*    (io)   velocity vectors
\param  acc         DIST_VECTOR*    (io)   acceleration vectors
\param  fie         DIST_VECTOR*    (io)   internal force vectors
\param  work        DIST_VECTOR*    (io)   working vectors
\param  intforce_a  ARRAY*          (io)   global int. force vector
\param  dirich_a    ARRAY*          (io)   Dirichlet RHS contribution
\author bborn
\date 05/07
*/
void tsi_st_genalp_equi(PARTITION* actpart,
                        INTRA* actintra,
                        FIELD* actfield,
                        INT disnum,
                        ARRAY_POSITION_SOL* isol,
                        ARRAY_POSITION_SOLINC* isolinc,
                        ARRAY_POSITION_SOLRES* isolres,
                        SOLVAR* actsolv,
                        INT numeq_total,
                        INT stiff_array,
                        INT mass_array,
                        INT damp_array,
                        STRUCT_DYNAMIC* actdyn,
                        STRUCT_DYN_CALC* dynvar,
                        CONTAINER* container,
                        DIST_VECTOR* vel,
                        DIST_VECTOR* acc,
                        DIST_VECTOR* fie,
                        DIST_VECTOR* dispi,
                        DIST_VECTOR* work,
                        ARRAY* intforce_a,
                        ARRAY* dirich_a)
{
  DOUBLE dirichfacs[10];  /* factors needed for Dirichlet-part of rhs */
  const INT numsf = genprob.numsf;  /* index of structure field */
  CALC_ACTION* action = &(calc_action[numsf]);  /* structure cal_action */
  DOUBLE* intforce = intforce_a->a.dv;  /* internal forces */
  DOUBLE* dirich = dirich_a->a.dv;  /* Dirichlet part of RHS */
  INT init;  /* init flag for solver */

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("tsi_st_genalp_equi");
#endif

  /*--------------------------------------------------------------------*/
  /* set factors needed for prescribed displacement terms on eff RHS */
  dirichfacs[0] = -dynvar->constants[0];
  dirichfacs[1] =  dynvar->constants[1];
  dirichfacs[2] =  dynvar->constants[2];
  dirichfacs[3] = -dynvar->constants[3];
  dirichfacs[4] =  dynvar->constants[4];
  dirichfacs[5] =  dynvar->constants[5];
  dirichfacs[6] =  0.0;
  if (damp_array > 0)
  {
    dirichfacs[7] = actdyn->m_damp;
    dirichfacs[8] = actdyn->k_damp;
  }
  else
  {
    dirichfacs[7] = 0.0;
    dirichfacs[8] = 0.0;
  }
  dirichfacs[9] = actdyn->dt;

  /*--------------------------------------------------------------------*/
  /* zero the stiffness matrix
   * and vector for internal forces and Dirichlet forces */
  solserv_zero_mat(actintra,
                   &(actsolv->sysarray[stiff_array]),
                   &(actsolv->sysarray_typ[stiff_array]));
  solserv_zero_mat(actintra,
                   &(actsolv->sysarray[mass_array]),
                   &(actsolv->sysarray_typ[mass_array]));
  amzero(intforce_a);
  amzero(dirich_a);

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement increments into local system
   * these are needed to determine the so-called Dirichlet forces
   * (in calelm) */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->disdi,
                          locsys_trf_XYZ_to_xyz);
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->veldn,
                          locsys_trf_XYZ_to_xyz);
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->accdn,
                          locsys_trf_XYZ_to_xyz);
#endif

  /*--------------------------------------------------------------------*/
  /* call element routines for calculation of
   * tangential stiffness and intforce */
  *action = calc_struct_nlnstiffmass;
  solserv_sol_zero(actfield, disnum, node_array_sol_increment,
                   isolinc->fintn);
  container->dvec = intforce;
  container->dirich = dirich;
  container->global_numeq = numeq_total;
  container->dirichfacs = dirichfacs;
  container->kstep = 0;
  calelm(actfield, actsolv, actpart, actintra,
         stiff_array, mass_array, container, action);

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement increments back into global system */
  /* these were needed to determine the so-called Dirichlet forces
   * (in calelm) */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->disdi,
                          locsys_trf_xyz_to_XYZ);
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->veldn,
                          locsys_trf_xyz_to_XYZ);
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->accdn,
                          locsys_trf_xyz_to_XYZ);
#endif

  /*--------------------------------------------------------------------*/
  /*  store positive internal forces on fie[2] */
  solserv_zero_vec(&fie[2]);
  assemble_vec(actintra, &(actsolv->sysarray_typ[stiff_array]),
               &(actsolv->sysarray[stiff_array]),
               &(fie[2]), intforce, 1.0);

  /*--------------------------------------------------------------------*/
  /* mid external force by interpolating */
  /* rhs[0] = (1-alphaf)rhs[1] + alphaf*rhs[2] */
  solserv_copy_vec(&(actsolv->rhs[2]), &(actsolv->rhs[0]));
  solserv_scalarprod_vec(&(actsolv->rhs[0]), actdyn->alpha_f);
  solserv_add_vec(&(actsolv->rhs[1]),
                  &(actsolv->rhs[0]),
                  (1.0 - actdyn->alpha_f));

  /*--------------------------------------------------------------------*/
  /* mid internal force by interpolating */
  /*  fie[0] = (1-alfaf)fie[2] + alphaf*fie[1] */
  solserv_copy_vec(&fie[2], &fie[0]);
  solserv_scalarprod_vec(&fie[0], (1.0-actdyn->alpha_f));
  solserv_add_vec(&fie[1], &fie[0], actdyn->alpha_f);

  /*--------------------------------------------------------------------*/
  /* subtract mid internal forces from mid external forces */
  solserv_add_vec(&fie[0], &(actsolv->rhs[0]), -1.0);

  /*--------------------------------------------------------------------*/
  /* add Dirichlet forces from prescribed displacements */
  /* ===> GENERALLY THIS SHOULD BE WRONG!!! --- HOWEVER, CCARAT MAY
   *      NEED IT ???? */
  assemble_vec(actintra,
               &(actsolv->sysarray_typ[stiff_array]),
               &(actsolv->sysarray[stiff_array]),
               &(actsolv->rhs[0]), dirich, 1.0);

  /*--------------------------------------------------------------------*/
  /*  create effective load vector (rhs[0]-fie[0])eff */
  pefnln_struct(dynvar, actdyn, actfield, actsolv, actintra,
                dispi, vel, acc, work, mass_array, damp_array);

  /*--------------------------------------------------------------------*/
  /* create effective stiffness matrix */
  kefnln_struct(dynvar, actdyn, actfield, actsolv, actintra,
                work, stiff_array, mass_array, damp_array);

  /*--------------------------------------------------------------------*/
  /* solve keff * work[0] = rhs[0] */
  /* solve for residual/iterative displacements \iinc\D_{n+1}^<i+1>
   * to correct incremental displacements \inc\D_{n+1}^<i> */
  init = 0;
  solver_control(actfield, disnum, actsolv, actintra,
                 &(actsolv->sysarray_typ[stiff_array]),
                 &(actsolv->sysarray[stiff_array]),
                 &(work[0]),  /* \iinc\D_{n+1}^<i+1> */
                 &(actsolv->rhs[0]), init);

  /*--------------------------------------------------------------------*/
  /* blank prior to calculation/assignment of locally oriented
   * residual displacements of DBC values */
#ifdef LOCALSYSTEMS_ST
  solserv_zerodirich(actfield, disnum, node_array_sol_residual,
                     isolres->disres);
#endif

  /*--------------------------------------------------------------------*/
  /* return residual displacements iinc D_{n+1}^<i+1> to the nodes */
  solserv_result_resid(actfield, disnum, actintra,
                       &(work[0]),
                       isolres->disres,
                       &(actsolv->sysarray[stiff_array]),
                       &(actsolv->sysarray_typ[stiff_array]));

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement back into global system */
  /* post return residual displacements to nodes
   * This operation will not only rotate the free DOFs of relevant
   * Dirichlet nodes, but all (i.e. prescribed & free) its DOFs.
   * This should not matter, 'coz the residual displacements are zero
   * at prescribed nodes */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol_residual,
                          isolres->disres, locsys_trf_xyz_to_XYZ);
#endif

  /*====================================================================*/
  /* update */

  /*--------------------------------------------------------------------*/
  /* iterative update of internal variables of elements */
  *action = calc_struct_update_iterstep;
  container->dvec = NULL;
  container->dirich = NULL;
  container->global_numeq = 0;
  container->kstep = 0;
  calelm(actfield, actsolv, actpart, actintra,
         stiff_array, -1, container, action);

  /*--------------------------------------------------------------------*/
  /* update the incremental displacements by the residual/iterative
   * displacements
   *    \inc\D_{n+1}^<i+1> := \inc\D_{n+1}^<i> + \iinc\D_{n+1}^<i+1> */
  solserv_add_vec(&(work[0]), &(dispi[0]), 1.0);

  /*--------------------------------------------------------------------*/
  /* update displacements : sol[1] = sol[0] + dispi[0]
   *    \D_{n+1}^<i+1> := \D_{n} + \inc\D_{n+1}^<i+1> */
  solserv_copy_vec(&(actsolv->sol[0]), &(actsolv->sol[1]));
  solserv_add_vec(&dispi[0], &(actsolv->sol[1]), 1.0);

  /*--------------------------------------------------------------------*/
  /* return total displacements to the nodes */
  solserv_result_total(actfield, disnum, actintra,
                       &(actsolv->sol[1]),
                       isol->disn,
                       &(actsolv->sysarray[stiff_array]),
                       &(actsolv->sysarray_typ[stiff_array]));

  /*--------------------------------------------------------------------*/
  /* put the scaled prescribed displacements to the nodes
   * in field sol at place 0 together with free displacements */
  /* these are used to calculate the stiffness matrix */
  /* In case of rotated DBCs we have to reevaluate these, because we can
   * only rotate the complete nodal displacement vector
   * (BRICK1 & SOLID3: triplet, WALL1: duple, SHELL*: NOT IMPLEMENTED) at once.
   * However, the nodal displacement vector can be partly free and partly
   * supported. Here, we have to assure all displacements components of
   * a DBC-node are in the _local_ co-ordinate system (`system in sync').
   * This is due to: The free components of a DBC-node are stored in local
   * directions on the assembled quantities (stiffness & mass matrix,
   * internal and external force vectors, displacement vector (actsolv->sol),
   * etc.) */
#ifdef LOCALSYSTEMS_ST
  solserv_putdirich_to_dof(actfield, disnum, node_array_sol,
                           isol->disn, actdyn->time);
#endif

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement back into global system */
  /* post return total free and presc. DBC displacements to nodes,
   * displacements of DBC-less nodes are in global system, but
   * displacements of DBC-ish nodes are in local system (this holds for
   * free and prescribed/supported DOFs of the node) and have to be
   * rotated into the global system */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->disn,
                          locsys_trf_xyz_to_XYZ);
#endif

  /*--------------------------------------------------------------------*/
  /* blank prior to calculation/assignment of locally oriented
   * prescribed DBC values */
#ifdef LOCALSYSTEMS_ST
  solserv_zerodirich(actfield, disnum, node_array_sol_increment,
                     isolinc->disinc);
#endif

  /*--------------------------------------------------------------------*/
  /* return incremental displacements to the nodes */
  solserv_result_incre(actfield, disnum, actintra,
                       &(dispi[0]),
                       isolinc->disinc,
                       &(actsolv->sysarray[stiff_array]),
                       &(actsolv->sysarray_typ[stiff_array]));

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet displacement back into global system */
  /* post return incremental displacements to nodes
   * This operation will not only rotate the free DOFs of relevant
   * Dirichlet nodes, but all (i.e. prescribed & free) its DOFs.
   * This should not matter, 'coz the increments are zero at prescribed nodes */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol_increment,
                          isolinc->disinc, locsys_trf_xyz_to_XYZ);
#endif

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}  /* end of void tsi_st_genalp_equi() */

/*======================================================================*/
/*!
\brief Check convergence
\brief Update increment
\param  actintra    INTRA*          (i)    intra-communicator
\param  actdyn      STRUCT_DYNAMIC* (i)    structure dyn. control
\param  dynvar      STRUCT_DYN_CALC*(i)    dynamic variables
\param  dispi       DIST_VECTOR*    (i)    incremental displ. increment
\param  work        DIST_VECTOR*    (i)    iterative displ. increment
\param  mod_stdout  INT             (i)    print to STDOUT if 0
\param  converged   INT*            (o)    converged: 1=TRUE,0=FALSE
\author bborn
*/
void tsi_st_genalp_chkcnv(INTRA* actintra,
                          STRUCT_DYNAMIC* actdyn,
                          STRUCT_DYN_CALC* dynvar,
                          DIST_VECTOR* work,
                          DIST_VECTOR* dispi,
                          INT mod_stdout,
                          INT* converged)
{
  DOUBLE dmax;  /* infinity norm of residual displacements */

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("tsi_st_genalp_chkcnv");
#endif

  /*====================================================================*/
  /* CHECK CONVERGENCE */
  /* convergence = 0; */
  dmax = 0.0;
  solserv_vecnorm_euclid(actintra, &(work[0]), &(dynvar->dinorm));
  solserv_vecnorm_euclid(actintra, &(dispi[0]), &(dynvar->dnorm));
  solserv_vecnorm_Linf(actintra, &(work[0]), &(dmax));
  fflush(stdout);
  if ( (dynvar->dinorm < actdyn->toldisp)
       || (dynvar->dnorm < EPS14)
       || ( (dynvar->dinorm < EPS14) && (dmax < EPS12) ) )
  {
    if ( (par.myrank == 0) && (mod_stdout == 0) )
    {
      printf("                                                   "
             "Residual %10.5E -- Convergence reached\n", dynvar->dinorm);
    }
    *converged = 1;
  }
  else
  {
    if ( (par.myrank == 0) && (mod_stdout == 0) )
    {
      printf("                                                   "
             "Residual %10.5E\n", dynvar->dinorm);
    }
  }

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}  /* end void tsi_st_genalp_chkcnv() */


/*======================================================================*/
/*!
\brief Update increment
\param  actpart     PARTITION*      (io)   partition
\param  actintra    INTRA*          (i)    intra-communicator
\param  actfield    FIELD           (i)    thermal field
\param  disnum      INT             (i)    discretisation index
\param  isol        ARRAY_POSITION_SOL*(i) indices in NODE sol arrays
\param  isolinc     ARRAY_POSITION_SOLINC*(i) indices in NODE sol_increment a.
\param  actsolv     SOLVAR*         (io)   solution variables
\param  mass_array  INT             (i)    mass array index
\param  stiff_array INT             (i)    tangent array index
\param  actdyn      STRUCT_DYNAMIC* (i)    structure dyn. control
\param  dynvar      STRUCT_DYN_CALC*(i)    dynamic variables
\param  container   CONTAINER*      (i)    container
\param  dispi       DIST_VECTOR*    (io)   dispi vectors
\param  vel         DIST_VECTOR*    (io)   velocity vectors
\param  acc         DIST_VECTOR*    (io)   acceleration vectors
\param  fie         DIST_VECTOR*    (io)   internal force vectors
\param  work        DIST_VECTOR*    (io)   working vectors
\author bborn
\date 05/07
*/
void tsi_st_genalp_updincr(PARTITION* actpart,
                           INTRA* actintra,
                           FIELD* actfield,
                           INT disnum,
                           ARRAY_POSITION_SOL* isol,
                           ARRAY_POSITION_SOLINC* isolinc,
                           SOLVAR* actsolv,
                           INT mass_array,
                           INT stiff_array,
                           STRUCT_DYNAMIC* actdyn,
                           STRUCT_DYN_CALC* dynvar,
                           CONTAINER* container,
                           DIST_VECTOR* vel,
                           DIST_VECTOR* acc,
                           DIST_VECTOR* dispi,
                           DIST_VECTOR* work)
{
  const INT numsf = genprob.numsf;  /* index of structure field */
  CALC_ACTION* action = &(calc_action[numsf]);  /* structure cal_action */
  DOUBLE deltaepot = 0.0;  /* increment potential energy */

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("tsi_st_genalp_updincr");
#endif

  /*--------------------------------------------------------------------*/
  /* make temporary copy of actsolv->rhs[2] to actsolv->rhs[0] */
  /* (load at t_n) because in  dyn_nlnstructupd actsolv->rhs[2] is
   *  overwritten but is  still needed to compute energies */
  solserv_copy_vec(&(actsolv->rhs[2]), &(actsolv->rhs[0]));

  /*--------------------------------------------------------------------*/
  /*  copy disp from sol place 0 to place 10 */
  solserv_sol_copy(actfield, disnum,
                   node_array_sol, node_array_sol,
                   isol->disn, isol->dis);

  /*--------------------------------------------------------------------*/
  /* copy vels from sol place 1 to place 11 */
  solserv_sol_copy(actfield, disnum,
                   node_array_sol, node_array_sol,
                   isol->veln, isol->vel);

  /*--------------------------------------------------------------------*/
  /* copy accs from sol place 2 to place 12 */
  solserv_sol_copy(actfield, disnum,
                   node_array_sol, node_array_sol,
                   isol->accn, isol->acc);

  /*--------------------------------------------------------------------*/
  /* update displacements, velocities and accelerations */
  dyn_nlnstructupd(actfield,
                   disnum,
                   dynvar, actdyn, actsolv,
                   &(actsolv->sol[0]),  /* total displ. at time t_{n} */
                   &(actsolv->sol[1]),  /* total displ. at time t_{n+1} */
                   &(actsolv->rhs[1]),  /* load vector at time t_{n} */
                   &(actsolv->rhs[2]),  /* load vector at time t_{n+1} */
                   &(vel[0]),  /* velocities at time t_n */
                   &(acc[0]),  /* accelerations at time t_n */
                   &(work[0]),  /* working vector */
                   &(work[1]),  /* working vector */
                   &(work[2]));  /* working vector */

  /*--------------------------------------------------------------------*/
  solserv_adddirich(actfield, disnum, node_array_sol,
                    isol->veldn, isol->disn, isol->veln, 1.0, 0.0);

  /*--------------------------------------------------------------------*/
  /* return velocities to the nodes */
  solserv_result_total(actfield, disnum, actintra,
                       &(vel[0]), isol->veln,
                       &(actsolv->sysarray[stiff_array]),
                       &(actsolv->sysarray_typ[stiff_array]));

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet velocities back into global system */
  /* post return velocities to nodes,
   * This operation will not only rotate the prescribed DOFs of the relevant
   * Dirichlet node, but all (i.e. prescribed & free) its DOFs. */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->veln,
                          locsys_trf_xyz_to_XYZ);
#endif

  /*------------------------------------------------------------------*/
  /* accel. for prescribed dofs */
  solserv_adddirich(actfield, disnum,
                    node_array_sol, isol->accdn, isol->disn, isol->accn,
                    1.0, 0.0);

  /*--------------------------------------------------------------------*/
  /* return accelerations to the nodes */
  solserv_result_total(actfield, disnum, actintra,
                       &(acc[0]), isol->accn,
                       &(actsolv->sysarray[stiff_array]),
                       &(actsolv->sysarray_typ[stiff_array]));

  /*--------------------------------------------------------------------*/
  /* rotate Dirichlet accelerations back into global system */
  /* post return accelerations to nodes
   * This operation will not only rotate the prescribed DOFs of the relevant
   * Dirichlet node, but all (i.e. prescribed & free) its DOFs. */
#ifdef LOCALSYSTEMS_ST
  locsys_trans_sol_dirich(actfield, disnum, node_array_sol, isol->accn,
                          locsys_trf_xyz_to_XYZ);
#endif

  /*--------------------------------------------------------------------*/
  /* incremental update of element internal variables */
  *action = calc_struct_update_istep;
  container->dvec = NULL;
  container->dirich = NULL;
  container->global_numeq = 0;
  container->kstep = 0;
  calelm(actfield, actsolv, actpart, actintra,
         stiff_array, -1, container, action);

  /*--------------------------------------------------------------------*/
  /* It is a bit messed up, but anyway:
   * in the nodes the results are stored the following way:
   *
   * in ARRAY sol.a.da[place][0..numdf-1]:
   * place 0  holds total displacements  time t      (free/prescr)
   * place 1  holds velocities           time t      (free/prescr)
   * place 2  holds accels               time t      (free/prescr)
   * place 3  holds displacements        time t-dt   (prescr only)
   * place 4  holds displacements        time t      (prescr only)
   * place 5  holds place 4 - place 3
   * place 6  holds velocities           time t      (prescr only)
   * place 7  holds accels               time t      (prescr only)
   * place 8  is working space
   * place 9  holds contact forces       time t      (free only)
   * place 10 holds total displacements  time t-dt   (free/prescr)
   * place 11 holds velocities           time t-dt   (free/prescr)
   * place 12 holds accels               time t-dt   (free/prescr)
   *
   * in ARRAY sol_increment.a.da[place][0..numdf-1]
   * place 0 holds converged incremental displacements
   *         (without prescribed dofs)
   * place 1 holds converged internal forces at time t-dt
   * place 2 holds converged internal forces at time t
   *
   * in ARRAY sol_residual
   * place 0 holds residual displacements during iteration
   *         (without prescribed dofs) */

  /*--------------------------------------------------------------------*/
  /* make incremental potential energy at the nodes */
  dyn_epot(actfield, disnum, actintra, dynvar, &deltaepot);
  dynvar->epot += deltaepot;

  /*--------------------------------------------------------------------*/
  /* make kinetic energy at element level */
  dyn_ekin(actfield, actsolv, actpart, actintra, action, container,
           stiff_array, mass_array);
  dynvar->ekin = container->ekin;

  /*--------------------------------------------------------------------*/
  /* make external energy */
  dyn_eout(dynvar, actdyn, actintra, actsolv,
           &(dispi[0]), &(actsolv->rhs[1]),
           &(actsolv->rhs[0]), &(work[0]));

  /*--------------------------------------------------------------------*/
  /* make total energy */
  dynvar->etot = dynvar->epot + dynvar->ekin;

  /*--------------------------------------------------------------------*/
  /* update the internal forces in sol_increment */
  /* copy from sol_increment.a.da[2][i] to sol_increment.a.da[1][i] */
  solserv_sol_copy(actfield, disnum,
                   node_array_sol_increment, node_array_sol_increment,
                   isolinc->fintn, isolinc->fint);

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}  /* end void tsi_st_genalp_updincr() */


/*======================================================================*/
/*!
\brief Output
\param  actpart     PARTITION*      (io)   partition
\param  actintra    INTRA*          (i)    intra-communicator
\param  actfield    FIELD           (i)    thermal field
\param  disnum      INT             (i)    discretisation index
\param  isol        ARRAY_POSITION_SOL*(i) indices in NODE sol arrays
\param  actsolv     SOLVAR*         (io)   solution variables
\param  stiff_array INT             (i)    tangent array index
\param  actdyn      STRUCT_DYNAMIC* (i)    structure dyn. control
\param  dynvar      STRUCT_DYN_CALC*(i)    dynamic variables
\param  container   CONTAINER*      (i)    container
\param  out_context BIN_OUT_FIELD*  (io)   BINIO context
\param  dispi_num   INT             (i)    number of dispi vectors
\param  dispi       DIST_VECTOR*    (io)   dispi vectors
\param  vel_num     INT             (i)    number of vel vectors
\param  vel         DIST_VECTOR*    (io)   velocity vectors
\param  acc_num     INT             (i)    number of acc vectors
\param  acc         DIST_VECTOR*    (io)   acceleration vectors
\param  fie_num     INT             (i)    number of fie vectors
\param  fie         DIST_VECTOR*    (io)   internal force vectors
\param  work_num    INT             (i)    number of work vectors
\param  work        DIST_VECTOR*    (io)   working vectors
\param  intforce_a  ARRAY*          (io)   global int. force vector
\param  dirich_a    ARRAY*          (io)   Dirichlet RHS contribution
\author bborn
\date 05/07
*/
void tsi_st_genalp_out(PARTITION* actpart,
                       INTRA* actintra,
                       FIELD* actfield,
                       INT disnum,
                       ARRAY_POSITION_SOL* isol,
                       SOLVAR* actsolv,
                       INT stiff_array,
                       STRUCT_DYNAMIC* actdyn,
                       STRUCT_DYN_CALC* dynvar,
                       CONTAINER* container,
#ifdef BINIO
                       BIN_OUT_FIELD* out_context,
#endif
                       INT dispi_num,
                       DIST_VECTOR* dispi,
                       INT vel_num,
                       DIST_VECTOR* vel,
                       INT acc_num,
                       DIST_VECTOR* acc,
                       INT fie_num,
                       DIST_VECTOR* fie,
                       INT work_num,
                       DIST_VECTOR* work,
                       ARRAY* intforce_a,
                       ARRAY* dirich_a)
{
  const INT timeadapt = 0;  /* no time step adaptivity */
  INT mod_disp;  /* indicate whether to print displacements */
  INT mod_stress;  /* indicate whether to print stresses */
  INT mod_stdout;  /* indicates whether to write to STDOUT */
  INT mod_res_write;

  const INT numsf = genprob.numsf;  /* index of structure field */
  CALC_ACTION* action = &(calc_action[numsf]);  /* structure cal_action */

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("tsi_st_genalp_out");
#endif

  /*--------------------------------------------------------------------*/
  /* check whether to write results or not */
  mod_disp = actdyn->step % actdyn->updevry_disp;
  mod_stress = actdyn->step % actdyn->updevry_stress;
  /*--------------------------------------------------------------------*/
  /* check whether results are written to STDOUT or are not */
  mod_stdout = actdyn->step % actdyn->updevry_disp;

  /*--------------------------------------------------------------------*/
  /* check whether to write restart or not */
  if (actdyn->res_write_evry > 0)
  {
    /* if mod_res_write becomes 0, i.e. current time step sdyn->step
     * is a integer multiple of sdyn->res_write_evry, the restart
     * will be written */
    mod_res_write = actdyn->step % actdyn->res_write_evry;
  }
  else
  {
    /* prevent the attempt to write a restart file */
    mod_res_write = -1;
  }

  /*--------------------------------------------------------------------*/
  /* perform stress calculation */
  if ( (mod_stress == 0) || (mod_disp == 0) )
  {
    if (ioflags.struct_stress == 1)
    {
      *action = calc_struct_stress;
      container->dvec = NULL;
      container->dirich = NULL;
      container->global_numeq = 0;
      container->dirichfacs = NULL;
      container->kstep = 0;
      calelm(actfield, actsolv, actpart, actintra,
             stiff_array, -1, container, action);
      /* reduce stresses, so they can be written */
      *action = calc_struct_stressreduce;
      container->kstep = 0;
      calreduce(actfield, actpart, disnum, actintra, action, container);
    }
  }

  /*--------------------------------------------------------------------*/
  /* print out results to out */
  if ( (mod_stress == 0) || (mod_disp == 0) )
  {
    if ( (ioflags.struct_stress == 1)
         && (ioflags.struct_disp == 1)
         && (ioflags.output_out == 1) )
    {
      out_sol(actfield, actpart, disnum, actintra, actdyn->step, isol->disn);
    }
  }

  /*--------------------------------------------------------------------*/
  /* printout results to gid no time adaptivity */
  if ( (timeadapt == 0)
       && (par.myrank == 0)
       && (ioflags.output_gid == 1) )
  {
    if ( (mod_disp == 0) && (ioflags.struct_disp == 1) )
    {
      out_gid_soldyn("displacement", actfield, disnum, actintra,
                     actdyn->step, 0, actdyn->time);
      /*out_gid_soldyn("velocity",actfield,disnum,actintra,actdyn->step,1,actdyn->time);*/
      /*out_gid_soldyn("accelerations",actfield,disnum,actintra,actdyn->step,2,actdyn->time);*/
    }  /* end of if */
    if ( (mod_stress == 0) && (ioflags.struct_stress == 1) )
    {
      /* change hard-coded 0==place ??? */
      out_gid_soldyn("stress", actfield, disnum, actintra,
                     actdyn->step, 0, actdyn->time);
      out_gid_soldyn("strain", actfield, disnum, actintra,
                     actdyn->step, 0, actdyn->time);
    }
  }

  /*--------------------------------------------------------------------*/
  /* write restart data to pss file */
  if (mod_res_write == 0)
  {
#ifdef BINIO
    restart_write_bin_nlnstructdyn(out_context,
                                   actdyn, dynvar,
                                   actsolv->nrhs, actsolv->rhs,
                                   actsolv->nsol, actsolv->sol,
                                   dispi_num, dispi,
                                   vel_num, vel,
                                   acc_num, acc,
                                   fie_num, fie,
                                   work_num, work);
#else
    restart_write_nlnstructdyn(actdyn, dynvar,
                               actfield, actpart,
                               actintra, action,
                               actsolv->nrhs, actsolv->rhs,
                               actsolv->nsol, actsolv->sol,
                               dispi_num, dispi,
                               vel_num, vel,
                               acc_num, acc,
                               fie_num, fie,
                               work_num, work,
                               intforce_a,
                               dirich_a,
                               container);
#endif
  }

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}


/*======================================================================*/
/*!
\brief Finalise generalised-alpha for structural dynamics
\param  actsolv     SOLVAR*         (io)   solution variables
\param  out_context BIN_OUT_FIELD*  (io)   BINIO context
\param  dispi_num   INT             (i)    number of dispi vectors
\param  dispi       DIST_VECTOR**   (io)   dispi vectors
\param  vel_num     INT             (i)    number of vel vectors
\param  vel         DIST_VECTOR**   (io)   velocity vectors
\param  acc_num     INT             (i)    number of acc vectors
\param  acc         DIST_VECTOR**   (io)   acceleration vectors
\param  fie_num     INT             (i)    number of fie vectors
\param  fie         DIST_VECTOR**   (io)   internal force vectors
\param  work_num    INT             (i)    number of work vectors
\param  work        DIST_VECTOR**   (io)   working vectors
\param  intforce_a  ARRAY*          (io)   global int. force vector
\param  dirich_a    ARRAY*          (io)   Dirichlet RHS contribution
\author bborn
\date 05/07
*/
void tsi_st_genalp_final(SOLVAR* actsolv,
#ifdef BINIO
                         BIN_OUT_FIELD* out_context,
#endif
			 const INT dispi_num,
			 DIST_VECTOR** dispi,
			 const INT vel_num,
			 DIST_VECTOR** vel,
			 const INT acc_num,
			 DIST_VECTOR** acc,
			 const INT fie_num,
			 DIST_VECTOR** fie,
			 const INT work_num,
			 DIST_VECTOR** work,
			 ARRAY* intforce_a,
			 ARRAY* dirich_a)
{
  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_enter("tsi_st_genalp_final");
#endif

  /*--------------------------------------------------------------------*/
  /* cleaning up phase */
  solserv_del_vec(&(actsolv->rhs), actsolv->nrhs);
  solserv_del_vec(&(actsolv->sol), actsolv->nsol);
  solserv_del_vec(dispi, dispi_num);
  solserv_del_vec(vel, vel_num);
  solserv_del_vec(acc, acc_num);
  solserv_del_vec(fie, fie_num);
  solserv_del_vec(work, work_num);
  amdel(intforce_a);
  amdel(dirich_a);
  /* clean BINIO */
#ifdef BINIO
  destroy_bin_out_field(out_context);
#endif

  /*--------------------------------------------------------------------*/
#ifdef DEBUG
  dstrc_exit();
#endif
  return;
}  /* end void tsi_st_genalp_final() */

/*======================================================================*/
/*!
\brief Generalised-alpha time integration of structural field
       Modularised
\param   disnum_s     INT    (i)   index of structural discretisation
\param   disnum_t     INT    (i)   index of thermal discretisation
\author bborn
\date 03/06
*/
void tsi_st_genalp_sub(INT disnum_s,
                       INT disnum_t)
{
  INT numsf = genprob.numsf;  /* number (index) of structure field */

  PARTITION* actpart = &(partition[numsf]);  /* structure partition */
  INTRA* actintra;  /* active intra-communicator */

  /* field */
  FIELD* actfield = &(field[numsf]);  /* pointer to active field */

  /* discretisation */
  /* disnum_s is curr. discretisation index */
  /* structure discretisation index:
   * disnum_s == 0,
   * as only a single discretisation
   * this variable name is a little confusing
   * it sets an _actual_ number (better index?)
   * of one of the actfield->ndis discretisations.
   * Simply said: disnum != n(um)dis */
  /* named positions of NODE sol etc. arrays */
  ARRAY_POSITION* ipos = &(actfield->dis[disnum_s].ipos);
  /* named positions (indices) of NODE sol array */
  ARRAY_POSITION_SOL* isol = &(ipos->isol);
  /* named positions (indices) of NODE sol_increment array */
  ARRAY_POSITION_SOLINC* isolinc = &(ipos->isolinc);
  /* named positions (indices) of NODE sol_residual array */
  ARRAY_POSITION_SOLRES* isolres = &(ipos->isolres);

  /* solution variable */
  SOLVAR* actsolv = &(solv[numsf]);  /* active solution structure */
  INT numeq;  /* number of equations on this proc */
  INT numeq_total;  /* total number of equations */
  INT stiff_array;  /* index of the active system sparse matrix */
  INT mass_array;  /* index of the active system sparse matrix */
  INT damp_array;  /* index of the active system sparse matrix */

  /* dynamic control */
  STRUCT_DYNAMIC* actdyn = alldyn[numsf].sdyn;  /* structural dynamics */
  TSI_DYNAMIC* tsidyn = alldyn[genprob.numfld].tsidyn;  /* TSI dynamics */
  STRUCT_DYN_CALC dynvar;  /* variables for dynamic structural simulation */
  INT timeadapt = actdyn->timeadapt;  /* flag to switch time adaption on/off */

  /* output */
  INT mod_stdout = 0;  /* indicates whether to write to STDOUT */
  /* INT mod_res_write; */
#ifdef BINIO
  BIN_OUT_FIELD out_context;
#endif

  /* container */
  CONTAINER container;  /* transfers variables given
                         * in (this) solution technique
                         * to element level */

  /* iteration */
  INT converged;  /* convergence flag */
  INT itnum;  /* iterator */

  /* timing */
  DOUBLE t0, t1;  /* wall clock times for measuring */

/*   CALC_ACTION* action = &(calc_action[numsf]);  /\* action *\/ */

  const INT vel_num = 1;  /* number of veloc. vectors */
  DIST_VECTOR* vel;  /* total velocities */
  const INT acc_num = 1;  /* number of accel. vectors */
  DIST_VECTOR* acc;  /* total accelerations */
  const INT fie_num = 3;  /* number of force vectors */
  DIST_VECTOR* fie;  /* internal forces and working array */
  const INT dispi_num = 1;  /* number of increm. displ. vectors */
  DIST_VECTOR* dispi;  /* distributed vector to hold increm. displacments */
  const INT work_num = 3;  /* number of work vectors */
  DIST_VECTOR* work;  /* working vectors */

  ARRAY intforce_a;  /* redundant vect. of full length for internal forces */
  ARRAY dirich_a;  /* redund. vect. of full length for Dirichlet-part of RHS */

  /*====================================================================*/
  /* begin body */

#ifdef DEBUG
  dstrc_enter("tsi_st_genalp_sub");
#endif

  /*--------------------------------------------------------------------*/
  /* a word to the user */
  if (par.myrank == 0)
  {
    printf("============================================================="
           "=============\n");
    printf("TSI structural time integration with generalised-alpha\n");
    printf("(with beta=%g, gamma=%g, alpha_f=%g, alpha_m=%g)\n",
           actdyn->beta, actdyn->gamma, actdyn->alpha_f, actdyn->alpha_m);
    printf("-------------------------------------------------------------"
           "-------------\n");
  }

  /*--------------------------------------------------------------------*/
  /* set up container */
  container.fieldtyp = actfield->fieldtyp;  /* field type : structure */
  container.isdyn = 1;  /* dynamic calculation */
  container.kintyp = 2;  /* kintyp  = 2: total Lagrangean */  /* ??? */
  container.disnum = disnum_s;
  container.disnum_s = disnum_s;  /* structure discretisation index ( ==0 ) */
  container.disnum_t = disnum_t;  /* thermo-discretisation index ( ==0 ) */

  /*--------------------------------------------------------------------*/
  /* synchronise structural and thermal dynamic control */
  tsidyn->dt = actdyn->dt;
  tsidyn->out_std_ev = actdyn->updevry_disp;

  /*--------------------------------------------------------------------*/
  /* check time step adaptivity */
  if (timeadapt)
  {
    dserror("Time step size adaptivity is not available!");
  }

  /*--------------------------------------------------------------------*/
  /* intra communicator */
#ifdef PARALLEL
  actintra = &(par.intra[numsf]);
#else
  actintra = (INTRA*) CCACALLOC(1, sizeof(INTRA));
  if (!actintra)
  {
    dserror("Allocation of INTRA failed");
  }
  actintra->intra_fieldtyp = structure;
  actintra->intra_rank = 0;
  actintra->intra_nprocs = 1;
#endif
  /* there are only procs allowed in here, that belong to the structural
   * intracommunicator (in case of linear statics, this should be all) */
  if (actintra->intra_fieldtyp != structure)
  {
    goto end;
  }


  /*====================================================================*/
  /* initialise */
  tsi_st_genalp_init(actpart,
                     actintra,
                     actfield,
                     disnum_s,
                     ipos, isol, isolinc,
                     actsolv,
                     &(numeq_total), &(numeq),
                     &(stiff_array), &(mass_array), &(damp_array),
                     actdyn, &(dynvar),
                     &(container),
#ifdef BINIO
                     &(out_context),
#endif
                     vel_num, &(vel),
                     acc_num, &(acc),
                     fie_num, &(fie),
                     dispi_num, &(dispi),
                     work_num, &(work),
                     &(intforce_a),
                     &(dirich_a));
  /*--------------------------------------------------------------------*/
  /* set initial step and time */
  acttime = actdyn->time;  /* initial time */
  actdyn->step = -1;
  actdyn->time = 0.0;

  /*--------------------------------------------------------------------*/
  /* printout head */
  if (par.myrank == 0)
  {
    dyn_nlnstruct_outhead(&(dynvar), actdyn);
  }


  /*====================================================================*/
  /* START LOOP OVER ALL TIME STEPS */
  /*====================================================================*/
  /*
   * rhs[3]    original load vector
   * rhs[2]             load vector at time t_{n}
   * rhs[1]             load vector at time t_{n+1}
   * rhs[0]    interpolated load vector and working array
   *
   * fie[2]    internal forces at step t_{n+1}
   * fie[1]    internal forces at step t_{n}
   * fie[0]    interpolated internal forces and working array
   *
   * dispi[0]  displacement increment \inc\D_{n+1} from t_{n} to t_{n+1}
   *
   * sol[0]    total displacements \D_{n} at time t_{n+1}
   * sol[1]    total displacements \D_{n} at time t_n
   *
   * vel[0]    velocities \V_{n} at t_{n}
   * acc[0]    accelerations \V_{n} at t_{n}
   *
   * work[2]   working vector for sums and matrix-vector products
   * work[1]   working vector for sums and matrix-vector products
   * work[0]   working vector for sums and matrix-vector products
   * work[0]   is used to hold residual displacements in corrector
   *           iteration
   *
   * in the nodes, displacements are kept in node[].sol[0][0..numdf-1]
   *               velocities    are kept in node[].sol[1][0..numdf-1]
   *               accelerations are kept in node[].sol[2][0..numdf-1]
   *
   * Values of the different vectors from above in one loop:
   *    /   ...   no change in this step
   *    =   ...   evaluation in this step
   *    +=  ...   evaluation in this step
   *
   */
  while ( (actdyn->step < actdyn->nstep-1)
          && (actdyn->time <= actdyn->maxtime) )
  {
    /*------------------------------------------------------------------*/
    /* wall clock time in the beginning of current time step*/
    t0 = ds_cputime();

    /*------------------------------------------------------------------*/
    /* increment step */
    actdyn->step++;
    tsidyn->step = actdyn->step;

    /*------------------------------------------------------------------*/
    /* set new time t_{n+1} */
    actdyn->time += actdyn->dt;
    /* put time to global variable for time-dependent load distributions */
    acttime = actdyn->time;


    /*==================================================================*/
    /* PREDICTOR */
    /*==================================================================*/
    tsi_st_genalp_pred(actpart,
                       actintra,
                       actfield,
                       disnum_s,
                       isol, isolinc, isolres,
                       actsolv,
                       numeq_total,
                       stiff_array, mass_array, damp_array,
                       actdyn,
                       &(dynvar),
                       &(container),
                       vel,
                       acc,
                       fie,
                       dispi,
                       work,
                       &(dirich_a),
                       &(intforce_a));
    /*==================================================================*/
    /* convergence check */
    converged = 0;
    tsi_st_genalp_chkcnv(actintra,
                         actdyn,
                         &(dynvar),
                         dispi,
                         dispi,
                         mod_stdout,
                         &(converged));

    /*==================================================================*/
    /* PERFORM EQUILIBRIUM ITERATION */
    /*==================================================================*/
    itnum = 0;
    while ( (converged != 1) && (itnum <= actdyn->maxiter) )
    {

      /*----------------------------------------------------------------*/
      /* check if maximally permitted iterations reached */
      if ( (itnum == actdyn->maxiter) && !timeadapt )
      {
        dserror("No convergence in maxiter steps");
      }

      /*================================================================*/
      /* make the equilibrium iteration */
      tsi_st_genalp_equi(actpart,
                         actintra,
                         actfield,
                         disnum_s,
                         isol, isolinc, isolres,
                         actsolv,
                         numeq_total,
                         stiff_array, mass_array, damp_array,
                         actdyn, &(dynvar),
                         &(container),
                         vel,
                         acc,
                         fie,
                         dispi,
                         work,
                         &(intforce_a),
                         &(dirich_a));

      /*================================================================*/
      /* convergence check */
      tsi_st_genalp_chkcnv(actintra,
                           actdyn,
                           &(dynvar),
                           work,
                           dispi,
                           mod_stdout,
                           &(converged));

      /*----------------------------------------------------------------*/
      /* increase iteration counter */
      ++itnum;

    }  /* end of equilibrium iteration */
    /*==================================================================*/
    /* END OF EQUILIBRIUM ITERATION */
    /*==================================================================*/


    /*==================================================================*/
    /* incremental update */
    tsi_st_genalp_updincr(actpart,
                          actintra,
                          actfield,
                          disnum_s,
                          isol,
                          isolinc,
                          actsolv,
                          mass_array,
                          stiff_array,
                          actdyn,
                          &(dynvar),
                          &(container),
                          vel,
                          acc,
                          dispi,
                          work);

    /*==================================================================*/
    /* output */
    tsi_st_genalp_out(actpart,
                      actintra,
                      actfield,
                      disnum_s,
                      isol,
                      actsolv,
                      stiff_array,
                      actdyn,
                      &(dynvar),
                      &(container),
#ifdef BINIO
                      &(out_context),
#endif
                      dispi_num,
                      dispi,
                      vel_num,
                      vel,
                      acc_num,
                      acc,
                      fie_num,
                      fie,
                      work_num,
                      work,
                      &(intforce_a),
                      &(dirich_a));

  /*--------------------------------------------------------------------*/
  /* print time step */
  if ( (par.myrank==0) && !timeadapt && (mod_stdout==0) )
  {
    dyn_nlnstruct_outstep(&(dynvar), actdyn, itnum, actdyn->dt);
  }

  /*--------------------------------------------------------------------*/
  /*  measure wall clock time for this step */
  t1 = ds_cputime();
  fprintf(allfiles.out_err, "TIME for step %d is %f sec\n",
          actdyn->step, t1-t0);

  }  /* end of time loop */
  /*====================================================================*/
  /* END OF TIME STEP LOOP */
  /*====================================================================*/

  /*--------------------------------------------------------------------*/
  end:

  /*====================================================================*/
  /* deallocate stuff */
  tsi_st_genalp_final(actsolv,
#ifdef BINIO
                      &(out_context),
#endif
                      dispi_num, &(dispi),
                      vel_num, &(vel),
                      acc_num, &(acc),
                      fie_num, &(fie),
                      work_num, &(work),
                      &(intforce_a),
                      &(dirich_a));


  /* clean PARALLEL */
#ifndef PARALLEL
  CCAFREE(actintra);
#endif

  /*--------------------------------------------------------------------*/
  /* a last word to the nervously waiting user */
  if (par.myrank == 0)
  {
    printf("-------------------------------------------------------------"
           "-----------\n");
    printf("TSI structural time integration generalised-alpha finished.\n");
    printf("============================================================="
           "===========\n");
  }

#ifdef DEBUG
  dstrc_exit();
#endif

  return;
}  /* end of tsi_st_genalp */
#endif

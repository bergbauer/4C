#include "../headers/standardtypes.h"
#include "../headers/solution.h"
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | vector of numfld FIELDs, defined in global_control.c                 |
 *----------------------------------------------------------------------*/
extern struct _FIELD      *field;
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | structure allfiles, which holds all file pointers                    |
 | is defined in input_control_global.c
 *----------------------------------------------------------------------*/
extern struct _FILES  allfiles;
/*----------------------------------------------------------------------*
 | global variable *solv, vector of lenght numfld of structures SOLVAR  |
 | defined in solver_control.c                                          |
 |                                                                      |
 |                                                       m.gee 11/00    |
 *----------------------------------------------------------------------*/
extern struct _SOLVAR  *solv;
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | global variable *partition, vector of lenght numfld of structures    |
 | PARTITION is defined in global_control.c                             |
 *----------------------------------------------------------------------*/
extern struct _PARTITION  *partition;
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | pointer to allocate static variables if needed                       |
 | defined in global_control.c                                          |
 *----------------------------------------------------------------------*/
extern struct _STATIC_VAR  *statvar;
/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | structure of flags to control output                                 |
 | defined in out_global.c                                              |
 *----------------------------------------------------------------------*/
extern struct _IO_FLAGS     ioflags;

/*----------------------------------------------------------------------*
 |  routine to control nonlinear static execution       m.gee 11/01     |
 *----------------------------------------------------------------------*/
void stanln() 
{
int           i;                  /* simply a counter */
int           numeq;              /* number of equations on this proc */
int           numeq_total;        /* total number of equations */
int           init;               /* flag for solver_control call */
int           actsysarray;        /* number of the active system sparse matrix */
int           cdof;               /* the Id of the controlled dof */

int           kstep;              /* active step in nonlinear analysis */
int           nstep;              /* total number of steps */
int           itnum;              /* number of iterations taken by corrector */
int           maxiter;            /* max. number of iterations allowed */

NR_CONTROLTYP controltyp;         /* type of path following technic */

DIST_VECTOR  *re;                 /* vector of out of balance loads */
DIST_VECTOR  *rsd;                /* vector for incremental displacements */
DIST_VECTOR  *dispi;              /* incrementel displacements */              


SOLVAR       *actsolv;            /* pointer to active solution structure */
PARTITION    *actpart;            /* pointer to active partition */
FIELD        *actfield;           /* pointer to active field */
INTRA        *actintra;           /* pointer to active intra-communicator */

SPARSE_TYP    array_typ;          /* type of sparse matrix */

STANLN        nln_data;           /* structure to store and pass data for stanln */

#ifdef DEBUG 
dstrc_enter("stanln");
#endif
/*----------------------------------------------------------------------*/
/*------------ the distributed system matrix, which is used for solving */
/* 
   NOTE: This routine only uses 1 global sparse matrix, which was created
         in global_mask_matrices. If You need more, it is necessary to allocate
         more matrices in actsolv->sysarray.
         Then one has to either copy the sparisty mask from 
         actsolv->actsysarray[0] to the others if using the same matrix format,
         or one has to call global mask matrices again for another type of 
         sparsity mask.
         Normally a control routine should not mix up different storage formats
         for system matrices
*/         
actsysarray=0;
/*--------------------------------------------------- set some pointers */
actfield    = &(field[0]);
actsolv     = &(solv[0]);
actpart     = &(partition[0]);
#ifdef PARALLEL 
actintra    = &(par.intra[0]);
/* if we are not parallel, we have to allocate an alibi intra-communicator structure */
#else
actintra    = (INTRA*)calloc(1,sizeof(INTRA));
if (!actintra) dserror("Allocation of INTRA failed");
actintra->intra_fieldtyp = structure;
actintra->intra_rank     = 0;
actintra->intra_nprocs   = 1;
#endif
/*- there are only procs allowed in here, that belong to the structural */
/* intracommunicator (in case of nonlinear statics, this should be all) */
if (actintra->intra_fieldtyp != structure) goto end;
/*================================ start with preparing the calculation */
/*------------------------------------------------ typ of global matrix */
array_typ   = actsolv->sysarray_typ[actsysarray];
/*---------------------------- get global and local number of equations */
switch(array_typ)
{
case msr:/*--------------------------------- system array is msr matrix */
   numeq       = actsolv->sysarray[actsysarray].msr->numeq;
   numeq_total = actsolv->sysarray[actsysarray].msr->numeq_total;
break;
case parcsr:/*--------------------------- system array is parcsr matrix */
   numeq       = actsolv->sysarray[actsysarray].parcsr->numeq;
   numeq_total = actsolv->sysarray[actsysarray].parcsr->numeq_total;
break;
case ucchb:/*----------------------------- system array is ucchb matrix */
   numeq       = actsolv->sysarray[actsysarray].ucchb->numeq;
   numeq_total = actsolv->sysarray[actsysarray].ucchb->numeq_total;
break;
case dense:/*----------------------------- system array is dense matrix */
   numeq       = actsolv->sysarray[actsysarray].dense->numeq;
   numeq_total = actsolv->sysarray[actsysarray].dense->numeq_total;
break;
case rc_ptr:/*----------------------- system array is row/column matrix */
   numeq       = actsolv->sysarray[actsysarray].rc_ptr->numeq;
   numeq_total = actsolv->sysarray[actsysarray].rc_ptr->numeq_total;
break;
default:
   dserror("unknown type of global matrix");
break;
}
/*--------------------------------------------------- find control node */
calstatserv_findcontroldof(actfield,
                           statvar->control_node_global,
                           statvar->control_dof,
                           &(statvar->controlnode),
                           &cdof); 
/*------------------------------------------------- get type of control */
controltyp = statvar->nr_controltyp;
/*------------------------------------------------- get number of steps */
nstep      = statvar->nstep; 
/*------------------------------------ get maximum number of iterations */
maxiter    = statvar->maxiter;
/*----------------------------------------------- number of rhs vectors */
/*------- the iteration uses rhs[0] for calculations and rhs[1] to hold */
/*----------------------------------------allocate 2 dist. load vectors */
actsolv->nrhs = 2;
solserv_create_vec(&(actsolv->rhs),actsolv->nrhs,numeq_total,numeq,"DV");
for (i=0; i<actsolv->nrhs; i++) solserv_zero_vec(&(actsolv->rhs[i]));
/*------------------------------------------ number of solution vectors */
/*-------------------- there is one solution vector to hold total displ.*/
actsolv->nsol= 1;
solserv_create_vec(&(actsolv->sol),actsolv->nsol,numeq_total,numeq,"DV");
for (i=0; i<actsolv->nsol; i++) solserv_zero_vec(&(actsolv->sol[i]));
/*------------------------ create vector re[0] for out of balance loads */
/*------------------ re[0] is used to hold residual forces in iteration */
solserv_create_vec(&(re),1,numeq_total,numeq,"DV");
solserv_zero_vec(&(re[0]));
/*--------------------- create 3 vectors rsd for residual displacements */
/*------------ the iteration uses rsd[0] to hold actual residual displ. */
/*--------------------- rsd[1] and rsd[2] are additional working arrays */
solserv_create_vec(&(rsd),3,numeq_total,numeq,"DV");
for (i=0; i<3; i++) solserv_zero_vec(&(rsd[i]));
/*------------------- create vector dispi for incremental displacements */
/*------------------ dispi[0] holds converged incremental displacements */
solserv_create_vec(&(dispi),1,numeq_total,numeq,"DV");
solserv_zero_vec(&(dispi[0]));
/*--------------------------------------------------- initialize solver */
init=1;
solver_control(  
                    actsolv,
                    actintra,
                  &(actsolv->sysarray_typ[actsysarray]),
                  &(actsolv->sysarray[actsysarray]),
                  &(actsolv->sol[actsysarray]),
                  &(actsolv->rhs[actsysarray]),
                    init
              );
/*----------------------------- init the assembly for ONE sparse matrix */
init_assembly(actpart,actsolv,actintra,actfield,actsysarray);
/*------------------------------- init the element calculating routines */
calinit(actfield,actpart);
/*-------------------------------------- create the original rhs vector */
calrhs(
          actfield,
          actsolv,
          actpart,
          actintra,
          actsysarray,
          &(actsolv->rhs[actsysarray]),
          &(actsolv->rhs[actsysarray+1]),
          0,
          6
      );
/*--------------------------------------------- add the two rhs vectors */
solserv_add_vec(&(actsolv->rhs[actsysarray+1]),&(actsolv->rhs[actsysarray]));
solserv_copy_vec(&(actsolv->rhs[actsysarray]),&(actsolv->rhs[actsysarray+1]));
/*----------------------------------------------------------------------*/
/*          The original rhs vector is now on actsolv->rhs[actsysarray] */
/*          AND in actsolv->rhs[actsysarray+1]                          */
/* the original load vector is held on [actsysarray+1]  all the time    */
/*----------------------------------------------------------------------*/
/*------------------ calculate euclidian vector norm of external forces */
solserv_vecnorm_euclid(actintra,&(actsolv->rhs[actsysarray+1]),&(nln_data.rinorm));
/*----------------------------------------------------------------------*/
nln_data.sp1   = 0.0;
nln_data.csp   = 0.0;
nln_data.rlold = 0.0;
nln_data.rlnew = 0.0;
nln_data.rlpre = 0.0;
amdef("arcfac",&(nln_data.arcfac),nstep,1,"DV");
amzero(&(nln_data.arcfac));
/*----------------------------------------- output to GID postprozessor */
if (par.myrank==0) 
{
   out_gid_domains(actfield);
}
/*----------------------------------------------------------------------*/
/*                     START LOOP OVER ALL STEPS                        */
/*----------------------------------------------------------------------*/
for (kstep=0; kstep<nstep; kstep++)
{
   /*--------------------------------------------------- make predictor */
   /*dstrace_to_err();*/
   conpre(
           actfield,
           actsolv,
           actpart,
           actintra,
           kstep,
           actsysarray,
           rsd,
           dispi,
           cdof,
          &nln_data,
           controltyp
         );
   /*-------------------------------------- make equillibrium iteration */  
   /*dstrace_to_err();*/
   conequ(
           actfield,
           actsolv,
           actpart,
           actintra,
           kstep,
          &itnum,
           actsysarray,
           rsd,
           dispi,
           re,
           cdof,
          &nln_data,
           controltyp
         );    
   /*dstrace_to_err();*/
    /*-------------------------------------- perform stress calculation */
    if (ioflags.struct_stress_file==1 || ioflags.struct_stress_gid==1)
    {
       calelm(actfield,actsolv,actpart,actintra,actsysarray,-1,NULL,0,kstep,5);
       /*---------------------- reduce stresses, so they can be written */
       calreduce(actfield,actpart,actintra,kstep);
    }
    /*---------------------------------------- print out results to out */
    out_sol(actfield,actpart,actintra,kstep);
    /*----------------------------------------- printout results to gid */
    if (par.myrank==0) 
    {
       if (ioflags.struct_disp_gid==1)
       out_gid_sol("displacement",actfield,actintra,kstep);
       if (ioflags.struct_stress_gid==1)
       out_gid_sol("stress"      ,actfield,actintra,kstep);
    }
} /* end of (kstep=0; kstep<nstep; kstep++) */
/*----------------------------------------------------------------------*/
end:
#ifndef PARALLEL 
free(actintra);
#endif
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of stanln */


/*----------------------------------------------------------------------*
 |  PERFORM LINEAR PREDICTOR STEP                            m.gee 11/01|
 *----------------------------------------------------------------------*/
void conpre(
            FIELD         *actfield,     /* the actual physical field */
            SOLVAR        *actsolv,      /* the field-corresponding solver */
            PARTITION     *actpart,      /* the partition of the proc */
            INTRA         *actintra,     /* the intra-communicator of this field */
            int            kstep,        /* the load or time step we are in */
            int            actsysarray,  /* number of the system matrix in actsolv->sysarray[actsysarray] to be used */
            DIST_VECTOR   *rsd,          /* dist. vector of incremental residual forces used for iteration in conequ */
            DIST_VECTOR   *dispi,        /* dist. vector of incremental displacements */
            int            cdof,         /* number of the dof to be controlled */
            STANLN        *nln_data,     /* data of the Newton-Raphson method */
            NR_CONTROLTYP  controltyp    /* type of control algorithm */
          )
{
int                  i;                /* a counter */
int                  init;             /* solver flag */
double               prenorm;          /* norm of predictor step */
double               controldisp;      /* displacment value at controled dof */ 
double               rldiff;           /* forgot.... */
double               spi;              /* forgot.... */

#ifdef DEBUG 
dstrc_enter("conpre");
#endif
/*----------------------------------------------------------------------*/
/*--------------------------------- init the dist sparse matrix to zero */
/*                  NOTE: Has to be called after solver_control(init=1) */
solserv_zero_mat(
                 actintra,
                 &(actsolv->sysarray[actsysarray]),
                 &(actsolv->sysarray_typ[actsysarray])
                );
/*----------------------- calculate tangential stiffness in actsysarray */
calelm(actfield,actsolv,actpart,actintra,actsysarray,-1,NULL,0,kstep,2);
/*----- copy original load vector from [actsysarray+1] to [actsysarray] */
solserv_copy_vec(&(actsolv->rhs[actsysarray+1]),&(actsolv->rhs[actsysarray]));
/*------------------------------------------ solve for incremental load */
init=0;
solver_control(  
                 actsolv,
                 actintra,
               &(actsolv->sysarray_typ[actsysarray]),
               &(actsolv->sysarray[actsysarray]),
               &(rsd[0]),
               &(actsolv->rhs[actsysarray]),
                 init
              );
/*--------------------------- calculate euclidian norm of displacements */
/*solserv_vecnorm_euclid(actintra,&(rsd[0]),&prenorm);*/
/*--------------------------------------- do scaling for load parameter */
switch(controltyp)
{
case control_disp:
   solserv_getele_vec( actintra,
                      &(actsolv->sysarray_typ[actsysarray]),
                      &(actsolv->sysarray[actsysarray]),
                      &(rsd[0]),
                       cdof,
                      &controldisp);
   rldiff = (statvar->stepsize)/controldisp;
break;
case control_none:
   dserror("Unknown typ of path following control");
break;
default:
   dserror("Unknown typ of path following control");
break;
}
/*---------------------------------- create current stiffness parameter */
solserv_dot_vec(actintra,&(actsolv->rhs[actsysarray]),&(rsd[0]),&spi);
if (ABS(nln_data->sp1) <= EPS14) nln_data->sp1 = spi;
if (ABS(spi)           <= EPS14) nln_data->csp = 1.0;
else                             nln_data->csp = nln_data->sp1 / spi;
/*--------------------------------------------------------- save values */
nln_data->rlnew = nln_data->rlold + rldiff;
/*---------- create the correct displacmements after predictor solution */
/* displacements of increment are stored in dispi[0]                    */
/* displacements of iteration are in rsd[0]                             */
solserv_scalarprod_vec(&(rsd[0]),rldiff);
solserv_copy_vec(&(rsd[0]),&(dispi[0]));
/*---------------------------------------- make new norm of load vector */
nln_data->rrnorm = nln_data->rinorm * nln_data->rlnew;

/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of conpre */


/*----------------------------------------------------------------------*
 |  PERFORM EQUILLIBRIUM ITERATION                           m.gee 11/01|
 |  within Newton Raphson                                               |
 *----------------------------------------------------------------------*/
void conequ(
            FIELD         *actfield,      /* the actual physical field */
            SOLVAR        *actsolv,       /* the field-corresponding solver */
            PARTITION     *actpart,       /* the partition of the proc */
            INTRA         *actintra,      /* the intra-communicator of this field */
            int            kstep,         /* the load or time step we are in */
            int           *itnum,         /* number of corrector steps taken by this routine */
            int            actsysarray,   /* number of the system matrix in actsolv->sysarray[actsysarray] to be used */
            DIST_VECTOR   *rsd,           /* dist. vector of incremental residual forces used for iteration in conequ */
            DIST_VECTOR   *dispi,         /* dist. vector of incremental displacements */
            DIST_VECTOR   *re,            /* re[0..2] 3 vectors for residual displacements */
            int            cdof,          /* number of dof to be controlled */
            STANLN        *nln_data,      /* data of the Newton-Raphson method */
            NR_CONTROLTYP  controltyp     /* type of control algorithm */
          )
{
int                  i;                      /* counter variable */
int                  init;                   /* init flag for solver */
int                  itemax;                 /* max. number of corrector steps allowed */
double               stepsize;               /* stepsize */
int                  convergence=0;          /* test flag for convergence */
double               rl0;                    /* some norms and working variables */                   
double               rlold;
double               rlnew;
double               rlpre;
double               renorm=0.0;
double               energy=0.0;
double               dnorm;
double               dinorm;
double               told;
double               disval;
double               rli;
double               rsd1;
double               rsd2;

ARRAY                intforce_a;             /* global redundant vector of internal forces */
double              *intforce;               /* pointer to intforce_a.a.dv */

#ifdef DEBUG 
dstrc_enter("conequ");
#endif
/*----------------------------------------------------------------------*/
*itnum=0;
/*----------------------------------- get load factor of last increment */
if (kstep != 0) rl0 = nln_data->arcfac.a.dv[kstep-1];
else            rl0 = 0.0;
nln_data->rlold     = rl0;
rlold               = nln_data->rlold;
rlnew               = nln_data->rlnew;
rlpre               = nln_data->rlpre;
stepsize            = statvar->stepsize;
itemax              = statvar->maxiter;
/*---------------------------- update total displacements on sol[0] */
solserv_add_vec(&(dispi[0]),&(actsolv->sol[0]));
/*------- put actsolv->sol[kstep] to the elements (total displacements) */ 
solserv_result_total(
                     actfield,
                     actintra,
                     &(actsolv->sol[0]),
                     0,
                     &(actsolv->sysarray[actsysarray]),
                     &(actsolv->sysarray_typ[actsysarray])
                    );
/*----------- printf out iteration heading to err and to shell */
if (actintra->intra_rank==0) conequ_printhead(kstep,controltyp,cdof);
/*--------------------- make iteration parameter before first iteration */
/*----------------------------------- norm of incremental displacements */
solserv_vecnorm_euclid(actintra,&(dispi[0]),&dnorm);
/*-------------------------------------- norm of residual displacements */
solserv_vecnorm_euclid(actintra,&(rsd[0]),&dinorm);
/*------------------------------------- total disp value of control dof */ 
solserv_getele_vec(actintra,
                   &(actsolv->sysarray_typ[actsysarray]),
                   &(actsolv->sysarray[actsysarray]),
                   &(actsolv->sol[0]),
                   cdof,
                   &disval);
/*------------------------------------------- print out first iteration */                   
if (actintra->intra_rank==0)
conequ_printiter(*itnum,disval,rlnew,dinorm,renorm,energy,dnorm,nln_data->rrnorm);
/*----------------------------------------------------------------------*/
/*----------------------------- create an array for the internal forces */
intforce = amdef("intforce",&intforce_a,actsolv->sol[0].numeq_total,1,"DV");
/*======================================================================*/
/*                        start of the iteration loop of this increment */
/*======================================================================*/
while (*itnum < itemax)
{
/*---------------------------- copy the initial rhs to rhs[actsysarray] */
solserv_copy_vec(&(actsolv->rhs[actsysarray+1]),&(actsolv->rhs[actsysarray]));
/*------------------------------- scale the rhs[actsysarray] with rlnew */
solserv_scalarprod_vec(&(actsolv->rhs[actsysarray]),rlnew);
/*--------------------------------------- copy the initial rhs to re[0] */
solserv_copy_vec(&(actsolv->rhs[actsysarray+1]),&(re[0]));
/*------------------------------------------ scale the re[0] with rlnew */
solserv_scalarprod_vec(&(re[0]),rlnew);
/* put residual displacements to the nodes (needed for material, eas ..)*/              
solserv_result_resid(
                     actfield,
                     actintra,
                     &(rsd[0]),
                     0,
                     &(actsolv->sysarray[actsysarray]),
                     &(actsolv->sysarray_typ[actsysarray])
                    );
/*-- put incremental displacements to the nodes (needed for material...)*/              
solserv_result_incre(
                     actfield,
                     actintra,
                     &(dispi[0]),
                     0,
                     &(actsolv->sysarray[actsysarray]),
                     &(actsolv->sysarray_typ[actsysarray])
                    );
/*---------------------------------------------initialize system matrix */
solserv_zero_mat(
                 actintra,
                 &(actsolv->sysarray[actsysarray]),
                 &(actsolv->sysarray_typ[actsysarray])
                );
/*------------------------------- initialize vector for internal forces */
amzero(&intforce_a);
/*------------------------- calculate new stiffness and internal forces */
calelm(actfield,
       actsolv,
       actpart,
       actintra,
       actsysarray,
       -1,
       intforce,
       actsolv->sol[0].numeq_total,
       kstep,
       2);
/* add internal forces to scaled external forces to get residual forces */
assemble_vec(actintra,
             &(actsolv->sysarray_typ[actsysarray]),
             &(actsolv->sysarray[actsysarray]),
             &(re[0]),
             intforce,
             -1.0
             );
/*-------------- solve for out-of balance loads, put solution to rsd[2] */
/*                                                         K * du2 = -R */
/*                                                initial guess is zero */
init=0;
solserv_zero_vec(&(rsd[2]));
solver_control(  
                 actsolv,
                 actintra,
               &(actsolv->sysarray_typ[actsysarray]),
               &(actsolv->sysarray[actsysarray]),
               &(rsd[2]),
               &(re[0]),
                 init
              );
/*-------------------------------------- solve for original load vector */
/*                                                          K * du1 = P */
/*                                initial guess is values of last solve */
init=0;
solver_control(  
                 actsolv,
                 actintra,
               &(actsolv->sysarray_typ[actsysarray]),
               &(actsolv->sysarray[actsysarray]),
               &(rsd[1]),
               &(actsolv->rhs[actsysarray+1]),
                 init
              );
/*------------------------------------------ get values of control node */
solserv_getele_vec(actintra,
                   &(actsolv->sysarray_typ[actsysarray]),
                   &(actsolv->sysarray[actsysarray]),
                   &(rsd[2]),
                   cdof,
                   &rsd2);
solserv_getele_vec(actintra,
                   &(actsolv->sysarray_typ[actsysarray]),
                   &(actsolv->sysarray[actsysarray]),
                   &(rsd[1]),
                   cdof,
                   &rsd1);
/*===============================make increment of load and displacment */
switch(controltyp)
{
case control_disp:/*===============================displacement control */
   rli = -rsd2/rsd1;
   /*-------------------------------- make rsd[0] = rsd[1]*rli + rsd[2] */
   /*------------------- make dispi[0] = dispi[0] + rsd[1]*rli + rsd[2] */
   solserv_copy_vec(&(rsd[1]),&(rsd[0]));
   solserv_scalarprod_vec(&(rsd[0]),rli);
   solserv_add_vec(&(rsd[0]),&(dispi[0]));
   solserv_add_vec(&(rsd[2]),&(rsd[0]));
   solserv_add_vec(&(rsd[2]),&(dispi[0]));
break;
case control_arc:
   dserror("arclenght control not yet impl.");
break;
case control_load:
   dserror("load control not yet impl.");
break;
case control_none:
   dserror("Unknown typ of path following technique");
break;
default:
   dserror("Unknown typ of path following technique");
break;
}
/*----------------------------------------- update of load factor rlnew */
rlnew += rli;
nln_data->rlnew = rlnew;
/*------------------------------------ update of load and displacements */
solserv_add_vec(&(rsd[0]),&(actsolv->sol[0]));
/*----------------------------- put actual total displacements to nodes */
solserv_result_total(
                     actfield,
                     actintra,
                     &(actsolv->sol[0]),
                     0,
                     &(actsolv->sysarray[actsysarray]),
                     &(actsolv->sysarray_typ[actsysarray])
                    );
/*------------------------------------------- calculate residual energy */
solserv_dot_vec(actintra,&(rsd[0]),&(re[0]),&energy);
nln_data->renergy = energy;
/*------------------------------ calculate norm of out-of-balance-loads */
solserv_vecnorm_euclid(actintra,&(re[0]),&renorm);
nln_data->renorm = renorm;
/*-------------------------------------------------------- update norms */
nln_data->rrnorm = nln_data->rinorm * rlnew;
/*------------------------------------- calculate norm of displacements */
solserv_vecnorm_euclid(actintra,&(dispi[0]),&dnorm);
/*----------------- calculate norm of updated incremental displacements */
solserv_vecnorm_euclid(actintra,&(rsd[0]),&dinorm);
/*----------------------------------------------- check for convergence */
told=0.0;
if (dinorm > statvar->toldisp) told = dinorm/dnorm;
if (told   > statvar->toldisp) convergence=0;
else                        convergence=1;

/*------------------------------------------------------- make printout */
solserv_getele_vec(actintra,
                   &(actsolv->sysarray_typ[actsysarray]),
                   &(actsolv->sysarray[actsysarray]),
                   &(actsolv->sol[0]),
                   cdof,
                   &disval);
if (actintra->intra_rank==0)
conequ_printiter(*itnum,disval,rlnew,dinorm,renorm,energy,dnorm,nln_data->rrnorm);
/*----------------------------- decide for or against another iteration */
if (convergence)/*------------------------------------------convergence */
{
   nln_data->arcfac.a.dv[kstep] = rlnew;
   nln_data->rlnew              = rlnew;
   break;
}
else/*---------------------------------------------------no convergence */
{
   if (*itnum < itemax) *itnum = *itnum + 1;
   else 
   {
      if (actintra->intra_rank==0)
      {
         printf("WARNING: No convergence in global NR in maxiter steps! continue....\n");
         fprintf(allfiles.out_err,"WARNING: No convergence in global NR in maxiter steps! continue....\n");
      }
      break;
   }
}
} /*--------------------------------------------- end of iteration loop */
/*---------------------------------- update data after incremental step */           
/*-------------------------------- copy converged solution to next step */
/*solserv_copy_vec(&(actsolv->sol[kstep]),&(actsolv->sol[kstep+1]));*/
/*----------------- put converged solution to the elements in next step */
solserv_result_total(
                     actfield,
                     actintra,
                     &(actsolv->sol[0]),
                     0,
                     &(actsolv->sysarray[actsysarray]),
                     &(actsolv->sysarray_typ[actsysarray])
                    );
/*----------------------------------------------- update of load factor */                                  
nln_data->rlold = nln_data->rlnew;
/*--------------- update of parameters for material law and eas etc.... */
/* nothing needed yet..... */
/*----------------------------------------------------------------------*/
end:
amdel(&intforce_a);
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of conequ */

/*----------------------------------------------------------------------*
 |  print out iteration head                                 m.gee 11/01|
 |  kstep                           load or time step we are in         |
 |  controltyp                      type of control algorithm           |
 |  cdof                            number of dof that is controlled    |      
 *----------------------------------------------------------------------*/
void conequ_printhead(int kstep, NR_CONTROLTYP  controltyp, int cdof)
{
#ifdef DEBUG 
dstrc_enter("conequ_printhead");
#endif
/*----------------------------------------------------------------------*/
   printf("----------------------------------------------------------------------------------------------\n");
   printf("Incemental load step No. %d\n",kstep);
   printf("----------------------------------------------------------------------------------------------\n");
   switch(controltyp)
   {
   case control_disp:
   printf("Displacement control\n");
   break;
   case control_arc:
   printf("Arclenght control\n");
   break;
   case control_load:
   printf("Load control\n");
   break;
   case control_none:
      dserror("Unknown typ of path following technique");
   break;
   default:
      dserror("Unknown typ of path following technique");
   break;
   }
   printf("----------------------------------------------------------------------------------------------\n");
   printf("DISVAL....Total Displacement at equation %d\n",cdof);
   printf("----------------------------------------------------------------------------------------------\n");
   printf("RLNEW.....Actual Load factor\n");
   printf("DINORM....Norm of Residual Displacements\n");
   printf("RENORM....Norm of Out-of-Balance Loads\n");
   printf("ENERGY....Product of Out-of-Balance Loads and Residual Displacements\n");
   printf("DNORM.....Norm of incemental Displacements\n");
   printf("RNORM.....Norm of total Load\n");
   printf("----------------------------------------------------------------------------------------------\n");
   printf("ITE  DISVAL       RLNEW        DINORM       RENORM       ENERGY       DNORM        RNORM\n");
/*----------------------------------------------------------------------*/
   fprintf(allfiles.out_err,"----------------------------------------------------------------------------------------------\n");
   fprintf(allfiles.out_err,"Incemental load step No. %d\n",kstep);
   fprintf(allfiles.out_err,"----------------------------------------------------------------------------------------------\n");
   switch(controltyp)
   {
   case control_disp:
   fprintf(allfiles.out_err,"Displacement control\n");
   break;
   case control_arc:
   fprintf(allfiles.out_err,"Arclenght control\n");
   break;
   case control_load:
   fprintf(allfiles.out_err,"Load control\n");
   break;
   case control_none:
      dserror("Unknown typ of path following technique");
   break;
   default: 
      dserror("Unknown typ of path following technique");
   break;
   }
   fprintf(allfiles.out_err,"----------------------------------------------------------------------------------------------\n");
   fprintf(allfiles.out_err,"DISVAL....Total Displacement at equation %d\n",cdof);
   fprintf(allfiles.out_err,"----------------------------------------------------------------------------------------------\n");
   fprintf(allfiles.out_err,"RLNEW.....Actual Load factor\n");
   fprintf(allfiles.out_err,"DINORM....Norm of Residual Displacements\n");
   fprintf(allfiles.out_err,"RENORM....Norm of Out-of-Balance Loads\n");
   fprintf(allfiles.out_err,"ENERGY....Product of Out-of-Balance Loads and Residual Displacements\n");
   fprintf(allfiles.out_err,"DNORM.....Norm of incemental Displacements\n");
   fprintf(allfiles.out_err,"RNORM.....Norm of total Load\n");
   fprintf(allfiles.out_err,"----------------------------------------------------------------------------------------------\n");
   fprintf(allfiles.out_err,"ITE  DISVAL       RLNEW        DINORM       RENORM       ENERGY       DNORM        RNORM\n");
   fflush(allfiles.out_err);
/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of conequ_printhead */
/*----------------------------------------------------------------------*
 |  print out iteration info                                 m.gee 11/01|
 |                                                                      |
 | itnum                                   number of actual iteration   |
 | disval               displcament value of controled dof in this step |
 | rlnew                                       actual total load factor |
 | dinorm                    norm of residual incremental displacements |
 | renorm                                  Norm of out-of-balance-loads |
 | energy   norm of product of out-of-balance-loads and residual displ. |
 | dnorm                              norm of incremental displacements | 
 | rrnorm                                            norm of total load |
 *----------------------------------------------------------------------*/
void conequ_printiter(int itnum, double disval, double rlnew, double dinorm,
                     double renorm, double energy, double dnorm, double rrnorm)
{
#ifdef DEBUG 
dstrc_enter("conequ_printiter");
#endif
/*----------------------------------------------------------------------*/
    printf("%4d %-12.5#E %-12.5#E %-12.5#E %-12.5#E %-12.5#E %-12.5#E %-12.5#E\n",
   itnum,disval,rlnew,dinorm,renorm,energy,dnorm,rrnorm);
/*----------------------------------------------------- printout to err */
   fprintf(allfiles.out_err,"%4d %-12.5#E %-12.5#E %-12.5#E %-12.5#E %-12.5#E %-12.5#E %-12.5#E\n",
   itnum,disval,rlnew,dinorm,renorm,energy,dnorm,rrnorm);
   fflush(allfiles.out_err);
/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of conequ_printiter */

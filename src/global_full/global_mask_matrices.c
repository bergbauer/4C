#include "../headers/standardtypes.h"
#include "../headers/solution.h"

/*----------------------------------------------------------------------*
 |  calculate the storage mask of the global matrices    m.gee 5/01     |
 |  for various kinds of distributed sparsity patterns                  |
 |  the sparsity pattern implemented at the moment can be found in      |
 |  solution.h                                                          |
 *----------------------------------------------------------------------*/
void mask_global_matrices()
{
int i,j,k,l;               /* some counters */
int isaztec_msr  =0;       /* flag for a certain sparsity pattern */
int ishypre      =0;
int isucchb      =0;
int isdense      =0;
int isrc_ptr     =0;
FIELD      *actfield;      /* the active field */
PARTITION  *actpart;       /* my partition of the active field */
SOLVAR     *actsolv;       /* the active SOLVAR */
INTRA      *actintra;      /* the field's intra-communicator */
#ifdef DEBUG 
dstrc_enter("mask_global_matrices");
#endif
/*----------------------------------------------------------------------*/
/*------------------------------------------------ loop over all fields */
for (i=0; i<genprob.numfld; i++)
{
   actfield = &(field[i]);
   actsolv  = &(solv[i]);
   actpart  = &(partition[i]);
#ifdef PARALLEL 
   actintra = &(par.intra[i]);
#else
   /* if we are not parallel here, we have to allocate a pseudo-intracommunicator */
   actintra    = (INTRA*)calloc(1,sizeof(INTRA));
   if (!actintra) dserror("Allocation of INTRA failed");
   actintra->intra_fieldtyp = actfield->fieldtyp;
   actintra->intra_rank   = 0;
   actintra->intra_nprocs   = 1;
#endif
/*-------------------------------------- not member of this field group */
   if (actintra->intra_fieldtyp==none) continue;
/*--------------------------------------------- first check some values */
   /*----------------------------- check solver and typ of partitioning */
   /*--------- matrix is distributed modified sparse row DMSR for Aztec */
   if (actsolv->solvertyp==aztec_msr)
   {
      if (actsolv->parttyp != cut_elements)
      dserror("Partitioning has to be Cut_Elements for solution with HYPRE"); 
      else isaztec_msr=1;
   }
   /*------------------------------------------- matrix is hypre_parcsr */
   if (
       actsolv->solvertyp==hypre_amg     ||
       actsolv->solvertyp==hypre_pcg     ||
       actsolv->solvertyp==hypre_gmres   ||
       actsolv->solvertyp==hypre_bicgstab 
      )
   {
      if (actsolv->parttyp != cut_elements)
      dserror("Partitioning has to be Cut_Elements for solution with Aztec"); 
      else ishypre=1;
   }
   /*---- matrix is unsym. column compressed Harwell Boeing for superLU */
   if (actsolv->solvertyp==parsuperlu)
   {
      if (actsolv->parttyp != cut_elements)
      dserror("Partitioning has to be Cut_Elements for solution with HYPRE"); 
      else isucchb=1;
   }
   /*------------------------ matrix is (non)symmetric dense for Lapack */
   if (actsolv->solvertyp==lapack_nonsym ||
       actsolv->solvertyp==lapack_sym)
   {
      if (actsolv->parttyp != cut_elements)
      dserror("Partitioning has to be Cut_Elements for solution with LAPACK"); 
      else isdense=1;
   }
   /*-------------------- matrix is row-column pointer format for Mumps */
   if (actsolv->solvertyp==mumps_sym || actsolv->solvertyp==mumps_nonsym)
   {
      if (actsolv->parttyp != cut_elements)
      dserror("Partitioning has to be Cut_Elements for solution with HYPRE"); 
      else isrc_ptr=1;
   }
   /* allocate only one sparse matrix for each field. The sparsity
      pattern of the matrices for mass and damping and stiffness are  
      supposed to be the same, so they are calculated only once (expensive!) */
   /*------------------------- matrix is ditributed modified sparse row */
   if (isaztec_msr==1)
   {
      actsolv->nsysarray = 1;
      actsolv->sysarray_typ = (SPARSE_TYP*)  calloc(actsolv->nsysarray,sizeof(SPARSE_TYP));
      actsolv->sysarray     = (SPARSE_ARRAY*)calloc(actsolv->nsysarray,sizeof(SPARSE_ARRAY));
      if (!actsolv->sysarray_typ || !actsolv->sysarray)
         dserror("Allocation of SPARSE_ARRAY failed");
      for (i=0; i<actsolv->nsysarray; i++)
      {
         actsolv->sysarray_typ[i] = msr;
         actsolv->sysarray[i].msr = (AZ_ARRAY_MSR*)calloc(1,sizeof(AZ_ARRAY_MSR));
         if (actsolv->sysarray[i].msr==NULL) dserror("Allocation of AZ_ARRAY_MSR failed");
         actsolv->sysarray[i].msr->bins=NULL;
      }
      mask_msr(actfield,actpart,actsolv,actintra,actsolv->sysarray[0].msr);
      isaztec_msr=0;
   }
   /*------------------------------------------- matrix is hypre_parcsr */
   if (ishypre==1)
   {
      actsolv->nsysarray = 1;
      actsolv->sysarray_typ = (SPARSE_TYP*)  calloc(actsolv->nsysarray,sizeof(SPARSE_TYP));
      actsolv->sysarray     = (SPARSE_ARRAY*)calloc(actsolv->nsysarray,sizeof(SPARSE_ARRAY));
      if (!actsolv->sysarray_typ || !actsolv->sysarray)
         dserror("Allocation of SPARSE_ARRAY failed");
      for (i=0; i<actsolv->nsysarray; i++)
      {
         actsolv->sysarray_typ[i] = parcsr;
         actsolv->sysarray[i].parcsr = (H_PARCSR*)calloc(1,sizeof(H_PARCSR));
         if (actsolv->sysarray[i].parcsr==NULL) dserror("Allocation of H_PARCSR failed");
      }
      mask_parcsr(actfield,actpart,actsolv,actintra,actsolv->sysarray[0].parcsr);
      ishypre=0;
   }
   /*---------------------------------------------------- matrix is ucchb */
   if (isucchb==1)
   {
      actsolv->nsysarray = 1;
      actsolv->sysarray_typ = (SPARSE_TYP*)  calloc(actsolv->nsysarray,sizeof(SPARSE_TYP));
      actsolv->sysarray     = (SPARSE_ARRAY*)calloc(actsolv->nsysarray,sizeof(SPARSE_ARRAY));
      if (!actsolv->sysarray_typ || !actsolv->sysarray)
         dserror("Allocation of SPARSE_ARRAY failed");
      for (i=0; i<actsolv->nsysarray; i++)
      {
         actsolv->sysarray_typ[i] = ucchb;
         actsolv->sysarray[i].ucchb = (UCCHB*)calloc(1,sizeof(UCCHB));
         if (actsolv->sysarray[i].ucchb==NULL) dserror("Allocation of UCCHB failed");
      }
      mask_ucchb(actfield,actpart,actsolv,actintra,actsolv->sysarray[0].ucchb);
      isucchb=0;
   }
   /*---------------------------------------------------- matrix is dense */
   if (isdense==1)
   {
      actsolv->nsysarray = 1;
      actsolv->sysarray_typ = (SPARSE_TYP*)  calloc(actsolv->nsysarray,sizeof(SPARSE_TYP));
      actsolv->sysarray     = (SPARSE_ARRAY*)calloc(actsolv->nsysarray,sizeof(SPARSE_ARRAY));
      if (!actsolv->sysarray_typ || !actsolv->sysarray)
         dserror("Allocation of SPARSE_ARRAY failed");
      for (i=0; i<actsolv->nsysarray; i++)
      {
         actsolv->sysarray_typ[i] = dense;
         actsolv->sysarray[i].dense = (DENSE*)calloc(1,sizeof(DENSE));
         if (actsolv->sysarray[i].dense==NULL) dserror("Allocation of DENSE failed");
      }
      mask_dense(actfield,actpart,actsolv,actintra,actsolv->sysarray[0].dense);
      isdense=0;
   }
   /*----------------------------- matrix is row-column pointer format  */
   if (isrc_ptr==1)
   {
      actsolv->nsysarray = 1;
      actsolv->sysarray_typ = (SPARSE_TYP*)  calloc(actsolv->nsysarray,sizeof(SPARSE_TYP));
      actsolv->sysarray     = (SPARSE_ARRAY*)calloc(actsolv->nsysarray,sizeof(SPARSE_ARRAY));
      if (!actsolv->sysarray_typ || !actsolv->sysarray)
         dserror("Allocation of SPARSE_ARRAY failed");
      for (i=0; i<actsolv->nsysarray; i++)
      {
         actsolv->sysarray_typ[i] = rc_ptr;
         actsolv->sysarray[i].rc_ptr = (RC_PTR*)calloc(1,sizeof(RC_PTR));
         if (actsolv->sysarray[i].rc_ptr==NULL) dserror("Allocation of RC_PTR failed");
      }
      mask_rc_ptr(actfield,actpart,actsolv,actintra,actsolv->sysarray[0].rc_ptr);
      isrc_ptr=0;
   }
/*----------------------------------------------------------------------*/
} /* end of loop over numfld fields */
/*----------------------------------------------------------------------*/
#ifndef PARALLEL 
free(actintra);
#endif
#ifdef DEBUG 
dstrc_exit();
#endif
return;
} /* end of mask_global_matrices */




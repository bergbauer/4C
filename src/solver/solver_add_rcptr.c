#include "../headers/standardtypes.h"
#include "../headers/solution.h"
/*----------------------------------------------------------------------*
 | global dense matrices for element routines             m.gee 9/01    |
 | (defined in global_calelm.c, so they are extern here)                |                
 *----------------------------------------------------------------------*/
extern struct _ARRAY estif_global;
extern struct _ARRAY emass_global;
/*----------------------------------------------------------------------*
 |  routine to assemble element array to global rcptr-matrix            |
 |  in parallel,taking care of coupling conditions                      |
 |                                                                      |
 |                                                                      |
 |                                                         m.gee 1/02   |
 *----------------------------------------------------------------------*/
void  add_rc_ptr(struct _PARTITION     *actpart,
                struct _SOLVAR        *actsolv,
                struct _INTRA         *actintra,
                struct _ELEMENT       *actele,
                struct _RC_PTR        *rc_ptr)
{
#ifdef MUMPS_PACKAGE
int         i,j,k,l,counter;          /* some counter variables */
int         start,index,lenght;       /* some more special-purpose counters */
int         ii,jj;                    /* counter variables for system matrix */
int         ii_iscouple;              /* flag whether ii is a coupled dof */
int         ii_owner;                 /* who is owner of dof ii -> procnumber */
int         ii_index;                 /* place of ii in dmsr format */
int         jj_index;                 /* place of jj in dmsr format */
int         nd,ndnd;                  /* size of estif */
int         nnz;                      /* number of nonzeros in sparse system matrix */
int         numeq_total;              /* total number of equations */
int         numeq;                    /* number of equations on this proc */
int         lm[MAXDOFPERELE];         /* location vector for this element */
int         owner[MAXDOFPERELE];      /* the owner of every dof */
int         myrank;                   /* my intra-proc number */
int         nprocs;                   /* my intra- number of processes */
double    **estif;                    /* element matrix to be added to system matrix */
int        *update;                   /* vector update see AZTEC manual */
double     *A_loc;                    /*    "       A_loc see MUMPS manual */
int        *irn;                      /*    "       irn see MUMPS manual */
int        *jcn;                      /*    "       jcn see MUMPS manual */
int        *rowptr;                   /*    "       rowptr see rc_ptr structure */
int       **cdofs;                    /* list of coupled dofs and there owners, see init_assembly */
int         ncdofs;                   /* total number of coupled dofs */
int       **isend;                    /* pointer to sendbuffer to communicate coupling conditions */
double    **dsend;                    /* pointer to sendbuffer to communicate coupling conditions */
int         nsend;
#ifdef DEBUG 
dstrc_enter("add_rc_ptr");
#endif
/*----------------------------------------------------------------------*/
/*------------------------------------- set some pointers and variables */
myrank     = actintra->intra_rank;
nprocs     = actintra->intra_nprocs;
estif      = estif_global.a.da;
nd         = actele->numnp * actele->node[0]->numdf;
ndnd       = nd*nd;
nnz        = rc_ptr->nnz;
numeq_total= rc_ptr->numeq_total;
numeq      = rc_ptr->numeq;
update     = rc_ptr->update.a.iv;
A_loc      = rc_ptr->A_loc.a.dv;
irn        = rc_ptr->irn_loc.a.iv;
jcn        = rc_ptr->jcn_loc.a.iv;
rowptr     = rc_ptr->rowptr.a.iv;
cdofs      = actpart->pdis[0].coupledofs.a.ia;
ncdofs     = actpart->pdis[0].coupledofs.fdim;
/*---------------------------------- put pointers to sendbuffers if any */
#ifdef PARALLEL 
if (rc_ptr->couple_i_send) 
{
   isend = rc_ptr->couple_i_send->a.ia;
   dsend = rc_ptr->couple_d_send->a.da;
   nsend = rc_ptr->couple_i_send->fdim;
}
#endif
/*---------------------------------------------- make location vector lm*/
counter=0;
for (i=0; i<actele->numnp; i++)
{
   for (j=0; j<actele->node[i]->numdf; j++)
   {
      lm[counter]    = actele->node[i]->dof[j];
#ifdef PARALLEL 
      owner[counter] = actele->node[i]->proc;
#endif
      counter++;
   }
}/* end of loop over element nodes */
if (counter != nd) dserror("assemblage failed due to wrong dof numbering");
/*========================================== now start looping the dofs */
/*======================================= loop over i (the element row) */
ii_iscouple = 0;
ii_owner    = myrank;
for (i=0; i<nd; i++)
{
   ii = lm[i];
   /*-------------------------------------------- loop only my own rows */
#ifdef PARALLEL 
   if (owner[i]!=myrank) continue;
#endif
   /*------------------------------------- check for boundary condition */
   if (ii>=numeq_total) continue;
   /*------------------------------------- check for coupling condition */
#ifdef PARALLEL 
   if (ncdofs)
   {
      ii_iscouple = 0;
      ii_owner    = -1;
      add_msr_checkcouple(ii,cdofs,ncdofs,&ii_iscouple,&ii_owner,nprocs);
   }
#endif
   /*-------------------- ii is not a coupled dofs or I am master owner */
   if (!ii_iscouple || ii_owner==myrank)
   {
      ii_index      = find_index(ii,update,numeq);
      if (ii_index==-1) dserror("dof ii not found on this proc");
      start         = rowptr[ii_index];
      lenght        = rowptr[ii_index+1]-rowptr[ii_index];
   }
   /*================================= loop over j (the element column) */
   /*                            This is the full unsymmetric version ! */
   for (j=0; j<nd; j++)
   {
      jj = lm[j];
      /*---------------------------------- check for boundary condition */
      if (jj>=numeq_total) continue;
      /*---------------------------------- check for coupling condition */
      /* 
        coupling condition for jj is not checked, because we add to 
        row ii here, which must also hold the coupled columns jj
      */ 
      /*======================================== do main-diagonal entry */
      /*                (either not a coupled dof or I am master owner) */
      if (!ii_iscouple || ii_owner==myrank)
      {
         index         = find_index(jj,&(jcn[start]),lenght);
         if (index==-1) dserror("dof jj not found in this row ii");
         index        += start;
         A_loc[index] += estif[i][j];
      }
      /*======================================== do main-diagonal entry */
      /*                           (a coupled dof and I am slave owner) */
      else
      {
         add_rcptr_sendbuff(ii,jj,i,j,ii_owner,isend,dsend,estif,nsend);
      }


   } /* end loop over j */
}/* end loop over i */
/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
#endif /* end of ifdef MUMPS_PACKAGE */
return;
} /* end of add_rc_ptr */

/*----------------------------------------------------------------------*
 |  fill sendbuffer isend and dsend                           m.gee 1/02|
 *----------------------------------------------------------------------*/
int add_rcptr_sendbuff(int ii,int jj,int i,int j,int ii_owner,int **isend,
                    double **dsend,double **estif, int numsend)
{
#ifdef MUMPS_PACKAGE
int         k,l;
#ifdef DEBUG 
dstrc_enter("add_rcptr_sendbuff");
#endif
/*----------------------------------------------------------------------*/
for (k=0; k<numsend; k++)
{
   if (isend[k][0]==ii) break;
}
isend[k][1]  = ii_owner;
dsend[k][jj]+= estif[i][j];
/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
#endif /* end of ifdef MUMPS_PACKAGE */
return;
} /* end of add_rcptr_sendbuff */


/*----------------------------------------------------------------------*
 |  exchange coupled dofs and add to row/column ptr matrix    m.gee 1/02|
 *----------------------------------------------------------------------*/
void exchange_coup_rc_ptr(
                         PARTITION     *actpart,
                         SOLVAR        *actsolv,
                         INTRA         *actintra,
                         RC_PTR        *rc_ptr
                        )
{
#ifdef MUMPS_PACKAGE
int            i,j;
int            ii,jj,ii_index;
int            start;
int            lenght;
int            index;
int            tag;
int            source;
int            numeq,numeq_total;
int            numsend;
int            numrecv;
int           *update;
int          **isend;
double       **dsend;
int          **irecv;
double       **drecv;
int            imyrank;
int            inprocs;
double        *A_loc;                    /*    "       A_loc see MUMPS manual */
int           *irn;                      /*    "       irn see MUMPS manual */
int           *jcn;                      /*    "       jcn see MUMPS manual */
int           *rowptr;                   /*    "       rowptr see rc_ptr structure */

#ifdef PARALLEL 
MPI_Status    *irecv_status;
MPI_Status    *drecv_status;

MPI_Request   *isendrequest;
MPI_Request   *dsendrequest;

MPI_Comm      *ACTCOMM;
#endif

#ifdef DEBUG 
dstrc_enter("exchange_coup_rc_ptr");
#endif
/*----------------------------------------------------------------------*/
#ifdef PARALLEL 
/*----------------------------------------------------------------------*/
imyrank = actintra->intra_rank;
inprocs = actintra->intra_nprocs;
ACTCOMM = &(actintra->MPI_INTRA_COMM);
/*---------------------------------------- set some pointers and values */
numsend     = rc_ptr->numcoupsend;
numrecv     = rc_ptr->numcouprecv;
A_loc      = rc_ptr->A_loc.a.dv;
irn        = rc_ptr->irn_loc.a.iv;
jcn        = rc_ptr->jcn_loc.a.iv;
rowptr     = rc_ptr->rowptr.a.iv;
update      = rc_ptr->update.a.iv;
numeq_total = rc_ptr->numeq_total;
numeq       = rc_ptr->numeq;
if (rc_ptr->couple_i_send) isend = rc_ptr->couple_i_send->a.ia;
if (rc_ptr->couple_d_send) dsend = rc_ptr->couple_d_send->a.da;
if (rc_ptr->couple_i_recv) irecv = rc_ptr->couple_i_recv->a.ia;
if (rc_ptr->couple_d_recv) drecv = rc_ptr->couple_d_recv->a.da;
/*--------------------------------------------- allocate some envelopes */
if (numrecv)
{
   irecv_status = (MPI_Status*)CALLOC(numrecv,sizeof(MPI_Status));
   drecv_status = (MPI_Status*)CALLOC(numrecv,sizeof(MPI_Status));
   if (!irecv_status || !drecv_status) dserror("Allocation of memory failed");
}
if (numsend)
{
   isendrequest = (MPI_Request*)CALLOC(numsend,sizeof(MPI_Request));
   dsendrequest = (MPI_Request*)CALLOC(numsend,sizeof(MPI_Request));
   if ( !isendrequest || !dsendrequest) dserror("Allocation of memory failed");
}
/*-------------------------------------------- loop the dofs to be send */
/* do all non-blocking sends and don't care about arrival (wird scho' klappe)*/
/*     the only thing to care for is the order in which things are send */
for (i=0; i<numsend; i++)
{
/*            sendbuffer       lenght    typ        dest.        tag          comm      request-handle */
   MPI_Isend(&(isend[i][0]),          2,MPI_INT   ,isend[i][1],isend[i][0],(*ACTCOMM),&(isendrequest[i]));
   MPI_Isend(&(dsend[i][0]),numeq_total,MPI_DOUBLE,isend[i][1],isend[i][0],(*ACTCOMM),&(dsendrequest[i]));
}/*------------------------------------------------ end of sending loop */
/*------------------------------- now loop over the dofs to be received */
/* 
   do blocking receives, 'cause one can't add something to the system
   matrix, which has not yet arrived, easy, isn't it?
*/
for (i=0; i<numrecv; i++)
{
   /*--------------------------- use wildcards to receive first to come */
   /*          recv-buf  lenght typ     source           tag       comm              status */
   MPI_Recv(&(irecv[i][0]),2,MPI_INT,MPI_ANY_SOURCE,MPI_ANY_TAG,(*ACTCOMM),&(irecv_status[i]));
   if (irecv_status[i].MPI_ERROR) dserror("An error in MPI - communication occured !");

   /*---------------------- the dof number was sent as tag and as entry */
   tag    = irecv_status[i].MPI_TAG;
   if (tag != irecv[i][0]) dserror("MPI messages somehow got mixed up");
   source = irecv_status[i].MPI_SOURCE;
   
   /* do not use wildcards for second recv, we know now where it should come from */
   MPI_Recv(&(drecv[i][0]),numeq_total,MPI_DOUBLE,source,tag,(*ACTCOMM),&(drecv_status[i]));   
   if (drecv_status[i].MPI_ERROR) dserror("An error in MPI - communication occured !");

   /* now add the received data properly to my own piece of sparse matrix */
   ii = tag;
   ii_index = find_index(ii,update,numeq);
   if (ii_index==-1) dserror("dof ii not found on this proc");
   start         = rowptr[ii_index];
   lenght        = rowptr[ii_index+1]-rowptr[ii_index];
   for (j=0; j<lenght; j++)
   {
      index         = start+j;
      jj            = jcn[index];
      A_loc[index] += drecv[i][jj];
   }
}/*---------------------------------------------- end of receiving loop */
/*-------------------------------------------- free allocated MPI-stuff */
if (numrecv){FREE(irecv_status);FREE(drecv_status);}
if (numsend){FREE(isendrequest);FREE(dsendrequest);}
/*----------------------------------------------------------------------
  do a barrier, because this is the end of the assembly, the msr matrix
  is now ready for solve
*/ 
MPI_Barrier(*ACTCOMM);
#endif /*---------------------------------------------- end of PARALLEL */ 
/*----------------------------------------------------------------------*/
#ifdef DEBUG 
dstrc_exit();
#endif
#endif /* end of ifdef MUMPS_PACKAGE */
return;
} /* end of exchange_coup_rc_ptr */

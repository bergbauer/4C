#include "../headers/standardtypes.h"
#include "../headers/solution.h"
/*----------------------------------------------------------------------*
 |  control solver lib SPOOLES                           m.gee 4/02     |
 | -only the unsymmetric solve is working                               |
 | -there is a way in spooles to solve singular matrices (not impl.)    |
 |  see spooles/misc/drivers                                            |
 | -a pure sequentiell call to spooles is in solver_spooles.cseq,       |
 |  but it is still missing the clean-up phase                          |
 | -to understand any of the written below, read the manuals            |
 *----------------------------------------------------------------------*/
void solver_spooles( 
                      struct _SOLVAR         *actsolv,
                      struct _INTRA          *actintra,
                      struct _SPOOLMAT       *spo,
                      struct _DIST_VECTOR    *sol,
                      struct _DIST_VECTOR    *rhs,
                      int                     option
                     )
{
#ifdef SPOOLES_PACKAGE
int        i,j=0;
int        imyrank;
int        inprocs;
int        nnz,numeq,numeq_total;
int        seed = 10101;
int        nedges;
int       *irn,*jcn;
int       *update;
int       *rowind1,nrow;
double    *A_loc;
double    *b;
double    *x;
double    *opcounts,minops,cutoff,cpus[20],tau=100.;
double     droptol=0.0;
int        root,firsttag=0,error,stats[20];
DV        *cumopsDV;
IV        *ownedColumnsIV;
int        nmycol;
InpMtx    *newA;    
DenseMtx  *newY;
SolveMap  *solvemap ;
int        sym=SPOOLES_NONSYMMETRIC;/* SPOOLES_SYMMETRIC laeuft nich */ 
int        pivotingflag=0;
static ARRAY recv_a;
static double *recv;
/* for debugging 
int   msglvl=2;
FILE *msgFile;*/
/* for debugging */
int   msglvl=0;
FILE *msgFile=NULL;
char  buffer[50];


#ifdef DEBUG 
dstrc_enter("solver_spooles");
#endif
if (msglvl)
{
   sprintf(buffer,"spooles.msg%d",actintra->intra_rank);
   msgFile = fopen(buffer,"a+");
}
/*----------------------------------------------------------------------*/
imyrank      = actintra->intra_rank;
inprocs      = actintra->intra_nprocs;
/*----------------------------------------------------------------------*/
switch(option)
{
/*----------------------------------------------------------------------*/
/*                                                           init phase */
/*----------------------------------------------------------------------*/
case 1:
   /*--------------------------------------- create the spooles objects */
   spo->mtxA = InpMtx_new();
   /*-------------------------------- make the solution and rhs objects */
   spo->mtxY = DenseMtx_new();
   spo->mtxX = DenseMtx_new();
   /*-------------------------------------------------- set some values */
   irn         = spo->irn_loc.a.iv;
   jcn         = spo->jcn_loc.a.iv;
   update      = spo->update.a.iv;
   nnz         = spo->nnz; 
   numeq       = spo->numeq; 
   numeq_total = spo->numeq_total;
   
   
   
   spo->is_init    =1;
   spo->ncall      =0;
   spo->is_factored=0;
break;
/*----------------------------------------------------------------------*/
/*                                                    end of init phase */
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
/*                                                    calculation phase */
/*----------------------------------------------------------------------*/
case 0:
   /*-------------------------------------------------------------------*/
   if (spo->is_init!=1) dserror("Sparse matrix has not been initialized");
   /*---------------------------------- set some pointers to the matrix */
   irn    = spo->irn_loc.a.iv;
   jcn    = spo->jcn_loc.a.iv;
   update = spo->update.a.iv;
   A_loc  = spo->A_loc.a.dv;
   b      = rhs->vec.a.dv;
   x      = sol->vec.a.dv;
   nnz    = spo->nnz; 
   numeq  = spo->numeq; 
   numeq_total = spo->numeq_total;   
   IVzero(20,stats);
   DVzero(20,cpus);
   /*-------------------------------------------------------------------*/
   if (recv_a.Typ != cca_DV)
   recv = amdef("recv",&recv_a,numeq_total,1,"DV");
   if (recv_a.fdim < numeq_total)
   {
      amdel(&recv_a);
      recv = amdef("recv",&recv_a,numeq_total,1,"DV");
   }
/*----------------------------------------------------------------------*/
/*
   --------------------------------------------
   STEP 1: read the entries 
           and create the InpMtx object
   --------------------------------------------
*/
   spo->mtxA = InpMtx_new();
   InpMtx_init(spo->mtxA,INPMTX_BY_ROWS,1,nnz,0);
   for (i=0; i<nnz; i++)
   InpMtx_inputRealEntry(spo->mtxA,irn[i],jcn[i],A_loc[i]);
   InpMtx_sortAndCompress(spo->mtxA);
   InpMtx_changeStorageMode(spo->mtxA,INPMTX_BY_VECTORS) ;
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n input matrix") ;
      InpMtx_writeForHumanEye(spo->mtxA, msgFile) ;
   }
/*--------------------------------------------------------------------*/
/*
   -----------------------------------------
   STEP 2: read the right hand side matrix B
   -----------------------------------------
*/
   spo->mtxY = DenseMtx_new();
   DenseMtx_init(spo->mtxY,SPOOLES_REAL,0,0,numeq,1,1,numeq);
   DenseMtx_zero(spo->mtxY);
   DenseMtx_rowIndices(spo->mtxY, &nrow, &rowind1);
   for (i=0; i<numeq; i++)
   {
      rowind1[i] = update[i];
      DenseMtx_setRealEntry(spo->mtxY,i,0,b[i]);
   }
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n rhs matrix in original ordering");
      DenseMtx_writeForHumanEye(spo->mtxY,msgFile);
      fflush(msgFile);
   }

/*----------------------------------------------------------------------*/
/*
   -------------------------------------------------
   STEP 3 : find a low-fill ordering
   (1) create the Graph object
   (2) order the graph using multiple minimum degree
   -------------------------------------------------
*/
   spo->graph  = Graph_new();
   spo->adjIVL = InpMtx_MPI_fullAdjacency(spo->mtxA,stats,msglvl,msgFile,
                                          actintra->MPI_INTRA_COMM);

   nedges = IVL_tsize(spo->adjIVL);
   Graph_init2(spo->graph,0,numeq_total,0,nedges,numeq_total,nedges,
               spo->adjIVL,NULL,NULL);
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n graph of the input matrix");
      Graph_writeForHumanEye(spo->graph,msgFile);
      fflush(msgFile);
   }
   spo->frontETree = orderViaMMD(spo->graph,seed+imyrank,msglvl,msgFile);
   IVL_free(spo->adjIVL);
   Graph_free(spo->graph) ;
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile,"\n\n front tree from ordering");
      ETree_writeForHumanEye(spo->frontETree, msgFile);
      fflush(msgFile);
   }
   opcounts = DVinit(inprocs,0.0);
   opcounts[imyrank] = ETree_nFactorOps(spo->frontETree,SPOOLES_REAL,sym);
   MPI_Allgather(&opcounts[imyrank],1,MPI_DOUBLE,
                 opcounts,1,MPI_DOUBLE,actintra->MPI_INTRA_COMM);
   minops = DVmin(inprocs,opcounts,&root);
   DVfree(opcounts);
   spo->frontETree = ETree_MPI_Bcast(spo->frontETree,root,msglvl,msgFile,
                                actintra->MPI_INTRA_COMM);
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile,"\n\n front tree from ordering");
      ETree_writeForHumanEye(spo->frontETree, msgFile);
      fflush(msgFile);
   }
/*----------------------------------------------------------------------*/
/*
   -----------------------------------------------------
   STEP 4: get the permutation, permute the matrix and 
           front tree and get the symbolic factorization,
           permute the right hand side.
   -----------------------------------------------------
*/
   spo->oldToNewIV = ETree_oldToNewVtxPerm(spo->frontETree);
   spo->newToOldIV = ETree_newToOldVtxPerm(spo->frontETree);
   ETree_permuteVertices(spo->frontETree,spo->oldToNewIV);
   InpMtx_permute(spo->mtxA,IV_entries(spo->oldToNewIV),
                            IV_entries(spo->oldToNewIV));
   if (sym==SPOOLES_SYMMETRIC)
   InpMtx_mapToUpperTriangle(spo->mtxA);
   InpMtx_changeCoordType(spo->mtxA,INPMTX_BY_CHEVRONS);
   InpMtx_changeStorageMode(spo->mtxA,INPMTX_BY_VECTORS);
   DenseMtx_permuteRows(spo->mtxY,spo->oldToNewIV);
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n old-to-new permutation vector");
      IV_writeForHumanEye(spo->oldToNewIV, msgFile);
      fprintf(msgFile, "\n\n new-to-old permutation vector");
      IV_writeForHumanEye(spo->newToOldIV, msgFile);
      fprintf(msgFile, "\n\n front tree after permutation");
      ETree_writeForHumanEye(spo->frontETree, msgFile);
      fprintf(msgFile, "\n\n input matrix after permutation");
      InpMtx_writeForHumanEye(spo->mtxA, msgFile);
      fprintf(msgFile, "\n\n symbolic factorization");
      DenseMtx_writeForHumanEye(spo->mtxY, msgFile) ;
      fflush(msgFile);
   }
/*--------------------------------------------------------------------*/
/*
   -------------------------------------------
   STEP 5: generate the owners map IV object
           and the map from vertices to owners
   -------------------------------------------
*/
   cutoff   = 1./(2*inprocs);
   cumopsDV = DV_new();
   DV_init(cumopsDV,inprocs,NULL);
   spo->ownersIV = ETree_ddMap(spo->frontETree,SPOOLES_REAL,sym,cumopsDV,cutoff);
   DV_free(cumopsDV);
   spo->vtxmapIV = IV_new();
   IV_init(spo->vtxmapIV,numeq_total,NULL);
   IVgather(numeq_total,IV_entries(spo->vtxmapIV),IV_entries(spo->ownersIV),
            ETree_vtxToFront(spo->frontETree));
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n map from fronts to owning processes") ;
      IV_writeForHumanEye(spo->ownersIV, msgFile) ;
      fprintf(msgFile, "\n\n map from vertices to owning processes") ;
      IV_writeForHumanEye(spo->vtxmapIV, msgFile) ;
      fflush(msgFile) ;
   }
/*--------------------------------------------------------------------*/
/*
   ---------------------------------------------------
   STEP 6: redistribute the matrix and right hand side
   ---------------------------------------------------
*/
   firsttag = 0;
   newA = InpMtx_MPI_split(spo->mtxA,spo->vtxmapIV,stats,msglvl,msgFile,
                           firsttag,actintra->MPI_INTRA_COMM);
   firsttag++;
   InpMtx_free(spo->mtxA);
   spo->mtxA = newA;
   InpMtx_changeStorageMode(spo->mtxA,INPMTX_BY_VECTORS);
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n split InpMtx") ;
      InpMtx_writeForHumanEye(spo->mtxA,msgFile) ;
      fflush(msgFile) ;
   }
   newY = DenseMtx_MPI_splitByRows(spo->mtxY,spo->vtxmapIV,stats,msglvl, 
                                   msgFile,firsttag,actintra->MPI_INTRA_COMM);
   DenseMtx_free(spo->mtxY);
   spo->mtxY = newY;
   firsttag += inprocs;
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n split DenseMtx Y") ;
      DenseMtx_writeForHumanEye(spo->mtxY, msgFile) ;
      fflush(msgFile) ;
   }
/*--------------------------------------------------------------------*/
/*
   ------------------------------------------
   STEP 7: compute the symbolic factorization
   ------------------------------------------
*/
   spo->symbfacIVL = SymbFac_MPI_initFromInpMtx(spo->frontETree,spo->ownersIV,
                     spo->mtxA,stats,msglvl,msgFile,firsttag,
                     actintra->MPI_INTRA_COMM);
   firsttag += spo->frontETree->nfront;
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n local symbolic factorization");
      IVL_writeForHumanEye(spo->symbfacIVL, msgFile);
      fflush(msgFile);
   }
/*--------------------------------------------------------------------*/
/*
   -----------------------------------
   STEP 8: initialize the front matrix
   -----------------------------------
*/
   spo->mtxmanager = SubMtxManager_new() ;
   SubMtxManager_init(spo->mtxmanager, NO_LOCK, 0) ;
   spo->frontmtx = FrontMtx_new() ;
   FrontMtx_init(spo->frontmtx,spo->frontETree,spo->symbfacIVL,SPOOLES_REAL,sym,
                 FRONTMTX_DENSE_FRONTS,pivotingflag,NO_LOCK,imyrank,
                 spo->ownersIV,spo->mtxmanager,msglvl,msgFile);
/*--------------------------------------------------------------------*/
/*
   -----------------------------------------
   STEP 9: compute the numeric factorization
   -----------------------------------------
*/
   spo->chvmanager = ChvManager_new();
   ChvManager_init(spo->chvmanager,NO_LOCK,0);
   spo->rootchv = FrontMtx_MPI_factorInpMtx(spo->frontmtx,spo->mtxA,tau,droptol,
                        spo->chvmanager,spo->ownersIV,0,&error,cpus, 
                        stats,msglvl,msgFile,firsttag,actintra->MPI_INTRA_COMM);
   ChvManager_free(spo->chvmanager);
   firsttag += 3*(spo->frontETree->nfront) + 2;
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n numeric factorization");
      FrontMtx_writeForHumanEye(spo->frontmtx, msgFile);
      fflush(msgFile);
   }
   if ( error >= 0 ) 
   {
      dserror("Error in spooles numeric factorization");
   }
/*--------------------------------------------------------------------*/
/*
   --------------------------------------
   STEP 10: post-process the factorization
   --------------------------------------
*/
   FrontMtx_MPI_postProcess(spo->frontmtx,spo->ownersIV,stats,msglvl,
                            msgFile,firsttag,actintra->MPI_INTRA_COMM);
   firsttag += 5*inprocs ;
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n numeric factorization after post-processing");
      FrontMtx_writeForHumanEye(spo->frontmtx, msgFile);
      fflush(msgFile);
   }
/*--------------------------------------------------------------------*/
/*
   -----------------------------------
   STEP 11: create the solve map object
   -----------------------------------
*/
   solvemap = SolveMap_new() ;
   SolveMap_ddMap(solvemap,spo->frontmtx->symmetryflag, 
                  FrontMtx_upperBlockIVL(spo->frontmtx),
                  FrontMtx_lowerBlockIVL(spo->frontmtx),
                  inprocs,spo->ownersIV,FrontMtx_frontTree(spo->frontmtx), 
                  seed,msglvl,msgFile);
   /* debug */
   if (msglvl)
   {
      SolveMap_writeForHumanEye(solvemap,msgFile);
      fflush(msgFile);
   }
/*--------------------------------------------------------------------*/
/*
   ----------------------------------------------------
   STEP 12: redistribute the submatrices of the factors
   ----------------------------------------------------
*/
   FrontMtx_MPI_split(spo->frontmtx,solvemap, 
                      stats,msglvl,msgFile,firsttag,actintra->MPI_INTRA_COMM);
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n numeric factorization after split") ;
      FrontMtx_writeForHumanEye(spo->frontmtx, msgFile) ;
      fflush(msgFile) ;
   }
/*--------------------------------------------------------------------*/
/*
   ------------------------------------------------
   STEP 13: permute and redistribute Y if necessary
   ------------------------------------------------
*/
   if ( FRONTMTX_IS_PIVOTING(spo->frontmtx) ) 
   {
      IV   *rowmapIV ;
/*
   ----------------------------------------------------------
   pivoting has taken place, redistribute the right hand side
   to match the final rows and columns in the fronts
   ----------------------------------------------------------
*/
   rowmapIV = FrontMtx_MPI_rowmapIV(spo->frontmtx,spo->ownersIV,msglvl,
                                    msgFile,actintra->MPI_INTRA_COMM);
   newY = DenseMtx_MPI_splitByRows(spo->mtxY,rowmapIV,stats,msglvl, 
                                   msgFile,firsttag,actintra->MPI_INTRA_COMM);
   DenseMtx_free(spo->mtxY);
   spo->mtxY = newY;
   IV_free(rowmapIV);
   }
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n\n rhs matrix after split");
      DenseMtx_writeForHumanEye(spo->mtxY, msgFile);
      fflush(msgFile);
   }
/*--------------------------------------------------------------------*/
/*
   ------------------------------------------
   STEP 14: create a solution DenseMtx object
   ------------------------------------------
*/
   ownedColumnsIV = FrontMtx_ownedColumnsIV(spo->frontmtx,imyrank,spo->ownersIV,
                                            msglvl,msgFile);
   nmycol = IV_size(ownedColumnsIV);
   spo->mtxX = DenseMtx_new();
   if ( nmycol > 0 ) 
   {
      DenseMtx_init(spo->mtxX,SPOOLES_REAL,0,0,nmycol,1,1,nmycol);
      DenseMtx_rowIndices(spo->mtxX,&nrow,&rowind1);
      IVcopy(nmycol,rowind1,IV_entries(ownedColumnsIV));
   }
/*--------------------------------------------------------------------*/
/*
   --------------------------------
   STEP 15: solve the linear system
   --------------------------------
*/
   spo->solvemanager = SubMtxManager_new();
   SubMtxManager_init(spo->solvemanager, NO_LOCK, 0);
   FrontMtx_MPI_solve(spo->frontmtx,spo->mtxX,spo->mtxY,spo->solvemanager,
                      solvemap,cpus,stats,msglvl,msgFile,firsttag,
                      actintra->MPI_INTRA_COMM);
   SubMtxManager_free(spo->solvemanager);
   /* debug */
   if (msglvl)
   {
      fprintf(msgFile, "\n solution in new ordering");
      DenseMtx_writeForHumanEye(spo->mtxX, msgFile);
   }
/*--------------------------------------------------------------------*/
/*
   --------------------------------------------------------
   STEP 15: permute the solution into the original ordering
            and assemble the solution onto processor zero
   --------------------------------------------------------
*/
   DenseMtx_permuteRows(spo->mtxX,spo->newToOldIV);
   /* debug */
   if (msglvl)
   {
   fprintf(msgFile, "\n\n solution in old ordering");
   DenseMtx_writeForHumanEye(spo->mtxX,msgFile);
   fflush(msgFile);
   }
   IV_fill(spo->vtxmapIV, 0);
   firsttag++ ;
   spo->mtxX = DenseMtx_MPI_splitByRows(spo->mtxX,spo->vtxmapIV,stats,msglvl,msgFile,
                                   firsttag,actintra->MPI_INTRA_COMM);
   /* put complete solution to recv on proc 0 */
   if (imyrank==0)
   {
      for (i=0; i<numeq_total; i++) recv[i] =  spo->mtxX->entries[i];
   }
   /* broadcast solution */
   MPI_Bcast(recv,numeq_total,MPI_DOUBLE,0,actintra->MPI_INTRA_COMM);
   /* every proc puts his own piece to sol */
   for (i=0; i<numeq; i++)
   {
      sol->vec.a.dv[i] = recv[spo->update.a.iv[i]];
   }
   /* debug */
   if (imyrank==0 && msglvl)
   {
   fprintf(msgFile, "\n\n complete solution in old ordering");
   DenseMtx_writeForHumanEye(spo->mtxX, msgFile);
   fflush(msgFile);
   }
/*--------------------------------------------------------------------*/
/*
   -----------
   free memory
   -----------
*/

   FrontMtx_free(spo->frontmtx);
   InpMtx_free(spo->mtxA);
   DenseMtx_free(spo->mtxY);
   DenseMtx_free(spo->mtxX);
   ETree_free(spo->frontETree);
   SubMtxManager_free(spo->mtxmanager);
   IV_free(spo->newToOldIV);
   IV_free(spo->oldToNewIV);
   IV_free(spo->ownersIV);
   IV_free(spo->vtxmapIV);
   IV_free(ownedColumnsIV);
   SolveMap_free(solvemap);
   IVL_free(spo->symbfacIVL);
/*--------------------------------------------------------------------*/
   spo->is_factored=1;
   spo->ncall++;
break;
/*----------------------------------------------------------------------*/
/*                                             end of calculation phase */
/*----------------------------------------------------------------------*/
default:
   dserror("Unknown option for solver call to Aztec");
break;   
}
/*----------------------------------------------------------------------*/
if (msglvl) fclose(msgFile);
#ifdef DEBUG 
dstrc_exit();
#endif
#endif /* end of ifdef SPOOLES_PACKAGE */
return;
} /* end of solver_spooles */





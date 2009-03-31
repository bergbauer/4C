#ifdef CCADISCRET

#include "fsi_overlapprec_fsiamg.H"
#include <Epetra_Time.h>
#include <ml_MultiLevelPreconditioner.h>
#include "MLAPI_Operator_Utils.h"
#include "MLAPI_CompObject.h"
#include "MLAPI_MultiVector.h"
#include "MLAPI_Expressions.h"
#include "MLAPI_Workspace.h"

#include "EpetraExt_SolverMap_CrsMatrix.h"

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
FSI::OverlappingBlockMatrixFSIAMG::OverlappingBlockMatrixFSIAMG(
                                                    const LINALG::MultiMapExtractor& maps,
                                                    ADAPTER::Structure& structure,
                                                    ADAPTER::Fluid& fluid,
                                                    ADAPTER::Ale& ale,
                                                    bool structuresplit,
                                                    int symmetric,
                                                    vector<double>& omega,
                                                    vector<int>& iterations,
                                                    double somega,
                                                    int siterations,
                                                    double fomega,
                                                    int fiterations,
                                                    FILE* err)
  : OverlappingBlockMatrix(maps,
                           structure,
                           fluid,
                           ale,
                           structuresplit,
                           symmetric,
                           omega[0],
                           iterations[0],
                           somega,
                           siterations,
                           fomega,
                           fiterations,
                           err),
pciter_(iterations),
pcomega_(omega)
{
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::SetupPreconditioner()
{
  if (!structuresplit_) dserror("FSIAMG only with structuresplit monoFSI");
  
  MLAPI::Init();
  const int myrank = Matrix(0,0).Comm().MyPID();

  const LINALG::SparseMatrix& structInnerOp = Matrix(0,0);
  const LINALG::SparseMatrix& fluidInnerOp  = Matrix(1,1);
  const LINALG::SparseMatrix& aleInnerOp    = Matrix(2,2);
  
  RCP<LINALG::MapExtractor> fsidofmapex = null;
  RCP<Epetra_Map>           irownodes = null;
  
  // build AMG hierarchies
  structuresolver_->Setup(structInnerOp.EpetraMatrix());
  fluidsolver_->Setup(fluidInnerOp.EpetraMatrix(),
                      fsidofmapex,
                      fluid_.Discretization(),
                      irownodes,
                      structuresplit_);
  if (constalesolver_==Teuchos::null)
    alesolver_->Setup(aleInnerOp.EpetraMatrix());

  // get the ml_MultiLevelPreconditioner class from within struct/fluid solver
  RCP<Epetra_Operator> sprec = structuresolver_->EpetraOperator();
  RCP<Epetra_Operator> fprec = fluidsolver_->EpetraOperator();
  RCP<Epetra_Operator> aprec = alesolver_->EpetraOperator();
  
  // get ML preconditioner class
  ML_Epetra::MultiLevelPreconditioner* smlclass = 
    dynamic_cast<ML_Epetra::MultiLevelPreconditioner*>(sprec.get());
  ML_Epetra::MultiLevelPreconditioner* fmlclass = 
    dynamic_cast<ML_Epetra::MultiLevelPreconditioner*>(fprec.get());
  ML_Epetra::MultiLevelPreconditioner* amlclass = 
    dynamic_cast<ML_Epetra::MultiLevelPreconditioner*>(aprec.get());
  if (!smlclass || !fmlclass || !amlclass) dserror("Not using ML for Fluid, Structure or Ale");

  // get copy of ML parameter list
  sparams_ = structuresolver_->Params().sublist("ML Parameters");
  fparams_ = fluidsolver_->Params().sublist("ML Parameters");
  aparams_ = alesolver_->Params().sublist("ML Parameters");
  //cout << "====================Structure Params\n" << sparams_;
  //cout << "========================Fluid Params\n" << fparams_;
  //cout << "========================Ale Params\n" << aparams_;
  // find for which field using nonsymmetric AMG
  bool SisPetrovGalerkin = sparams_.get("energy minimization: enable",false);
  bool FisPetrovGalerkin = fparams_.get("energy minimization: enable",false);
  bool AisPetrovGalerkin = aparams_.get("energy minimization: enable",false);

  // get ML handle
  ML* sml = const_cast<ML*>(smlclass->GetML());
  ML* fml = const_cast<ML*>(fmlclass->GetML());
  ML* aml = const_cast<ML*>(amlclass->GetML());
  if (!sml || !fml || !aml) dserror("Not using ML for Fluid, Structure or Ale");

  // number of grids in structure, fluid and ale
  snlevel_ = sml->ML_num_actual_levels;
  fnlevel_ = fml->ML_num_actual_levels;
  anlevel_ = aml->ML_num_actual_levels;
  // min of number of grids over fields
  minnlevel_ = min(snlevel_,fnlevel_);
  minnlevel_ = min(minnlevel_,anlevel_);
  
  // check whether we have enough iteration and damping factors
  if ((int)pciter_.size() < minnlevel_ ||
      (int)pcomega_.size() < minnlevel_)
    dserror("You need at least %d values of PCITER and PCOMEGA in input file",minnlevel_);
  
  if (!myrank)
  {
    printf("Setting up FSIAMG: snlevel %d fnlevel %d anlevel %d minnlevel %d\n",
           snlevel_,fnlevel_,anlevel_,minnlevel_);
  }

  Ass_.resize(snlevel_);
  Pss_.resize(snlevel_-1);
  Rss_.resize(snlevel_-1);
  Sss_.resize(snlevel_);

  Aff_.resize(fnlevel_);
  Pff_.resize(fnlevel_-1);
  Rff_.resize(fnlevel_-1);
  Sff_.resize(fnlevel_);
  
  Aaa_.resize(anlevel_);
  Paa_.resize(anlevel_-1);
  Raa_.resize(anlevel_-1);
  Saa_.resize(anlevel_);
  
  ASF_.resize(minnlevel_);
  AFS_.resize(minnlevel_);
  AFA_.resize(minnlevel_);
  AAF_.resize(minnlevel_);

  //---------------------------------------------------------- timing
  Epetra_Time etime(Matrix(0,0).Comm());
  //------------------------------------------------------- Structure
  {
    // fine space matching Epetra objects
    MLAPI::Space finespace(structInnerOp.RowMap());
    if (!myrank) printf("Structure: NumGlobalElements fine space %d\n",structInnerOp.RowMap().NumGlobalElements());
    
    // extract transfer operator P,R from ML
    MLAPI::Space fspace;
    MLAPI::Space cspace;
    MLAPI::Operator P;
    MLAPI::Operator R;
    for (int i=1; i<snlevel_; ++i)
    {
      ML_Operator* Pml = &(sml->Pmat[i]);
      if (i==1) fspace = finespace;
      else      fspace.Reshape(-1,Pml->outvec_leng);
      cspace.Reshape(-1,Pml->invec_leng);
      P.Reshape(cspace,fspace,Pml,false);
      if (SisPetrovGalerkin)
      {
        ML_Operator* Rml = &(sml->Rmat[i-1]);
        R.Reshape(fspace,cspace,Rml,false);
      }
      else 
        R = MLAPI::GetTranspose(P);
      Pss_[i-1] = P;
      Rss_[i-1] = R;
    }
    // extract matrix A from ML
    MLAPI::Space space;
    for (int i=0; i<snlevel_; ++i)
    {
      ML_Operator* Aml = &(sml->Amat[i]);
      if (i==0) space = finespace;
      else      space.Reshape(-1,Aml->invec_leng);
      MLAPI::Operator A(space,space,Aml,false);
      Ass_[i] = A;
    }
  }

  //------------------------------------------------------- Fluid
  {
    // fine space matching Epetra objects
    MLAPI::Space finespace(fluidInnerOp.RowMap());
    if (!myrank) printf("Fluid    : NumGlobalElements fine space %d\n",fluidInnerOp.RowMap().NumGlobalElements());
    
    // extract transfer operator P,R from ML
    MLAPI::Space fspace;
    MLAPI::Space cspace;
    MLAPI::Operator P;
    MLAPI::Operator R;
    for (int i=1; i<fnlevel_; ++i)
    {
      ML_Operator* Pml = &(fml->Pmat[i]);
      if (i==1) fspace = finespace;
      else      fspace.Reshape(-1,Pml->outvec_leng);
      cspace.Reshape(-1,Pml->invec_leng);
      P.Reshape(cspace,fspace,Pml,false);
      if (FisPetrovGalerkin)
      {
        ML_Operator* Rml = &(fml->Rmat[i-1]);
        R.Reshape(fspace,cspace,Rml,false);
      }
      else 
      {
        R = MLAPI::GetTranspose(P);
      }
      Pff_[i-1] = P;
      Rff_[i-1] = R;
    }
    // extract matrix A from ML
    MLAPI::Space space;
    for (int i=0; i<fnlevel_; ++i)
    {
      ML_Operator* Aml = &(fml->Amat[i]);
      if (i==0) space = finespace;
      else      space.Reshape(-1,Aml->invec_leng);
      MLAPI::Operator A(space,space,Aml,false);
      Aff_[i] = A;
    }
  }

  //------------------------------------------------------- Ale
  {
    // fine space matching Epetra objects
    MLAPI::Space finespace(aleInnerOp.RowMap());
    if (!myrank) printf("Ale      : NumGlobalElements fine space %d\n",aleInnerOp.RowMap().NumGlobalElements());
    
    // extract transfer operator P,R from ML
    MLAPI::Space fspace;
    MLAPI::Space cspace;
    MLAPI::Operator P;
    MLAPI::Operator R;
    for (int i=1; i<anlevel_; ++i)
    {
      ML_Operator* Pml = &(aml->Pmat[i]);
      if (i==1) fspace = finespace;
      else      fspace.Reshape(-1,Pml->outvec_leng);
      cspace.Reshape(-1,Pml->invec_leng);
      P.Reshape(cspace,fspace,Pml,false);
      if (AisPetrovGalerkin)
      {
        ML_Operator* Rml = &(aml->Rmat[i-1]);
        R.Reshape(fspace,cspace,Rml,false);
      }
      else
        R = MLAPI::GetTranspose(P);
        
      Paa_[i-1] = P;
      Raa_[i-1] = R;
    }
    // extract matrix A from ML
    MLAPI::Space space;
    for (int i=0; i<anlevel_; ++i)
    {
      ML_Operator* Aml = &(aml->Amat[i]);
      if (i==0) space = finespace;
      else      space.Reshape(-1,Aml->invec_leng);
      MLAPI::Operator A(space,space,Aml,false);
      Aaa_[i] = A;
    }
  }

  // wrap the off-diagonal matrix blocks into MLAPI operators
  {
    MLAPI::Space dspace(Matrix(0,1).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(0,1).EpetraMatrix()->RangeMap());
    Asf_.Reshape(dspace,rspace,Matrix(0,1).EpetraMatrix().get(),false);
    ASF_[0] = Asf_;
  }
  {
    MLAPI::Space dspace(Matrix(1,0).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(1,0).EpetraMatrix()->RangeMap());
    Afs_.Reshape(dspace,rspace,Matrix(1,0).EpetraMatrix().get(),false);
    AFS_[0] = Afs_;
  }
  {
    MLAPI::Space dspace(Matrix(1,2).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(1,2).EpetraMatrix()->RangeMap());
    Afa_.Reshape(dspace,rspace,Matrix(1,2).EpetraMatrix().get(),false);
    AFA_[0] = Afa_;
  }
  {
    MLAPI::Space dspace(Matrix(2,1).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(2,1).EpetraMatrix()->RangeMap());
    Aaf_.Reshape(dspace,rspace,Matrix(2,1).EpetraMatrix().get(),false);
    AAF_[0] = Aaf_;
  }
  
  //==================== explicit FSI off-diagonal blocks on coarse levels
  RAPoffdiagonals();

  //================set up MLAPI smoothers for structure, fluid, ale on each level
  MLAPI::InverseOperator S;
  // structure
  for (int i=0; i<snlevel_-1; ++i)
  {
    Teuchos::ParameterList p;
    Teuchos::ParameterList pushlist;
    char levelstr[11];
    sprintf(levelstr,"(level %d)",i);
    Teuchos::ParameterList& subp = sparams_.sublist("smoother: list "+(string)levelstr);
    string type = "";
    SelectMLAPISmoother(type,i,subp,p,pushlist);
    if (type=="ILU") WrapILUSmoother(sml,Ass_[i],S,i);
    else             S.Reshape(Ass_[i],type,p,&pushlist);
    Sss_[i] = S;
  }
  // coarse grid:
  S.Reshape(Ass_[snlevel_-1],"Amesos-KLU");
  Sss_[snlevel_-1] = S;
  // fluid
  for (int i=0; i<fnlevel_-1; ++i)
  {
    Teuchos::ParameterList p;
    Teuchos::ParameterList pushlist;
    char levelstr[11];
    sprintf(levelstr,"(level %d)",i);
    Teuchos::ParameterList& subp = fparams_.sublist("smoother: list "+(string)levelstr);
    string type = "";
    SelectMLAPISmoother(type,i,subp,p,pushlist);
    if (type=="ILU") WrapILUSmoother(fml,Aff_[i],S,i);
    else             S.Reshape(Aff_[i],type,p,&pushlist);
    Sff_[i] = S;
  }
  // coarse grid:
  S.Reshape(Aff_[fnlevel_-1],"Amesos-KLU");
  Sff_[fnlevel_-1] = S;
  // ale
  for (int i=0; i<anlevel_-1; ++i)
  {
    Teuchos::ParameterList p;
    Teuchos::ParameterList pushlist;
    char levelstr[11];
    sprintf(levelstr,"(level %d)",i);
    Teuchos::ParameterList& subp = aparams_.sublist("smoother: list "+(string)levelstr);
    string type = "";
    SelectMLAPISmoother(type,i,subp,p,pushlist);
    if (type=="ILU") WrapILUSmoother(aml,Aaa_[i],S,i);
    else             S.Reshape(Aaa_[i],type,p,&pushlist);
    Saa_[i] = S;
  }
  // coarse grid:
  S.Reshape(Aaa_[anlevel_-1],"Amesos-KLU");
  Saa_[anlevel_-1] = S;
  
  //-------------------------------------------------------------- timing
  if (!myrank) printf("Additional FSIAMG setup time %10.5e [s]\n",etime.ElapsedTime());

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::RAPoffdiagonals()
{
  for (int i=0; i<minnlevel_-1; ++i)
  {
    //------ Asf (trouble maker)
    if (!i) RAPfine(ASF_[i+1],Rss_[i],Matrix(0,1).EpetraMatrix(),Pff_[i]);
    else    RAPcoarse(ASF_[i+1],Rss_[i],ASF_[i],Pff_[i]);
    //------ Afs (trouble maker)
    if (!i) RAPfine(AFS_[i+1],Rff_[i],Matrix(1,0).EpetraMatrix(),Pss_[i]);
    else    RAPcoarse(AFS_[i+1],Rff_[i],AFS_[i],Pss_[i]);
    //------ Afa
    if (!i) RAPfine(AFA_[i+1],Rff_[i],Matrix(1,2).EpetraMatrix(),Paa_[i]);
    else    RAPcoarse(AFA_[i+1],Rff_[i],AFA_[i],Paa_[i]);
    //------ Aaf
    if (!i) RAPfine(AAF_[i+1],Raa_[i],Matrix(2,1).EpetraMatrix(),Pff_[i]);
    else    RAPcoarse(AAF_[i+1],Raa_[i],AAF_[i],Pff_[i]);
  }
  return;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::RAPfine(
                                        MLAPI::Operator& RAP,
                                        const MLAPI::Operator& R,
                                        Teuchos::RCP<Epetra_CrsMatrix> A,
                                        const MLAPI::Operator& P)
{
  // this epetraext thingy patches the inherent ml<->epetra conflict
  EpetraExt::CrsMatrix_SolverMap transform;
  Epetra_CrsMatrix* Btrans = &(transform(*A));

  // down to the salt mines of ML....
  ML_Operator* mlB = ML_Operator_Create(MLAPI::GetML_Comm());
  ML_Operator_WrapEpetraMatrix(Btrans,mlB);
  ML_Operator* mlBP = ML_Operator_Create(MLAPI::GetML_Comm());
  ML_2matmult(mlB,P.GetML_Operator(),mlBP,ML_CSR_MATRIX);

  ML_Operator* mlRBP = ML_Operator_Create(MLAPI::GetML_Comm());
  ML_2matmult(R.GetML_Operator(),mlBP,mlRBP,ML_CSR_MATRIX);

  ML_Operator_Destroy(&mlB);
  ML_Operator_Destroy(&mlBP);
  
  // take ownership of coarse operator
  RAP.Reshape(P.GetDomainSpace(),R.GetRangeSpace(),mlRBP,true);

#if 0 // compare explicit <-> implicit coarse operator on a vector
  MLAPI::Space dspace(Btrans->DomainMap());
  MLAPI::Space rspace(Btrans->RangeMap());
  MLAPI::Operator mlapiBtrans;
  mlapiBtrans.Reshape(dspace,rspace,Btrans,false); 


  MLAPI::MultiVector in(P.GetDomainSpace(),1,true);
  in = 1.0;

  MLAPI::MultiVector outRBP;
  outRBP = RAP * in;
  cout << outRBP;

  MLAPI::MultiVector outR_B_P;
  MLAPI::MultiVector tmp;
  tmp = P * in;
  tmp = mlapiBtrans * tmp;
  outR_B_P = R * tmp;
  cout << outR_B_P;
#endif  
  
  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::RAPcoarse(
                                              MLAPI::Operator& RAP,
                                              const MLAPI::Operator& R, 
                                              const MLAPI::Operator& A,
                                              const MLAPI::Operator& P)
{
  MLAPI::Operator AP;
  // Intentionally do not use MLAPI's build in RAP product
  AP = A * P;
  RAP = R * AP;
  return;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::WrapILUSmoother(ML* ml,
                                                        MLAPI::Operator& A,
                                                        MLAPI::InverseOperator& S,
                                                        const int level)
{
  // we use pre == post smoother here, so get postsmoother from ml
  void* data = ml->post_smoother[level].smoother->data;
  Ifpack_Preconditioner* prec = static_cast<Ifpack_Preconditioner*>(data);
  // make sure this really is an Epetra_Operator
  std::string test = prec->Label();
  if (!prec->Comm().MyPID()) cout << test << endl;
  S.Reshape(prec,A,false);

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::SelectMLAPISmoother(std::string& type,
                                                            const int level,
                                                            Teuchos::ParameterList& subp,
                                                            Teuchos::ParameterList& p,
                                                            Teuchos::ParameterList& pushlist)
{
  type = subp.get("smoother: type","none");
  if (type=="none") dserror("Cannot find msoother type");
  if (type=="symmetric Gauss-Seidel" || type=="Gauss-Seidel")
  {
    const int    sweeps  = subp.get("smoother: sweeps",1);
    const double damping = subp.get("smoother: damping factor",1.0);
    p.set("smoother: sweeps",sweeps);
    p.set("smoother: damping factor",damping); 
  }
  else if (type=="IFPACK")
  {
    type                 = subp.get("smoother: ifpack type","ILU");
    const double lof     = subp.get<double>("smoother: ifpack level-of-fill",0);
    const double damping = subp.get("smoother: damping factor",1.0);
    p.set("smoother: ilu fill",(int)lof); 
    p.set("smoother: damping factor",damping);
    p.set("schwarz: reordering type","rcm");
    pushlist.set("ILU: sweeps", (int)lof);
    pushlist.set("fact: absolute threshold",0.0);
    pushlist.set("fact: ict level-of-fill",lof);
    pushlist.set("fact: ilut level-of-fill",lof);
    pushlist.set("schwarz: reordering type","rcm");
  }
  else if (type=="MLS")
  {
    const int poly = subp.get("smoother: MLS polynomial order",3);
    p.set("smoother: MLS polynomial order",poly);
  }
  else if (type=="Amesos-KLU"); // nothing to do
  else if (type=="Amesos-Superludist") dserror("No SuperLUDist support in MLAPI");
  else dserror("Smoother not recognized");
  p.set("relaxation: zero starting solution", false);
  return;
}                                                            

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::ExplicitBlockVcycle(
                                                    const int level,
                                                    const int nlevel,
                                                    MLAPI::MultiVector& mlsy,
                                                    MLAPI::MultiVector& mlfy,
                                                    MLAPI::MultiVector& mlay,
                                                    const MLAPI::MultiVector& mlsx,
                                                    const MLAPI::MultiVector& mlfx,
                                                    const MLAPI::MultiVector& mlax) const
{
  mlsy = 0.0; 
  mlay = 0.0; 
  mlfy = 0.0;

  // coarsest common level
  if (level==nlevel-1)
  {
    // on the coarsest common level, use a BlockRichardson that
    // uses "the leftover peak" of the individual multigrid hierachies instead of
    // the simple smoothing schemes within the level. In case a field does not
    // have a remaining "leftover peak", direct solve will be called
    // automatically.
    ExplicitBlockGaussSeidelSmoother(level,mlsy,mlfy,mlay,mlsx,mlfx,mlax,true);
    return;
  }
  
  //-------------------------- presmoothing block Gauss Seidel
  ExplicitBlockGaussSeidelSmoother(level,mlsy,mlfy,mlay,mlsx,mlfx,mlax,false);
  
  //----------------------------------- coarse level residuals
  // structure
  // sxc = Rss_[level] * ( mlsx - Ass_[level] * mlsy - Asf[level] * mlfy)
  MLAPI::MultiVector sxc;
  sxc = mlsx - Ass_[level] * mlsy;
  sxc = sxc - ASF_[level] * mlfy;
  sxc = Rss_[level] * sxc;
  
  // ale
  // axc = Raa_[level] * ( mlax - Aaa_[level] * mlay - Aaf[level] * mlfy)
  MLAPI::MultiVector axc;
  axc = mlax - Aaa_[level] * mlay;
  axc = axc - AAF_[level] * mlfy;
  axc = Raa_[level] * axc;
  
  // fluid
  // fxc = Rff_[level] * ( mlfx - Aff_[level] * mlfy - Afs[level] * mlsy - Afa[level] * mlay)
  MLAPI::MultiVector fxc;
  fxc = mlfx - Aff_[level] * mlfy;
  fxc = fxc - AFS_[level] * mlsy;
  fxc = fxc - AFA_[level] * mlay;
  fxc = Rff_[level] * fxc;

  //----------------------------------- coarse level corrections
  MLAPI::MultiVector syc(sxc.GetVectorSpace(),1,false);
  MLAPI::MultiVector ayc(axc.GetVectorSpace(),1,false);
  MLAPI::MultiVector fyc(fxc.GetVectorSpace(),1,false);

  //--------------------------------------- solve coarse problem
  ExplicitBlockVcycle(level+1,nlevel,syc,fyc,ayc,sxc,fxc,axc);

  //------------------------------- prolongate coarse correction
  MLAPI::MultiVector tmp;
  tmp = Pss_[level] * syc;
  mlsy.Update(1.0,tmp,1.0);
  tmp = Paa_[level] * ayc;
  mlay.Update(1.0,tmp,1.0);
  tmp = Pff_[level] * fyc;
  mlfy.Update(1.0,tmp,1.0);

  //---------------------------- postsmoothing block Gauss Seidel
  // (do NOT zero initial guess)
  ExplicitBlockGaussSeidelSmoother(level,mlsy,mlfy,mlay,mlsx,mlfx,mlax,false);

  return;
}                                                    


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
MLAPI::MultiVector FSI::OverlappingBlockMatrixFSIAMG::ProlongateMultiplyRestrict(
                                    const int                      level,
                                    const MLAPI::MultiVector&      coarse,
                                    const vector<MLAPI::Operator>& R,
                                    const MLAPI::Operator&         A,
                                    const vector<MLAPI::Operator>& P) const
{
  MLAPI::MultiVector tmp;
  if (!level) // we are on the fine grid, nothing to prolongate/restrict
  {
    tmp = A * coarse;
    return tmp;
  }

  // prolongate to fine level
  tmp = P[level-1] * coarse;
  for (int i=level-1; i>0; --i)
    tmp = P[i-1] * tmp;
  
  // multiply on fine level
  tmp = A * tmp;
  
  // restrict to coarse level
  for (int i=0; i<level; ++i)
    tmp = R[i] * tmp;

  return tmp;
}                                           

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::BlockVcycle(const int level,
                                                    const int nlevel,
                                                    MLAPI::MultiVector& mlsy,
                                                    MLAPI::MultiVector& mlfy,
                                                    MLAPI::MultiVector& mlay,
                                                    const MLAPI::MultiVector& mlsx,
                                                    const MLAPI::MultiVector& mlfx,
                                                    const MLAPI::MultiVector& mlax) const
{
  // coarsest common level
  if (level==nlevel-1)
  {
    // on the coarsest common level, use a BlockRichardson that
    // uses "the leftover peak" of the individual multigrid hierachies instead of
    // the simple smoothing schemes within the level. In case a field does not
    // have a remaining "leftover peak", direct solve will be called
    // automatically.
    mlsy = 0.0; mlay = 0.0; mlfy = 0.0;
    BlockGaussSeidelSmoother(level,mlsy,mlfy,mlay,mlsx,mlfx,mlax,true);
    return;
  }
  
  //-------------------------- presmoothing block Gauss Seidel
  mlsy = 0.0; mlay = 0.0; mlfy = 0.0;
  BlockGaussSeidelSmoother(level,mlsy,mlfy,mlay,mlsx,mlfx,mlax,false);
  
  //----------------------------------- coarse level residuals
  // structure
  // sxc = Rss_[level] * ( mlsx - Ass_[level] * mlsy - Asf[level] * mlfy)
  MLAPI::MultiVector sxc;
  sxc = mlsx - Ass_[level] * mlsy;
  sxc = sxc - ProlongateMultiplyRestrict(level,mlfy,Rss_,Asf_,Pff_);
  sxc = Rss_[level] * sxc;
  
  // ale
  // axc = Raa_[level] * ( mlax - Aaa_[level] * mlay - Aaf[level] * mlfy)
  MLAPI::MultiVector axc;
  axc = mlax - Aaa_[level] * mlay;
  axc = axc - ProlongateMultiplyRestrict(level,mlfy,Raa_,Aaf_,Pff_);
  axc = Raa_[level] * axc;
  
  // fluid
  // fxc = Rff_[level] * ( mlfx - Aff_[level] * mlfy - Afs[level] * mlsy - Afa[level] * mlay)
  MLAPI::MultiVector fxc;
  fxc = mlfx - Aff_[level] * mlfy;
  fxc = fxc - ProlongateMultiplyRestrict(level,mlsy,Rff_,Afs_,Pss_);
  fxc = fxc - ProlongateMultiplyRestrict(level,mlay,Rff_,Afa_,Paa_);
  fxc = Rff_[level] * fxc;

  //----------------------------------- coarse level corrections
  MLAPI::MultiVector syc(sxc.GetVectorSpace(),1,false);
  MLAPI::MultiVector ayc(axc.GetVectorSpace(),1,false);
  MLAPI::MultiVector fyc(fxc.GetVectorSpace(),1,false);

  //--------------------------------------- solve coarse problem
  BlockVcycle(level+1,nlevel,syc,fyc,ayc,sxc,fxc,axc);

  //------------------------------- prolongate coarse correction
  MLAPI::MultiVector tmp;
  tmp = Pss_[level] * syc;
  mlsy.Update(1.0,tmp,1.0);
  tmp = Paa_[level] * ayc;
  mlay.Update(1.0,tmp,1.0);
  tmp = Pff_[level] * fyc;
  mlfy.Update(1.0,tmp,1.0);

  //---------------------------- postsmoothing block Gauss Seidel
  // (do NOT zero initial guess)
  BlockGaussSeidelSmoother(level,mlsy,mlfy,mlay,mlsx,mlfx,mlax,false);

  return;
}                                                    


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::BlockGaussSeidelSmoother(
                                  const int level,
                                  MLAPI::MultiVector& mlsy,
                                  MLAPI::MultiVector& mlfy,
                                  MLAPI::MultiVector& mlay,
                                  const MLAPI::MultiVector& mlsx,
                                  const MLAPI::MultiVector& mlfx,
                                  const MLAPI::MultiVector& mlax,
                                  const bool amgsolve) const
{
  
  MLAPI::MultiVector sx(mlsx.GetVectorSpace(),1,false);
  MLAPI::MultiVector fx(mlfx.GetVectorSpace(),1,false);
  MLAPI::MultiVector ax(mlax.GetVectorSpace(),1,false);
  MLAPI::MultiVector sz(mlsy.GetVectorSpace(),1,false);
  MLAPI::MultiVector fz(mlfy.GetVectorSpace(),1,false);
  MLAPI::MultiVector az(mlay.GetVectorSpace(),1,false);
  
  for (int run=0; run<pciter_[level]; ++run)
  {
    // copy of original residual
    sx.Update(mlsx);
    fx.Update(mlfx);
    ax.Update(mlax);
    
    //-------------- structure block
    {
      // compute ( r - A y ) for structure row
      {
        // tmp = A_ss * mlsy
        MLAPI::MultiVector tmp;
        tmp = Ass_[level] * mlsy;
        sx.Update(-1.0,tmp,1.0);
        // tmp = R_s A_sf P_f mlfy
        tmp = ProlongateMultiplyRestrict(level,mlfy,Rss_,Asf_,Pff_);
        sx.Update(-1.0,tmp,1.0);
      }
      sz = 0.0; 
      if (!amgsolve) Sss_[level].Apply(sx,sz);
      else           Vcycle(level,snlevel_,sz,sx,Ass_,Sss_,Pss_,Rss_);
      mlsy.Update(pcomega_[level],sz,1.0);
    }
    
    //-------------------- ale block
    {
      // compute ( r - A y ) for ale row
      {
        MLAPI::MultiVector tmp;
        tmp = Aaa_[level] * mlay;
        ax.Update(-1.0,tmp,1.0);
        // tmp = R_a * A_af * P_f * mlfy
        tmp = ProlongateMultiplyRestrict(level,mlfy,Raa_,Aaf_,Pff_);
        ax.Update(-1.0,tmp,1.0);
      }
      az = 0.0;
      if (!amgsolve) Saa_[level].Apply(ax,az);
      else           Vcycle(level,anlevel_,az,ax,Aaa_,Saa_,Paa_,Raa_);
      mlay.Update(pcomega_[level],az,1.0);
    }
    
    //------------------ fluid block
    {
      // compute ( r - A y ) for fluid row
      {
        MLAPI::MultiVector tmp;
        tmp = Aff_[level] * mlfy;
        fx.Update(-1.0,tmp,1.0);
  
        // tmp = R_f A_fs P_s mlsy
        tmp = ProlongateMultiplyRestrict(level,mlsy,Rff_,Afs_,Pss_);
        fx.Update(-1.0,tmp,1.0);
      
        // tmp = R_f A_fa P_a mlay
        tmp = ProlongateMultiplyRestrict(level,mlay,Rff_,Afa_,Paa_);
        fx.Update(-1.0,tmp,1.0);
      }
      fz = 0.0;
      if (!amgsolve) Sff_[level].Apply(fx,fz);
      else           Vcycle(level,fnlevel_,fz,fx,Aff_,Sff_,Pff_,Rff_);
      mlfy.Update(pcomega_[level],fz,1.0);
    }
    
  } // for (int run=0; run<pciter_[level]; ++run)
  
  return;
}                                  

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::ExplicitBlockGaussSeidelSmoother(
                                  const int level,
                                  MLAPI::MultiVector& mlsy,
                                  MLAPI::MultiVector& mlfy,
                                  MLAPI::MultiVector& mlay,
                                  const MLAPI::MultiVector& mlsx,
                                  const MLAPI::MultiVector& mlfx,
                                  const MLAPI::MultiVector& mlax,
                                  const bool amgsolve) const
{
  MLAPI::MultiVector sx;
  MLAPI::MultiVector fx;
  MLAPI::MultiVector ax;
  MLAPI::MultiVector sz(mlsy.GetVectorSpace(),1,false);
  MLAPI::MultiVector fz(mlfy.GetVectorSpace(),1,false);
  MLAPI::MultiVector az(mlay.GetVectorSpace(),1,false);
  
  for (int run=0; run<pciter_[level]; ++run)
  {
    //-------------- structure block
    {
      // compute ( r - A y ) for structure row
      sx = mlsx - Ass_[level] * mlsy;
      sx = sx - ASF_[level] * mlfy;
      // zero initial guess
      sz = 0.0; 
      if (!amgsolve) Sss_[level].Apply(sx,sz);
      else           Vcycle(level,snlevel_,sz,sx,Ass_,Sss_,Pss_,Rss_);
      mlsy.Update(pcomega_[level],sz,1.0);
    }
    
    //-------------------- ale block
    {
      // compute ( r - A y ) for ale row
      ax = mlax - Aaa_[level] * mlay;
      ax = ax - AAF_[level] * mlfy;
      // zero initial guess
      az = 0.0;
      if (!amgsolve) Saa_[level].Apply(ax,az);
      else           Vcycle(level,anlevel_,az,ax,Aaa_,Saa_,Paa_,Raa_);
      mlay.Update(pcomega_[level],az,1.0);
    }
    
    //------------------ fluid block
    {
      // compute ( r - A y ) for fluid row
      fx = mlfx - Aff_[level] * mlfy;
      fx = fx - AFS_[level] * mlsy;
      fx = fx - AFA_[level] * mlay;
      // zero initial guess
      fz = 0.0;
      if (!amgsolve) Sff_[level].Apply(fx,fz);
      else           Vcycle(level,fnlevel_,fz,fx,Aff_,Sff_,Pff_,Rff_);
      mlfy.Update(pcomega_[level],fz,1.0);
    }
    
  } // for (int run=0; run<pciter_[level]; ++run)
  
  return;
}                                  


#if 1
/*----------------------------------------------------------------------*
strongly coupled AMG-Block-Gauss-Seidel
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::SGS(
                 const Epetra_MultiVector &X, Epetra_MultiVector &Y) const
{
  if (!structuresplit_) dserror("FSIAMG for structuresplit monoFSI only");
  if (symmetric_)       dserror("FSIAMG symmetric Block Gauss-Seidel not impl.");

  // rewrap the matrix every time as Uli shoots them irrespective
  // of whether the precond is reused or not.
  {
    MLAPI::Space dspace(Matrix(0,0).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(0,0).EpetraMatrix()->RangeMap());
    Ass_[0].Reshape(dspace,rspace,Matrix(0,0).EpetraMatrix().get(),false);
  }
  {
    MLAPI::Space dspace(Matrix(0,1).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(0,1).EpetraMatrix()->RangeMap());
    Asf_.Reshape(dspace,rspace,Matrix(0,1).EpetraMatrix().get(),false);
    ASF_[0] = Asf_;
  }
  {
    MLAPI::Space dspace(Matrix(1,0).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(1,0).EpetraMatrix()->RangeMap());
    Afs_.Reshape(dspace,rspace,Matrix(1,0).EpetraMatrix().get(),false);
    AFS_[0] = Afs_;
  }
  {
    MLAPI::Space dspace(Matrix(1,1).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(1,1).EpetraMatrix()->RangeMap());
    Aff_[0].Reshape(dspace,rspace,Matrix(1,1).EpetraMatrix().get(),false);
  }
  {
    MLAPI::Space dspace(Matrix(1,2).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(1,2).EpetraMatrix()->RangeMap());
    Afa_.Reshape(dspace,rspace,Matrix(1,2).EpetraMatrix().get(),false);
    AFA_[0] = Afa_;
  }
  {
    MLAPI::Space dspace(Matrix(2,1).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(2,1).EpetraMatrix()->RangeMap());
    Aaf_.Reshape(dspace,rspace,Matrix(2,1).EpetraMatrix().get(),false);
    AAF_[0] = Aaf_;
  }
  {
    MLAPI::Space dspace(Matrix(2,2).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(2,2).EpetraMatrix()->RangeMap());
    Aaa_[0].Reshape(dspace,rspace,Matrix(2,2).EpetraMatrix().get(),false);
  }

  const Epetra_Vector &x = Teuchos::dyn_cast<const Epetra_Vector>(X);

  // various range and domain spaces
  MLAPI::Space rsspace(Matrix(0,0).RangeMap());
  MLAPI::Space rfspace(Matrix(1,1).RangeMap());
  MLAPI::Space raspace(Matrix(2,2).RangeMap());

  MLAPI::Space dsspace(Matrix(0,0).DomainMap());
  MLAPI::Space dfspace(Matrix(1,1).DomainMap());
  MLAPI::Space daspace(Matrix(2,2).DomainMap());

  // initial guess
  Epetra_Vector& y = Teuchos::dyn_cast<Epetra_Vector>(Y);
  Teuchos::RCP<Epetra_Vector> sy = RangeExtractor().ExtractVector(y,0);
  Teuchos::RCP<Epetra_Vector> fy = RangeExtractor().ExtractVector(y,1);
  Teuchos::RCP<Epetra_Vector> ay = RangeExtractor().ExtractVector(y,2);
  MLAPI::MultiVector mlsy(rsspace,sy->Pointers());
  MLAPI::MultiVector mlfy(rfspace,fy->Pointers());
  MLAPI::MultiVector mlay(raspace,ay->Pointers());
  
  // rhs
  Teuchos::RCP<Epetra_Vector> sx = DomainExtractor().ExtractVector(x,0);
  Teuchos::RCP<Epetra_Vector> fx = DomainExtractor().ExtractVector(x,1);
  Teuchos::RCP<Epetra_Vector> ax = DomainExtractor().ExtractVector(x,2);
  MLAPI::MultiVector mlsx(dsspace,sx->Pointers());
  MLAPI::MultiVector mlfx(dfspace,fx->Pointers());
  MLAPI::MultiVector mlax(daspace,ax->Pointers());


  // run FSIAMG
  // using implicit off-diagonals on coarse levels
  //BlockVcycle(0,minnlevel_,mlsy,mlfy,mlay,mlsx,mlfx,mlax);
  // using explicit off-diagonals on coarse levels
  ExplicitBlockVcycle(0,minnlevel_,mlsy,mlfy,mlay,mlsx,mlfx,mlax);


  // Note that mlsy, mlfy, mlay are views of sy, fy, ay, respectively.
  RangeExtractor().InsertVector(*sy,0,y);
  RangeExtractor().InsertVector(*fy,1,y);
  RangeExtractor().InsertVector(*ay,2,y);
}
#endif



#if 0
/*----------------------------------------------------------------------*
MLAPI version of the identical Kuettler Block Gauss Seidel monolithic precond,
mainly for proof of concept of using extracted MG hierarchies and to achieve
same convergence / timings through MLAPI
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::SGS(
                 const Epetra_MultiVector &X, Epetra_MultiVector &Y) const
{
  if (!structuresplit_) dserror("FSIAMG for structuresplit monoFSI only");
  if (symmetric_)       dserror("FSIAMG symmetric Block Gauss-Seidel not impl.");

  // Matrix(0,0); // Ass
  // Matrix(0,1); // Asf
  // Matrix(1,0); // Afs
  // Matrix(1,1); // Aff
  // Matrix(1,2); // Afa
  // Matrix(2,1); // Aaf
  // Matrix(2,2); // Aaa
  // rewrap the matrix every time as Uli shoots them irrespective
  // of whether the precond is reused or not.
  {
    MLAPI::Space dspace(Matrix(0,0).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(0,0).EpetraMatrix()->RangeMap());
    Ass_[0].Reshape(dspace,rspace,Matrix(0,0).EpetraMatrix().get(),false);
  }
  {
    MLAPI::Space dspace(Matrix(0,1).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(0,1).EpetraMatrix()->RangeMap());
    Asf_.Reshape(dspace,rspace,Matrix(0,1).EpetraMatrix().get(),false);
  }
  {
    MLAPI::Space dspace(Matrix(1,0).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(1,0).EpetraMatrix()->RangeMap());
    Afs_.Reshape(dspace,rspace,Matrix(1,0).EpetraMatrix().get(),false);
  }
  {
    MLAPI::Space dspace(Matrix(1,1).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(1,1).EpetraMatrix()->RangeMap());
    Aff_[0].Reshape(dspace,rspace,Matrix(1,1).EpetraMatrix().get(),false);
  }
  {
    MLAPI::Space dspace(Matrix(1,2).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(1,2).EpetraMatrix()->RangeMap());
    Afa_.Reshape(dspace,rspace,Matrix(1,2).EpetraMatrix().get(),false);
  }
  {
    MLAPI::Space dspace(Matrix(2,1).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(2,1).EpetraMatrix()->RangeMap());
    Aaf_.Reshape(dspace,rspace,Matrix(2,1).EpetraMatrix().get(),false);
  }
  {
    MLAPI::Space dspace(Matrix(2,2).EpetraMatrix()->DomainMap());
    MLAPI::Space rspace(Matrix(2,2).EpetraMatrix()->RangeMap());
    Aaa_[0].Reshape(dspace,rspace,Matrix(2,2).EpetraMatrix().get(),false);
  }

  // Extract vector blocks
  // RHS
  const Epetra_Vector &x = Teuchos::dyn_cast<const Epetra_Vector>(X);

  // initial guess
  Epetra_Vector& y = Teuchos::dyn_cast<Epetra_Vector>(Y);

  // various range and domain spaces
  MLAPI::Space rsspace(Matrix(0,0).RangeMap());
  MLAPI::Space rfspace(Matrix(1,1).RangeMap());
  MLAPI::Space raspace(Matrix(2,2).RangeMap());

  MLAPI::Space dsspace(Matrix(0,0).DomainMap());
  MLAPI::Space dfspace(Matrix(1,1).DomainMap());
  MLAPI::Space daspace(Matrix(2,2).DomainMap());

  // these wrap RCP<Epetra_Vector> objects
  Teuchos::RCP<Epetra_Vector> sy = RangeExtractor().ExtractVector(y,0);
  Teuchos::RCP<Epetra_Vector> fy = RangeExtractor().ExtractVector(y,1);
  Teuchos::RCP<Epetra_Vector> ay = RangeExtractor().ExtractVector(y,2);
  MLAPI::MultiVector mlsy(rsspace,sy->Pointers());
  MLAPI::MultiVector mlfy(rfspace,fy->Pointers());
  MLAPI::MultiVector mlay(raspace,ay->Pointers());
  
  // these are original MLAPI objects
  MLAPI::MultiVector mlsz(rsspace,1,true);
  MLAPI::MultiVector mlfz(rfspace,1,true);
  MLAPI::MultiVector mlaz(raspace,1,true);
  

  // outer Richardson loop
  for (int run=0; run<iterations_; ++run)
  {
    // rhs
    Teuchos::RCP<Epetra_Vector> sx = DomainExtractor().ExtractVector(x,0);
    Teuchos::RCP<Epetra_Vector> fx = DomainExtractor().ExtractVector(x,1);
    Teuchos::RCP<Epetra_Vector> ax = DomainExtractor().ExtractVector(x,2);
    // these wrap the above RCP<Epetra_Vector> objects
    MLAPI::MultiVector mlsx(dsspace,sx->Pointers());
    MLAPI::MultiVector mlfx(dfspace,fx->Pointers());
    MLAPI::MultiVector mlax(daspace,ax->Pointers());
    
    // ----------------------------------------------------------------
    // lower Gauss-Seidel using AMG for main diag block inverses
    {
      if (run)
      {
        MLAPI::MultiVector tmp;
        tmp = Ass_[0] * mlsy;
        mlsx.Update(-1.0,tmp,1.0);
        tmp = Asf_ * mlfy;
        mlsx.Update(-1.0,tmp,1.0);
      }

      // Solve structure equations for sy with the rhs sx
      Vcycle(0,snlevel_,mlsz,mlsx,Ass_,Sss_,Pss_,Rss_);
      //mlsz = 0.0; Sss_[0].Apply(mlsx,mlsz);

      if (!run) mlsy.Update(omega_,mlsz);
      else      mlsy.Update(omega_,mlsz,1.0);
    }

    {
      // Solve ale equations for ay with the rhs ax - A(I,Gamma) sy
      if (run)
      {
        MLAPI::MultiVector tmp;
        tmp = Aaa_[0] * mlay;
        mlax.Update(-1.0,tmp,1.0);
        tmp = Aaf_ * mlfy;
        mlax.Update(-1.0,tmp,1.0);
      }

      // solve ale block
      Vcycle(0,anlevel_,mlaz,mlax,Aaa_,Saa_,Paa_,Raa_);
      //mlaz = 0.0; Saa_[0].Apply(mlax,mlaz);

      if (!run) mlay.Update(omega_,mlaz);
      else      mlay.Update(omega_,mlaz,1.0);
    }

    {
      // Solve fluid equations for fy with the rhs fx - F(I,Gamma) sy - F(Mesh) ay
      MLAPI::MultiVector tmp;
      if (run)
      {
        tmp = Aff_[0] * mlfy;
        mlfx.Update(-1.0,tmp,1.0);
      }

      tmp = Afs_ * mlsy;
      mlfx.Update(-1.0,tmp,1.0);
      tmp = Afa_ * mlay;
      mlfx.Update(-1.0,tmp,1.0);

      // solve fluid block
      Vcycle(0,fnlevel_,mlfz,mlfx,Aff_,Sff_,Pff_,Rff_,true);
      //mlfz = 0.0; Sff_[0].Apply(mlfx,mlfz);

      if (!run) mlfy.Update(omega_,mlfz);
      else      mlfy.Update(omega_,mlfz,1.0);
    }

  }

  // Note that mlsy, mlfy, mlay are views of sy, fy, ay, respectively.
  RangeExtractor().InsertVector(*sy,0,y);
  RangeExtractor().InsertVector(*fy,1,y);
  RangeExtractor().InsertVector(*ay,2,y);
}
#endif





#if 0
/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::SGS(
                 const Epetra_MultiVector &X, Epetra_MultiVector &Y) const
{
  if (!structuresplit_) dserror("FSIAMG for structuresplit monoFSI only");
  if (symmetric_)       dserror("FSIAMG symmetric Block Gauss-Seidel not impl.");
  
  const LINALG::SparseMatrix& structInnerOp = Matrix(0,0); // Ass
  const LINALG::SparseMatrix& structBoundOp = Matrix(0,1); // Asf
  const LINALG::SparseMatrix& fluidBoundOp  = Matrix(1,0); // Afs
  const LINALG::SparseMatrix& fluidInnerOp  = Matrix(1,1); // Aff
  const LINALG::SparseMatrix& fluidMeshOp   = Matrix(1,2); // Afa
  const LINALG::SparseMatrix& aleBoundOp    = Matrix(2,1); // Aaf
  const LINALG::SparseMatrix& aleInnerOp    = Matrix(2,2); // Aaa


  // Extract vector blocks
  // RHS
  const Epetra_Vector &x = Teuchos::dyn_cast<const Epetra_Vector>(X);

  // initial guess
  Epetra_Vector& y = Teuchos::dyn_cast<Epetra_Vector>(Y);

  Teuchos::RCP<Epetra_Vector> sy = RangeExtractor().ExtractVector(y,0);
  Teuchos::RCP<Epetra_Vector> fy = RangeExtractor().ExtractVector(y,1);
  Teuchos::RCP<Epetra_Vector> ay = RangeExtractor().ExtractVector(y,2);

  Teuchos::RCP<Epetra_Vector> sz = Teuchos::rcp(new Epetra_Vector(sy->Map()));
  Teuchos::RCP<Epetra_Vector> fz = Teuchos::rcp(new Epetra_Vector(fy->Map()));
  Teuchos::RCP<Epetra_Vector> az = Teuchos::rcp(new Epetra_Vector(ay->Map()));

  Teuchos::RCP<Epetra_Vector> tmpsx = Teuchos::rcp(new Epetra_Vector(DomainMap(0)));
  Teuchos::RCP<Epetra_Vector> tmpfx = Teuchos::rcp(new Epetra_Vector(DomainMap(1)));
  Teuchos::RCP<Epetra_Vector> tmpax = Teuchos::rcp(new Epetra_Vector(DomainMap(2)));

  // outer Richardson loop
  for (int run=0; run<iterations_; ++run)
  {
    // rhs
    Teuchos::RCP<Epetra_Vector> sx = DomainExtractor().ExtractVector(x,0);
    Teuchos::RCP<Epetra_Vector> fx = DomainExtractor().ExtractVector(x,1);
    Teuchos::RCP<Epetra_Vector> ax = DomainExtractor().ExtractVector(x,2);

    // ----------------------------------------------------------------
    // lower GS
    {
      if (run>0)
      {
        structInnerOp.Multiply(false,*sy,*tmpsx);
        sx->Update(-1.0,*tmpsx,1.0);
        structBoundOp.Multiply(false,*fy,*tmpsx);
        sx->Update(-1.0,*tmpsx,1.0);
      }

      // Solve structure equations for sy with the rhs sx
      structuresolver_->Solve(structInnerOp.EpetraMatrix(),sz,sx,true);

      // do Richardson iteration
      LocalBlockRichardson(structuresolver_,structInnerOp,sx,sz,tmpsx,siterations_,somega_,err_,Comm());

      if (!run) sy->Update(omega_,*sz,0.0);
      else      sy->Update(omega_,*sz,1.0);
    }

    {
      // Solve ale equations for ay with the rhs ax - A(I,Gamma) sy
      if (run>0)
      {
        aleInnerOp.Multiply(false,*ay,*tmpax);
        ax->Update(-1.0,*tmpax,1.0);

        aleBoundOp.Multiply(false,*fy,*tmpax);
        ax->Update(-1.0,*tmpax,1.0);
      }

      alesolver_->Solve(aleInnerOp.EpetraMatrix(),az,ax,true);

      if (!run) ay->Update(omega_,*az,0.0);
      else      ay->Update(omega_,*az,1.0);
    }

    {
      // Solve fluid equations for fy with the rhs fx - F(I,Gamma) sy - F(Mesh) ay

      if (run>0)
      {
        fluidInnerOp.Multiply(false,*fy,*tmpfx);
        fx->Update(-1.0,*tmpfx,1.0);
      }

      fluidBoundOp.Multiply(false,*sy,*tmpfx);
      fx->Update(-1.0,*tmpfx,1.0);
      fluidMeshOp.Multiply(false,*ay,*tmpfx);
      fx->Update(-1.0,*tmpfx,1.0);
      fluidsolver_->Solve(fluidInnerOp.EpetraMatrix(),fz,fx,true);

      LocalBlockRichardson(fluidsolver_,fluidInnerOp,fx,fz,tmpfx,fiterations_,fomega_,err_,Comm());

      if (!run) fy->Update(omega_,*fz,0.0);
      else      fy->Update(omega_,*fz,1.0);
    }

  }

  RangeExtractor().InsertVector(*sy,0,y);
  RangeExtractor().InsertVector(*fy,1,y);
  RangeExtractor().InsertVector(*ay,2,y);
}
#endif

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void FSI::OverlappingBlockMatrixFSIAMG::Vcycle(const int level,
                                               const int nlevel,
                                               MLAPI::MultiVector& z,
                                               const MLAPI::MultiVector& b,
                                               const vector<MLAPI::Operator>& A,
                                               const vector<MLAPI::InverseOperator>& S,
                                               const vector<MLAPI::Operator>& P,
                                               const vector<MLAPI::Operator>& R,
                                               const bool trigger) const
{
  // in presmoothing, the initial guess has to be zero, we do this manually here.
  // in postsmoothing, the initial guess has to be nonzero. This is tricky, as
  // SGS smoothers assume nonzero initial guess, but ILU smoothers ALWAYS assume
  // zero guess. We circumvent this by reformulating the postsmoothing step (see below)
  // such that the initial guess can be zero by hand.
  
  
  // coarse solve
  if (level==nlevel-1)
  {
    z = S[level] * b;
    return;
  }
  
  // presmoothing (initial guess = 0)
  z = 0.0;
  S[level].Apply(b,z);

  // coarse level residual and correction
  MLAPI::MultiVector bc;
  MLAPI::MultiVector zc(P[level].GetDomainSpace(),1,true);

  // compute residual and restrict to coarser level
  bc = R[level] * ( b - A[level] * z );
  
  // solve coarse problem
  Vcycle(level+1,nlevel,zc,bc,A,S,P,R);
  
  // prolongate correction
  z = z + P[level] * zc;
  
  
  // postsmoothing (initial guess != 0 !!)
  MLAPI::MultiVector r(b.GetVectorSpace(),true);
  MLAPI::MultiVector dz(b.GetVectorSpace(),true);
  r = A[level] * z;
  r.Update(1.0,b,-1.0);
  S[level].Apply(r,dz);
  z.Update(1.0,dz,1.0);
  

  return;
}                


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
const char* FSI::OverlappingBlockMatrixFSIAMG::Label() const
{
  return "FSI::OverlappingBlockMatrix_FSIAMG";
}

#endif


#ifdef CCADISCRET

#include "drt_globalproblem.H"
#include "drt_utils.H"

#ifdef PARALLEL
#include "Epetra_MpiComm.h"
#else
#include "Epetra_SerialComm.h"
#endif

extern struct _MATERIAL *mat;

// the instances
vector<RefCountPtr<DRT::Problem> > DRT::Problem::instances_;


RefCountPtr<DRT::Problem> DRT::Problem::Instance(int num)
{
  if (num > static_cast<int>(instances_.size())-1)
  {
    instances_.resize(num+1);
    instances_[num] = rcp(new Problem());
  }
  return instances_[num];
}


RefCountPtr<DRT::Discretization> DRT::Problem::Dis(int fieldnum, int disnum) const
{
  return discretizations_[fieldnum][disnum];
}


void DRT::Problem::AddDis(int fieldnum, RefCountPtr<Discretization> dis)
{
  if (fieldnum > static_cast<int>(discretizations_.size())-1)
  {
    discretizations_.resize(fieldnum+1);
  }
  discretizations_[fieldnum].push_back(dis);
}

void DRT::Problem::AddMaterial(const _MATERIAL& m)
{
  if (m.m.fluid==NULL)
    dserror("invalid material added");
  material_.push_back(m); // copy!
  ActivateMaterial();     // always reset!
}

void DRT::Problem::ActivateMaterial()
{
  mat = &material_[0];
}


#endif

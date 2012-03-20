/*!----------------------------------------------------------------------*
 \file adapter_fluid_poro.cpp

 \brief Fluid field adapter for poroelasticity

 <pre>
   Maintainer: Anh-Tu Vuong
               vuong@lnm.mw.tum.de
               http://www.lnm.mw.tum.de
               089 - 289-15264
 </pre>
*----------------------------------------------------------------------*/
#ifdef CCADISCRET

/*----------------------------------------------------------------------*/
#include "adapter_fluid_poro.H"
#include "../drt_lib/drt_condition_utils.H"
#include "../linalg/linalg_utils.H"
#include "../drt_lib/drt_condition_utils.H"
#include "../drt_fluid/fluid_utils_mapextractor.H"
#include "../drt_io/io.H"
#include "../drt_fluid_ele/fluid_ele.H"

/*----------------------------------------------------------------------*
 |                                                       m.gee 06/01    |
 | general problem data                                                 |
 | global variable GENPROB genprob is defined in global_control.c       |
 *----------------------------------------------------------------------*/
extern struct _GENPROB     genprob;


/*======================================================================*/
/* constructor */
ADAPTER::FluidPoro::FluidPoro(Teuchos::RCP<Fluid> fluid)
: FluidWrapper(fluid)
{
  // make sure

  if (fluid_ == null)
    dserror("Failed to create the underlying fluid adapter");

  Discretization()->GetCondition("NoPenetration", nopencond_);
}

/*======================================================================*/
/* evaluate poroelasticity specific constraint*/
void ADAPTER::FluidPoro::EvaluateNoPenetrationCond(Teuchos::RCP<Epetra_Vector> Cond_RHS,
                                              Teuchos::RCP<LINALG::SparseMatrix> ConstraintMatrix,
                                              Teuchos::RCP<LINALG::SparseMatrix> StructVelConstraintMatrix,
                                              Teuchos::RCP< std::set<int> > condIDs,
                                              int coupltype)
{
  if (!(Discretization()->Filled())) dserror("FillComplete() was not called");
  if (!Discretization()->HaveDofs()) dserror("AssignDegreesOfFreedom() was not called");

  Discretization()->ClearState();
  Discretization()->SetState("dispnp", Dispnp());
  Discretization()->SetState(0,"velnp",Velnp());
  Discretization()->SetState(0,"gridv",GridVel());

  ConstraintMatrix->Zero();

  ParameterList params;

  // set action for elements
  params.set("action","no penetration");
  if(coupltype==0)
    params.set("coupling","fluid fluid");
  else if(coupltype==1)
  {
    params.set("coupling","fluid structure");

    StructVelConstraintMatrix->Zero();
  }
  else
    dserror("unknown coupling type for no penetration BC");

  //std::set<int> condIDs;
  condIDs->clear();

  //---------------------------------------------------------------------
  // loop through conditions and evaluate them
  //---------------------------------------------------------------------
  for (unsigned int i = 0; i < nopencond_.size(); ++i)
  {
    DRT::Condition& cond = *(nopencond_[i]);

    // elements might need condition
    params.set<RefCountPtr<DRT::Condition> >("condition", rcp(&cond,false));

    // define element matrices and vectors
    Epetra_SerialDenseMatrix elematrix1;
    Epetra_SerialDenseMatrix elematrix2;
    Epetra_SerialDenseVector elevector1;
    Epetra_SerialDenseVector elevector2;
    Epetra_SerialDenseVector elevector3;

    map<int,RefCountPtr<DRT::Element> >& geom = cond.Geometry();
    // no check for empty geometry here since in parallel computations
    // there might be processors which do not own a portion of the elements belonging
    // to the condition geometry
    map<int,RefCountPtr<DRT::Element> >::iterator curr;

    for (curr=geom.begin(); curr!=geom.end(); ++curr)
    {
      DRT::Element::LocationArray la(2);
      DRT::ELEMENTS::Fluid3Boundary* fluid = dynamic_cast<DRT::ELEMENTS::Fluid3Boundary*>(curr->second.get());
      fluid->LocationVector(*Discretization(),la,false);

      // get dimension of element matrices and vectors
      // Reshape element matrices and vectors and init to zero
      const int eledim = (int)la[0].lm_.size();
      const int eledim2 = (int)la[1].lm_.size();
      elevector1.Size(eledim);

      int eid = curr->second->Id();

      if(coupltype==0)
      {
        elematrix1.Shape(eledim,eledim);
        //---------------------------------------------------------------------
        // call the element specific evaluate method
        int err = curr->second->Evaluate(params,*Discretization(),la[0].lm_,elematrix1,elematrix2,
                                         elevector1,elevector2,elevector3);
        if (err) dserror("error while evaluating elements");

        //---------------------------------------------------------------------
        // assembly
        ConstraintMatrix->Assemble(eid,la[0].stride_,elematrix1,la[0].lm_,la[0].lmowner_);

        Teuchos::RCP<std::vector<int> > mycondIDs=params.get<Teuchos::RCP<std::vector<int> > >("mycondIDs",Teuchos::null);
        int condID=-1;
        if(mycondIDs != Teuchos::null)
          for(unsigned int i=0;i<mycondIDs->size();i++)
          {
            condID=(*mycondIDs)[i];
            if ( Discretization()->Comm().MyPID() == la[0].lmowner_[ i ] and condID!= -1)
              condIDs->insert(condID);
          }
      }
      else
      {
        elematrix1.Shape(eledim,eledim2);
        elematrix2.Shape(eledim,eledim2);
        //---------------------------------------------------------------------
        // call the element specific evaluate method
        int err = curr->second->Evaluate(params,*Discretization(),la[0].lm_,elematrix1,elematrix2,
                                         elevector1,elevector2,elevector3);
        if (err) dserror("error while evaluating elements");

        ConstraintMatrix->Assemble( eid, la[1].stride_, elematrix1, la[0].lm_, la[0].lmowner_, la[1].lm_ );
        StructVelConstraintMatrix->Assemble( eid, la[1].stride_, elematrix1, la[0].lm_, la[0].lmowner_, la[1].lm_ );
        LINALG::Assemble(*Cond_RHS,elevector1,la[0].lm_,la[0].lmowner_);
      }
    }
  }

  return;
}

/*----------------------------------------------------------------------*/
#endif  // #ifdef CCADISCRET

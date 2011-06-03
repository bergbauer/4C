
/*----------------------------------------------------------------------*/
/*!
\file  xfluidfluidresulttest.cpp

\brief tesing of fluid calculation results

<pre>
Maintainer: Shadan Shahmiri
            shahmiri@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15240
</pre>
*/
/*----------------------------------------------------------------------*/

#ifdef CCADISCRET

#include <string>

#include "xfluidfluidresulttest.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_linedefinition.H"

#ifdef PARALLEL
#include <mpi.h>
#endif

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
FLD::XFluidFluidResultTest::XFluidFluidResultTest(XFluidFluid& fluid)
{
  embfluiddis_= fluid.embdis_;
  bgfluiddis_= fluid.bgdis_;
  embfluidsol_   = fluid.state_->alevelnp_ ;
  bgfluidsol_   = fluid.state_->velnp_ ;
//  mytraction_ = fluid.calcStresses();
}
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FLD::XFluidFluidResultTest::TestNode(DRT::INPUT::LineDefinition& res, int& nerr, int& test_count)
{
   int dis;
   res.ExtractInt("DIS",dis);

   int node;
   res.ExtractInt("NODE",node);
   //node -= 1;

   if (dis == 1)
   {
     if (bgfluiddis_->HaveGlobalNode(node))
     {
         const DRT::Node* actnode = bgfluiddis_->gNode(node);

         // Test only, if actnode is a row node
         if (actnode->Owner() != bgfluiddis_->Comm().MyPID())
             return;

         double result = 0.;

         const Epetra_BlockMap& velnpmap = bgfluidsol_->Map();

         const int numdim = DRT::Problem::Instance()->ProblemSizeParams().get<int>("DIM");

     std::string position;
     res.ExtractString("POSITION",position);
         if (position=="velx")
         {
             result = (*bgfluidsol_)[velnpmap.LID(bgfluiddis_->Dof(actnode,0))];
         }
         else if (position=="vely")
         {
             result = (*bgfluidsol_)[velnpmap.LID(bgfluiddis_->Dof(actnode,1))];
         }
         else if (position=="velz")
         {
             if (numdim==2)
                 dserror("Cannot test result for velz in 2D case.");
             result = (*bgfluidsol_)[velnpmap.LID(bgfluiddis_->Dof(actnode,2))];
         }
         else if (position=="pressure")
         {
             if (numdim==2)
             {
                 if (bgfluiddis_->NumDof(actnode)<3)
                     dserror("too few dofs at node %d for pressure testing",actnode->Id());
                 result = (*bgfluidsol_)[velnpmap.LID(bgfluiddis_->Dof(actnode,2))];
             }
             else
             {
                 if (bgfluiddis_->NumDof(actnode)<4)
                     dserror("too few dofs at node %d for pressure testing",actnode->Id());
                 result = (*bgfluidsol_)[velnpmap.LID(bgfluiddis_->Dof(actnode,3))];
             }
         }
//          else if (position=="tractionx")
//              result = (*mytraction_)[(mytraction_->Map()).LID(bgfluiddis_->Dof(actnode,0))];
//          else if (position=="tractiony")
//              result = (*mytraction_)[(mytraction_->Map()).LID(bgfluiddis_->Dof(actnode,1))];
//          else if (position=="tractionz")
//          {
//              if (numdim==2)
//                  dserror("Cannot test result for tractionz in 2D case.");
//              result = (*mytraction_)[(mytraction_->Map()).LID(bgfluiddis_->Dof(actnode,2))];
//          }
         else
         {
             dserror("position '%s' not supported in fluid testing", position.c_str());
         }

         nerr += CompareValues(result, res);
         test_count++;
     }
   }
   else if(dis == 2)
   {
     if (embfluiddis_->HaveGlobalNode(node))
     {
       const DRT::Node* actnode = embfluiddis_->gNode(node);

       // Test only, if actnode is a row node
       if (actnode->Owner() != embfluiddis_->Comm().MyPID())
         return;

       double result = 0.;

       const Epetra_BlockMap& velnpmap = embfluidsol_->Map();

       const int numdim = DRT::Problem::Instance()->ProblemSizeParams().get<int>("DIM");

       std::string position;
       res.ExtractString("POSITION",position);
       if (position=="velx")
       {
         result = (*embfluidsol_)[velnpmap.LID(embfluiddis_->Dof(actnode,0))];
       }
       else if (position=="vely")
       {
         result = (*embfluidsol_)[velnpmap.LID(embfluiddis_->Dof(actnode,1))];
       }
       else if (position=="velz")
       {
         if (numdim==2)
           dserror("Cannot test result for velz in 2D case.");
         result = (*embfluidsol_)[velnpmap.LID(embfluiddis_->Dof(actnode,2))];
       }
       else if (position=="pressure")
       {
         if (numdim==2)
         {
           if (embfluiddis_->NumDof(actnode)<3)
             dserror("too few dofs at node %d for pressure testing",actnode->Id());
           result = (*embfluidsol_)[velnpmap.LID(embfluiddis_->Dof(actnode,2))];
         }
         else
         {
           if (embfluiddis_->NumDof(actnode)<4)
             dserror("too few dofs at node %d for pressure testing",actnode->Id());
           result = (*embfluidsol_)[velnpmap.LID(embfluiddis_->Dof(actnode,3))];
         }
       }
//          else if (position=="tractionx")
//             result = (*mytraction_)[(mytraction_->Map()).LID(xfluiddis_->Dof(actnode,0))];
//          else if (position=="tractiony")
//              result = (*mytraction_)[(mytraction_->Map()).LID(xfluiddis_->Dof(actnode,1))];
//          else if (position=="tractionz")
//          {
//              if (numdim==2)
//                  dserror("Cannot test result for tractionz in 2D case.");
//              result = (*mytraction_)[(mytraction_->Map()).LID(xfluiddis_->Dof(actnode,2))];
//          }
         else
         {
             dserror("position '%s' not supported in fluid testing", position.c_str());
         }

         nerr += CompareValues(result, res);
         test_count++;
     }
   }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool FLD::XFluidFluidResultTest::Match(DRT::INPUT::LineDefinition& res)
{
  return res.HaveNamed("FLUID");
}

#endif /* CCADISCRET       */

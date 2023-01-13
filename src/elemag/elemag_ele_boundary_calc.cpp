/*----------------------------------------------------------------------*/
/*! \file
\brief boundary calc base routines
\level 2

*/
/*--------------------------------------------------------------------------*/


#include "elemag_ele_boundary_calc.H"
#include "elemag_ele.H"
#include "elemag_ele_action.H"
#include "lib_node.H"
#include "lib_globalproblem.H"
#include "inpar_parameterlist_utils.H"


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::ElemagBoundaryImplInterface* DRT::ELEMENTS::ElemagBoundaryImplInterface::Impl(
    const DRT::Element* ele)
{
  switch (ele->Shape())
  {
    case DRT::Element::quad4:
    {
      return ElemagBoundaryImpl<DRT::Element::quad4>::Instance();
    }
    case DRT::Element::quad8:
    {
      return ElemagBoundaryImpl<DRT::Element::quad8>::Instance();
    }
    case DRT::Element::quad9:
    {
      return ElemagBoundaryImpl<DRT::Element::quad9>::Instance();
    }
    case DRT::Element::tri3:
    {
      return ElemagBoundaryImpl<DRT::Element::tri3>::Instance();
    }
    case DRT::Element::tri6:
    {
      return ElemagBoundaryImpl<DRT::Element::tri6>::Instance();
    }
    case DRT::Element::line2:
    {
      return ElemagBoundaryImpl<DRT::Element::line2>::Instance();
    }
    case DRT::Element::line3:
    {
      return ElemagBoundaryImpl<DRT::Element::line3>::Instance();
    }
    case DRT::Element::nurbs2:  // 1D nurbs boundary element
    {
      return ElemagBoundaryImpl<DRT::Element::nurbs2>::Instance();
    }
    case DRT::Element::nurbs3:  // 1D nurbs boundary element
    {
      return ElemagBoundaryImpl<DRT::Element::nurbs3>::Instance();
    }
    case DRT::Element::nurbs4:  // 2D nurbs boundary element
    {
      return ElemagBoundaryImpl<DRT::Element::nurbs4>::Instance();
    }
    case DRT::Element::nurbs9:  // 2D nurbs boundary element
    {
      return ElemagBoundaryImpl<DRT::Element::nurbs9>::Instance();
    }
    default:
      dserror(
          "Element shape %d (%d nodes) not activated. Just do it.", ele->Shape(), ele->NumNode());
      break;
  }
  return NULL;
}

template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::ElemagBoundaryImpl<distype>* DRT::ELEMENTS::ElemagBoundaryImpl<distype>::Instance(
    ::UTILS::SingletonAction action)
{
  static auto singleton_owner = ::UTILS::MakeSingletonOwner(
      []()
      {
        return std::unique_ptr<DRT::ELEMENTS::ElemagBoundaryImpl<distype>>(
            new DRT::ELEMENTS::ElemagBoundaryImpl<distype>());
      });

  return singleton_owner.Instance(action);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::ElemagBoundaryImpl<distype>::ElemagBoundaryImpl()
    : xyze_(true),
      funct_(true),
      deriv_(true),
      unitnormal_(true),
      velint_(true),
      drs_(0.0),
      fac_(0.0)
{
  return;
}



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::ElemagBoundaryImpl<distype>::EvaluateNeumann(DRT::ELEMENTS::ElemagBoundary* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization, DRT::Condition& condition,
    std::vector<int>& lm, Epetra_SerialDenseVector& elevec1_epetra,
    Epetra_SerialDenseMatrix* elemat1_epetra)
{
  return 0;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::ElemagBoundaryImpl<distype>::Evaluate(DRT::ELEMENTS::ElemagBoundary* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization, std::vector<int>& lm,
    Epetra_SerialDenseMatrix& elemat1_epetra, Epetra_SerialDenseMatrix& elemat2_epetra,
    Epetra_SerialDenseVector& elevec1_epetra, Epetra_SerialDenseVector& elevec2_epetra,
    Epetra_SerialDenseVector& elevec3_epetra)
{
  /* the term representing absorbing first order boundary conditions for the
   * here given problem looks like < lambda, mu > over Gamma_ext, hence it belongs
   * to the matrix Gmat evaluated at Gamma_ext. When condensing the local
   * unknowns we build K with G as summand. Hence, we can just add the terms
   * resulting from this boundary condition to K (and hence G)
   */
  const ELEMAG::Action action = DRT::INPUT::get<ELEMAG::Action>(params, "action");
  switch (action)
  {
    case ELEMAG::calc_abc:
    {
      const int* nodeids = ele->NodeIds();

      DRT::Element* parent = ele->ParentElement();
      Teuchos::RCP<DRT::FaceElement>* faces = parent->Faces();
      bool same = false;
      for (int i = 0; i < parent->NumFace(); ++i)
      {
        const int* nodeidsfaces = faces[i]->NodeIds();

        if (faces[i]->NumNode() != ele->NumNode()) break;

        for (int j = 0; j < ele->NumNode(); ++j)
        {
          if (nodeidsfaces[j] == nodeids[j])
            same = true;
          else
          {
            same = false;
            break;
          }
        }
        if (same == true)
        {
          // i is the number we were searching for!!!!
          params.set<int>("face", i);
          ele->ParentElement()->Evaluate(params, discretization, lm, elemat1_epetra, elemat2_epetra,
              elevec1_epetra, elevec2_epetra, elevec3_epetra);
          // break;
        }
      }
      if (same == false && (faces[0]->NumNode() != ele->NumNode()))
      {
        // in this case we have a three dimensional problem and the absorbing boundary condition on
        // a line and not on a surface. hence, we have to evaluate the abc term only at a part of
        // the face. here, we want to figure, which part!
        int elenode = ele->NumNode();
        int face = -1;
        if (elenode != 2)
          dserror("absorbing line in 3d not implemented for higher order geometry approximation");
        // find the first face which contains the line!
        for (int i = 0; i < parent->NumFace(); ++i)
        {
          const int* nodeidsfaces = faces[i]->NodeIds();

          int count = 0;
          for (int j = 0; j < faces[i]->NumNode(); ++j)
          {
            for (int n = 0; n < elenode; ++n)
            {
              count += (nodeidsfaces[j] == nodeids[n]);
            }
          }
          if (count == elenode)
          {
            same = true;
            face = i;
            params.set<int>("face", i);

            const int* nodeidsface = faces[face]->NodeIds();
            Teuchos::RCP<std::vector<int>> indices = Teuchos::rcp(new std::vector<int>(elenode));
            for (int j = 0; j < faces[face]->NumNode(); ++j)
            {
              for (int n = 0; n < elenode; ++n)
                if (nodeids[n] == nodeidsface[j]) (*indices)[n] = j;
            }
            params.set<Teuchos::RCP<std::vector<int>>>("nodeindices", indices);
            ele->ParentElement()->Evaluate(params, discretization, lm, elemat1_epetra,
                elemat2_epetra, elevec1_epetra, elevec2_epetra, elevec3_epetra);
          }
        }
        if (same == false) dserror("no face contains absorbing line");
        // now, we know which face contains the line, but we have to tell the element, which nodes
        // we are talking about! therefore, we create a vector of ints and this vector stores the
        // relevant nodes, for example: the line has two nodes, we store which position these nodes
        // have in the face element
      }
      // if (same == false)
      //  dserror("either nodeids are sorted differently or a boundary element does not know to whom
      //  it belongs");
      break;
    }
    case ELEMAG::bd_integrate:
    {
      const int* nodeids = ele->NodeIds();

      DRT::Element* parent = ele->ParentElement();
      Teuchos::RCP<DRT::FaceElement>* faces = parent->Faces();
      bool same = false;
      for (int i = 0; i < parent->NumFace(); ++i)
      {
        const int* nodeidsfaces = faces[i]->NodeIds();

        if (faces[i]->NumNode() != ele->NumNode()) break;
        /*
        for(int j=0; j<ele->NumNode(); ++j)
        {
          if(nodeidsfaces[j]==nodeids[j])
            same = true;
          else
          {
            same = false;
            break;
          }
        }
        */
        int count = 0;
        for (int j = 0; j < ele->NumNode(); ++j)
        {
          for (int k = 0; k < ele->NumNode(); ++k)
          {
            if (nodeidsfaces[j] == nodeids[k]) count++;
          }
        }
        if (count == ele->NumNode()) same = true;


        if (same == true)
        {
          // i is the number we were searching for!!!!
          params.set<int>("face", i);
          ele->ParentElement()->Evaluate(params, discretization, lm, elemat1_epetra, elemat2_epetra,
              elevec1_epetra, elevec2_epetra, elevec3_epetra);
          break;
        }
      }

      break;
    }
    default:
      dserror("unknown action %d provided to ElemagBoundaryImpl", action);
      break;
  }
  return 0;
}

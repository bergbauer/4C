/*!----------------------------------------------------------------------
\file elemag_ele_intfaces_calc.cpp

\brief Integrate internal face terms on an internal faces element

<pre>
\level 2

\maintainer Luca Berardocco
            berardocco@lnm.mw.tum.de
            089 - 289-15244
</pre>
*/
/*----------------------------------------------------------------------*/
#include <Teuchos_TimeMonitor.hpp>

#include "elemag_ele_action.H"
#include "elemag_ele_intfaces_calc.H"

#include "../linalg/linalg_utils.H"

#include "../drt_lib/drt_discret_faces.H"

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
DRT::ELEMENTS::ElemagIntFaceImplInterface* DRT::ELEMENTS::ElemagIntFaceImplInterface::Impl(
    const DRT::Element* ele)
{
  switch (ele->Shape())
  {
    case DRT::Element::quad4:
    {
      return ElemagIntFaceImpl<DRT::Element::quad4>::Instance();
    }
    case DRT::Element::quad8:
    {
      return ElemagIntFaceImpl<DRT::Element::quad8>::Instance();
    }
    case DRT::Element::quad9:
    {
      return ElemagIntFaceImpl<DRT::Element::quad9>::Instance();
    }
    case DRT::Element::tri3:
    {
      return ElemagIntFaceImpl<DRT::Element::tri3>::Instance();
    }
    case DRT::Element::tri6:
    {
      return ElemagIntFaceImpl<DRT::Element::tri6>::Instance();
    }
    case DRT::Element::line2:
    {
      return ElemagIntFaceImpl<DRT::Element::line2>::Instance();
    }
    case DRT::Element::line3:
    {
      return ElemagIntFaceImpl<DRT::Element::line3>::Instance();
    }
    default:
      dserror(
          "Element shape %d (%d nodes) not activated. Just do it.", ele->Shape(), ele->NumNode());
      break;
  }
  return NULL;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::ElemagIntFaceImpl<distype>* DRT::ELEMENTS::ElemagIntFaceImpl<distype>::Instance(
    bool create)
{
  static ElemagIntFaceImpl<distype>* instance;
  if (create)
  {
    if (instance == NULL) instance = new ElemagIntFaceImpl<distype>();
  }
  else
  {
    if (instance != NULL) delete instance;
    instance = NULL;
  }
  return instance;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::ElemagIntFaceImpl<distype>::Done()
{
  // delete this pointer! Afterwards we have to go! But since this is a
  // cleanup call, we can do it this way.
  Instance(false);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
DRT::ELEMENTS::ElemagIntFaceImpl<distype>::ElemagIntFaceImpl()
{
  return;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
void DRT::ELEMENTS::ElemagIntFaceImpl<distype>::AssembleInternalFacesUsingNeighborData(
    DRT::ELEMENTS::ElemagIntFace* intface,            // internal face element
    std::vector<int>& nds_master,                     // nodal dofset w.r.t. master element
    std::vector<int>& nds_slave,                      // nodal dofset w.r.t. slave element
    Teuchos::ParameterList& params,                   // parameter list
    DRT::DiscretizationFaces& discretization,         // faces discretization
    Teuchos::RCP<LINALG::SparseMatrix> systemmatrix,  // systemmatrix
    Teuchos::RCP<Epetra_Vector> systemvector          // systemvector
)
{
  return;
}



/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <DRT::Element::DiscretizationType distype>
int DRT::ELEMENTS::ElemagIntFaceImpl<distype>::EvaluateInternalFaces(
    DRT::ELEMENTS::ElemagIntFace* intface,   // internal face element
    Teuchos::ParameterList& params,          // parameter list
    DRT::Discretization& discretization,     // discretization
    std::vector<int>& patchlm,               // patch local map
    std::vector<int>& lm_masterToPatch,      // local map between master dofs and patchlm
    std::vector<int>& lm_slaveToPatch,       // local map between slave dofs and patchlm
    std::vector<int>& lm_faceToPatch,        // local map between face dofs and patchlm
    std::vector<int>& lm_masterNodeToPatch,  // local map between master nodes and nodes in patch
    std::vector<int>& lm_slaveNodeToPatch,   // local map between slave nodes and nodes in patch
    std::vector<Epetra_SerialDenseMatrix>& elemat_blocks,  // element matrix blocks
    std::vector<Epetra_SerialDenseVector>& elevec_blocks   // element vector blocks
)
{
  return 0;
}
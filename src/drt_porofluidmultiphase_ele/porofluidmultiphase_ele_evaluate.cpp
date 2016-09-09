/*----------------------------------------------------------------------*/
/*!
 \file porofluidmultiphase_ele_evaluate.cpp

 \brief evaluation methods of the porofluidmultiphase element

   \level 3

   \maintainer  Anh-Tu Vuong
                vuong@lnm.mw.tum.de
                http://www.lnm.mw.tum.de
                089 - 289-15251
 *----------------------------------------------------------------------*/


#include "../drt_porofluidmultiphase_ele/porofluidmultiphase_ele.H"

#include "../drt_inpar/inpar_parameterlist_utils.H"

#include "../drt_lib/drt_discret.H"
#include "../drt_porofluidmultiphase_ele/porofluidmultiphase_ele_factory.H"
#include "../drt_porofluidmultiphase_ele/porofluidmultiphase_ele_interface.H"
#include "../drt_porofluidmultiphase_ele/porofluidmultiphase_ele_parameter.H"
#include "../drt_porofluidmultiphase_ele/porofluidmultiphase_ele_action.H"

/*----------------------------------------------------------------------*
 |  evaluate the element (public)                           vuong 08/16 |
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::PoroFluidMultiPhase::Evaluate(
    Teuchos::ParameterList&   params,
    DRT::Discretization&      discretization,
    LocationArray&            la,
    Epetra_SerialDenseMatrix& elemat1,
    Epetra_SerialDenseMatrix& elemat2,
    Epetra_SerialDenseVector& elevec1,
    Epetra_SerialDenseVector& elevec2,
    Epetra_SerialDenseVector& elevec3)
{
  // we assume here, that numdofpernode is equal for every node within
  // the element and does not change during the computations
  const int numdofpernode = NumDofPerNode(*(Nodes()[0]));

  // check for the action parameter
  const POROFLUIDMULTIPHASE::Action action = DRT::INPUT::get<POROFLUIDMULTIPHASE::Action>(params,"action");
  switch(action)
  {
    // all physics-related stuff is included in the implementation class(es) that can
    // be used in principle inside any element (at the moment: only PoroFluidMultiPhase element)
    case POROFLUIDMULTIPHASE::calc_mat_and_rhs:
    {
      return DRT::ELEMENTS::PoroFluidMultiPhaseFactory::ProvideImpl(Shape(),numdofpernode,discretization.Name())->Evaluate(
              this,
              params,
              discretization,
              la,
              elemat1,
              elemat2,
              elevec1,
              elevec2,
              elevec3
              );
      break;
    }
    case POROFLUIDMULTIPHASE::calc_error:
    case POROFLUIDMULTIPHASE::calc_pres_and_sat:
    case POROFLUIDMULTIPHASE::calc_solidpressure:
    case POROFLUIDMULTIPHASE::recon_flux_at_nodes:
    {
      return DRT::ELEMENTS::PoroFluidMultiPhaseFactory::ProvideImpl(Shape(),numdofpernode,discretization.Name())->EvaluateService(
               this,
               params,
               discretization,
               la,
               elemat1,
               elemat2,
               elevec1,
               elevec2,
               elevec3);
      break;
    }
    case POROFLUIDMULTIPHASE::set_timestep_parameter:
    case POROFLUIDMULTIPHASE::set_general_parameter:
    // these actions have already been evaluated during element pre-evaluate
      break;
    default:
    {
      dserror("Unknown type of action '%i' for PoroFluidMultiPhase", action);
      break;
    }
  } // switch(action)

  return 0;
} //DRT::ELEMENTS::PoroFluidMultiPhase::Evaluate


/*----------------------------------------------------------------------*
 |  dummy                                                   vuong 08/16 |
 *----------------------------------------------------------------------*/
int DRT::ELEMENTS::PoroFluidMultiPhase::EvaluateNeumann(Teuchos::ParameterList& params,
    DRT::Discretization&      discretization,
    DRT::Condition&           condition,
    std::vector<int>&         lm,
    Epetra_SerialDenseVector& elevec1,
    Epetra_SerialDenseMatrix* elemat1)
{
  dserror("EvaluateNeumann for PoroFluidMultiPhase  not yet implemented!");
//    The function is just a dummy. For PoroFluidMultiPhase elements, the integration
//    integration of volume Neumann conditions (body forces) takes place
//    in the element. We need it there for potential stabilisation terms!
  return 0;
}

/*---------------------------------------------------------------------*
|  Call the element to set all basic parameter             vuong 08/16 |
*----------------------------------------------------------------------*/
void DRT::ELEMENTS::PoroFluidMultiPhaseType::PreEvaluate(
    DRT::Discretization&                  dis,
    Teuchos::ParameterList&               p,
    Teuchos::RCP<LINALG::SparseOperator>  systemmatrix1,
    Teuchos::RCP<LINALG::SparseOperator>  systemmatrix2,
    Teuchos::RCP<Epetra_Vector>           systemvector1,
    Teuchos::RCP<Epetra_Vector>           systemvector2,
    Teuchos::RCP<Epetra_Vector>           systemvector3)
{
  const POROFLUIDMULTIPHASE::Action action = DRT::INPUT::get<POROFLUIDMULTIPHASE::Action>(p,"action");

  switch(action)
  {
  case POROFLUIDMULTIPHASE::set_general_parameter:
  {
    DRT::ELEMENTS::PoroFluidMultiPhaseEleParameter::Instance(dis.Name())->SetGeneralParameters(p);

    break;
  }

  case POROFLUIDMULTIPHASE::set_timestep_parameter:
  {
    DRT::ELEMENTS::PoroFluidMultiPhaseEleParameter::Instance(dis.Name())->SetTimeStepParameters(p);

    break;
  }
  default:
    // do nothing in all other cases
    break;
  }

  return;
}



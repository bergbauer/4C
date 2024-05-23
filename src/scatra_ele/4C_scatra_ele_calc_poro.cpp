/*----------------------------------------------------------------------*/
/*! \file

 \brief evaluation class containing routines for calculation of scalar transport
        within porous medium

\level 2

 *----------------------------------------------------------------------*/

#include "4C_scatra_ele_calc_poro.hpp"

#include "4C_discretization_fem_general_extract_values.hpp"
#include "4C_fluid_rotsym_periodicbc.hpp"
#include "4C_global_data.hpp"
#include "4C_lib_discret.hpp"
#include "4C_lib_element.hpp"
#include "4C_mat_list.hpp"
#include "4C_mat_scatra.hpp"
#include "4C_mat_structporo.hpp"
#include "4C_scatra_ele.hpp"
#include "4C_scatra_ele_parameter_std.hpp"
#include "4C_scatra_ele_parameter_timint.hpp"
#include "4C_utils_singleton_owner.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 |                                                           vuong 07/14 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
DRT::ELEMENTS::ScaTraEleCalcPoro<distype>* DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::Instance(
    const int numdofpernode, const int numscal, const std::string& disname)
{
  static auto singleton_map = CORE::UTILS::MakeSingletonMap<std::string>(
      [](const int numdofpernode, const int numscal, const std::string& disname)
      {
        return std::unique_ptr<ScaTraEleCalcPoro<distype>>(
            new ScaTraEleCalcPoro<distype>(numdofpernode, numscal, disname));
      });

  return singleton_map[disname].Instance(
      CORE::UTILS::SingletonAction::create, numdofpernode, numscal, disname);
}

/*----------------------------------------------------------------------*
 |                                                           vuong 07/14 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::ScaTraEleCalcPoro(
    const int numdofpernode, const int numscal, const std::string& disname)
    : DRT::ELEMENTS::ScaTraEleCalc<distype>::ScaTraEleCalc(numdofpernode, numscal, disname),
      xyze0_(true),
      eporosity_(true),
      isnodalporosity_(false)
{
  // initialization of diffusion manager (override initialization in base class)
  my::diffmanager_ = Teuchos::rcp(new ScaTraEleDiffManagerPoro(my::numscal_));

  return;
}

// /*----------------------------------------------------------------------*
// * Action type: Evaluate                                    vuong 07/14 |
// *----------------------------------------------------------------------*/
// template <CORE::FE::CellType distype>
// int DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::Evaluate(
//  DRT::Element*              ele,
//  Teuchos::ParameterList&    params,
//  DRT::Discretization&       discretization,
//  const std::vector<int>&    lm,
//  CORE::LINALG::SerialDenseMatrix&  elemat1_epetra,
//  CORE::LINALG::SerialDenseMatrix&  elemat2_epetra,
//  CORE::LINALG::SerialDenseVector&  elevec1_epetra,
//  CORE::LINALG::SerialDenseVector&  elevec2_epetra,
//  CORE::LINALG::SerialDenseVector&  elevec3_epetra
//  )
//{
//  // check for the action parameter
//  const SCATRA::Action action = CORE::UTILS::GetAsEnum<SCATRA::Action>(params,"action");
//  switch(action)
//  {
//    case SCATRA::calc_scatra_mono_odblock_mesh:
//    {
//      return EvaluateODMesh(
//          ele,
//          params,
//          discretization,
//          lm,
//          elemat1_epetra,
//          elemat2_epetra,
//          elevec1_epetra,
//          elevec2_epetra,
//          elevec3_epetra
//          );
//      break;
//    }
//    case SCATRA::calc_scatra_mono_odblock_fluid:
//    {
//      return EvaluateODFluid(
//          ele,
//          params,
//          discretization,
//          lm,
//          elemat1_epetra,
//          elemat2_epetra,
//          elevec1_epetra,
//          elevec2_epetra,
//          elevec3_epetra
//          );
//      break;
//    }
//    default:
//    {
//      return my::Evaluate(
//          ele,
//          params,
//          discretization,
//          lm,
//          elemat1_epetra,
//          elemat2_epetra,
//          elevec1_epetra,
//          elevec2_epetra,
//          elevec3_epetra
//          );
//      break;
//    }
//  }
//
//  //you should no turn up here -> return error code
//  return -1;
//}

/*----------------------------------------------------------------------*
 | evaluate action                                          vuong 07/15 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
int DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::EvaluateAction(DRT::Element* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization,
    const SCATRA::Action& action, DRT::Element::LocationArray& la,
    CORE::LINALG::SerialDenseMatrix& elemat1_epetra,
    CORE::LINALG::SerialDenseMatrix& elemat2_epetra,
    CORE::LINALG::SerialDenseVector& elevec1_epetra,
    CORE::LINALG::SerialDenseVector& elevec2_epetra,
    CORE::LINALG::SerialDenseVector& elevec3_epetra)
{
  // determine and evaluate action
  switch (action)
  {
    case SCATRA::Action::calc_total_and_mean_scalars:
    {
      // get flag for inverting
      bool inverting = params.get<bool>("inverting");

      // need current scalar vector
      // -> extract local values from the global vectors
      Teuchos::RCP<const Epetra_Vector> phinp = discretization.GetState("phinp");
      if (phinp == Teuchos::null) FOUR_C_THROW("Cannot get state vector 'phinp'");
      CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nen_, 1>>(*phinp, my::ephinp_, la[0].lm_);

      extract_element_and_node_values_poro(ele, params, discretization, la);

      // calculate scalars and domain integral
      CalculateScalars(ele, elevec1_epetra, inverting, false);

      break;
    }
    default:
      return my::EvaluateAction(ele, params, discretization, action, la, elemat1_epetra,
          elemat2_epetra, elevec1_epetra, elevec2_epetra, elevec3_epetra);
      break;
  }

  return 0;
}

/*----------------------------------------------------------------------*
 | read element coordinates                                 vuong 10/14 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
void DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::read_element_coordinates(const DRT::Element* ele)
{
  // call base class
  my::read_element_coordinates(ele);

  // copy initial node position
  xyze0_ = my::xyze_;

  return;
}

/*----------------------------------------------------------------------*
 | extract element based or nodal values                     ehrl 12/13 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
void DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::extract_element_and_node_values(DRT::Element* ele,
    Teuchos::ParameterList& params, DRT::Discretization& discretization,
    DRT::Element::LocationArray& la)
{
  extract_element_and_node_values_poro(ele, params, discretization, la);

  my::extract_element_and_node_values(ele, params, discretization, la);

  return;
}

/*----------------------------------------------------------------------*
 | extract element based or nodal values                     ehrl 12/13 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
void DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::extract_element_and_node_values_poro(
    DRT::Element* ele, Teuchos::ParameterList& params, DRT::Discretization& discretization,
    DRT::Element::LocationArray& la)
{
  // get number of dofset associated with velocity related dofs
  const int ndsvel = my::scatrapara_->NdsVel();

  // get velocity values at nodes
  const Teuchos::RCP<const Epetra_Vector> convel =
      discretization.GetState(ndsvel, "convective velocity field");

  // safety check
  if (convel == Teuchos::null) FOUR_C_THROW("Cannot get state vector convective velocity");

  // determine number of velocity related dofs per node
  const int numveldofpernode = la[ndsvel].lm_.size() / nen_;

  // extract pressure if applicable
  if (static_cast<unsigned>(numveldofpernode) > nsd_)
  {
    // construct location vector for pressure dofs
    std::vector<int> lmpre(nen_, -1);
    for (unsigned inode = 0; inode < nen_; ++inode)
      lmpre[inode] = la[ndsvel].lm_[inode * numveldofpernode + nsd_];

    // extract local values of pressure field from global state vector
    CORE::FE::ExtractMyValues<CORE::LINALG::Matrix<nen_, 1>>(*convel, my::eprenp_, lmpre);
  }

  // this is a hack. Check if the structure (assumed to be the dofset 1) has more DOFs than
  // dimension. If so, we assume that this is the porosity
  if (discretization.NumDof(1, ele->Nodes()[0]) == nsd_ + 1)
  {
    isnodalporosity_ = true;

    // get number of dof-set associated with velocity related dofs
    const int ndsdisp = my::scatrapara_->NdsDisp();

    Teuchos::RCP<const Epetra_Vector> disp = discretization.GetState(ndsdisp, "dispnp");

    if (disp != Teuchos::null)
    {
      std::vector<double> mydisp(la[ndsdisp].lm_.size());
      CORE::FE::ExtractMyValues(*disp, mydisp, la[ndsdisp].lm_);

      for (unsigned inode = 0; inode < nen_; ++inode)  // number of nodes
        eporosity_(inode, 0) = mydisp[nsd_ + (inode * (nsd_ + 1))];
    }
    else
      FOUR_C_THROW("Cannot get state vector displacement");
  }
  else
    isnodalporosity_ = false;

  return;
}

/*----------------------------------------------------------------------*
 |  get the material constants  (protected)                  vuong 10/14|
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
void DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::GetMaterialParams(
    const DRT::Element* ele,      //!< the element we are dealing with
    std::vector<double>& densn,   //!< density at t_(n)
    std::vector<double>& densnp,  //!< density at t_(n+1) or t_(n+alpha_F)
    std::vector<double>& densam,  //!< density at t_(n+alpha_M)
    double& visc,                 //!< fluid viscosity
    const int iquad               //!< id of current gauss point
)
{
  // calculate gauss point porosity from fluid and solid and (potentially) scatra solution
  ComputePorosity(ele);

  // get the material
  Teuchos::RCP<CORE::MAT::Material> material = ele->Material();

  // get diffusivity / diffusivities
  if (material->MaterialType() == CORE::Materials::m_matlist)
  {
    const Teuchos::RCP<const MAT::MatList>& actmat =
        Teuchos::rcp_dynamic_cast<const MAT::MatList>(material);
    if (actmat->NumMat() < my::numscal_) FOUR_C_THROW("Not enough materials in MatList.");

    for (int k = 0; k < my::numscal_; ++k)
    {
      int matid = actmat->MatID(k);
      Teuchos::RCP<CORE::MAT::Material> singlemat = actmat->MaterialById(matid);

      my::Materials(singlemat, k, densn[k], densnp[k], densam[k], visc, iquad);
    }
  }
  else
    my::Materials(material, 0, densn[0], densnp[0], densam[0], visc, iquad);

  return;
}  // ScaTraEleCalcPoro::GetMaterialParams

/*----------------------------------------------------------------------*
 |                                                           vuong 07/14 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
void DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::MatScaTra(
    const Teuchos::RCP<const CORE::MAT::Material> material,  //!< pointer to current material
    const int k,                                             //!< id of current scalar
    double& densn,                                           //!< density at t_(n)
    double& densnp,  //!< density at t_(n+1) or t_(n+alpha_F)
    double& densam,  //!< density at t_(n+alpha_M)
    double& visc,    //!< fluid viscosity
    const int iquad  //!< id of current gauss point
)
{
  if (iquad == -1)
    FOUR_C_THROW("no gauss point given for evaluation of scatra material. Check your input file.");

  // read the porosity from the diffusion manager
  const double porosity = DiffManager()->GetPorosity(k);

  const Teuchos::RCP<const MAT::ScatraMat>& actmat =
      Teuchos::rcp_dynamic_cast<const MAT::ScatraMat>(material);

  //  if(k<NO_CONVECTION_NR)
  {
    // set diffusivity (scaled with porosity)
    SetDiffusivity(actmat, k, porosity);

    // set densities (scaled with porosity)
    SetDensities(porosity, densn, densnp, densam);
  }
  //  else
  //  {
  //    // set diffusivity (scaled with porosity)
  //    SetDiffusivity(actmat,k,1.0);
  //
  //    // set densities (scaled with porosity)
  //    SetDensities(1.0,densn,densnp,densam);
  //  }

  return;
}  // ScaTraEleCalcPoro<distype>::MatScaTra

/*----------------------------------------------------------------------*
 |                                                           vuong 07/14 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
inline void DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::SetDiffusivity(
    const Teuchos::RCP<const MAT::ScatraMat>& material, const int k, const double scale)
{
  my::diffmanager_->SetIsotropicDiff(material->Diffusivity() * scale, k);

  return;
}

/*----------------------------------------------------------------------*
 |                                                           vuong 07/14 |
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
inline void DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::SetDensities(
    double porosity, double& densn, double& densnp, double& densam)
{
  // all densities are set to the porosity
  densn = porosity;
  densnp = porosity;
  densam = porosity;

  return;
}

/*----------------------------------------------------------------------*
 |  get the material constants  (protected)                  vuong 10/14|
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
void DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::ComputePorosity(
    const DRT::Element* ele  //!< the element we are dealing with
)
{
  double porosity = 0.0;

  if (isnodalporosity_)
  {
    porosity = eporosity_.Dot(my::funct_);
  }
  else
  {
    // gauss point displacements
    CORE::LINALG::Matrix<nsd_, 1> dispint(false);
    dispint.Multiply(my::edispnp_, my::funct_);

    //------------------------get determinant of Jacobian dX / ds
    // transposed jacobian "dX/ds"
    CORE::LINALG::Matrix<nsd_, nsd_> xjm0;
    xjm0.MultiplyNT(my::deriv_, xyze0_);

    // inverse of transposed jacobian "ds/dX"
    const double det0 = xjm0.Determinant();

    my::xjm_.MultiplyNT(my::deriv_, my::xyze_);
    const double det = my::xjm_.Determinant();

    // determinant of deformationgradient det F = det ( d x / d X ) = det (dx/ds) * ( det(dX/ds)
    // )^-1
    const double J = det / det0;

    // fluid pressure at gauss point
    const double pres = ComputePorePressure();

    // empty parameter list
    Teuchos::ParameterList params;

    if (ele->NumMaterial() < 2) FOUR_C_THROW("no secondary material available");

    // here we rely that the structure material has been added as second material
    Teuchos::RCP<MAT::StructPoro> structmat =
        Teuchos::rcp_dynamic_cast<MAT::StructPoro>(ele->Material(1));
    if (structmat == Teuchos::null) FOUR_C_THROW("cast to MAT::StructPoro failed!");

    // just evaluate the first scalar (used only in case of reactive porosity)
    Teuchos::RCP<std::vector<double>> scalars = Teuchos::rcp(new std::vector<double>(0));
    for (int k = 0; k < my::numscal_; ++k)
    {
      const double phinp = my::ephinp_[k].Dot(my::funct_);
      scalars->push_back(phinp);
    }
    params.set<Teuchos::RCP<std::vector<double>>>("scalar", scalars);

    params.set<double>("delta time", my::scatraparatimint_->Dt());

    // use structure material to evaluate porosity
    structmat->ComputePorosity(
        params, pres, J, -1, porosity, nullptr, nullptr, nullptr, nullptr, nullptr, false);
  }

  // save porosity in diffusion manager for later access
  DiffManager()->SetPorosity(porosity);

  return;
}

/*----------------------------------------------------------------------*
 |  get the material constants  (protected)                  vuong 10/14|
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
double DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::ComputePorePressure()
{
  return my::eprenp_.Dot(my::funct_);
}

/*----------------------------------------------------------------------*
|  calculate scalar(s) and domain integral                  vuong 07/15|
| (overwrites method in ScaTraEleCalc)                                 |
*----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
void DRT::ELEMENTS::ScaTraEleCalcPoro<distype>::CalculateScalars(const DRT::Element* ele,
    CORE::LINALG::SerialDenseVector& scalars, bool inverting, bool calc_grad_phi)
{
  // integration points and weights
  const CORE::FE::IntPointsAndWeights<nsd_ele_> intpoints(
      SCATRA::DisTypeToOptGaussRule<distype>::rule);

  // integration loop
  for (int iquad = 0; iquad < intpoints.IP().nquad; ++iquad)
  {
    const double fac = my::eval_shape_func_and_derivs_at_int_point(intpoints, iquad);

    // calculate gauss point porosity from fluid and solid and (potentially) scatra solution
    ComputePorosity(ele);

    // calculate integrals of (inverted) scalar(s) and domain
    if (inverting)
    {
      for (unsigned i = 0; i < nen_; i++)
      {
        const double fac_funct_i = fac * my::funct_(i);
        for (int k = 0; k < my::numscal_; k++)
        {
          const double porosity = DiffManager()->GetPorosity(k);
          if (std::abs(my::ephinp_[k](i, 0)) > 1e-14)
            scalars[k] += fac_funct_i / (my::ephinp_[k](i, 0) * porosity);
          else
            FOUR_C_THROW("Division by zero");
        }
        // for domain volume
        scalars[my::numscal_] += fac_funct_i;
      }
    }
    else
    {
      for (unsigned i = 0; i < nen_; i++)
      {
        const double fac_funct_i = fac * my::funct_(i);
        for (int k = 0; k < my::numscal_; k++)
        {
          const double porosity = DiffManager()->GetPorosity(k);
          scalars[k] += fac_funct_i * my::ephinp_[k](i, 0) * porosity;
        }
        // for domain volume
        scalars[my::numscal_] += fac_funct_i;
      }
    }
  }  // loop over integration points

  return;
}  // ScaTraEleCalc::CalculateScalars

// template classes

// 1D elements
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::line2>;
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::line3>;

// 2D elements
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::tri3>;
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::tri6>;
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::quad4>;
// template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::quad8>;
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::quad9>;

// 3D elements
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::hex8>;
// template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::hex20>;
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::hex27>;
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::tet4>;
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::tet10>;
// template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::wedge6>;
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::pyramid5>;
template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::nurbs9>;
// template class DRT::ELEMENTS::ScaTraEleCalcPoro<CORE::FE::CellType::nurbs27>;

FOUR_C_NAMESPACE_CLOSE

/*----------------------------------------------------------------------*/
/*! \file

\brief utility class supporting element evaluation for concentrated electrolytes

\level 2

 */
/*----------------------------------------------------------------------*/
#include "4C_scatra_ele_utils_elch_scl.hpp"

#include "4C_mat_elchmat.hpp"
#include "4C_mat_elchphase.hpp"
#include "4C_mat_scl.hpp"
#include "4C_scatra_ele_calc_elch_scl.hpp"
#include "4C_utils_singleton_owner.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
DRT::ELEMENTS::ScaTraEleUtilsElchScl<distype>*
DRT::ELEMENTS::ScaTraEleUtilsElchScl<distype>::Instance(
    const int numdofpernode, const int numscal, const std::string& disname)
{
  static auto singleton_map = CORE::UTILS::MakeSingletonMap<std::string>(
      [](const int numdofpernode, const int numscal, const std::string& disname)
      {
        return std::unique_ptr<ScaTraEleUtilsElchScl<distype>>(
            new ScaTraEleUtilsElchScl<distype>(numdofpernode, numscal, disname));
      });

  return singleton_map[disname].Instance(
      CORE::UTILS::SingletonAction::create, numdofpernode, numscal, disname);
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
DRT::ELEMENTS::ScaTraEleUtilsElchScl<distype>::ScaTraEleUtilsElchScl(
    const int numdofpernode, const int numscal, const std::string& disname)
    : mydiffcond::ScaTraEleUtilsElchDiffCond(numdofpernode, numscal, disname)
{
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
void DRT::ELEMENTS::ScaTraEleUtilsElchScl<distype>::MatElchMat(
    Teuchos::RCP<const CORE::MAT::Material> material, const std::vector<double>& concentrations,
    const double temperature, Teuchos::RCP<ScaTraEleDiffManagerElchScl> diffmanager,
    INPAR::ELCH::DiffCondMat& diffcondmat)
{
  // cast material to electrolyte material
  const auto elchmat = Teuchos::rcp_static_cast<const MAT::ElchMat>(material);

  // safety check
  if (elchmat->NumPhase() != 1)
    FOUR_C_THROW("Can only have a single electrolyte phase at the moment!");

  // extract electrolyte phase
  const auto elchphase = elchmat->PhaseById(elchmat->PhaseID(0));

  if (elchphase->MaterialType() == CORE::Materials::m_elchphase)
  {
    // evaluate electrolyte phase
    MatElchPhase(elchphase, concentrations, temperature, diffmanager, diffcondmat);
  }
  else
    FOUR_C_THROW("Invalid material type!");
}
template <CORE::FE::CellType distype>
void DRT::ELEMENTS::ScaTraEleUtilsElchScl<distype>::MatScl(
    Teuchos::RCP<const CORE::MAT::Material> material, const double concentration,
    const double temperature, Teuchos::RCP<ScaTraEleDiffManagerElchScl> diffmanager)
{
  // cast material to Scl material
  const auto matscl = Teuchos::rcp_static_cast<const MAT::Scl>(material);

  // valence of ionic species
  diffmanager->SetValence(matscl->Valence(), 0);

  // set constant anion concentration (=bulk concentration of cations)
  diffmanager->SetBulkConc(matscl->BulkConcentration());

  // set concentration dependent conductivity of cations
  diffmanager->SetCond(matscl->ComputeConductivity(concentration, temperature));

  // derivative of electronic conductivity w.r.t. concentration
  diffmanager->SetConcDerivCond(
      matscl->compute_concentration_derivative_of_conductivity(concentration, temperature), 0);

  // diffusion coefficient of cations
  diffmanager->SetIsotropicDiff(
      matscl->compute_diffusion_coefficient(concentration, temperature), 0);

  // derivation of concentration depending diffusion coefficient wrt concentration
  diffmanager->set_conc_deriv_iso_diff_coef(
      matscl->compute_concentration_derivative_of_diffusion_coefficient(concentration, temperature),
      0, 0);

  // Susceptibility of background lattice
  diffmanager->SetSusceptibility(matscl->compute_susceptibility());

  // Permittivity based on susceptibility
  diffmanager->SetPermittivity(matscl->ComputePermittivity());

  // derivation of concentration dependent diffusion coefficient wrt temperature
  diffmanager->set_temp_deriv_iso_diff_coef(
      matscl->compute_temperature_derivative_of_diffusion_coefficient(concentration, temperature),
      0, 0);

  // concentration dependent transference number
  diffmanager->SetTransNum(matscl->compute_transference_number(concentration), 0);

  // derivation of concentration dependent transference number wrt all ionic species
  diffmanager->SetDerivTransNum(matscl->compute_first_deriv_trans(concentration), 0, 0);

  // derivative of electronic conductivity w.r.t. temperature
  diffmanager->SetTempDerivCond(
      matscl->compute_temperature_derivative_of_conductivity(concentration, temperature), 0);
}
/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <CORE::FE::CellType distype>
void DRT::ELEMENTS::ScaTraEleUtilsElchScl<distype>::MatElchPhase(
    Teuchos::RCP<const CORE::MAT::Material> material, const std::vector<double>& concentrations,
    const double temperature, Teuchos::RCP<ScaTraEleDiffManagerElchScl> diffmanager,
    INPAR::ELCH::DiffCondMat& diffcondmat)
{
  // cast material to electrolyte phase
  const auto matelchphase = Teuchos::rcp_static_cast<const MAT::ElchPhase>(material);

  // set porosity
  diffmanager->SetPhasePoro(matelchphase->Epsilon(), 0);

  // set tortuosity
  diffmanager->SetPhaseTort(matelchphase->Tortuosity(), 0);

  // loop over materials within electrolyte phase
  for (int imat = 0; imat < matelchphase->NumMat(); ++imat)
  {
    const auto elchPhaseMaterial = matelchphase->MatById(matelchphase->MatID(imat));

    switch (elchPhaseMaterial->MaterialType())
    {
      case CORE::Materials::m_scl:
      {
        diffcondmat = INPAR::ELCH::diffcondmat_scl;
        MatScl(elchPhaseMaterial, concentrations[0], temperature, diffmanager);
        break;
      }
      default:
      {
        FOUR_C_THROW("Invalid material type!");
        break;
      }
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/

// template classes
// 1D elements
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::line2>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::line3>;

// 2D elements
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::quad4>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::quad8>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::quad9>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::tri3>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::tri6>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::nurbs3>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::nurbs9>;

// 3D elements
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::hex8>;
// template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::hex20>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::hex27>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::tet4>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::tet10>;
// template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::wedge6>;
template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::pyramid5>;
// template class DRT::ELEMENTS::ScaTraEleUtilsElchScl<CORE::FE::CellType::nurbs27>;

FOUR_C_NAMESPACE_CLOSE

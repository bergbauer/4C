/*----------------------------------------------------------------------*/
/*! \file

\brief Mixture rule for growth and remodeling simulations with homogenized constrained mixtures

\level 3


*/
/*----------------------------------------------------------------------*/
#include "mixture_rule_simple.H"
#include <Epetra_ConfigDefs.h>
#include <Epetra_SerialDenseMatrix.h>
#include <bits/c++config.h>
#include <cmath>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_RCP.hpp>
#include <algorithm>
#include <utility>
#include "../drt_lib/drt_dserror.H"
#include "../drt_mat/matpar_material.H"
#include "mixture_constituent.H"
#include <functional>

// forward declarations
namespace DRT
{
  class PackBuffer;
}

MIXTURE::PAR::SimpleMixtureRule::SimpleMixtureRule(const Teuchos::RCP<MAT::PAR::Material>& matdata)
    : MixtureRule(matdata),
      initial_reference_density_(matdata->GetDouble("DENS")),
      mass_fractions_(*matdata->Get<std::vector<double>>("MASSFRAC"))
{
  // check, whether the mass frac sums up to 1
  double sum = 0.0;
  for (double massfrac : mass_fractions_) sum += massfrac;

  if (std::abs(1.0 - sum) > 1e-8) dserror("Mass fractions don't sum up to 1, which is unphysical.");
}

std::unique_ptr<MIXTURE::MixtureRule> MIXTURE::PAR::SimpleMixtureRule::CreateRule()
{
  return std::unique_ptr<MIXTURE::SimpleMixtureRule>(new MIXTURE::SimpleMixtureRule(this));
}

MIXTURE::SimpleMixtureRule::SimpleMixtureRule(MIXTURE::PAR::SimpleMixtureRule* params)
    : MixtureRule(params), params_(params)
{
}

void MIXTURE::SimpleMixtureRule::Evaluate(const LINALG::Matrix<3, 3>& F,
    const LINALG::Matrix<6, 1>& E_strain, Teuchos::ParameterList& params,
    LINALG::Matrix<6, 1>& S_stress, LINALG::Matrix<6, 6>& cmat, const int gp, const int eleGID)
{
  // define temporary matrices
  static LINALG::Matrix<6, 1> cstress;
  static LINALG::Matrix<6, 6> ccmat;

  // This is the simplest mixture rule
  // Just iterate over all constituents and add all stress/cmat contributions
  for (std::size_t i = 0; i < Constituents().size(); ++i)
  {
    MixtureConstituent& constituent = *Constituents()[i];
    cstress.Clear();
    ccmat.Clear();
    constituent.Evaluate(F, E_strain, params, cstress, ccmat, gp, eleGID);

    // Add stress contribution to global stress
    // In this basic mixture rule, the mass fractions do not change
    double constituent_density = params_->initial_reference_density_ * params_->mass_fractions_[i];
    S_stress.Update(constituent_density, cstress, 1.0);
    cmat.Update(constituent_density, ccmat, 1.0);
  }
}
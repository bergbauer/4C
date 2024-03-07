/*----------------------------------------------------------------------------*/
/*! \file
\brief implements a mirco contact constitutive law
\level 3

*----------------------------------------------------------------------*/


#include "baci_contact_constitutivelaw_mirco_contactconstitutivelaw.hpp"

#include "baci_global_data.hpp"
#include "baci_linalg_serialdensematrix.hpp"
#include "baci_linalg_serialdensevector.hpp"
#include "baci_mat_par_bundle.hpp"

#ifdef BACI_WITH_MIRCO

#include <mirco_evaluate.h>
#include <mirco_topology.h>
#include <mirco_topologyutilities.h>

#include <vector>

BACI_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
CONTACT::CONSTITUTIVELAW::MircoConstitutiveLawParams::MircoConstitutiveLawParams(
    const Teuchos::RCP<const CONTACT::CONSTITUTIVELAW::Container> container)
    : CONTACT::CONSTITUTIVELAW::Parameter(container),
      firstmatid_(*container->Get<double>("FirstMatID")),
      secondmatid_(*container->Get<double>("SecondMatID")),
      lateralLength_(*container->Get<double>("LateralLength")),
      resolution_(*container->Get<double>("Resolution")),
      randomTopologyFlag_(*container->Get<double>("RandomTopologyFlag")),
      randomSeedFlag_(*container->Get<double>("RandomSeedFlag")),
      randomGeneratorSeed_(*container->Get<double>("RandomGeneratorSeed")),
      tolerance_(*container->Get<double>("Tolerance")),
      maxIteration_(*container->Get<double>("MaxIteration")),
      warmStartingFlag_(*container->Get<double>("WarmStartingFlag")),
      finiteDifferenceFraction_(*container->Get<double>("FiniteDifferenceFraction")),
      activeGapTolerance_(*container->Get<double>("ActiveGapTolerance")),
      topologyFilePath_(*(container->Get<std::string>("TopologyFilePath")))
{
  this->SetParameters();
}
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<CONTACT::CONSTITUTIVELAW::ConstitutiveLaw>
CONTACT::CONSTITUTIVELAW::MircoConstitutiveLawParams::CreateConstitutiveLaw()
{
  return Teuchos::rcp(new CONTACT::CONSTITUTIVELAW::MircoConstitutiveLaw(this));
}
/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
CONTACT::CONSTITUTIVELAW::MircoConstitutiveLaw::MircoConstitutiveLaw(
    CONTACT::CONSTITUTIVELAW::MircoConstitutiveLawParams* params)
    : params_(params)
{
}
void CONTACT::CONSTITUTIVELAW::MircoConstitutiveLawParams::SetParameters()
{
  // retrieve problem instance to read from
  const int probinst = GLOBAL::Problem::Instance()->Materials()->GetReadFromProblem();

  // for the sake of safety
  if (GLOBAL::Problem::Instance(probinst)->Materials() == Teuchos::null)
    dserror("List of materials cannot be accessed in the global problem instance.");
  // yet another safety check
  if (GLOBAL::Problem::Instance(probinst)->Materials()->Num() == 0)
    dserror("List of materials in the global problem instance is empty.");

  // retrieve validated input line of material ID in question
  Teuchos::RCP<MAT::PAR::Material> firstmat =
      GLOBAL::Problem::Instance(probinst)->Materials()->ById(GetFirstMatID());
  Teuchos::RCP<MAT::PAR::Material> secondmat =
      GLOBAL::Problem::Instance(probinst)->Materials()->ById(GetSecondMatID());

  const double E1 = *firstmat->Get<double>("YOUNG");
  const double E2 = *secondmat->Get<double>("YOUNG");
  const double nu1 = *firstmat->Get<double>("NUE");
  const double nu2 = *secondmat->Get<double>("NUE");

  compositeYoungs_ = pow(((1 - pow(nu1, 2)) / E1 + (1 - pow(nu2, 2)) / E2), -1);

  gridSize_ = lateralLength_ / (pow(2, resolution_) + 1);

  // Correction factor vector
  // These are the correction factors to calculate the elastic compliance of the micro-scale contact
  // constitutive law for various resolutions. These constants are taken from Table 1 of Bonari et
  // al. (2020). https://doi.org/10.1007/s00466-019-01791-3
  std::vector<double> alpha_con{0.778958541513360, 0.805513388666376, 0.826126871395416,
      0.841369158110513, 0.851733020725652, 0.858342234203154, 0.862368243479785,
      0.864741597831785};
  const double alpha = alpha_con[resolution_ - 1];
  elasticComplianceCorrection_ = lateralLength_ * compositeYoungs_ / alpha;

  const int iter = int(ceil((lateralLength_ - (gridSize_ / 2)) / gridSize_));
  meshgrid_ = Teuchos::Ptr(new std::vector<double>(iter));
  MIRCO::CreateMeshgrid(*meshgrid_, iter, gridSize_);
}
/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
double CONTACT::CONSTITUTIVELAW::MircoConstitutiveLaw::Evaluate(
    double gap, CONTACT::RoughNode* cnode)
{
  if (gap + params_->GetOffset() > 0.0)
  {
    dserror("You should not be here. The Evaluate function is only tested for active nodes. ");
  }
  if (-(gap + params_->GetOffset()) < params_->GetActiveGapTolerance())
  {
    return 0.0;
  }

  double pressure = 0.0;
  MIRCO::Evaluate(pressure, -(gap + params_->GetOffset()), params_->GetLateralLength(),
      params_->GetGridSize(), params_->GetTolerance(), params_->GetMaxIteration(),
      params_->GetCompositeYoungs(), params_->GetWarmStartingFlag(),
      params_->GetComplianceCorrection(), *cnode->GetTopology(), cnode->GetMaxTopologyHeight(),
      *params_->GetMeshGrid());

  return (-1 * pressure);
}  // end of mirco_coconstlaw evaluate
/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
double CONTACT::CONSTITUTIVELAW::MircoConstitutiveLaw::EvaluateDeriv(
    double gap, CONTACT::RoughNode* cnode)
{
  if (gap + params_->GetOffset() > 0.0)
  {
    dserror("You should not be here. The Evaluate function is only tested for active nodes.");
  }
  if (-(gap + params_->GetOffset()) < params_->GetActiveGapTolerance())
  {
    return 0.0;
  }

  double pressure1 = 0.0;
  double pressure2 = 0.0;
  // using backward difference approach
  MIRCO::Evaluate(pressure1, -1.0 * (gap + params_->GetOffset()), params_->GetLateralLength(),
      params_->GetGridSize(), params_->GetTolerance(), params_->GetMaxIteration(),
      params_->GetCompositeYoungs(), params_->GetWarmStartingFlag(),
      params_->GetComplianceCorrection(), *cnode->GetTopology(), cnode->GetMaxTopologyHeight(),
      *params_->GetMeshGrid());
  MIRCO::Evaluate(pressure2,
      -(1 - params_->GetFiniteDifferenceFraction()) * (gap + params_->GetOffset()),
      params_->GetLateralLength(), params_->GetGridSize(), params_->GetTolerance(),
      params_->GetMaxIteration(), params_->GetCompositeYoungs(), params_->GetWarmStartingFlag(),
      params_->GetComplianceCorrection(), *cnode->GetTopology(), cnode->GetMaxTopologyHeight(),
      *params_->GetMeshGrid());
  return ((pressure1 - pressure2) /
          (-(params_->GetFiniteDifferenceFraction()) * (gap + params_->GetOffset())));
}

BACI_NAMESPACE_CLOSE

#endif

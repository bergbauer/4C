/*----------------------------------------------------------------------*/
/*! \file
\brief Weickenmeier active skeletal muscle material model

\level 3
*/
/*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 |  Headers                                                             |
 *----------------------------------------------------------------------*/
#include "muscle_weickenmeier.H"
#include "../drt_lib/standardtypes_cpp.H"
#include "../drt_lib/drt_linedefinition.H"
#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/voigt_notation.H"
#include "../drt_matelast/elast_aniso_structuraltensor_strategy.H"
#include "../drt_mat/elasthyper_service.H"
#include "../drt_mat/material_service.H"
#include "../drt_mat/matpar_bundle.H"
#include "Epetra_SerialDenseSolver.h"

/*----------------------------------------------------------------------*
 | Constructor                                                          |
 *----------------------------------------------------------------------*/
MAT::PAR::Muscle_Weickenmeier::Muscle_Weickenmeier(Teuchos::RCP<MAT::PAR::Material> matdata)
    : Parameter(matdata),
      alpha_(matdata->GetDouble("ALPHA")),
      beta_(matdata->GetDouble("BETA")),
      gamma_(matdata->GetDouble("GAMMA")),
      kappa_(matdata->GetDouble("KAPPA")),
      omega0_(matdata->GetDouble("OMEGA0")),
      Na_(matdata->GetDouble("ACTMUNUM")),
      muTypesNum_(matdata->GetInt("MUTYPESNUM")),
      I_(*(matdata->Get<std::vector<double>>("INTERSTIM"))),
      rho_(*(matdata->Get<std::vector<double>>("FRACACTMU"))),
      F_(*(matdata->Get<std::vector<double>>("FTWITCH"))),
      T_(*(matdata->Get<std::vector<double>>("TTWITCH"))),
      lambdaMin_(matdata->GetDouble("LAMBDAMIN")),
      lambdaOpt_(matdata->GetDouble("LAMBDAOPT")),
      dotLambdaMMin_(matdata->GetDouble("DOTLAMBDAMIN")),
      ke_(matdata->GetDouble("KE")),
      kc_(matdata->GetDouble("KC")),
      actTimesNum_(matdata->GetInt("ACTTIMESNUM")),
      actTimes_(*(matdata->Get<std::vector<double>>("ACTTIMES"))),
      actIntervalsNum_(matdata->GetInt("ACTINTERVALSNUM")),
      actValues_(*(matdata->Get<std::vector<double>>("ACTVALUES"))),
      density_(matdata->GetDouble("DENS"))
{
  // error handling for parameter ranges
  // passive material parameters
  if (alpha_ <= 0.0) dserror("Material parameter ALPHA must be greater zero");
  if (beta_ <= 0.0) dserror("Material parameter BETA must be greater zero");
  if (gamma_ <= 0.0) dserror("Material parameter GAMMA must be greater zero");
  if (omega0_ < 0.0 || omega0_ > 1.0) dserror("Material parameter OMEGA0 must be in [0;1]");

  // active material parameters
  // stimulation frequency dependent parameters
  if (Na_ < 0.0)
    dserror(
        "Material parameter ACTMUNUM (# of active motor units per undeformed cross-sectional "
        "area) must be postive or zero");

  double sumrho = 0.0;
  for (int iMU = 0; iMU < muTypesNum_; ++iMU)
  {
    if (I_[iMU] < 0.0)
      dserror("Material parameter INTERSTIM (interstimulus interval) must be postive or zero");
    if (rho_[iMU] < 0.0)
      dserror(
          "Material parameter FRACACTMU (fractions of motor unit types) must be postive or "
          "zero");
    sumrho += rho_[iMU];
    if (F_[iMU] < 0.0) dserror("Material parameter FTWITCH (twitch force) must be postive or zero");
    if (T_[iMU] < 0.0)
      dserror("Material parameter TTWITCH (twitch contraction time) must be postive or zero");
  }

  if (muTypesNum_ > 1 && sumrho != 1.0)
    dserror("Sum of fractions of motor unit types must equal one");

  // stretch dependent parameters
  if (lambdaMin_ <= 0.0) dserror("Material parameter LAMBDAMIN must be postive");
  if (lambdaOpt_ <= 0.0) dserror("Material parameter LAMBDAOPT must be postive");

  // velocity dependent parameters
  // realistically ke and kc are positive, but not necessarily restricted to positive values, so not
  // checked yet

  // prescribed activation in time intervals
  if (actTimesNum_ != int(actTimes_.size()))
    dserror("Number of activation times ACTTIMES must be equal to ACTTIMESNUM");
  if (actIntervalsNum_ != int(actValues_.size()))
    dserror(
        "Number of activation values ACTVALUES must be equal to number of activation intervals "
        "ACTINTERVALSNUM");
  if (actTimesNum_ != actIntervalsNum_ + 1)
    dserror(
        "Number of activation times ACTTIMESNUM must be one smaller than number of activation "
        "intervals ACTINTERVALSNUM");
}

/*---------------------------------------------------------------------------*
 | Create material instance of matching type with parameters                 |
 *---------------------------------------------------------------------------*/
Teuchos::RCP<MAT::Material> MAT::PAR::Muscle_Weickenmeier::CreateMaterial()
{
  return Teuchos::rcp(new MAT::Muscle_Weickenmeier(this));
}

/*---------------------------------------------------------------------------*
 | Define static class member                                                |
 *---------------------------------------------------------------------------*/
MAT::Muscle_WeickenmeierType MAT::Muscle_WeickenmeierType::instance_;

/*---------------------------------------------------------------------------*
 *---------------------------------------------------------------------------*/
DRT::ParObject* MAT::Muscle_WeickenmeierType::Create(const std::vector<char>& data)
{
  MAT::Muscle_Weickenmeier* mu_we = new MAT::Muscle_Weickenmeier();
  mu_we->Unpack(data);
  return mu_we;
}

/*----------------------------------------------------------------------*
 | Constructor (empty material object)                                  |
 *----------------------------------------------------------------------*/
MAT::Muscle_Weickenmeier::Muscle_Weickenmeier()
    : params_(NULL),
      t_tot_(0),
      anisotropy_(),
      anisotropyExtension_(1, 0.0, 0,
          Teuchos::rcp<MAT::ELASTIC::StructuralTensorStrategyBase>(
              new MAT::ELASTIC::StructuralTensorStrategyStandard(nullptr)),
          {0})
{
}

/*----------------------------------------------------------------------*
 | Constructor (with given material parameters)                         |
 *----------------------------------------------------------------------*/
MAT::Muscle_Weickenmeier::Muscle_Weickenmeier(MAT::PAR::Muscle_Weickenmeier* params)
    : params_(params),
      t_tot_(0),
      anisotropy_(),
      anisotropyExtension_(1, 0.0, 0,
          Teuchos::rcp<MAT::ELASTIC::StructuralTensorStrategyBase>(
              new MAT::ELASTIC::StructuralTensorStrategyStandard(nullptr)),
          {0})
{
  // initialize total simulation time
  t_tot_ = 0.0;

  // initialize fiber directions and structural tensor
  anisotropyExtension_.RegisterNeededTensors(
      FiberAnisotropyExtension<1>::FIBER_VECTORS | FiberAnisotropyExtension<1>::STRUCTURAL_TENSOR);

  // register anisotropy extension to global anisotropy
  anisotropy_.RegisterAnisotropyExtension(anisotropyExtension_);
}

/*----------------------------------------------------------------------*
 | Pack                                                                 |
 *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::Pack(DRT::PackBuffer& data) const
{
  DRT::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data, type);

  // matid
  int matid = -1;
  if (params_ != NULL) matid = params_->Id();  // in case we are in post-process mode
  AddtoPack(data, matid);

  anisotropyExtension_.PackAnisotropy(data);
}

/*----------------------------------------------------------------------*
 | Unpack                                                               |
 *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::Unpack(const std::vector<char>& data)
{
  // make sure we have a pristine material
  params_ = NULL;

  std::vector<char>::size_type position = 0;
  // extract type
  int type = 0;
  ExtractfromPack(position, data, type);
  if (type != UniqueParObjectId()) dserror("wrong instance type data");

  // matid and recover params_
  int matid;
  ExtractfromPack(position, data, matid);
  if (DRT::Problem::Instance()->Materials() != Teuchos::null)
  {
    if (DRT::Problem::Instance()->Materials()->Num() != 0)
    {
      const unsigned int probinst = DRT::Problem::Instance()->Materials()->GetReadFromProblem();
      MAT::PAR::Parameter* mat =
          DRT::Problem::Instance(probinst)->Materials()->ParameterById(matid);
      if (mat->Type() == MaterialType())
        params_ = static_cast<MAT::PAR::Muscle_Weickenmeier*>(mat);
      else
        dserror("Type of parameter material %d does not fit to calling type %d", mat->Type(),
            MaterialType());
    }
  }

  anisotropyExtension_.UnpackAnisotropy(data, position);

  if (position != data.size()) dserror("Mismatch in size of data %d <-> %d", data.size(), position);
}

/*----------------------------------------------------------------------*
 | Setup                                                                |
 *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::Setup(int numgp, DRT::INPUT::LineDefinition* linedef)
{
  // Read anisotropy
  anisotropy_.SetNumberOfGaussPoints(numgp);
  anisotropy_.ReadAnisotropyFromElement(linedef);

  return;
}

/*----------------------------------------------------------------------*
 | Postsetup                                                            |
 *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::PostSetup(Teuchos::ParameterList& params, const int eleGID)
{
  anisotropy_.ReadAnisotropyFromParameterList(params);
}

/*----------------------------------------------------------------------*
 | Update                                                               |
 *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::Update(LINALG::Matrix<3, 3> const& defgrd, int const gp,
    Teuchos::ParameterList& params, int const eleGID)
{
  return;
}

/*----------------------------------------------------------------------*
 | Evaluate                                                             |
 *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::Evaluate(const LINALG::Matrix<3, 3>* defgrd,
    const LINALG::Matrix<6, 1>* glstrain, Teuchos::ParameterList& params,
    LINALG::Matrix<6, 1>* stress, LINALG::Matrix<6, 6>* cmat, const int gp, const int eleGID)
{
  // save current simulation time
  t_tot_ = params.get<double>("total time");

  // time step size
  // double dt = params.get<double>("delta time"); // TODO: needed for velocity dependent
  // activation that is not implemented yet

  // blank resulting quantities
  stress->Clear();
  cmat->Clear();

  // get passive material parameters
  const double alpha = params_->alpha_;
  const double beta = params_->beta_;
  const double gamma = params_->gamma_;
  const double kappa = params_->kappa_;
  const double omega0 = params_->omega0_;

  // compute matrices
  // right Cauchy Green tensor C
  LINALG::Matrix<3, 3> C(false);                  // matrix notation
  C.MultiplyTN(1.0, *defgrd, *defgrd);            // C = F^T F
  LINALG::Matrix<6, 1> Cv(false);                 // Voigt notation
  UTILS::VOIGT::Stresses::MatrixToVector(C, Cv);  // Cv

  // inverse right Cauchy Green tensor C
  LINALG::Matrix<3, 3> invC(false);                     // matrix notation
  invC.Invert(C);                                       // invC = C^-1
  LINALG::Matrix<6, 1> invCv(false);                    // Voigt notation
  UTILS::VOIGT::Stresses::MatrixToVector(invC, invCv);  // invCv

  // structural tensor M, i.e. dyadic product of fibre directions
  LINALG::Matrix<3, 3> M = anisotropyExtension_.GetStructuralTensor(gp, 0);

  // structural tensor L = omega0/3*Identity + omegap*M
  LINALG::Matrix<3, 3> L(M);
  L.Scale(1.0 - omega0);  // omegap*M
  for (unsigned i = 0; i < 3; ++i) L(i, i) += omega0 / 3.0;

  // product C*M
  LINALG::Matrix<3, 3> CM(false);
  CM.MultiplyNN(C, M);

  // product C^T*M
  LINALG::Matrix<3, 3> transpCM(false);
  transpCM.MultiplyTN(1.0, C, M);  // C^TM = C^T*M

  // product invC*L
  LINALG::Matrix<3, 3> invCL(false);
  invCL.MultiplyNN(invC, L);

  // product invC*L*invC
  LINALG::Matrix<3, 3> invCLinvC(false);  // matrix notation
  invCLinvC.MultiplyNN(invCL, invC);
  LINALG::Matrix<6, 1> invCLinvCv(false);  // Voigt notation
  UTILS::VOIGT::Stresses::MatrixToVector(invCLinvC, invCLinvCv);

  // stretch in fibre direction lambdaM
  double lambdaM =
      sqrt(transpCM(0, 0) + transpCM(1, 1) +
           transpCM(2, 2));  // lambdaM = sqrt(C:M) = sqrt(tr(C^T M)), see Holzapfel2000, p.14

  // computation of active nominal stress Pa, and derivative derivPa
  double Pa = 0.0;
  double derivPa = 0.0;
  if (params_->muTypesNum_ != 0)
  {  // if active material
    EvaluateActiveNominalStress(params, lambdaM, Pa, derivPa);
  }  // else: Pa and derivPa remain 0.0

  // computation of activation level omegaa and derivative \frac{\partial omegaa}{\partial C}
  double omegaa = 0.0;
  LINALG::Matrix<3, 3> domegaadC(true);    // matrix notation
  LINALG::Matrix<6, 1> domegaadCv(false);  // Voigt notation
  if (Pa !=
      0.0)  // compute activation level and derivative only of active nominal stress is not zero
  {
    EvaluateActivationLevel(params, lambdaM, M, Pa, derivPa, omegaa, domegaadC);
  }
  else  // if active nominal stress is zero, material is purely passive, thus activation level and
        // derivative are zero
  {
    domegaadC.Clear();
    // omegaa remains zero as initialized
  }
  UTILS::VOIGT::Stresses::MatrixToVector(domegaadC, domegaadCv);  // convert to Voigt notation

  // compute helper matrices for further calculation
  LINALG::Matrix<3, 3> LomegaaM(L);
  LomegaaM.Update(omegaa, M, 1.0);  // LomegaaM = L + omegaa*M
  LINALG::Matrix<6, 1> LomegaaMv(false);
  UTILS::VOIGT::Stresses::MatrixToVector(LomegaaM, LomegaaMv);

  LINALG::Matrix<3, 3> LfacomegaaM(L);  // LfacomegaaM = L + fac*M
  LfacomegaaM.Update(
      (1.0 + omegaa * alpha * std::pow(lambdaM, 2.)) / (alpha * std::pow(lambdaM, 2.)), M,
      1.0);  // + fac*M
  LINALG::Matrix<6, 1> LfacomegaaMv(false);
  UTILS::VOIGT::Stresses::MatrixToVector(LfacomegaaM, LfacomegaaMv);

  LINALG::Matrix<3, 3> transpCLomegaaM(false);
  transpCLomegaaM.MultiplyTN(1.0, C, LomegaaM);  // C^T*(L+omegaa*M)
  LINALG::Matrix<6, 1> transpCLomegaaMv(false);
  UTILS::VOIGT::Stresses::MatrixToVector(transpCLomegaaM, transpCLomegaaMv);

  // generalized invariants including active material properties
  double detC = C.Determinant();  // detC = det(C)
  double I = transpCLomegaaM(0, 0) + transpCLomegaaM(1, 1) +
             transpCLomegaaM(2, 2);  // I = C:(L+omegaa*M) = tr(C^T (L+omegaa*M)) since A:B =
                                     // tr(A^T B) for real matrices
  double J = detC * (invCL(0, 0) + invCL(1, 1) +
                        invCL(2, 2));  // J = cof(C):L = tr(cof(C)^T L) = tr(adj(C) L) = tr(det(C)
                                       // C^-1 L) = det(C)*tr(C^-1 L)

  // exponential prefactors
  double expalpha = exp(alpha * (I - 1.0));
  double expbeta = exp(beta * (J - 1.0));

  // compute second Piola-Kirchhoff stress
  LINALG::Matrix<3, 3> stressM(false);
  UTILS::VOIGT::Stresses::VectorToMatrix(*stress, stressM);  // convert to matrix
  stressM.Update(expalpha, LomegaaM, 1.0);                   // add contributions
  stressM.Update(-expbeta, invCLinvC, 1.0);
  stressM.Update(J * expbeta - std::pow(detC, -kappa), invC, 1.0);
  stressM.Scale(0.5 * gamma);
  UTILS::VOIGT::Stresses::MatrixToVector(
      stressM, *stress);  // convert to Voigt notation and update stress

  // compute cmat
  cmat->MultiplyNT(alpha * expalpha, LomegaaMv, LomegaaMv, 1.0);  // add contributions
  cmat->MultiplyNT(alpha * std::pow(lambdaM, 2.) * expalpha, LfacomegaaMv, domegaadCv, 1.0);
  cmat->MultiplyNT(beta * expbeta * std::pow(detC, 2.), invCLinvCv, invCLinvCv, 1.0);
  cmat->MultiplyNT(-(beta * J + 1.) * expbeta * detC, invCv, invCLinvCv, 1.0);
  cmat->MultiplyNT(-(beta * J + 1.) * expbeta * detC, invCLinvCv, invCv, 1.0);
  cmat->MultiplyNT(
      (beta * J + 1.) * J * expbeta + kappa * std::pow(detC, -kappa), invCv, invCv, 1.0);
  AddtoCmatHolzapfelProduct(*cmat, invCv,
      -(J * expbeta -
          std::pow(detC,
              -kappa)));  // adds scalar * (invC boeppel invC) to cmat, see Holzapfel2000, p. 254
  AddtoCmatDerivInvCLInvCProduct(*cmat, invC, invCLinvC,
      -expbeta * detC);  // cmat.Update(-expbeta*detC, dinvCLinvCdCv, 1.0);
  cmat->Scale(gamma);
}

/*----------------------------------------------------------------------*
 | Evaluate active nominal stress Pact and its derivative w.r.t. the    |
 | fiber stretch                                                        |
 *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::EvaluateActiveNominalStress(
    Teuchos::ParameterList& params, const double lambdaM, double& Pa, double& derivPa)
{
  // get active microstructural parameters from params_
  const double Na = params_->Na_;
  const int muTypesNum = params_->muTypesNum_;
  const std::vector<double> I = params_->I_;
  const std::vector<double> rho = params_->rho_;
  const std::vector<double> F = params_->F_;
  const std::vector<double> T = params_->T_;

  const double lambdaMin = params_->lambdaMin_;
  const double lambdaOpt = params_->lambdaOpt_;

  const double dotLambdaMMin = params_->dotLambdaMMin_;
  const double ke = params_->ke_;
  const double kc = params_->kc_;

  // const int actTimesNum = params_->actTimesNum_; //TODO: may be needed.
  const int actIntervalsNum = params_->actIntervalsNum_;
  const std::vector<double> actTimes = params_->actTimes_;
  const std::vector<double> actValues = params_->actValues_;

  // compute twitch force of motor unit (MU) type iMU
  double t_iMU_jstim = 0;                // time of arriving stimulus jstim for MU type iMU
  double t_end = 0;                      // helper variable
  double ratiotime = 1.0;                // helper variable
  std::vector<double> sumg(muTypesNum);  // superposition of single twitches until current time
  std::vector<double> G(muTypesNum);     // gain function for MU type iMU
  std::vector<double> Ft(muTypesNum);    // twitch force for MU type iMU

  for (int iMU = 0; iMU < muTypesNum; ++iMU)
  {  // for all motor unit types i
    for (int actinterval = 0; actinterval < actIntervalsNum; ++actinterval)
    {  // for all activation intervals

      t_iMU_jstim = actTimes[actinterval];  // set time of first stimulus jstim=1 to start time of
                                            // the current activation interval

      // determine end time of activation interval
      if (t_tot_ < actTimes[actinterval + 1])
      {                  // if inside of actinterval
        t_end = t_tot_;  // set end time to current simulation time
      }
      else
      {                                     // if outside of actinterval
        t_end = actTimes[actinterval + 1];  // set end time to end time of actinterval
      }

      // superposition of single twitches
      while (t_iMU_jstim < t_end)
      {  // for all impulses from start of actinterval until determined end time of actinterval
        ratiotime = (t_tot_ - t_iMU_jstim) / T[iMU];

        // add single twitch force response for stimulus jstim and motor unit iMU scaled by
        // percentage activation prescribed in actinterval
        sumg[iMU] += actValues[actinterval] * ratiotime * F[iMU] * exp(1 - ratiotime);

        // next impulse jstim at time t_iMU_jstim after stimulation interval I
        t_iMU_jstim += I[iMU];
      }
    }  // end actinterval

    G[iMU] = (1 - exp(-2 * std::pow(T[iMU] / I[iMU], 3))) /
             (T[iMU] / I[iMU]);    // gain function for MU type iMU
    Ft[iMU] = G[iMU] * sumg[iMU];  // twitch force for MU type iMU
  }                                // end iMU

  // compute force-time/stimulation frequency dependency Poptft
  double Poptft = 0.0;
  for (int iMU = 0; iMU < muTypesNum; ++iMU)
  {
    Poptft += Na * Ft[iMU] * rho[iMU];  // sum up twitch forces for all MU types weighted by the
                                        // respective MU type fraction
  }

  // compute force-stretch dependency fxi
  double fxi = 1.0;
  double explambda = exp(((2 * lambdaMin - lambdaM - lambdaOpt) * (lambdaM - lambdaOpt)) /
                         (2 * std::pow(lambdaMin - lambdaOpt, 2)));  // prefactor
  if (lambdaM > lambdaMin)
  {
    fxi = ((lambdaM - lambdaMin) / (lambdaOpt - lambdaMin)) * explambda;
  }
  else
  {
    fxi = 0.0;
  }

  // compute force-velocity dependency fv
  double fv = 1.0;
  double dFvdDotLambdaM = 1.0;  // derivative of fv w.r.t. dotLambdaM
  double dotLambdaM = 0.0;      // TODO: include velocity dependency through BW Euler approximation
                                // dotLambdaM = (lambdaM - lambdaM_old)/dt;
  double ratioDotLambdaM = dotLambdaM / dotLambdaMMin;  // helper variable
  double dDotLambdaMdLambdaM = 0.0;  // TODO: include velocity dependency through BW Euler
                                     // approximation dDotLambdaMdLambdaM = 1/dt;
  if (dotLambdaM > 0)
  {
    fv = (1 + ratioDotLambdaM) / (1 - ke * kc * ratioDotLambdaM);
    dFvdDotLambdaM =
        dDotLambdaMdLambdaM *
        ((1 + ke * kc) / (dotLambdaMMin * std::pow((1 - ke * kc * ratioDotLambdaM), 2)));
  }
  else
  {
    fv = (1 - ratioDotLambdaM) / (1 + kc * ratioDotLambdaM);
    dFvdDotLambdaM = -dDotLambdaMdLambdaM *
                     ((1 + kc) / (dotLambdaMMin * std::pow((1 + kc * ratioDotLambdaM), 2)));
  }

  // compute active nominal stress Pa
  Pa = Poptft * fxi * fv;

  // compute derivative of active nominal stress Pa w.r.t. lambdaM
  double dFsdLamdaM = 0.0;  // derivative of fxi w.r.t. lambdaM
  if (Pa != 0)
  {
    dFsdLamdaM = ((std::pow(lambdaMin - lambdaM, 2) - std::pow(lambdaMin - lambdaOpt, 2)) /
                     std::pow(lambdaMin - lambdaOpt, 3)) *
                 explambda;
  }
  derivPa = Poptft * (fv * dFsdLamdaM + fxi * dFvdDotLambdaM * dDotLambdaMdLambdaM);

  return;
}

/*----------------------------------------------------------------------*
 | Evaluate activation level omegaa and its derivative w.r.t. the right |
 | Cauchy Green tensor                                                  |
 *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::EvaluateActivationLevel(Teuchos::ParameterList& params,
    const double lambdaM, LINALG::Matrix<3, 3>& M, double Pa, double derivPa, double& omegaa,
    LINALG::Matrix<3, 3>& domegaadC)
{
  // get passive material parameters
  const double alpha = params_->alpha_;
  const double gamma = params_->gamma_;
  const double omega0 = params_->omega0_;

  // passive part of invariant I and its first and second derivatives w.r.t. lambdaM
  double Ip = (omega0 / 3.0) * (std::pow(lambdaM, 2.) + 2.0 / lambdaM) +
              (1.0 - omega0) * std::pow(lambdaM, 2.);
  double derivIp = (omega0 / 3.0) * (2.0 * lambdaM - 2.0 / std::pow(lambdaM, 2.)) +
                   2.0 * (1.0 - omega0) * lambdaM;
  double derivderivIp = (omega0 / 3.0) * (2.0 + 4.0 * std::pow(lambdaM, 3.)) + 2.0 * (1.0 - omega0);

  // argument for Lambert W function
  double xi =
      Pa * ((2.0 * alpha * lambdaM) / gamma) *
          exp(0.5 * alpha * (2.0 - 2.0 * Ip + lambdaM * derivIp)) +
      0.5 * alpha * lambdaM * derivIp * exp(0.5 * alpha * lambdaM * derivIp);  // argument xi

  // solution W0 of principal branch of Lambert W function approximated with Halley's method
  double W0 = 1.0;                 // starting guess for solution
  double tol = 0.000000000000001;  // tolerance for numeric approximation 10^-15
  int maxiter = 100;               // maximal number of iterations
  EvaluateLambert(xi, W0, tol, maxiter);

  // derivatives of xi and W0 w.r.t. lambdaM used for activation level computation
  double derivXi = (2.0 * alpha / gamma * exp(0.5 * alpha * (2.0 - 2.0 * Ip + lambdaM * derivIp))) *
                       (Pa + lambdaM * derivPa +
                           0.5 * alpha * Pa * lambdaM * (lambdaM * derivderivIp - derivIp)) +
                   0.5 * alpha * (1.0 + 0.5 * alpha) * exp(0.5 * alpha * lambdaM * derivIp) *
                       (derivIp + lambdaM * derivderivIp);
  double derivLambert = derivXi / ((1.0 + W0) * exp(W0));

  // computation of activation level omegaa
  omegaa = W0 / (alpha * std::pow(lambdaM, 2.)) - derivIp / (2.0 * lambdaM);

  // computation of partial derivative of omegaa w.r.t. C
  domegaadC.Update(1.0, M, 1.0);  // structural tensor M
  domegaadC.Scale(
      derivLambert / (2.0 * alpha * std::pow(lambdaM, 3.)) - W0 / (alpha * std::pow(lambdaM, 4)) -
      derivderivIp / (4.0 * std::pow(lambdaM, 2.)) + derivIp / (4.0 * std::pow(lambdaM, 3.)));
}

/*----------------------------------------------------------------------*
 |  Compute the solution of the principal branch of the Lambert W       |
 |  functions using Halley's method                                     |
 *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::EvaluateLambert(double xi, double& W0, double tol, double maxiter)
{
  // Solution of Lambert W function is functional inverse of xi = W_0*exp(W_0)
  // Computation here restricted to principal branch W_0
  // Use of Halley's method according to:
  // https://blogs.mathworks.com/cleve/2013/09/02/the-lambert-w-funsolutiction/

  double W0_old =
      std::numeric_limits<double>::infinity();  // s.t. error is infinite in the beginning
  int numiter = 0;                              // number of iterations

  // Halley's method
  while ((abs(W0 - W0_old) / abs(W0) > tol) && (numiter <= maxiter))
  {
    numiter++;
    W0_old = W0;
    W0 = W0 - (W0 * exp(W0) - xi) /
                  ((exp(W0) * (W0 + 1) - (W0 + 2) * (W0 * exp(W0) - xi) / (2 * W0 + 2)));
  }

  // error handling
  if (numiter >= maxiter)
  {
    dserror(
        "Maximal number of iterations for evaluation of Lambert W function with Halley's method "
        "exceeded for tolerance %d.",
        tol);
  }

  return;
}

/*----------------------------------------------------------------------*
  |  Add the following contribution to the constitutive tensor cmat(6,6) based on
  |  - the inverse of the right Cauchy-Green vector invC
  |  - the term invC*L*invC with the structural tensor L:
  |              \partial tensor(C)^-1 tensor(L) tensor(C)^-1
  |  scalar *    --------------------------------------------
  |              \partial tensor(C)
  |
  |  wherein the derivative frac{\partial tensor(C)^-1 tensor(L) tensor(C)^-1}{\partial tensor(C)}
  is computed to: |  - 1/2 * ( Cinv_{ik} Cinv_{jm} L_{mn} Cinv_{nl} + Cinv_{il} Cinv_{jm} L_{mn}
  Cinv_{nk} + Cinv_{jk} Cinv_{im} L_{mn} Cinv_{nl} + Cinv_{jl} Cinv_{im} L_{mn} Cinv_{nk})
  *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::AddtoCmatDerivInvCLInvCProduct(LINALG::Matrix<6, 6>& cmat,
    const LINALG::Matrix<3, 3>& invC, const LINALG::Matrix<3, 3>& invCLinvC, double scalar)
{
  cmat(0, 0) += scalar * -0.5 *
                (invC(0, 0) * invCLinvC(0, 0) + invC(0, 0) * invCLinvC(0, 0) +
                    invC(0, 0) * invCLinvC(0, 0) + invC(0, 0) * invCLinvC(0, 0));
  cmat(0, 1) += scalar * -0.5 *
                (invC(0, 1) * invCLinvC(0, 1) + invC(0, 1) * invCLinvC(0, 1) +
                    invC(0, 1) * invCLinvC(0, 1) + invC(0, 1) * invCLinvC(0, 1));
  cmat(0, 2) += scalar * -0.5 *
                (invC(0, 2) * invCLinvC(0, 2) + invC(0, 2) * invCLinvC(0, 2) +
                    invC(0, 2) * invCLinvC(0, 2) + invC(0, 2) * invCLinvC(0, 2));
  cmat(0, 3) += scalar * -0.5 *
                (invC(0, 0) * invCLinvC(0, 1) + invC(0, 1) * invCLinvC(0, 0) +
                    invC(0, 0) * invCLinvC(0, 1) + invC(0, 1) * invCLinvC(0, 0));
  cmat(0, 4) += scalar * -0.5 *
                (invC(0, 1) * invCLinvC(0, 2) + invC(0, 2) * invCLinvC(0, 1) +
                    invC(0, 1) * invCLinvC(0, 2) + invC(0, 2) * invCLinvC(0, 1));
  cmat(0, 5) += scalar * -0.5 *
                (invC(0, 0) * invCLinvC(0, 2) + invC(0, 2) * invCLinvC(0, 0) +
                    invC(0, 0) * invCLinvC(0, 2) + invC(0, 2) * invCLinvC(0, 0));

  cmat(1, 0) += scalar * -0.5 *
                (invC(1, 0) * invCLinvC(1, 0) + invC(1, 0) * invCLinvC(1, 0) +
                    invC(1, 0) * invCLinvC(1, 0) + invC(1, 0) * invCLinvC(1, 0));
  cmat(1, 1) += scalar * -0.5 *
                (invC(1, 1) * invCLinvC(1, 1) + invC(1, 1) * invCLinvC(1, 1) +
                    invC(1, 1) * invCLinvC(1, 1) + invC(1, 1) * invCLinvC(1, 1));
  cmat(1, 2) += scalar * -0.5 *
                (invC(1, 2) * invCLinvC(1, 2) + invC(1, 2) * invCLinvC(1, 2) +
                    invC(1, 2) * invCLinvC(1, 2) + invC(1, 2) * invCLinvC(1, 2));
  cmat(1, 3) += scalar * -0.5 *
                (invC(1, 0) * invCLinvC(1, 1) + invC(1, 1) * invCLinvC(1, 0) +
                    invC(1, 0) * invCLinvC(1, 1) + invC(1, 1) * invCLinvC(1, 0));
  cmat(1, 4) += scalar * -0.5 *
                (invC(1, 1) * invCLinvC(1, 2) + invC(1, 2) * invCLinvC(1, 1) +
                    invC(1, 1) * invCLinvC(1, 2) + invC(1, 2) * invCLinvC(1, 1));
  cmat(1, 5) += scalar * -0.5 *
                (invC(1, 0) * invCLinvC(1, 2) + invC(1, 2) * invCLinvC(1, 0) +
                    invC(1, 0) * invCLinvC(1, 2) + invC(1, 2) * invCLinvC(1, 0));

  cmat(2, 0) += scalar * -0.5 *
                (invC(2, 0) * invCLinvC(2, 0) + invC(2, 0) * invCLinvC(2, 0) +
                    invC(2, 0) * invCLinvC(2, 0) + invC(2, 0) * invCLinvC(2, 0));
  cmat(2, 1) += scalar * -0.5 *
                (invC(2, 1) * invCLinvC(2, 1) + invC(2, 1) * invCLinvC(2, 1) +
                    invC(2, 1) * invCLinvC(2, 1) + invC(2, 1) * invCLinvC(2, 1));
  cmat(2, 2) += scalar * -0.5 *
                (invC(2, 2) * invCLinvC(2, 2) + invC(2, 2) * invCLinvC(2, 2) +
                    invC(2, 2) * invCLinvC(2, 2) + invC(2, 2) * invCLinvC(2, 2));
  cmat(2, 3) += scalar * -0.5 *
                (invC(2, 0) * invCLinvC(2, 1) + invC(2, 1) * invCLinvC(2, 0) +
                    invC(2, 0) * invCLinvC(2, 1) + invC(2, 1) * invCLinvC(2, 0));
  cmat(2, 4) += scalar * -0.5 *
                (invC(2, 1) * invCLinvC(2, 2) + invC(2, 2) * invCLinvC(2, 1) +
                    invC(2, 1) * invCLinvC(2, 2) + invC(2, 2) * invCLinvC(2, 1));
  cmat(2, 5) += scalar * -0.5 *
                (invC(2, 0) * invCLinvC(2, 2) + invC(2, 2) * invCLinvC(2, 0) +
                    invC(2, 0) * invCLinvC(2, 2) + invC(2, 2) * invCLinvC(2, 0));

  cmat(3, 0) += scalar * -0.5 *
                (invC(0, 0) * invCLinvC(1, 0) + invC(0, 0) * invCLinvC(1, 0) +
                    invC(1, 0) * invCLinvC(0, 0) + invC(1, 0) * invCLinvC(0, 0));
  cmat(3, 1) += scalar * -0.5 *
                (invC(0, 1) * invCLinvC(1, 1) + invC(0, 1) * invCLinvC(1, 1) +
                    invC(1, 1) * invCLinvC(0, 1) + invC(1, 1) * invCLinvC(0, 1));
  cmat(3, 2) += scalar * -0.5 *
                (invC(0, 2) * invCLinvC(1, 2) + invC(0, 2) * invCLinvC(1, 2) +
                    invC(1, 2) * invCLinvC(0, 2) + invC(1, 2) * invCLinvC(0, 2));
  cmat(3, 3) += scalar * -0.5 *
                (invC(0, 0) * invCLinvC(1, 1) + invC(0, 1) * invCLinvC(1, 0) +
                    invC(1, 0) * invCLinvC(0, 1) + invC(1, 1) * invCLinvC(0, 0));
  cmat(3, 4) += scalar * -0.5 *
                (invC(0, 1) * invCLinvC(1, 2) + invC(0, 2) * invCLinvC(1, 1) +
                    invC(1, 1) * invCLinvC(0, 2) + invC(1, 2) * invCLinvC(0, 1));
  cmat(3, 5) += scalar * -0.5 *
                (invC(0, 0) * invCLinvC(1, 2) + invC(0, 2) * invCLinvC(1, 0) +
                    invC(1, 0) * invCLinvC(0, 2) + invC(1, 2) * invCLinvC(0, 0));

  cmat(4, 0) += scalar * -0.5 *
                (invC(1, 0) * invCLinvC(2, 0) + invC(1, 0) * invCLinvC(2, 0) +
                    invC(2, 0) * invCLinvC(1, 0) + invC(2, 0) * invCLinvC(1, 0));
  cmat(4, 1) += scalar * -0.5 *
                (invC(1, 1) * invCLinvC(2, 1) + invC(1, 1) * invCLinvC(2, 1) +
                    invC(2, 1) * invCLinvC(1, 1) + invC(2, 1) * invCLinvC(1, 1));
  cmat(4, 2) += scalar * -0.5 *
                (invC(1, 2) * invCLinvC(2, 2) + invC(1, 2) * invCLinvC(2, 2) +
                    invC(2, 2) * invCLinvC(1, 2) + invC(2, 2) * invCLinvC(1, 2));
  cmat(4, 3) += scalar * -0.5 *
                (invC(1, 0) * invCLinvC(2, 1) + invC(1, 1) * invCLinvC(2, 0) +
                    invC(2, 0) * invCLinvC(1, 1) + invC(2, 1) * invCLinvC(1, 0));
  cmat(4, 4) += scalar * -0.5 *
                (invC(1, 1) * invCLinvC(2, 2) + invC(1, 2) * invCLinvC(2, 1) +
                    invC(2, 1) * invCLinvC(1, 2) + invC(2, 2) * invCLinvC(1, 1));
  cmat(4, 5) += scalar * -0.5 *
                (invC(1, 0) * invCLinvC(2, 2) + invC(1, 2) * invCLinvC(2, 0) +
                    invC(2, 0) * invCLinvC(1, 2) + invC(2, 2) * invCLinvC(1, 0));

  cmat(5, 0) += scalar * -0.5 *
                (invC(0, 0) * invCLinvC(2, 0) + invC(0, 0) * invCLinvC(2, 0) +
                    invC(2, 0) * invCLinvC(0, 0) + invC(2, 0) * invCLinvC(0, 0));
  cmat(5, 1) += scalar * -0.5 *
                (invC(0, 1) * invCLinvC(2, 1) + invC(0, 1) * invCLinvC(2, 1) +
                    invC(2, 1) * invCLinvC(0, 1) + invC(2, 1) * invCLinvC(0, 1));
  cmat(5, 2) += scalar * -0.5 *
                (invC(0, 2) * invCLinvC(2, 2) + invC(0, 2) * invCLinvC(2, 2) +
                    invC(2, 2) * invCLinvC(0, 2) + invC(2, 2) * invCLinvC(0, 2));
  cmat(5, 3) += scalar * -0.5 *
                (invC(0, 0) * invCLinvC(2, 1) + invC(0, 1) * invCLinvC(2, 0) +
                    invC(2, 0) * invCLinvC(0, 1) + invC(2, 1) * invCLinvC(0, 0));
  cmat(5, 4) += scalar * -0.5 *
                (invC(0, 1) * invCLinvC(2, 2) + invC(0, 2) * invCLinvC(2, 1) +
                    invC(2, 1) * invCLinvC(0, 2) + invC(2, 2) * invCLinvC(0, 1));
  cmat(5, 5) += scalar * -0.5 *
                (invC(0, 0) * invCLinvC(2, 2) + invC(0, 2) * invCLinvC(2, 0) +
                    invC(2, 0) * invCLinvC(0, 2) + invC(2, 2) * invCLinvC(0, 0));

  return;
}


/*----------------------------------------------------------------------*
  Return visualization data                                             |
 *----------------------------------------------------------------------*/
bool MAT::Muscle_Weickenmeier::VisData(
    const std::string& name, std::vector<double>& data, int numgp, int eleID)
{
  // not yet implemented but useful for visualization of e.g. activation level
  return false;
}

/*----------------------------------------------------------------------*
  Return names of visualization data                                    |
 *----------------------------------------------------------------------*/
void MAT::Muscle_Weickenmeier::VisNames(std::map<std::string, int>& names)
{
  // not yet implemented but useful for visualization of e.g. activation level
  return;
}

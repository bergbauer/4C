/*----------------------------------------------------------------------*/
/*! \file
\brief Implementation of a hyperelastic transversely isotropic material model for large strain
computations

\level 3
*/
/*----------------------------------------------------------------------*/

#include "4C_matelast_couptransverselyisotropic.hpp"

#include "4C_io_linedefinition.hpp"
#include "4C_io_pstream.hpp"
#include "4C_linalg_fixedsizematrix_voigt_notation.hpp"
#include "4C_mat_service.hpp"
#include "4C_matelast_aniso_structuraltensor_strategy.hpp"
#include "4C_material_parameter_base.hpp"
#include "4C_structure_new_elements_paramsinterface.hpp"

FOUR_C_NAMESPACE_OPEN


Mat::Elastic::PAR::CoupTransverselyIsotropic::CoupTransverselyIsotropic(
    const Teuchos::RCP<Core::Mat::PAR::Material>& matdata)
    : ParameterAniso(matdata),
      alpha_(matdata->Get<double>("ALPHA")),
      beta_(matdata->Get<double>("BETA")),
      gamma_(matdata->Get<double>("GAMMA")),
      angle_(matdata->Get<double>("ANGLE")),
      fiber_gid_(matdata->Get<int>("FIBER")),
      init_(matdata->Get<int>("INIT"))
{
  /* empty */
}

void Mat::Elastic::PAR::CoupTransverselyIsotropic::Print() const
{
  Core::IO::cout << "--- Material parameters of CoupTransverselyIsotropic\n";
  Core::IO::cout << "ALPHA           = " << alpha_ << Core::IO::endl;
  Core::IO::cout << "BETA            = " << beta_ << Core::IO::endl;
  Core::IO::cout << "GAMMA           = " << gamma_ << Core::IO::endl;
  Core::IO::cout << "ANGLE           = " << angle_ << Core::IO::endl;
  Core::IO::cout << "GLOBAL FIBER ID = " << fiber_gid_ << Core::IO::endl;
  Core::IO::cout << "INIT            = " << init_ << Core::IO::endl;
  Core::IO::cout << "----------------------------------------------------\n";
}

Mat::Elastic::CoupTransverselyIsotropic::CoupTransverselyIsotropic(my_params* params)
    : params_(params)
{
  /* empty */
}

void Mat::Elastic::CoupTransverselyIsotropic::Setup(int numgp, Input::LineDefinition* linedef)
{
  switch (params_->init_)
  {
    // path if fibers aren't given in .dat file
    case 0:
    {
      // fibers aligned in YZ-plane with gamma around Z in global cartesian cosy
      Core::LinAlg::Matrix<3, 3> Id(true);
      for (int i = 0; i < 3; i++) Id(i, i) = 1.0;
      SetFiberVecs(-1.0, Id, Id);

      break;
    }
    // path if fibers are given in .dat file
    case 1:
    {
      std::ostringstream ss;
      ss << params_->fiber_gid_;
      std::string fibername = "FIBER" + ss.str();  // FIBER Name
      // CIR-AXI-RAD nomenclature
      if (linedef->has_named("RAD") and linedef->has_named("AXI") and linedef->has_named("CIR"))
      {
        // Read in of data
        Core::LinAlg::Matrix<3, 3> locsys(true);
        ReadRadAxiCir(linedef, locsys);
        Core::LinAlg::Matrix<3, 3> Id(true);
        for (int i = 0; i < 3; i++) Id(i, i) = 1.0;
        // final setup of fiber data
        SetFiberVecs(0.0, locsys, Id);
      }
      // FIBERi nomenclature
      else if (linedef->has_named(fibername))
      {
        // Read in of data
        ReadFiber(linedef, fibername, a_);
        params_->structural_tensor_strategy()->setup_structural_tensor(a_, aa_);
      }
      // error path
      else
      {
        FOUR_C_THROW("Reading of element local cosy for anisotropic materials failed");
      }

      break;
    }
    default:
    {
      FOUR_C_THROW("INIT mode not implemented");
      exit(EXIT_FAILURE);
    }
  }
}

void Mat::Elastic::CoupTransverselyIsotropic::PackSummand(
    Core::Communication::PackBuffer& data) const
{
  add_to_pack(data, a_);
  add_to_pack(data, aa_);
}

void Mat::Elastic::CoupTransverselyIsotropic::UnpackSummand(
    const std::vector<char>& data, std::vector<char>::size_type& position)
{
  extract_from_pack(position, data, a_);
  extract_from_pack(position, data, aa_);
}

void Mat::Elastic::CoupTransverselyIsotropic::GetFiberVecs(
    std::vector<Core::LinAlg::Matrix<3, 1>>& fibervecs)
{
  fibervecs.push_back(a_);
}

void Mat::Elastic::CoupTransverselyIsotropic::SetFiberVecs(const double newangle,
    const Core::LinAlg::Matrix<3, 3>& locsys, const Core::LinAlg::Matrix<3, 3>& defgrd)
{
  if ((params_->angle_ < -90) || (params_->angle_ > 90))
    FOUR_C_THROW("Fiber angle not in [-90,90]! Given angle = %d", params_->angle_);
  // convert
  const double angle = (params_->gamma_ * M_PI) / 180.;

  Core::LinAlg::Matrix<3, 1> ca(true);
  for (int i = 0; i < 3; ++i)
  {
    // a = cos gamma e3 + sin gamma e2
    ca(i) = std::cos(angle) * locsys(i, 2) + std::sin(angle) * locsys(i, 1);
  }
  // pull back in reference configuration
  Core::LinAlg::Matrix<3, 1> A_0(true);
  Core::LinAlg::Matrix<3, 3> idefgrd(true);
  idefgrd.Invert(defgrd);

  A_0.Multiply(idefgrd, ca);
  a_.Update(1. / A_0.Norm2(), A_0);

  params_->structural_tensor_strategy()->setup_structural_tensor(a_, aa_);
}

void Mat::Elastic::CoupTransverselyIsotropic::AddStrainEnergy(double& psi,
    const Core::LinAlg::Matrix<3, 1>& prinv, const Core::LinAlg::Matrix<3, 1>& modinv,
    const Core::LinAlg::Matrix<6, 1>& glstrain, const int gp, const int eleGID)
{
  // build Cartesian identity 2-tensor I_{AB}
  Core::LinAlg::Matrix<6, 1> identity(true);
  std::fill(identity.A(), identity.A() + 3, 1.0);

  // convert Green-Lagrange strain to right Cauchy-Green Tensor
  // C_{AB} = 2 * E_{AB} + I_{AB} [ REMARK: strain-like 6-Voigt vector ]
  Core::LinAlg::Matrix<6, 1> rcg(true);
  rcg.Update(2.0, glstrain, 1.0);
  rcg.Update(1.0, identity, 1.0);

  reset_invariants(rcg);

  const double alpha = params_->alpha_;
  const double beta = params_->beta_;
  const double gamma = params_->gamma_;

  psi += (alpha + 0.5 * beta * std::log(prinv(2)) + gamma * (i4_ - 1.0)) * (i4_ - 1.0) -
         0.5 * alpha * (i5_ - 1.0);
}

void Mat::Elastic::CoupTransverselyIsotropic::add_stress_aniso_principal(
    const Core::LinAlg::Matrix<6, 1>& rcg, Core::LinAlg::Matrix<6, 6>& cmat,
    Core::LinAlg::Matrix<6, 1>& stress, Teuchos::ParameterList& params, const int gp,
    const int eleGID)
{
  // direct return if an error occurred
  if (reset_invariants(rcg, &params)) return;

  // switch to stress notation
  Core::LinAlg::Matrix<6, 1> rcg_s(false);
  Core::LinAlg::Voigt::Strains::to_stress_like(rcg, rcg_s);

  Core::LinAlg::Matrix<6, 1> rcg_inv_s(false);
  update_second_piola_kirchhoff_stress(stress, rcg_s, rcg_inv_s);

  update_elasticity_tensor(cmat, rcg_inv_s);
}

void Mat::Elastic::CoupTransverselyIsotropic::update_elasticity_tensor(
    Core::LinAlg::Matrix<6, 6>& cmat, const Core::LinAlg::Matrix<6, 1>& rcg_inv_s) const
{
  const double alpha = params_->alpha_;
  const double beta = params_->beta_;
  const double gamma = params_->gamma_;

  const std::array<double, 6> identity = {1.0, 1.0, 1.0, 0.0, 0.0, 0.0};

  // (0) contribution
  {
    const double delta = 8.0 * gamma;
    cmat.MultiplyNT(delta, aa_, aa_, 1.0);
  }

  // (1) contribution
  {
    const double delta = 2.0 * beta;
    cmat.MultiplyNT(delta, rcg_inv_s, aa_, 1.0);
    cmat.MultiplyNT(delta, aa_, rcg_inv_s, 1.0);
  }

  using vmap = Core::LinAlg::Voigt::IndexMappings;
  // (2) contribution
  {
    const double delta = -alpha;
    for (unsigned a = 0; a < 6; ++a)
    {
      for (unsigned b = 0; b < 6; ++b)
      {
        const unsigned i = vmap::voigt_6x6_to_four_tensor_index(a, b, 0);
        const unsigned j = vmap::voigt_6x6_to_four_tensor_index(a, b, 1);
        const unsigned k = vmap::voigt_6x6_to_four_tensor_index(a, b, 2);
        const unsigned l = vmap::voigt_6x6_to_four_tensor_index(a, b, 3);

        cmat(a, b) +=
            delta * (a_(i) * a_(l) * identity[vmap::symmetric_tensor_to_voigt6_index(j, k)] +
                        a_(i) * a_(k) * identity[vmap::symmetric_tensor_to_voigt6_index(j, l)] +
                        a_(k) * a_(j) * identity[vmap::symmetric_tensor_to_voigt6_index(i, l)] +
                        a_(l) * a_(j) * identity[vmap::symmetric_tensor_to_voigt6_index(i, k)]);
      }
    }
  }

  // (3) contribution
  {
    const double delta = -beta * (i4_ - 1.0);
    for (unsigned a = 0; a < 6; ++a)
    {
      for (unsigned b = 0; b < 6; ++b)
      {
        const unsigned i = vmap::voigt_6x6_to_four_tensor_index(a, b, 0);
        const unsigned j = vmap::voigt_6x6_to_four_tensor_index(a, b, 1);
        const unsigned k = vmap::voigt_6x6_to_four_tensor_index(a, b, 2);
        const unsigned l = vmap::voigt_6x6_to_four_tensor_index(a, b, 3);

        cmat(a, b) += delta * (rcg_inv_s(vmap::symmetric_tensor_to_voigt6_index(i, k)) *
                                      rcg_inv_s(vmap::symmetric_tensor_to_voigt6_index(j, l)) +
                                  rcg_inv_s(vmap::symmetric_tensor_to_voigt6_index(i, l)) *
                                      rcg_inv_s(vmap::symmetric_tensor_to_voigt6_index(j, k)));
      }
    }
  }
}

int Mat::Elastic::CoupTransverselyIsotropic::reset_invariants(
    const Core::LinAlg::Matrix<6, 1>& rcg, const Teuchos::ParameterList* params)
{
  // calculate the square root of the third invariant alias the determinant
  // of the deformation gradient
  const double I3 = rcg(0) * rcg(1) * rcg(2) + 0.25 * rcg(3) * rcg(4) * rcg(5) -
                    0.25 * rcg(1) * rcg(5) * rcg(5) - 0.25 * rcg(2) * rcg(3) * rcg(3) -
                    0.25 * rcg(0) * rcg(4) * rcg(4);
  if (I3 < 0.0)
  {
    std::stringstream msg;
    msg << __LINE__ << " -- " << __PRETTY_FUNCTION__ << "I3 is negative!";
    error_handling(params, msg);
    return -1;
  }

  // jacobian determinant
  j_ = std::sqrt(I3);

  // calculate pseudo invariant I4 ( strain measure in fiber direction )
  i4_ = aa_(0) * rcg(0) + aa_(1) * rcg(1) + aa_(2) * rcg(2) + aa_(3) * rcg(3) + aa_(4) * rcg(4) +
        aa_(5) * rcg(5);

  // calculate pseudo invariant I5 ( quad. strain measure in fiber direction )
  Core::LinAlg::Matrix<6, 1> rcg_quad(false);
  Core::LinAlg::Voigt::Strains::power_of_symmetric_tensor(2, rcg, rcg_quad);
  i5_ = aa_(0) * (rcg_quad(0)) + aa_(1) * (rcg_quad(1)) + aa_(2) * (rcg_quad(2)) +
        aa_(3) * (rcg_quad(3)) + aa_(4) * (rcg_quad(4)) + aa_(5) * (rcg_quad(5));

  return 0;
}

void Mat::Elastic::CoupTransverselyIsotropic::update_second_piola_kirchhoff_stress(
    Core::LinAlg::Matrix<6, 1>& stress, const Core::LinAlg::Matrix<6, 1>& rcg_s,
    Core::LinAlg::Matrix<6, 1>& rcg_inv_s) const
{
  const double alpha = params_->alpha_;
  const double beta = params_->beta_;
  const double gamma = params_->gamma_;

  // compute inverse right Cauchy Green tensor
  Core::LinAlg::Voigt::Stresses::inverse_tensor(rcg_s, rcg_inv_s);

  // (0) contribution
  {
    const double fac = beta * (i4_ - 1.0);
    stress.Update(fac, rcg_inv_s, 1.0);
  }

  // (1) contribution
  {
    const double fac = 2.0 * (alpha + beta * std::log(j_) + 2.0 * gamma * (i4_ - 1.0));
    stress.Update(fac, aa_, 1.0);
  }

  // (2) contribution
  {
    Core::LinAlg::Matrix<3, 1> ca(true);
    Core::LinAlg::Voigt::Stresses::multiply_tensor_vector(rcg_s, a_, ca);

    Core::LinAlg::Matrix<6, 1> caa_aac(true);
    Core::LinAlg::Voigt::Stresses::symmetric_outer_product(ca, a_, caa_aac);

    const double fac = -alpha;
    stress.Update(fac, caa_aac, 1.0);
  }
}

void Mat::Elastic::CoupTransverselyIsotropic::error_handling(
    const Teuchos::ParameterList* params, std::stringstream& msg) const
{
  if (params and params->isParameter("interface"))
  {
    Teuchos::RCP<Core::Elements::ParamsInterface> interface_ptr = Teuchos::null;
    interface_ptr = params->get<Teuchos::RCP<Core::Elements::ParamsInterface>>("interface");
    Teuchos::RCP<STR::ELEMENTS::ParamsInterface> pinter =
        Teuchos::rcp_dynamic_cast<STR::ELEMENTS::ParamsInterface>(interface_ptr, true);

    if (pinter->is_tolerate_errors())
    {
      pinter->set_ele_eval_error_flag(STR::ELEMENTS::ele_error_material_failed);
      return;
    }
  }

  FOUR_C_THROW("Uncaught error detected:\n%s", msg.str().c_str());
}
FOUR_C_NAMESPACE_CLOSE
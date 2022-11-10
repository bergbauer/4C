/*----------------------------------------------------------------------------*/
/*! \file
\brief A class to perform integrations of nitsche related terms for the ssi contact case

\level 3

*/
/*----------------------------------------------------------------------------*/

#include "contact_nitsche_integrator_ssi.H"
#include "contact_nitsche_utils.H"

#include "scatra_ele_parameter_boundary.H"
#include "scatra_ele_parameter_timint.H"

#include "so_base.H"
#include "so3_scatra.H"

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
CONTACT::CoIntegratorNitscheSsi::CoIntegratorNitscheSsi(Teuchos::ParameterList& params,
    DRT::Element::DiscretizationType eletype, const Epetra_Comm& comm)
    : CoIntegratorNitsche(params, eletype, comm),
      scatraparamstimint_(DRT::ELEMENTS::ScaTraEleParameterTimInt::Instance("scatra")),
      scatraparamsboundary_(DRT::ELEMENTS::ScaTraEleParameterBoundary::Instance("scatra"))
{
  if (std::abs(theta_) > 1.0e-16) dserror("SSI Contact just implemented Adjoint free ...");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void CONTACT::CoIntegratorNitscheSsi::IntegrateGP_3D(MORTAR::MortarElement& sele,
    MORTAR::MortarElement& mele, LINALG::SerialDenseVector& sval, LINALG::SerialDenseVector& lmval,
    LINALG::SerialDenseVector& mval, LINALG::SerialDenseMatrix& sderiv,
    LINALG::SerialDenseMatrix& mderiv, LINALG::SerialDenseMatrix& lmderiv,
    GEN::pairedvector<int, Epetra_SerialDenseMatrix>& dualmap, double& wgt, double& jac,
    GEN::pairedvector<int, double>& derivjac, double* normal,
    std::vector<GEN::pairedvector<int, double>>& dnmap_unit, double& gap,
    GEN::pairedvector<int, double>& deriv_gap, double* sxi, double* mxi,
    std::vector<GEN::pairedvector<int, double>>& derivsxi,
    std::vector<GEN::pairedvector<int, double>>& derivmxi)
{
  GPTSForces<3>(sele, mele, sval, sderiv, derivsxi, mval, mderiv, derivmxi, jac, derivjac, wgt, gap,
      deriv_gap, normal, dnmap_unit, sxi, mxi);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void CONTACT::CoIntegratorNitscheSsi::IntegrateGP_2D(MORTAR::MortarElement& sele,
    MORTAR::MortarElement& mele, LINALG::SerialDenseVector& sval, LINALG::SerialDenseVector& lmval,
    LINALG::SerialDenseVector& mval, LINALG::SerialDenseMatrix& sderiv,
    LINALG::SerialDenseMatrix& mderiv, LINALG::SerialDenseMatrix& lmderiv,
    GEN::pairedvector<int, Epetra_SerialDenseMatrix>& dualmap, double& wgt, double& jac,
    GEN::pairedvector<int, double>& derivjac, double* normal,
    std::vector<GEN::pairedvector<int, double>>& dnmap_unit, double& gap,
    GEN::pairedvector<int, double>& deriv_gap, double* sxi, double* mxi,
    std::vector<GEN::pairedvector<int, double>>& derivsxi,
    std::vector<GEN::pairedvector<int, double>>& derivmxi)
{
  dserror("2D is not implemented!");
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <int dim>
void CONTACT::CoIntegratorNitscheSsi::GPTSForces(MORTAR::MortarElement& slave_ele,
    MORTAR::MortarElement& master_ele, const LINALG::SerialDenseVector& slave_shape,
    const LINALG::SerialDenseMatrix& slave_shape_deriv,
    const std::vector<GEN::pairedvector<int, double>>& d_slave_xi_dd,
    const LINALG::SerialDenseVector& master_shape,
    const LINALG::SerialDenseMatrix& master_shape_deriv,
    const std::vector<GEN::pairedvector<int, double>>& d_master_xi_dd, const double jac,
    const GEN::pairedvector<int, double>& d_jac_dd, const double gp_wgt, const double gap,
    const GEN::pairedvector<int, double>& d_gap_dd, const double* gp_normal,
    const std::vector<GEN::pairedvector<int, double>>& d_gp_normal_dd, double* slave_xi,
    double* master_xi)
{
  if (slave_ele.Owner() != Comm_.MyPID()) return;

  static const bool do_fast_checks = true;
  // first rough check
  if (do_fast_checks)
  {
    if ((std::abs(theta_) < 1.0e-16) and
        (gap > std::max(slave_ele.MaxEdgeSize(), master_ele.MaxEdgeSize())))
      return;
  }

  dsassert(dim == Dim(), "dimension inconsistency");

  // calculate normals and derivatives
  const LINALG::Matrix<dim, 1> normal(gp_normal, true);
  LINALG::Matrix<dim, 1> slave_normal, master_normal;
  std::vector<GEN::pairedvector<int, double>> d_slave_normal_dd(0, 0);
  std::vector<GEN::pairedvector<int, double>> d_master_normal_dd(0, 0);
  slave_ele.ComputeUnitNormalAtXi(slave_xi, slave_normal.A());
  master_ele.ComputeUnitNormalAtXi(master_xi, master_normal.A());
  slave_ele.DerivUnitNormalAtXi(slave_xi, d_slave_normal_dd);
  master_ele.DerivUnitNormalAtXi(master_xi, d_master_normal_dd);

  double pen = ppn_;
  double pet = ppt_;
  double nitsche_wgt_slave(0.0), nitsche_wgt_master(0.0);

  CONTACT::UTILS::NitscheWeightsAndScaling(
      slave_ele, master_ele, nit_wgt_, dt_, nitsche_wgt_slave, nitsche_wgt_master, pen, pet);

  double cauchy_nn_weighted_average(0.0);
  GEN::pairedvector<int, double> d_cauchy_nn_weighted_average_dd(
      slave_ele.NumNode() * 3 * 12 + slave_ele.MoData().ParentDisp().size() +
      master_ele.MoData().ParentDisp().size());
  GEN::pairedvector<int, double> d_cauchy_nn_weighted_average_ds(
      slave_ele.MoData().ParentScalarDof().size() + master_ele.MoData().ParentScalarDof().size());

  // evaluate cauchy stress components and derivatives
  SoEleCauchy<dim>(slave_ele, slave_xi, d_slave_xi_dd, gp_wgt, slave_normal, d_slave_normal_dd,
      normal, d_gp_normal_dd, nitsche_wgt_slave, cauchy_nn_weighted_average,
      d_cauchy_nn_weighted_average_dd, d_cauchy_nn_weighted_average_ds);
  SoEleCauchy<dim>(master_ele, master_xi, d_master_xi_dd, gp_wgt, master_normal, d_master_normal_dd,
      normal, d_gp_normal_dd, -nitsche_wgt_master, cauchy_nn_weighted_average,
      d_cauchy_nn_weighted_average_dd, d_cauchy_nn_weighted_average_ds);

  const double cauchy_nn_average_pen_gap = cauchy_nn_weighted_average + pen * gap;
  GEN::pairedvector<int, double> d_cauchy_nn_average_pen_gap_dd(
      d_cauchy_nn_weighted_average_dd.size() + d_gap_dd.size());
  for (const auto& p : d_cauchy_nn_weighted_average_dd)
    d_cauchy_nn_average_pen_gap_dd[p.first] += p.second;
  for (const auto& p : d_gap_dd) d_cauchy_nn_average_pen_gap_dd[p.first] += pen * p.second;

  if (cauchy_nn_average_pen_gap < 0.0)
  {
    // test in normal contact direction
    IntegrateTest<dim>(-1.0, slave_ele, slave_shape, slave_shape_deriv, d_slave_xi_dd, jac,
        d_jac_dd, gp_wgt, cauchy_nn_average_pen_gap, d_cauchy_nn_average_pen_gap_dd,
        d_cauchy_nn_weighted_average_ds, normal, d_gp_normal_dd);
    if (!two_half_pass_)
    {
      IntegrateTest<dim>(+1.0, master_ele, master_shape, master_shape_deriv, d_master_xi_dd, jac,
          d_jac_dd, gp_wgt, cauchy_nn_average_pen_gap, d_cauchy_nn_average_pen_gap_dd,
          d_cauchy_nn_weighted_average_ds, normal, d_gp_normal_dd);
    }

    // integrate the scatra-scatra interface condition
    IntegrateSSIInterfaceCondition<dim>(slave_ele, slave_shape, slave_shape_deriv, d_slave_xi_dd,
        master_ele, master_shape, master_shape_deriv, d_master_xi_dd, jac, d_jac_dd, gp_wgt);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <int dim>
void CONTACT::CoIntegratorNitscheSsi::SoEleCauchy(MORTAR::MortarElement& mortar_ele,
    double* gp_coord, const std::vector<GEN::pairedvector<int, double>>& d_gp_coord_dd,
    const double gp_wgt, const LINALG::Matrix<dim, 1>& gp_normal,
    const std::vector<GEN::pairedvector<int, double>>& d_gp_normal_dd,
    const LINALG::Matrix<dim, 1>& test_dir,
    const std::vector<GEN::pairedvector<int, double>>& d_test_dir_dd, const double nitsche_wgt,
    double& cauchy_nt_wgt, GEN::pairedvector<int, double>& d_cauchy_nt_dd,
    GEN::pairedvector<int, double>& d_cauchy_nt_ds)
{
  Epetra_SerialDenseMatrix d_sigma_nt_ds;

  SoEleCauchyStruct<dim>(mortar_ele, gp_coord, d_gp_coord_dd, gp_wgt, gp_normal, d_gp_normal_dd,
      test_dir, d_test_dir_dd, nitsche_wgt, cauchy_nt_wgt, d_cauchy_nt_dd, &d_sigma_nt_ds);

  if (!mortar_ele.MoData().ParentScalar().empty())
  {
    for (int i = 0; i < mortar_ele.ParentElement()->NumNode(); ++i)
      d_cauchy_nt_ds[mortar_ele.MoData().ParentScalarDof().at(i)] +=
          nitsche_wgt * d_sigma_nt_ds(i, 0);
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <int dim>
void CONTACT::CoIntegratorNitscheSsi::SoEleCauchyStruct(MORTAR::MortarElement& mortar_ele,
    double* gp_coord, const std::vector<GEN::pairedvector<int, double>>& d_gp_coord_dd,
    const double gp_wgt, const LINALG::Matrix<dim, 1>& gp_normal,
    const std::vector<GEN::pairedvector<int, double>>& d_gp_normal_dd,
    const LINALG::Matrix<dim, 1>& test_dir,
    const std::vector<GEN::pairedvector<int, double>>& d_test_dir_dd, double nitsche_wgt,
    double& cauchy_nt_wgt, GEN::pairedvector<int, double>& d_cauchy_nt_dd,
    Epetra_SerialDenseMatrix* d_sigma_nt_ds)
{
  static LINALG::Matrix<dim, 1> parent_xi(true);
  static LINALG::Matrix<dim, dim> local_to_parent_trafo(true);
  CONTACT::UTILS::MapGPtoParent<dim>(
      mortar_ele, gp_coord, gp_wgt, parent_xi, local_to_parent_trafo);

  // cauchy stress tensor contracted with normal and test direction
  double sigma_nt(0.0);
  Epetra_SerialDenseMatrix d_sigma_nt_dd;
  static LINALG::Matrix<dim, 1> d_sigma_nt_dn(true), d_sigma_nt_dt(true), d_sigma_nt_dxi(true);

  if (mortar_ele.MoData().ParentScalar().empty())
  {
    dynamic_cast<DRT::ELEMENTS::So_base*>(mortar_ele.ParentElement())
        ->GetCauchyNDirAndDerivativesAtXi(parent_xi, mortar_ele.MoData().ParentDisp(), gp_normal,
            test_dir, sigma_nt, &d_sigma_nt_dd, nullptr, nullptr, nullptr, nullptr, &d_sigma_nt_dn,
            &d_sigma_nt_dt, &d_sigma_nt_dxi, nullptr, nullptr, nullptr, nullptr, nullptr);
  }
  else
  {
    switch (mortar_ele.ParentElement()->Shape())
    {
      case DRT::Element::hex8:
      {
        dynamic_cast<DRT::ELEMENTS::So3_Scatra<DRT::ELEMENTS::So_hex8, DRT::Element::hex8>*>(
            mortar_ele.ParentElement())
            ->GetCauchyNDirAndDerivativesAtXi(parent_xi, mortar_ele.MoData().ParentDisp(),
                mortar_ele.MoData().ParentScalar(), gp_normal, test_dir, sigma_nt, &d_sigma_nt_dd,
                d_sigma_nt_ds, &d_sigma_nt_dn, &d_sigma_nt_dt, &d_sigma_nt_dxi);
        break;
      }
      case DRT::Element::tet4:
      {
        dynamic_cast<DRT::ELEMENTS::So3_Scatra<DRT::ELEMENTS::So_tet4, DRT::Element::tet4>*>(
            mortar_ele.ParentElement())
            ->GetCauchyNDirAndDerivativesAtXi(parent_xi, mortar_ele.MoData().ParentDisp(),
                mortar_ele.MoData().ParentScalar(), gp_normal, test_dir, sigma_nt, &d_sigma_nt_dd,
                d_sigma_nt_ds, &d_sigma_nt_dn, &d_sigma_nt_dt, &d_sigma_nt_dxi);
        break;
      }
      default:
      {
        dserror("SSI Nitsche contact not implemented for used (bulk) elements");
        break;
      }
    }
  }

  cauchy_nt_wgt += nitsche_wgt * sigma_nt;

  for (int i = 0; i < mortar_ele.ParentElement()->NumNode() * dim; ++i)
    d_cauchy_nt_dd[mortar_ele.MoData().ParentDof().at(i)] += nitsche_wgt * d_sigma_nt_dd(i, 0);

  for (int i = 0; i < dim - 1; ++i)
  {
    for (const auto& d_gp_coord_dd_i : d_gp_coord_dd[i])
    {
      for (int k = 0; k < dim; ++k)
      {
        d_cauchy_nt_dd[d_gp_coord_dd_i.first] +=
            nitsche_wgt * d_sigma_nt_dxi(k) * local_to_parent_trafo(k, i) * d_gp_coord_dd_i.second;
      }
    }
  }

  for (int i = 0; i < dim; ++i)
  {
    for (const auto& dn_dd_i : d_gp_normal_dd[i])
      d_cauchy_nt_dd[dn_dd_i.first] += nitsche_wgt * d_sigma_nt_dn(i) * dn_dd_i.second;

    for (const auto& dt_dd_i : d_test_dir_dd[i])
      d_cauchy_nt_dd[dt_dd_i.first] += nitsche_wgt * d_sigma_nt_dt(i) * dt_dd_i.second;
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <int dim>
void CONTACT::CoIntegratorNitscheSsi::IntegrateTest(const double fac, MORTAR::MortarElement& ele,
    const LINALG::SerialDenseVector& shape, const LINALG::SerialDenseMatrix& shape_deriv,
    const std::vector<GEN::pairedvector<int, double>>& d_xi_dd, const double jac,
    const GEN::pairedvector<int, double>& d_jac_dd, const double wgt, const double test_val,
    const GEN::pairedvector<int, double>& d_test_val_dd,
    const GEN::pairedvector<int, double>& d_test_val_ds, const LINALG::Matrix<dim, 1>& normal,
    const std::vector<GEN::pairedvector<int, double>>& d_normal_dd)
{
  if (std::abs(fac) < 1.0e-16) return;

  CONTACT::CoIntegratorNitsche::IntegrateTest<dim>(fac, ele, shape, shape_deriv, d_xi_dd, jac,
      d_jac_dd, wgt, test_val, d_test_val_dd, normal, d_normal_dd);

  for (const auto& d_testval_ds : d_test_val_ds)
  {
    double* row = ele.GetNitscheContainer().Kds(d_testval_ds.first);
    for (int s = 0; s < ele.NumNode(); ++s)
    {
      for (int d = 0; d < dim; ++d)
      {
        row[DRT::UTILS::getParentNodeNumberFromFaceNodeNumber(
                ele.ParentElement()->Shape(), ele.FaceParentNumber(), s) *
                dim +
            d] -= fac * jac * wgt * d_testval_ds.second * normal(d) * shape(s);
      }
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <int dim>
void CONTACT::CoIntegratorNitscheSsi::SetupGpConcentrations(MORTAR::MortarElement& ele,
    const LINALG::SerialDenseVector& shape_func, const LINALG::SerialDenseMatrix& shape_deriv,
    const std::vector<GEN::pairedvector<int, double>>& d_xi_dd, double& gp_conc,
    GEN::pairedvector<int, double>& d_conc_dc, GEN::pairedvector<int, double>& d_conc_dd)
{
  LINALG::SerialDenseVector ele_conc(shape_func.Length());
  for (int i = 0; i < ele.NumNode(); ++i)
    ele_conc(i) = ele.MoData().ParentScalar().at(DRT::UTILS::getParentNodeNumberFromFaceNodeNumber(
        ele.ParentElement()->Shape(), ele.FaceParentNumber(), i));

  // calculate gp concentration
  gp_conc = shape_func.Dot(ele_conc);

  // calculate derivative of concentration w.r.t. concentration
  d_conc_dc.resize(shape_func.Length());
  d_conc_dc.clear();
  for (int i = 0; i < ele.NumNode(); ++i)
    d_conc_dc[ele.MoData().ParentScalarDof().at(DRT::UTILS::getParentNodeNumberFromFaceNodeNumber(
        ele.ParentElement()->Shape(), ele.FaceParentNumber(), i))] = shape_func(i);

  // calculate derivative of concentration w.r.t. displacements
  std::size_t deriv_size = 0;
  for (int i = 0; i < dim - 1; ++i) deriv_size += d_xi_dd.at(i).size();
  d_conc_dd.resize(deriv_size);
  d_conc_dd.clear();
  for (int i = 0; i < dim - 1; ++i)
  {
    for (const auto& d_xi_dd_i : d_xi_dd.at(i))
    {
      for (int n = 0; n < ele.NumNode(); ++n)
        d_conc_dd[d_xi_dd_i.first] += ele_conc(n) * shape_deriv(n, i) * d_xi_dd_i.second;
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <int dim>
void CONTACT::CoIntegratorNitscheSsi::IntegrateSSIInterfaceCondition(
    MORTAR::MortarElement& slave_ele, const LINALG::SerialDenseVector& slave_shape,
    const LINALG::SerialDenseMatrix& slave_shape_deriv,
    const std::vector<GEN::pairedvector<int, double>>& d_slave_xi_dd,
    MORTAR::MortarElement& master_ele, const LINALG::SerialDenseVector& master_shape,
    const LINALG::SerialDenseMatrix& master_shape_deriv,
    const std::vector<GEN::pairedvector<int, double>>& d_master_xi_dd, const double jac,
    const GEN::pairedvector<int, double>& d_jac_dd, const double wgt)
{
  // do only integrate if there is something to integrate!
  if (slave_ele.MoData().ParentScalarDof().empty()) return;
  if (master_ele.MoData().ParentScalarDof().empty()) dserror("This is not allowed!");

  // prepare the slave and master side gauss point concentrations and derivatives w.r.t. the
  // concentration and the displacement
  double slave_conc(0.0), master_conc(0.0);
  GEN::pairedvector<int, double> d_slave_conc_dc(0), d_master_conc_dc(0), d_slave_conc_dd(0),
      d_master_conc_dd(0);
  SetupGpConcentrations<dim>(slave_ele, slave_shape, slave_shape_deriv, d_slave_xi_dd, slave_conc,
      d_slave_conc_dc, d_slave_conc_dd);
  SetupGpConcentrations<dim>(master_ele, master_shape, master_shape_deriv, d_master_xi_dd,
      master_conc, d_master_conc_dc, d_master_conc_dd);

  // get the scatra-scatra interface condition kinetic model
  const int kinetic_model = GetScaTraEleParameterBoundary()->KineticModel();

  // perform integration according to kinetic model
  switch (kinetic_model)
  {
    case INPAR::S2I::kinetics_constperm:
    {
      const double permeability = (*GetScaTraEleParameterBoundary()->Permeabilities())[0];

      // calculate the interface flux
      const double flux = permeability * (slave_conc - master_conc);

      // initialize derivatives of flux w.r.t. concentrations
      GEN::pairedvector<int, double> dflux_dc(d_slave_conc_dc.size() + d_master_conc_dc.size());
      for (const auto& p : d_slave_conc_dc) dflux_dc[p.first] += permeability * p.second;
      for (const auto& p : d_master_conc_dc) dflux_dc[p.first] -= permeability * p.second;

      // initialize derivatives of flux w.r.t. displacements
      GEN::pairedvector<int, double> dflux_dd(d_slave_conc_dd.size() + d_master_conc_dd.size());
      for (const auto& p : d_slave_conc_dd) dflux_dd[p.first] += permeability * p.second;
      for (const auto& p : d_master_conc_dd) dflux_dd[p.first] -= permeability * p.second;

      IntegrateScaTraTest<dim>(-1.0, slave_ele, slave_shape, slave_shape_deriv, d_slave_xi_dd, jac,
          d_jac_dd, wgt, flux, dflux_dd, dflux_dc);
      if (!two_half_pass_)
      {
        IntegrateScaTraTest<dim>(1.0, master_ele, master_shape, master_shape_deriv, d_master_xi_dd,
            jac, d_jac_dd, wgt, flux, dflux_dd, dflux_dc);
      }

      break;
    }
    default:
    {
      dserror(
          "Integration can not be performed as kinetic model of scatra-scatra interface condition "
          "is not recognized: %i",
          kinetic_model);

      break;
    }
  }
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
template <int dim>
void CONTACT::CoIntegratorNitscheSsi::IntegrateScaTraTest(const double fac,
    MORTAR::MortarElement& ele, const LINALG::SerialDenseVector& shape_func,
    const LINALG::SerialDenseMatrix& shape_deriv,
    const std::vector<GEN::pairedvector<int, double>>& d_xi_dd, const double jac,
    const GEN::pairedvector<int, double>& d_jac_dd, const double wgt, const double test_val,
    const GEN::pairedvector<int, double>& d_test_val_dd,
    const GEN::pairedvector<int, double>& d_test_val_ds)
{
  // get time integration factors
  const double time_fac = GetScaTraEleParameterTimInt()->TimeFac();
  const double time_fac_rhs = GetScaTraEleParameterTimInt()->TimeFacRhs();

  const double val = fac * jac * wgt * test_val;

  for (int s = 0; s < ele.NumNode(); ++s)
  {
    *(ele.GetNitscheContainer().RhsS(DRT::UTILS::getParentNodeNumberFromFaceNodeNumber(
        ele.ParentElement()->Shape(), ele.FaceParentNumber(), s))) +=
        time_fac_rhs * val * shape_func(s);
  }

  for (const auto& d_testval_ds : d_test_val_ds)
  {
    double* row = ele.GetNitscheContainer().Kss(d_testval_ds.first);
    for (int s = 0; s < ele.NumNode(); ++s)
    {
      row[DRT::UTILS::getParentNodeNumberFromFaceNodeNumber(
          ele.ParentElement()->Shape(), ele.FaceParentNumber(), s)] -=
          time_fac * fac * jac * wgt * d_testval_ds.second * shape_func(s);
    }
  }

  GEN::pairedvector<int, double> d_val_dd(d_jac_dd.size() + d_test_val_dd.size());
  for (const auto& djac_dd : d_jac_dd)
    d_val_dd[djac_dd.first] += fac * djac_dd.second * wgt * test_val;
  for (const auto& d_testval_dd : d_test_val_dd)
    d_val_dd[d_testval_dd.first] += fac * jac * wgt * d_testval_dd.second;

  for (const auto& dval_dd : d_val_dd)
  {
    double* row = ele.GetNitscheContainer().Ksd(dval_dd.first);
    for (int s = 0; s < ele.NumNode(); ++s)
      row[DRT::UTILS::getParentNodeNumberFromFaceNodeNumber(ele.ParentElement()->Shape(),
          ele.FaceParentNumber(), s)] -= time_fac * dval_dd.second * shape_func(s);
  }

  for (int e = 0; e < dim - 1; ++e)
  {
    for (const auto& d_xi_dd_e : d_xi_dd[e])
    {
      double* row = ele.GetNitscheContainer().Ksd(d_xi_dd_e.first);
      for (int s = 0; s < ele.NumNode(); ++s)
        row[DRT::UTILS::getParentNodeNumberFromFaceNodeNumber(ele.ParentElement()->Shape(),
            ele.FaceParentNumber(), s)] -= time_fac * val * shape_deriv(s, e) * d_xi_dd_e.second;
    }
  }
}
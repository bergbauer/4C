/*---------------------------------------------------------------------*/
/*! \file

\brief Internal implementation of RedAirway element. Methods implemented here
       are called by airway_evaluate.cpp by Discret::ELEMENTS::RedAirway::Evaluate()
       with the corresponding action.


\level 3

*/
/*---------------------------------------------------------------------*/



#include "4C_red_airways_airway_impl.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_fem_general_extract_values.hpp"
#include "4C_fem_general_utils_fem_shapefunctions.hpp"
#include "4C_global_data.hpp"
#include "4C_mat_newtonianfluid.hpp"
#include "4C_mat_par_bundle.hpp"
#include "4C_red_airways_elem_params.hpp"
#include "4C_red_airways_evaluation_data.hpp"
#include "4C_utils_function.hpp"
#include "4C_utils_function_of_time.hpp"

#include <fstream>
#include <iomanip>

FOUR_C_NAMESPACE_OPEN

namespace
{
  /*----------------------------------------------------------------------*
  |  get element length                                      ismail 08/13|
  |                                                                      |
  *----------------------------------------------------------------------*/
  template <Core::FE::CellType distype>
  double GetElementLength(Discret::ELEMENTS::RedAirway* ele)
  {
    double length = 0.0;
    // get node coordinates and number of elements per node
    static const int numnode = Core::FE::num_nodes<distype>;
    Core::Nodes::Node** nodes = ele->Nodes();
    // get airway length
    Core::LinAlg::Matrix<3, numnode> xyze;
    for (int inode = 0; inode < numnode; inode++)
    {
      const auto& x = nodes[inode]->X();
      xyze(0, inode) = x[0];
      xyze(1, inode) = x[1];
      xyze(2, inode) = x[2];
    }
    // Calculate the length of airway element
    length = sqrt(pow(xyze(0, 0) - xyze(0, 1), 2) + pow(xyze(1, 0) - xyze(1, 1), 2) +
                  pow(xyze(2, 0) - xyze(2, 1), 2));
    // get airway area

    return length;
  }

  /*----------------------------------------------------------------------*
  |  calculate curve value a node with a certain BC          ismail 06/13|
  |                                                                      |
  *----------------------------------------------------------------------*/
  template <Core::FE::CellType distype>
  bool GetCurveValAtCond(double& bcVal, Core::Nodes::Node* node, std::string condName,
      std::string optionName, std::string condType, double time)
  {
    // initialize bc value
    bcVal = 0.0;

    // check if node exists
    if (!node)
    {
      // return BC doesn't exist
      return false;
    }

    // check if condition exists
    if (node->GetCondition(condName))
    {
      Core::Conditions::Condition* condition = node->GetCondition(condName);
      // Get the type of prescribed bc
      std::string Bc = (condition->parameters().Get<std::string>(optionName));
      if (Bc == condType)
      {
        const auto* curve = condition->parameters().GetIf<std::vector<int>>("curve");
        double curvefac = 1.0;
        const auto* vals = &condition->parameters().Get<std::vector<double>>("val");

        // -----------------------------------------------------------------
        // Read in the value of the applied BC
        //  Val = curve1*val1 + curve2*func
        // -----------------------------------------------------------------
        // get curve1 and val1
        int curvenum = -1;
        if (curve) curvenum = (*curve)[0];
        if (curvenum >= 0)
          curvefac = Global::Problem::Instance()
                         ->FunctionById<Core::UTILS::FunctionOfTime>(curvenum)
                         .Evaluate(time);

        bcVal = (*vals)[0] * curvefac;

        // get funct 1
        const int* function = condition->parameters().GetIf<int>("funct");
        int functnum = -1;
        if (function) functnum = (*function);

        double functionfac = 0.0;
        if (functnum > 0)
        {
          functionfac = Global::Problem::Instance()
                            ->FunctionById<Core::UTILS::FunctionOfSpaceTime>(functnum - 1)
                            .Evaluate(node->X().data(), time, 0);
        }
        // get curve2
        int curve2num = -1;
        double curve2fac = 1.0;
        if (curve) curve2num = (*curve)[1];
        if (curve2num >= 0)
          curve2fac = Global::Problem::Instance()
                          ->FunctionById<Core::UTILS::FunctionOfTime>(curve2num)
                          .Evaluate(time);

        bcVal += functionfac * curve2fac;

        // return BC exists
        return true;
      }
    }
    // return BC doesn't exist
    return false;
  }

  /*!
  \brief calculate element matrix and rhs

  \param ele              (i) the element those matrix is calculated
  \param eqnp             (i) nodal volumetric flow rate at n+1
  \param evelnp           (i) nodal velocity at n+1
  \param eareanp          (i) nodal cross-sectional area at n+1
  \param eprenp           (i) nodal pressure at n+1
  \param estif            (o) element matrix to calculate
  \param eforce           (o) element rhs to calculate
  \param material         (i) airway material/dimesion
  \param time             (i) current simulation time
  \param dt               (i) timestep
  \param compute_awacinter(i) computing airway-acinus interdependency
  */
  template <Core::FE::CellType distype>
  void Sysmat(Discret::ELEMENTS::RedAirway* ele, Core::LinAlg::SerialDenseVector& epnp,
      Core::LinAlg::SerialDenseVector& epn, Core::LinAlg::SerialDenseVector& epnm,
      Core::LinAlg::SerialDenseMatrix& sysmat, Core::LinAlg::SerialDenseVector& rhs,
      Teuchos::RCP<const Core::Mat::Material> material, Discret::ReducedLung::ElemParams& params,
      double time, double dt, bool compute_awacinter)
  {
    const auto airway_params = ele->GetAirwayParams();

    double dens = 0.0;
    double visc = 0.0;

    if (material->MaterialType() == Core::Materials::m_fluid)
    {
      // get actual material
      const Mat::NewtonianFluid* actmat = static_cast<const Mat::NewtonianFluid*>(material.get());

      // get density
      dens = actmat->Density();

      // get dynamic viscosity
      visc = actmat->Viscosity();
    }
    else
    {
      FOUR_C_THROW("Material law is not a Newtonian fluid");
      exit(1);
    }

    rhs.putScalar(0.0);
    sysmat.putScalar(0.0);

    // Calculate the length of airway element
    const double L = GetElementLength<distype>(ele);

    double qout_n = params.qout_n;
    double qout_np = params.qout_np;
    double qin_n = params.qin_n;
    double qin_np = params.qin_np;

    // get the generation number
    const int generation = airway_params.generation;

    double R = -1.0;

    // get element information
    const double Ao = airway_params.area;
    double A = Ao;
    const double velPow = airway_params.power_velocity_profile;

    if (ele->ElemSolvingType() == "Linear")
    {
    }
    else if (ele->ElemSolvingType() == "NonLinear")
    {
      A = params.volnp / L;
    }
    else
    {
      FOUR_C_THROW("[%s] is not a defined ElemSolvingType of a RED_AIRWAY element",
          ele->ElemSolvingType().c_str());
    }

    // Get airway branch length
    double l_branch = airway_params.branch_length;
    if (l_branch < 0.0) l_branch = L;

    // evaluate Poiseuille resistance
    double Rp = 2.0 * (2.0 + velPow) * M_PI * visc * L / (pow(A, 2));

    // evaluate the Reynolds number
    const double Re = 2.0 * fabs(qout_np) / (visc / dens * sqrt(A * M_PI));

    if (ele->Resistance() == "Poiseuille")
    {
      R = Rp;
    }
    else if (ele->Resistance() == "Pedley")
    {
      //-----------------------------------------------------------------
      // resistance evaluated using Pedley's model from :
      // Pedley et al (1970)
      //-----------------------------------------------------------------
      double gamma = 0.327;
      R = gamma * (sqrt(Re * 2.0 * sqrt(A / M_PI) / l_branch)) * Rp;

      //-----------------------------------------------------------------
      // Correct any resistance smaller than Poiseuille's one
      //-----------------------------------------------------------------
      //    if (R < Rp)
      //    {
      //      R = Rp;
      //    }
      double alfa = sqrt(2.0 * sqrt(A / M_PI) / l_branch);

      double Rep = 1.0 / ((gamma * alfa) * (gamma * alfa));
      double k = 0.50;
      double st = 1.0 / (1.0 + exp(-2 * k * (Re - Rep)));

      R = R * st + Rp * (1.0 - st);
    }
    else if (ele->Resistance() == "Generation_Dependent_Pedley")
    {
      //-----------------------------------------------------------------
      // Gamma is taken from Ertbruggen et al
      //-----------------------------------------------------------------
      double gamma = 0.327;
      switch (generation)
      {
        case 0:
          gamma = 0.162;
          break;
        case 1:
          gamma = 0.239;
          break;
        case 2:
          gamma = 0.244;
          break;
        case 3:
          gamma = 0.295;
          break;
        case 4:
          gamma = 0.175;
          break;
        case 5:
          gamma = 0.303;
          break;
        case 6:
          gamma = 0.356;
          break;
        case 7:
          gamma = 0.566;
          break;
        default:
          gamma = 0.327;
          break;
      }
      //-----------------------------------------------------------------
      // resistance evaluated using Pedley's model from :
      // Pedley et al (1970)
      //-----------------------------------------------------------------
      R = gamma * (sqrt(Re * 2.0 * sqrt(A / M_PI) / l_branch)) * Rp;

      //-----------------------------------------------------------------
      // Correct any resistance smaller than Poiseuille's one
      //-----------------------------------------------------------------
      if (R < Rp)
      {
        R = Rp;
      }
    }
    else if (ele->Resistance() == "Cont_Pedley")
    {
      //-----------------------------------------------------------------
      // resistance evaluated using Pedley's model from :
      // Pedley et al (1970)
      //-----------------------------------------------------------------
      double gamma = 0.327;
      double D = sqrt(A / M_PI) * 2.0;
      double Rel = (l_branch / D) * (1.0 / (gamma * gamma));
      double lambda = 1.2;
      double Ret = lambda * Rel;

      //-----------------------------------------------------------------
      // Correct any resistance smaller than Poiseuille's one
      //-----------------------------------------------------------------
      if (Re >= Ret)
        R = gamma * (sqrt(Re * 2.0 * sqrt(A / M_PI) / l_branch)) * Rp;
      else
      {
        double St = gamma * sqrt((D / l_branch) * Ret);
        double bRe = 2.0 * St / (St - 1.0);
        double aRe = (St - 1.0) / pow(Ret, bRe);
        R = (aRe * pow(Re, bRe) + 1.0) * Rp;
      }
    }
    else if (ele->Resistance() == "Generation_Dependent_Cont_Pedley")
    {
      //-----------------------------------------------------------------
      // Gamma is taken from Ertbruggen et al
      //-----------------------------------------------------------------
      double gamma = 0.327;
      switch (generation)
      {
        case 0:
          gamma = 0.162;
          break;
        case 1:
          gamma = 0.239;
          break;
        case 2:
          gamma = 0.244;
          break;
        case 3:
          gamma = 0.295;
          break;
        case 4:
          gamma = 0.175;
          break;
        case 5:
          gamma = 0.303;
          break;
        case 6:
          gamma = 0.356;
          break;
        case 7:
          gamma = 0.566;
          break;
        default:
          gamma = 0.327;
          break;
      }
      //-----------------------------------------------------------------
      // resistance evaluated using Pedley's model from :
      // Pedley et al (1970)
      //-----------------------------------------------------------------
      double D = sqrt(A / M_PI) * 2.0;
      double Rel = (l_branch / D) * (1.0 / (gamma * gamma));
      double lambda = 1.2;
      double Ret = lambda * Rel;

      //-----------------------------------------------------------------
      // Correct any resistance smaller than Poiseuille's one
      //-----------------------------------------------------------------
      if (Re >= Ret)
        R = gamma * (sqrt(Re * 2.0 * sqrt(A / M_PI) / l_branch)) * Rp;
      else
      {
        double St = gamma * sqrt((D / l_branch) * Ret);
        double bRe = 2.0 * St / (St - 1.0);
        double aRe = (St - 1.0) / pow(Ret, bRe);
        R = (aRe * pow(Re, bRe) + 1.0) * Rp;
      }
    }
    else if (ele->Resistance() == "Reynolds")
    {
      R = Rp * (3.4 + 2.1e-3 * Re);
    }
    else
    {
      FOUR_C_THROW("[%s] is not a defined resistance model", ele->Resistance().c_str());
    }

    //------------------------------------------------------------
    // Set high resistance for collapsed airway
    //------------------------------------------------------------
    const double airwayColl = airway_params.airway_coll;

    if (airwayColl == 1)
    {
      double opennp = params.open;
      if (opennp == 0)
      {
        // R = 10000000000;
        R = 10000000;  // 000 before: 10^10, Bates: 10^8
      }
    }

    //------------------------------------------------------------
    // get airway compliance
    //------------------------------------------------------------
    const double Ew = airway_params.wall_elasticity;
    const double tw = airway_params.wall_thickness;
    const double nu = airway_params.poisson_ratio;

    // Get element compliance
    double C = 0.0;
    double Ec = 0.0;
    Ec = (Ew * tw * sqrt(M_PI)) / ((1.0 - nu * nu) * 2.0 * sqrt(A) * Ao * L);
    if (Ec != 0.0)
    {
      C = 1.0 / Ec;
    }

    //------------------------------------------------------------
    // get airway viscous resistance
    //------------------------------------------------------------
    const double Ts = airway_params.viscous_Ts;
    const double phis = airway_params.viscous_phase_shift;
    // define 0D airway components
    double gammas = Ts * tan(phis) * (Ew * tw * sqrt(M_PI) / (1.0 - nu * nu)) / (4.0 * M_PI);
    double Rvis = gammas / (Ao * sqrt(Ao) * L);

    //------------------------------------------------------------
    // get airway inductance
    //------------------------------------------------------------
    double I = dens * L / Ao;

    //------------------------------------------------------------
    // get airway convective resistance
    //------------------------------------------------------------
    // get Poiseuille resistance with parabolic profile
    double Rp2nd = 2.0 * (2.0 + 2.0) * M_PI * visc * L / (pow(A, 2));
    // get the power of velocity profile for the currently used resistance
    double gamma = 4.0 / (Rp2nd / R) - 2.0;
    // get the Coriolis coefficient
    double alpha = (2.0 + gamma) / (1.0 + gamma);
    double Rconv = 2.0 * alpha * dens * (qout_np - qin_np) / (A * A);

    //------------------------------------------------------------
    // get airway external pressure
    //------------------------------------------------------------
    double pextn = 0.0;
    double pextnp = 0.0;

    // loop over all nodes
    // pext is the average pressure over the nodes
    for (int i = 0; i < ele->num_node(); i++)
    {
      double pextVal = 0.0;
      // get Pext at time step n
      GetCurveValAtCond<distype>(pextVal, ele->Nodes()[i], "RedAirwayPrescribedExternalPressure",
          "boundarycond", "ExternalPressure", time - dt);
      pextn += pextVal / double(ele->num_node());

      // get Pext at time step n+1e
      GetCurveValAtCond<distype>(pextVal, ele->Nodes()[i], "RedAirwayPrescribedExternalPressure",
          "boundarycond", "ExternalPressure", time);
      pextnp += pextVal / double(ele->num_node());
    }

    // Routine to compute pextnp andd pextn from neighbourung acinus pressure
    // ComputePext() analog zu EvaluateCollapse()
    // bool compute_awacinter = params.get<bool>("compute_awacinter");
    if (compute_awacinter)
    {
      pextn = params.p_extn;
      pextnp = params.p_extnp;
    }

    if (ele->Type() == "Resistive")
    {
      C = 0.0;
      I = 0.0;
      Rconv = 0.0;
      Rvis = 0.0;
    }
    else if (ele->Type() == "InductoResistive")
    {
      C = 0.0;
      Rconv = 0.0;
      Rvis = 0.0;
    }
    else if (ele->Type() == "ComplientResistive")
    {
      I = 0.0;
      Rconv = 0.0;
      Rvis = 0.0;
    }
    else if (ele->Type() == "RLC")
    {
      Rconv = 0.0;
      Rvis = 0.0;
    }
    else if (ele->Type() == "ViscoElasticRLC")
    {
      Rconv = 0.0;
    }
    else if (ele->Type() == "ConvectiveViscoElasticRLC")
    {
    }
    else
    {
      FOUR_C_THROW("[%s] is not an implemented element yet", (ele->Type()).c_str());
      exit(1);
    }

    double Ainv = -0.5 * C / (dt + Rvis * C);
    double B = 0.5 * I / dt + 0.5 * (Rconv + R);
    double P1 = epn(0) + epn(1) - 2.0 * Rvis * (qin_n - qout_n) + 2.0 * (pextnp - pextn);
    double P2 = -I * (qin_n + qout_n) / (2.0 * dt);

    sysmat(0, 0) = 0.5 * Ainv - 0.5 / B;
    sysmat(0, 1) = 0.5 * Ainv + 0.5 / B;
    sysmat(1, 0) = 0.5 * Ainv + 0.5 / B;
    sysmat(1, 1) = 0.5 * Ainv - 0.5 / B;

    rhs(0) = 0.5 * (P1 * Ainv - P2 / B);
    rhs(1) = 0.5 * (P1 * Ainv + P2 / B);

    // If airway is collapsed, set pressure equal in the downstream airway to
    // force zero flow downstream of the collapse
    if (airwayColl == 1)
    {
      double opennp = params.open;

      if (opennp == 0)
      {
        sysmat(1, 0) = 0;
        sysmat(1, 1) = 0;
        // rhs(0) = 0;
        rhs(1) = 0;
      }
    }
  }
}  // namespace

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Discret::ELEMENTS::RedAirwayImplInterface* Discret::ELEMENTS::RedAirwayImplInterface::Impl(
    Discret::ELEMENTS::RedAirway* red_airway)
{
  switch (red_airway->Shape())
  {
    case Core::FE::CellType::line2:
    {
      static AirwayImpl<Core::FE::CellType::line2>* airway;
      if (airway == nullptr)
      {
        airway = new AirwayImpl<Core::FE::CellType::line2>;
      }
      return airway;
    }
    default:
      FOUR_C_THROW(
          "shape %d (%d nodes) not supported", red_airway->Shape(), red_airway->num_node());
      break;
  }
  return nullptr;
}


/*----------------------------------------------------------------------*
 | evaluate (public)                                       ismail 01/10 |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
int Discret::ELEMENTS::AirwayImpl<distype>::Evaluate(RedAirway* ele, Teuchos::ParameterList& params,
    Core::FE::Discretization& discretization, std::vector<int>& lm,
    Core::LinAlg::SerialDenseMatrix& elemat1_epetra,
    Core::LinAlg::SerialDenseMatrix& elemat2_epetra,
    Core::LinAlg::SerialDenseVector& elevec1_epetra,
    Core::LinAlg::SerialDenseVector& elevec2_epetra,
    Core::LinAlg::SerialDenseVector& elevec3_epetra, Teuchos::RCP<Core::Mat::Material> mat)
{
  const int elemVecdim = elevec1_epetra.length();

  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();
  const auto airway_params = ele->GetAirwayParams();

  //----------------------------------------------------------------------
  // get control parameters for time integration
  //----------------------------------------------------------------------
  // get time-step size
  const double dt = evaluation_data.dt;
  // get time
  const double time = evaluation_data.time;

  // ---------------------------------------------------------------------
  // get control parameters for stabilization and higher-order elements
  //----------------------------------------------------------------------
  // flag for higher order elements
  // bool higher_order_ele = ele->is_higher_order_element(distype);

  // ---------------------------------------------------------------------
  // get all general state vectors: flow, pressure,
  // ---------------------------------------------------------------------

  Teuchos::RCP<const Epetra_Vector> pnp = discretization.GetState("pnp");
  Teuchos::RCP<const Epetra_Vector> pn = discretization.GetState("pn");
  Teuchos::RCP<const Epetra_Vector> pnm = discretization.GetState("pnm");

  if (pnp == Teuchos::null || pn == Teuchos::null || pnm == Teuchos::null)
    FOUR_C_THROW("Cannot get state vectors 'pnp', 'pn', and/or 'pnm''");

  // extract local values from the global vectors
  std::vector<double> mypnp(lm.size());
  Core::FE::ExtractMyValues(*pnp, mypnp, lm);

  // extract local values from the global vectors
  std::vector<double> mypn(lm.size());
  Core::FE::ExtractMyValues(*pn, mypn, lm);

  // extract local values from the global vectors
  std::vector<double> mypnm(lm.size());
  Core::FE::ExtractMyValues(*pnm, mypnm, lm);

  // create objects for element arrays
  Core::LinAlg::SerialDenseVector epnp(elemVecdim);
  Core::LinAlg::SerialDenseVector epn(elemVecdim);
  Core::LinAlg::SerialDenseVector epnm(elemVecdim);
  for (int i = 0; i < elemVecdim; ++i)
  {
    // split area and volumetric flow rate, insert into element arrays
    epnp(i) = mypnp[i];
    epn(i) = mypn[i];
    epnm(i) = mypnm[i];
  }

  double e_acin_e_vnp;
  double e_acin_e_vn;

  // split area and volumetric flow rate, insert into element arrays
  e_acin_e_vnp = (*evaluation_data.acinar_vnp)[ele->LID()];
  e_acin_e_vn = (*evaluation_data.acinar_vn)[ele->LID()];

  // get the volumetric flow rate from the previous time step
  Discret::ReducedLung::ElemParams elem_params;
  elem_params.qout_np = (*evaluation_data.qout_np)[ele->LID()];
  elem_params.qout_n = (*evaluation_data.qout_n)[ele->LID()];
  elem_params.qout_nm = (*evaluation_data.qout_nm)[ele->LID()];
  elem_params.qin_np = (*evaluation_data.qin_np)[ele->LID()];
  elem_params.qin_n = (*evaluation_data.qin_n)[ele->LID()];
  elem_params.qin_nm = (*evaluation_data.qin_nm)[ele->LID()];
  elem_params.volnp = (*evaluation_data.elemVolumenp)[ele->LID()];
  elem_params.voln = (*evaluation_data.elemVolumen)[ele->LID()];

  elem_params.acin_vnp = e_acin_e_vnp;
  elem_params.acin_vn = e_acin_e_vn;

  elem_params.lungVolume_np = evaluation_data.lungVolume_np;
  elem_params.lungVolume_n = evaluation_data.lungVolume_n;
  elem_params.lungVolume_nm = evaluation_data.lungVolume_nm;

  // Routine for computing pextn and pextnp
  if (evaluation_data.compute_awacinter)
  {
    ComputePext(ele, pn, pnp, params);
    elem_params.p_extn = (*evaluation_data.p_extn)[ele->LID()];
    elem_params.p_extnp = (*evaluation_data.p_extnp)[ele->LID()];
  }

  // Routine for open/collapsed decision
  const double airwayColl = airway_params.airway_coll;
  if (airwayColl == 1)
  {
    EvaluateCollapse(ele, epnp, params, dt);
    elem_params.open = (*evaluation_data.open)[ele->LID()];
  }

  // ---------------------------------------------------------------------
  // call routine for calculating element matrix and right hand side
  // ---------------------------------------------------------------------
  Sysmat<distype>(ele, epnp, epn, epnm, elemat1_epetra, elevec1_epetra, mat, elem_params, time, dt,
      evaluation_data.compute_awacinter);

  return 0;
}


/*----------------------------------------------------------------------*
 |  calculate element matrix and right hand side (private)  ismail 01/10|
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::Initial(RedAirway* ele, Teuchos::ParameterList& params,
    Core::FE::Discretization& discretization, std::vector<int>& lm,
    Core::LinAlg::SerialDenseVector& radii_in, Core::LinAlg::SerialDenseVector& radii_out,
    Teuchos::RCP<const Core::Mat::Material> material)
{
  const int myrank = discretization.Comm().MyPID();

  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();
  const auto airway_params = ele->GetAirwayParams();

  std::vector<int> lmstride;
  Teuchos::RCP<std::vector<int>> lmowner = Teuchos::rcp(new std::vector<int>);
  ele->LocationVector(discretization, lm, *lmowner, lmstride);

  // Calculate the length of airway element
  const double L = GetElementLength<distype>(ele);

  //--------------------------------------------------------------------
  // Initialize the pressure vectors
  //--------------------------------------------------------------------
  if (myrank == (*lmowner)[0])
  {
    int gid = lm[0];
    double val = 0.0;
    evaluation_data.p0np->ReplaceGlobalValues(1, &val, &gid);
    evaluation_data.p0n->ReplaceGlobalValues(1, &val, &gid);
    evaluation_data.p0nm->ReplaceGlobalValues(1, &val, &gid);
  }
  {
    int gid = lm[1];
    double val = 0.0;
    if (myrank == ele->Nodes()[1]->Owner())
    {
      evaluation_data.p0np->ReplaceGlobalValues(1, &val, &gid);
      evaluation_data.p0n->ReplaceGlobalValues(1, &val, &gid);
      evaluation_data.p0nm->ReplaceGlobalValues(1, &val, &gid);
    }

    const double A = airway_params.area;

    val = sqrt(A / M_PI);
    radii_in(0) = val;
    radii_in(1) = 0.0;
    radii_out(0) = 0.0;
    radii_out(1) = val;
  }

  //--------------------------------------------------------------------
  // get the generation numbers
  //--------------------------------------------------------------------
  //  if(myrank == ele->Owner())
  {
    int gid = ele->Id();
    const int generation = airway_params.generation;

    double val = double(generation);
    evaluation_data.generations->ReplaceGlobalValues(1, &val, &gid);


    const double A = airway_params.area;
    double V = A * L;
    evaluation_data.elemVolume->ReplaceGlobalValues(1, &V, &gid);
    evaluation_data.elemArea0->ReplaceGlobalValues(1, &A, &gid);
  }

}  // AirwayImpl::Initial


/*----------------------------------------------------------------------*
 |  Evaluate open/collapsed state of an airway element following        |
 |  Bates and Irvin (2002), J. Appl. Physiol., 93:705-713.              |
 |                                                         roth 12/2015 |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::EvaluateCollapse(
    RedAirway* ele, Core::LinAlg::SerialDenseVector& epn, Teuchos::ParameterList& params, double dt)
{
  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();
  const auto airway_params = ele->GetAirwayParams();

  const double s_c = airway_params.s_close;
  const double s_o = airway_params.s_open;
  const double Pcrit_o = airway_params.p_crit_open;
  const double Pcrit_c = airway_params.p_crit_close;

  double xnp = (*evaluation_data.x_np)[ele->LID()];
  double xn = (*evaluation_data.x_n)[ele->LID()];
  double opennp = (*evaluation_data.open)[ele->LID()];

  // as decisive quantity the pressure value at the first node of the airway element is chosen;
  // using the mean pressure of the airway element caused convergence problems
  double tmp = epn(0);

  /*if (epn(0)-Pcrit_o > 0)
  {
    xnp=xn + s_o*dt*(epn(0)-Pcrit_o);
  }
  else if (epn(0)-Pcrit_c < 0)
  {
    xnp=xn + s_c*dt*(epn(0)-Pcrit_c);
  }*/

  if (tmp > Pcrit_o)
  {
    xnp = xn + s_o * dt * (tmp - Pcrit_o);
  }
  else if (tmp < Pcrit_c)
  {
    xnp = xn + s_c * dt * (tmp - Pcrit_c);
  }

  if (xnp > 1.0)
  {
    xnp = 1.0;
    opennp = 1;
  }
  else if (xnp < 0.0)
  {
    xnp = 0.0;
    opennp = 0;
  }

  int gid = ele->Id();
  evaluation_data.x_np->ReplaceGlobalValues(1, &xnp, &gid);
  evaluation_data.open->ReplaceGlobalValues(1, &opennp, &gid);
}

/*----------------------------------------------------------------------*
 |  Neighbour search for computing pressure prevailing on the outside   |
 |  of an airway.                                                       |
 |                                                         roth 02/2016 |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::ComputePext(RedAirway* ele,
    Teuchos::RCP<const Epetra_Vector> pn, Teuchos::RCP<const Epetra_Vector> pnp,
    Teuchos::ParameterList& params)
{
  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();

  // get node-Id of nearest acinus
  int node_id = (*evaluation_data.airway_acinus_dep)[ele->LID()];

  // Set pextn and pextnp
  double pextnp = (*pnp)[node_id];
  double pextn = (*pn)[node_id];


  int gid = ele->Id();
  evaluation_data.p_extnp->ReplaceGlobalValues(1, &pextnp, &gid);
  evaluation_data.p_extn->ReplaceGlobalValues(1, &pextn, &gid);
}

/*----------------------------------------------------------------------*
 |  Evaluate the values of the degrees of freedom           ismail 01/10|
 |  at terminal nodes.                                                  |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::EvaluateTerminalBC(RedAirway* ele,
    Teuchos::ParameterList& params, Core::FE::Discretization& discretization, std::vector<int>& lm,
    Core::LinAlg::SerialDenseVector& rhs, Teuchos::RCP<Core::Mat::Material> material)
{
  const int myrank = discretization.Comm().MyPID();

  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();

  // get total time
  const double time = evaluation_data.time;

  // get time-step size
  const double dt = evaluation_data.dt;

  // the number of nodes
  const int numnode = lm.size();

  Teuchos::RCP<const Epetra_Vector> pn = discretization.GetState("pn");

  if (pn == Teuchos::null) FOUR_C_THROW("Cannot get state vectors 'pn'");

  // extract local values from the global vectors
  std::vector<double> mypn(lm.size());
  Core::FE::ExtractMyValues(*pn, mypn, lm);

  // create objects for element arrays
  Core::LinAlg::SerialDenseVector epn(numnode);

  // get all values at the last computed time step
  for (int i = 0; i < numnode; ++i)
  {
    // split area and volumetric flow rate, insert into element arrays
    epn(i) = mypn[i];
  }

  Core::LinAlg::SerialDenseVector eqn(2);
  eqn(0) = (*evaluation_data.qin_n)[ele->LID()];
  eqn(1) = (*evaluation_data.qout_n)[ele->LID()];
  // ---------------------------------------------------------------------------------
  // Resolve the BCs
  // ---------------------------------------------------------------------------------
  for (int i = 0; i < ele->num_node(); i++)
  {
    if (ele->Nodes()[i]->Owner() == myrank)
    {
      if (ele->Nodes()[i]->GetCondition("RedAirwayPrescribedCond") ||
          ele->Nodes()[i]->GetCondition("Art_redD_3D_CouplingCond") ||
          ele->Nodes()[i]->GetCondition("RedAirwayVentilatorCond"))
      {
        std::string Bc;
        double BCin = 0.0;
        if (ele->Nodes()[i]->GetCondition("RedAirwayPrescribedCond"))
        {
          Core::Conditions::Condition* condition =
              ele->Nodes()[i]->GetCondition("RedAirwayPrescribedCond");
          // Get the type of prescribed bc
          Bc = (condition->parameters().Get<std::string>("boundarycond"));

          if (Bc == "switchFlowPressure")
          {
            // get switch condition variables
            Core::Conditions::Condition* switchCondition =
                ele->Nodes()[i]->GetCondition("RedAirwaySwitchFlowPressureCond");

            const int funct_id_flow = switchCondition->parameters().Get<int>("FUNCT_ID_FLOW");
            const int funct_id_pressure =
                switchCondition->parameters().Get<int>("FUNCT_ID_PRESSURE");
            const int funct_id_switch =
                switchCondition->parameters().Get<int>("FUNCT_ID_PRESSURE_ACTIVE");

            const double pressure_active =
                Global::Problem::Instance()
                    ->FunctionById<Core::UTILS::FunctionOfTime>(funct_id_switch - 1)
                    .Evaluate(time);

            int funct_id_current = 0;
            if (std::abs(pressure_active - 1.0) < 10e-8)
            {
              // phase with pressure bc
              Bc = "pressure";
              funct_id_current = funct_id_pressure;
            }
            else if (std::abs(pressure_active) < 10e-8)
            {
              // phase with flow bc
              Bc = "flow";
              funct_id_current = funct_id_flow;
            }
            else
            {
              FOUR_C_THROW(
                  "FUNCTION %i has to take either value 0.0 or 1.0. Not clear if flow or pressure "
                  "boundary condition should be active.",
                  (funct_id_switch - 1));
              exit(1);
            }

            BCin = Global::Problem::Instance()
                       ->FunctionById<Core::UTILS::FunctionOfTime>(funct_id_current - 1)
                       .Evaluate(time);
          }
          else
          {
            // -----------------------------------------------------------------
            // Read in the value of the applied BC
            //  Val = curve1*val1 + curve2*func
            // -----------------------------------------------------------------
            const auto* curve = condition->parameters().GetIf<std::vector<int>>("curve");
            const auto* vals = &condition->parameters().Get<std::vector<double>>("val");

            // get factor of curve1 or curve2
            const auto curvefac = [&](unsigned id)
            {
              int curvenum = -1;
              if (curve)
              {
                if ((curvenum = (*curve)[id]) >= 0)
                  return Global::Problem::Instance()
                      ->FunctionById<Core::UTILS::FunctionOfTime>(curvenum)
                      .Evaluate(time);
                else
                  return 1.0;
              }
              else
                return 1.0;
            };

            // get factor of func
            const double functfac = std::invoke(
                [&]()
                {
                  int functnum = -1;
                  const std::vector<int>* functions =
                      condition->parameters().GetIf<std::vector<int>>("funct");
                  if (functions)
                    if ((functnum = (*functions)[0]) > 0)
                      return Global::Problem::Instance()
                          ->FunctionById<Core::UTILS::FunctionOfSpaceTime>(functnum - 1)
                          .Evaluate((ele->Nodes()[i])->X().data(), time, 0);
                    else
                      return 0.0;
                  else
                    return 0.0;
                });

            BCin = (*vals)[0] * curvefac(0) + functfac * curvefac(1);
          }
          // -----------------------------------------------------------------------------
          // get the local id of the node to whome the bc is prescribed
          // -----------------------------------------------------------------------------
          int local_id = discretization.NodeRowMap()->LID(ele->Nodes()[i]->Id());
          if (local_id < 0)
          {
            FOUR_C_THROW("node (%d) doesn't exist on proc(%d)", ele->Nodes()[i]->Id(),
                discretization.Comm().MyPID());
            exit(1);
          }
        }
        else if (ele->Nodes()[i]->GetCondition("Art_redD_3D_CouplingCond"))
        {
          const Core::Conditions::Condition* condition =
              ele->Nodes()[i]->GetCondition("Art_redD_3D_CouplingCond");

          Teuchos::RCP<Teuchos::ParameterList> CoupledTo3DParams =
              params.get<Teuchos::RCP<Teuchos::ParameterList>>("coupling with 3D fluid params");
          // -----------------------------------------------------------------
          // If the parameter list is empty, then something is wrong!
          // -----------------------------------------------------------------
          if (CoupledTo3DParams.get() == nullptr)
          {
            FOUR_C_THROW(
                "Cannot prescribe a boundary condition from 3D to reduced D, if the parameters "
                "passed don't exist");
            exit(1);
          }

          // -----------------------------------------------------------------
          // Read in Condition type
          // -----------------------------------------------------------------
          //        Type = (condition->parameters().Get<std::string>("CouplingType"));
          // -----------------------------------------------------------------
          // Read in coupling variable rescribed by the 3D simulation
          //
          //     In this case a map called map3D has the following form:
          //     +-----------------------------------------------------------+
          //     |           std::map< std::string               ,  double        >    |
          //     |     +------------------------------------------------+    |
          //     |     |  ID  | coupling variable name | variable value |    |
          //     |     +------------------------------------------------+    |
          //     |     |  1   |   flow1                |     0.12116    |    |
          //     |     +------+------------------------+----------------+    |
          //     |     |  2   |   pressure2            |    10.23400    |    |
          //     |     +------+------------------------+----------------+    |
          //     |     .  .   .   ....                 .     .......    .    |
          //     |     +------+------------------------+----------------+    |
          //     |     |  N   |   variableN            |    value(N)    |    |
          //     |     +------+------------------------+----------------+    |
          //     +-----------------------------------------------------------+
          // -----------------------------------------------------------------

          int ID = condition->parameters().Get<int>("ConditionID");
          Teuchos::RCP<std::map<std::string, double>> map3D;
          map3D = CoupledTo3DParams->get<Teuchos::RCP<std::map<std::string, double>>>(
              "3D map of values");

          // find the applied boundary variable
          std::stringstream stringID;
          stringID << "_" << ID;
          for (std::map<std::string, double>::iterator itr = map3D->begin(); itr != map3D->end();
               itr++)
          {
            std::string VariableWithId = itr->first;
            size_t found;
            found = VariableWithId.rfind(stringID.str());
            if (found != std::string::npos)
            {
              Bc = std::string(VariableWithId, 0, found);
              BCin = itr->second;
              break;
            }
          }
        }
        else if (ele->Nodes()[i]->GetCondition("RedAirwayVentilatorCond"))
        {
          Core::Conditions::Condition* condition =
              ele->Nodes()[i]->GetCondition("RedAirwayVentilatorCond");
          // Get the type of prescribed bc
          Bc = (condition->parameters().Get<std::string>("phase1"));

          // get the smoothness flag of the two different phases
          std::string phase1Smooth = (condition->parameters().Get<std::string>("Phase1Smoothness"));
          std::string phase2Smooth = (condition->parameters().Get<std::string>("Phase2Smoothness"));

          double period = condition->parameters().Get<double>("period");
          double period1 = condition->parameters().Get<double>("phase1_period");

          double smoothnessT1 = condition->parameters().Get<double>("smoothness_period1");
          double smoothnessT2 = condition->parameters().Get<double>("smoothness_period2");

          unsigned int phase_number = 0;

          if (fmod(time, period) >= period1)
          {
            phase_number = 1;
            Bc = (condition->parameters().Get<std::string>("phase2"));
          }

          const auto* curve = condition->parameters().GetIf<std::vector<int>>("curve");
          double curvefac = 1.0;
          const auto* vals = &condition->parameters().Get<std::vector<double>>("val");

          // -----------------------------------------------------------------
          // Read in the value of the applied BC
          // -----------------------------------------------------------------
          int curvenum = -1;
          if (curve) curvenum = (*curve)[phase_number];
          if (curvenum >= 0)
            curvefac = Global::Problem::Instance()
                           ->FunctionById<Core::UTILS::FunctionOfTime>(curvenum)
                           .Evaluate(time);

          BCin = (*vals)[phase_number] * curvefac;

          // -----------------------------------------------------------------
          // Compute flow value in case a volume is prescribed in the RedAirwayVentilatorCond
          // -----------------------------------------------------------------
          if (Bc == "volume")
          {
            if (fmod(time, period) < period1)
            {
              double Vnp = BCin;
              double Vn =
                  (*vals)[phase_number] * Global::Problem::Instance()
                                              ->FunctionById<Core::UTILS::FunctionOfTime>(curvenum)
                                              .Evaluate(time - dt);
              BCin = (Vnp - Vn) / dt;
              Bc = "flow";
            }
          }

          // -----------------------------------------------------------------
          // treat smoothness of the solution
          // -----------------------------------------------------------------
          // if phase 1
          if ((fmod(time, period) < smoothnessT1 && phase_number == 0) ||
              (fmod(time, period) < period1 + smoothnessT2 && phase_number == 1))
          {
            double tsmooth = period;
            if (phase_number == 0 && phase1Smooth == "smooth")
            {
              tsmooth = fmod(time, period);
              double tau = smoothnessT2 / 6.0;
              double Xo = 0.0;
              double Xinf = BCin;
              double Xn = 0.0;
              if (Bc == "pressure")
              {
                Xn = epn(i);
              }
              if (Bc == "flow")
              {
                Xn = eqn(i);
              }
              Xo = (Xn - Xinf) / (exp(-(tsmooth - dt) / tau));
              BCin = Xo * exp(-tsmooth / tau) + Xinf;
            }
            if (phase_number == 1 && phase2Smooth == "smooth")
            {
              tsmooth = fmod(time, period) - period1;
              double tau = smoothnessT2 / 6.0;
              double Xo = 0.0;
              double Xinf = BCin;
              double Xn = 0.0;
              if (Bc == "pressure")
              {
                Xn = epn(i);
              }
              if (Bc == "flow")
              {
                Xn = eqn(i);
              }
              Xo = (Xn - Xinf) / (exp(-(tsmooth - dt) / tau));
              BCin = Xo * exp(-tsmooth / tau) + Xinf;
            }
          }

          // -----------------------------------------------------------------------------
          // get the local id of the node to whome the bc is prescribed
          // -----------------------------------------------------------------------------
          int local_id = discretization.NodeRowMap()->LID(ele->Nodes()[i]->Id());
          if (local_id < 0)
          {
            FOUR_C_THROW("node (%d) doesn't exist on proc(%d)", ele->Nodes()[i]->Id(),
                discretization.Comm().MyPID());
            exit(1);
          }
        }
        else
        {
        }

        if (Bc == "pressure")
        {
          // set pressure at node i
          int gid;
          double val;

          gid = lm[i];
          val = BCin;
          evaluation_data.bcval->ReplaceGlobalValues(1, &val, &gid);

          gid = lm[i];
          val = 1;
          evaluation_data.dbctog->ReplaceGlobalValues(1, &val, &gid);
        }
        else if (Bc == "flow")
        {
          // ----------------------------------------------------------
          // Since a node might belong to multiple elements then the
          // flow might be added to the rhs multiple time.
          // To fix this the flow is devided by the number of elements
          // (which is the number of branches). Thus the sum of the
          // final added values is the actual prescribed flow.
          // ----------------------------------------------------------
          int numOfElems = (ele->Nodes()[i])->NumElement();
          BCin /= double(numOfElems);

          rhs(i) += -BCin + rhs(i);
        }
        else
        {
          FOUR_C_THROW("precribed [%s] is not defined for reduced airways", Bc.c_str());
          exit(1);
        }
      }
      else
      {
        // ---------------------------------------------------------------
        // If the node is a terminal node, but no b.c is prescribed to it
        // then a zero output pressure is assumed
        // ---------------------------------------------------------------
        if (ele->Nodes()[i]->NumElement() == 1)
        {
          // -------------------------------------------------------------
          // get the local id of the node to whome the bc is prescribed
          // -------------------------------------------------------------

          int local_id = discretization.NodeRowMap()->LID(ele->Nodes()[i]->Id());
          if (local_id < 0)
          {
            FOUR_C_THROW("node (%d) doesn't exist on proc(%d)", ele->Nodes()[i],
                discretization.Comm().MyPID());
            exit(1);
          }

          // set pressure at node i
          int gid;
          double val;

          gid = lm[i];
          val = 0.0;
          evaluation_data.bcval->ReplaceGlobalValues(1, &val, &gid);

          gid = lm[i];
          val = 1;
          evaluation_data.dbctog->ReplaceGlobalValues(1, &val, &gid);
        }
      }  // END of if there is no BC but the node still is at the terminal

    }  // END of if node is available on this processor
  }    // End of node i has a condition
}


/*----------------------------------------------------------------------*
 |  Evaluate the values of the degrees of freedom           ismail 01/10|
 |  at terminal nodes.                                                  |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::CalcFlowRates(RedAirway* ele,
    Teuchos::ParameterList& params, Core::FE::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<Core::Mat::Material> material)
{
  const int elemVecdim = lm.size();

  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();

  //----------------------------------------------------------------------
  // get control parameters for time integration
  //----------------------------------------------------------------------
  // get time-step size
  const double dt = evaluation_data.dt;

  // get time
  const double time = evaluation_data.time;

  // ---------------------------------------------------------------------
  // get all general state vectors: flow, pressure,
  // ---------------------------------------------------------------------

  Teuchos::RCP<const Epetra_Vector> pnp = discretization.GetState("pnp");
  Teuchos::RCP<const Epetra_Vector> pn = discretization.GetState("pn");
  Teuchos::RCP<const Epetra_Vector> pnm = discretization.GetState("pnm");

  if (pnp == Teuchos::null || pn == Teuchos::null || pnm == Teuchos::null)
    FOUR_C_THROW("Cannot get state vectors 'pnp', 'pn', and/or 'pnm''");

  // extract local values from the global vectors
  std::vector<double> mypnp(lm.size());
  Core::FE::ExtractMyValues(*pnp, mypnp, lm);

  // extract local values from the global vectors
  std::vector<double> mypn(lm.size());
  Core::FE::ExtractMyValues(*pn, mypn, lm);

  // extract local values from the global vectors
  std::vector<double> mypnm(lm.size());
  Core::FE::ExtractMyValues(*pnm, mypnm, lm);

  // create objects for element arrays
  Core::LinAlg::SerialDenseVector epnp(elemVecdim);
  Core::LinAlg::SerialDenseVector epn(elemVecdim);
  Core::LinAlg::SerialDenseVector epnm(elemVecdim);
  for (int i = 0; i < elemVecdim; ++i)
  {
    // split area and volumetric flow rate, insert into element arrays
    epnp(i) = mypnp[i];
    epn(i) = mypn[i];
    epnm(i) = mypnm[i];
  }

  double e_acin_vnp = 0.0;
  double e_acin_vn = 0.0;

  for (int i = 0; i < elemVecdim; ++i)
  {
    // split area and volumetric flow rate, insert into element arrays
    e_acin_vnp = (*evaluation_data.acinar_vnp)[ele->LID()];
    e_acin_vn = (*evaluation_data.acinar_vn)[ele->LID()];
  }


  // get the volumetric flow rate from the previous time step
  Discret::ReducedLung::ElemParams elem_params;
  elem_params.qout_np = (*evaluation_data.qout_np)[ele->LID()];
  elem_params.qout_n = (*evaluation_data.qout_n)[ele->LID()];
  elem_params.qout_nm = (*evaluation_data.qout_nm)[ele->LID()];
  elem_params.qin_np = (*evaluation_data.qin_np)[ele->LID()];
  elem_params.qin_n = (*evaluation_data.qin_n)[ele->LID()];
  elem_params.qin_nm = (*evaluation_data.qin_nm)[ele->LID()];

  // TODO same volume is used is this correct?
  elem_params.volnp = (*evaluation_data.elemVolumenp)[ele->LID()];
  elem_params.voln = (*evaluation_data.elemVolumenp)[ele->LID()];

  elem_params.acin_vnp = e_acin_vnp;
  elem_params.acin_vn = e_acin_vn;

  elem_params.lungVolume_np = 0.0;
  elem_params.lungVolume_n = 0.0;
  elem_params.lungVolume_nm = 0.0;

  elem_params.x_np = (*evaluation_data.x_np)[ele->LID()];
  elem_params.x_n = (*evaluation_data.x_n)[ele->LID()];
  elem_params.open = (*evaluation_data.open)[ele->LID()];

  elem_params.p_extn = (*evaluation_data.p_extn)[ele->LID()];
  elem_params.p_extnp = (*evaluation_data.p_extnp)[ele->LID()];

  Core::LinAlg::SerialDenseMatrix sysmat(elemVecdim, elemVecdim, true);
  Core::LinAlg::SerialDenseVector rhs(elemVecdim);


  // ---------------------------------------------------------------------
  // call routine for calculating element matrix and right hand side
  // ---------------------------------------------------------------------
  Sysmat<distype>(ele, epnp, epn, epnm, sysmat, rhs, material, elem_params, time, dt,
      evaluation_data.compute_awacinter);

  double qinnp = -1.0 * (sysmat(0, 0) * epnp(0) + sysmat(0, 1) * epnp(1) - rhs(0));
  double qoutnp = 1.0 * (sysmat(1, 0) * epnp(0) + sysmat(1, 1) * epnp(1) - rhs(1));

  int gid = ele->Id();

  evaluation_data.qin_np->ReplaceGlobalValues(1, &qinnp, &gid);
  evaluation_data.qout_np->ReplaceGlobalValues(1, &qoutnp, &gid);
}  // CalcFlowRates


/*----------------------------------------------------------------------*
 |  Evaluate the elements volume from the change in flow    ismail 07/13|
 |  rates.                                                              |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::CalcElemVolume(RedAirway* ele,
    Teuchos::ParameterList& params, Core::FE::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<Core::Mat::Material> material)
{
  // get all essential vector variables

  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();
  const auto airway_params = ele->GetAirwayParams();

  // extract all essential element variables from their corresponding variables
  double qinnp = (*evaluation_data.qin_np)[ele->LID()];
  double qoutnp = (*evaluation_data.qout_np)[ele->LID()];
  double eVolumen = (*evaluation_data.elemVolumen)[ele->LID()];
  double eVolumenp = (*evaluation_data.elemVolumenp)[ele->LID()];

  // get time-step size
  const double dt = evaluation_data.dt;

  // get element global ID
  int gid = ele->Id();

  // -------------------------------------------------------------------
  // find the change of volume from the conservation equation
  // par(V)/par(t) = Qin - Qout
  // numerically
  // (v^n+1 - v^n)/dt = (Qin^n+1 - Qout^n+1)
  // -------------------------------------------------------------------
  double dVol = dt * (qinnp - qoutnp);
  // new volume
  eVolumenp = eVolumen + dVol;

  // -------------------------------------------------------------------
  // Treat possible collapses
  // -------------------------------------------------------------------
  // Calculate the length of airway element
  const double L = GetElementLength<distype>(ele);

  // get area0
  const double area0 = airway_params.area;

  // calculate the current area
  double area = eVolumenp / L;

  // if the airway is near collapsing then fix area to 0.01*area0
  if (area / area0 < 0.01)
  {
    eVolumenp = L * area0 * 0.01;
  }
  // update elem
  evaluation_data.elemVolumenp->ReplaceGlobalValues(1, &eVolumenp, &gid);

  // calculate and update element radius
  double eRadiusnp = std::sqrt(eVolumenp / L * M_1_PI);
  evaluation_data.elemRadiusnp->ReplaceGlobalValues(1, &eRadiusnp, &gid);
}  // CalcElemVolume


/*----------------------------------------------------------------------*
 |  Get the coupled the values on the coupling interface    ismail 07/10|
 |  of the 3D/reduced-D problem                                         |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::GetCoupledValues(RedAirway* ele,
    Teuchos::ParameterList& params, Core::FE::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<Core::Mat::Material> material)
{
  const int myrank = discretization.Comm().MyPID();

  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();

  // get total time
  const double time = evaluation_data.time;

  // the number of nodes
  const int numnode = lm.size();

  Teuchos::RCP<const Epetra_Vector> pnp = discretization.GetState("pnp");

  if (pnp == Teuchos::null) FOUR_C_THROW("Cannot get state vectors 'pnp'");

  // extract local values from the global vectors
  std::vector<double> mypnp(lm.size());
  Core::FE::ExtractMyValues(*pnp, mypnp, lm);

  // create objects for element arrays
  Core::LinAlg::SerialDenseVector epnp(numnode);

  // get all values at the last computed time step
  for (int i = 0; i < numnode; ++i)
  {
    // split area and volumetric flow rate, insert into element arrays
    epnp(i) = mypnp[i];
  }

  // ---------------------------------------------------------------------------------
  // Resolve the BCs
  // ---------------------------------------------------------------------------------
  for (int i = 0; i < ele->num_node(); i++)
  {
    if (ele->Nodes()[i]->Owner() == myrank)
    {
      if (ele->Nodes()[i]->GetCondition("Art_redD_3D_CouplingCond"))
      {
        const Core::Conditions::Condition* condition =
            ele->Nodes()[i]->GetCondition("Art_redD_3D_CouplingCond");
        Teuchos::RCP<Teuchos::ParameterList> CoupledTo3DParams =
            params.get<Teuchos::RCP<Teuchos::ParameterList>>("coupling with 3D fluid params");
        // -----------------------------------------------------------------
        // If the parameter list is empty, then something is wrong!
        // -----------------------------------------------------------------
        if (CoupledTo3DParams.get() == nullptr)
        {
          FOUR_C_THROW(
              "Cannot prescribe a boundary condition from 3D to reduced D, if the parameters "
              "passed don't exist");
          exit(1);
        }


        // -----------------------------------------------------------------
        // Compute the variable solved by the reduced D simulation to be
        // passed to the 3D simulation
        //
        //     In this case a map called map1D has the following form:
        //     +-----------------------------------------------------------+
        //     |              std::map< std::string            ,  double        > >  |
        //     |     +------------------------------------------------+    |
        //     |     |  ID  | coupling variable name | variable value |    |
        //     |     +------------------------------------------------+    |
        //     |     |  1   |   flow1                |     xxxxxxx    |    |
        //     |     +------+------------------------+----------------+    |
        //     |     |  2   |   pressure2            |     xxxxxxx    |    |
        //     |     +------+------------------------+----------------+    |
        //     |     .  .   .   ....                 .     .......    .    |
        //     |     +------+------------------------+----------------+    |
        //     |     |  N   |   variable(N)          | trash value(N) |    |
        //     |     +------+------------------------+----------------+    |
        //     +-----------------------------------------------------------+
        // -----------------------------------------------------------------

        int ID = condition->parameters().Get<int>("ConditionID");
        Teuchos::RCP<std::map<std::string, double>> map1D;
        map1D = CoupledTo3DParams->get<Teuchos::RCP<std::map<std::string, double>>>(
            "reducedD map of values");

        std::string returnedBC = (condition->parameters().Get<std::string>("ReturnedVariable"));

        double BC3d = 0.0;
        if (returnedBC == "flow")
        {
          // MUST BE DONE
        }
        else if (returnedBC == "pressure")
        {
          BC3d = epnp(i);
        }
        else
        {
          std::string str = (condition->parameters().Get<std::string>("ReturnedVariable"));
          FOUR_C_THROW("%s, is an unimplimented type of coupling", str.c_str());
          exit(1);
        }
        std::stringstream returnedBCwithId;
        returnedBCwithId << returnedBC << "_" << ID;
        std::cout << "COND [" << ID << "] Returning at time " << time << " " << returnedBC << "= "
                  << BC3d << std::endl;
        // -----------------------------------------------------------------
        // Check whether the coupling wrapper has already initialized this
        // map else wise we will have problems with parallelization, that's
        // because of the preassumption that the map is filled and sorted
        // Thus we can use parallel addition
        // -----------------------------------------------------------------

        std::map<std::string, double>::iterator itrMap1D;
        itrMap1D = map1D->find(returnedBCwithId.str());
        if (itrMap1D == map1D->end())
        {
          FOUR_C_THROW("The 3D map for (1D - 3D coupling) has no variable (%s) for ID [%d]",
              returnedBC.c_str(), ID);
          exit(1);
        }

        // update the 1D map
        (*map1D)[returnedBCwithId.str()] = BC3d;
      }
    }  // END of if node is available on this processor
  }    // End of node i has a condition
}


/*----------------------------------------------------------------------*
 |  calculate the ammount of fluid mixing inside a          ismail 02/13|
 |  junction                                                            |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::get_junction_volume_mix(RedAirway* ele,
    Teuchos::ParameterList& params, Core::FE::Discretization& discretization,
    Core::LinAlg::SerialDenseVector& volumeMix_np, std::vector<int>& lm,
    Teuchos::RCP<Core::Mat::Material> material)
{
  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();

  // get the elements Qin and Qout
  double qoutnp = (*evaluation_data.qout_np)[ele->LID()];
  double qinnp = (*evaluation_data.qin_np)[ele->LID()];
  double evolnp = (*evaluation_data.elemVolumenp)[ele->LID()];

  //--------------------------------------------------------------------
  // get element length
  //--------------------------------------------------------------------
  const double L = GetElementLength<distype>(ele);

  // Check if the node is attached to any other elements
  if (qoutnp >= 0.0)
  {
    volumeMix_np(1) = evolnp / L;
  }
  if (qinnp < 0.0)
  {
    volumeMix_np(0) = evolnp / L;
  }

  for (int i = 0; i < iel; i++)
  {
    {
      if (ele->Nodes()[i]->NumElement() == 1) volumeMix_np(i) = evolnp / L;
    }
  }

  if (ele->Nodes()[0]->GetCondition("RedAirwayPrescribedScatraCond"))
  {
    if (qinnp >= 0) volumeMix_np(0) = evolnp / L;
  }
  if (ele->Nodes()[1]->GetCondition("RedAirwayPrescribedScatraCond"))
  {
    if (qoutnp < 0) volumeMix_np(1) = evolnp / L;
  }
}

/*----------------------------------------------------------------------*
 |  calculate element CFL                                   ismail 02/13|
 |                                                                      |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::CalcCFL(RedAirway* ele, Teuchos::ParameterList& params,
    Core::FE::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<Core::Mat::Material> material)
{
  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();

  // get the elements Qin and Qout
  double q_outnp = (*evaluation_data.qout_np)[ele->LID()];
  double q_innp = (*evaluation_data.qin_np)[ele->LID()];
  double eVolnp = (*evaluation_data.elemVolumenp)[ele->LID()];


  // get time step size
  const double dt = evaluation_data.dt;

  // get time
  //  const double time = evaluation_data.time;

  //--------------------------------------------------------------------
  // get element length
  //--------------------------------------------------------------------
  // Calculate the length of airway element
  const double L = GetElementLength<distype>(ele);

  // get area
  double area = eVolnp / L;

  // evaluate velocity at nodes (1) and (2)
  double vel1np = q_innp / area;
  double vel2np = q_outnp / area;

  double cfl1np = fabs(vel1np) * dt / L;
  double cfl2np = fabs(vel2np) * dt / L;

  double cflmax = 0.0;
  cflmax = (cfl1np > cflmax) ? cfl1np : cflmax;
  cflmax = (cfl2np > cflmax) ? cfl2np : cflmax;

  int gid = ele->Id();
  if (ele->Nodes()[1]->Owner()) evaluation_data.cfl->ReplaceGlobalValues(1, &cflmax, &gid);
}


/*----------------------------------------------------------------------*
 |  calculate the scalar transport                          ismail 02/13|
 |                                                                      |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::update_scatra(RedAirway* ele,
    Teuchos::ParameterList& params, Core::FE::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<Core::Mat::Material> material)
{
  const int myrank = discretization.Comm().MyPID();

  // ---------------------------------------------------------------------
  // perform this step only for capillaries
  // ---------------------------------------------------------------------
  if (ele->Nodes()[0]->GetCondition("RedAirwayScatraCapillaryCond") == nullptr ||
      ele->Nodes()[1]->GetCondition("RedAirwayScatraCapillaryCond") == nullptr)
  {
    return;
  }
  else
  {
    Teuchos::RCP<const Epetra_Vector> scatranp = discretization.GetState("scatranp");
    Teuchos::RCP<const Epetra_Vector> avgscatranp = discretization.GetState("avg_scatranp");
    Teuchos::RCP<const Epetra_Vector> dscatranp = discretization.GetState("dscatranp");

    Discret::ReducedLung::EvaluationData& evaluation_data =
        Discret::ReducedLung::EvaluationData::get();

    // extract local values from the global vectors
    std::vector<double> mydscatranp(lm.size());
    Core::FE::ExtractMyValues(*dscatranp, mydscatranp, lm);

    // extract local values from the global vectors
    std::vector<double> myscatranp(lm.size());
    Core::FE::ExtractMyValues(*scatranp, myscatranp, lm);

    // extract local values from the global vectors
    std::vector<double> myavgscatranp(lm.size());
    Core::FE::ExtractMyValues(*avgscatranp, myavgscatranp, lm);

    // get flowrate

    // Get the average concentration

    double scatra_avg = 0.0;
    for (unsigned int i = 0; i < lm.size(); i++)
    {
      scatra_avg += myavgscatranp[i];
    }
    scatra_avg /= double(lm.size());
    for (unsigned int i = 0; i < lm.size(); i++)
    {
      scatra_avg += mydscatranp[i];
    }

    // modify dscatranp to have the new average scatranp
    for (unsigned int i = 0; i < lm.size(); i++)
    {
      int gid = lm[i];
      mydscatranp[i] = scatra_avg - myscatranp[i];
      double val = mydscatranp[i];
      if (myrank == ele->Nodes()[i]->Owner())
      {
        evaluation_data.dscatranp->ReplaceGlobalValues(1, &val, &gid);
      }
    }
  }
}  // update_scatra



template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::UpdateElem12Scatra(RedAirway* ele,
    Teuchos::ParameterList& params, Core::FE::Discretization& discretization, std::vector<int>& lm,
    Teuchos::RCP<Core::Mat::Material> material)
{
  // ---------------------------------------------------------------------
  // perform this step only for capillaries
  // ---------------------------------------------------------------------
  if (ele->Nodes()[0]->GetCondition("RedAirwayScatraCapillaryCond") == nullptr ||
      ele->Nodes()[1]->GetCondition("RedAirwayScatraCapillaryCond") == nullptr)
  {
    return;
  }


  Teuchos::RCP<const Epetra_Vector> dscatranp = discretization.GetState("dscatranp");
  Teuchos::RCP<const Epetra_Vector> scatranp = discretization.GetState("scatranp");
  Teuchos::RCP<const Epetra_Vector> volumeMix = discretization.GetState("junctionVolumeInMix");

  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();

  // extract local values from the global vectors
  std::vector<double> mydscatranp(lm.size());
  Core::FE::ExtractMyValues(*dscatranp, mydscatranp, lm);

  // extract local values from the global vectors
  std::vector<double> myscatranp(lm.size());
  Core::FE::ExtractMyValues(*scatranp, myscatranp, lm);

  // extract local values from the global vectors
  std::vector<double> myvolmix(lm.size());
  Core::FE::ExtractMyValues(*volumeMix, myvolmix, lm);

  // ---------------------------------------------------------------------
  // element scatra must be updated only at the capillary nodes.
  // ---------------------------------------------------------------------
  double e1s = myscatranp[0];
  double e2s = myscatranp[1];

  int gid = ele->Id();
  evaluation_data.e1scatranp->ReplaceGlobalValues(1, &e1s, &gid);
  evaluation_data.e2scatranp->ReplaceGlobalValues(1, &e2s, &gid);
}


/*----------------------------------------------------------------------*
 |  calculate the scalar transport                          ismail 06/13|
 |                                                                      |
 *----------------------------------------------------------------------*/
template <Core::FE::CellType distype>
void Discret::ELEMENTS::AirwayImpl<distype>::eval_nodal_essential_values(RedAirway* ele,
    Teuchos::ParameterList& params, Core::FE::Discretization& discretization,
    Core::LinAlg::SerialDenseVector& nodal_surface, Core::LinAlg::SerialDenseVector& nodal_volume,
    Core::LinAlg::SerialDenseVector& nodal_avg_scatra, std::vector<int>& lm,
    Teuchos::RCP<Core::Mat::Material> material)
{
  // ---------------------------------------------------------------------
  // perform this step only for capillaries
  // ---------------------------------------------------------------------
  if (ele->Nodes()[0]->GetCondition("RedAirwayScatraCapillaryCond") == nullptr ||
      ele->Nodes()[1]->GetCondition("RedAirwayScatraCapillaryCond") == nullptr)
  {
    return;
  }

  Discret::ReducedLung::EvaluationData& evaluation_data =
      Discret::ReducedLung::EvaluationData::get();

  // get time-step size
  const double dt = evaluation_data.dt;

  // ---------------------------------------------------------------------
  // get all general state vectors: flow, pressure,
  // ---------------------------------------------------------------------
  Teuchos::RCP<const Epetra_Vector> scatranp = discretization.GetState("scatranp");

  // ---------------------------------------------------------------------
  // extract scatra values
  // ---------------------------------------------------------------------
  // extract local values from the global vectors
  std::vector<double> myscatranp(lm.size());
  Core::FE::ExtractMyValues(*scatranp, myscatranp, lm);

  double qin = (*evaluation_data.qin_np)[ele->LID()];
  double eVolnp = (*evaluation_data.elemVolumenp)[ele->LID()];

  // ---------------------------------------------------------------------
  // get volume of capillaries
  // ---------------------------------------------------------------------
  // Calculate the length of airway element
  const double length = GetElementLength<distype>(ele);
  // get airway area
  double area = eVolnp / length;

  // get node coordinates and number of elements per node
  {
    nodal_volume[0] = length * area;
    nodal_volume[1] = length * area;

    double avg_scatra = 0.0;
    double vel = fabs(qin / area);
    double dx = dt * vel;
    if (qin >= 0.0)
    {
      //      avg_scatra = myscatranp[1]- 0.5*(myscatranp[1]-myscatranp[0])*dx/length;
      avg_scatra = myscatranp[1] - (myscatranp[1] - myscatranp[0]) * dx / length;
    }
    else
    {
      //      avg_scatra = myscatranp[0]- 0.5*(myscatranp[0]-myscatranp[1])*dx/length;
      avg_scatra = myscatranp[0] - (myscatranp[0] - myscatranp[1]) * dx / length;
    }

    nodal_avg_scatra[0] = avg_scatra;
    nodal_avg_scatra[1] = avg_scatra;
  }
}

FOUR_C_NAMESPACE_CLOSE
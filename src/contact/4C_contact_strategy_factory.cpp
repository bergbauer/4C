/*---------------------------------------------------------------------*/
/*! \file
\brief Factory to create the desired contact strategy


\level 3

*/
/*---------------------------------------------------------------------*/

#include "4C_contact_strategy_factory.hpp"

#include "4C_contact_aug_combo_strategy.hpp"
#include "4C_contact_aug_interface.hpp"
#include "4C_contact_aug_lagrange_interface.hpp"
#include "4C_contact_aug_lagrange_strategy.hpp"
#include "4C_contact_aug_steepest_ascent_interface.hpp"
#include "4C_contact_aug_steepest_ascent_strategy.hpp"
#include "4C_contact_constitutivelaw_bundle.hpp"
#include "4C_contact_constitutivelaw_contactconstitutivelaw_parameter.hpp"
#include "4C_contact_constitutivelaw_interface.hpp"
#include "4C_contact_element.hpp"
#include "4C_contact_friction_node.hpp"
#include "4C_contact_lagrange_strategy.hpp"
#include "4C_contact_lagrange_strategy_tsi.hpp"
#include "4C_contact_lagrange_strategy_wear.hpp"
#include "4C_contact_nitsche_strategy.hpp"
#include "4C_contact_nitsche_strategy_ssi.hpp"
#include "4C_contact_nitsche_strategy_ssi_elch.hpp"
#include "4C_contact_nitsche_strategy_tsi.hpp"
#include "4C_contact_paramsinterface.hpp"
#include "4C_contact_penalty_strategy.hpp"
#include "4C_contact_rough_node.hpp"
#include "4C_contact_tsi_interface.hpp"
#include "4C_contact_utils.hpp"
#include "4C_contact_wear_interface.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_inpar_s2i.hpp"
#include "4C_inpar_ssi.hpp"
#include "4C_inpar_validparameters.hpp"
#include "4C_inpar_wear.hpp"
#include "4C_io.hpp"
#include "4C_io_pstream.hpp"
#include "4C_linalg_utils_sparse_algebra_math.hpp"
#include "4C_scatra_timint_meshtying_strategy_s2i.hpp"
#include "4C_structure_new_timint_basedataglobalstate.hpp"
#include "4C_structure_new_utils.hpp"

#include <Teuchos_ParameterList.hpp>

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::STRATEGY::Factory::Setup()
{
  check_init();
  Mortar::STRATEGY::Factory::Setup();

  set_is_setup();
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::STRATEGY::Factory::read_and_check_input(Teuchos::ParameterList& params) const
{
  check_init();
  // console output at the beginning
  if (comm().MyPID() == 0)
  {
    std::cout << "Checking contact input parameters...........";
    fflush(stdout);
  }

  // read parameter lists from Global::Problem
  const Teuchos::ParameterList& mortar = Global::Problem::Instance()->mortar_coupling_params();
  const Teuchos::ParameterList& contact = Global::Problem::Instance()->contact_dynamic_params();
  const Teuchos::ParameterList& wearlist = Global::Problem::Instance()->WearParams();
  const Teuchos::ParameterList& tsic = Global::Problem::Instance()->TSIContactParams();

  // read Problem Type and Problem Dimension from Global::Problem
  const Core::ProblemType problemtype = Global::Problem::Instance()->GetProblemType();
  Core::FE::ShapeFunctionType distype = Global::Problem::Instance()->spatial_approximation_type();
  const int dim = Global::Problem::Instance()->NDim();

  // in case just System type system_condensed_lagmult
  if (Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(contact, "SYSTEM") ==
      Inpar::CONTACT::system_condensed_lagmult)
  {
    FOUR_C_THROW(
        "For Contact anyway just the lagrange multiplier can be condensed, "
        "choose SYSTEM = Condensed.");
  }

  // ---------------------------------------------------------------------
  // invalid parallel strategies
  // ---------------------------------------------------------------------
  const Teuchos::ParameterList& mortarParallelRedistParams =
      mortar.sublist("PARALLEL REDISTRIBUTION");

  if (Teuchos::getIntegralValue<Inpar::Mortar::ParallelRedist>(mortarParallelRedistParams,
          "PARALLEL_REDIST") != Inpar::Mortar::ParallelRedist::redist_none &&
      mortarParallelRedistParams.get<int>("MIN_ELEPROC") < 0)
    FOUR_C_THROW(
        "Minimum number of elements per processor for parallel redistribution must be >= 0");

  if (Teuchos::getIntegralValue<Inpar::Mortar::ParallelRedist>(mortarParallelRedistParams,
          "PARALLEL_REDIST") == Inpar::Mortar::ParallelRedist::redist_dynamic &&
      mortarParallelRedistParams.get<double>("MAX_BALANCE_EVAL_TIME") < 1.0)
  {
    FOUR_C_THROW(
        "Maximum allowed value of load balance for dynamic parallel redistribution must be "
        ">= 1.0");
  }

  if (problemtype == Core::ProblemType::tsi &&
      Teuchos::getIntegralValue<Inpar::Mortar::ParallelRedist>(mortarParallelRedistParams,
          "PARALLEL_REDIST") != Inpar::Mortar::ParallelRedist::redist_none &&
      Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") !=
          Inpar::CONTACT::solution_nitsche)
    FOUR_C_THROW("Parallel redistribution not yet implemented for TSI problems");

  // ---------------------------------------------------------------------
  // adhesive contact
  // ---------------------------------------------------------------------
  if (Core::UTILS::IntegralValue<Inpar::CONTACT::AdhesionType>(contact, "ADHESION") !=
          Inpar::CONTACT::adhesion_none and
      Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(wearlist, "WEARLAW") !=
          Inpar::Wear::wear_none)
    FOUR_C_THROW("Adhesion combined with wear not yet tested!");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::AdhesionType>(contact, "ADHESION") !=
          Inpar::CONTACT::adhesion_none and
      Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") !=
          Inpar::CONTACT::friction_none)
    FOUR_C_THROW("Adhesion combined with friction not yet tested!");

  // ---------------------------------------------------------------------
  // generally invalid combinations (nts/mortar)
  // ---------------------------------------------------------------------
  if ((Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
              Inpar::CONTACT::solution_penalty ||
          Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
              Inpar::CONTACT::solution_nitsche) &&
      contact.get<double>("PENALTYPARAM") <= 0.0)
    FOUR_C_THROW("Penalty parameter eps = 0, must be greater than 0");

  if ((Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
              Inpar::CONTACT::solution_penalty ||
          Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
              Inpar::CONTACT::solution_nitsche) &&
      Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") !=
          Inpar::CONTACT::friction_none &&
      contact.get<double>("PENALTYPARAMTAN") <= 0.0)
    FOUR_C_THROW("Tangential penalty parameter eps = 0, must be greater than 0");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
          Inpar::CONTACT::solution_uzawa &&
      contact.get<double>("PENALTYPARAM") <= 0.0)
    FOUR_C_THROW("Penalty parameter eps = 0, must be greater than 0");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
          Inpar::CONTACT::solution_uzawa &&
      Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") !=
          Inpar::CONTACT::friction_none &&
      contact.get<double>("PENALTYPARAMTAN") <= 0.0)
    FOUR_C_THROW("Tangential penalty parameter eps = 0, must be greater than 0");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
          Inpar::CONTACT::solution_uzawa &&
      contact.get<int>("UZAWAMAXSTEPS") < 2)
    FOUR_C_THROW("Maximum number of Uzawa / Augmentation steps must be at least 2");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
          Inpar::CONTACT::solution_uzawa &&
      contact.get<double>("UZAWACONSTRTOL") <= 0.0)
    FOUR_C_THROW("Constraint tolerance for Uzawa / Augmentation scheme must be greater than 0");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") !=
          Inpar::CONTACT::friction_none &&
      contact.get<double>("SEMI_SMOOTH_CT") == 0.0)
    FOUR_C_THROW("Parameter ct = 0, must be greater than 0 for frictional contact");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
          Inpar::CONTACT::solution_augmented &&
      contact.get<double>("SEMI_SMOOTH_CN") <= 0.0)
    FOUR_C_THROW("Regularization parameter cn, must be greater than 0 for contact problems");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") ==
          Inpar::CONTACT::friction_tresca &&
      dim == 3 &&
      Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") !=
          Inpar::CONTACT::solution_nitsche)
    FOUR_C_THROW(
        "3D frictional contact with Tresca's law only implemented for nitsche formulation");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") !=
          Inpar::CONTACT::friction_none &&
      Core::UTILS::IntegralValue<int>(contact, "SEMI_SMOOTH_NEWTON") != 1 && dim == 3)
    FOUR_C_THROW("3D frictional contact only implemented with Semi-smooth Newton");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
          Inpar::CONTACT::solution_augmented &&
      Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") !=
          Inpar::CONTACT::friction_none)
    FOUR_C_THROW(
        "Frictional contact is for the augmented Lagrange formulation not yet implemented!");

  if (Core::UTILS::IntegralValue<int>(mortar, "CROSSPOINTS") == true && dim == 3)
    FOUR_C_THROW("Crosspoints / edge node modification not yet implemented for 3D");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") ==
          Inpar::CONTACT::friction_tresca &&
      Core::UTILS::IntegralValue<int>(contact, "FRLESS_FIRST") == true)
    // hopefully coming soon, when Coulomb and Tresca are combined
    FOUR_C_THROW("Frictionless first contact step with Tresca's law not yet implemented");

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::Regularization>(
          contact, "CONTACT_REGULARIZATION") != Inpar::CONTACT::reg_none &&
      Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") !=
          Inpar::CONTACT::solution_lagmult)
  {
    FOUR_C_THROW(
        "Regularized Contact just available for Dual Mortar Contact with Lagrangean "
        "Multiplier!");
  }

  if (Core::UTILS::IntegralValue<Inpar::CONTACT::Regularization>(
          contact, "CONTACT_REGULARIZATION") != Inpar::CONTACT::reg_none &&
      Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") !=
          Inpar::CONTACT::friction_none)
    FOUR_C_THROW("Regularized Contact for contact with friction not implemented yet!");

  // ---------------------------------------------------------------------
  // warnings
  // ---------------------------------------------------------------------
  if (comm().MyPID() == 0)
  {
    if (mortar.get<double>("SEARCH_PARAM") == 0.0)
      std::cout << ("Warning: Contact search called without inflation of bounding volumes\n")
                << std::endl;

    if (Core::UTILS::IntegralValue<Inpar::Wear::WearSide>(wearlist, "WEAR_SIDE") !=
        Inpar::Wear::wear_slave)
      std::cout << ("\n \n Warning: Contact with both-sided wear is still experimental !")
                << std::endl;
  }

  // ---------------------------------------------------------------------
  //                       MORTAR-SPECIFIC CHECKS
  // ---------------------------------------------------------------------
  if (Core::UTILS::IntegralValue<Inpar::Mortar::AlgorithmType>(mortar, "ALGORITHM") ==
      Inpar::Mortar::algorithm_mortar)
  {
    // ---------------------------------------------------------------------
    // invalid parameter combinations
    // ---------------------------------------------------------------------
    if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") !=
            Inpar::CONTACT::solution_lagmult &&
        Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(mortar, "LM_SHAPEFCN") ==
            Inpar::Mortar::shape_petrovgalerkin)
      FOUR_C_THROW("Petrov-Galerkin approach for LM only with Lagrange multiplier strategy");

    if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
            Inpar::CONTACT::solution_lagmult &&
        (Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(mortar, "LM_SHAPEFCN") ==
                Inpar::Mortar::shape_standard &&
            Core::UTILS::IntegralValue<Inpar::Mortar::LagMultQuad>(mortar, "LM_QUAD") !=
                Inpar::Mortar::lagmult_const) &&
        Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(contact, "SYSTEM") ==
            Inpar::CONTACT::system_condensed)
      FOUR_C_THROW("Condensation of linear system only possible for dual Lagrange multipliers");

    if (Core::UTILS::IntegralValue<int>(mortar, "LM_DUAL_CONSISTENT") == true &&
        Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") !=
            Inpar::CONTACT::solution_lagmult &&
        Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(mortar, "LM_SHAPEFCN") !=
            Inpar::Mortar::shape_standard)
    {
      FOUR_C_THROW(
          "Consistent dual shape functions in boundary elements only for Lagrange "
          "multiplier strategy.");
    }

    if (Core::UTILS::IntegralValue<int>(mortar, "LM_DUAL_CONSISTENT") == true &&
        Core::UTILS::IntegralValue<Inpar::Mortar::IntType>(mortar, "INTTYPE") ==
            Inpar::Mortar::inttype_elements &&
        (Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(mortar, "LM_SHAPEFCN") ==
            Inpar::Mortar::shape_dual))
    {
      FOUR_C_THROW(
          "Consistent dual shape functions in boundary elements not for purely "
          "element-based integration.");
    }

    if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
            Inpar::CONTACT::solution_augmented &&
        Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(mortar, "LM_SHAPEFCN") ==
            Inpar::Mortar::shape_dual)
      FOUR_C_THROW("The augmented Lagrange formulation does not support dual shape functions.");

    // ---------------------------------------------------------------------
    // not (yet) implemented combinations
    // ---------------------------------------------------------------------

    if (Core::UTILS::IntegralValue<int>(mortar, "CROSSPOINTS") == true &&
        Core::UTILS::IntegralValue<Inpar::Mortar::LagMultQuad>(mortar, "LM_QUAD") ==
            Inpar::Mortar::lagmult_lin)
      FOUR_C_THROW("Crosspoints and linear LM interpolation for quadratic FE not yet compatible");

    // check for self contact
    std::vector<Core::Conditions::Condition*> contactConditions(0);
    discret().GetCondition("Mortar", contactConditions);
    bool self = false;

    for (const auto& condition : contactConditions)
    {
      const auto& side = condition->parameters().Get<std::string>("Side");
      if (side == "Selfcontact") self = true;
    }

    if (self && Teuchos::getIntegralValue<Inpar::Mortar::ParallelRedist>(mortarParallelRedistParams,
                    "PARALLEL_REDIST") != Inpar::Mortar::ParallelRedist::redist_none)
      FOUR_C_THROW("Self contact and parallel redistribution not yet compatible");

    if (Core::UTILS::IntegralValue<int>(contact, "INITCONTACTBYGAP") == true &&
        contact.get<double>("INITCONTACTGAPVALUE") == 0.0)
      FOUR_C_THROW(
          "For initialization of init contact with gap, the INITCONTACTGAPVALUE is needed.");

    if (Core::UTILS::IntegralValue<int>(mortar, "LM_DUAL_CONSISTENT") == true &&
        Core::UTILS::IntegralValue<Inpar::Mortar::LagMultQuad>(mortar, "LM_QUAD") !=
            Inpar::Mortar::lagmult_undefined &&
        distype != Core::FE::ShapeFunctionType::nurbs)
    {
      FOUR_C_THROW(
          "Consistent dual shape functions in boundary elements only for linear shape "
          "functions or NURBS.");
    }

    if (Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(wearlist, "WEARLAW") !=
            Inpar::Wear::wear_none &&
        Core::UTILS::IntegralValue<int>(contact, "FRLESS_FIRST") == true)
      FOUR_C_THROW("Frictionless first contact step with wear not yet implemented");

    if (problemtype != Core::ProblemType::ehl &&
        Core::UTILS::IntegralValue<int>(contact, "REGULARIZED_NORMAL_CONTACT") == true)
      FOUR_C_THROW("Regularized normal contact only implemented for EHL");

    // ---------------------------------------------------------------------
    // Augmented Lagrangian strategy
    // ---------------------------------------------------------------------
    if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") ==
        Inpar::CONTACT::solution_augmented)
    {
      if (Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(mortar, "LM_SHAPEFCN") ==
          Inpar::Mortar::shape_dual)
        FOUR_C_THROW("AUGEMENTED LAGRANGIAN STRATEGY: No support for dual shape functions.");

      if (not Core::UTILS::IntegralValue<int>(contact, "SEMI_SMOOTH_NEWTON"))
      {
        FOUR_C_THROW(
            "AUGEMENTED LAGRANGIAN STRATEGY: Support ony for the semi-smooth Newton "
            "case at the moment!");
      }

      if (Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") ==
          Inpar::CONTACT::friction_tresca)
        FOUR_C_THROW("AUGEMENTED LAGRANGIAN STRATEGY: No frictional contact support!");
    }

    // ---------------------------------------------------------------------
    // thermal-structure-interaction contact
    // ---------------------------------------------------------------------
    if (problemtype == Core::ProblemType::tsi &&
        Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(mortar, "LM_SHAPEFCN") ==
            Inpar::Mortar::shape_standard &&
        Core::UTILS::IntegralValue<Inpar::Mortar::LagMultQuad>(mortar, "LM_QUAD") !=
            Inpar::Mortar::lagmult_const)
      FOUR_C_THROW("Thermal contact only for dual shape functions");

    if (problemtype == Core::ProblemType::tsi &&
        Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(contact, "SYSTEM") !=
            Inpar::CONTACT::system_condensed)
      FOUR_C_THROW("Thermal contact only for dual shape functions with condensed system");

    // no nodal scaling in for thermal-structure-interaction
    if (problemtype == Core::ProblemType::tsi &&
        tsic.get<double>("TEMP_DAMAGE") <= tsic.get<double>("TEMP_REF"))
      FOUR_C_THROW("damage temperature must be greater than reference temperature");

    // ---------------------------------------------------------------------
    // contact with wear
    // ---------------------------------------------------------------------
    if (Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(wearlist, "WEARLAW") ==
            Inpar::Wear::wear_none &&
        wearlist.get<double>("WEARCOEFF") != 0.0)
      FOUR_C_THROW("Wear coefficient only necessary in the context of wear.");

    if (problemtype == Core::ProblemType::structure and
        Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(wearlist, "WEARLAW") !=
            Inpar::Wear::wear_none and
        Core::UTILS::IntegralValue<Inpar::Wear::WearTimInt>(wearlist, "WEARTIMINT") !=
            Inpar::Wear::wear_expl)
    {
      FOUR_C_THROW(
          "Wear calculation for pure structure problems only with explicit internal state "
          "variable approach reasonable!");
    }

    if (Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") ==
            Inpar::CONTACT::friction_none &&
        Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(wearlist, "WEARLAW") !=
            Inpar::Wear::wear_none)
      FOUR_C_THROW("Wear models only applicable to frictional contact.");

    if (Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(wearlist, "WEARLAW") !=
            Inpar::Wear::wear_none &&
        wearlist.get<double>("WEARCOEFF") <= 0.0)
      FOUR_C_THROW("No valid wear coefficient provided, must be equal or greater 0.0");

    if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") !=
            Inpar::CONTACT::solution_lagmult &&
        Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(wearlist, "WEARLAW") !=
            Inpar::Wear::wear_none)
      FOUR_C_THROW("Wear model only applicable in combination with Lagrange multiplier strategy.");

    if (Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") ==
            Inpar::CONTACT::friction_tresca &&
        Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(wearlist, "WEARLAW") !=
            Inpar::Wear::wear_none)
      FOUR_C_THROW("Wear only for Coulomb friction!");

    // ---------------------------------------------------------------------
    // 3D quadratic mortar (choice of interpolation and testing fcts.)
    // ---------------------------------------------------------------------
    if (Core::UTILS::IntegralValue<Inpar::Mortar::LagMultQuad>(mortar, "LM_QUAD") ==
            Inpar::Mortar::lagmult_pwlin &&
        Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(mortar, "LM_SHAPEFCN") ==
            Inpar::Mortar::shape_dual)
    {
      FOUR_C_THROW(
          "No piecewise linear approach (for LM) implemented for quadratic contact with "
          "DUAL shape fct.");
    }

    // ---------------------------------------------------------------------
    // poroelastic contact
    // ---------------------------------------------------------------------
    if ((problemtype == Core::ProblemType::poroelast || problemtype == Core::ProblemType::fpsi ||
            problemtype == Core::ProblemType::fpsi_xfem) &&
        (Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(mortar, "LM_SHAPEFCN") !=
                Inpar::Mortar::shape_dual &&
            Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(mortar, "LM_SHAPEFCN") !=
                Inpar::Mortar::shape_petrovgalerkin))
      FOUR_C_THROW("POROCONTACT: Only dual and petrovgalerkin shape functions implemented yet!");

    if ((problemtype == Core::ProblemType::poroelast || problemtype == Core::ProblemType::fpsi ||
            problemtype == Core::ProblemType::fpsi_xfem) &&
        Teuchos::getIntegralValue<Inpar::Mortar::ParallelRedist>(mortarParallelRedistParams,
            "PARALLEL_REDIST") != Inpar::Mortar::ParallelRedist::redist_none)
      FOUR_C_THROW(
          "POROCONTACT: Parallel Redistribution not implemented yet!");  // Since we use Pointers to
                                                                         // Parent Elements, which
                                                                         // are not copied to other
                                                                         // procs!

    if ((problemtype == Core::ProblemType::poroelast || problemtype == Core::ProblemType::fpsi ||
            problemtype == Core::ProblemType::fpsi_xfem) &&
        Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") !=
            Inpar::CONTACT::solution_lagmult)
      FOUR_C_THROW("POROCONTACT: Use Lagrangean Strategy for poro contact!");

    if ((problemtype == Core::ProblemType::poroelast || problemtype == Core::ProblemType::fpsi ||
            problemtype == Core::ProblemType::fpsi_xfem) &&
        Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(contact, "FRICTION") !=
            Inpar::CONTACT::friction_none)
      FOUR_C_THROW("POROCONTACT: Friction for poro contact not implemented!");

    if ((problemtype == Core::ProblemType::poroelast || problemtype == Core::ProblemType::fpsi ||
            problemtype == Core::ProblemType::fpsi_xfem) &&
        Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(contact, "SYSTEM") !=
            Inpar::CONTACT::system_condensed)
      FOUR_C_THROW("POROCONTACT: System has to be condensed for poro contact!");

    if ((problemtype == Core::ProblemType::poroelast || problemtype == Core::ProblemType::fpsi ||
            problemtype == Core::ProblemType::fpsi_xfem) &&
        (dim != 3) && (dim != 2))
    {
      const Teuchos::ParameterList& porodyn =
          Global::Problem::Instance()->poroelast_dynamic_params();
      if (Core::UTILS::IntegralValue<int>(porodyn, "CONTACTNOPEN"))
        FOUR_C_THROW("POROCONTACT: PoroContact with no penetration just tested for 3d (and 2d)!");
    }

    // ---------------------------------------------------------------------
    // element-based vs. segment-based mortar integration
    // ---------------------------------------------------------------------
    auto inttype = Core::UTILS::IntegralValue<Inpar::Mortar::IntType>(mortar, "INTTYPE");

    if (inttype == Inpar::Mortar::inttype_elements && mortar.get<int>("NUMGP_PER_DIM") <= 0)
      FOUR_C_THROW("Invalid Gauss point number NUMGP_PER_DIM for element-based integration.");

    if (inttype == Inpar::Mortar::inttype_elements_BS && mortar.get<int>("NUMGP_PER_DIM") <= 0)
    {
      FOUR_C_THROW(
          "Invalid Gauss point number NUMGP_PER_DIM for element-based integration with "
          "boundary segmentation."
          "\nPlease note that the value you have to provide only applies to the element-based "
          "integration"
          "\ndomain, while pre-defined default values will be used in the segment-based boundary "
          "domain.");
    }

    if ((inttype == Inpar::Mortar::inttype_elements ||
            inttype == Inpar::Mortar::inttype_elements_BS) &&
        mortar.get<int>("NUMGP_PER_DIM") <= 1)
      FOUR_C_THROW("Invalid Gauss point number NUMGP_PER_DIM for element-based integration.");
  }  // END MORTAR CHECKS

  // ---------------------------------------------------------------------
  //                       NTS-SPECIFIC CHECKS
  // ---------------------------------------------------------------------
  else if (Core::UTILS::IntegralValue<Inpar::Mortar::AlgorithmType>(mortar, "ALGORITHM") ==
           Inpar::Mortar::algorithm_nts)
  {
    if (problemtype == Core::ProblemType::poroelast or problemtype == Core::ProblemType::fpsi or
        problemtype == Core::ProblemType::tsi)
      FOUR_C_THROW("NTS only for problem type: structure");
  }  // END NTS CHECKS

  // ---------------------------------------------------------------------
  //                       GPTS-SPECIFIC CHECKS
  // ---------------------------------------------------------------------
  else if (Core::UTILS::IntegralValue<Inpar::Mortar::AlgorithmType>(mortar, "ALGORITHM") ==
           Inpar::Mortar::algorithm_gpts)
  {
    if (Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") !=
            Inpar::CONTACT::solution_penalty &&
        Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(contact, "STRATEGY") !=
            Inpar::CONTACT::solution_nitsche)
      FOUR_C_THROW("GPTS-Algorithm only with penalty or nitsche strategy");

    if (contact.get<double>("PENALTYPARAM") <= 0.0)
      FOUR_C_THROW("Penalty parameter eps = 0, must be greater than 0");

    if (Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(wearlist, "WEARLAW") !=
        Inpar::Wear::wear_none)
      FOUR_C_THROW("GPTS algorithm not implemented for wear");

    if (Core::UTILS::IntegralValue<Inpar::Mortar::LagMultQuad>(mortar, "LM_QUAD") !=
        Inpar::Mortar::lagmult_undefined)
      FOUR_C_THROW("GPTS algorithm only implemented for first order interpolation");

    if (dim != 3) FOUR_C_THROW("GPTS algorithm only implemented for 3D contact");
  }  // END GPTS CHECKS

  // ---------------------------------------------------------------------
  // store contents of BOTH ParameterLists in local parameter list
  // ---------------------------------------------------------------------
  params.setParameters(mortar);
  params.setParameters(contact);
  params.setParameters(wearlist);
  params.setParameters(tsic);

  switch (problemtype)
  {
    case Core::ProblemType::tsi:
    {
      double timestep = Global::Problem::Instance()->TSIDynamicParams().get<double>("TIMESTEP");
      // rauch 01/16
      if (comm().MyPID() == 0)
      {
        std::cout << "\n \n  Warning: CONTACT::STRATEGY::Factory::read_and_check_input() reads "
                     "TIMESTEP = "
                  << timestep << " from Global::Problem::Instance()->TSIDynamicParams().  \n"
                  << "Anyway, you should not use the \"TIMESTEP\" variable inside of "
                  << "the new structural/contact framework!" << std::endl;
      }
      params.set<double>("TIMESTEP", timestep);
      break;
    }
    case Core::ProblemType::structure:
    {
      params.set<double>("TIMESTEP",
          Global::Problem::Instance()->structural_dynamic_params().get<double>("TIMESTEP"));
      break;
    }
    default:
      /* Do nothing, all the time integration related stuff is supposed to be handled outside
       * of the contact strategy. */
      break;
  }

  // ---------------------------------------------------------------------
  // NURBS contact
  // ---------------------------------------------------------------------
  switch (distype)
  {
    case Core::FE::ShapeFunctionType::nurbs:
    {
      params.set<bool>("NURBS", true);
      break;
    }
    default:
    {
      params.set<bool>("NURBS", false);
      break;
    }
  }

  // ---------------------------------------------------------------------
  params.setName("CONTACT DYNAMIC / MORTAR COUPLING");

  // store relevant problem types
  if (problemtype == Core::ProblemType::tsi)
  {
    params.set<int>("PROBTYPE", Inpar::CONTACT::tsi);
  }
  else if (problemtype == Core::ProblemType::ssi)
  {
    if (Teuchos::getIntegralValue<Inpar::SSI::ScaTraTimIntType>(
            Global::Problem::Instance()->SSIControlParams(), "SCATRATIMINTTYPE") ==
        Inpar::SSI::ScaTraTimIntType::elch)
    {
      params.set<int>("PROBTYPE", Inpar::CONTACT::ssi_elch);
    }
    else
    {
      params.set<int>("PROBTYPE", Inpar::CONTACT::ssi);
    }
  }
  else if (problemtype == Core::ProblemType::struct_ale)
  {
    params.set<int>("PROBTYPE", Inpar::CONTACT::structalewear);
  }
  else if (problemtype == Core::ProblemType::poroelast or problemtype == Core::ProblemType::fpsi or
           problemtype == Core::ProblemType::fpsi_xfem)
  {
    FOUR_C_THROW(
        "Everything which is related to a special time integration scheme has to be moved to the"
        " related scheme. Don't do it here! -- hiermeier 02/2016");
    const Teuchos::ParameterList& porodyn = Global::Problem::Instance()->poroelast_dynamic_params();
    params.set<int>("PROBTYPE", Inpar::CONTACT::poroelast);
    //    //porotimefac = 1/(theta*dt) --- required for derivation of structural displacements!
    //    double porotimefac = 1/(stru.sublist("ONESTEPTHETA").get<double>("THETA") *
    //    stru.get<double>("TIMESTEP")); params.set<double> ("porotimefac", porotimefac);
    params.set<bool>("CONTACTNOPEN",
        Core::UTILS::IntegralValue<int>(porodyn, "CONTACTNOPEN"));  // used in the integrator
  }
  else if (problemtype == Core::ProblemType::fsi_xfem)
  {
    params.set<int>("PROBTYPE", Inpar::CONTACT::fsi);
  }
  else if (problemtype == Core::ProblemType::fpsi_xfem)
  {
    FOUR_C_THROW(
        "Everything which is related to a special time integration scheme has to be moved to the"
        " related scheme. Don't do it here! -- hiermeier 02/2016");
    const Teuchos::ParameterList& porodyn = Global::Problem::Instance()->poroelast_dynamic_params();
    params.set<int>("PROBTYPE", Inpar::CONTACT::fpi);
    //    //porotimefac = 1/(theta*dt) --- required for derivation of structural displacements!
    //    double porotimefac = 1/(stru.sublist("ONESTEPTHETA").get<double>("THETA") *
    //    stru.get<double>("TIMESTEP")); params.set<double> ("porotimefac", porotimefac);
    params.set<bool>("CONTACTNOPEN",
        Core::UTILS::IntegralValue<int>(porodyn, "CONTACTNOPEN"));  // used in the integrator
  }
  else
  {
    params.set<int>("PROBTYPE", Inpar::CONTACT::other);
  }

  // no parallel redistribution in the serial case
  if (comm().NumProc() == 1)
    params.sublist("PARALLEL REDISTRIBUTION").set<std::string>("PARALLEL_REDIST", "None");

  // console output at the end
  if (comm().MyPID() == 0) std::cout << "done!" << std::endl;

  // set dimension
  params.set<int>("DIMENSION", dim);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::STRATEGY::Factory::BuildInterfaces(const Teuchos::ParameterList& params,
    std::vector<Teuchos::RCP<CONTACT::Interface>>& interfaces, bool& poroslave,
    bool& poromaster) const
{
  // start building interfaces
  if (comm().MyPID() == 0)
  {
    std::cout << "Building contact interface(s)..............." << std::endl;
    fflush(stdout);
  }

  // Vector that solely contains solid-to-solid contact pairs
  std::vector<std::vector<Core::Conditions::Condition*>> ccond_grps(0);
  CONTACT::UTILS::GetContactConditionGroups(ccond_grps, discret());

  std::set<const Core::Nodes::Node*> dbc_slave_nodes;
  std::set<const Core::Elements::Element*> dbc_slave_eles;
  CONTACT::UTILS::DbcHandler::detect_dbc_slave_nodes_and_elements(
      discret(), ccond_grps, dbc_slave_nodes, dbc_slave_eles);

  // maximum dof number in discretization
  // later we want to create NEW Lagrange multiplier degrees of
  // freedom, which of course must not overlap with displacement dofs
  int maxdof = discret().dof_row_map()->MaxAllGID();

  // get input par.
  auto stype = Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(params, "STRATEGY");
  auto wlaw = Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(params, "WEARLAW");
  auto constr_direction = Core::UTILS::IntegralValue<Inpar::CONTACT::ConstraintDirection>(
      params, "CONSTRAINT_DIRECTIONS");
  auto ftype = Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(params, "FRICTION");
  auto ad = Core::UTILS::IntegralValue<Inpar::CONTACT::AdhesionType>(params, "ADHESION");
  auto algo = Core::UTILS::IntegralValue<Inpar::Mortar::AlgorithmType>(params, "ALGORITHM");

  bool friplus = false;
  if ((wlaw != Inpar::Wear::wear_none) || (params.get<int>("PROBTYPE") == Inpar::CONTACT::tsi))
    friplus = true;

  // only for poro
  bool isporo = (params.get<int>("PROBTYPE") == Inpar::CONTACT::poroelast) ||
                (params.get<int>("PROBTYPE") == Inpar::CONTACT::poroscatra);
  bool structmaster = false;
  bool structslave = false;
  bool isanyselfcontact = false;
  enum Mortar::Element::PhysicalType slavetype = Mortar::Element::other;
  enum Mortar::Element::PhysicalType mastertype = Mortar::Element::other;

  // loop over all contact condition groups
  for (auto& currentgroup : ccond_grps)
  {
    // initialize a reference to the i-th contact condition group
    const auto groupid1 = currentgroup[0]->parameters().Get<int>("Interface ID");

    // In case of MultiScale contact this is the id of the interface's constitutive contact law
    int contactconstitutivelaw_id = currentgroup[0]->parameters().Get<int>("ConstitutiveLawID");

    // Initialize a flag to check for MIRCO contact consitutive law
    bool mircolaw = false;

    // Initialize the variables to create a RoughNode for MIRCO
    int resolution = 0;
    bool randomtopologyflag = false;
    bool randomseedflag = false;
    int randomgeneratorseed = 0;
    int hurstexponentfunction = 0;
    int initialtopologystddeviationfunction = 0;

    if (contactconstitutivelaw_id != 0)
    {
      const int probinst =
          Global::Problem::Instance()->contact_constitutive_laws()->GetReadFromProblem();
      auto coconstlaw = Global::Problem::Instance(probinst)->contact_constitutive_laws()->ById(
          contactconstitutivelaw_id);
      // Set the variables if MIRCO contact constitutive law is found
      if (coconstlaw->Name() == "CoConstLaw_mirco")
      {
        mircolaw = true;
        resolution = coconstlaw->Get<int>("Resolution");
        randomtopologyflag = coconstlaw->Get<bool>("RandomTopologyFlag");
        randomseedflag = coconstlaw->Get<bool>("RandomSeedFlag");
        randomgeneratorseed = coconstlaw->Get<int>("RandomGeneratorSeed");
        hurstexponentfunction = coconstlaw->Get<int>("HurstExponentFunct");
        initialtopologystddeviationfunction =
            coconstlaw->Get<int>("InitialTopologyStdDeviationFunct");
      }
    }

    // find out which sides are Master and Slave
    std::vector<bool> isslave(0);
    std::vector<bool> isself(0);
    CONTACT::UTILS::GetMasterSlaveSideInfo(isslave, isself, currentgroup);
    for (const bool is : isself)
    {
      if (is)
      {
        isanyselfcontact = true;
        break;
      }
    }

    // find out which sides are initialized as In/Active and other initalization data
    std::vector<bool> isactive(currentgroup.size());
    bool Two_half_pass(false);
    bool Check_nonsmooth_selfcontactsurface(false);
    bool Searchele_AllProc(false);

    CONTACT::UTILS::GetInitializationInfo(Two_half_pass, Check_nonsmooth_selfcontactsurface,
        Searchele_AllProc, isactive, isslave, isself, currentgroup);

    // create interface local parameter list (copy)
    Teuchos::ParameterList icparams = params;

    // find out if interface-specific coefficients of friction are given
    if (ftype == Inpar::CONTACT::friction_tresca or ftype == Inpar::CONTACT::friction_coulomb or
        ftype == Inpar::CONTACT::friction_stick)
    {
      // read interface COFs
      std::vector<double> frcoeff(currentgroup.size());
      for (std::size_t j = 0; j < currentgroup.size(); ++j)
        frcoeff[j] = currentgroup[j]->parameters().Get<double>("FrCoeffOrBound");

      // check consistency of interface COFs
      for (std::size_t j = 1; j < currentgroup.size(); ++j)
        if (frcoeff[j] != frcoeff[0])
          FOUR_C_THROW("Inconsistency in friction coefficients of interface %i", groupid1);

      // check for infeasible value of COF
      if (frcoeff[0] < 0.0) FOUR_C_THROW("Negative FrCoeff / FrBound on interface %i", groupid1);

      // add COF locally to contact parameter list of this interface
      if (ftype == Inpar::CONTACT::friction_tresca)
      {
        icparams.setEntry("FRBOUND", static_cast<Teuchos::ParameterEntry>(frcoeff[0]));
        icparams.setEntry("FRCOEFF", static_cast<Teuchos::ParameterEntry>(-1.0));
      }
      else if (ftype == Inpar::CONTACT::friction_coulomb)
      {
        icparams.setEntry("FRCOEFF", static_cast<Teuchos::ParameterEntry>(frcoeff[0]));
        icparams.setEntry("FRBOUND", static_cast<Teuchos::ParameterEntry>(-1.0));
      }
      // dummy values for FRCOEFF and FRBOUND have to be set,
      // since entries are accessed regardless of the friction law
      else if (ftype == Inpar::CONTACT::friction_stick)
      {
        icparams.setEntry("FRCOEFF", static_cast<Teuchos::ParameterEntry>(-1.0));
        icparams.setEntry("FRBOUND", static_cast<Teuchos::ParameterEntry>(-1.0));
      }
    }

    // find out if interface-specific coefficients of adhesion are given
    if (ad == Inpar::CONTACT::adhesion_bound)
    {
      // read interface COFs
      std::vector<double> ad_bound(currentgroup.size());
      for (std::size_t j = 0; j < currentgroup.size(); ++j)
        ad_bound[j] = currentgroup[j]->parameters().Get<double>("AdhesionBound");

      // check consistency of interface COFs
      for (std::size_t j = 1; j < currentgroup.size(); ++j)
        if (ad_bound[j] != ad_bound[0])
          FOUR_C_THROW("Inconsistency in adhesion bounds of interface %i", groupid1);

      // check for infeasible value of COF
      if (ad_bound[0] < 0.0) FOUR_C_THROW("Negative adhesion bound on interface %i", groupid1);

      // add COF locally to contact parameter list of this interface
      icparams.setEntry("ADHESION_BOUND", static_cast<Teuchos::ParameterEntry>(ad_bound[0]));
    }

    // add information to contact parameter list of this interface
    icparams.set<bool>("Two_half_pass", Two_half_pass);
    icparams.set<bool>("Check_nonsmooth_selfcontactsurface", Check_nonsmooth_selfcontactsurface);
    icparams.set<bool>("Searchele_AllProc", Searchele_AllProc);

    set_parameters_for_contact_condition(groupid1, icparams);

    // for structural contact we currently choose redundant master storage
    // the only exception is self contact where a redundant slave is needed, too
    auto redundant = Teuchos::getIntegralValue<Inpar::Mortar::ExtendGhosting>(
        icparams.sublist("PARALLEL REDISTRIBUTION"), "GHOSTING_STRATEGY");
    if (isanyselfcontact && redundant != Inpar::Mortar::ExtendGhosting::redundant_all)
      FOUR_C_THROW("Self contact requires fully redundant slave and master storage");

    // ------------------------------------------------------------------------
    // create the desired interface object
    // ------------------------------------------------------------------------
    const auto& non_owning_discret =
        Teuchos::rcp<const Core::FE::Discretization>(&discret(), false);

    Teuchos::RCP<CONTACT::Interface> newinterface = CreateInterface(groupid1, comm(), dim(),
        icparams, isself[0], non_owning_discret, Teuchos::null, contactconstitutivelaw_id);
    interfaces.push_back(newinterface);

    // get it again
    const Teuchos::RCP<CONTACT::Interface>& interface = interfaces.back();

    /* note that the nodal ids are unique because they come from
     * one global problem discretization containing all nodes of the
     * contact interface.
     * We rely on this fact, therefore it is not possible to
     * do contact between two distinct discretizations here. */

    // collect all intial active nodes
    std::vector<int> initialactive_nodeids;

    //-------------------------------------------------- process nodes
    for (int j = 0; j < (int)currentgroup.size(); ++j)
    {
      // get all nodes and add them
      const std::vector<int>* nodeids = currentgroup[j]->GetNodes();
      if (!nodeids) FOUR_C_THROW("Condition does not have Node Ids");
      for (int gid : *nodeids)
      {
        // do only nodes that I have in my discretization
        if (!discret().HaveGlobalNode(gid)) continue;
        Core::Nodes::Node* node = discret().gNode(gid);
        if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);

        if (node->NumElement() == 0)
        {
          FOUR_C_THROW(
              "surface node without adjacent element detected! "
              "(node-id = %d)",
              node->Id());
        }

        const bool nurbs = Core::FE::IsNurbsDisType(node->Elements()[0]->Shape());
        for (unsigned elid = 0; elid < static_cast<unsigned>(node->NumElement()); ++elid)
        {
          const Core::Elements::Element* adj_ele = node->Elements()[elid];
          if (nurbs != Core::FE::IsNurbsDisType(adj_ele->Shape()))
          {
            FOUR_C_THROW(
                "There are NURBS and non-NURBS adjacent elements to this "
                "node. What shall be done?");
          }
        }

        // skip dbc slave nodes ( if the corresponding option is set for
        // the slave condition )
        if (dbc_slave_nodes.find(node) != dbc_slave_nodes.end()) continue;

        // store initial active node gids
        if (isactive[j]) initialactive_nodeids.push_back(gid);

        /* find out if this node is initial active on another Condition
         * and do NOT overwrite this status then! */
        bool foundinitialactive = false;
        if (!isactive[j])
        {
          for (int initialactive_nodeid : initialactive_nodeids)
          {
            if (gid == initialactive_nodeid)
            {
              foundinitialactive = true;
              break;
            }
          }
        }

        /* create Node object or FriNode object in the frictional case
         * for the boolean variable initactive we use isactive[j]+foundinitialactive,
         * as this is true for BOTH initial active nodes found for the first time
         * and found for the second, third, ... time! */
        if (ftype != Inpar::CONTACT::friction_none)
        {
          Teuchos::RCP<CONTACT::FriNode> cnode =
              Teuchos::rcp(new CONTACT::FriNode(node->Id(), node->X(), node->Owner(),
                  discret().Dof(0, node), isslave[j], isactive[j] + foundinitialactive, friplus));
          //-------------------
          // get nurbs weight!
          if (nurbs)
          {
            prepare_nurbs_node(node, cnode);
          }

          // get edge and corner information:
          std::vector<Core::Conditions::Condition*> contactCornerConditions(0);
          discret().GetCondition("mrtrcorner", contactCornerConditions);
          for (auto& condition : contactCornerConditions)
          {
            if (condition->ContainsNode(node->Id()))
            {
              cnode->SetOnCorner() = true;
            }
          }
          std::vector<Core::Conditions::Condition*> contactEdgeConditions(0);
          discret().GetCondition("mrtredge", contactEdgeConditions);
          for (auto& condition : contactEdgeConditions)
          {
            if (condition->ContainsNode(node->Id()))
            {
              cnode->SetOnEdge() = true;
            }
          }

          // Check, if this node (and, in case, which dofs) are in the contact symmetry condition
          std::vector<Core::Conditions::Condition*> contactSymConditions(0);
          discret().GetCondition("mrtrsym", contactSymConditions);

          for (auto& condition : contactSymConditions)
          {
            if (condition->ContainsNode(node->Id()))
            {
              const auto& onoff = condition->parameters().Get<std::vector<int>>("onoff");
              for (unsigned k = 0; k < onoff.size(); k++)
                if (onoff.at(k) == 1) cnode->DbcDofs()[k] = true;
              if (stype == Inpar::CONTACT::solution_lagmult &&
                  constr_direction != Inpar::CONTACT::constr_xyz)
              {
                FOUR_C_THROW(
                    "Contact symmetry with Lagrange multiplier method"
                    " only with contact constraints in xyz direction.\n"
                    "Set CONSTRAINT_DIRECTIONS to xyz in CONTACT input section");
              }
            }
          }

          /* note that we do not have to worry about double entries
           * as the AddNode function can deal with this case!
           * the only problem would have occurred for the initial active nodes,
           * as their status could have been overwritten, but is prevented
           * by the "foundinitialactive" block above! */
          interface->AddNode(cnode);
        }
        else
        {
          Teuchos::RCP<CONTACT::Node> cnode;
          if (mircolaw == true)
          {
            cnode = Teuchos::rcp(new CONTACT::RoughNode(node->Id(), node->X(), node->Owner(),
                discret().Dof(0, node), isslave[j], isactive[j] + foundinitialactive,
                hurstexponentfunction, initialtopologystddeviationfunction, resolution,
                randomtopologyflag, randomseedflag, randomgeneratorseed));
          }
          else
          {
            cnode = Teuchos::rcp(new CONTACT::Node(node->Id(), node->X(), node->Owner(),
                discret().Dof(0, node), isslave[j], isactive[j] + foundinitialactive));
          }

          //-------------------
          // get nurbs weight!
          if (nurbs)
          {
            prepare_nurbs_node(node, cnode);
          }

          // get edge and corner information:
          std::vector<Core::Conditions::Condition*> contactCornerConditions(0);
          discret().GetCondition("mrtrcorner", contactCornerConditions);
          for (auto& condition : contactCornerConditions)
          {
            if (condition->ContainsNode(node->Id()))
            {
              cnode->SetOnCorner() = true;
            }
          }
          std::vector<Core::Conditions::Condition*> contactEdgeConditions(0);
          discret().GetCondition("mrtredge", contactEdgeConditions);
          for (auto& condition : contactEdgeConditions)
          {
            if (condition->ContainsNode(node->Id()))
            {
              cnode->SetOnEdge() = true;
            }
          }

          // Check, if this node (and, in case, which dofs) are in the contact symmetry condition
          std::vector<Core::Conditions::Condition*> contactSymConditions(0);
          discret().GetCondition("mrtrsym", contactSymConditions);

          for (auto& condition : contactSymConditions)
          {
            if (condition->ContainsNode(node->Id()))
            {
              const auto& onoff = condition->parameters().Get<std::vector<int>>("onoff");
              for (unsigned k = 0; k < onoff.size(); k++)
              {
                if (onoff.at(k) == 1)
                {
                  cnode->DbcDofs()[k] = true;
                  if (stype == Inpar::CONTACT::solution_lagmult &&
                      constr_direction != Inpar::CONTACT::constr_xyz)
                  {
                    FOUR_C_THROW(
                        "Contact symmetry with Lagrange multiplier method"
                        " only with contact constraints in xyz direction.\n"
                        "Set CONSTRAINT_DIRECTIONS to xyz in CONTACT input section");
                  }
                }
              }
            }
          }

          /* note that we do not have to worry about double entries
           * as the AddNode function can deal with this case!
           * the only problem would have occurred for the initial active nodes,
           * as their status could have been overwritten, but is prevented
           * by the "foundinitialactive" block above! */
          interface->AddNode(cnode);
        }
      }
    }

    //----------------------------------------------- process elements
    int ggsize = 0;
    for (std::size_t j = 0; j < currentgroup.size(); ++j)
    {
      // get elements from condition j of current group
      std::map<int, Teuchos::RCP<Core::Elements::Element>>& currele = currentgroup[j]->Geometry();

      /* elements in a boundary condition have a unique id
       * but ids are not unique among 2 distinct conditions
       * due to the way elements in conditions are build.
       * We therefore have to give the second, third,... set of elements
       * different ids. ids do not have to be continuous, we just add a large
       * enough number ggsize to all elements of cond2, cond3,... so they are
       * different from those in cond1!!!
       * note that elements in ele1/ele2 already are in column (overlapping) map */

      /* We count only elements, which are owned by the processor. In this way
       * the element ids stay the same for more than one processor in use.
       * hiermeier 02/2016 */
      int lsize = 0;
      std::map<int, Teuchos::RCP<Core::Elements::Element>>::iterator fool;
      for (fool = currele.begin(); fool != currele.end(); ++fool)
        if (fool->second->Owner() == comm().MyPID()) ++lsize;

      int gsize = 0;
      comm().SumAll(&lsize, &gsize, 1);

      bool nurbs = false;
      if (currele.size() > 0) nurbs = Core::FE::IsNurbsDisType(currele.begin()->second->Shape());

      for (fool = currele.begin(); fool != currele.end(); ++fool)
      {
        Teuchos::RCP<Core::Elements::Element> ele = fool->second;
        if (Core::FE::IsNurbsDisType(ele->Shape()) != nurbs)
        {
          FOUR_C_THROW(
              "All elements of one interface side (i.e. slave or master) "
              "must be NURBS or LAGRANGE elements. A mixed NURBS/Lagrange "
              "discretizations on one side of the interface is currently "
              "unsupported.");
        }

        // skip dbc slave elements ( if the corresponding option is set for
        // the slave condition )
        if (dbc_slave_eles.find(ele.get()) != dbc_slave_eles.end()) continue;

        Teuchos::RCP<CONTACT::Element> cele = Teuchos::rcp(new CONTACT::Element(ele->Id() + ggsize,
            ele->Owner(), ele->Shape(), ele->num_node(), ele->NodeIds(), isslave[j], nurbs));

        if (isporo) set_poro_parent_element(slavetype, mastertype, cele, ele, discret());

        if (algo == Inpar::Mortar::algorithm_gpts)
        {
          Teuchos::RCP<Core::Elements::FaceElement> faceele =
              Teuchos::rcp_dynamic_cast<Core::Elements::FaceElement>(ele, true);
          if (faceele == Teuchos::null) FOUR_C_THROW("Cast to FaceElement failed!");
          if (faceele->parent_element() == nullptr) FOUR_C_THROW("face parent does not exist");
          if (discret().ElementColMap()->LID(faceele->parent_element()->Id()) == -1)
            FOUR_C_THROW("vol dis does not have parent ele");
          cele->set_parent_master_element(faceele->parent_element(), faceele->FaceParentNumber());
        }

        //------------------------------------------------------------------
        // get knotvector, normal factor and zero-size information for nurbs
        if (nurbs)
        {
          prepare_nurbs_element(discret(), ele, cele);
        }

        interface->add_element(cele);
      }  // for (fool=ele1.start(); fool != ele1.end(); ++fool)

      ggsize += gsize;  // update global element counter
    }

    //-------------------- finalize the contact interface construction
    interface->fill_complete(true, maxdof);

    if (isporo)
      find_poro_interface_types(
          poromaster, poroslave, structmaster, structslave, slavetype, mastertype);

  }  // for (int i=0; i<(int)contactconditions.size(); ++i)

  if (not isanyselfcontact) fully_overlapping_interfaces(interfaces);

  // finish the interface creation
  if (comm().MyPID() == 0) std::cout << "done!" << std::endl;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::STRATEGY::Factory::fully_overlapping_interfaces(
    std::vector<Teuchos::RCP<CONTACT::Interface>>& interfaces) const
{
  int ocount = 0;
  for (auto it = interfaces.begin(); it != interfaces.end(); ++it, ++ocount)
  {
    Interface& interface = **it;

    const Epetra_Map& srownodes_i = *interface.SlaveRowNodes();
    const Epetra_Map& mrownodes_i = *interface.MasterRowNodes();

    for (auto iit = (it + 1); iit != interfaces.end(); ++iit)
    {
      Interface& iinterface = **iit;

      const Epetra_Map& srownodes_ii = *iinterface.SlaveRowNodes();
      const Epetra_Map& mrownodes_ii = *iinterface.MasterRowNodes();

      const int sl_fullsubset_id = identify_full_subset(srownodes_i, srownodes_ii);
      if (sl_fullsubset_id != -1)
        FOUR_C_THROW("Currently the slave element maps are not allowed to overlap!");

      const int ma_fullsubset_id = identify_full_subset(mrownodes_i, mrownodes_ii);

      // handle fully overlapping master interfaces
      if (ma_fullsubset_id == 0)
        interface.add_ma_sharing_ref_interface(&iinterface);
      else if (ma_fullsubset_id == 1)
        iinterface.add_ma_sharing_ref_interface(&interface);
    }
  }

  for (const auto& inter : interfaces)
  {
    if (inter->has_ma_sharing_ref_interface())
    {
      Core::IO::cout << "master side of mortar interface #" << inter->Id()
                     << " is fully overlapping with the master side of interface #"
                     << inter->get_ma_sharing_ref_interface().Id() << Core::IO::endl;
    }
  }

  for (auto& interface : interfaces)
    Core::IO::cout << interface->Id() << " has_ma_sharing_ref_interface  = "
                   << (interface->has_ma_sharing_ref_interface() ? "TRUE" : "FALSE")
                   << Core::IO::endl;

  // resort the interface vector via a short bubble sort:
  /* Move all interfaces with a shared reference interface to the end of the
   * vector */
  for (auto it = interfaces.begin(); it != interfaces.end(); ++it)
  {
    if ((*it)->has_ma_sharing_ref_interface())
    {
      for (auto iit = it + 1; iit != interfaces.end(); ++iit)
      {
        if (not(*iit)->has_ma_sharing_ref_interface())
        {
          std::swap(*it, *iit);
          break;
        }
      }
    }
  }

  Core::IO::cout << "After sorting:\n";
  for (auto& interface : interfaces)
    Core::IO::cout << interface->Id() << " has_ma_sharing_ref_interface  = "
                   << (interface->has_ma_sharing_ref_interface() ? "TRUE" : "FALSE")
                   << Core::IO::endl;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
int CONTACT::STRATEGY::Factory::identify_full_subset(
    const Epetra_Map& map_0, const Epetra_Map& map_1, bool throw_if_partial_subset_on_proc) const
{
  const Epetra_Map* ref_map = nullptr;
  const Epetra_Map* sub_map = nullptr;

  int sub_id = -1;

  if (map_0.NumGlobalElements() >= map_1.NumGlobalElements())
  {
    ref_map = &map_0;

    sub_id = 1;
    sub_map = &map_1;
  }
  else
  {
    ref_map = &map_1;

    sub_id = 0;
    sub_map = &map_0;
  }

  const unsigned nummysubentries = sub_map->NumMyElements();
  const int* mysubgids = sub_map->MyGlobalElements();

  bool is_fullsubmap = false;
  for (unsigned i = 0; i < nummysubentries; ++i)
  {
    if (i == 0 and ref_map->MyGID(mysubgids[i]))
      is_fullsubmap = true;
    else if (is_fullsubmap != ref_map->MyGID(mysubgids[i]))
    {
      if (throw_if_partial_subset_on_proc)
        FOUR_C_THROW("Partial sub-map detected on proc #%d!", comm().MyPID());
      is_fullsubmap = false;
    }
  }

  if (nummysubentries == 0) is_fullsubmap = true;

  int lfullsubmap = static_cast<int>(is_fullsubmap);
  int gfullsubmap = 0;

  comm().SumAll(&lfullsubmap, &gfullsubmap, 1);

  return (gfullsubmap == comm().NumProc() ? sub_id : -1);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<CONTACT::Interface> CONTACT::STRATEGY::Factory::CreateInterface(const int id,
    const Epetra_Comm& comm, const int dim, Teuchos::ParameterList& icparams,
    const bool selfcontact, const Teuchos::RCP<const Core::FE::Discretization>& parent_dis,
    Teuchos::RCP<CONTACT::InterfaceDataContainer> interfaceData_ptr,
    const int contactconstitutivelaw_id)
{
  auto stype = Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(icparams, "STRATEGY");

  return CreateInterface(stype, id, comm, dim, icparams, selfcontact, parent_dis, interfaceData_ptr,
      contactconstitutivelaw_id);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<CONTACT::Interface> CONTACT::STRATEGY::Factory::CreateInterface(
    const enum Inpar::CONTACT::SolvingStrategy stype, const int id, const Epetra_Comm& comm,
    const int dim, Teuchos::ParameterList& icparams, const bool selfcontact,
    const Teuchos::RCP<const Core::FE::Discretization>& parent_dis,
    Teuchos::RCP<CONTACT::InterfaceDataContainer> interface_data_ptr,
    const int contactconstitutivelaw_id)
{
  Teuchos::RCP<CONTACT::Interface> newinterface = Teuchos::null;

  auto wlaw = Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(icparams, "WEARLAW");

  switch (stype)
  {
    // ------------------------------------------------------------------------
    // Create an augmented contact interface
    // ------------------------------------------------------------------------
    case Inpar::CONTACT::solution_augmented:
    case Inpar::CONTACT::solution_combo:
    {
      if (interface_data_ptr.is_null())
      {
        interface_data_ptr = Teuchos::rcp(new CONTACT::Aug::InterfaceDataContainer());

        newinterface = Teuchos::rcp(
            new CONTACT::Aug::Interface(interface_data_ptr, id, comm, dim, icparams, selfcontact));
      }
      else
      {
        Teuchos::RCP<CONTACT::Aug::InterfaceDataContainer> iaugdata_ptr =
            Teuchos::rcp_dynamic_cast<CONTACT::Aug::InterfaceDataContainer>(
                interface_data_ptr, true);
        newinterface = Teuchos::rcp(new CONTACT::Aug::Interface(iaugdata_ptr));
      }

      break;
    }
    // ------------------------------------------------------------------------
    // Create an augmented steepest ascent contact interface
    // ------------------------------------------------------------------------
    case Inpar::CONTACT::solution_steepest_ascent:
    {
      if (interface_data_ptr.is_null())
      {
        interface_data_ptr = Teuchos::rcp(new CONTACT::Aug::InterfaceDataContainer());

        newinterface = Teuchos::rcp(new CONTACT::Aug::SteepestAscent::Interface(
            interface_data_ptr, id, comm, dim, icparams, selfcontact));
      }
      else
      {
        Teuchos::RCP<CONTACT::Aug::InterfaceDataContainer> iaugdata_ptr =
            Teuchos::rcp_dynamic_cast<CONTACT::Aug::InterfaceDataContainer>(
                interface_data_ptr, true);
        newinterface = Teuchos::rcp(new CONTACT::Aug::SteepestAscent::Interface(iaugdata_ptr));
      }

      break;
    }
    // ------------------------------------------------------------------------
    // Create an augmented steepest ascent contact interface (saddlepoint)
    // ------------------------------------------------------------------------
    case Inpar::CONTACT::solution_steepest_ascent_sp:
    {
      if (interface_data_ptr.is_null())
      {
        interface_data_ptr = Teuchos::rcp(new CONTACT::Aug::InterfaceDataContainer());

        newinterface = Teuchos::rcp(new CONTACT::Aug::Lagrange::Interface(
            interface_data_ptr, id, comm, dim, icparams, selfcontact));
      }
      else
      {
        Teuchos::RCP<CONTACT::Aug::InterfaceDataContainer> iaugdata_ptr =
            Teuchos::rcp_dynamic_cast<CONTACT::Aug::InterfaceDataContainer>(
                interface_data_ptr, true);
        newinterface = Teuchos::rcp(new CONTACT::Aug::Lagrange::Interface(iaugdata_ptr));
      }

      break;
    }
    // ------------------------------------------------------------------------
    // Create a lagrange contact interface (based on the augmented formulation)
    // ------------------------------------------------------------------------
    case Inpar::CONTACT::solution_std_lagrange:
    {
      if (interface_data_ptr.is_null())
      {
        interface_data_ptr = Teuchos::rcp(new CONTACT::Aug::InterfaceDataContainer());

        newinterface = Teuchos::rcp(new CONTACT::Aug::Lagrange::Interface(
            interface_data_ptr, id, comm, dim, icparams, selfcontact));
      }
      else
      {
        Teuchos::RCP<CONTACT::Aug::InterfaceDataContainer> iaugdata_ptr =
            Teuchos::rcp_dynamic_cast<CONTACT::Aug::InterfaceDataContainer>(
                interface_data_ptr, true);
        newinterface = Teuchos::rcp(new CONTACT::Aug::Lagrange::Interface(iaugdata_ptr));
      }

      break;
    }
    case Inpar::CONTACT::solution_multiscale:
      interface_data_ptr = Teuchos::rcp(new CONTACT::InterfaceDataContainer());
      newinterface = Teuchos::rcp(new CONTACT::ConstitutivelawInterface(
          interface_data_ptr, id, comm, dim, icparams, selfcontact, contactconstitutivelaw_id));
      break;
    // ------------------------------------------------------------------------
    // Default case for the wear, TSI and standard Lagrangian case
    // ------------------------------------------------------------------------
    default:
    {
      interface_data_ptr = Teuchos::rcp(new CONTACT::InterfaceDataContainer());

      if (wlaw != Inpar::Wear::wear_none)
      {
        newinterface = Teuchos::rcp(
            new Wear::WearInterface(interface_data_ptr, id, comm, dim, icparams, selfcontact));
      }
      else if (icparams.get<int>("PROBTYPE") == Inpar::CONTACT::tsi &&
               stype == Inpar::CONTACT::solution_lagmult)
      {
        newinterface = Teuchos::rcp(
            new CONTACT::TSIInterface(interface_data_ptr, id, comm, dim, icparams, selfcontact));
      }
      else
        newinterface = Teuchos::rcp(
            new CONTACT::Interface(interface_data_ptr, id, comm, dim, icparams, selfcontact));
      break;
    }
  }

  return newinterface;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::STRATEGY::Factory::set_poro_parent_element(
    enum Mortar::Element::PhysicalType& slavetype, enum Mortar::Element::PhysicalType& mastertype,
    Teuchos::RCP<CONTACT::Element>& cele, Teuchos::RCP<Core::Elements::Element>& ele,
    const Core::FE::Discretization& discret) const
{
  // ints to communicate decision over poro bools between processors on every interface
  // safety check - because there may not be mixed interfaces and structural slave elements
  Teuchos::RCP<Core::Elements::FaceElement> faceele =
      Teuchos::rcp_dynamic_cast<Core::Elements::FaceElement>(ele, true);
  if (faceele == Teuchos::null) FOUR_C_THROW("Cast to FaceElement failed!");
  cele->PhysType() = Mortar::Element::other;
  std::vector<Teuchos::RCP<Core::Conditions::Condition>> poroCondVec;
  discret.GetCondition("PoroCoupling", poroCondVec);
  if (!cele->IsSlave())  // treat an element as a master element if it is no slave element
  {
    for (auto& poroCond : poroCondVec)
    {
      std::map<int, Teuchos::RCP<Core::Elements::Element>>::const_iterator eleitergeometry;
      for (eleitergeometry = poroCond->Geometry().begin();
           eleitergeometry != poroCond->Geometry().end(); ++eleitergeometry)
      {
        if (faceele->parent_element()->Id() == eleitergeometry->second->Id())
        {
          if (mastertype == Mortar::Element::poro)
          {
            FOUR_C_THROW(
                "struct and poro master elements on the same processor - no mixed interface "
                "supported");
          }
          cele->PhysType() = Mortar::Element::poro;
          mastertype = Mortar::Element::poro;
          break;
        }
      }
    }
    if (cele->PhysType() == Mortar::Element::other)
    {
      if (mastertype == Mortar::Element::structure)
        FOUR_C_THROW(
            "struct and poro master elements on the same processor - no mixed interface supported");
      cele->PhysType() = Mortar::Element::structure;
      mastertype = Mortar::Element::structure;
    }
  }
  else if (cele->IsSlave())  // treat an element as slave element if it is one
  {
    for (auto& poroCond : poroCondVec)
    {
      std::map<int, Teuchos::RCP<Core::Elements::Element>>::const_iterator eleitergeometry;
      for (eleitergeometry = poroCond->Geometry().begin();
           eleitergeometry != poroCond->Geometry().end(); ++eleitergeometry)
      {
        if (faceele->parent_element()->Id() == eleitergeometry->second->Id())
        {
          if (slavetype == Mortar::Element::structure)
          {
            FOUR_C_THROW(
                "struct and poro master elements on the same processor - no mixed interface "
                "supported");
          }
          cele->PhysType() = Mortar::Element::poro;
          slavetype = Mortar::Element::poro;
          break;
        }
      }
    }
    if (cele->PhysType() == Mortar::Element::other)
    {
      if (slavetype == Mortar::Element::poro)
        FOUR_C_THROW(
            "struct and poro master elements on the same processor - no mixed interface supported");
      cele->PhysType() = Mortar::Element::structure;
      slavetype = Mortar::Element::structure;
    }
  }
  // store information about parent for porous contact (required for calculation of deformation
  // gradient!) in every contact element although only really needed for phystype poro
  cele->set_parent_master_element(faceele->parent_element(), faceele->FaceParentNumber());
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::STRATEGY::Factory::find_poro_interface_types(bool& poromaster, bool& poroslave,
    bool& structmaster, bool& structslave, enum Mortar::Element::PhysicalType& slavetype,
    enum Mortar::Element::PhysicalType& mastertype) const
{
  // find poro and structure elements when a poro coupling condition is applied on an element
  // and restrict to pure poroelastic or pure structural interfaces' sides.
  //(only poro slave elements AND (only poro master elements or only structure master elements)
  // Tell the contact element which physical type it is to extract PhysType in contact integrator
  // bools to decide which side is structural and which side is poroelastic to manage all 4
  // constellations
  // s-s, p-s, s-p, p-p
  // wait for all processors to determine if they have poro or structural master or slave elements
  comm().Barrier();
  /* FixMe Should become possible for scoped enumeration with C++11,
   * till then we use the shown workaround.
   *  enum Mortar::Element::PhysicalType slaveTypeList[Comm().NumProc()];
   *  enum Mortar::Element::PhysicalType masterTypeList[Comm().NumProc()];
   *  Comm().GatherAll(static_cast<int*>(&slavetype),static_cast<int*>(slaveTypeList.data()),1);
   *  Comm().GatherAll(static_cast<int*>(&mastertype),static_cast<int*>(masterTypeList.data()),1);
   */
  std::vector<int> slaveTypeList(comm().NumProc());
  std::vector<int> masterTypeList(comm().NumProc());
  int int_slavetype = static_cast<int>(slavetype);
  int int_mastertype = static_cast<int>(mastertype);
  comm().GatherAll(&int_slavetype, slaveTypeList.data(), 1);
  comm().GatherAll(&int_mastertype, masterTypeList.data(), 1);
  comm().Barrier();

  for (int i = 0; i < comm().NumProc(); ++i)
  {
    switch (slaveTypeList[i])
    {
      case static_cast<int>(Mortar::Element::other):
        break;
      case static_cast<int>(Mortar::Element::poro):
      {
        if (structslave)
        {
          FOUR_C_THROW(
              "struct and poro slave elements in the same problem - no mixed interface "
              "constellations supported");
        }
        // adjust FOUR_C_THROW text, when more than one interface is supported
        poroslave = true;
        break;
      }
      case static_cast<int>(Mortar::Element::structure):
      {
        if (poroslave)
        {
          FOUR_C_THROW(
              "struct and poro slave elements in the same problem - no mixed interface "
              "constellations supported");
        }
        structslave = true;
        break;
      }
      default:
      {
        FOUR_C_THROW("this cannot happen");
        break;
      }
    }
  }

  for (int i = 0; i < comm().NumProc(); ++i)
  {
    switch (masterTypeList[i])
    {
      case static_cast<int>(Mortar::Element::other):
        break;
      case static_cast<int>(Mortar::Element::poro):
      {
        if (structmaster)
        {
          FOUR_C_THROW(
              "struct and poro master elements in the same problem - no mixed interface "
              "constellations supported");
        }
        // adjust FOUR_C_THROW text, when more than one interface is supported
        poromaster = true;
        break;
      }
      case static_cast<int>(Mortar::Element::structure):
      {
        if (poromaster)
        {
          FOUR_C_THROW(
              "struct and poro master elements in the same problem - no mixed interface "
              "constellations supported");
        }
        structmaster = true;
        break;
      }
      default:
      {
        FOUR_C_THROW("this cannot happen");
        break;
      }
    }
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<CONTACT::AbstractStrategy> CONTACT::STRATEGY::Factory::BuildStrategy(
    const Teuchos::ParameterList& params, const bool& poroslave, const bool& poromaster,
    const int& dof_offset, std::vector<Teuchos::RCP<CONTACT::Interface>>& interfaces,
    CONTACT::ParamsInterface* cparams_interface) const
{
  const auto stype =
      Core::UTILS::IntegralValue<enum Inpar::CONTACT::SolvingStrategy>(params, "STRATEGY");
  Teuchos::RCP<CONTACT::AbstractStratDataContainer> data_ptr = Teuchos::null;

  return BuildStrategy(stype, params, poroslave, poromaster, dof_offset, interfaces,
      discret().dof_row_map(), discret().NodeRowMap(), dim(), comm_ptr(), data_ptr,
      cparams_interface);
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<CONTACT::AbstractStrategy> CONTACT::STRATEGY::Factory::BuildStrategy(
    const Inpar::CONTACT::SolvingStrategy stype, const Teuchos::ParameterList& params,
    const bool& poroslave, const bool& poromaster, const int& dof_offset,
    std::vector<Teuchos::RCP<CONTACT::Interface>>& interfaces, const Epetra_Map* dof_row_map,
    const Epetra_Map* node_row_map, const int dim, const Teuchos::RCP<const Epetra_Comm>& comm_ptr,
    Teuchos::RCP<CONTACT::AbstractStratDataContainer> data_ptr,
    CONTACT::ParamsInterface* cparams_interface)
{
  if (comm_ptr->MyPID() == 0)
  {
    std::cout << "Building contact strategy object............";
    fflush(stdout);
  }
  Teuchos::RCP<CONTACT::AbstractStrategy> strategy_ptr = Teuchos::null;

  // get input par.
  auto wlaw = Core::UTILS::IntegralValue<Inpar::Wear::WearLaw>(params, "WEARLAW");
  auto wtype = Core::UTILS::IntegralValue<Inpar::Wear::WearType>(params, "WEARTYPE");
  auto algo = Core::UTILS::IntegralValue<Inpar::Mortar::AlgorithmType>(params, "ALGORITHM");

  // Set dummy parameter. The correct parameter will be read directly from time integrator. We still
  // need to pass an argument as long as we want to support the same strategy contructor as the old
  // time integration.
  double dummy = -1.0;

  // create LagrangeStrategyWear for wear as non-distinct quantity
  if (stype == Inpar::CONTACT::solution_lagmult && wlaw != Inpar::Wear::wear_none &&
      (wtype == Inpar::Wear::wear_intstate || wtype == Inpar::Wear::wear_primvar))
  {
    data_ptr = Teuchos::rcp(new CONTACT::AbstractStratDataContainer());
    strategy_ptr = Teuchos::rcp(new Wear::LagrangeStrategyWear(
        data_ptr, dof_row_map, node_row_map, params, interfaces, dim, comm_ptr, dummy, dof_offset));
  }
  else if (stype == Inpar::CONTACT::solution_lagmult)
  {
    if (params.get<int>("PROBTYPE") == Inpar::CONTACT::poroelast ||
        params.get<int>("PROBTYPE") == Inpar::CONTACT::poroscatra)
    {
      FOUR_C_THROW("This contact strategy is not yet considered!");
      //      strategy_ptr = Teuchos::rcp(new LagrangeStrategyPoro(
      //          dof_row_map,
      //          node_row_map,
      //          params,
      //          interfaces,
      //          dim,
      //          comm_ptr,
      //          maxdof,
      //          poroslave,
      //          poromaster));
    }
    else if (params.get<int>("PROBTYPE") == Inpar::CONTACT::tsi)
    {
      data_ptr = Teuchos::rcp(new CONTACT::AbstractStratDataContainer());
      strategy_ptr = Teuchos::rcp(new LagrangeStrategyTsi(data_ptr, dof_row_map, node_row_map,
          params, interfaces, dim, comm_ptr, dummy, dof_offset));
    }
    else
    {
      data_ptr = Teuchos::rcp(new CONTACT::AbstractStratDataContainer());
      strategy_ptr = Teuchos::rcp(new LagrangeStrategy(data_ptr, dof_row_map, node_row_map, params,
          interfaces, dim, comm_ptr, dummy, dof_offset));
    }
  }
  else if (((stype == Inpar::CONTACT::solution_penalty or
                stype == Inpar::CONTACT::solution_multiscale) &&
               algo != Inpar::Mortar::algorithm_gpts) &&
           stype != Inpar::CONTACT::solution_uzawa)
  {
    strategy_ptr = Teuchos::rcp(new PenaltyStrategy(
        dof_row_map, node_row_map, params, interfaces, dim, comm_ptr, dummy, dof_offset));
  }
  else if (stype == Inpar::CONTACT::solution_uzawa)
  {
    FOUR_C_THROW("This contact strategy is not yet considered!");
    //    strategy_ptr = Teuchos::rcp(new PenaltyStrategy(
    //        dof_row_map,
    //        node_row_map,
    //        params,
    //        interfaces,
    //        dim,
    //        comm_ptr,
    //        maxdof));
  }
  else if (stype == Inpar::CONTACT::solution_combo)
  {
    data_ptr = Teuchos::rcp(new Aug::DataContainer());

    strategy_ptr = Aug::ComboStrategy::Create(data_ptr, dof_row_map, node_row_map, params,
        interfaces, dim, comm_ptr, dof_offset, cparams_interface);
  }
  else if (stype == Inpar::CONTACT::solution_augmented)
  {
    if (data_ptr.is_null()) data_ptr = Teuchos::rcp(new Aug::DataContainer());

    strategy_ptr = Teuchos::rcp(new Aug::Strategy(
        data_ptr, dof_row_map, node_row_map, params, interfaces, dim, comm_ptr, dof_offset));
  }
  else if (stype == Inpar::CONTACT::solution_steepest_ascent)
  {
    if (data_ptr.is_null()) data_ptr = Teuchos::rcp(new Aug::DataContainer());

    strategy_ptr = Teuchos::rcp(new Aug::SteepestAscent::Strategy(
        data_ptr, dof_row_map, node_row_map, params, interfaces, dim, comm_ptr, dof_offset));
  }
  else if (stype == Inpar::CONTACT::solution_steepest_ascent_sp)
  {
    if (data_ptr.is_null()) data_ptr = Teuchos::rcp(new Aug::DataContainer());

    strategy_ptr = Teuchos::rcp(new Aug::SteepestAscentSaddlePoint::Strategy(
        data_ptr, dof_row_map, node_row_map, params, interfaces, dim, comm_ptr, dof_offset));
  }
  else if (stype == Inpar::CONTACT::solution_std_lagrange)
  {
    if (data_ptr.is_null()) data_ptr = Teuchos::rcp(new Aug::DataContainer());

    strategy_ptr = Teuchos::rcp(new Aug::Lagrange::Strategy(
        data_ptr, dof_row_map, node_row_map, params, interfaces, dim, comm_ptr, dof_offset));
  }
  else if (algo == Inpar::Mortar::algorithm_gpts &&
           (stype == Inpar::CONTACT::solution_nitsche || stype == Inpar::CONTACT::solution_penalty))
  {
    if (params.get<int>("PROBTYPE") == Inpar::CONTACT::tsi)
    {
      data_ptr = Teuchos::rcp(new CONTACT::AbstractStratDataContainer());
      strategy_ptr = Teuchos::rcp(new NitscheStrategyTsi(
          data_ptr, dof_row_map, node_row_map, params, interfaces, dim, comm_ptr, 0, dof_offset));
    }
    else if (params.get<int>("PROBTYPE") == Inpar::CONTACT::ssi)
    {
      data_ptr = Teuchos::rcp(new CONTACT::AbstractStratDataContainer());
      strategy_ptr = Teuchos::rcp(new NitscheStrategySsi(
          data_ptr, dof_row_map, node_row_map, params, interfaces, dim, comm_ptr, 0, dof_offset));
    }
    else if (params.get<int>("PROBTYPE") == Inpar::CONTACT::ssi_elch)
    {
      data_ptr = Teuchos::rcp(new CONTACT::AbstractStratDataContainer());
      strategy_ptr = Teuchos::rcp(new NitscheStrategySsiElch(
          data_ptr, dof_row_map, node_row_map, params, interfaces, dim, comm_ptr, 0, dof_offset));
    }
    else
    {
      data_ptr = Teuchos::rcp(new CONTACT::AbstractStratDataContainer());
      strategy_ptr = Teuchos::rcp(new NitscheStrategy(
          data_ptr, dof_row_map, node_row_map, params, interfaces, dim, comm_ptr, 0, dof_offset));
    }
  }
  else
  {
    FOUR_C_THROW(
        "Unrecognized strategy: \"%s\"", Inpar::CONTACT::SolvingStrategy2String(stype).c_str());
  }

  // setup the stategy object
  strategy_ptr->Setup(false, true);

  if (comm_ptr->MyPID() == 0) std::cout << "done!" << std::endl;

  return strategy_ptr;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::STRATEGY::Factory::BuildSearchTree(
    const std::vector<Teuchos::RCP<CONTACT::Interface>>& interfaces) const
{
  // create binary search tree
  for (const auto& interface : interfaces) interface->CreateSearchTree();
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::STRATEGY::Factory::Print(
    const std::vector<Teuchos::RCP<CONTACT::Interface>>& interfaces,
    const Teuchos::RCP<CONTACT::AbstractStrategy>& strategy_ptr,
    const Teuchos::ParameterList& params) const
{
  // print friction information of interfaces
  if (comm().MyPID() == 0)
  {
    // get input parameter
    auto ftype = Core::UTILS::IntegralValue<Inpar::CONTACT::FrictionType>(params, "FRICTION");

    for (unsigned i = 0; i < interfaces.size(); ++i)
    {
      double checkfrcoeff = 0.0;
      if (ftype == Inpar::CONTACT::friction_tresca)
      {
        checkfrcoeff = interfaces[i]->interface_params().get<double>("FRBOUND");
        std::cout << std::endl << "Interface         " << i + 1 << std::endl;
        std::cout << "FrBound (Tresca)  " << checkfrcoeff << std::endl;
      }
      else if (ftype == Inpar::CONTACT::friction_coulomb)
      {
        checkfrcoeff = interfaces[i]->interface_params().get<double>("FRCOEFF");
        std::cout << std::endl << "Interface         " << i + 1 << std::endl;
        std::cout << "FrCoeff (Coulomb) " << checkfrcoeff << std::endl;
      }
    }
  }

  // print initial parallel redistribution
  for (const auto& interface : interfaces) interface->print_parallel_distribution();

  if (comm().MyPID() == 0)
  {
    PrintStrategyBanner(strategy_ptr->Type());
  }
}


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::STRATEGY::Factory::PrintStrategyBanner(
    const enum Inpar::CONTACT::SolvingStrategy soltype)
{
  // some parameters
  const Teuchos::ParameterList& smortar = Global::Problem::Instance()->mortar_coupling_params();
  const Teuchos::ParameterList& scontact = Global::Problem::Instance()->contact_dynamic_params();
  auto shapefcn = Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(smortar, "LM_SHAPEFCN");
  auto systype = Core::UTILS::IntegralValue<Inpar::CONTACT::SystemType>(scontact, "SYSTEM");
  auto algorithm = Core::UTILS::IntegralValue<Inpar::Mortar::AlgorithmType>(smortar, "ALGORITHM");
  bool nonSmoothGeometries = Core::UTILS::IntegralValue<int>(scontact, "NONSMOOTH_GEOMETRIES");

  if (nonSmoothGeometries)
  {
    if (soltype == Inpar::CONTACT::solution_lagmult)
    {
      Core::IO::cout << "================================================================\n";
      Core::IO::cout << "===== Lagrange Multiplier Strategy =============================\n";
      Core::IO::cout << "===== NONSMOOTH - GEOMETRIES ===================================\n";
      Core::IO::cout << "================================================================\n\n";
    }
    else if (soltype == Inpar::CONTACT::solution_nitsche and
             algorithm == Inpar::Mortar::algorithm_gpts)
    {
      Core::IO::cout << "================================================================\n";
      Core::IO::cout << "===== Gauss-Point-To-Segment approach ==========================\n";
      Core::IO::cout << "===== using Nitsche formulation ================================\n";
      Core::IO::cout << "===== NONSMOOTH - GEOMETRIES ===================================\n";
      Core::IO::cout << "================================================================\n\n";
    }
    else
      FOUR_C_THROW("Invalid system type for contact/meshtying interface smoothing");
  }
  else
  {
    if (algorithm == Inpar::Mortar::algorithm_mortar)
    {
      // saddle point formulation
      if (systype == Inpar::CONTACT::system_saddlepoint)
      {
        if (soltype == Inpar::CONTACT::solution_lagmult &&
            shapefcn == Inpar::Mortar::shape_standard)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Standard Lagrange multiplier strategy ====================\n";
          Core::IO::cout << "===== (Saddle point formulation) ===============================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_lagmult &&
                 shapefcn == Inpar::Mortar::shape_dual)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Dual Lagrange multiplier strategy ========================\n";
          Core::IO::cout << "===== (Saddle point formulation) ===============================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_lagmult &&
                 shapefcn == Inpar::Mortar::shape_petrovgalerkin)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Petrov-Galerkin Lagrange multiplier strategy =============\n";
          Core::IO::cout << "===== (Saddle point formulation) ===============================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_penalty &&
                 shapefcn == Inpar::Mortar::shape_standard)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Standard Penalty strategy ================================\n";
          Core::IO::cout << "===== (Pure displacement formulation) ==========================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_penalty &&
                 shapefcn == Inpar::Mortar::shape_dual)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Dual Penalty strategy ====================================\n";
          Core::IO::cout << "===== (Pure displacement formulation) ==========================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_uzawa &&
                 shapefcn == Inpar::Mortar::shape_standard)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Uzawa Augmented Lagrange strategy ========================\n";
          Core::IO::cout << "===== (Pure displacement formulation) ==========================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_uzawa && shapefcn == Inpar::Mortar::shape_dual)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Dual Uzawa Augmented Lagrange strategy ===================\n";
          Core::IO::cout << "===== (Pure displacement formulation) ==========================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_combo)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Combination of different Solving Strategies ==============\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_augmented)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Augmented Lagrange strategy ==============================\n";
          Core::IO::cout << "===== (Saddle point formulation) ===============================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_std_lagrange)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Standard Lagrange strategy ===============================\n";
          Core::IO::cout << "===== Derived from the Augmented formulation ===================\n";
          Core::IO::cout << "===== (Saddle point formulation) ===============================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_steepest_ascent)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Steepest Ascent strategy =================================\n";
          Core::IO::cout << "===== (Condensed formulation) ==================================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_steepest_ascent_sp)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Steepest Ascent strategy =================================\n";
          Core::IO::cout << "===== (Saddle point formulation) ===============================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else
          FOUR_C_THROW("Invalid strategy or shape function type for contact/meshtying");
      }

      // condensed formulation
      else if (systype == Inpar::CONTACT::system_condensed ||
               systype == Inpar::CONTACT::system_condensed_lagmult)
      {
        if (soltype == Inpar::CONTACT::solution_lagmult && shapefcn == Inpar::Mortar::shape_dual)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Dual Lagrange multiplier strategy ========================\n";
          Core::IO::cout << "===== (Condensed formulation) ==================================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_lagmult &&
                 shapefcn == Inpar::Mortar::shape_standard &&
                 Core::UTILS::IntegralValue<Inpar::Mortar::LagMultQuad>(smortar, "LM_QUAD") ==
                     Inpar::Mortar::lagmult_const)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== const Lagrange multiplier strategy =======================\n";
          Core::IO::cout << "===== (Condensed formulation) ==================================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_lagmult &&
                 shapefcn == Inpar::Mortar::shape_petrovgalerkin)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Petrov-Galerkin Lagrange multiplier strategy =============\n";
          Core::IO::cout << "===== (Condensed formulation) ==================================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_penalty &&
                 shapefcn == Inpar::Mortar::shape_standard)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Standard Penalty strategy ================================\n";
          Core::IO::cout << "===== (Pure displacement formulation) ==========================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_penalty &&
                 shapefcn == Inpar::Mortar::shape_dual)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Dual Penalty strategy ====================================\n";
          Core::IO::cout << "===== (Pure displacement formulation) ==========================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_multiscale &&
                 shapefcn == Inpar::Mortar::shape_standard)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Multi Scale strategy ================================\n";
          Core::IO::cout << "===== (Pure displacement formulation) ==========================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_multiscale &&
                 shapefcn == Inpar::Mortar::shape_dual)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout
              << "===== Dual Multi Scale strategy ====================================\n";
          Core::IO::cout << "===== (Pure displacement formulation) ==========================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_uzawa &&
                 shapefcn == Inpar::Mortar::shape_standard)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Uzawa Augmented Lagrange strategy ========================\n";
          Core::IO::cout << "===== (Pure displacement formulation) ==========================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else if (soltype == Inpar::CONTACT::solution_uzawa && shapefcn == Inpar::Mortar::shape_dual)
        {
          Core::IO::cout << "================================================================\n";
          Core::IO::cout << "===== Dual Uzawa Augmented Lagrange strategy ===================\n";
          Core::IO::cout << "===== (Pure displacement formulation) ==========================\n";
          Core::IO::cout << "================================================================\n\n";
        }
        else
          FOUR_C_THROW("Invalid strategy or shape function type for contact/meshtying");
      }
    }
    else if (algorithm == Inpar::Mortar::algorithm_nts)
    {
      Core::IO::cout << "================================================================\n";
      Core::IO::cout << "===== Node-To-Segment approach =================================\n";
      Core::IO::cout << "================================================================\n\n";
    }
    else if (algorithm == Inpar::Mortar::algorithm_lts)
    {
      Core::IO::cout << "================================================================\n";
      Core::IO::cout << "===== Line-To-Segment approach =================================\n";
      Core::IO::cout << "================================================================\n\n";
    }
    else if (algorithm == Inpar::Mortar::algorithm_stl)
    {
      Core::IO::cout << "================================================================\n";
      Core::IO::cout << "===== Segment-To-Line approach =================================\n";
      Core::IO::cout << "================================================================\n\n";
    }
    else if (algorithm == Inpar::Mortar::algorithm_gpts)
    {
      Core::IO::cout << "================================================================\n";
      Core::IO::cout << "===== Gauss-Point-To-Segment approach ==========================\n";
      Core::IO::cout << "================================================================\n\n";
    }
    // invalid system type
    else
      FOUR_C_THROW("Invalid system type for contact/meshtying");
  }
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
void CONTACT::STRATEGY::Factory::set_parameters_for_contact_condition(
    const int conditiongroupid, Teuchos::ParameterList& contactinterfaceparameters) const
{
  // add parameters if we have SSI contact
  if (discret().GetCondition("SSIInterfaceContact") != nullptr)
  {
    // get the scatra-scatra interface coupling condition
    std::vector<Core::Conditions::Condition*> s2ikinetics_conditions;
    discret().GetCondition("S2IKinetics", s2ikinetics_conditions);

    // create a sublist which is filled and added to the contact interface parameters
    auto& s2icouplingparameters = contactinterfaceparameters.sublist("ContactS2ICoupling");

    // loop over all s2i conditions and get the one with the same condition id (that they have to
    // match is assured within the setup of the SSI framework) at the slave-side, as only this
    // stores all the information
    for (const auto& s2ikinetics_cond : s2ikinetics_conditions)
    {
      // only add to parameters if condition ID's match
      if (s2ikinetics_cond->parameters().Get<int>("ConditionID") == conditiongroupid)
      {
        // only the slave-side stores the parameters
        if (s2ikinetics_cond->parameters().Get<int>("interface side") == Inpar::S2I::side_slave)
        {
          // fill the parameters from the s2i condition
          ScaTra::MeshtyingStrategyS2I::
              write_s2_i_kinetics_specific_sca_tra_parameters_to_parameter_list(
                  *s2ikinetics_cond, s2icouplingparameters);

          // add the sublist to the contact interface parameter list
          contactinterfaceparameters.setParameters(s2icouplingparameters);
        }
      }
    }
  }
}

FOUR_C_NAMESPACE_CLOSE
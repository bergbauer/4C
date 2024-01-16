/*----------------------------------------------------------------------*/
/*! \file

\brief Implementation of registration of parallel objects

\level 1


*/
/*----------------------------------------------------------------------*/

#include "baci_global_legacy_module.H"

#include "baci_ale_ale2.H"
#include "baci_ale_ale2_nurbs.H"
#include "baci_ale_ale3.H"
#include "baci_ale_ale3_nurbs.H"
#include "baci_art_net_artery.H"
#include "baci_beam3_euler_bernoulli.H"
#include "baci_beam3_kirchhoff.H"
#include "baci_beam3_reissner.H"
#include "baci_beaminteraction_crosslinker_node.H"
#include "baci_beaminteraction_link_beam3_reissner_line2_pinjointed.H"
#include "baci_beaminteraction_link_beam3_reissner_line2_rigidjointed.H"
#include "baci_beaminteraction_link_truss.H"
#include "baci_bele_bele3.H"
#include "baci_bele_vele3.H"
#include "baci_binstrategy_meshfree_multibin.H"
#include "baci_constraint_element2.H"
#include "baci_constraint_element3.H"
#include "baci_contact_element.H"
#include "baci_contact_friction_node.H"
#include "baci_contact_node.H"
#include "baci_elemag_diff_ele.H"
#include "baci_elemag_ele.H"
#include "baci_fluid_ele.H"
#include "baci_fluid_ele_hdg.H"
#include "baci_fluid_ele_hdg_weak_comp.H"
#include "baci_fluid_ele_immersed.H"
#include "baci_fluid_ele_poro.H"
#include "baci_fluid_ele_xwall.H"
#include "baci_fluid_functions.H"
#include "baci_fluid_xfluid_functions.H"
#include "baci_fluid_xfluid_functions_combust.H"
#include "baci_lib_immersed_node.H"
#include "baci_lubrication_ele.H"
#include "baci_mat_aaa_mixedeffects.H"
#include "baci_mat_aaagasser.H"
#include "baci_mat_aaaneohooke.H"
#include "baci_mat_aaaneohooke_stopro.H"
#include "baci_mat_aaaraghavanvorp_damage.H"
#include "baci_mat_activefiber.H"
#include "baci_mat_arrhenius_pv.H"
#include "baci_mat_arrhenius_spec.H"
#include "baci_mat_arrhenius_temp.H"
#include "baci_mat_beam_elasthyper.H"
#include "baci_mat_carreauyasuda.H"
#include "baci_mat_cnst_1d_art.H"
#include "baci_mat_constraintmixture.H"
#include "baci_mat_constraintmixture_history.H"
#include "baci_mat_crystal_plasticity.H"
#include "baci_mat_damage.H"
#include "baci_mat_elasthyper.H"
#include "baci_mat_elchmat.H"
#include "baci_mat_electromagnetic.H"
#include "baci_mat_ferech_pv.H"
#include "baci_mat_fluid_linear_density_viscosity.H"
#include "baci_mat_fluid_murnaghantait.H"
#include "baci_mat_fluid_weakly_compressible.H"
#include "baci_mat_fluidporo.H"
#include "baci_mat_fluidporo_multiphase.H"
#include "baci_mat_fluidporo_multiphase_reactions.H"
#include "baci_mat_fluidporo_multiphase_singlereaction.H"
#include "baci_mat_fluidporo_singlephase.H"
#include "baci_mat_fourieriso.H"
#include "baci_mat_growth.H"
#include "baci_mat_growthremodel_elasthyper.H"
#include "baci_mat_herschelbulkley.H"
#include "baci_mat_ion.H"
#include "baci_mat_lin_elast_1D.H"
#include "baci_mat_list.H"
#include "baci_mat_list_chemoreac.H"
#include "baci_mat_list_chemotaxis.H"
#include "baci_mat_list_reactions.H"
#include "baci_mat_maxwell_0d_acinus.H"
#include "baci_mat_maxwell_0d_acinus_DoubleExponential.H"
#include "baci_mat_maxwell_0d_acinus_Exponential.H"
#include "baci_mat_maxwell_0d_acinus_NeoHookean.H"
#include "baci_mat_maxwell_0d_acinus_Ogden.H"
#include "baci_mat_membrane_active_strain.H"
#include "baci_mat_membrane_elasthyper.H"
#include "baci_mat_micromaterial.H"
#include "baci_mat_mixfrac.H"
#include "baci_mat_mixture.H"
#include "baci_mat_modpowerlaw.H"
#include "baci_mat_myocard.H"
#include "baci_mat_newtonianfluid.H"
#include "baci_mat_plastic_VarConstUpdate.H"
#include "baci_mat_plasticelasthyper.H"
#include "baci_mat_plasticlinelast.H"
#include "baci_mat_robinson.H"
#include "baci_mat_scalardepinterp.H"
#include "baci_mat_scatra_mat.H"
#include "baci_mat_scatra_mat_multiporo.H"
#include "baci_mat_scatra_mat_poro_ecm.H"
#include "baci_mat_spring.H"
#include "baci_mat_structporo.H"
#include "baci_mat_structporo_reaction.H"
#include "baci_mat_structporo_reaction_ecm.H"
#include "baci_mat_stvenantkirchhoff.H"
#include "baci_mat_sutherland.H"
#include "baci_mat_tempdepwater.H"
#include "baci_mat_thermoplasticlinelast.H"
#include "baci_mat_thermostvenantkirchhoff.H"
#include "baci_mat_viscoanisotropic.H"
#include "baci_mat_viscoelasthyper.H"
#include "baci_mat_visconeohooke.H"
#include "baci_mat_yoghurt.H"
#include "baci_membrane_eletypes.H"
#include "baci_membrane_scatra_eletypes.H"
#include "baci_module_registry_callbacks.H"
#include "baci_mortar_element.H"
#include "baci_mortar_node.H"
#include "baci_nurbs_discret_control_point.H"
#include "baci_particle_engine_object.H"
#include "baci_porofluidmultiphase_ele.H"
#include "baci_poromultiphase_scatra_function.H"
#include "baci_red_airways_elementbase.H"
#include "baci_rigidsphere.H"
#include "baci_scatra_ele.H"
#include "baci_shell7p_ele.H"
#include "baci_shell7p_ele_scatra.H"
#include "baci_so3_hex18.H"
#include "baci_so3_hex20.H"
#include "baci_so3_hex27.H"
#include "baci_so3_hex8.H"
#include "baci_so3_hex8fbar.H"
#include "baci_so3_hex8p1j1.H"
#include "baci_so3_nstet5.H"
#include "baci_so3_nurbs27.H"
#include "baci_so3_plast_ssn_eletypes.H"
#include "baci_so3_plast_ssn_sosh18.H"
#include "baci_so3_plast_ssn_sosh8.H"
#include "baci_so3_poro_eletypes.H"
#include "baci_so3_poro_p1_eletypes.H"
#include "baci_so3_poro_p1_scatra_eletypes.H"
#include "baci_so3_poro_scatra_eletypes.H"
#include "baci_so3_pyramid5.H"
#include "baci_so3_pyramid5fbar.H"
#include "baci_so3_scatra_eletypes.H"
#include "baci_so3_sh18.H"
#include "baci_so3_sh8.H"
#include "baci_so3_sh8p8.H"
#include "baci_so3_shw6.H"
#include "baci_so3_tet10.H"
#include "baci_so3_tet4.H"
#include "baci_so3_tet4av.H"
#include "baci_so3_thermo_eletypes.H"
#include "baci_so3_weg6.H"
#include "baci_solid_ele.H"
#include "baci_solid_poro_ele.H"
#include "baci_structure_new_functions.H"
#include "baci_thermo_element.H"
#include "baci_torsion3.H"
#include "baci_truss3.H"
#include "baci_truss3_scatra.H"
#include "baci_utils_function_library.H"
#include "baci_w1.H"
#include "baci_w1_nurbs.H"
#include "baci_w1_poro_eletypes.H"
#include "baci_w1_poro_p1_eletypes.H"
#include "baci_w1_poro_p1_scatra_eletypes.H"
#include "baci_w1_poro_scatra_eletypes.H"
#include "baci_w1_scatra.H"

#include <iostream>
#include <string>

BACI_NAMESPACE_OPEN

namespace
{
  void RegisterParObjectTypes()
  {
    // Perform a dummy operation for the side-effect of forcing registration.
    std::stringstream s;

    s << DRT::ContainerType::Instance().Name() << " " << DRT::ConditionObjectType::Instance().Name()
      << " " << DRT::NodeType::Instance().Name() << " "
      << DRT::NURBS::ControlPointType::Instance().Name() << " "
      << DRT::ImmersedNodeType::Instance().Name() << " "
      << CROSSLINKING::CrosslinkerNodeType::Instance().Name() << " "
      << DRT::MESHFREE::MeshfreeMultiBinType::Instance().Name() << " "
      << DRT::ELEMENTS::Beam3rType::Instance().Name() << " "
      << DRT::ELEMENTS::Beam3ebType::Instance().Name() << " "
      << DRT::ELEMENTS::Beam3kType::Instance().Name() << " "
      << DRT::ELEMENTS::RigidsphereType::Instance().Name() << " "
      << DRT::ELEMENTS::Truss3Type::Instance().Name() << " "
      << DRT::ELEMENTS::Truss3ScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::Torsion3Type::Instance().Name() << " "
      << DRT::ELEMENTS::Shell7pType::Instance().Name() << " "
      << DRT::ELEMENTS::Shell7pScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::Membrane_tri3Type::Instance().Name() << " "
      << DRT::ELEMENTS::Membrane_tri6Type::Instance().Name() << " "
      << DRT::ELEMENTS::Membrane_quad4Type::Instance().Name() << " "
      << DRT::ELEMENTS::Membrane_quad9Type::Instance().Name() << " "
      << DRT::ELEMENTS::MembraneScatra_tri3Type::Instance().Name() << " "
      << DRT::ELEMENTS::MembraneScatra_tri6Type::Instance().Name() << " "
      << DRT::ELEMENTS::MembraneScatra_quad4Type::Instance().Name() << " "
      << DRT::ELEMENTS::MembraneScatra_quad9Type::Instance().Name() << " "
      << DRT::ELEMENTS::Wall1Type::Instance().Name() << " "
      << DRT::ELEMENTS::WallTri3PoroType::Instance().Name() << " "
      << DRT::ELEMENTS::WallTri3PoroP1Type::Instance().Name() << " "
      << DRT::ELEMENTS::WallQuad4PoroType::Instance().Name() << " "
      << DRT::ELEMENTS::WallQuad4PoroP1Type::Instance().Name() << " "
      << DRT::ELEMENTS::WallQuad9PoroType::Instance().Name() << " "
      << DRT::ELEMENTS::WallQuad9PoroP1Type::Instance().Name() << " "
      << DRT::ELEMENTS::WallNurbs4PoroType::Instance().Name() << " "
      << DRT::ELEMENTS::WallNurbs9PoroType::Instance().Name() << " "
      << DRT::ELEMENTS::NURBS::Wall1NurbsType::Instance().Name() << " "
      << DRT::ELEMENTS::Wall1ScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::WallQuad4PoroScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::WallQuad4PoroP1ScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::FluidType::Instance().Name() << " "
      << DRT::ELEMENTS::FluidXWallType::Instance().Name() << " "
      << DRT::ELEMENTS::FluidXWallBoundaryType::Instance().Name() << " "
      << DRT::ELEMENTS::FluidTypeImmersed::Instance().Name() << " "
      << DRT::ELEMENTS::FluidPoroEleType::Instance().Name() << " "
      << DRT::ELEMENTS::FluidHDGType::Instance().Name() << " "
      << DRT::ELEMENTS::FluidHDGWeakCompType::Instance().Name() << " "
      << DRT::ELEMENTS::FluidBoundaryType::Instance().Name() << " "
      << DRT::ELEMENTS::FluidPoroBoundaryType::Instance().Name() << " "
      << DRT::ELEMENTS::Ale3Type::Instance().Name() << " "
      << DRT::ELEMENTS::NURBS::Ale3_NurbsType::Instance().Name() << " "
      << DRT::ELEMENTS::Ale2Type::Instance().Name() << " "
      << DRT::ELEMENTS::NURBS::Ale2_NurbsType::Instance().Name() << " "
      << DRT::ELEMENTS::Bele3Type::Instance().Name() << " "
      << DRT::ELEMENTS::Vele3Type::Instance().Name() << " "
      << DRT::ELEMENTS::NStet5Type::Instance().Name() << " "
      << DRT::ELEMENTS::NURBS::So_nurbs27Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_nurbs27PoroType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex18Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_sh18Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_sh18PlastType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex8Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_Hex8P1J1Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex8fbarType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex8fbarScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex8fbarThermoType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex8PoroType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex8PoroP1Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex8ScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex8ThermoType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex8PlastType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex8Type::Instance().Name() << " "
      << DRT::ELEMENTS::SolidType::Instance().Name() << " "
      << DRT::ELEMENTS::SolidPoroType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex20Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex27Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex27ScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex27PoroType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex27ThermoType::Instance().Name() << " "
      << DRT::ELEMENTS::So_nurbs27ThermoType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex20ThermoType::Instance().Name() << " "
      << DRT::ELEMENTS::So_hex27PlastType::Instance().Name() << " "
      << DRT::ELEMENTS::So_sh8Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_sh8PlastType::Instance().Name() << " "
      << DRT::ELEMENTS::So_sh8p8Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_shw6Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet10Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet10PoroType::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet10ScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet4PlastType::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet4Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet4PoroType::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet4PoroP1Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet4ScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet4PoroScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet4PoroP1ScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet4ThermoType::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet4avType::Instance().Name() << " "
      << DRT::ELEMENTS::So_tet10ThermoType::Instance().Name() << " "
      << DRT::ELEMENTS::So_weg6Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_pyramid5Type::Instance().Name() << " "
      << DRT::ELEMENTS::So_pyramid5fbarType::Instance().Name() << " "
      << DRT::ELEMENTS::ArteryType::Instance().Name() << " "
      << DRT::ELEMENTS::RedAirwayType::Instance().Name() << " "
      << DRT::ELEMENTS::RedAcinusType::Instance().Name() << " "
      << DRT::ELEMENTS::RedInterAcinarDepType::Instance().Name() << " "
      << DRT::ELEMENTS::RedAirBloodScatraType::Instance().Name() << " "
      << DRT::ELEMENTS::RedAirBloodScatraLine3Type::Instance().Name() << " "
      << DRT::ELEMENTS::ConstraintElement2Type::Instance().Name() << " "
      << DRT::ELEMENTS::ConstraintElement3Type::Instance().Name() << " "
      << DRT::ELEMENTS::LubricationType::Instance().Name() << " "
      << DRT::ELEMENTS::PoroFluidMultiPhaseType::Instance().Name() << " "
      << DRT::ELEMENTS::TransportType::Instance().Name() << " "
      << DRT::ELEMENTS::ThermoType::Instance().Name() << " "
      << DRT::ELEMENTS::ElemagType::Instance().Name() << " "
      << DRT::ELEMENTS::ElemagDiffType::Instance().Name() << " "
      << DRT::ELEMENTS::ElemagBoundaryType::Instance().Name() << " "
      << DRT::ELEMENTS::ElemagDiffBoundaryType::Instance().Name() << " "
      << DRT::ELEMENTS::ElemagIntFaceType::Instance().Name() << " "
      << DRT::ELEMENTS::ElemagDiffIntFaceType::Instance().Name() << " "
      << MAT::Cnst_1d_artType::Instance().Name() << " " << MAT::AAAgasserType::Instance().Name()
      << " " << MAT::AAAneohookeType::Instance().Name() << " "
      << MAT::AAAneohooke_stoproType::Instance().Name() << " "
      << MAT::AAAraghavanvorp_damageType::Instance().Name() << " "
      << MAT::AAA_mixedeffectsType::Instance().Name() << " "
      << MAT::ArrheniusPVType::Instance().Name() << " " << MAT::ArrheniusSpecType::Instance().Name()
      << " " << MAT::ArrheniusTempType::Instance().Name() << " "
      << MAT::CarreauYasudaType::Instance().Name() << " "
      << MAT::ConstraintMixtureType::Instance().Name() << " "
      << MAT::ConstraintMixtureHistoryType::Instance().Name() << " "
      << MAT::CrystalPlasticityType::Instance().Name() << " "
      << MAT::ElastHyperType::Instance().Name() << " "
      << MAT::PlasticElastHyperType::Instance().Name() << " "
      << MAT::PlasticElastHyperVCUType::Instance().Name() << " "
      << MAT::ViscoElastHyperType::Instance().Name() << " " << MAT::FerEchPVType::Instance().Name()
      << " " << MAT::FluidPoroType::Instance().Name() << " "
      << MAT::FluidPoroSinglePhaseType::Instance().Name() << " "
      << MAT::FluidPoroSingleVolFracType::Instance().Name() << " "
      << MAT::FluidPoroVolFracPressureType::Instance().Name() << " "
      << MAT::FluidPoroSingleReactionType::Instance().Name() << " "
      << MAT::FluidPoroMultiPhaseType::Instance().Name() << " "
      << MAT::FluidPoroMultiPhaseReactionsType::Instance().Name() << " "
      << MAT::FourierIsoType::Instance().Name() << " "
      << MAT::GrowthVolumetricType::Instance().Name() << " "
      << MAT::Membrane_ElastHyperType::Instance().Name() << " "
      << MAT::Membrane_ActiveStrainType::Instance().Name() << " "
      << MAT::GrowthRemodel_ElastHyperType::Instance().Name() << " "
      << MAT::MixtureType::Instance().Name() << " " << MAT::HerschelBulkleyType::Instance().Name()
      << " " << MAT::IonType::Instance().Name() << " "
      << MAT::LinearDensityViscosityType::Instance().Name() << " "
      << MAT::WeaklyCompressibleFluidType::Instance().Name() << " "
      << MAT::MatListType::Instance().Name() << " " << MAT::MatListReactionsType::Instance().Name()
      << " " << MAT::MatListChemotaxisType::Instance().Name() << " "
      << MAT::MatListChemoReacType::Instance().Name() << " " << MAT::ElchMatType::Instance().Name()
      << " " << MAT::MicroMaterialType::Instance().Name() << " "
      << MAT::MixFracType::Instance().Name() << " " << MAT::ModPowerLawType::Instance().Name()
      << " " << MAT::MurnaghanTaitFluidType::Instance().Name() << " "
      << MAT::MyocardType::Instance().Name() << MAT::NewtonianFluidType::Instance().Name() << " "
      << MAT::StructPoroType::Instance().Name() << " "
      << MAT::StructPoroReactionType::Instance().Name() << " "
      << MAT::StructPoroReactionECMType::Instance().Name() << " "
      << MAT::ScalarDepInterpType::Instance().Name() << " " << MAT::ScatraMatType::Instance().Name()
      << " " << MAT::ScatraMatPoroECMType::Instance().Name() << " "
      << MAT::ScatraMatMultiPoroFluidType::Instance().Name() << " "
      << MAT::ScatraMatMultiPoroVolFracType::Instance().Name() << " "
      << MAT::ScatraMatMultiPoroSolidType::Instance().Name() << " "
      << MAT::ScatraMatMultiPoroTemperatureType::Instance().Name() << " "
      << MAT::StVenantKirchhoffType::Instance().Name() << " "
      << MAT::LinElast1DType::Instance().Name() << " "
      << MAT::LinElast1DGrowthType::Instance().Name() << " "
      << MAT::SutherlandType::Instance().Name() << " " << MAT::TempDepWaterType::Instance().Name()
      << " " << MAT::ThermoStVenantKirchhoffType::Instance().Name() << " "
      << MAT::ThermoPlasticLinElastType::Instance().Name() << " "
      << MAT::ViscoAnisotropicType::Instance().Name() << " "
      << MAT::ViscoNeoHookeType::Instance().Name() << " " << MAT::YoghurtType::Instance().Name()
      << " " << MAT::SpringType::Instance().Name() << " "
      << MAT::BeamElastHyperMaterialType<double>::Instance().Name() << " "
      << MAT::BeamElastHyperMaterialType<Sacado::Fad::DFad<double>>::Instance().Name() << " "
      << MAT::PlasticLinElastType::Instance().Name() << " " << MAT::RobinsonType::Instance().Name()
      << " " << MAT::DamageType::Instance().Name() << " "
      << MAT::ElectromagneticMatType::Instance().Name() << " "
      << MAT::Maxwell_0d_acinusType::Instance().Name() << " "
      << MAT::Maxwell_0d_acinusNeoHookeanType::Instance().Name() << " "
      << MAT::Maxwell_0d_acinusExponentialType::Instance().Name() << " "
      << MAT::Maxwell_0d_acinusDoubleExponentialType::Instance().Name() << " "
      << MAT::Maxwell_0d_acinusOgdenType::Instance().Name() << " "
      << MORTAR::NodeType::Instance().Name() << " " << MORTAR::ElementType::Instance().Name() << " "
      << CONTACT::NodeType::Instance().Name() << " " << CONTACT::FriNodeType::Instance().Name()
      << " " << CONTACT::ElementType::Instance().Name() << " "
      << MAT::ActiveFiberType::Instance().Name()
      << BEAMINTERACTION::BeamLinkBeam3rLine2RigidJointedType::Instance().Name() << " "
      << BEAMINTERACTION::BeamLinkBeam3rLine2PinJointedType::Instance().Name() << " "
      << BEAMINTERACTION::BeamLinkTrussType::Instance().Name() << " "
      << PARTICLEENGINE::ParticleObjectType::Instance().Name() << " ";
  }

  void AttachFunctionDefinitions(CORE::UTILS::FunctionManager& function_manager)
  {
    AddValidBuiltinFunctions(function_manager);
    STR::AddValidStructureFunctions(function_manager);
    FLD::AddValidFluidFunctions(function_manager);
    DRT::UTILS::AddValidCombustFunctions(function_manager);
    DRT::UTILS::AddValidXfluidFunctions(function_manager);
    AddValidLibraryFunctions(function_manager);
    POROMULTIPHASESCATRA::AddValidPoroFunctions(function_manager);
  }

}  // namespace

ModuleCallbacks GlobalLegacyModuleCallbacks()
{
  ModuleCallbacks callbacks;
  callbacks.RegisterParObjectTypes = RegisterParObjectTypes;
  callbacks.AttachFunctionDefinitions = AttachFunctionDefinitions;
  return callbacks;
}

BACI_NAMESPACE_CLOSE

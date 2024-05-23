/*-----------------------------------------------------------*/
/*! \file

\brief Basic time integration driver for reduced models


\level 2

*/
/*-----------------------------------------------------------*/

#include "4C_fluid_timint_red.hpp"

#include "4C_adapter_art_net.hpp"
#include "4C_fluid_coupling_red_models.hpp"
#include "4C_fluid_meshtying.hpp"
#include "4C_fluid_volumetric_surfaceFlow_condition.hpp"
#include "4C_global_data.hpp"
#include "4C_io.hpp"
#include "4C_lib_locsys.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*
 |  Constructor (public)                                       bk 11/13 |
 *----------------------------------------------------------------------*/
FLD::TimIntRedModels::TimIntRedModels(const Teuchos::RCP<DRT::Discretization>& actdis,
    const Teuchos::RCP<CORE::LINALG::Solver>& solver,
    const Teuchos::RCP<Teuchos::ParameterList>& params,
    const Teuchos::RCP<IO::DiscretizationWriter>& output, bool alefluid /*= false*/)
    : FluidImplicitTimeInt(actdis, solver, params, output, alefluid),
      traction_vel_comp_adder_bc_(Teuchos::null),
      coupled3D_redDbc_art_(Teuchos::null),
      ART_timeInt_(Teuchos::null),
      coupled3D_redDbc_airways_(Teuchos::null),
      airway_imp_timeInt_(Teuchos::null),
      vol_surf_flow_bc_(Teuchos::null),
      vol_surf_flow_bc_maps_(Teuchos::null),
      vol_flow_rates_bc_extractor_(Teuchos::null),
      strong_redD_3d_coupling_(false)
{
}


/*----------------------------------------------------------------------*
 |  initialize algorithm                                rasthofer 04/14 |
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::Init()
{
  // Vectors associated to boundary conditions
  // -----------------------------------------

  // create the volumetric-surface-flow condition
  if (alefluid_)
  {
    discret_->SetState("dispnp", dispn_);
  }

  vol_surf_flow_bc_ = Teuchos::rcp(new UTILS::FluidVolumetricSurfaceFlowWrapper(discret_, dta_));

  // evaluate the map of the womersley bcs
  vol_flow_rates_bc_extractor_ = Teuchos::rcp(new FLD::UTILS::VolumetricFlowMapExtractor());
  vol_flow_rates_bc_extractor_->Setup(*discret_);
  vol_surf_flow_bc_maps_ =
      Teuchos::rcp(new Epetra_Map(*(vol_flow_rates_bc_extractor_->VolumetricSurfaceFlowCondMap())));

  // -------------------------------------------------------------------
  // Initialize the reduced models
  // -------------------------------------------------------------------

  strong_redD_3d_coupling_ = false;
  if (params_->get<std::string>("Strong 3D_redD coupling", "no") == "yes")
    strong_redD_3d_coupling_ = true;

  {
    ART_timeInt_ = dyn_art_net_drt(true);
    // Check if one-dimensional artery network problem exist
    if (ART_timeInt_ != Teuchos::null)
    {
      IO::DiscretizationWriter output_redD(ART_timeInt_->Discretization(),
          GLOBAL::Problem::Instance()->OutputControlFile(),
          GLOBAL::Problem::Instance()->spatial_approximation_type());
      discret_->ClearState();
      discret_->SetState("velaf", zeros_);
      if (alefluid_)
      {
        discret_->SetState("dispnp", dispnp_);
      }
      coupled3D_redDbc_art_ =
          Teuchos::rcp(new UTILS::FluidCouplingWrapper<ADAPTER::ArtNet>(discret_,
              ART_timeInt_->Discretization(), ART_timeInt_, output_redD, dta_, ART_timeInt_->Dt()));
    }


    airway_imp_timeInt_ = dyn_red_airways_drt(true);
    // Check if one-dimensional artery network problem exist
    if (airway_imp_timeInt_ != Teuchos::null)
    {
      IO::DiscretizationWriter output_redD(airway_imp_timeInt_->Discretization(),
          GLOBAL::Problem::Instance()->OutputControlFile(),
          GLOBAL::Problem::Instance()->spatial_approximation_type());
      discret_->ClearState();
      discret_->SetState("velaf", zeros_);
      if (alefluid_)
      {
        discret_->SetState("dispnp", dispnp_);
      }
      coupled3D_redDbc_airways_ =
          Teuchos::rcp(new UTILS::FluidCouplingWrapper<AIRWAY::RedAirwayImplicitTimeInt>(discret_,
              airway_imp_timeInt_->Discretization(), airway_imp_timeInt_, output_redD, dta_,
              airway_imp_timeInt_->Dt()));
    }


    zeros_->PutScalar(0.0);  // just in case of change
  }

  traction_vel_comp_adder_bc_ = Teuchos::rcp(new UTILS::TotalTractionCorrector(discret_, dta_));


  // ------------------------------------------------------------------------------
  // Check, if features are used with the locsys manager that are not supported,
  // or better, not implemented yet.
  // ------------------------------------------------------------------------------
  if (locsysman_ != Teuchos::null)
  {
    // Models
    if ((ART_timeInt_ != Teuchos::null) or (airway_imp_timeInt_ != Teuchos::null))
    {
      FOUR_C_THROW(
          "No problem types involving airways are supported for use with locsys conditions!");
    }
  }
}



/*----------------------------------------------------------------------*
 | evaluate special boundary conditions                        bk 12/13 |
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::do_problem_specific_boundary_conditions()
{
  if (alefluid_)
  {
    discret_->SetState("dispnp", dispnp_);
  }

  // Check if one-dimensional artery network problem exist
  if (ART_timeInt_ != Teuchos::null)
  {
    coupled3D_redDbc_art_->EvaluateDirichlet(velnp_, *(dbcmaps_->CondMap()), time_);
  }
  // update the 3D-to-reduced_D coupling data
  // Check if one-dimensional artery network problem exist
  if (airway_imp_timeInt_ != Teuchos::null)
  {
    coupled3D_redDbc_airways_->EvaluateDirichlet(velnp_, *(dbcmaps_->CondMap()), time_);
  }

  // Evaluate the womersley velocities
  vol_surf_flow_bc_->EvaluateVelocities(velnp_, time_);
}

/*----------------------------------------------------------------------*
| Update3DToReduced in AssembleMatAndRHS                       bk 11/13 |
*----------------------------------------------------------------------*/
void FLD::TimIntRedModels::update3_d_to_reduced_mat_and_rhs()
{
  discret_->ClearState();

  discret_->SetState("velaf", velnp_);
  discret_->SetState("hist", hist_);

  if (alefluid_)
  {
    discret_->SetState("dispnp", dispnp_);
  }

  // Check if one-dimensional artery network problem exist
  if (ART_timeInt_ != Teuchos::null)
  {
    if (strong_redD_3d_coupling_)
    {
      coupled3D_redDbc_art_->LoadState();
      coupled3D_redDbc_art_->FlowRateCalculation(time_, dta_);
      coupled3D_redDbc_art_->apply_boundary_conditions(time_, dta_, theta_);
    }
    coupled3D_redDbc_art_->UpdateResidual(residual_);
  }
  // update the 3D-to-reduced_D coupling data
  // Check if one-dimensional artery network problem exist
  if (airway_imp_timeInt_ != Teuchos::null)
  {
    if (strong_redD_3d_coupling_)
    {
      coupled3D_redDbc_airways_->LoadState();
      coupled3D_redDbc_airways_->FlowRateCalculation(time_, dta_);
      coupled3D_redDbc_airways_->apply_boundary_conditions(time_, dta_, theta_);
    }
    coupled3D_redDbc_airways_->UpdateResidual(residual_);
  }

  //----------------------------------------------------------------------
  // add the traction velocity component
  //----------------------------------------------------------------------

  traction_vel_comp_adder_bc_->EvaluateVelocities(velnp_, time_, theta_, dta_);
  traction_vel_comp_adder_bc_->UpdateResidual(residual_);

  discret_->ClearState();
}

/*----------------------------------------------------------------------*
| call update3_d_to_reduced_mat_and_rhs                              bk 11/13 |
*----------------------------------------------------------------------*/
void FLD::TimIntRedModels::set_custom_ele_params_assemble_mat_and_rhs(
    Teuchos::ParameterList& eleparams)
{
  // these are the only routines that have to be called in AssembleMatAndRHS
  // before Evaluate in the RedModels case
  update3_d_to_reduced_mat_and_rhs();
}

/*----------------------------------------------------------------------*
 | output of solution vector of ReducedD problem to binio   ismail 01/13|
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::OutputReducedD()
{
  // output of solution
  if (step_ % upres_ == 0)
  {
    // write reduced model problem
    // Check if one-dimensional artery network problem exist
    if (ART_timeInt_ != Teuchos::null)
    {
      Teuchos::RCP<Teuchos::ParameterList> redD_export_params;
      redD_export_params = Teuchos::rcp(new Teuchos::ParameterList());

      redD_export_params->set<int>("step", step_);
      redD_export_params->set<int>("upres", upres_);
      redD_export_params->set<int>("uprestart", uprestart_);
      redD_export_params->set<double>("time", time_);

      ART_timeInt_->Output(true, redD_export_params);
    }

    // Check if one-dimensional artery network problem exist
    if (airway_imp_timeInt_ != Teuchos::null)
    {
      Teuchos::RCP<Teuchos::ParameterList> redD_export_params;
      redD_export_params = Teuchos::rcp(new Teuchos::ParameterList());

      redD_export_params->set<int>("step", step_);
      redD_export_params->set<int>("upres", upres_);
      redD_export_params->set<int>("uprestart", uprestart_);
      redD_export_params->set<double>("time", time_);

      airway_imp_timeInt_->Output(true, redD_export_params);
    }
  }
}  // FLD::TimIntRedModels::OutputReducedD

/*----------------------------------------------------------------------*
 | read some additional data in restart                         bk 12/13|
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::ReadRestart(int step)
{
  IO::DiscretizationReader reader(discret_, GLOBAL::Problem::Instance()->InputControlFile(), step);

  vol_surf_flow_bc_->ReadRestart(reader);

  traction_vel_comp_adder_bc_->ReadRestart(reader);

  // Read restart of one-dimensional arterial network
  if (ART_timeInt_ != Teuchos::null)
  {
    coupled3D_redDbc_art_->ReadRestart(reader);
  }
  // Check if zero-dimensional airway network problem exist
  if (airway_imp_timeInt_ != Teuchos::null)
  {
    coupled3D_redDbc_airways_->ReadRestart(reader);
  }

  ReadRestartReducedD(step);
}

/*----------------------------------------------------------------------*
 |                                                          ismail 01/13|
 -----------------------------------------------------------------------*/
void FLD::TimIntRedModels::ReadRestartReducedD(int step)
{
  // Check if one-dimensional artery network problem exist
  if (ART_timeInt_ != Teuchos::null)
  {
    ART_timeInt_->ReadRestart(step, true);
  }

  // Check if one-dimensional artery network problem exist
  if (airway_imp_timeInt_ != Teuchos::null)
  {
    airway_imp_timeInt_->ReadRestart(step, true);
  }
}  // FLD::TimIntRedModels::ReadRestartReadRestart(int step)

/*----------------------------------------------------------------------*
 | do some additional steps in SetupMeshtying                   bk 12/13|
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::SetupMeshtying()
{
  FluidImplicitTimeInt::SetupMeshtying();
  // Volume surface flow conditions are treated in the same way as Dirichlet condition.
  // Therefore, a volume surface flow condition cannot be defined on the same nodes as the
  // slave side of an internal interface
  // Solution:  Exclude those nodes of your surface
  // but:       The resulting inflow rate (based on the area)
  //            as well as the profile will be different
  //            since it is based on a different surface discretization!!

  if (vol_surf_flow_bc_maps_->NumGlobalElements() != 0)
  {
    meshtying_->CheckOverlappingBC(vol_surf_flow_bc_maps_);
    meshtying_->DirichletOnMaster(vol_surf_flow_bc_maps_);
  }
}

/*----------------------------------------------------------------------*
 | output of solution vector to binio                        gammi 04/07|
 | overloading function                                         bk 12/13|
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::Output()
{
  FluidImplicitTimeInt::Output();
  // output of solution
  if (step_ % upres_ == 0)
  {
    vol_surf_flow_bc_->Output(*output_);
    traction_vel_comp_adder_bc_->Output(*output_);

    if (uprestart_ != 0 && step_ % uprestart_ == 0)  // add restart data
    {
      // Check if one-dimensional artery network problem exist
      if (ART_timeInt_ != Teuchos::null)
      {
        coupled3D_redDbc_art_->WriteRestart(*output_);
      }
      // Check if zero-dimensional airway network problem exist
      if (airway_imp_timeInt_ != Teuchos::null)
      {
        coupled3D_redDbc_airways_->WriteRestart(*output_);
      }
    }
  }
  // write restart also when uprestart_ is not a integer multiple of upres_
  else if (uprestart_ > 0 && step_ % uprestart_ == 0)
  {
    // write reduced model problem
    // Check if one-dimensional artery network problem exist
    if (ART_timeInt_ != Teuchos::null)
    {
      coupled3D_redDbc_art_->WriteRestart(*output_);
    }
    // Check if zero-dimensional airway network problem exist
    if (airway_imp_timeInt_ != Teuchos::null)
    {
      coupled3D_redDbc_airways_->WriteRestart(*output_);
    }
  }

  OutputReducedD();
}  // TimIntRedModels::Output

/*----------------------------------------------------------------------*
 | read some additional data in restart                         bk 12/13|
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::insert_volumetric_surface_flow_cond_vector(
    Teuchos::RCP<Epetra_Vector> vel, Teuchos::RCP<Epetra_Vector> res)
{
  // -------------------------------------------------------------------
  // take surface volumetric flow rate into account
  //    Teuchos::RCP<Epetra_Vector> temp_vec = Teuchos::rcp(new
  //    Epetra_Vector(*vol_surf_flow_bc_maps_,true)); vol_surf_flow_bc_->InsertCondVector( *temp_vec
  //    , *residual_);
  // -------------------------------------------------------------------
  vol_flow_rates_bc_extractor_->InsertVolumetricSurfaceFlowCondVector(
      vol_flow_rates_bc_extractor_->ExtractVolumetricSurfaceFlowCondVector(vel), res);
}

/*----------------------------------------------------------------------*
 | prepare AVM3-based scale separation                         vg 10/08 |
 | overloaded in TimIntRedModelsModels and TimIntLoma        bk 12/13 |
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::AVM3Preparation()
{
  // time measurement: avm3
  TEUCHOS_FUNC_TIME_MONITOR("           + avm3");

  // create the parameters for the discretization
  Teuchos::ParameterList eleparams;

  // necessary here, because some application time integrations add something to the residual
  // before the Neumann loads are added
  residual_->PutScalar(0.0);

  // Maybe this needs to be inserted in case of impedanceBC + AVM3
  //  if (nonlinearbc_ && isimpedancebc_)
  //  {
  //    // add impedance Neumann loads
  //    impedancebc_->UpdateResidual(residual_);
  //  }

  av_m3_assemble_mat_and_rhs(eleparams);

  // apply Womersley as a Dirichlet BC
  CORE::LINALG::apply_dirichlet_to_system(
      *sysmat_, *incvel_, *residual_, *zeros_, *(vol_surf_flow_bc_maps_));

  // get scale-separation matrix
  av_m3_get_scale_separation_matrix();
}  // TimIntRedModels::AVM3Preparation

/*----------------------------------------------------------------------*
 | RedModels - specific BC in linear_relaxation_solve            bk 12/13|
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::CustomSolve(Teuchos::RCP<Epetra_Vector> relax)
{
  // apply Womersley as a Dirichlet BC
  CORE::LINALG::apply_dirichlet_to_system(*incvel_, *residual_, *relax, *(vol_surf_flow_bc_maps_));

  // apply Womersley as a Dirichlet BC
  sysmat_->ApplyDirichlet(*(vol_surf_flow_bc_maps_));
}

/*----------------------------------------------------------------------*
 | RedModels - prepare time step                            ismail 06/14|
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::PrepareTimeStep()
{
  FluidImplicitTimeInt::PrepareTimeStep();

  discret_->ClearState();
  discret_->SetState("velaf", velnp_);
  discret_->SetState("hist", hist_);

  if (alefluid_) discret_->SetState("dispnp", dispnp_);

  // Check if one-dimensional artery network problem exist
  if (ART_timeInt_ != Teuchos::null)
  {
    coupled3D_redDbc_art_->SaveState();
    coupled3D_redDbc_art_->FlowRateCalculation(time_, dta_);
    coupled3D_redDbc_art_->apply_boundary_conditions(time_, dta_, theta_);
  }


  // Check if one-dimensional artery network problem exist
  if (airway_imp_timeInt_ != Teuchos::null)
  {
    coupled3D_redDbc_airways_->SaveState();
    coupled3D_redDbc_airways_->FlowRateCalculation(time_, dta_);
    coupled3D_redDbc_airways_->apply_boundary_conditions(time_, dta_, theta_);
  }

  discret_->ClearState();
}


/*----------------------------------------------------------------------*
 | Apply Womersley bc to shapederivatives                       bk 12/13|
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::AssembleMatAndRHS()
{
  FluidImplicitTimeInt::AssembleMatAndRHS();

  if (shapederivatives_ != Teuchos::null)
  {
    // apply the womersley bc as a dirichlet bc
    shapederivatives_->ApplyDirichlet(*(vol_surf_flow_bc_maps_), false);
  }
}

/*----------------------------------------------------------------------*
 | Apply Womersley bc to system                                 bk 12/13|
 *----------------------------------------------------------------------*/
void FLD::TimIntRedModels::apply_dirichlet_to_system()
{
  FluidImplicitTimeInt::apply_dirichlet_to_system();

  if (LocsysManager() != Teuchos::null)
  {
    // apply Womersley as a Dirichlet BC
    CORE::LINALG::apply_dirichlet_to_system(
        *CORE::LINALG::CastToSparseMatrixAndCheckSuccess(sysmat_), *incvel_, *residual_,
        *locsysman_->Trafo(), *zeros_, *(vol_surf_flow_bc_maps_));
  }
  else
  {
    // apply Womersley as a Dirichlet BC
    CORE::LINALG::apply_dirichlet_to_system(
        *sysmat_, *incvel_, *residual_, *zeros_, *(vol_surf_flow_bc_maps_));
  }
}

FOUR_C_NAMESPACE_CLOSE

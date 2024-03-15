/*----------------------------------------------------------------------*/
/*! \file

\brief Base meshtying element for meshtying between a 3D beam and a 3D solid element.

\level 3
*/


#include "baci_beaminteraction_beam_to_solid_volume_meshtying_pair_base.hpp"

#include "baci_beaminteraction_beam_to_beam_contact_defines.hpp"
#include "baci_beaminteraction_beam_to_beam_contact_utils.hpp"
#include "baci_beaminteraction_beam_to_solid_visualization_output_writer_base.hpp"
#include "baci_beaminteraction_beam_to_solid_visualization_output_writer_visualization.hpp"
#include "baci_beaminteraction_beam_to_solid_volume_meshtying_params.hpp"
#include "baci_beaminteraction_beam_to_solid_volume_meshtying_visualization_output_params.hpp"
#include "baci_beaminteraction_contact_pair.hpp"
#include "baci_beaminteraction_contact_params.hpp"
#include "baci_beaminteraction_geometry_pair_access_traits.hpp"
#include "baci_geometry_pair_element_evaluation_functions.hpp"
#include "baci_geometry_pair_factory.hpp"
#include "baci_geometry_pair_line_to_volume.hpp"
#include "baci_linalg_serialdensematrix.hpp"
#include "baci_linalg_serialdensevector.hpp"
#include "baci_linalg_utils_densematrix_inverse.hpp"

BACI_NAMESPACE_OPEN


/**
 *
 */
template <typename beam, typename solid>
BEAMINTERACTION::BeamToSolidVolumeMeshtyingPairBase<beam,
    solid>::BeamToSolidVolumeMeshtyingPairBase()
    : base_class(), meshtying_is_evaluated_(false)
{
  // Empty constructor.
}

/**
 *
 */
template <typename beam, typename solid>
void BEAMINTERACTION::BeamToSolidVolumeMeshtyingPairBase<beam, solid>::Setup()
{
  // Call setup of base class first.
  base_class::Setup();

  // Get the solid element data container
  ele2posref_ = GEOMETRYPAIR::InitializeElementData<solid, double>::Initialize(this->Element2());
  ele2pos_ = GEOMETRYPAIR::InitializeElementData<solid, scalar_type>::Initialize(this->Element2());

  // Set reference nodal positions for the solid element
  for (unsigned int n = 0; n < solid::n_nodes_; ++n)
  {
    const DRT::Node* node = this->Element2()->Nodes()[n];
    for (int d = 0; d < 3; ++d) ele2posref_.element_position_(3 * n + d) = node->X()[d];
  }

  // Initialize current nodal positions for the solid element
  for (unsigned int i = 0; i < solid::n_dof_; i++) ele2pos_.element_position_(i) = 0.0;
}

/**
 *
 */
template <typename beam, typename solid>
void BEAMINTERACTION::BeamToSolidVolumeMeshtyingPairBase<beam, solid>::CreateGeometryPair(
    const DRT::Element* element1, const DRT::Element* element2,
    const Teuchos::RCP<GEOMETRYPAIR::GeometryEvaluationDataBase>& geometry_evaluation_data_ptr)
{
  this->geometry_pair_ = GEOMETRYPAIR::GeometryPairLineToVolumeFactory<double, beam, solid>(
      element1, element2, geometry_evaluation_data_ptr);
}


/**
 *
 */
template <typename beam, typename solid>
void BEAMINTERACTION::BeamToSolidVolumeMeshtyingPairBase<beam, solid>::PreEvaluate()
{
  // Call PreEvaluate on the geometry Pair.
  if (!meshtying_is_evaluated_)
  {
    GEOMETRYPAIR::ElementData<beam, double> beam_coupling_ref;
    GEOMETRYPAIR::ElementData<solid, double> solid_coupling_ref;
    this->GetCouplingReferencePosition(beam_coupling_ref, solid_coupling_ref);
    CastGeometryPair()->PreEvaluate(
        beam_coupling_ref, solid_coupling_ref, this->line_to_3D_segments_);
  }
}

/**
 *
 */
template <typename beam, typename solid>
void BEAMINTERACTION::BeamToSolidVolumeMeshtyingPairBase<beam, solid>::ResetState(
    const std::vector<double>& beam_centerline_dofvec,
    const std::vector<double>& solid_nodal_dofvec)
{
  // Call the method in the parent class.
  base_class::ResetState(beam_centerline_dofvec, solid_nodal_dofvec);

  // Solid element.
  for (unsigned int i = 0; i < solid::n_dof_; i++)
  {
    ele2pos_.element_position_(i) = CORE::FADUTILS::HigherOrderFadValue<scalar_type>::apply(
        beam::n_dof_ + solid::n_dof_, beam::n_dof_ + i, solid_nodal_dofvec[i]);
  }
}

/**
 *
 */
template <typename beam, typename solid>
void BEAMINTERACTION::BeamToSolidVolumeMeshtyingPairBase<beam, solid>::SetRestartDisplacement(
    const std::vector<std::vector<double>>& centerline_restart_vec_)
{
  // Call the parent method.
  base_class::SetRestartDisplacement(centerline_restart_vec_);

  // We only set the restart displacement, if the current section has the restart coupling flag.
  if (this->Params()->BeamToSolidVolumeMeshtyingParams()->GetCoupleRestartState())
  {
    for (unsigned int i_dof = 0; i_dof < beam::n_dof_; i_dof++)
      ele1posref_offset_(i_dof) = centerline_restart_vec_[0][i_dof];

    // Add the displacement at the restart step to the solid reference position.
    for (unsigned int i_dof = 0; i_dof < solid::n_dof_; i_dof++)
      ele2posref_offset_(i_dof) = centerline_restart_vec_[1][i_dof];
  }
}


/**
 *
 */
template <typename beam, typename solid>
void BEAMINTERACTION::BeamToSolidVolumeMeshtyingPairBase<beam, solid>::GetPairVisualization(
    Teuchos::RCP<BeamToSolidVisualizationOutputWriterBase> visualization_writer,
    Teuchos::ParameterList& visualization_params) const
{
  // Get visualization of base class.
  base_class::GetPairVisualization(visualization_writer, visualization_params);

  // Get the writers.
  Teuchos::RCP<BEAMINTERACTION::BeamToSolidOutputWriterVisualization> visualization_segmentation =
      visualization_writer->GetVisualizationWriter("btsvc-segmentation");
  Teuchos::RCP<BEAMINTERACTION::BeamToSolidOutputWriterVisualization>
      visualization_integration_points =
          visualization_writer->GetVisualizationWriter("btsvc-integration-points");
  if (visualization_segmentation.is_null() and visualization_integration_points.is_null()) return;

  const Teuchos::RCP<const BeamToSolidVolumeMeshtyingVisualizationOutputParams>& output_params_ptr =
      visualization_params
          .get<Teuchos::RCP<const BeamToSolidVolumeMeshtyingVisualizationOutputParams>>(
              "btsvc-output_params_ptr");
  const bool write_unique_ids = output_params_ptr->GetWriteUniqueIDsFlag();

  if (visualization_segmentation != Teuchos::null)
  {
    // Setup variables.
    CORE::LINALG::Matrix<3, 1, scalar_type> X;
    CORE::LINALG::Matrix<3, 1, scalar_type> u;
    CORE::LINALG::Matrix<3, 1, scalar_type> r;

    // Get the visualization vectors.
    auto& visualization_data = visualization_segmentation->GetVisualizationData();
    std::vector<double>& point_coordinates = visualization_data.GetPointCoordinates();
    std::vector<double>& displacement = visualization_data.GetPointData<double>("displacement");

    std::vector<double>* pair_beam_id = nullptr;
    std::vector<double>* pair_solid_id = nullptr;
    if (write_unique_ids)
    {
      pair_beam_id = &(visualization_data.GetPointData<double>("uid_0_pair_beam_id"));
      pair_solid_id = &(visualization_data.GetPointData<double>("uid_1_pair_solid_id"));
    }

    // Loop over the segments on the beam.
    for (const auto& segment : this->line_to_3D_segments_)
    {
      // Add the left and right boundary point of the segment.
      for (const auto& segmentation_point : {segment.GetEtaA(), segment.GetEtaB()})
      {
        GEOMETRYPAIR::EvaluatePosition<beam>(segmentation_point, this->ele1posref_, X);
        GEOMETRYPAIR::EvaluatePosition<beam>(segmentation_point, this->ele1pos_, r);
        u = r;
        u -= X;
        for (unsigned int dim = 0; dim < 3; dim++)
        {
          point_coordinates.push_back(CORE::FADUTILS::CastToDouble(X(dim)));
          displacement.push_back(CORE::FADUTILS::CastToDouble(u(dim)));
        }

        if (write_unique_ids)
        {
          pair_beam_id->push_back(this->Element1()->Id());
          pair_solid_id->push_back(this->Element2()->Id());
        }
      }
    }
  }

  // If a writer exists for integration point data, add the integration point data.
  if (visualization_integration_points != Teuchos::null)
  {
    // Setup variables.
    CORE::LINALG::Matrix<3, 1, double> X;
    CORE::LINALG::Matrix<3, 1, double> u;
    CORE::LINALG::Matrix<3, 1, double> r;
    CORE::LINALG::Matrix<3, 1, double> r_solid;
    CORE::LINALG::Matrix<3, 1, double> force_integration_point;

    // Get the visualization vectors.
    auto& visualization_data = visualization_integration_points->GetVisualizationData();
    std::vector<double>& point_coordinates = visualization_data.GetPointCoordinates();
    std::vector<double>& displacement = visualization_data.GetPointData<double>("displacement");
    std::vector<double>& force = visualization_data.GetPointData<double>("force");

    std::vector<double>* pair_beam_id = nullptr;
    std::vector<double>* pair_solid_id = nullptr;
    if (write_unique_ids)
    {
      pair_beam_id = &(visualization_data.GetPointData<double>("uid_0_pair_beam_id"));
      pair_solid_id = &(visualization_data.GetPointData<double>("uid_1_pair_solid_id"));
    }

    // Loop over the segments on the beam.
    for (const auto& segment : this->line_to_3D_segments_)
    {
      // Add the integration points.
      for (const auto& projection_point : segment.GetProjectionPoints())
      {
        this->EvaluateBeamPositionDouble(projection_point, X, true);
        this->EvaluateBeamPositionDouble(projection_point, r, false);
        u = r;
        u -= X;
        GEOMETRYPAIR::EvaluatePosition<solid>(projection_point.GetXi(),
            GEOMETRYPAIR::ElementDataToDouble<solid>::ToDouble(this->ele2pos_), r_solid);
        EvaluatePenaltyForceDouble(r, r_solid, force_integration_point);
        for (unsigned int dim = 0; dim < 3; dim++)
        {
          point_coordinates.push_back(X(dim));
          displacement.push_back(u(dim));
          force.push_back(force_integration_point(dim));
        }

        if (write_unique_ids)
        {
          pair_beam_id->push_back(this->Element1()->Id());
          pair_solid_id->push_back(this->Element2()->Id());
        }
      }
    }
  }
}

/**
 *
 */
template <typename beam, typename solid>
void BEAMINTERACTION::BeamToSolidVolumeMeshtyingPairBase<beam, solid>::EvaluatePenaltyForceDouble(
    const CORE::LINALG::Matrix<3, 1, double>& r_beam,
    const CORE::LINALG::Matrix<3, 1, double>& r_solid,
    CORE::LINALG::Matrix<3, 1, double>& force) const
{
  // The base implementation of the force is a simple linear penalty law.
  force = r_solid;
  force -= r_beam;
  force.Scale(this->Params()->BeamToSolidVolumeMeshtyingParams()->GetPenaltyParameter());
}

/**
 *
 */
template <typename beam, typename solid>
void BEAMINTERACTION::BeamToSolidVolumeMeshtyingPairBase<beam, solid>::GetCouplingReferencePosition(
    GEOMETRYPAIR::ElementData<beam, double>& beam_coupling_ref,
    GEOMETRYPAIR::ElementData<solid, double>& solid_coupling_ref) const
{
  // Add the offset to the reference position.
  beam_coupling_ref = this->ele1posref_;
  beam_coupling_ref.element_position_ += this->ele1posref_offset_;
  solid_coupling_ref = ele2posref_;
  solid_coupling_ref.element_position_ += ele2posref_offset_;
}


/**
 * Explicit template initialization of template class.
 */
namespace BEAMINTERACTION
{
  using namespace GEOMETRYPAIR;

  template class BeamToSolidVolumeMeshtyingPairBase<t_hermite, t_hex8>;
  template class BeamToSolidVolumeMeshtyingPairBase<t_hermite, t_hex20>;
  template class BeamToSolidVolumeMeshtyingPairBase<t_hermite, t_hex27>;
  template class BeamToSolidVolumeMeshtyingPairBase<t_hermite, t_tet4>;
  template class BeamToSolidVolumeMeshtyingPairBase<t_hermite, t_tet10>;
  template class BeamToSolidVolumeMeshtyingPairBase<t_hermite, t_nurbs27>;
}  // namespace BEAMINTERACTION

BACI_NAMESPACE_CLOSE

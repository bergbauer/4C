/*! \file

\brief Meshtying element for meshtying between a 3D beam and a 3D fluid element using mortar shape
functions.

\level 3
*/


#include "4C_fbi_beam_to_fluid_meshtying_pair_mortar.hpp"

#include "4C_beaminteraction_beam_to_solid_visualization_output_writer_base.hpp"
#include "4C_beaminteraction_beam_to_solid_visualization_output_writer_visualization.hpp"
#include "4C_discretization_fem_general_extract_values.hpp"
#include "4C_fbi_beam_to_fluid_meshtying_output_params.hpp"
#include "4C_fbi_beam_to_fluid_mortar_manager.hpp"
#include "4C_geometry_pair_element.hpp"
#include "4C_geometry_pair_element_evaluation_functions.hpp"
#include "4C_geometry_pair_line_to_volume.hpp"
#include "4C_linalg_serialdensematrix.hpp"
#include "4C_linalg_serialdensevector.hpp"
#include "4C_linalg_utils_densematrix_inverse.hpp"

FOUR_C_NAMESPACE_OPEN


/**
 *
 */
template <typename beam, typename fluid, typename mortar>
BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<beam, fluid,
    mortar>::BeamToFluidMeshtyingPairMortar()
    : BeamToFluidMeshtyingPairBase<beam, fluid>()
{
  // Empty constructor.
}


/**
 *
 */
template <typename beam, typename fluid, typename mortar>
bool BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<beam, fluid, mortar>::EvaluateDM(
    CORE::LINALG::SerialDenseMatrix& local_D, CORE::LINALG::SerialDenseMatrix& local_M,
    CORE::LINALG::SerialDenseVector& local_kappa,
    CORE::LINALG::SerialDenseVector& local_constraint_offset)
{
  if (!this->meshtying_is_evaluated_)
  {
    this->CastGeometryPair()->Evaluate(
        this->ele1poscur_, this->ele2poscur_, this->line_to_3D_segments_);
    this->meshtying_is_evaluated_ = true;
  }

  // If there are no intersection segments, return no contact status.
  if (this->line_to_3D_segments_.size() == 0) return false;

  // Initialize variables for local mortar matrices.
  local_D.shape(mortar::n_dof_, beam::n_dof_);
  local_M.shape(mortar::n_dof_, fluid::n_dof_);
  local_kappa.size(mortar::n_dof_);


  // Initialize variables for shape function values.
  CORE::LINALG::Matrix<1, mortar::n_nodes_ * mortar::n_val_, double> N_mortar(true);
  CORE::LINALG::Matrix<1, beam::n_nodes_ * beam::n_val_, double> N_beam(true);
  CORE::LINALG::Matrix<1, fluid::n_nodes_ * fluid::n_val_, double> N_fluid(true);

  // Initialize variable for beam position derivative.
  CORE::LINALG::Matrix<3, 1, double> dr_beam_ref(true);

  // Initialize scalar variables.Clear
  double segment_jacobian, beam_segmentation_factor;

  // Calculate the mortar matrices.
  // Loop over segments.
  for (unsigned int i_segment = 0; i_segment < this->line_to_3D_segments_.size(); i_segment++)
  {
    // Factor to account for the integration segment length.
    beam_segmentation_factor = 0.5 * this->line_to_3D_segments_[i_segment].GetSegmentLength();

    // Gauss point loop.
    for (unsigned int i_gp = 0;
         i_gp < this->line_to_3D_segments_[i_segment].GetProjectionPoints().size(); i_gp++)
    {
      // Get the current Gauss point.
      const GEOMETRYPAIR::ProjectionPoint1DTo3D<double>& projected_gauss_point =
          this->line_to_3D_segments_[i_segment].GetProjectionPoints()[i_gp];

      // Get the jacobian in the reference configuration.
      GEOMETRYPAIR::EvaluatePositionDerivative1<beam>(
          projected_gauss_point.GetEta(), this->ele1posref_, dr_beam_ref);

      // Jacobian including the segment length.
      segment_jacobian = dr_beam_ref.Norm2() * beam_segmentation_factor;

      // Get the shape function matrices.
      N_mortar.Clear();
      N_beam.Clear();
      N_fluid.Clear();
      GEOMETRYPAIR::EvaluateShapeFunction<mortar>::Evaluate(
          N_mortar, projected_gauss_point.GetEta());
      GEOMETRYPAIR::EvaluateShapeFunction<beam>::Evaluate(
          N_beam, projected_gauss_point.GetEta(), this->ele1pos_.shape_function_data_);
      GEOMETRYPAIR::EvaluateShapeFunction<fluid>::Evaluate(N_fluid, projected_gauss_point.GetXi());

      // Fill in the local templated mortar matrix D.
      for (unsigned int i_mortar_node = 0; i_mortar_node < mortar::n_nodes_; i_mortar_node++)
        for (unsigned int i_mortar_val = 0; i_mortar_val < mortar::n_val_; i_mortar_val++)
          for (unsigned int i_beam_node = 0; i_beam_node < beam::n_nodes_; i_beam_node++)
            for (unsigned int i_beam_val = 0; i_beam_val < beam::n_val_; i_beam_val++)
              for (unsigned int i_dim = 0; i_dim < 3; i_dim++)
                local_D(i_mortar_node * mortar::n_val_ * 3 + i_mortar_val * 3 + i_dim,
                    i_beam_node * beam::n_val_ * 3 + i_beam_val * 3 + i_dim) +=
                    N_mortar(i_mortar_node * mortar::n_val_ + i_mortar_val) *
                    N_beam(i_beam_node * beam::n_val_ + i_beam_val) *
                    projected_gauss_point.GetGaussWeight() * segment_jacobian;

      // Fill in the local templated mortar matrix M.
      for (unsigned int i_mortar_node = 0; i_mortar_node < mortar::n_nodes_; i_mortar_node++)
        for (unsigned int i_mortar_val = 0; i_mortar_val < mortar::n_val_; i_mortar_val++)
          for (unsigned int i_fluid_node = 0; i_fluid_node < fluid::n_nodes_; i_fluid_node++)
            for (unsigned int i_fluid_val = 0; i_fluid_val < fluid::n_val_; i_fluid_val++)
              for (unsigned int i_dim = 0; i_dim < 3; i_dim++)
                local_M(i_mortar_node * mortar::n_val_ * 3 + i_mortar_val * 3 + i_dim,
                    i_fluid_node * fluid::n_val_ * 3 + i_fluid_val * 3 + i_dim) +=
                    N_mortar(i_mortar_node * mortar::n_val_ + i_mortar_val) *
                    N_fluid(i_fluid_node * fluid::n_val_ + i_fluid_val) *
                    projected_gauss_point.GetGaussWeight() * segment_jacobian;

      // Fill in the local templated mortar scaling vector kappa.
      for (unsigned int i_mortar_node = 0; i_mortar_node < mortar::n_nodes_; i_mortar_node++)
        for (unsigned int i_mortar_val = 0; i_mortar_val < mortar::n_val_; i_mortar_val++)
          for (unsigned int i_dim = 0; i_dim < 3; i_dim++)
            local_kappa(i_mortar_node * mortar::n_val_ * 3 + i_mortar_val * 3 + i_dim) +=
                N_mortar(i_mortar_node * mortar::n_val_ + i_mortar_val) *
                projected_gauss_point.GetGaussWeight() * segment_jacobian;
    }
  }

  // If we get to this point, the pair has a mortar contribution.
  return true;
}

/**
 *
 */
template <typename beam, typename fluid, typename mortar>
void BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<beam, fluid, mortar>::get_pair_visualization(
    Teuchos::RCP<BeamToSolidVisualizationOutputWriterBase> visualization_writer,
    Teuchos::ParameterList& visualization_params) const
{
  // Get visualization of base method.
  BeamToFluidMeshtyingPairBase<beam, fluid>::get_pair_visualization(
      visualization_writer, visualization_params);


  Teuchos::RCP<BEAMINTERACTION::BeamToSolidOutputWriterVisualization> visualization_discret =
      visualization_writer->get_visualization_writer("mortar");
  Teuchos::RCP<BEAMINTERACTION::BeamToSolidOutputWriterVisualization> visualization_continuous =
      visualization_writer->get_visualization_writer("mortar-continuous");
  if (visualization_discret != Teuchos::null || visualization_continuous != Teuchos::null)
  {
    // Setup variables.
    auto q_lambda = GEOMETRYPAIR::InitializeElementData<mortar, double>::Initialize(nullptr);
    CORE::LINALG::Matrix<3, 1, scalar_type> current_beamposition;
    CORE::LINALG::Matrix<3, 1, scalar_type> ref_beamposition;
    CORE::LINALG::Matrix<3, 1, scalar_type> beamdisplacement;
    CORE::LINALG::Matrix<3, 1, double> lambda_discret;
    CORE::LINALG::Matrix<3, 1, double> xi_mortar_node;

    // Get the mortar manager and the global lambda vector, those objects will be used to get the
    // discrete Lagrange multiplier values for this pair.
    Teuchos::RCP<const BEAMINTERACTION::BeamToFluidMortarManager> mortar_manager =
        visualization_params.get<Teuchos::RCP<const BEAMINTERACTION::BeamToFluidMortarManager>>(
            "mortar_manager");
    Teuchos::RCP<Epetra_Vector> lambda =
        visualization_params.get<Teuchos::RCP<Epetra_Vector>>("lambda");

    // Get the lambda GIDs of this pair.
    Teuchos::RCP<const BeamContactPair> this_rcp = Teuchos::rcp(this, false);
    std::vector<int> lambda_row;
    std::vector<double> lambda_pair;
    mortar_manager->LocationVector(this_rcp, lambda_row);
    CORE::FE::ExtractMyValues(*lambda, lambda_pair, lambda_row);
    for (unsigned int i_dof = 0; i_dof < mortar::n_dof_; i_dof++)
      q_lambda.element_position_(i_dof) = lambda_pair[i_dof];

    // Add the discrete values of the Lagrange multipliers.
    if (visualization_discret != Teuchos::null)
    {
      // Get the visualization vectors.
      auto& visualization_data = visualization_discret->get_visualization_data();
      std::vector<double>& point_coordinates = visualization_data.GetPointCoordinates();
      std::vector<double>& displacement = visualization_data.GetPointData<double>("displacement");
      std::vector<double>& lambda_vis = visualization_data.GetPointData<double>("lambda");

      for (unsigned int i_node = 0; i_node < mortar::n_nodes_; i_node++)
      {
        // Get the local coordinate of this node.
        xi_mortar_node = CORE::FE::GetNodeCoordinates(i_node, mortar::discretization_);

        // Get position and displacement of the mortar node.
        GEOMETRYPAIR::EvaluatePosition<beam>(
            xi_mortar_node(0), this->ele1pos_, current_beamposition);
        GEOMETRYPAIR::EvaluatePosition<beam>(
            xi_mortar_node(0), this->ele1posref_, ref_beamposition);
        beamdisplacement = current_beamposition;
        beamdisplacement -= ref_beamposition;

        // Get the discrete Lagrangian multiplier.
        GEOMETRYPAIR::EvaluatePosition<mortar>(xi_mortar_node(0), q_lambda, lambda_discret);

        // Add to output data.
        for (unsigned int dim = 0; dim < 3; dim++)
        {
          point_coordinates.push_back(CORE::FADUTILS::CastToDouble(current_beamposition(dim)));
          displacement.push_back(CORE::FADUTILS::CastToDouble(beamdisplacement(dim)));
          lambda_vis.push_back(CORE::FADUTILS::CastToDouble(lambda_discret(dim)));
        }
      }
    }


    // Add the continuous values for the Lagrange multipliers.
    if (visualization_continuous != Teuchos::null)
    {
      const unsigned int mortar_segments =
          visualization_params
              .get<Teuchos::RCP<const BeamToSolidVolumeMeshtyingVisualizationOutputParams>>(
                  "output_params_ptr")
              ->get_mortar_lambda_continuous_segments();
      double xi;
      auto& visualization_data = visualization_continuous->get_visualization_data();
      std::vector<double>& point_coordinates = visualization_data.GetPointCoordinates(
          (mortar_segments + 1) * 3 * this->line_to_3D_segments_.size());
      std::vector<double>& displacement = visualization_data.GetPointData<double>(
          "displacement", (mortar_segments + 1) * 3 * this->line_to_3D_segments_.size());
      std::vector<double>& lambda_vis = visualization_data.GetPointData<double>(
          "lambda", (mortar_segments + 1) * 3 * this->line_to_3D_segments_.size());
      std::vector<uint8_t>& cell_types = visualization_data.GetCellTypes();
      std::vector<int32_t>& cell_offsets = visualization_data.GetCellOffsets();

      for (const auto& segment : this->line_to_3D_segments_)
      {
        for (unsigned int i_curve_segment = 0; i_curve_segment <= mortar_segments;
             i_curve_segment++)
        {
          // Get the position, displacement and lambda value at the current point.
          xi = segment.GetEtaA() +
               i_curve_segment * (segment.GetEtaB() - segment.GetEtaA()) / (double)mortar_segments;
          GEOMETRYPAIR::EvaluatePosition<beam>(xi, this->ele1pos_, current_beamposition);
          GEOMETRYPAIR::EvaluatePosition<beam>(xi, this->ele1posref_, ref_beamposition);
          beamdisplacement = current_beamposition;
          beamdisplacement -= ref_beamposition;
          GEOMETRYPAIR::EvaluatePosition<mortar>(xi, q_lambda, lambda_discret);

          // Add to output data.
          for (unsigned int dim = 0; dim < 3; dim++)
          {
            point_coordinates.push_back(CORE::FADUTILS::CastToDouble(current_beamposition(dim)));
            displacement.push_back(CORE::FADUTILS::CastToDouble(beamdisplacement(dim)));
            lambda_vis.push_back(CORE::FADUTILS::CastToDouble(lambda_discret(dim)));
          }
        }

        // Add the cell for this segment (poly line).
        cell_types.push_back(4);
        cell_offsets.push_back(point_coordinates.size() / 3);
      }
    }
  }
}

template <typename beam, typename fluid, typename mortar>
void BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<beam, fluid, mortar>::evaluate_penalty_force(
    CORE::LINALG::Matrix<3, 1, scalar_type>& force,
    const GEOMETRYPAIR::ProjectionPoint1DTo3D<double>& projected_gauss_point,
    CORE::LINALG::Matrix<3, 1, scalar_type> v_beam) const
{
  force.PutScalar(0.);
}
/**
 * Explicit template initialization of template class.
 */
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex8, GEOMETRYPAIR::t_line2>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex20, GEOMETRYPAIR::t_line2>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex27, GEOMETRYPAIR::t_line2>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet4, GEOMETRYPAIR::t_line2>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet10, GEOMETRYPAIR::t_line2>;

template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex8, GEOMETRYPAIR::t_line3>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex20, GEOMETRYPAIR::t_line3>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex27, GEOMETRYPAIR::t_line3>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet4, GEOMETRYPAIR::t_line3>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet10, GEOMETRYPAIR::t_line3>;

template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex8, GEOMETRYPAIR::t_line4>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex20, GEOMETRYPAIR::t_line4>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex27, GEOMETRYPAIR::t_line4>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet4, GEOMETRYPAIR::t_line4>;
template class BEAMINTERACTION::BeamToFluidMeshtyingPairMortar<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet10, GEOMETRYPAIR::t_line4>;

FOUR_C_NAMESPACE_CLOSE

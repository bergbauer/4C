/*----------------------------------------------------------------------*/
/*! \file

\brief Meshtying element for meshtying between a 1D beam and a 3D fluid element.

\level 1
*/


#include "4C_fbi_beam_to_fluid_meshtying_pair_gauss_point.hpp"

#include "4C_beaminteraction_contact_params.hpp"
#include "4C_fbi_beam_to_fluid_meshtying_pair_base.hpp"
#include "4C_fbi_beam_to_fluid_meshtying_params.hpp"
#include "4C_geometry_pair_element.hpp"
#include "4C_geometry_pair_element_evaluation_functions.hpp"
#include "4C_geometry_pair_line_to_volume.hpp"
#include "4C_linalg_fixedsizematrix.hpp"
#include "4C_linalg_serialdensematrix.hpp"
#include "4C_linalg_serialdensevector.hpp"

FOUR_C_NAMESPACE_OPEN

/**
 *
 */
template <typename beam, typename fluid>
BEAMINTERACTION::BeamToFluidMeshtyingPairGaussPoint<beam,
    fluid>::BeamToFluidMeshtyingPairGaussPoint()
    : BeamToFluidMeshtyingPairBase<beam, fluid>()
{
  // Empty constructor.
}

/*------------------------------------------------------------------------------------------------*/

template <typename beam, typename fluid>
bool BEAMINTERACTION::BeamToFluidMeshtyingPairGaussPoint<beam, fluid>::Evaluate(
    CORE::LINALG::SerialDenseVector* forcevec1, CORE::LINALG::SerialDenseVector* forcevec2,
    CORE::LINALG::SerialDenseMatrix* stiffmat11, CORE::LINALG::SerialDenseMatrix* stiffmat12,
    CORE::LINALG::SerialDenseMatrix* stiffmat21, CORE::LINALG::SerialDenseMatrix* stiffmat22)
{
  // Call Evaluate on the geometry Pair
  if (!this->meshtying_is_evaluated_)
  {
    this->CastGeometryPair()->Evaluate(
        this->ele1poscur_, this->ele2poscur_, this->line_to_3D_segments_);
    this->meshtying_is_evaluated_ = true;
  }

  // If there are no intersection segments, return no contact status.
  if (this->line_to_3D_segments_.size() == 0) return false;

  // Initialize variables for position and force vectors.
  CORE::LINALG::Matrix<3, 1, double> dr_beam_ref;
  CORE::LINALG::Matrix<3, 1, scalar_type> r_beam;
  CORE::LINALG::Matrix<3, 1, scalar_type> r_fluid;
  CORE::LINALG::Matrix<3, 1, scalar_type> v_beam;
  CORE::LINALG::Matrix<3, 1, scalar_type> v_fluid;
  CORE::LINALG::Matrix<3, 1, scalar_type> force;
  CORE::LINALG::Matrix<3, 1, scalar_type> force2;
  CORE::LINALG::Matrix<beam::n_dof_, 1, scalar_type> force_element_1(true);
  CORE::LINALG::Matrix<fluid::n_dof_, 1, scalar_type> force_element_2(true);
  CORE::LINALG::Matrix<fluid::n_dof_, 1, scalar_type> force_element_f(true);
  CORE::LINALG::Matrix<1, beam::n_nodes_ * beam::n_val_, double> N_beam(true);
  CORE::LINALG::Matrix<1, fluid::n_nodes_ * fluid::n_val_, double> N_fluid(true);

  // Resize and initialize the return variables.
  if (forcevec1 != nullptr) forcevec1->size(beam::n_dof_);
  if (forcevec2 != nullptr) forcevec2->size(fluid::n_dof_);
  if (stiffmat11 != nullptr) stiffmat11->shape(beam::n_dof_, beam::n_dof_);
  if (stiffmat12 != nullptr) stiffmat12->shape(beam::n_dof_, fluid::n_dof_);
  if (stiffmat21 != nullptr) stiffmat21->shape(fluid::n_dof_, beam::n_dof_);
  if (stiffmat22 != nullptr) stiffmat22->shape(fluid::n_dof_, fluid::n_dof_);

  // Initialize scalar variables.
  double segment_jacobian, beam_segmentation_factor;

  // Calculate the meshtying forces.
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

      // Get the current positions on beam and fluid.
      GEOMETRYPAIR::EvaluatePosition<beam>(projected_gauss_point.GetEta(), this->ele1pos_, r_beam);
      GEOMETRYPAIR::EvaluatePosition<fluid>(projected_gauss_point.GetXi(), this->ele2pos_, r_fluid);

      N_beam.Clear();
      N_fluid.Clear();

      // Evaluate the chapefunctions at the current gauss point
      GEOMETRYPAIR::EvaluateShapeFunction<beam>::Evaluate(
          N_beam, projected_gauss_point.GetEta(), this->ele1pos_.shape_function_data_);
      GEOMETRYPAIR::EvaluateShapeFunction<fluid>::Evaluate(N_fluid, projected_gauss_point.GetXi());

      // assemble fluid mass matrix
      if (stiffmat22 != nullptr)
      {
        for (unsigned int i_fluid_node1 = 0; i_fluid_node1 < fluid::n_nodes_; i_fluid_node1++)
          for (unsigned int i_fluid_val1 = 0; i_fluid_val1 < fluid::n_val_; i_fluid_val1++)
            for (unsigned int i_dir = 0; i_dir < 3; i_dir++)
              for (unsigned int i_fluid_node2 = 0; i_fluid_node2 < fluid::n_nodes_; i_fluid_node2++)
                for (unsigned int i_fluid_val2 = 0; i_fluid_val2 < fluid::n_val_; i_fluid_val2++)
                  (*stiffmat22)(i_fluid_node1 * fluid::n_val_ * 3 + i_fluid_val1 * 3 + i_dir,
                      i_fluid_node2 * fluid::n_val_ * 3 + i_fluid_val2 * 3 + i_dir) +=
                      N_fluid(i_fluid_node1) * N_fluid(i_fluid_node2) *
                      projected_gauss_point.GetGaussWeight() * segment_jacobian;
      }

      // assemble beam mass matrix
      if (stiffmat11 != nullptr)
      {
        for (unsigned int i_beam_node1 = 0; i_beam_node1 < beam::n_nodes_; i_beam_node1++)
          for (unsigned int i_beam_val1 = 0; i_beam_val1 < beam::n_val_; i_beam_val1++)
            for (unsigned int i_dir = 0; i_dir < 3; i_dir++)
              for (unsigned int i_beam_node2 = 0; i_beam_node2 < beam::n_nodes_; i_beam_node2++)
                for (unsigned int i_beam_val2 = 0; i_beam_val2 < beam::n_val_; i_beam_val2++)
                  (*stiffmat11)(i_beam_node1 * beam::n_val_ * 3 + 3 * i_beam_val1 + i_dir,
                      i_beam_node2 * beam::n_val_ * 3 + 3 * i_beam_val2 + i_dir) +=
                      N_beam(i_beam_node1 * beam::n_val_ + i_beam_val1) *
                      N_beam(i_beam_node2 * beam::n_val_ + i_beam_val2) *
                      projected_gauss_point.GetGaussWeight() * segment_jacobian;
      }

      // assemble fluid beam coupling matrix
      if (stiffmat21 != nullptr)
      {
        for (unsigned int i_fluid_node1 = 0; i_fluid_node1 < fluid::n_nodes_; i_fluid_node1++)
          for (unsigned int i_fluid_val1 = 0; i_fluid_val1 < fluid::n_val_; i_fluid_val1++)
            for (unsigned int i_dir = 0; i_dir < 3; i_dir++)
              for (unsigned int i_beam_node2 = 0; i_beam_node2 < beam::n_nodes_; i_beam_node2++)
                for (unsigned int i_beam_val2 = 0; i_beam_val2 < beam::n_val_; i_beam_val2++)
                  (*stiffmat21)(i_fluid_node1 * fluid::n_val_ * 3 + i_fluid_val1 * 3 + i_dir,
                      i_beam_node2 * beam::n_val_ * 3 + 3 * i_beam_val2 + i_dir) +=
                      N_fluid(i_fluid_node1) * N_beam(i_beam_node2 * beam::n_val_ + i_beam_val2) *
                      projected_gauss_point.GetGaussWeight() * segment_jacobian;
      }

      // assemble fluid beam coupling matrix
      if (stiffmat12 != nullptr)
      {
        for (unsigned int i_beam_node1 = 0; i_beam_node1 < beam::n_nodes_; i_beam_node1++)
          for (unsigned int i_beam_val1 = 0; i_beam_val1 < beam::n_val_; i_beam_val1++)
            for (unsigned int i_dir = 0; i_dir < 3; i_dir++)
              for (unsigned int i_fluid_node2 = 0; i_fluid_node2 < fluid::n_nodes_; i_fluid_node2++)
                for (unsigned int i_fluid_val2 = 0; i_fluid_val2 < fluid::n_val_; i_fluid_val2++)
                  (*stiffmat12)(i_beam_node1 * beam::n_val_ * 3 + 3 * i_beam_val1 + i_dir,
                      i_fluid_node2 * fluid::n_val_ * 3 + 3 * i_fluid_val2 + i_dir) +=
                      N_fluid(i_fluid_node2) * N_beam(i_beam_node1 * beam::n_val_ + i_beam_val1) *
                      projected_gauss_point.GetGaussWeight() * segment_jacobian;
      }
    }
  }

  // assemble structure force vector
  {
    if (forcevec1 != nullptr)
    {
      for (unsigned int i_dof1 = 0; i_dof1 < beam::n_dof_; i_dof1++)
      {
        for (unsigned int i_dof2 = 0; i_dof2 < beam::n_dof_; i_dof2++)
          (*forcevec1)(i_dof1) +=
              (*stiffmat11)(i_dof1, i_dof2) *
              CORE::FADUTILS::CastToDouble(this->ele1vel_.element_position_(i_dof2));
        for (unsigned int i_dof2 = 0; i_dof2 < fluid::n_dof_; i_dof2++)
          (*forcevec1)(i_dof1) -=
              (*stiffmat12)(i_dof1, i_dof2) *
              CORE::FADUTILS::CastToDouble(this->ele2vel_.element_position_(i_dof2));
      }
    }

    // assemble fluid force vector
    if (forcevec2 != nullptr)
    {
      if (!Teuchos::rcp_dynamic_cast<FBI::BeamToFluidMeshtyingParams>(this->Params(), true)
               ->get_weak_dirichlet_flag())

      {
        for (unsigned int i_dof1 = 0; i_dof1 < fluid::n_dof_; i_dof1++)
        {
          for (unsigned int i_dof2 = 0; i_dof2 < fluid::n_dof_; i_dof2++)
            (*forcevec2)(i_dof1) +=
                (*stiffmat22)(i_dof1, i_dof2) *
                CORE::FADUTILS::CastToDouble(this->ele2vel_.element_position_(i_dof2));
          for (unsigned int i_dof2 = 0; i_dof2 < beam::n_dof_; i_dof2++)
            (*forcevec2)(i_dof1) -=
                (*stiffmat21)(i_dof1, i_dof2) *
                CORE::FADUTILS::CastToDouble(this->ele1vel_.element_position_(i_dof2));
        }
      }
      else

        for (unsigned int i_dof1 = 0; i_dof1 < fluid::n_dof_; i_dof1++)
        {
          for (unsigned int i_dof2 = 0; i_dof2 < beam::n_dof_; i_dof2++)
            (*forcevec2)(i_dof1) -=
                (*stiffmat21)(i_dof1, i_dof2) *
                CORE::FADUTILS::CastToDouble(this->ele1vel_.element_position_(i_dof2));
        }
    }
  }

  // Return true as there are meshtying contributions.
  return true;
}

/**
 * Explicit template initialization of template class.
 */
// Hermite beam element, hex8 solid element.
template class BEAMINTERACTION::BeamToFluidMeshtyingPairGaussPoint<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex8>;
// Hermite beam element, hex20 solid element.
template class BEAMINTERACTION::BeamToFluidMeshtyingPairGaussPoint<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex20>;
// Hermite beam element, hex27 solid element.
template class BEAMINTERACTION::BeamToFluidMeshtyingPairGaussPoint<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_hex27>;
// Hermite beam element, tet4 solid element.
template class BEAMINTERACTION::BeamToFluidMeshtyingPairGaussPoint<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet4>;
// Hermite beam element, tet10 solid element.
template class BEAMINTERACTION::BeamToFluidMeshtyingPairGaussPoint<GEOMETRYPAIR::t_hermite,
    GEOMETRYPAIR::t_tet10>;

FOUR_C_NAMESPACE_CLOSE

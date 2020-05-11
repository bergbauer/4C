/*----------------------------------------------------------------------*/
/*! \file

\brief Element classes that represent faces, i.e. surface elements.

\level 1
\maintainer Ivo Steinbrecher
*/
// End doxygen header.


#include "geometry_pair_element_faces.H"

#include "../drt_fem_general/drt_utils_local_connectivity_matrices.H"


/**
 *
 */
void GEOMETRYPAIR::FaceElement::Setup(const Teuchos::RCP<const DRT::Discretization>& discret,
    const std::unordered_map<int, Teuchos::RCP<GEOMETRYPAIR::FaceElement>>& face_elements)
{
  // Initialize class variables.
  patch_dof_gid_.clear();
  connected_faces_.clear();

  // Initialize variables for this method.
  std::vector<int> my_node_gid, other_faces_node_gid;
  my_node_gid.clear();
  my_node_gid.reserve(drt_face_element_->NumNode());
  other_faces_node_gid.clear();

  // Temporary variables.
  std::vector<int> temp_node_dof_gid(3);

  // First add the node GIDs of this face.
  for (int i_node = 0; i_node < drt_face_element_->NumNode(); i_node++)
  {
    const DRT::Node* my_node = drt_face_element_->Nodes()[i_node];
    my_node_gid.push_back(my_node->Id());
    discret->Dof(my_node, 0, temp_node_dof_gid);
    for (const auto& value : temp_node_dof_gid) patch_dof_gid_.push_back(value);
  }

  // Add the node GIDs of the connected faces.
  ConnectedFace temp_connected_face;
  for (int i_node = 0; i_node < drt_face_element_->NumNode(); i_node++)
  {
    // Loop over all elements connected to a node of this face.
    const DRT::Node* node = drt_face_element_->Nodes()[i_node];
    for (int i_element = 0; i_element < node->NumElement(); i_element++)
    {
      const DRT::Element* element = node->Elements()[i_element];

      // Do nothing for this element.
      if (element->Id() == drt_face_element_->ParentElementId()) continue;

      // Check if the element was already searched for.
      if (connected_faces_.find(element->Id()) == connected_faces_.end())
      {
        temp_connected_face.node_lid_map_.clear();
        temp_connected_face.my_node_patch_lid_.clear();

        // Check if the element is part of the surface condition.
        auto find_in_faces = face_elements.find(element->Id());
        if (find_in_faces != face_elements.end())
        {
          // Add the node GIDs of this element.
          for (int i_node_connected_element = 0;
               i_node_connected_element < find_in_faces->second->GetDrtFaceElement()->NumNode();
               i_node_connected_element++)
          {
            const DRT::Node* other_node =
                find_in_faces->second->GetDrtFaceElement()->Nodes()[i_node_connected_element];
            const int node_id = other_node->Id();

            // Check if this node is part of this face element. If not and if it is not already in
            // other_faces_node_gid, it is added to that vector.
            auto it = std::find(my_node_gid.begin(), my_node_gid.end(), node_id);
            if (it == my_node_gid.end())
            {
              if (std::find(other_faces_node_gid.begin(), other_faces_node_gid.end(), node_id) ==
                  other_faces_node_gid.end())
              {
                other_faces_node_gid.push_back(node_id);
                discret->Dof(other_node, 0, temp_node_dof_gid);
                for (const auto& value : temp_node_dof_gid) patch_dof_gid_.push_back(value);
              }
              temp_connected_face.my_node_patch_lid_.push_back(
                  my_node_gid.size() + other_faces_node_gid.size() - 1);
            }
            else
            {
              // The node is part of this face element, add to the map entry.
              const int index_my_node = std::distance(my_node_gid.begin(), it);
              temp_connected_face.node_lid_map_[i_node_connected_element] = index_my_node;
              temp_connected_face.my_node_patch_lid_.push_back(index_my_node);
            }
          }

          // Add this element to the already searched connected elements.
          connected_faces_[element->Id()] = temp_connected_face;
        }
      }
    }
  }
}

/**
 *
 */
void GEOMETRYPAIR::FaceElement::GetConnectedFaces(
    const std::unordered_map<int, Teuchos::RCP<GEOMETRYPAIR::FaceElement>>& face_elements,
    std::vector<Teuchos::RCP<GEOMETRYPAIR::FaceElement>>& connected_faces) const
{
  // First we have to find the global IDs of the volume elements connected to the face.
  std::set<int> connected_elements_id;
  for (int i_node = 0; i_node < drt_face_element_->NumNode(); i_node++)
  {
    const DRT::Node* node = drt_face_element_->Nodes()[i_node];
    for (int i_element = 0; i_element < node->NumElement(); i_element++)
    {
      const DRT::Element* node_element = node->Elements()[i_element];
      connected_elements_id.insert(node_element->Id());
    }
  }

  // Check that the global IDs are in the face_element map, i.e. they are in the surface condition.
  connected_faces.clear();
  for (const auto& volume_id : connected_elements_id)
  {
    // This element does not count as a connected face.
    if (volume_id == drt_face_element_->ParentElementId()) continue;

    auto find_in_faces = face_elements.find(volume_id);
    if (find_in_faces != face_elements.end()) connected_faces.push_back(find_in_faces->second);
  }
}

/**
 *
 */
void GEOMETRYPAIR::FaceElement::GetPatchLocalToGlobalIndices(
    const std::vector<Teuchos::RCP<GEOMETRYPAIR::FaceElement>>& connected_faces,
    std::vector<int>& ltg) const
{
  dserror("Has to be implemented!");
  // The fist DOFs will be the ones from this face element.
  ltg.clear();
}


/**
 *
 */
template <typename surface, typename scalar_type>
void GEOMETRYPAIR::FaceElementTemplate<surface, scalar_type>::Setup(
    const Teuchos::RCP<const DRT::Discretization>& discret,
    const std::unordered_map<int, Teuchos::RCP<GEOMETRYPAIR::FaceElement>>& face_elements)
{
  // Call setup of base class.
  FaceElement::Setup(discret, face_elements);

  // Set the reference position from the nodes connected to this face.
  face_reference_position_.Clear();
  const DRT::Node* const* nodes = drt_face_element_->Nodes();
  for (unsigned int i_node = 0; i_node < surface::n_nodes_; i_node++)
    for (unsigned int i_dim = 0; i_dim < 3; i_dim++)
      face_reference_position_(i_node * 3 + i_dim) = nodes[i_node]->X()[i_dim];
}

/**
 *
 */
template <typename surface, typename scalar_type>
void GEOMETRYPAIR::FaceElementTemplate<surface, scalar_type>::SetState(
    const Teuchos::RCP<const DRT::Discretization>& discret,
    const Teuchos::RCP<const Epetra_Vector>& displacement)
{
  // Set the displacement at the current configuration for this element.
  std::vector<int> lmowner, lmstride, face_dof_gid;
  std::vector<double> element_displacement;
  drt_face_element_->LocationVector(*discret, face_dof_gid, lmowner, lmstride);
  DRT::UTILS::ExtractMyValues(*displacement, element_displacement, face_dof_gid);
  for (unsigned int i_dof = 0; i_dof < surface::n_dof_; i_dof++)
    face_position_(i_dof) =
        face_reference_position_(i_dof) +
        FADUTILS::HigherOrderFad<scalar_type>::apply(
            surface::n_dof_ + n_beam_dof_, i_dof + n_beam_dof_, element_displacement[i_dof]);
};

/**
 *
 */
template <typename surface, typename scalar_type>
void GEOMETRYPAIR::FaceElementTemplate<surface, scalar_type>::CalculateAveragedNormals(
    const std::unordered_map<int, Teuchos::RCP<GEOMETRYPAIR::FaceElement>>& face_elements)
{
  // Get the connected face elements.
  std::vector<Teuchos::RCP<GEOMETRYPAIR::FaceElement>> connected_faces;
  GetConnectedFaces(face_elements, connected_faces);

  // From here on we need this face element in the connected_faces vector.
  connected_faces.push_back(face_elements.at(drt_face_element_->ParentElementId()));

  // Set up nodal averaged normal vectors (in reference and current configuration).
  LINALG::Matrix<surface::n_nodes_, 1, int> normal_ids(true);
  LINALG::Matrix<surface::n_nodes_, 1, int> normal_count(true);
  LINALG::Matrix<3, 1, double> reference_normal(true);
  LINALG::Matrix<3, 1, scalar_type> current_normal(true);
  LINALG::Matrix<surface::n_nodes_, 1, LINALG::Matrix<3, 1, double>> reference_normals(true);
  LINALG::Matrix<surface::n_nodes_, 1, LINALG::Matrix<3, 1, scalar_type>> current_normals(true);

  // Fill in the global node IDs.
  for (unsigned int i_node = 0; i_node < surface::n_nodes_; i_node++)
    normal_ids(i_node) = drt_face_element_->Nodes()[i_node]->Id();

  // Calculate the normals on the nodes from all connected (including this one) faces.
  for (const auto& connected_face : connected_faces)
  {
    const auto connected_face_template = dynamic_cast<const my_type*>(connected_face.get());
    for (unsigned int i_node = 0; i_node < surface::n_nodes_; i_node++)
    {
      if (connected_face_template->EvaluateNodalNormal(
              normal_ids(i_node), reference_normal, current_normal))
      {
        normal_count(i_node) += 1;
        reference_normals(i_node) += reference_normal;
        current_normals(i_node) += current_normal;
      }
    }
  }

  // Average the normals.
  for (unsigned int i_node = 0; i_node < surface::n_nodes_; i_node++)
  {
    if (normal_count(i_node) == 0)
      dserror(
          "No normals calculated for node %d in volume element %d. There has to be at least one.",
          i_node, drt_face_element_->ParentElementId());

    reference_normals(i_node).Scale(1.0 / FADUTILS::VectorNorm(reference_normals(i_node)));
    current_normals(i_node).Scale(1.0 / FADUTILS::VectorNorm(current_normals(i_node)));
  }
  for (unsigned int i_node = 0; i_node < surface::n_nodes_; i_node++)
  {
    for (unsigned int i_dir = 0; i_dir < 3; i_dir++)
    {
      reference_normals_(3 * i_node + i_dir) = reference_normals(i_node)(i_dir);
      current_normals_(3 * i_node + i_dir) = current_normals(i_node)(i_dir);
    }
  }
}

/**
 *
 */
template <typename surface, typename scalar_type>
bool GEOMETRYPAIR::FaceElementTemplate<surface, scalar_type>::EvaluateNodalNormal(
    const int node_gid, LINALG::Matrix<3, 1, double>& reference_normal,
    LINALG::Matrix<3, 1, scalar_type>& current_normal) const
{
  // Check if the desired node is part of this face.
  int node_lid = -1;
  for (unsigned int i_node = 0; i_node < surface::n_nodes_; i_node++)
  {
    if (drt_face_element_->NodeIds()[i_node] == node_gid)
    {
      node_lid = i_node;
      break;
    }
  }
  if (node_lid < 0)
    // The desired node is not in this face.
    return false;

  // Evaluate this the normal at the desired node.
  {
    reference_normal.Clear();
    current_normal.Clear();

    // Set the parameter coordinate.
    LINALG::Matrix<3, 1, double> xi(true);
    LINALG::SerialDenseMatrix nodal_coordinates =
        DRT::UTILS::getEleNodeNumbering_nodes_paramspace(surface::discretization_);
    for (unsigned int i_dim = 0; i_dim < 2; i_dim++) xi(i_dim) = nodal_coordinates(i_dim, node_lid);

    // Calculate the normal on the surface.
    EvaluateSurfaceNormal<surface>(
        xi, face_reference_position_, reference_normal, drt_face_element_.get());
    EvaluateSurfaceNormal<surface>(xi, face_position_, current_normal, drt_face_element_.get());

    return true;
  }
}

/**
 *
 */
template <typename surface, typename scalar_type>
void GEOMETRYPAIR::FaceElementTemplate<surface, scalar_type>::EvaluateFacePositionDouble(
    const LINALG::Matrix<2, 1, double>& xi, LINALG::Matrix<3, 1, double>& r, bool reference) const
{
  LINALG::Matrix<surface::n_dof_, 1, double> position_double;
  if (reference)
    position_double = FADUTILS::CastToDouble(face_reference_position_);
  else
    position_double = FADUTILS::CastToDouble(face_position_);

  EvaluatePosition<surface>(xi, position_double, r, drt_face_element_.get());
}

/**
 *
 */
template <typename surface, typename scalar_type>
void GEOMETRYPAIR::FaceElementTemplate<surface, scalar_type>::EvaluateFaceNormalDouble(
    const LINALG::Matrix<2, 1, double>& xi, LINALG::Matrix<3, 1, double>& n, const bool reference,
    const bool averaged_normal) const
{
  LINALG::Matrix<surface::n_dof_, 1, double> position_double(true);
  LINALG::Matrix<3 * surface::n_nodes_, 1, double> normals_double;
  bool valid_pointer = false;

  if (averaged_normal)
  {
    if (reference)
      VectorPointerToVectorDouble(this->GetReferenceNormals(), normals_double, valid_pointer);
    else
      VectorPointerToVectorDouble(this->GetCurrentNormals(), normals_double, valid_pointer);
  }

  if (valid_pointer && averaged_normal)
  {
    // Return the normal calculated with the averaged normal field.
    EvaluateSurfaceNormal<surface>(
        xi, position_double, n, drt_face_element_.get(), &normals_double);
  }
  else if ((not valid_pointer) && averaged_normal)
  {
    // Averaged normals are desired, but there is no valid pointer to them -> return a zero vector.
    n.PutScalar(0.);
  }
  else
  {
    // Calculate the normals on the face geometry.
    if (reference)
      position_double = FADUTILS::CastToDouble(face_reference_position_);
    else
      position_double = FADUTILS::CastToDouble(face_position_);

    EvaluateSurfaceNormal<surface>(xi, position_double, n, drt_face_element_.get());
  }
}


/**
 *
 */
Teuchos::RCP<GEOMETRYPAIR::FaceElement> GEOMETRYPAIR::FaceElementFactory(
    const Teuchos::RCP<const DRT::FaceElement>& face_element, const bool is_fad)
{
  if (not is_fad)
  {
    switch (face_element->Shape())
    {
      case DRT::Element::tri3:
        return Teuchos::rcp(new FaceElementTemplate<t_tri3,
            Sacado::ELRFad::SLFad<double, t_hermite::n_dof_ + t_tri3::n_dof_>>(face_element));
      case DRT::Element::tri6:
        return Teuchos::rcp(new FaceElementTemplate<t_tri6,
            Sacado::ELRFad::SLFad<double, t_hermite::n_dof_ + t_tri6::n_dof_>>(face_element));
      case DRT::Element::quad4:
        return Teuchos::rcp(new FaceElementTemplate<t_quad4,
            Sacado::ELRFad::SLFad<double, t_hermite::n_dof_ + t_quad4::n_dof_>>(face_element));
      case DRT::Element::quad8:
        return Teuchos::rcp(new FaceElementTemplate<t_quad8,
            Sacado::ELRFad::SLFad<double, t_hermite::n_dof_ + t_quad8::n_dof_>>(face_element));
      case DRT::Element::quad9:
        return Teuchos::rcp(new FaceElementTemplate<t_quad9,
            Sacado::ELRFad::SLFad<double, t_hermite::n_dof_ + t_quad9::n_dof_>>(face_element));
      case DRT::Element::nurbs9:
        return Teuchos::rcp(new FaceElementTemplate<t_nurbs9,
            Sacado::ELRFad::SLFad<double, t_hermite::n_dof_ + t_nurbs9::n_dof_>>(face_element));
      default:
        dserror("Wrong discretization type given.");
    }
  }
  else
  {
    switch (face_element->Shape())
    {
      case DRT::Element::tri3:
        return Teuchos::rcp(
            new FaceElementTemplate<t_tri3, Sacado::ELRFad::DFad<Sacado::ELRFad::DFad<double>>>(
                face_element));
      case DRT::Element::tri6:
        return Teuchos::rcp(
            new FaceElementTemplate<t_tri6, Sacado::ELRFad::DFad<Sacado::ELRFad::DFad<double>>>(
                face_element));
      case DRT::Element::quad4:
        return Teuchos::rcp(
            new FaceElementTemplate<t_quad4, Sacado::ELRFad::DFad<Sacado::ELRFad::DFad<double>>>(
                face_element));
      case DRT::Element::quad8:
        return Teuchos::rcp(
            new FaceElementTemplate<t_quad8, Sacado::ELRFad::DFad<Sacado::ELRFad::DFad<double>>>(
                face_element));
      case DRT::Element::quad9:
        return Teuchos::rcp(
            new FaceElementTemplate<t_quad9, Sacado::ELRFad::DFad<Sacado::ELRFad::DFad<double>>>(
                face_element));
      case DRT::Element::nurbs9:
        return Teuchos::rcp(new FaceElementTemplate<t_nurbs9,
            Sacado::ELRFad::SLFad<Sacado::ELRFad::SLFad<double, GEOMETRYPAIR::t_hermite::n_dof_ +
                                                                    GEOMETRYPAIR::t_nurbs9::n_dof_>,
                GEOMETRYPAIR::t_hermite::n_dof_ + GEOMETRYPAIR::t_nurbs9::n_dof_>>(face_element));
      default:
        dserror("Wrong discretization type given.");
    }
  }

  return Teuchos::null;
}

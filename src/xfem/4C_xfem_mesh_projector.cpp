/*----------------------------------------------------------------------*/
/*! \file

\brief Projection of state vectors between overlapping meshes

\level 2


*/
/*----------------------------------------------------------------------*/

#include "4C_xfem_mesh_projector.hpp"

#include "4C_comm_exporter.hpp"
#include "4C_cut_boundingbox.hpp"
#include "4C_cut_position.hpp"
#include "4C_fem_general_extract_values.hpp"
#include "4C_fem_geometry_searchtree.hpp"
#include "4C_fem_geometry_searchtree_service.hpp"
#include "4C_io.hpp"
#include "4C_io_control.hpp"
#include "4C_io_gmsh.hpp"
#include "4C_io_pstream.hpp"
#include "4C_linalg_serialdensevector.hpp"
#include "4C_linalg_utils_sparse_algebra_math.hpp"
#include "4C_xfem_discretization.hpp"
#include "4C_xfem_discretization_utils.hpp"

#include <Teuchos_TimeMonitor.hpp>

FOUR_C_NAMESPACE_OPEN

XFEM::MeshProjector::MeshProjector(Teuchos::RCP<const Core::FE::Discretization> sourcedis,
    Teuchos::RCP<const Core::FE::Discretization> targetdis, const Teuchos::ParameterList& params,
    Teuchos::RCP<const Epetra_Vector> sourcedisp)
    : sourcedis_(sourcedis),
      targetdis_(targetdis),
      searchradius_fac_(
          params.sublist("XFLUID DYNAMIC/GENERAL").get<double>("XFLUIDFLUID_SEARCHRADIUS"))
{
  set_source_position_vector(sourcedisp);
  // in case the source discretization is empty on this proc
  if (!sourcedis_->NumMyRowElements())
  {
    searchradius_ = 0.0;
    return;
  }

  // determine the radius of the search tree - grab an arbitrary element to
  // find a characteristic size -dependent length scale of the fluid mesh
  // (not the best choice)
  switch (sourcedis_->lRowElement(0)->Shape())
  {
    case Core::FE::CellType::hex8:
      find_search_radius<Core::FE::CellType::hex8>();
      break;
    case Core::FE::CellType::hex20:
      find_search_radius<Core::FE::CellType::hex20>();
      break;
    case Core::FE::CellType::hex27:
      find_search_radius<Core::FE::CellType::hex27>();
      break;
    default:
      searchradius_ = searchradius_fac_;  // avoid a
      break;
  }
}

void XFEM::MeshProjector::set_source_position_vector(Teuchos::RCP<const Epetra_Vector> sourcedisp)
{
  src_nodepositions_n_.clear();
  // set position of source nodes
  // we run over the col nodes, as we need the full src_nodepositions
  // for all nodes of an element on each proc
  for (int lid = 0; lid < sourcedis_->NumMyColNodes(); ++lid)
  {
    const Core::Nodes::Node* node = sourcedis_->lColNode(lid);
    std::vector<int> src_dofs(4);
    std::vector<double> mydisp(3, 0.0);

    if (sourcedisp != Teuchos::null)
    {
      // get the current displacement
      sourcedis_->Dof(node, 0, src_dofs);
      Core::FE::ExtractMyValues(*sourcedisp, mydisp, src_dofs);
    }

    for (int d = 0; d < 3; ++d) src_nodepositions_n_[node->Id()](d) = node->X()[d] + mydisp.at(d);
  }
}

template <Core::FE::CellType distype>
void XFEM::MeshProjector::find_search_radius()
{
  Core::Elements::Element* actele = sourcedis_->lRowElement(0);
  const Core::Nodes::Node* const* nodes = actele->Nodes();

  // problem dimension
  const unsigned int dim = Core::FE::dim<distype>;

  // we are looking for the maximum diameter of the source element
  // as an estimate for the search radius
  // REMARK: the selection of the embedded element for this estimate is still
  // arbitrary --> choose a sufficiently large safety factor in the input file
  double max_diameter = 0.0;

  // build connectivity matrix for every surface of the embedded element
  std::vector<std::vector<int>> connectivity = Core::FE::getEleNodeNumberingSurfaces(distype);

  //-----------------------------------------------------------------------------------
  // We have hex elements & the faces are quads:
  // the first 4 nodes in the element node numbering vector for a given surface are the
  // corner nodes (equally numbered for hex8/20/hex27), followed by the central nodes
  // in case of hex20/27; in approx. diameter estimation, mid nodes are neglected
  //-----------------------------------------------------------------------------------

  // loop over element surfaces
  for (std::vector<std::vector<int>>::const_iterator ic = connectivity.begin();
       ic != connectivity.end(); ++ic)
  {
    // get the set of nodes (connected in sequence) for the current surface
    const std::vector<int>& surf_nodeset = *ic;

    // compute the connections 0th->2nd, 1st->3rd corner node
    for (unsigned int icn = 0; icn < 2; ++icn)
    {
      // next but one node position in vector
      const unsigned icnn = icn + 2;

      // compute the distance
      double dist_square = 0.0;
      for (unsigned int isd = 0; isd < dim; isd++)
      {
        double dx = nodes[surf_nodeset[icnn]]->X()[isd] - nodes[icn]->X()[isd];
        dist_square += dx * dx;
      }

      double dist = sqrt(dist_square);

      // new maximum?
      if (dist > max_diameter) max_diameter = dist;
    }
  }  // done with the surface elements

  // the spatial diagonals

  const unsigned ncn_face = 4;
  for (unsigned icn = 0; icn < 1; ++icn)
  {
    // diagonally opposite (0-6, 1-7)
    {
      const unsigned icn_opp = icn + 2 + ncn_face;
      double dist_square = 0.0;
      for (unsigned int isd = 0; isd < dim; isd++)
      {
        double dx = nodes[icn_opp]->X()[isd] - nodes[icn]->X()[isd];
        dist_square += dx * dx;
      }
      double dist = sqrt(dist_square);
      if (dist > max_diameter) max_diameter = dist;
    }

    // diagonally opposite (2-4, 3-5)
    {
      const unsigned icn_opp = icn + ncn_face;
      double dist_square = 0.0;
      for (unsigned int isd = 0; isd < dim; isd++)
      {
        double dx = nodes[icn_opp]->X()[isd] - nodes[icn + 2]->X()[isd];
        dist_square += dx * dx;
      }
      double dist = sqrt(dist_square);
      if (dist > max_diameter) max_diameter = dist;
    }
  }

  // TODO: tets are not yet supported by this framework!
  searchradius_ = searchradius_fac_ * max_diameter;
}

void XFEM::MeshProjector::setup_search_tree()
{
  // init of 3D search tree
  search_tree_ = Teuchos::rcp(new Core::Geo::SearchTree(5));

  // find the bounding box of all elements of source discretization
  const Core::LinAlg::Matrix<3, 2> sourceEleBox =
      Core::Geo::getXAABBofPositions(src_nodepositions_n_);
  search_tree_->initializeTree(sourceEleBox, *sourcedis_, Core::Geo::TreeType(Core::Geo::OCTTREE));

  // TODO: find the bounding box of the nodes from the target discretization, that demand
  // projection, intersect the bounding boxes to obtain a smaller one
}

void XFEM::MeshProjector::Project(std::map<int, std::set<int>>& projection_nodeToDof,
    std::vector<Teuchos::RCP<Epetra_Vector>> target_statevecs,
    Teuchos::RCP<const Epetra_Vector> targetdisp)
{
  // TEUCHOS_FUNC_TIME_MONITOR( "XFEM::MeshProjector::Project" );

  const unsigned num_projection_nodes = projection_nodeToDof.size();
  // size of a fluid dofset
  const unsigned numdofperset = 4;

  targetnode_to_parent_.clear();

  // vector of node ids to be projected
  std::vector<int> projection_targetnodes;
  projection_targetnodes.reserve(num_projection_nodes);

  // target node positions (in sequence of projection_targetnodes)
  std::vector<Core::LinAlg::Matrix<3, 1>> tar_nodepositions_n;
  tar_nodepositions_n.reserve(num_projection_nodes);

  // state vectors veln and accn (in sequence of projection_targetnodes)
  std::vector<Core::LinAlg::Matrix<8, 1>> interpolated_vecs;
  interpolated_vecs.reserve(num_projection_nodes);

  // set position of nodes in target cloud
  for (std::map<int, std::set<int>>::const_iterator i = projection_nodeToDof.begin();
       i != projection_nodeToDof.end(); ++i)
  {
    const Core::Nodes::Node* node = targetdis_->gNode(i->first);

    std::vector<int> tar_dofs(4);
    std::vector<double> mydisp(4, 0.0);

    if (targetdisp != Teuchos::null)
    {
      // get the current displacement
      targetdis_->Dof(node, 0, tar_dofs);
      Core::FE::ExtractMyValues(*targetdisp, mydisp, tar_dofs);
    }

    Core::LinAlg::Matrix<3, 1> pos;
    for (int d = 0; d < 3; ++d)
    {
      pos(d) = node->X()[d] + mydisp.at(d);
    }

    tar_dofs.clear();
    mydisp.clear();

    tar_nodepositions_n.push_back(pos);
    projection_targetnodes.push_back(i->first);
    interpolated_vecs.push_back(Core::LinAlg::Matrix<8, 1>(true));
  }

  setup_search_tree();

  // vector which identifies if a target node has already interpolated values (initialize to false)
  std::vector<int> have_values(projection_targetnodes.size(), 0);
  if (sourcedis_->Comm().NumProc() > 1)
    communicate_nodes(tar_nodepositions_n, interpolated_vecs, projection_targetnodes, have_values);
  else
  {
    find_covering_elements_and_interpolate_values(
        tar_nodepositions_n, interpolated_vecs, projection_targetnodes, have_values);
  }

  for (unsigned ni = 0; ni < projection_targetnodes.size(); ++ni)
  {
    const int node_id = projection_targetnodes[ni];
    const Core::Nodes::Node* node = targetdis_->gNode(node_id);

    if (!have_values.at(ni))
    {
#ifdef FOUR_C_ENABLE_ASSERTIONS
      if (targetdis_->Comm().MyPID() == 0)
        Core::IO::cout << "WARNING: Found no parent for node: " << node_id << Core::IO::endl;
#endif
      continue;
    }

    const std::set<int>& dofsets = projection_nodeToDof.at(node_id);
    int offset = 0;
    for (size_t iv = 0; iv < target_statevecs.size(); ++iv)
    {
      if (target_statevecs[iv] == Teuchos::null) continue;

      std::vector<int> dofs;
      dofs.reserve(dofsets.size() * numdofperset);

      for (std::set<int>::const_iterator iset = dofsets.begin(); iset != dofsets.end(); ++iset)
      {
        targetdis_->Dof(dofs, node, 0, *iset);

        for (unsigned isd = 0; isd < numdofperset; ++isd)
        {
          (*target_statevecs[iv])[target_statevecs[iv]->Map().LID(dofs[isd])] =
              interpolated_vecs[ni](isd + offset);
        }
        dofs.clear();
      }
      offset += numdofperset;
    }
    // if projection was successful, remove the node from the projection map
    projection_nodeToDof.erase(node_id);
  }
}

void XFEM::MeshProjector::project_in_full_target_discretization(
    std::vector<Teuchos::RCP<Epetra_Vector>> target_statevecs,
    Teuchos::RCP<const Epetra_Vector> targetdisp)
{
  // this routine supports only non-XFEM discretizations!
  Teuchos::RCP<const XFEM::DiscretizationXFEM> xdiscret =
      Teuchos::rcp_dynamic_cast<const XFEM::DiscretizationXFEM>(targetdis_);
  if (xdiscret != Teuchos::null)
    FOUR_C_THROW(
        "Value projection for between different mesh deformation states does not support "
        "DiscretizationXFEM.");
  std::map<int, std::set<int>> projection_nodeToDof;
  for (int ni = 0; ni < targetdis_->NumMyRowNodes(); ++ni)
  {
    const Core::Nodes::Node* node = targetdis_->lRowNode(ni);
    // set of dofset indices
    std::set<int> dofsets;
    dofsets.insert(0);

    projection_nodeToDof[node->Id()] = dofsets;
  }

  Project(projection_nodeToDof, target_statevecs, targetdisp);
}

template <Core::FE::CellType distype>
bool XFEM::MeshProjector::check_position_and_project(const Core::Elements::Element* src_ele,
    const Core::LinAlg::Matrix<3, 1>& node_xyz, Core::LinAlg::Matrix<8, 1>& interpolatedvec)
{
  // number of element's nodes
  const unsigned int src_numnodes = Core::FE::num_nodes<distype>;
  // nodal coordinates
  Core::LinAlg::Matrix<3, src_numnodes> src_xyze(true);

  for (int in = 0; in < src_ele->num_node(); ++in)
  {
    const unsigned nid = src_ele->NodeIds()[in];

    for (int d = 0; d < 3; ++d)
    {
      src_xyze(d, in) = src_nodepositions_n_.at(nid)(d);
    }
  }

  // compute node position w.r.t. embedded element
  Teuchos::RCP<Core::Geo::Cut::Position> pos =
      Core::Geo::Cut::PositionFactory::build_position<3, distype>(src_xyze, node_xyz);
  bool inside = pos->Compute();

  if (inside)
  {
    // node position in covering element's local coordinates
    Core::LinAlg::Matrix<3, 1> xsi;
    pos->local_coordinates(xsi);

    // Evaluate elements shape function at this point and fill values
    Core::LinAlg::SerialDenseVector shp(src_numnodes);
    Core::FE::shape_function_3D(shp, xsi(0, 0), xsi(1, 0), xsi(2, 0), distype);

    // extract state values and interpolate
    for (int in = 0; in < src_ele->num_node(); ++in)
    {
      const Core::Nodes::Node* node = src_ele->Nodes()[in];
      const unsigned numdofpernode = src_ele->NumDofPerNode(*node);

      std::vector<double> myval(numdofpernode);
      std::vector<int> src_dofs(numdofpernode);

      sourcedis_->Dof(node, 0, src_dofs);
      unsigned offset = 0;
      for (size_t iv = 0; iv < source_statevecs_.size(); ++iv)
      {
        if (source_statevecs_[iv] == Teuchos::null) continue;

        Core::FE::ExtractMyValues(*source_statevecs_[iv], myval, src_dofs);
        for (unsigned isd = 0; isd < numdofpernode; ++isd)
        {
          interpolatedvec(isd + offset) += myval[isd] * shp(in);
        }

        offset += myval.size();

        myval.clear();
      }

      src_dofs.clear();
    }
  }

  return inside;
}

void XFEM::MeshProjector::find_covering_elements_and_interpolate_values(
    std::vector<Core::LinAlg::Matrix<3, 1>>& tar_nodepositions,
    std::vector<Core::LinAlg::Matrix<8, 1>>& interpolated_vecs,
    std::vector<int>& projection_targetnodes, std::vector<int>& have_values)
{
  // loop over the nodes (coordinates)
  for (unsigned int ni = 0; ni < projection_targetnodes.size(); ++ni)
  {
    bool insideelement = false;

    // node coordinate
    const Core::LinAlg::Matrix<3, 1>& node_xyz = tar_nodepositions.at(ni);
    // interpolated vector which is zero at the beginning
    Core::LinAlg::Matrix<8, 1> interpolatedvec(true);

    // search for near elements
    std::map<int, std::set<int>> closeeles = search_tree_->search_elements_in_radius(
        *sourcedis_, src_nodepositions_n_, node_xyz, searchradius_, 0);

    if (closeeles.empty())
    {
      continue;
    }

    // loop over the map of target node-IDs and source elements within the search radius
    for (std::map<int, std::set<int>>::const_iterator closele = closeeles.begin();
         closele != closeeles.end(); closele++)
    {
      // loop over the set of source elements within the search radius
      for (std::set<int>::const_iterator eleIter = (closele->second).begin();
           eleIter != (closele->second).end(); eleIter++)
      {
        Core::Elements::Element* pele = sourcedis_->gElement(*eleIter);
        // determine values for target fluid node
        switch (pele->Shape())
        {
          case Core::FE::CellType::hex8:
            insideelement = check_position_and_project<Core::FE::CellType::hex8>(
                pele, node_xyz, interpolatedvec);
            break;
          case Core::FE::CellType::hex20:
            insideelement = check_position_and_project<Core::FE::CellType::hex20>(
                pele, node_xyz, interpolatedvec);
            break;
          case Core::FE::CellType::hex27:
            insideelement = check_position_and_project<Core::FE::CellType::hex27>(
                pele, node_xyz, interpolatedvec);
            break;
          default:
            FOUR_C_THROW(
                "Unsupported element shape %s!", Core::FE::CellTypeToString(pele->Shape()).c_str());
            break;
        }

        if (insideelement)
        {
          targetnode_to_parent_[projection_targetnodes[ni]] = pele->Id();
          break;
        }
      }
      if (insideelement)
      {
        if (have_values.at(ni) == 0)
        {
          have_values[ni] = 1;
          interpolated_vecs.at(ni) = interpolatedvec;
        }
        break;
      }
    }
  }

  return;
}

void XFEM::MeshProjector::communicate_nodes(
    std::vector<Core::LinAlg::Matrix<3, 1>>& tar_nodepositions,
    std::vector<Core::LinAlg::Matrix<8, 1>>& interpolated_vecs,
    std::vector<int>& projection_targetnodes, std::vector<int>& have_values)
{
  // get number of processors and the current processors id
  const int numproc = sourcedis_->Comm().NumProc();

  // information how many processors work at all
  std::vector<int> allproc(numproc);

  // create an exporter for point to point comunication
  Core::Communication::Exporter exporter(sourcedis_->Comm());

  // necessary variables
  MPI_Request request;

  // define send and receive blocks
  std::vector<char> sblock;
  std::vector<char> rblock;

  //----------------------------------------------------------------------
  // communication is done in a round robin loop
  //----------------------------------------------------------------------
  for (int np = 0; np < numproc + 1; ++np)
  {
    // in the first step, we cannot receive anything
    if (np > 0)
    {
      receive_block(rblock, exporter, request);

      std::vector<char>::size_type position = 0;
      Core::Communication::ParObject::extract_from_pack(position, rblock, tar_nodepositions);
      Core::Communication::ParObject::extract_from_pack(position, rblock, interpolated_vecs);
      Core::Communication::ParObject::extract_from_pack(position, rblock, projection_targetnodes);
      Core::Communication::ParObject::extract_from_pack(position, rblock, have_values);
    }

    // in the last step, we keep everything on this proc
    if (np < numproc)
    {
      // -----------------------
      // do what we wanted to do
      find_covering_elements_and_interpolate_values(
          tar_nodepositions, interpolated_vecs, projection_targetnodes, have_values);

      // Pack info into block to send it
      pack_values(
          tar_nodepositions, interpolated_vecs, projection_targetnodes, have_values, sblock);

      // add size to sendblock
      send_block(sblock, exporter, request);
    }
  }  // end of loop over processors
}

void XFEM::MeshProjector::receive_block(
    std::vector<char>& rblock, Core::Communication::Exporter& exporter, MPI_Request& request)
{
  // get number of processors and the current processors id
  int numproc = sourcedis_->Comm().NumProc();
  int myrank = sourcedis_->Comm().MyPID();

  // necessary variables
  int length = -1;
  int frompid = (myrank + numproc - 1) % numproc;
  int tag = frompid;

  // receive from predecessor
  exporter.ReceiveAny(frompid, tag, rblock, length);

#ifdef FOUR_C_ENABLE_ASSERTIONS
  // Core::IO::cout << "----receiving " << rblock.size() <<  " bytes: to proc " << myrank << " from
  // proc "
  // << frompid << Core::IO::endl;
#endif

  if (tag != (myrank + numproc - 1) % numproc)
  {
    FOUR_C_THROW("received wrong message (ReceiveAny)");
  }

  exporter.Wait(request);

  // for safety
  exporter.Comm().Barrier();

  return;
}

void XFEM::MeshProjector::send_block(
    std::vector<char>& sblock, Core::Communication::Exporter& exporter, MPI_Request& request)
{
  // get number of processors and the current processors id
  int numproc = sourcedis_->Comm().NumProc();
  int myrank = sourcedis_->Comm().MyPID();

  // Send block to next proc.
  int tag = myrank;
  int frompid = myrank;
  int topid = (myrank + 1) % numproc;

#ifdef FOUR_C_ENABLE_ASSERTIONS
  // Core::IO::cout << "----sending " << sblock.size() <<  " bytes: from proc " << myrank << " to
  // proc "
  // << topid << Core::IO::endl;
#endif

  exporter.i_send(frompid, topid, sblock.data(), sblock.size(), tag, request);

  // for safety
  exporter.Comm().Barrier();

  return;
}

void XFEM::MeshProjector::pack_values(std::vector<Core::LinAlg::Matrix<3, 1>>& tar_nodepositions,
    std::vector<Core::LinAlg::Matrix<8, 1>>& interpolated_vecs,
    std::vector<int>& projection_targetnodes, std::vector<int>& have_values,
    std::vector<char>& sblock)
{
  // Pack info into block to send
  Core::Communication::PackBuffer data;
  Core::Communication::ParObject::add_to_pack(data, tar_nodepositions);
  Core::Communication::ParObject::add_to_pack(data, interpolated_vecs);
  Core::Communication::ParObject::add_to_pack(data, projection_targetnodes);
  Core::Communication::ParObject::add_to_pack(data, have_values);
  swap(sblock, data());
}

void XFEM::MeshProjector::GmshOutput(int step, Teuchos::RCP<const Epetra_Vector> targetdisp)
{
  // output of source discretization with element numbers and target nodes together with element id
  // of source element for value projection
  const std::string filename = Core::IO::Gmsh::GetNewFileNameAndDeleteOldFiles("tarnode_to_src_ele",
      targetdis_->Writer()->output()->file_name(), step, 30, 0, targetdis_->Comm().MyPID());
  std::ofstream gmshfilecontent(filename.c_str());
  {
    XFEM::UTILS::PrintDiscretizationToStream(
        Teuchos::rcp_const_cast<Core::FE::Discretization>(sourcedis_), sourcedis_->Name(), true,
        false, false, false, false, false, gmshfilecontent, &src_nodepositions_n_);

    gmshfilecontent << "View \" "
                    << "nodeToEle n\" {\n";

    std::vector<int> tar_dofs(3);
    std::vector<double> mydisp(3, 0.0);
    for (int i = 0; i < targetdis_->NumMyColNodes(); ++i)
    {
      const Core::Nodes::Node* actnode = targetdis_->lColNode(i);
      Core::LinAlg::Matrix<3, 1> pos(actnode->X().data(), false);
      if (targetdisp != Teuchos::null)
      {
        // get the current displacement
        targetdis_->Dof(actnode, 0, tar_dofs);
        Core::FE::ExtractMyValues(*targetdisp, mydisp, tar_dofs);
        for (unsigned isd = 0; isd < 3; ++isd)
        {
          pos(isd, 0) += mydisp[isd];
        }
        mydisp.clear();
      }
      tar_dofs.clear();

      std::map<int, int>::const_iterator iter = targetnode_to_parent_.find(actnode->Id());

      if (iter != targetnode_to_parent_.end())
      {
        int id = iter->second;
        Core::IO::Gmsh::ScalarToStream(pos, id, gmshfilecontent);
      }
    }
    gmshfilecontent << "};\n";
  }
}

FOUR_C_NAMESPACE_CLOSE
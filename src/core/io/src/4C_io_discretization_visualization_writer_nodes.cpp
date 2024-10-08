/*-----------------------------------------------------------------------------------------------*/
/*! \file

\brief Write nodal coordinates and values at the nodes to a visualization file

\level 3

*/
/*-----------------------------------------------------------------------------------------------*/

/* headers */
#include "4C_io_discretization_visualization_writer_nodes.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_fem_general_node.hpp"
#include "4C_io_visualization_manager.hpp"
#include "4C_utils_exceptions.hpp"

#include <utility>

FOUR_C_NAMESPACE_OPEN


namespace Core::IO
{
  /*-----------------------------------------------------------------------------------------------*
   *-----------------------------------------------------------------------------------------------*/
  DiscretizationVisualizationWriterNodes::DiscretizationVisualizationWriterNodes(
      const Teuchos::RCP<const Core::FE::Discretization>& discretization,
      VisualizationParameters parameters)
      : discretization_(discretization),
        visualization_manager_(Teuchos::make_rcp<Core::IO::VisualizationManager>(
            std::move(parameters), discretization->get_comm(), discretization->name()))
  {
  }


  /*-----------------------------------------------------------------------------------------------*
   *-----------------------------------------------------------------------------------------------*/
  void DiscretizationVisualizationWriterNodes::set_geometry_from_discretization()
  {
    // Todo assume 3D for now
    const unsigned int num_spatial_dimensions = 3;

    // count number of nodes and number for each processor; output is completely independent of
    // the number of processors involved
    unsigned int num_row_nodes = (unsigned int)discretization_->num_my_row_nodes();

    // get and prepare storage for point coordinate values
    std::vector<double>& point_coordinates =
        visualization_manager_->get_visualization_data().get_point_coordinates();
    point_coordinates.clear();
    point_coordinates.reserve(num_spatial_dimensions * num_row_nodes);


    // loop over my nodes and collect the geometry/grid data, i.e. reference positions of nodes
    for (const Core::Nodes::Node* node : discretization_->my_row_node_range())
    {
      for (unsigned int idim = 0; idim < num_spatial_dimensions; ++idim)
      {
        point_coordinates.push_back(node->x()[idim]);
      }
    }


    // safety check
    if (point_coordinates.size() != num_spatial_dimensions * num_row_nodes)
    {
      FOUR_C_THROW(
          "DiscretizationVisualizationWriterNodes expected %d coordinate values, but got %d",
          num_spatial_dimensions * num_row_nodes, point_coordinates.size());
    }
  }

  /*-----------------------------------------------------------------------------------------------*
   *-----------------------------------------------------------------------------------------------*/
  void DiscretizationVisualizationWriterNodes::append_dof_based_result_data_vector(
      const Teuchos::RCP<Core::LinAlg::Vector<double>>& result_data_dofbased,
      unsigned int result_num_dofs_per_node, const std::string& resultname)
  {
    /* the idea is to transform the given data to a 'point data vector' and append it to the
     * collected solution data vectors by calling AppendVisualizationPointDataVector() */

    std::vector<double> point_result_data;
    point_result_data.reserve(result_data_dofbased->MyLength());

    for (int lid = 0; lid < result_data_dofbased->MyLength(); ++lid)
      point_result_data.push_back((*result_data_dofbased)[lid]);

    visualization_manager_->get_visualization_data().set_point_data_vector<double>(
        resultname, point_result_data, result_num_dofs_per_node);
  }

  /*-----------------------------------------------------------------------------------------------*
   *-----------------------------------------------------------------------------------------------*/
  void DiscretizationVisualizationWriterNodes::append_node_based_result_data_vector(
      const Teuchos::RCP<Epetra_MultiVector>& result_data_nodebased,
      unsigned int result_num_components_per_node, const std::string& resultname)
  {
    /*  the idea is to transform the given data to a 'point data vector' and append it to the
     *  collected solution data vectors by calling append_visualization_point_data_vector()
     */

    // count number of nodes for each processor
    const unsigned int num_row_nodes = (unsigned int)result_data_nodebased->Map().NumMyElements();

    // safety check
    if ((unsigned int)result_data_nodebased->NumVectors() != result_num_components_per_node)
      FOUR_C_THROW(
          "DiscretizationVisualizationWriterNodes: expected Epetra_MultiVector with %d columns but "
          "got %d",
          result_num_components_per_node, result_data_nodebased->NumVectors());


    std::vector<double> point_result_data;
    point_result_data.reserve(result_num_components_per_node * num_row_nodes);

    for (unsigned int lid = 0; lid < num_row_nodes; ++lid)
    {
      for (unsigned int idf = 0; idf < result_num_components_per_node; ++idf)
      {
        auto* column = (*result_data_nodebased)(idf);
        point_result_data.push_back((*column)[lid]);
      }
    }

    visualization_manager_->get_visualization_data().set_point_data_vector(
        resultname, point_result_data, result_num_components_per_node);
  }

  /*-----------------------------------------------------------------------------------------------*
   *-----------------------------------------------------------------------------------------------*/
  void DiscretizationVisualizationWriterNodes::write_to_disk(
      const double visualization_time, const int visualization_step)
  {
    visualization_manager_->write_to_disk(visualization_time, visualization_step);
  }
}  // namespace Core::IO
FOUR_C_NAMESPACE_CLOSE

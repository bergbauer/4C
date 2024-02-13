/*----------------------------------------------------------------------*/
/*! \file

\brief input parameters for mortar methods

\level 1


*/
/*----------------------------------------------------------------------*/
#ifndef BACI_INPAR_MORTAR_HPP
#define BACI_INPAR_MORTAR_HPP

#include "baci_config.hpp"

#include "baci_inpar_parameterlist_utils.hpp"

BACI_NAMESPACE_OPEN

// forward declaration
namespace INPUT
{
  class ConditionDefinition;
}  // namespace INPUT

/*----------------------------------------------------------------------*/
namespace INPAR
{
  namespace MORTAR
  {
    /// Type of employed set of Lagrange multiplier shape functions
    /// (this enum represents the input file parameter LM_SHAPEFCN)
    enum ShapeFcn
    {
      shape_undefined,       ///< undefined
      shape_standard,        ///< standard shape functions
      shape_dual,            ///< dual shape functions
      shape_petrovgalerkin,  ///< Petrov-Galerkin approach
      shape_none             ///< for all methods w/o Lagrange multiplier interpolation
    };

    /// Type of Lagrange multiplier interpolation for quadratic FE case
    /// (this enum represents the input file parameter LM_QUADRATIC)
    enum LagMultQuad
    {
      lagmult_undefined,  ///< undefined
      lagmult_quad,       ///< quadratic interpolation
      lagmult_pwlin,      ///< piecewise linear interpolation
      lagmult_lin,        ///< linear interpolation
      lagmult_const       ///< element-wise constant interpolation (only for quadratic FE)
    };

    /// Type of mortar coupling search algorithm
    /// (this enum represents the input file parameter SEARCH_ALGORITHM)
    enum SearchAlgorithm
    {
      search_bfele,      ///< brute force element-based
      search_binarytree  ///< binary tree element based
    };

    /// Local definition of problemtype to avoid use of globalproblem.H
    enum Problemtype
    {
      poroelast,   ///< poroelasticity problem with mortar
      poroscatra,  ///< poroscatra problem with mortar
      other        ///< other problemtypes
    };

    /// Type of binary tree update
    /// (this enum represents the input file parameter BINARYTREE_UPDATETYPE)
    enum BinaryTreeUpdateType
    {
      binarytree_bottom_up,  ///< indicates a bottom-up update of binary tree
      binarytree_top_down    ///< indicates a top-down update of binary tree
    };

    /// Type of mesh relocation
    /// (this enum represents the input file parameter MESH_RELOCATION)
    enum MeshRelocation
    {
      relocation_initial,   ///< only initial mesh relocation
      relocation_timestep,  ///< mesh relocation in every time step, but no initial mesh relocation
      relocation_none       ///< no mesh relocation
    };

    /// Type of ghosting of interface values
    /// (this enum represents the input file parameter GHOSTING_STRATEGY)
    enum class ExtendGhosting
    {
      redundant_all,     ///< Store all master & slave surfaces redundantly on all MPI ranks
      redundant_master,  ///< Store all master surfaces redundantly on all MPI ranks
      roundrobin,        ///< Extend master-sided ghosting via Round-Robin loop
      binning            ///< Extend master-sided ghosting via binning
    };

    /// Type of meshtying/contact algorithm
    /// (this enum represents the input file parameter ALGORITHM)
    enum AlgorithmType
    {
      algorithm_mortar,  ///< mortar algorithm (segment-to-segment)
      algorithm_nts,     ///< node-to-segment algorithm
      algorithm_gpts,    ///< gp-to-segment algorithm
      algorithm_lts,     ///< line-to-segment algorithm
      algorithm_ltl,     ///< line-to-segment algorithm
      algorithm_ntl,     ///< node-to-line algorithm (coming soon...)
      algorithm_stl      ///< segment-to-line algorithm
    };

    /// Type of parallel redistribution algorithm
    /// (this enum represents the input file parameter PARALLEL_REDIST)
    enum class ParallelRedist
    {
      redist_none,    ///< no redistribution
      redist_static,  ///< static redistribution (at t=0 and after restart)
      redist_dynamic  ///< dynamic redistribution
    };

    /// Type of integration procedure
    /// (this enum represents the input file parameter INTTYPE)
    enum IntType
    {
      inttype_segments,    ///< segmentation of mortar interface
      inttype_elements,    ///< fast, elementwise integration
      inttype_elements_BS  ///< fast, elementwise integration with boundary segmentation
    };

    /// Type of triangulation for segment-base d integration
    /// (this enum represents the input file parameter TRIANGULATION)
    enum Triangulation
    {
      triangulation_center,   ///< simpler center-based triangulation (see e.g. Popp et al. 2010)
      triangulation_delaunay  ///< delaunay triangulation
    };

    /// Determining, on which quadrature points biorthogonality is enforced
    enum ConsistentDualType
    {
      consistent_none,      ///< always use element GP (fastest option)
      consistent_boundary,  ///< use triangulation GPs only in partially integrated elements
      consistent_all,       ///< use triangulation GPs for all elements
    };

    /// Enum to encode handling of Dirichlet boundary conditions at contact interfaces
    enum class DBCHandling : int
    {
      do_nothing,
      remove_dbc_nodes_from_slave_side  // ToDo (mayr.mt) Remove? Do not change DBCs at runtime!
    };

    /// set the mortar parameters
    void SetValidParameters(Teuchos::RCP<Teuchos::ParameterList> list);

    /// set specific mortar conditions
    void SetValidConditions(std::vector<Teuchos::RCP<INPUT::ConditionDefinition>>& condlist);

  }  // namespace MORTAR

}  // namespace INPAR

/*----------------------------------------------------------------------*/
BACI_NAMESPACE_CLOSE

#endif  // INPAR_MORTAR_H
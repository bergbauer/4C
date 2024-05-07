/*-----------------------------------------------------------*/
/*! \file

\brief input parameter for rebalancing the discretization

\level 2

*/
/*-----------------------------------------------------------*/

#include "4C_inpar_rebalance.hpp"

#include "4C_rebalance.hpp"
#include "4C_utils_parameter_list.hpp"

FOUR_C_NAMESPACE_OPEN

void INPAR::REBALANCE::SetValidParameters(Teuchos::RCP<Teuchos::ParameterList> list)
{
  using Teuchos::setStringToIntegralParameter;
  using Teuchos::tuple;

  Teuchos::ParameterList& meshpartitioning = list->sublist("MESH PARTITIONING", false, "");

  setStringToIntegralParameter<CORE::REBALANCE::RebalanceType>("METHOD", "hypergraph",
      "Type of rebalance/partition algorithm to be used for decomposing the entire mesh into "
      "subdomains for parallel computing.",
      tuple<std::string>("none", "hypergraph", "recursive_coordinate_bisection", "monolithic"),
      tuple<CORE::REBALANCE::RebalanceType>(CORE::REBALANCE::RebalanceType::none,
          CORE::REBALANCE::RebalanceType::hypergraph,
          CORE::REBALANCE::RebalanceType::recursive_coordinate_bisection,
          CORE::REBALANCE::RebalanceType::monolithic),
      &meshpartitioning);

  CORE::UTILS::DoubleParameter("IMBALANCE_TOL", 1.1,
      "Tolerance for relative imbalance of subdomain sizes for graph partitioning of unstructured "
      "meshes read from input files.",
      &meshpartitioning);
}

FOUR_C_NAMESPACE_CLOSE

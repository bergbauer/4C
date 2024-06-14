/*-----------------------------------------------------------*/
/*! \file

\brief input parameter for binning strategy


\level 2

*/
/*-----------------------------------------------------------*/


#include "4C_inpar_binningstrategy.hpp"

#include "4C_utils_parameter_list.hpp"

FOUR_C_NAMESPACE_OPEN



void Inpar::BINSTRATEGY::SetValidParameters(Teuchos::RCP<Teuchos::ParameterList> list)
{
  using Teuchos::setStringToIntegralParameter;
  using Teuchos::tuple;

  Teuchos::ParameterList& binningstrategy = list->sublist("BINNING STRATEGY", false, "");


  Core::UTILS::DoubleParameter("BIN_SIZE_LOWER_BOUND", -1.0,
      "Lower bound for bin size. Exact bin size is computed via (Domain edge "
      "length)/BIN_SIZE_LOWER_BOUND. This also determines the number of bins in each spatial "
      "direction",
      &binningstrategy);

  Core::UTILS::StringParameter("BIN_PER_DIR", "-1 -1 -1",
      "Number of bins per direction (x, y, z) in particle simulations. Either Define this value or "
      "BIN_SIZE_LOWER_BOUND",
      &binningstrategy);

  Core::UTILS::StringParameter("PERIODICONOFF", "0 0 0",
      "Turn on/off periodic boundary conditions in each spatial direction", &binningstrategy);

  Core::UTILS::StringParameter("DOMAINBOUNDINGBOX", "1e12 1e12 1e12 1e12 1e12 1e12",
      "Bounding box for computational domain using binning strategy. Specify diagonal corner "
      "points",
      &binningstrategy);

  setStringToIntegralParameter<int>("WRITEBINS", "none",
      "Write none, row or column bins for visualization",
      tuple<std::string>("none", "rows", "cols"), tuple<int>(none, rows, cols), &binningstrategy);
}

FOUR_C_NAMESPACE_CLOSE
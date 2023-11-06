/*----------------------------------------------------------------------------*/
/*! \file

\brief one beam contact segment living on an element pair

\level 3

*/
/*----------------------------------------------------------------------------*/

#include "baci_beaminteraction_beam_to_beam_contact_variables.H"

#include "baci_beam3_euler_bernoulli.H"
#include "baci_beam3_reissner.H"
#include "baci_beaminteraction_beam_to_beam_contact_defines.H"
#include "baci_beaminteraction_beam_to_beam_contact_pair.H"
#include "baci_beaminteraction_beam_to_beam_contact_utils.H"
#include "baci_discretization_fem_general_utils_fem_shapefunctions.H"
#include "baci_inpar_beamcontact.H"
#include "baci_inpar_contact.H"
#include "baci_lib_discret.H"
#include "baci_lib_exporter.H"
#include "baci_lib_globalproblem.H"
#include "baci_linalg_utils_sparse_algebra_math.H"
#include "baci_structure_timint_impl.H"
#include "baci_utils_exceptions.H"

#include <Teuchos_TimeMonitor.hpp>

/*----------------------------------------------------------------------*
 |  constructor (public)                                     meier 01/14|
 *----------------------------------------------------------------------*/
template <unsigned int numnodes, unsigned int numnodalvalues>
BEAMINTERACTION::BeamToBeamContactVariables<numnodes, numnodalvalues>::BeamToBeamContactVariables(
    std::pair<TYPE, TYPE>& closestpoint, std::pair<int, int>& segids, std::pair<int, int>& intids,
    const double& pp, TYPE jacobi)
    : closestpoint_(closestpoint),
      segids_(segids),
      intids_(intids),
      jacobi_(jacobi),
      gap_(0.0),
      normal_(CORE::LINALG::Matrix<3, 1, TYPE>(true)),
      pp_(pp),
      ppfac_(0.0),
      dppfac_(0.0),
      fp_(0.0),
      dfp_(0.0),
      energy_(0.0),
      integratedenergy_(0.0),
      angle_(0.0)
{
  return;
}
/*----------------------------------------------------------------------*
 |  end: constructor
 *----------------------------------------------------------------------*/

// Possible template cases: this is necessary for the compiler
template class BEAMINTERACTION::BeamToBeamContactVariables<2, 1>;
template class BEAMINTERACTION::BeamToBeamContactVariables<3, 1>;
template class BEAMINTERACTION::BeamToBeamContactVariables<4, 1>;
template class BEAMINTERACTION::BeamToBeamContactVariables<5, 1>;
template class BEAMINTERACTION::BeamToBeamContactVariables<2, 2>;
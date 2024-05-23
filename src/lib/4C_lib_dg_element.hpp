/*----------------------------------------------------------------------------*/
/*! \file
    \brief virtual DG Element interface for HDG discretization method

    \level 3
 */
/*----------------------------------------------------------------------------*/

#ifndef FOUR_C_LIB_DG_ELEMENT_HPP
#define FOUR_C_LIB_DG_ELEMENT_HPP

#include "4C_config.hpp"

FOUR_C_NAMESPACE_OPEN

namespace DRT
{
  class DgElement
  {
   public:
    /*!
     \brief Destructor
    */
    virtual ~DgElement() = default;
    virtual int num_dof_per_node_auxiliary() const = 0;

    virtual int num_dof_per_element_auxiliary() const = 0;
  };
}  // namespace DRT

FOUR_C_NAMESPACE_CLOSE

#endif

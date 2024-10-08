/*----------------------------------------------------------------------*/
/*! \file

 \brief Managing and evaluating of spatial functions for combustion and two-phase flow problems

\level 2

 *------------------------------------------------------------------------------------------------*/

#include "4C_fluid_xfluid_functions_combust.hpp"

#include "4C_io_linedefinition.hpp"
#include "4C_utils_function_manager.hpp"

FOUR_C_NAMESPACE_OPEN

namespace
{
  Teuchos::RCP<Core::UTILS::FunctionOfSpaceTime> create_combust_function(
      const std::vector<Input::LineDefinition>& function_line_defs)
  {
    if (function_line_defs.size() != 1) return Teuchos::null;

    if (function_line_defs.front().container().get_or("ZALESAKSDISK", false))
    {
      return Teuchos::make_rcp<Discret::UTILS::ZalesaksDiskFunction>();
    }
    else if (function_line_defs.front().container().get_or("COLLAPSINGWATERCOLUMN", false))
    {
      return Teuchos::make_rcp<Discret::UTILS::CollapsingWaterColumnFunction>();
    }
    else
    {
      return Teuchos::null;
    }
  }
}  // namespace

void Discret::UTILS::add_valid_combust_functions(Core::UTILS::FunctionManager& function_manager)
{
  Input::LineDefinition zalesaksdisk =
      Input::LineDefinition::Builder().add_tag("ZALESAKSDISK").build();

  Input::LineDefinition collapsingwatercolumn =
      Input::LineDefinition::Builder().add_tag("COLLAPSINGWATERCOLUMN").build();

  function_manager.add_function_definition(
      {std::move(zalesaksdisk), std::move(collapsingwatercolumn)}, create_combust_function);
}



double Discret::UTILS::ZalesaksDiskFunction::evaluate(
    const double* xp, const double t, const std::size_t component) const
{
  // the disk consists of 3 lines and a part of a circle and four points
  // decide if the orthogonal projection of the current point lies on the lines and the circle (four
  // different distances possible) additionally the smallest distance can be between the current
  // point and one of the four corners

  double distance = 99999.0;

  //=====================================
  // distances to the four corners
  //=====================================
  // upper points
  double y_upper = std::sqrt(((0.15) * (0.15)) - ((0.025) * (0.025))) + 0.25;
  // warning: sign must be positive
  double dist_lu =
      std::sqrt(((xp[0] + 0.025) * (xp[0] + 0.025)) + ((xp[1] - y_upper) * (xp[1] - y_upper)));
  if (std::abs(dist_lu) < std::abs(distance)) distance = dist_lu;

  // warning: sign must be positive
  double dist_ru =
      std::sqrt(((xp[0] - 0.025) * (xp[0] - 0.025)) + ((xp[1] - y_upper) * (xp[1] - y_upper)));
  if (std::abs(dist_ru) < std::abs(distance)) distance = dist_ru;

  // under points
  double y_down = 0.15;
  // warning: sign must be negative
  double dist_ld =
      std::sqrt(((xp[0] + 0.025) * (xp[0] + 0.025)) + ((xp[1] - y_down) * (xp[1] - y_down)));
  if (std::abs(dist_ld) < std::abs(distance)) distance = -dist_ld;

  // warning: sign must be negative
  double dist_rd =
      std::sqrt(((xp[0] - 0.025) * (xp[0] - 0.025)) + ((xp[1] - y_down) * (xp[1] - y_down)));
  if (std::abs(dist_rd) < std::abs(distance)) distance = -dist_rd;

  //=====================================
  // projection on the 3 lines
  //=====================================
  // decide for vertical lines
  if (xp[1] >= 0.15 && xp[1] <= y_upper)
  {
    // leftVertLine
    if (std::abs(xp[0] + 0.025) < std::abs(distance)) distance = xp[0] + 0.025;

    // rightVertLine
    if (std::abs(0.025 - xp[0]) < std::abs(distance)) distance = 0.025 - xp[0];
  }
  // decide for horizontal line
  if (xp[0] >= -0.025 && xp[0] <= 0.025)
  {
    // horizontalLine
    if (std::abs(xp[1] - 0.15) < std::abs(distance)) distance = xp[1] - 0.15;
  }

  //======================================
  // distance to the circle
  //======================================
  // decide for part of circle
  // get radius of sphere for current point
  double s = std::sqrt(((xp[0] - 0.0) * (xp[0] - 0.0)) + ((xp[1] - 0.25) * (xp[1] - 0.25)));
  // get angle between line form midpoint of circle to current point and vector (0,1,0)
  double y_tmp = std::sqrt(((0.15) * (0.15)) - ((0.025) * (0.025))) * s / 0.15;
  if ((xp[1] - 0.25) <= y_tmp)
  {
    if (std::abs(s - 0.15) < std::abs(distance)) distance = s - 0.15;
  }

  return distance;
}


double Discret::UTILS::CollapsingWaterColumnFunction::evaluate(
    const double* xp, const double t, const std::size_t component) const
{
  // here calculation of distance (sign is already taken in consideration)
  double distance = 0.0;

  double xp_corner[2];
  double xp_center[2];
  double radius = 0.0;  // 0.03;

  xp_corner[0] = 0.146;  // 0.144859;
  xp_corner[1] = 0.292;  // 0.290859;
  xp_center[0] = xp_corner[0] - radius;
  xp_center[1] = xp_corner[1] - radius;


  if (xp[0] <= xp_center[0] and xp[1] >= xp_center[1])
  {
    distance = xp[1] - xp_corner[1];
  }
  else if (xp[0] >= xp_center[0] and xp[1] <= xp_center[1] and
           !(xp[0] == xp_center[0] and xp[1] == xp_center[1]))
  {
    distance = xp[0] - xp_corner[0];
  }
  else if (xp[0] < xp_center[0] and xp[1] < xp_center[1])
  {
    if (xp[1] > (xp_corner[1] + (xp[0] - xp_corner[0])))
    {
      distance = -fabs(xp_corner[1] - xp[1]);
    }
    else
    {
      distance = -fabs(xp_corner[0] - xp[0]);
    }
  }
  else
  {
    distance = sqrt(((xp[0] - xp_center[0]) * (xp[0] - xp_center[0])) +
                    ((xp[1] - xp_center[1]) * (xp[1] - xp_center[1]))) -
               radius;
  }

  return distance;
}

FOUR_C_NAMESPACE_CLOSE

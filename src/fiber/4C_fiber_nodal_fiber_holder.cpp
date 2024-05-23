/*----------------------------------------------------------------------*/
/*! \file

\brief This file implements a class that holds different nodal fibers

\level 3


*/
/*----------------------------------------------------------------------*/

#include "4C_fiber_nodal_fiber_holder.hpp"

#include "4C_comm_parobject.hpp"
#include "4C_fiber_node.hpp"

FOUR_C_NAMESPACE_OPEN

void DRT::FIBER::NodalFiberHolder::set_coordinate_system_direction(
    DRT::FIBER::CoordinateSystemDirection type,
    const std::vector<CORE::LINALG::Matrix<3, 1>>& fiber)
{
  coordinate_system_directions_.insert(std::pair<DRT::FIBER::CoordinateSystemDirection,
      const std::vector<CORE::LINALG::Matrix<3, 1>>>(type, fiber));
}

const std::vector<CORE::LINALG::Matrix<3, 1>>&
DRT::FIBER::NodalFiberHolder::get_coordinate_system_direction(
    DRT::FIBER::CoordinateSystemDirection type) const
{
  return coordinate_system_directions_.at(type);
}

std::vector<CORE::LINALG::Matrix<3, 1>>&
DRT::FIBER::NodalFiberHolder::get_coordinate_system_direction_mutual(
    DRT::FIBER::CoordinateSystemDirection type)
{
  return coordinate_system_directions_.at(type);
}

void DRT::FIBER::NodalFiberHolder::AddFiber(const std::vector<CORE::LINALG::Matrix<3, 1>>& fiber)
{
  fibers_.emplace_back(fiber);
}

const std::vector<CORE::LINALG::Matrix<3, 1>>& DRT::FIBER::NodalFiberHolder::GetFiber(
    std::size_t fiberid) const
{
  return fibers_.at(fiberid);
}

std::vector<CORE::LINALG::Matrix<3, 1>>& DRT::FIBER::NodalFiberHolder::GetFiberMutual(
    std::size_t fiberid)
{
  return fibers_.at(fiberid);
}

void DRT::FIBER::NodalFiberHolder::SetAngle(AngleType type, const std::vector<double>& angle)
{
  angles_.insert(std::pair<DRT::FIBER::AngleType, const std::vector<double>>(type, angle));
}

const std::vector<double>& DRT::FIBER::NodalFiberHolder::GetAngle(DRT::FIBER::AngleType type) const
{
  return angles_.at(type);
}

std::size_t DRT::FIBER::NodalFiberHolder::FibersSize() const { return fibers_.size(); }

std::size_t DRT::FIBER::NodalFiberHolder::coordinate_system_size() const
{
  return coordinate_system_directions_.size();
}

std::size_t DRT::FIBER::NodalFiberHolder::AnglesSize() const { return angles_.size(); }

bool DRT::FIBER::NodalFiberHolder::contains_coordinate_system_direction(
    DRT::FIBER::CoordinateSystemDirection type) const
{
  return coordinate_system_directions_.find(type) != coordinate_system_directions_.end();
}

FOUR_C_NAMESPACE_CLOSE

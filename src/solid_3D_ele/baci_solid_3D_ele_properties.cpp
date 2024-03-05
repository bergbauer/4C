/*! \file

\brief Properties of solid elements

\level 1
*/


#include "baci_config.hpp"

#include "baci_solid_3D_ele_properties.hpp"

#include "baci_comm_parobject.hpp"

BACI_NAMESPACE_OPEN

void DRT::ELEMENTS::AddToPack(
    CORE::COMM::PackBuffer& data, const DRT::ELEMENTS::SolidElementProperties& properties)
{
  CORE::COMM::ParObject::AddtoPack(data, static_cast<int>(properties.kintype));
  CORE::COMM::ParObject::AddtoPack(data, static_cast<int>(properties.element_technology));
  CORE::COMM::ParObject::AddtoPack(data, static_cast<int>(properties.prestress_technology));
}

void DRT::ELEMENTS::ExtractFromPack(std::size_t& position, const std::vector<char>& data,
    DRT::ELEMENTS::SolidElementProperties& properties)
{
  properties.kintype =
      static_cast<INPAR::STR::KinemType>(CORE::COMM::ParObject::ExtractInt(position, data));
  properties.element_technology =
      static_cast<ElementTechnology>(CORE::COMM::ParObject::ExtractInt(position, data));
  properties.prestress_technology =
      static_cast<PrestressTechnology>(CORE::COMM::ParObject::ExtractInt(position, data));
}

BACI_NAMESPACE_CLOSE
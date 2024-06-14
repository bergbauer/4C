/*----------------------------------------------------------------------------*/
/*! \file
\brief Element types of the 2D wall element for solid-part of porous medium using p1 (mixed)
 approach including scatra functionality.

\level 2


*/
/*---------------------------------------------------------------------------*/

#ifndef FOUR_C_W1_PORO_P1_SCATRA_ELETYPES_HPP
#define FOUR_C_W1_PORO_P1_SCATRA_ELETYPES_HPP

#include "4C_config.hpp"

#include "4C_w1_poro_p1_eletypes.hpp"

FOUR_C_NAMESPACE_OPEN

namespace Core::FE
{
  class Discretization;
}  // namespace Core::FE

namespace Discret
{
  namespace ELEMENTS
  {
    /*----------------------------------------------------------------------*
     |  QUAD 4 Element                                        schmidt 09/17 |
     *----------------------------------------------------------------------*/
    class WallQuad4PoroP1ScatraType : public Discret::ELEMENTS::WallQuad4PoroP1Type
    {
     public:
      std::string Name() const override { return "WallQuad4PoroP1ScatraType"; }

      static WallQuad4PoroP1ScatraType& Instance();

      Core::Communication::ParObject* Create(const std::vector<char>& data) override;

      Teuchos::RCP<Core::Elements::Element> Create(const std::string eletype,
          const std::string eledistype, const int id, const int owner) override;

      Teuchos::RCP<Core::Elements::Element> Create(const int id, const int owner) override;

      void setup_element_definition(
          std::map<std::string, std::map<std::string, Input::LineDefinition>>& definitions)
          override;

     private:
      static WallQuad4PoroP1ScatraType instance_;
    };

    /*----------------------------------------------------------------------*
     |  QUAD 9 Element                                        schmidt 09/17 |
     *----------------------------------------------------------------------*/
    class WallQuad9PoroP1ScatraType : public Discret::ELEMENTS::WallQuad9PoroP1Type
    {
     public:
      std::string Name() const override { return "WallQuad9PoroP1ScatraType"; }

      static WallQuad9PoroP1ScatraType& Instance();

      Core::Communication::ParObject* Create(const std::vector<char>& data) override;

      Teuchos::RCP<Core::Elements::Element> Create(const std::string eletype,
          const std::string eledistype, const int id, const int owner) override;

      Teuchos::RCP<Core::Elements::Element> Create(const int id, const int owner) override;

      void setup_element_definition(
          std::map<std::string, std::map<std::string, Input::LineDefinition>>& definitions)
          override;

     private:
      static WallQuad9PoroP1ScatraType instance_;
    };

    /*----------------------------------------------------------------------*
     |  TRI 3 Element                                         schmidt 09/17 ||
     *----------------------------------------------------------------------*/
    class WallTri3PoroP1ScatraType : public Discret::ELEMENTS::WallTri3PoroP1Type
    {
     public:
      std::string Name() const override { return "WallTri3PoroP1ScatraType"; }

      static WallTri3PoroP1ScatraType& Instance();

      Core::Communication::ParObject* Create(const std::vector<char>& data) override;

      Teuchos::RCP<Core::Elements::Element> Create(const std::string eletype,
          const std::string eledistype, const int id, const int owner) override;

      Teuchos::RCP<Core::Elements::Element> Create(const int id, const int owner) override;

      void setup_element_definition(
          std::map<std::string, std::map<std::string, Input::LineDefinition>>& definitions)
          override;

     private:
      static WallTri3PoroP1ScatraType instance_;
    };

  }  // namespace ELEMENTS
}  // namespace Discret


FOUR_C_NAMESPACE_CLOSE

#endif
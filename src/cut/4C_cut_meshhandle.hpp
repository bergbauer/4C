/*----------------------------------------------------------------------*/
/*! \file

\brief handle that holds the mesh specific information


\level 2
 *------------------------------------------------------------------------------------------------*/

#ifndef FOUR_C_CUT_MESHHANDLE_HPP
#define FOUR_C_CUT_MESHHANDLE_HPP

#include "4C_config.hpp"

#include "4C_cut_elementhandle.hpp"
#include "4C_cut_mesh.hpp"
#include "4C_cut_sidehandle.hpp"

FOUR_C_NAMESPACE_OPEN

namespace CORE::GEO
{
  namespace CUT
  {
    class Element;
    class LinearElement;
    class QuadraticElement;
    class Mesh;
    class Options;

    class SideHandle;
    class LinearSideHandle;
    class QuadraticSideHandle;

    class ElementHandle;
    class LinearElementHandle;
    class QuadraticElementHandle;

    /*!
     \brief Outside world interface to the mesh
     */
    class MeshHandle
    {
     public:
      /// constructor
      MeshHandle(Options& options, double norm = 1, Teuchos::RCP<PointPool> pp = Teuchos::null,
          bool cutmesh = false, int myrank = -1)
          : mesh_(options, norm, pp, cutmesh, myrank)
      {
      }

      /*========================================================================*/
      //! @name Create-routines for cut sides and mesh elements
      /*========================================================================*/

      /// create a new side (sidehandle) of the cutter discretization and return the sidehandle,
      /// non-tri3 sides will be subdivided into tri3 subsides depending on the options
      SideHandle* CreateSide(int sid, const std::vector<int>& nids, CORE::FE::CellType distype,
          CORE::GEO::CUT::Options& options);

      /// create a new element (elementhandle) of the background discretization and return the
      /// elementhandle, quadratic elements will create linear shadow elements
      ElementHandle* CreateElement(
          int eid, const std::vector<int>& nids, CORE::FE::CellType distype);

      /// create a new data structure for face oriented stabilization; the sides of the linear
      /// element are included into a sidehandle
      void CreateElementSides(Element& element);

      /// create a new data structure for face oriented stabilization; the sides of the quadratic
      /// element are included into a sidehandle
      void CreateElementSides(const std::vector<int>& nids, CORE::FE::CellType distype);

      /*========================================================================*/
      //! @name Get-routines for nodes, cutter sides, elements and element sides
      /*========================================================================*/

      /// get the node based on node id
      Node* GetNode(int nid) const;

      /// get the side (handle) based on side id of the cut mesh
      SideHandle* GetSide(int sid) const;

      /// get the mesh's element based on element id
      ElementHandle* GetElement(int eid) const;

      /// get the element' side of the mesh's element based on node ids
      SideHandle* GetSide(std::vector<int>& nodeids) const;

      /// Remove this side from the Sidehandle (Used by the SelfCut)
      void RemoveSubSide(CORE::GEO::CUT::Side* side);

      /// Add this side into the corresponding Sidehandle (Used by the SelfCut)
      void AddSubSide(CORE::GEO::CUT::Side* side);

      /// Mark this side as unphysical (Used by SelfCut)
      void mark_sub_sideas_unphysical(CORE::GEO::CUT::Side* side);

      /*========================================================================*/
      //! @name Get method for private variables
      /*========================================================================*/

      /// get the linear mesh
      Mesh& LinearMesh() { return mesh_; }

     private:
      /*========================================================================*/
      //! @name private class variables
      /*========================================================================*/

      Mesh mesh_;  ///< the linear mesh
      std::map<int, LinearElementHandle>
          linearelements_;  ///< map of element id and linear element handles
      std::map<int, Teuchos::RCP<QuadraticElementHandle>>
          quadraticelements_;  ///< map of element id and quadratic element handles
      std::map<int, LinearSideHandle> linearsides_;  ///< map of cut side id and linear side handles
      std::map<int, Teuchos::RCP<QuadraticSideHandle>>
          quadraticsides_;  ///< map of cut side id and quadratic side handles
      std::map<plain_int_set, LinearSideHandle>
          elementlinearsides_;  ///< map of element side node ids and linear side handles
      std::map<plain_int_set, Teuchos::RCP<QuadraticSideHandle>>
          elementquadraticsides_;  ///< map of element side node ids and quadratic side handles
    };

  }  // namespace CUT
}  // namespace CORE::GEO

FOUR_C_NAMESPACE_CLOSE

#endif

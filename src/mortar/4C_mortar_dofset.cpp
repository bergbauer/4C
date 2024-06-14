/*-----------------------------------------------------------------------*/
/*! \file
\brief A set of degrees of freedom special for mortar coupling


\level 1
*/
/*---------------------------------------------------------------------*/

#include "4C_mortar_dofset.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_mortar_node.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 |  ctor (public)                                             ukue 04/07|
 *----------------------------------------------------------------------*/
Mortar::DofSet::DofSet() : Core::DOFSets::DofSet() {}

/*----------------------------------------------------------------------*
 |  setup everything  (public)                                ukue 04/07|
 *----------------------------------------------------------------------*/
int Mortar::DofSet::assign_degrees_of_freedom(
    const Core::FE::Discretization& dis, const unsigned dspos, const int start)
{
  // first, we call the standard assign_degrees_of_freedom from the base class
  const int count = Core::DOFSets::DofSet::assign_degrees_of_freedom(dis, dspos, start);
  if (pccdofhandling_)
    FOUR_C_THROW("Point coupling conditions not yet implemented for Mortar::DofSet");

  // we'll get ourselves the row and column dof maps from the base class
  // and later replace them with our own version of them
  const int nummyrow = dofrowmap_->NumMyElements();
  std::vector<int> myrow(nummyrow);
  const int nummycol = dofcolmap_->NumMyElements();
  std::vector<int> mycol(nummycol);

  // now we loop all nodes in the interface discretization and create the new DOF vectors
  const int numMyColumnNodes = dis.NumMyColNodes();
  for (int i = 0; i < numMyColumnNodes; ++i)
  {
    Core::Nodes::Node* node = dis.lColNode(i);
    if (!node) FOUR_C_THROW("Cannot find local column node %d", i);

    // get dofs of node as created by base class DofSet
    std::vector<int> gdofs = Dof(node);
    const std::size_t numDofsOfNode = gdofs.size();

    // get dofs of node as we want them
    Mortar::Node* mrtrnode =
#ifndef FOUR_C_ENABLE_ASSERTIONS
        static_cast<Mortar::Node*>(node);
#else
        dynamic_cast<Mortar::Node*>(node);
    if (!mrtrnode) FOUR_C_THROW("dynamic_cast Core::Nodes::Node -> Mortar::Node failed");
#endif
    const auto& newdofs = mrtrnode->Dofs();
    for (std::size_t j = 0; j < numDofsOfNode; ++j)
    {
      // build dof column map
      if (!dofcolmap_->MyGID(gdofs[j])) FOUR_C_THROW("Mismatch in degrees of freedom");
      int lid = dofcolmap_->LID(gdofs[j]);
      mycol[lid] = newdofs[j];

      // build dof row map
      if (dofrowmap_->MyGID(gdofs[j]))  // only if proc owns this DOF
      {
        lid = dofrowmap_->LID(gdofs[j]);
        myrow[lid] = newdofs[j];
      }
    }
    if (numDofsOfNode > 0)
    {
      (*idxcolnodes_)[i] = newdofs[0];
    }
  }

  // we have new vectors, so recreate Epetra maps and replace old ones with them
  Teuchos::RCP<Epetra_Map> newdofrowmap =
      Teuchos::rcp(new Epetra_Map(-1, nummyrow, myrow.data(), 0, dofrowmap_->Comm()));
  Teuchos::RCP<Epetra_Map> newdofcolmap =
      Teuchos::rcp(new Epetra_Map(-1, nummycol, mycol.data(), 0, dofcolmap_->Comm()));

  // be a little psychotic in checking whether everything is ok....
  if (newdofrowmap->NumMyElements() != dofrowmap_->NumMyElements() ||
      newdofrowmap->NumGlobalElements() != dofrowmap_->NumGlobalElements() ||
      newdofcolmap->NumMyElements() != dofcolmap_->NumMyElements() ||
      newdofcolmap->NumGlobalElements() != dofcolmap_->NumGlobalElements() ||
      !newdofrowmap->UniqueGIDs())
    FOUR_C_THROW("Something's wrong in dof maps");

  // replace the old maps by our new ones (note: automatically deletes old ones)
  dofrowmap_ = newdofrowmap;
  dofcolmap_ = newdofcolmap;

  // mortar element == face element: we need this...
  idxcolfaces_ = idxcolelements_;
  numdfcolfaces_ = numdfcolelements_;

  // tell all proxies (again!)
  NotifyAssigned();

  return count;
}

FOUR_C_NAMESPACE_CLOSE
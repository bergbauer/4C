/*----------------------------------------------------------------------*/
/*! \file

\brief utils for biofilm fs3i

\level 3


 *----------------------------------------------------------------------*/

#include "4C_fs3i_biofilm_fsi_utils.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_global_data.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void FS3I::BioFilm::UTILS::ScatraChangeConfig(Teuchos::RCP<Core::FE::Discretization> scatradis,
    Teuchos::RCP<Core::FE::Discretization> dis, Teuchos::RCP<Epetra_Vector> disp)
{
  const int numnode = (scatradis->node_col_map())->NumMyElements();

  // Create Vector which holds all col-displacments of processor
  Teuchos::RCP<Epetra_Vector> coldisp = Teuchos::rcp(new Epetra_Vector(*(dis->dof_col_map())));

  // Export row-displacments to col-displacements
  Core::LinAlg::export_to(*disp, *coldisp);


  const Epetra_Vector& gvector = *coldisp;

  // loop over all nodes
  for (int index = 0; index < numnode; ++index)
  {
    // get current node
    int gid = (scatradis->node_col_map())->GID(index);
    Core::Nodes::Node* mynode = scatradis->g_node(gid);

    // get local fluid/structure node with the same local node id
    Core::Nodes::Node* lnode = dis->l_col_node(index);

    // get degrees of freedom associated with this fluid/structure node
    std::vector<int> nodedofs = dis->dof(0, lnode);

    // Since ChangePos requires vector of length 3, we use hardcoded length here
    // init with zero just to be sure
    std::vector<double> nvector(3, 0.0);

    // determine number of space dimensions
    const int numdim = Global::Problem::instance()->n_dim();

    for (int i = 0; i < numdim; ++i)
    {
      const int lid = gvector.Map().LID(nodedofs[i]);

      if (lid < 0)
        FOUR_C_THROW(
            "Proc %d: Cannot find gid=%d in Epetra_Vector", gvector.Comm().MyPID(), nodedofs[i]);
      nvector[i] += gvector[lid];
    }

    mynode->change_pos(nvector);
  }

  return;
}

FOUR_C_NAMESPACE_CLOSE

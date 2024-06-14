/*----------------------------------------------------------------------*/
/*! \file

\brief A collection of algebraic creation methods for namespace Core::LinAlg

\level 0
*/
/*----------------------------------------------------------------------*/

#include "4C_linalg_utils_sparse_algebra_create.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------*
 |  create a Epetra_CrsMatrix                                mwgee 12/06|
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_CrsMatrix> Core::LinAlg::CreateMatrix(const Epetra_Map& rowmap, const int npr)
{
  if (!rowmap.UniqueGIDs()) FOUR_C_THROW("Row map is not unique");
  return Teuchos::rcp(new Epetra_CrsMatrix(::Copy, rowmap, npr, false));
}

/*----------------------------------------------------------------------*
 |  create a Epetra_Vector                                   mwgee 12/06|
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Vector> Core::LinAlg::CreateVector(
    const Epetra_BlockMap& rowmap, const bool init)
{
  return Teuchos::rcp(new Epetra_Vector(rowmap, init));
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_IntVector> Core::LinAlg::CreateIntVector(
    const Epetra_BlockMap& rowmap, const bool init)
{
  return Teuchos::rcp(new Epetra_IntVector(rowmap, init));
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_MultiVector> Core::LinAlg::CreateMultiVector(
    const Epetra_BlockMap& rowmap, const int numrows, const bool init)
{
  return Teuchos::rcp(new Epetra_MultiVector(rowmap, numrows, init));
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Map> Core::LinAlg::CreateMap(const std::set<int>& gids, const Epetra_Comm& comm)
{
  std::vector<int> mapvec;
  mapvec.reserve(gids.size());
  mapvec.assign(gids.begin(), gids.end());
  Teuchos::RCP<Epetra_Map> map =
      Teuchos::rcp(new Epetra_Map(-1, mapvec.size(), mapvec.data(), 0, comm));
  mapvec.clear();
  return map;
}

/*----------------------------------------------------------------------*
 | create epetra_map with out-of-bound check                 farah 06/14|
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_Map> Core::LinAlg::CreateMap(
    const std::vector<int>& gids, const Epetra_Comm& comm)
{
  Teuchos::RCP<Epetra_Map> map;

  if ((int)gids.size() > 0)
    map = Teuchos::rcp(new Epetra_Map(-1, gids.size(), gids.data(), 0, comm));
  else
    map = Teuchos::rcp(new Epetra_Map(-1, gids.size(), nullptr, 0, comm));

  return map;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Core::LinAlg::CreateMapExtractorFromDiscretization(
    const Core::FE::Discretization& dis, int ndim, Core::LinAlg::MultiMapExtractor& extractor)
{
  std::set<int> conddofset;
  std::set<int> otherdofset;

  int numrownodes = dis.NumMyRowNodes();
  for (int i = 0; i < numrownodes; ++i)
  {
    Core::Nodes::Node* node = dis.lRowNode(i);

    std::vector<int> dof = dis.Dof(0, node);
    for (unsigned j = 0; j < dof.size(); ++j)
    {
      // test for dof position
      if (j != static_cast<unsigned>(ndim))
      {
        otherdofset.insert(dof[j]);
      }
      else
      {
        conddofset.insert(dof[j]);
      }
    }
  }

  std::vector<int> conddofmapvec;
  conddofmapvec.reserve(conddofset.size());
  conddofmapvec.assign(conddofset.begin(), conddofset.end());
  conddofset.clear();
  Teuchos::RCP<Epetra_Map> conddofmap =
      Teuchos::rcp(new Epetra_Map(-1, conddofmapvec.size(), conddofmapvec.data(), 0, dis.Comm()));
  conddofmapvec.clear();

  std::vector<int> otherdofmapvec;
  otherdofmapvec.reserve(otherdofset.size());
  otherdofmapvec.assign(otherdofset.begin(), otherdofset.end());
  otherdofset.clear();
  Teuchos::RCP<Epetra_Map> otherdofmap =
      Teuchos::rcp(new Epetra_Map(-1, otherdofmapvec.size(), otherdofmapvec.data(), 0, dis.Comm()));
  otherdofmapvec.clear();

  std::vector<Teuchos::RCP<const Epetra_Map>> maps(2);
  maps[0] = otherdofmap;
  maps[1] = conddofmap;
  extractor.Setup(*dis.dof_row_map(), maps);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Core::LinAlg::CreateMapExtractorFromDiscretization(const Core::FE::Discretization& dis,
    const Core::DOFSets::DofSetInterface& dofset, int ndim, Core::LinAlg::MapExtractor& extractor)
{
  std::set<int> conddofset;
  std::set<int> otherdofset;

  int numrownodes = dis.NumMyRowNodes();
  for (int i = 0; i < numrownodes; ++i)
  {
    Core::Nodes::Node* node = dis.lRowNode(i);

    std::vector<int> dof = dofset.Dof(node);
    for (unsigned j = 0; j < dof.size(); ++j)
    {
      // test for dof position
      if (j < static_cast<unsigned>(ndim))
      {
        otherdofset.insert(dof[j]);
      }
      else
      {
        conddofset.insert(dof[j]);
      }
    }
  }

  std::vector<int> conddofmapvec;
  conddofmapvec.reserve(conddofset.size());
  conddofmapvec.assign(conddofset.begin(), conddofset.end());
  conddofset.clear();
  Teuchos::RCP<Epetra_Map> conddofmap =
      Teuchos::rcp(new Epetra_Map(-1, conddofmapvec.size(), conddofmapvec.data(), 0, dis.Comm()));
  conddofmapvec.clear();

  std::vector<int> otherdofmapvec;
  otherdofmapvec.reserve(otherdofset.size());
  otherdofmapvec.assign(otherdofset.begin(), otherdofset.end());
  otherdofset.clear();
  Teuchos::RCP<Epetra_Map> otherdofmap =
      Teuchos::rcp(new Epetra_Map(-1, otherdofmapvec.size(), otherdofmapvec.data(), 0, dis.Comm()));
  otherdofmapvec.clear();

  extractor.Setup(*dofset.dof_row_map(), conddofmap, otherdofmap);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void Core::LinAlg::CreateMapExtractorFromDiscretization(const Core::FE::Discretization& dis,
    int ndim_field1, int ndim_field2, Core::LinAlg::MultiMapExtractor& extractor)
{
  unsigned fp_dim = static_cast<unsigned>(ndim_field1 + ndim_field2);

  std::set<int> conddofset;
  std::set<int> otherdofset;

  int numrownodes = dis.NumMyRowNodes();
  for (int i = 0; i < numrownodes; ++i)
  {
    Core::Nodes::Node* node = dis.lRowNode(i);

    std::vector<int> dof = dis.Dof(0, node);

    if ((dof.size() % fp_dim) != 0)
      FOUR_C_THROW(
          "Vector-Scalar-Split is not unique! Mismatch between number of dofs and vector/scalar "
          "dim");

    for (unsigned j = 0; j < dof.size(); ++j)
    {
      // test for dof position
      if (j % fp_dim < static_cast<unsigned>(ndim_field1))
      {
        otherdofset.insert(dof[j]);
      }
      else
      {
        conddofset.insert(dof[j]);
      }
    }
  }

  std::vector<int> conddofmapvec;
  conddofmapvec.reserve(conddofset.size());
  conddofmapvec.assign(conddofset.begin(), conddofset.end());
  conddofset.clear();
  Teuchos::RCP<Epetra_Map> conddofmap =
      Teuchos::rcp(new Epetra_Map(-1, conddofmapvec.size(), conddofmapvec.data(), 0, dis.Comm()));
  conddofmapvec.clear();

  std::vector<int> otherdofmapvec;
  otherdofmapvec.reserve(otherdofset.size());
  otherdofmapvec.assign(otherdofset.begin(), otherdofset.end());
  otherdofset.clear();
  Teuchos::RCP<Epetra_Map> otherdofmap =
      Teuchos::rcp(new Epetra_Map(-1, otherdofmapvec.size(), otherdofmapvec.data(), 0, dis.Comm()));
  otherdofmapvec.clear();

  std::vector<Teuchos::RCP<const Epetra_Map>> maps(2);
  maps[0] = otherdofmap;
  maps[1] = conddofmap;
  extractor.Setup(*dis.dof_row_map(), maps);
}

FOUR_C_NAMESPACE_CLOSE
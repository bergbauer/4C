/*----------------------------------------------------------------------*/
/*! \file

\brief MueLu transfer factory class for contact
\level 2
\maintainer Matthias Mayr

*----------------------------------------------------------------------*/

#ifndef MUELU_CONTACTTRANSFERFACTORY_DEF_HPP_
#define MUELU_CONTACTTRANSFERFACTORY_DEF_HPP_

#ifdef HAVE_MueLuContact

#include "MueLu_ContactTransferFactory_decl.hpp"

#include <Xpetra_Matrix.hpp>
#include <Xpetra_CrsMatrixWrap.hpp>
#include <Xpetra_MapFactory.hpp>
#include <Xpetra_VectorFactory.hpp>

#include "MueLu_Level.hpp"
#include "MueLu_Monitor.hpp"

namespace MueLu
{
  template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
  ContactTransferFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::ContactTransferFactory(
      Teuchos::RCP<FactoryBase> PtentFact)
      : PtentFact_(PtentFact)
  {
  }

  template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
  ContactTransferFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::~ContactTransferFactory()
  {
  }

  template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
  void ContactTransferFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::DeclareInput(
      Level &fineLevel, Level &coarseLevel) const
  {
    // fineLevel.DeclareInput(varName_,factory_,this);
    coarseLevel.DeclareInput("P", PtentFact_.get(), this);
    fineLevel.DeclareInput("SegAMapExtractor", MueLu::NoFactory::get(), this);
  }

  template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
  void ContactTransferFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::Build(
      Level &fineLevel, Level &coarseLevel) const
  {
    typedef Xpetra::Matrix<Scalar, LocalOrdinal, GlobalOrdinal, Node> OperatorClass;
    typedef Xpetra::MapFactory<LocalOrdinal, GlobalOrdinal, Node> MapFactoryClass;
    typedef Xpetra::Vector<Scalar, LocalOrdinal, GlobalOrdinal, Node> VectorClass;
    typedef Xpetra::VectorFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node> VectorFactoryClass;

    Monitor m(*this, "Contact transfer factory");

    if (fineLevel.IsAvailable("SegAMapExtractor", MueLu::NoFactory::get()) == false)
    {
      GetOStream(Runtime0, 0)
          << "ContactTransferFactory::Build: Use user provided map extractor with "
          << mapextractor_->NumMaps() << " submaps for segregation filter for " << varName_
          << std::endl;
      fineLevel.Set("SegAMapExtractor", mapextractor_, MueLu::NoFactory::get());
    }
    TEUCHOS_TEST_FOR_EXCEPTION(!coarseLevel.IsAvailable("P", PtentFact_.get()),
        Exceptions::RuntimeError,
        "MueLu::ContactTransferFactory::Build(): P (generated by TentativePFactory) not "
        "available.");

    // fetch map extractor from level
    Teuchos::RCP<const MapExtractorClass> mapextractor =
        fineLevel.Get<Teuchos::RCP<const MapExtractorClass>>(
            "SegAMapExtractor", MueLu::NoFactory::get());

    Teuchos::RCP<OperatorClass> Ptent =
        coarseLevel.Get<Teuchos::RCP<OperatorClass>>("P", PtentFact_.get());

    // extract maps from mapextractor:
    std::vector<Teuchos::RCP<Vector>> subMapBlockVectors;
    for (size_t i = 0; i < mapextractor->NumMaps(); i++)
    {
      Teuchos::RCP<const Map> submap = mapextractor->getMap(i);
      Teuchos::RCP<Vector> fullBlockVector = VectorFactoryClass::Build(mapextractor->getFullMap());

      Teuchos::RCP<Vector> partBlockVector = VectorFactoryClass::Build(submap);
      partBlockVector->putScalar(1.0);
      mapextractor->InsertVector(partBlockVector, i, fullBlockVector);
      subMapBlockVectors.push_back(fullBlockVector);
    }

    // BlockColMapVector is a vector which lives in the column map of Ptent
    // Its values are the number of submap in the coarse-level map extractor or -1, if it is not in
    // the mapextractor at all we initialize the vector with -2.0 (= uninitialized) in the end all
    // entries should be > -2.0
    Teuchos::RCP<VectorClass> BlockColMapVector = VectorFactoryClass::Build(Ptent->getColMap());
    BlockColMapVector->putScalar(-2.0);
    Teuchos::ArrayRCP<Scalar> localBlockColMapVector = BlockColMapVector->getDataNonConst(0);

    // needed for access of local values of fullMasterVector and fullSlaveVector
    Teuchos::ArrayRCP<const Scalar> localMasterVector =
        subMapBlockVectors.at(0)->getData(0);  // TODO just use the maps!!!!
    Teuchos::ArrayRCP<const Scalar> localSlaveVector = subMapBlockVectors.at(1)->getData(0);

    // loop over local rows of Ptent
    for (size_t row = 0; row < Ptent->getNodeNumRows(); row++)
    {
      Scalar cnt = 0.0;
      LocalOrdinal curSubMap = -1;
      for (size_t i = 0; i < mapextractor->NumMaps(); i++)
      {
        Teuchos::ArrayRCP<const Scalar> localsubMapBlockVector =
            subMapBlockVectors.at(i)->getData(0);
        cnt += localsubMapBlockVector[row];
        if (localsubMapBlockVector[row] == 1.0) curSubMap = Teuchos::as<LocalOrdinal>(i);
      }
      if (cnt > 1)
        std::cout << " warning: This cannot be. row is both master and slave?" << std::endl;

      Teuchos::ArrayView<const LocalOrdinal> indices;
      Teuchos::ArrayView<const Scalar> vals;
      Ptent->getLocalRowView(row, indices, vals);

      for (size_t i = 0; i < (size_t)indices.size(); i++)
      {
        // furthermore: localBlockColMapVector(indices[i]) should be either -2 (not initialized) or
        // have the same number of submap from map extractor!

        // 1) if Index indices[i] has not been assigned yet, define it as inner DOF per default
        if (localBlockColMapVector[indices[i]] == -2.0)
          localBlockColMapVector[indices[i]] = -1.0;  // -1.0 denotes (no MASTER nor SLAVE)

        if (curSubMap > -1)
        {
          if (localBlockColMapVector[indices[i]] != -1.0 &&
              localBlockColMapVector[indices[i]] != curSubMap)
            std::cout << "warning: localBlockColMapVector already initialized" << std::endl;
          localBlockColMapVector[indices[i]] = curSubMap;  // e.g. 0 denotes MASTER, 1 denotes SLAVE
        }
      }
    }

    TEUCHOS_TEST_FOR_EXCEPTION(
        BlockColMapVector->getLocalLength() != Teuchos::as<size_t>(localBlockColMapVector.size()),
        Exceptions::RuntimeError,
        "MueLu::ContactTransferFactory::Build(): size of localBlockColMapVector wrong");

    std::vector<std::vector<GlobalOrdinal>> coarseSubMapGids;
    for (size_t i = 0; i < mapextractor->NumMaps(); i++)
    {
      coarseSubMapGids.push_back(std::vector<GlobalOrdinal>(0));
    }
    for (size_t i = 0; i < Teuchos::as<size_t>(localBlockColMapVector.size()); i++)
    {
      if (localBlockColMapVector[i] > -1.0)
      {
        const LocalOrdinal subMapIndex = (LocalOrdinal)localBlockColMapVector[i];
        coarseSubMapGids.at(subMapIndex)
            .push_back(BlockColMapVector->getMap()->getGlobalElement(Teuchos::as<LocalOrdinal>(i)));
      }
    }

    // build column maps
    std::vector<Teuchos::RCP<const Map>> colMaps(mapextractor->NumMaps());
    for (size_t i = 0; i < mapextractor->NumMaps(); i++)
    {
      std::vector<GlobalOrdinal> gidvector = coarseSubMapGids[i];
      std::sort(gidvector.begin(), gidvector.end());
      gidvector.erase(std::unique(gidvector.begin(), gidvector.end()), gidvector.end());

      Teuchos::ArrayView<GlobalOrdinal> gidsForMap(&gidvector[0], gidvector.size());
      colMaps[i] = MapFactoryClass::Build(Ptent->getColMap()->lib(), gidsForMap.size(), gidsForMap,
          Ptent->getColMap()->getIndexBase(), Ptent->getColMap()->getComm());
      TEUCHOS_TEST_FOR_EXCEPTION(colMaps[i] == Teuchos::null, Exceptions::RuntimeError,
          "MueLu::ContactTransferFactory::Build: error when building column map.");
      TEUCHOS_TEST_FOR_EXCEPTION((colMaps[i])->getNodeNumElements() != gidvector.size(),
          Exceptions::RuntimeError,
          "MueLu::ContactTransferFactory::Build: size of map does not fit to size of gids.");
    }

    // build coarse level MapExtractor
    Teuchos::RCP<const MapExtractorClass> coarseMapExtractor =
        Xpetra::MapExtractorFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::Build(
            Ptent->getDomainMap(), colMaps);

    // store map extractor in coarse level
    coarseLevel.Set("SegAMapExtractor", coarseMapExtractor, MueLu::NoFactory::get());
  }


}  // namespace MueLu

#endif  // HAVE_MueLuContact


#endif /* MUELU_CONTACTTRANSFERFACTORY_DEF_HPP_ */

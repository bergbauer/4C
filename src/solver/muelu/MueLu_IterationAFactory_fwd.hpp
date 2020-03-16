/*----------------------------------------------------------------------*/
/*! \file

\brief MueLu iteration factory class
\level 2
\maintainer Matthias Mayr

*----------------------------------------------------------------------*/

#ifndef MUELU_ITERATIONAFACTORY_FWD_HPP_
#define MUELU_ITERATIONAFACTORY_FWD_HPP_

#include <Trilinos_version.h>
#if !(TRILINOS_MAJOR_MINOR_VERSION >= 121400) || defined(HAVE_MueLuContact)

namespace MueLu
{
  template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
  class IterationAFactory;
}

#ifndef MUELU_ITERATIONAFACTORY_SHORT
#define MUELU_ITERATIONAFACTORY_SHORT
#endif

#endif  // HAVE_MueLuContact

#endif /* MUELU_ITERATIONAFACTORY_FWD_HPP_ */

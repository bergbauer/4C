/*----------------------------------------------------------------------*/
/*! \file

\brief MueLu repartition algorithm for contact
\level 2
\maintainer Matthias Mayr

*----------------------------------------------------------------------*/

#ifndef MUELU_CONCTACTSPREPARTITIONINTERFACE_FWD_HPP_
#define MUELU_CONCTACTSPREPARTITIONINTERFACE_FWD_HPP_

#ifdef HAVE_MueLuContact

namespace MueLu
{
  template <class LocalOrdinal, class GlobalOrdinal, class Node, class LocalMatOps>
  class ContactSPRepartitionInterface;
}

#ifndef MUELU_CONTACTSPREPARTITIONINTERFACE_SHORT
#define MUELU_CONTACTSPREPARTITIONINTERFACE_SHORT
#endif

#endif  // HAVE_MueLuContact

#endif /* MUELU_CONCTACTSPREPARTITIONINTERFACE_FWD_HPP_ */

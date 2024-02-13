/*----------------------------------------------------------------------*/
/*! \file
\brief This bundle is used to hold all contact constitutive laws from the input file

\level 3

*/

/*----------------------------------------------------------------------*/

#ifndef BACI_CONTACT_CONSTITUTIVELAW_BUNDLE_HPP
#define BACI_CONTACT_CONSTITUTIVELAW_BUNDLE_HPP

/*----------------------------------------------------------------------*/
/* headers */
#include "baci_config.hpp"

#include <Teuchos_RCP.hpp>

#include <vector>

BACI_NAMESPACE_OPEN

namespace CONTACT
{
  namespace CONSTITUTIVELAW
  {
    class Container;
    class Parameter;
    /*----------------------------------------------------------------------*/
    /**
     * \brief This bundle is used to hold all contact constitutive laws from the input file
     *
     * Basically it is a map, mapping IDs to contact constitutive laws which is wrapped to make some
     * sanity checks
     */
    class Bundle
    {
     public:
      /// construct
      Bundle();

      /** \brief insert new container holding contact constitutive law parameter and ID
       * \param[in] id ID od the contact constitutive law in the input file
       * \law[in] container holding the law parameter read from the input file
       */
      void Insert(int id, Teuchos::RCP<Container> mat);

      /** \brief check if a contact constitutive law exists for provided ID
       *
       *\param[in] id ID of the contact constitutive law in the input file
       * \return Upon failure -1 is returned, otherwise >=0
       */
      int Find(const int id) const;

      /// make quick access parameters
      void MakeParameters();

      /// return number of defined materials
      int Num() const { return map_.size(); }

      /** return contact constitutive law by ID
       *
       * \param[in] id ID of the contact constitutive law given in the input file
       */
      Teuchos::RCP<Container> ById(const int id) const;

      /// return problem index to read from
      int GetReadFromProblem() const { return readfromproblem_; }

     private:
      /// the map linking contact constitutive law IDs to input constitutive laws
      std::map<int, Teuchos::RCP<Container>> map_;

      /// the index of problem instance of which contact constitutive law read-in shall be performed
      int readfromproblem_;
    };

  }  // namespace CONSTITUTIVELAW

}  // namespace CONTACT


BACI_NAMESPACE_CLOSE

#endif  // CONTACT_CONSTITUTIVELAW_BUNDLE_H
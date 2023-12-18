/*----------------------------------------------------------------------*/
/*! \file

\brief Central type object management.

\level 0


*/
/*----------------------------------------------------------------------*/

#include "baci_comm_parobjectfactory.H"

#include "baci_comm_parobject.H"
#include "baci_lib_discret.H"
#include "baci_lib_element.H"
#include "baci_lib_elementtype.H"
#include "baci_linalg_utils_densematrix_communication.H"
#include "baci_utils_exceptions.H"

#include <Epetra_Comm.h>

BACI_NAMESPACE_OPEN

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
namespace CORE::COMM
{
  namespace
  {
    class ParObjectPreRegister
    {
     public:
      static ParObjectPreRegister* Instance()
      {
        if (instance_ == nullptr)
        {
          instance_ = std::make_unique<ParObjectPreRegister>();
        }
        return instance_.get();
      }

      void Register(ParObjectType* parobjecttype) { types_.push_back(parobjecttype); }

      static void Finalize()
      {
        if (instance_)
        {
          for (auto& parobjecttype : instance_->types_)
          {
            parobjecttype->UniqueParObjectId();
          }
          instance_.reset();
        }
      }

     private:
      static std::unique_ptr<ParObjectPreRegister> instance_;

      /// preregistered types
      std::vector<ParObjectType*> types_;
    };

    std::unique_ptr<ParObjectPreRegister> ParObjectPreRegister::instance_;
  }  // namespace
}  // namespace CORE::COMM

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
CORE::COMM::ParObjectType::ParObjectType() : objectid_(0)
{
  ParObjectPreRegister::Instance()->Register(this);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
int CORE::COMM::ParObjectType::UniqueParObjectId()
{
  if (objectid_ == 0)
  {
    CORE::COMM::ParObjectFactory::Instance().Register(this);
    // ParObjectFactory::Instance().FinalizeRegistration();
  }
  return objectid_;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
CORE::COMM::ParObjectFactory& CORE::COMM::ParObjectFactory::Instance()
{
  static std::unique_ptr<CORE::COMM::ParObjectFactory> instance;
  if (instance == nullptr)
  {
    // Create on demand. This is required since the instance will be accessed
    // by ParObjectType constructors. ParObjectType are singletons as
    // well. The singleton creation order is undefined.
    instance = std::unique_ptr<CORE::COMM::ParObjectFactory>(new ParObjectFactory);
  }
  return *instance;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
CORE::COMM::ParObject* CORE::COMM::ParObjectFactory::Create(const std::vector<char>& data)
{
  FinalizeRegistration();

  // mv ptr behind the size record
  const int* ptr = (const int*)(data.data());
  // get the type
  const int type = *ptr;

  std::map<int, ParObjectType*>::iterator i = type_map_.find(type);
  if (i == type_map_.end())
  {
    dserror("object id %d undefined. Have you extended CORE::COMM::ParObjectList()?", type);
  }

  ParObject* o = i->second->Create(data);

  if (o == nullptr)
  {
    dserror("failed to create object of type %d", type);
  }

  return o;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<DRT::Element> CORE::COMM::ParObjectFactory::Create(
    const std::string eletype, const std::string eledistype, const int id, const int owner)
{
  FinalizeRegistration();
  std::map<std::string, DRT::ElementType*>::iterator c = element_cache_.find(eletype);
  if (c != element_cache_.end())
  {
    return c->second->Create(eletype, eledistype, id, owner);
  }

  // This is element specific code. Thus we need a down cast.

  for (std::map<int, ParObjectType*>::iterator i = type_map_.begin(); i != type_map_.end(); ++i)
  {
    ParObjectType* pot = i->second;
    DRT::ElementType* eot = dynamic_cast<DRT::ElementType*>(pot);
    if (eot != nullptr)
    {
      Teuchos::RCP<DRT::Element> ele = eot->Create(eletype, eledistype, id, owner);
      if (ele != Teuchos::null)
      {
        element_cache_[eletype] = eot;
        return ele;
      }
    }
  }

  dserror("Unknown type '%s' of finite element", eletype.c_str());
  return Teuchos::null;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void CORE::COMM::ParObjectFactory::Register(ParObjectType* object_type)
{
  std::string name = object_type->Name();
  const unsigned char* str = reinterpret_cast<const unsigned char*>(name.c_str());

  // simple hash
  // see http://www.cse.yorku.ca/~oz/hash.html
  int hash = 5381;
  for (int c = 0; (c = *str); ++str)
  {
    hash = ((hash << 5) + hash) ^ c; /* hash * 33 ^ c */
  }

  // assume no collisions for now

  std::map<int, ParObjectType*>::iterator i = type_map_.find(hash);
  if (i != type_map_.end())
  {
    dserror("object (%s,%d) already defined: (%s,%d)", name.c_str(), hash,
        i->second->Name().c_str(), i->first);
  }

  if (hash == 0)
  {
    dserror("illegal hash value");
  }

  // std::cout << "register type object: '" << name << "': " << hash << "\n";

  type_map_[hash] = object_type;
  object_type->objectid_ = hash;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void CORE::COMM::ParObjectFactory::FinalizeRegistration() { ParObjectPreRegister::Finalize(); }


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void CORE::COMM::ParObjectFactory::InitializeElements(DRT::Discretization& dis)
{
  FinalizeRegistration();

  // find participating element types such that only those element types are initialized

  std::set<int> ids;
  int numelements = dis.NumMyColElements();
  for (int i = 0; i < numelements; ++i)
  {
    ids.insert(dis.lColElement(i)->ElementType().UniqueParObjectId());
  }

  std::vector<int> localtypeids;
  std::vector<int> globaltypeids;

  localtypeids.reserve(ids.size());
  localtypeids.assign(ids.begin(), ids.end());

  CORE::LINALG::AllreduceVector(localtypeids, globaltypeids, dis.Comm());

  std::set<DRT::ElementType*>& ae = active_elements_[&dis];

  // This is element specific code. Thus we need a down cast.

  for (std::vector<int>::iterator i = globaltypeids.begin(); i != globaltypeids.end(); ++i)
  {
    ParObjectType* pot = type_map_[*i];
    DRT::ElementType* eot = dynamic_cast<DRT::ElementType*>(pot);
    if (eot != nullptr)
    {
      ae.insert(eot);
      int err = eot->Initialize(dis);
      if (err) dserror("Element Initialize returned err=%d", err);
    }
    else
    {
      dserror("illegal element type id %d", *i);
    }
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void CORE::COMM::ParObjectFactory::PreEvaluate(DRT::Discretization& dis, Teuchos::ParameterList& p,
    Teuchos::RCP<CORE::LINALG::SparseOperator> systemmatrix1,
    Teuchos::RCP<CORE::LINALG::SparseOperator> systemmatrix2,
    Teuchos::RCP<Epetra_Vector> systemvector1, Teuchos::RCP<Epetra_Vector> systemvector2,
    Teuchos::RCP<Epetra_Vector> systemvector3)
{
  FinalizeRegistration();

  std::set<DRT::ElementType*>& ae = active_elements_[&dis];

  for (std::set<DRT::ElementType*>::iterator i = ae.begin(); i != ae.end(); ++i)
  {
    (*i)->PreEvaluate(
        dis, p, systemmatrix1, systemmatrix2, systemvector1, systemvector2, systemvector3);
  }
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void CORE::COMM::ParObjectFactory::SetupElementDefinition(
    std::map<std::string, std::map<std::string, DRT::INPUT::LineDefinition>>& definitions)
{
  FinalizeRegistration();

  // Here we want to visit all elements known to the factory.
  //
  // This is element specific code. Thus we need a down cast.

  for (std::map<int, ParObjectType*>::iterator i = type_map_.begin(); i != type_map_.end(); ++i)
  {
    ParObjectType* pot = i->second;
    DRT::ElementType* eot = dynamic_cast<DRT::ElementType*>(pot);
    if (eot != nullptr)
    {
      eot->SetupElementDefinition(definitions);
    }
  }
}

BACI_NAMESPACE_CLOSE

/*-----------------------------------------------------------*/
/*! \file

\brief small data container for beam interaction framework


\level 3

*/
/*-----------------------------------------------------------*/

#ifndef FOUR_C_BEAMINTERACTION_DATA_HPP
#define FOUR_C_BEAMINTERACTION_DATA_HPP

#include "4C_config.hpp"

#include "4C_inpar_beaminteraction.hpp"
#include "4C_linalg_fixedsizematrix.hpp"

#include <map>
#include <set>

FOUR_C_NAMESPACE_OPEN


// forward declarations
namespace Discret
{
  class ParObject;
  class PackBuffer;
}  // namespace Discret
// forward declaration
namespace STR
{
  namespace TimeInt
  {
    class BaseDataGlobalState;
  }
}  // namespace STR
namespace BEAMINTERACTION
{
  /*!
   * data container for input file parameters for submodel crosslinking in beam interaction
   * author eichinger*/
  class BeamInteractionParams
  {
   public:
    //! constructor
    BeamInteractionParams();

    //! destructor
    virtual ~BeamInteractionParams() = default;

    //! initialize with the stuff coming from input file
    void Init();

    //! setup member variables
    void Setup();

    //! returns the isinit_ flag
    inline const bool& is_init() const { return isinit_; };

    //! returns the issetup_ flag
    inline const bool& is_setup() const { return issetup_; };

    //! Checks the init and setup status
    inline void check_init_setup() const
    {
      if (!is_init() or !is_setup()) FOUR_C_THROW("Call Init() and Setup() first!");
    }

    //! Checks the init status
    inline void check_init() const
    {
      if (!is_init()) FOUR_C_THROW("Init() has not been called, yet!");
    }

    /// number of crosslinkers per type
    Inpar::BEAMINTERACTION::RepartitionStrategy get_repartition_strategy() const
    {
      check_init_setup();
      return rep_strategy_;
    };

    Inpar::BEAMINTERACTION::SearchStrategy GetSearchStrategy() const
    {
      check_init_setup();
      return search_strategy_;
    }


   private:
    bool isinit_;

    bool issetup_;

    /// number of crosslinker that are initially set
    Inpar::BEAMINTERACTION::RepartitionStrategy rep_strategy_;

    /// search strategy for beam coupling
    Inpar::BEAMINTERACTION::SearchStrategy search_strategy_;
  };



  namespace Data
  {
    //! struct to store crosslinker data and enable parallel redistribution
    struct CrosslinkerData
    {
     public:
      //! constructor
      CrosslinkerData();

      //! destructor
      virtual ~CrosslinkerData() = default;


      //!@name data access functions
      //! @{

      int get_id() const { return id_; };

      Core::LinAlg::Matrix<3, 1> const& GetPosition() const { return pos_; };

      int GetNumberOfBonds() const { return numbond_; };

      std::vector<std::pair<int, int>> const& GetBSpots() const { return bspots_; };

      void SetId(int id) { id_ = id; };

      void SetPosition(Core::LinAlg::Matrix<3, 1> const& clpos) { pos_ = clpos; };

      void SetNumberOfBonds(int clnumbond) { numbond_ = clnumbond; };

      void SetBSpots(std::vector<std::pair<int, int>> const& clbspots) { bspots_ = clbspots; };
      void SetBspot(int bspotid, std::pair<int, int> bpsotone) { bspots_[bspotid] = bpsotone; };

      //! @}

      //!@name methods for enabling parallel use of data container
      //! @{

      /*!
      \brief Pack this class so it can be communicated
      */
      void Pack(Core::Communication::PackBuffer& data) const;

      /*!
      \brief Unpack data from a char vector into this container
      */
      void Unpack(std::vector<char> const& data);

      //! @}

     private:
      //!@name linker member variables
      //! @{

      /// linker gid
      int id_;

      /// current position of crosslinker
      Core::LinAlg::Matrix<3, 1> pos_;

      /// number of active bonds
      int numbond_;

      /// gid of element to local number of bspot, [0] and [1] first and second bspot
      std::vector<std::pair<int, int>> bspots_;


      //! @}
    };

    //! struct to store crosslinker data and enable parallel redistribution
    struct BeamData
    {
     public:
      //! constructor
      BeamData();

      //! destructor
      virtual ~BeamData() = default;


      //!@name data access functions
      //! @{

      int get_id() const { return id_; };

      std::map<Inpar::BEAMINTERACTION::CrosslinkerType,
          std::map<int, Core::LinAlg::Matrix<3, 1>>> const&
      GetBSpotPositions() const
      {
        return bspotpos_;
      };

      Core::LinAlg::Matrix<3, 1> const& GetBSpotPosition(
          Inpar::BEAMINTERACTION::CrosslinkerType linkertype, int bspotid) const
      {
        return bspotpos_.at(linkertype).at(bspotid);
      };

      std::map<Inpar::BEAMINTERACTION::CrosslinkerType,
          std::map<int, Core::LinAlg::Matrix<3, 3>>> const&
      GetBSpotTriads() const
      {
        return bspottriad_;
      };
      Core::LinAlg::Matrix<3, 3> const& GetBSpotTriad(
          Inpar::BEAMINTERACTION::CrosslinkerType linkertype, int bspotid) const
      {
        return bspottriad_.at(linkertype).at(bspotid);
      };

      std::map<Inpar::BEAMINTERACTION::CrosslinkerType, std::map<int, std::set<int>>> const&
      GetBSpotStatus() const
      {
        return bspotstatus_;
      };

      std::set<int> const& GetBSpotStatusAt(
          Inpar::BEAMINTERACTION::CrosslinkerType linkertype, int bspotid) const
      {
        return bspotstatus_.at(linkertype).at(bspotid);
      };

      // not [] necessary here in case element has no binding spot of certain type
      unsigned int get_number_of_binding_spots_of_type(
          Inpar::BEAMINTERACTION::CrosslinkerType linkertype)
      {
        return bspotstatus_[linkertype].size();
      };


      void SetId(int id) { id_ = id; };

      void SetBSpotPositions(std::map<Inpar::BEAMINTERACTION::CrosslinkerType,
          std::map<int, Core::LinAlg::Matrix<3, 1>>> const& bspotpos)
      {
        bspotpos_ = bspotpos;
      };
      void SetBSpotPosition(Inpar::BEAMINTERACTION::CrosslinkerType linkertype, int bspotid,
          Core::LinAlg::Matrix<3, 1> const& bspotpos)
      {
        bspotpos_[linkertype][bspotid] = bspotpos;
      };

      void SetBSpotTriads(std::map<Inpar::BEAMINTERACTION::CrosslinkerType,
          std::map<int, Core::LinAlg::Matrix<3, 3>>> const& bspottriad)
      {
        bspottriad_ = bspottriad;
      };
      void SetBSpotTriad(Inpar::BEAMINTERACTION::CrosslinkerType linkertype, int bspotid,
          Core::LinAlg::Matrix<3, 3> const& bspottriad)
      {
        bspottriad_[linkertype][bspotid] = bspottriad;
      };

      void SetBSpotStatus(
          std::map<Inpar::BEAMINTERACTION::CrosslinkerType, std::map<int, std::set<int>>> const&
              bspotstatus)
      {
        bspotstatus_ = bspotstatus;
      };
      void SetBSpotStatus(
          Inpar::BEAMINTERACTION::CrosslinkerType linkertype, int bspotid, std::set<int> clgids)
      {
        bspotstatus_[linkertype][bspotid] = clgids;
      };

      void erase_bond_from_binding_spot(
          Inpar::BEAMINTERACTION::CrosslinkerType linkertype, int locbspotid, int clgid)
      {
        bspotstatus_.at(linkertype).at(locbspotid).erase(clgid);
      }
      void add_bond_to_binding_spot(
          Inpar::BEAMINTERACTION::CrosslinkerType linkertype, int locbspotid, int clgid)
      {
        bspotstatus_.at(linkertype).at(locbspotid).insert(clgid);
      }


      //! @}

      //!@name methods for enabling parallel use of data container
      //! @{

      /*!
      \brief Pack this class so it can be communicated
      */
      void Pack(Core::Communication::PackBuffer& data) const;

      /*!
      \brief Unpack data from a char vector into this container
      */
      void Unpack(std::vector<char> const& data);

      //! @}

     private:
      //!@name linker member variables
      //! @{

      /// beam gid
      int id_;

      /// current position at bindingspots (xi) (key is local number of binding spot)
      std::map<Inpar::BEAMINTERACTION::CrosslinkerType, std::map<int, Core::LinAlg::Matrix<3, 1>>>
          bspotpos_;

      /// current triad at bindingspots (xi) (key is local number of binding spot)
      std::map<Inpar::BEAMINTERACTION::CrosslinkerType, std::map<int, Core::LinAlg::Matrix<3, 3>>>
          bspottriad_;

      /// key is locn of bspot, holds gid of crosslinker to which it is bonded
      std::map<Inpar::BEAMINTERACTION::CrosslinkerType, std::map<int, std::set<int>>> bspotstatus_;

      //! @}
    };

    struct BindEventData
    {
      //! constructor
      BindEventData();

      //! destructor
      virtual ~BindEventData() = default;

      //! Init
      void Init(int clgid, int elegid, int bspotlocn, int requestproc, int permission);


      //!@name data access functions
      //! @{

      int GetClId() const { return clgid_; };

      int GetEleId() const { return elegid_; };

      int GetBSpotLocN() const { return bspotlocn_; };

      int GetRequestProc() const { return requestproc_; };

      int GetPermission() const { return permission_; };

      void SetClId(int clgid) { clgid_ = clgid; };

      void SetEleId(int elegid) { elegid_ = elegid; };

      void SetBSpotLocN(int bspotlocn) { bspotlocn_ = bspotlocn; };

      void SetRequestProc(int requestproc) { requestproc_ = requestproc; };

      void SetPermission(int permission) { permission_ = permission; };

      //! @}

      //!@name methods for enabling parallel use of data container
      //! @{

      /*!
      \brief Pack this class so it can be communicated
      */
      void Pack(Core::Communication::PackBuffer& data) const;

      /*!
      \brief Unpack data from a char vector into this container
      */
      void Unpack(std::vector<char> const& data);

      //! @}

     private:
      //!@name member variables
      //! @{

      // gid of crosslinker
      int clgid_;

      // ele gid crosslinker wants to bind to
      int elegid_;

      // loc number of bspot on ele cl wants to bind to
      int bspotlocn_;

      // myrank, processor that is requesting
      int requestproc_;

      // permission/veto, if crosslinker is allowed to bind
      int permission_;

      //! @}
    };

    struct UnBindEventData
    {
      //! constructor
      UnBindEventData();

      //! destructor
      virtual ~UnBindEventData() = default;

      //!@name data access functions
      //! @{

      int GetClId() const { return clgid_; };

      std::pair<int, int> const& GetEleToUpdate() const { return eletoupdate_; };

      Inpar::BEAMINTERACTION::CrosslinkerType GetLinkerType() const { return linkertype_; };


      void SetClId(int clgid) { clgid_ = clgid; };

      void SetEleToUpdate(std::pair<int, int> eletoupdate) { eletoupdate_ = eletoupdate; };

      void SetLinkerType(Inpar::BEAMINTERACTION::CrosslinkerType linkertype)
      {
        linkertype_ = linkertype;
      };


      //! @}


      //!@name methods for enabling parallel use of data container
      //! @{

      /*!
      \brief Pack this class so it can be communicated
      */
      void Pack(Core::Communication::PackBuffer& data) const;

      /*!
      \brief Unpack data from a char vector into this container
      */
      void Unpack(std::vector<char> const& data);

      //! @}

     private:
      //!@name member variables
      //! @{

      /// crosslinker (gid) that is no longer bonded to above binding spot
      int clgid_;

      /// element gid (first) that needs to be updated at local binding (second)
      std::pair<int, int> eletoupdate_;

      /// type of binding spot where unbinding takes place
      Inpar::BEAMINTERACTION::CrosslinkerType linkertype_;


      //! @}
    };

    struct BspotLinkerData
    {
      //! constructor
      BspotLinkerData();

      //! destructor
      virtual ~BspotLinkerData() = default;

      //!@name data access functions
      //! @{

      int GetEleGid1() const { return elegid_1_; };
      int GetEleGid2() const { return elegid_2_; };

      int GetLocBspotId1() const { return locbspot_1_; };
      int GetLocBspotId2() const { return locbspot_2_; };

      int GetType() const { return type_; };

      int GetMatId() const { return mat_id_; };

      int GetNumberOfBonds1() const { return number_of_bonds_1_; };
      int GetNumberOfBonds2() const { return number_of_bonds_2_; };

      void SetEleGid1(int elegid) { elegid_1_ = elegid; };
      void SetEleGid2(int elegid) { elegid_2_ = elegid; };

      void SetLocBspotId1(int locbspot) { locbspot_1_ = locbspot; };
      void SetLocBspotId2(int locbspot) { locbspot_2_ = locbspot; };

      void set_type(int type) { type_ = type; };

      void SetMatId(int mat_id) { mat_id_ = mat_id; };

      void SetNumberOfBonds1(int number_of_bonds) { number_of_bonds_1_ = number_of_bonds; };
      void SetNumberOfBonds2(int number_of_bonds) { number_of_bonds_2_ = number_of_bonds; };

      bool SameAs(BspotLinkerData bspotlinker);

      //! @}

     private:
      //!@name member variables
      //! @{

      /// element ids
      int elegid_1_;
      int elegid_2_;

      /// binding spot local ids
      int locbspot_1_;
      int locbspot_2_;

      /// crosslinker type
      int type_;

      /// crosslinker mat id
      int mat_id_;

      /// number of bonds
      int number_of_bonds_1_;
      int number_of_bonds_2_;

      /// element

      //! @}
    };

    /// create respective data container from char vector
    template <typename T>
    T* CreateDataContainer(std::vector<char> const& data)
    {
      T* new_container = new T();
      new_container->Unpack(data);
      return new_container;
    };

  }  // namespace Data
}  // namespace BEAMINTERACTION

FOUR_C_NAMESPACE_CLOSE

#endif
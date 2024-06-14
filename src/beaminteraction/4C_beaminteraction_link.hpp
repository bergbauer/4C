/*----------------------------------------------------------------------*/
/*! \file

\brief One beam-to-beam pair (two beam elements) connected by a mechanical link

\level 3

*/
/*----------------------------------------------------------------------*/
#ifndef FOUR_C_BEAMINTERACTION_LINK_HPP
#define FOUR_C_BEAMINTERACTION_LINK_HPP

#include "4C_config.hpp"

#include "4C_comm_parobject.hpp"
#include "4C_comm_parobjectfactory.hpp"
#include "4C_inpar_beaminteraction.hpp"
#include "4C_linalg_fixedsizematrix.hpp"

FOUR_C_NAMESPACE_OPEN

// forward declarations
namespace Core::Communication
{
  class PackBuffer;
}

namespace Discret
{
  namespace ELEMENTS
  {
    class Beam3Base;
    class Beam3r;
  }  // namespace ELEMENTS
}  // namespace Discret

namespace Core::Elements
{
  class Element;
}

namespace Core::LinAlg
{
  class SerialDenseVector;
  class SerialDenseMatrix;
}  // namespace Core::LinAlg

namespace BEAMINTERACTION
{
  class BeamLinkType : public Core::Communication::ParObjectType
  {
   public:
    std::string Name() const override { return "BeamLinkType"; };

    static BeamLinkType& Instance() { return instance_; };

   private:
    static BeamLinkType instance_;
  };


  /*!
   \brief element for interaction of two 3D beam elements via a mechanical linkage
   */
  class BeamLink : public Core::Communication::ParObject
  {
   public:
    //! @name Constructors and destructors and related methods

    //! Constructor
    BeamLink();

    /*!
    \brief Copy Constructor

    Makes a deep copy of a Element

    */
    BeamLink(const BeamLink& old);

    //! Initialization
    virtual void Init(const int id, const std::vector<std::pair<int, int>>& eleids,
        const std::vector<Core::LinAlg::Matrix<3, 1>>& initpos,
        const std::vector<Core::LinAlg::Matrix<3, 3>>& inittriad,
        Inpar::BEAMINTERACTION::CrosslinkerType linkertype, double timelinkwasset);

    //! Setup
    virtual void Setup(const int matnum);

    /*!
    \brief Return unique ParObject id

    Every class implementing ParObject needs a unique id defined at the
    top of parobject.H
    */
    int UniqueParObjectId() const override = 0;

    //! return copy of this linking object object
    virtual Teuchos::RCP<BeamLink> Clone() const = 0;

    /*!
    \brief Pack this class so it can be communicated

    \ref Pack and \ref Unpack are used to communicate this element

    */
    void Pack(Core::Communication::PackBuffer& data) const override;

    /*!
    \brief Unpack data from a char vector into this class

    \ref Pack and \ref Unpack are used to communicate this element

    */
    void Unpack(const std::vector<char>& data) override;

    //@}

    //! @name Access methods

    /*!
    \brief Return global id
    */
    virtual inline int Id() const { return id_; }

    /*!
    \brief Return gid of first/second element (specified via given local element number 0/1)
    */
    virtual inline const int& GetEleGid(const int& elenum) const
    {
      return bspot_ids_[elenum].first;
    }

    /*!
    \brief Return element-local binding spot number of first/second element
           (specified via given local element number 0/1)
    */
    virtual inline const int& GetLocBSpotNum(const int& elenum) const
    {
      return bspot_ids_[elenum].second;
    }

    //! return position of first connection site
    inline const Core::LinAlg::Matrix<3, 1, double>& GetBindSpotPos1() const { return bspotpos1_; }

    //! return position of second connection site
    inline const Core::LinAlg::Matrix<3, 1, double>& GetBindSpotPos2() const { return bspotpos2_; }

    inline Inpar::BEAMINTERACTION::CrosslinkerType GetLinkerType() const { return linkertype_; }

    //! return time at which linker was set
    inline double GetTimeLinkWasSet() const { return timelinkwasset_; }

    //! return time at which linker was set
    inline double GetReferenceLength() const { return reflength_; }

    //! get force in first or second binding spot
    virtual void GetBindingSpotForce(
        int bspotid, Core::LinAlg::SerialDenseVector& bspotforce) const = 0;

    //! get internal linker energy
    virtual double GetInternalEnergy() const = 0;

    //! get kinetic linker energy
    virtual double GetKineticEnergy() const = 0;

    //! scale linker element reference length
    virtual void scale_linker_reference_length(double scalefac) = 0;

    //@}

    //! @name Public evaluation methods

    /*
    \brief Update position and triad of both connection sites (a.k.a. binding spots)
    */
    virtual void ResetState(std::vector<Core::LinAlg::Matrix<3, 1>>& bspotpos,
        std::vector<Core::LinAlg::Matrix<3, 3>>& bspottriad);

    /*!
    \brief Evaluate forces
    */
    virtual bool evaluate_force(
        Core::LinAlg::SerialDenseVector& forcevec1, Core::LinAlg::SerialDenseVector& forcevec2) = 0;

    /*!
    \brief Evaluate stiffness contribution
    */
    virtual bool evaluate_stiff(Core::LinAlg::SerialDenseMatrix& stiffmat11,
        Core::LinAlg::SerialDenseMatrix& stiffmat12, Core::LinAlg::SerialDenseMatrix& stiffmat21,
        Core::LinAlg::SerialDenseMatrix& stiffmat22) = 0;

    /*!
    \brief Evaluate forces and stiffness contribution
    */
    virtual bool evaluate_force_stiff(Core::LinAlg::SerialDenseVector& forcevec1,
        Core::LinAlg::SerialDenseVector& forcevec2, Core::LinAlg::SerialDenseMatrix& stiffmat11,
        Core::LinAlg::SerialDenseMatrix& stiffmat12, Core::LinAlg::SerialDenseMatrix& stiffmat21,
        Core::LinAlg::SerialDenseMatrix& stiffmat22) = 0;


    void Print(std::ostream& out) const;

    //@}

   protected:
    //! returns init state
    inline const bool& is_init() const { return isinit_; };

    //! returns setup state
    inline const bool& is_setup() const { return issetup_; };

    //! Check the init state
    inline void check_init() const
    {
      if (not is_init()) FOUR_C_THROW("Call Init() first!");
    }

    //! Check the init and setup state
    inline void check_init_setup() const
    {
      if (not is_init() or not is_setup()) FOUR_C_THROW("Call Init() and Setup() first!");
    }



   protected:
    //! @name member variables

    //! indicates if the Init() function has been called
    bool isinit_;

    //! indicates if the Setup() function has been called
    bool issetup_;

   private:
    //! a unique global id
    int id_;

    //! unique identifiers for first [0] and second [1] binding spot:
    // each is a pair of element GID and local binding spot number
    std::vector<std::pair<int, int>> bspot_ids_;

    //! current position of the two connection sites (a.k.a. binding spots)
    Core::LinAlg::Matrix<3, 1> bspotpos1_;
    Core::LinAlg::Matrix<3, 1> bspotpos2_;

    //! type of filament element belongs to
    Inpar::BEAMINTERACTION::CrosslinkerType linkertype_;

    //! stores the the time the link was set (can e.g. be used to calculate
    //  lifetime of a link or check if link is new in certain time step)
    double timelinkwasset_;

    //! linker reference length
    double reflength_;

    //@}
  };

}  // namespace BEAMINTERACTION

FOUR_C_NAMESPACE_CLOSE

#endif
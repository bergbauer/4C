/*----------------------------------------------------------------------*/
/*! \file
 \brief

This file contains the material for chemotactic scalars. It derives from MAT_matlist
and adds everything to supervise all the MAT_scatra_chemotaxis materials. The chemotaxation
itself is defined inside the MAT_scatra_chemotaxis materials. So MAT_matlist_chemotaxis
is just a "control instance".


\level 3
*----------------------------------------------------------------------*/

#ifndef FOUR_C_MAT_LIST_CHEMOTAXIS_HPP
#define FOUR_C_MAT_LIST_CHEMOTAXIS_HPP

#include "4C_config.hpp"

#include "4C_comm_parobjectfactory.hpp"
#include "4C_mat_list.hpp"
#include "4C_mat_material_factory.hpp"
#include "4C_material_base.hpp"
#include "4C_material_parameter_base.hpp"

FOUR_C_NAMESPACE_OPEN

namespace Mat
{
  namespace PAR
  {
    /*----------------------------------------------------------------------*/
    /// material parameters for list of materials
    class MatListChemotaxis : public virtual MatList
    {
     public:
      /// standard constructor
      MatListChemotaxis(Teuchos::RCP<Core::Mat::PAR::Material> matdata);

      /// create material instance of matching type with my parameters
      Teuchos::RCP<Core::Mat::Material> create_material() override;

      /// @name material parameters
      //@{

      /// provide ids of the individual pair materials
      const std::vector<int>* PairIds() const { return &pairids_; }

      /// length of pair list
      const int numpair_;

      /// the list of pair IDs
      const std::vector<int> pairids_;

      //@}

    };  // class MatListReactions

  }  // namespace PAR

  class MatListChemotaxisType : public Core::Communication::ParObjectType
  {
   public:
    std::string Name() const override { return "MatListChemotaxisType"; }

    static MatListChemotaxisType& Instance() { return instance_; };

    Core::Communication::ParObject* Create(const std::vector<char>& data) override;

   private:
    static MatListChemotaxisType instance_;
  };

  /*----------------------------------------------------------------------*/
  /// Wrapper for a list of materials
  class MatListChemotaxis : public virtual MatList
  {
   public:
    /// construct empty material object
    MatListChemotaxis();

    /// construct the material object given material parameters
    explicit MatListChemotaxis(Mat::PAR::MatListChemotaxis* params);

    //! @name Packing and Unpacking

    /*!
      \brief Return unique ParObject id

      every class implementing ParObject needs a unique id defined at the
      top of parobject.H (this file) and should return it in this method.
    */
    int UniqueParObjectId() const override
    {
      return MatListChemotaxisType::Instance().UniqueParObjectId();
    }

    /*!
      \brief Pack this class so it can be communicated

      Resizes the vector data and stores all information of a class in it.
      The first information to be stored in data has to be the
      unique parobject id delivered by UniqueParObjectId() which will then
      identify the exact class on the receiving processor.

      \param data (in/out): char vector to store class information
    */
    void Pack(Core::Communication::PackBuffer& data) const override;

    /*!
      \brief Unpack data from a char vector into this class

      The vector data contains all information to rebuild the
      exact copy of an instance of a class on a different processor.
      The first entry in data has to be an integer which is the unique
      parobject id defined at the top of this file and delivered by
      UniqueParObjectId().

      \param data (in) : vector storing all data to be unpacked into this
      instance.
    */
    void Unpack(const std::vector<char>& data) override;

    //@}

    /// material type
    Core::Materials::MaterialType MaterialType() const override
    {
      return Core::Materials::m_matlist_chemotaxis;
    }

    /// return copy of this material object
    Teuchos::RCP<Core::Mat::Material> Clone() const override
    {
      return Teuchos::rcp(new MatListChemotaxis(*this));
    }

    /// number of reactions
    int NumPair() const { return paramschemo_->numpair_; }

    /// reaction ID by Index
    int PairID(const unsigned index) const;

    /// Return quick accessible material parameter data
    Mat::PAR::MatListChemotaxis* Parameter() const override { return paramschemo_; }

   protected:
    /// setup of material map
    virtual void setup_mat_map();

   private:
    /// clear everything
    void clear();

    /// my material parameters
    Mat::PAR::MatListChemotaxis* paramschemo_;
  };

}  // namespace Mat


FOUR_C_NAMESPACE_CLOSE

#endif
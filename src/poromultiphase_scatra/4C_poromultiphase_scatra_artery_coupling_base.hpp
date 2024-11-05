// This file is part of 4C multiphysics licensed under the
// GNU Lesser General Public License v3.0 or later.
//
// See the LICENSE.md file in the top-level for license information.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef FOUR_C_POROMULTIPHASE_SCATRA_ARTERY_COUPLING_BASE_HPP
#define FOUR_C_POROMULTIPHASE_SCATRA_ARTERY_COUPLING_BASE_HPP

#include "4C_config.hpp"

#include "4C_fem_condition.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linalg_vector.hpp"
#include "4C_utils_parameter_list.fwd.hpp"

#include <memory>

FOUR_C_NAMESPACE_OPEN

namespace Core::FE
{
  class Discretization;
}  // namespace Core::FE

namespace PoroMultiPhaseScaTra
{
  //! base class for coupling between artery network and poromultiphasescatra algorithm
  class PoroMultiPhaseScaTraArtCouplBase
  {
   public:
    //! constructor
    PoroMultiPhaseScaTraArtCouplBase(std::shared_ptr<Core::FE::Discretization> arterydis,
        std::shared_ptr<Core::FE::Discretization> contdis,
        const Teuchos::ParameterList& couplingparams, const std::string& condname,
        const std::string& artcoupleddofname, const std::string& contcoupleddofname);

    //! virtual destructor
    virtual ~PoroMultiPhaseScaTraArtCouplBase() = default;

    //! access to full DOF map
    const std::shared_ptr<const Epetra_Map>& full_map() const;

    //! Recompute the CouplingDOFs for each CouplingNode if ntp-coupling active
    void recompute_coupled_do_fs_for_ntp(
        std::vector<Core::Conditions::Condition*> coupcond, unsigned int couplingnode);

    //! get global extractor
    const std::shared_ptr<Core::LinAlg::MultiMapExtractor>& global_extractor() const;

    //! check if initial fields on coupled DOFs are equal
    virtual void check_initial_fields(std::shared_ptr<const Core::LinAlg::Vector<double>> vec_cont,
        std::shared_ptr<const Core::LinAlg::Vector<double>> vec_art) = 0;

    //! access artery (1D) dof row map
    virtual std::shared_ptr<const Epetra_Map> artery_dof_row_map() const = 0;

    //! access full dof row map
    virtual std::shared_ptr<const Epetra_Map> dof_row_map() const = 0;

    //! print out the coupling method
    virtual void print_out_coupling_method() const = 0;

    //! Evaluate the 1D-3D coupling
    virtual void evaluate(std::shared_ptr<Core::LinAlg::BlockSparseMatrixBase> sysmat,
        std::shared_ptr<Core::LinAlg::Vector<double>> rhs) = 0;

    //! set-up of global system of equations of coupled problem
    virtual void setup_system(std::shared_ptr<Core::LinAlg::BlockSparseMatrixBase> sysmat,
        std::shared_ptr<Core::LinAlg::Vector<double>> rhs,
        std::shared_ptr<Core::LinAlg::SparseMatrix> sysmat_cont,
        std::shared_ptr<Core::LinAlg::SparseMatrix> sysmat_art,
        std::shared_ptr<const Core::LinAlg::Vector<double>> rhs_cont,
        std::shared_ptr<const Core::LinAlg::Vector<double>> rhs_art,
        std::shared_ptr<const Core::LinAlg::MapExtractor> dbcmap_cont,
        std::shared_ptr<const Core::LinAlg::MapExtractor> dbcmap_art) = 0;

    //! set solution vectors of single fields
    virtual void set_solution_vectors(
        std::shared_ptr<const Core::LinAlg::Vector<double>> phinp_cont,
        std::shared_ptr<const Core::LinAlg::Vector<double>> phin_cont,
        std::shared_ptr<const Core::LinAlg::Vector<double>> phinp_art);

    //! set the element pairs that are close as found by search algorithm
    virtual void set_nearby_ele_pairs(const std::map<int, std::set<int>>* nearbyelepairs);

    /*!
     * @brief setup global vector
     *
     * @param[out]  vec combined vector containing both artery and continuous field quantities
     * @param[in]   vec_cont vector containing quantities from continuous field
     * @param[in]   vec_art vector containing quantities from artery field
     */
    virtual void setup_vector(std::shared_ptr<Core::LinAlg::Vector<double>> vec,
        std::shared_ptr<const Core::LinAlg::Vector<double>> vec_cont,
        std::shared_ptr<const Core::LinAlg::Vector<double>> vec_art) = 0;

    /*!
     * @brief extract single field vectors
     *
     * @param[out]  globalvec combined vector containing both artery and continuous field quantities
     * @param[in]   vec_cont vector containing quantities from continuous field
     * @param[in]   vec_art vector containing quantities from artery field
     */
    virtual void extract_single_field_vectors(
        std::shared_ptr<const Core::LinAlg::Vector<double>> globalvec,
        std::shared_ptr<const Core::LinAlg::Vector<double>>& vec_cont,
        std::shared_ptr<const Core::LinAlg::Vector<double>>& vec_art) = 0;

    //! init the strategy
    virtual void init() = 0;

    //! setup the strategy
    virtual void setup() = 0;

    //! apply mesh movement (on artery elements)
    virtual void apply_mesh_movement() = 0;

    //! return blood vessel volume fraction inside each 2D/3D element
    virtual std::shared_ptr<const Core::LinAlg::Vector<double>> blood_vessel_volume_fraction() = 0;

   protected:
    //! communicator
    const Epetra_Comm& get_comm() const { return comm_; }

    //! artery (1D) discretization
    std::shared_ptr<Core::FE::Discretization> arterydis_;

    //! continous field (2D, 3D) discretization
    std::shared_ptr<Core::FE::Discretization> contdis_;

    //! coupled dofs of artery field
    std::vector<int> coupleddofs_art_;

    //! coupled dofs of continous field
    std::vector<int> coupleddofs_cont_;

    //! number of coupled dofs
    int num_coupled_dofs_;

    //! dof row map (not splitted)
    std::shared_ptr<Epetra_Map> fullmap_;

    //! global extractor
    std::shared_ptr<Core::LinAlg::MultiMapExtractor> globalex_;

    //! myrank
    const int myrank_;

    /*!
     * @brief decide if artery elements are evaluated in reference configuration
     *
     * so far, it is assumed that artery elements always follow the deformation of the underlying
     * porous medium. Hence, we actually have to evaluate them in current configuration. If this
     * flag is set to true, artery elements will not move and are evaluated in reference
     * configuration
     */
    bool evaluate_in_ref_config_;

   private:
    //! communication (mainly for screen output)
    const Epetra_Comm& comm_;
  };

}  // namespace PoroMultiPhaseScaTra



FOUR_C_NAMESPACE_CLOSE

#endif

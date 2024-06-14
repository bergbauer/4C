/*-----------------------------------------------------------*/
/*! \file

\brief Methods to apply meshtying to fluid and scatra systems


\level 2

*/
/*-----------------------------------------------------------*/

#ifndef FOUR_C_FLUID_MESHTYING_HPP
#define FOUR_C_FLUID_MESHTYING_HPP

#include "4C_config.hpp"

#include "4C_coupling_adapter_mortar.hpp"
#include "4C_inpar_fluid.hpp"
#include "4C_linalg_mapextractor.hpp"

#include <Epetra_Vector.h>
#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN

// forward declarations
namespace Core::FE
{
  class Discretization;
}  // namespace Core::FE

namespace Discret
{
  namespace ELEMENTS
  {
    class FluidEleParameter;
  }
}  // namespace Discret

namespace Mortar
{
  class Interface;
}

namespace Adapter
{
  class CouplingMortar;
}

namespace Core::LinAlg
{
  class Solver;
  struct SolverParams;
  class SparseMatrix;
  class MultiMapExtractor;
  class MapExtractor;
  class BlockSparseMatrixBase;
  class SparseOperator;
  class KrylovProjector;
}  // namespace Core::LinAlg

namespace FLD
{
  namespace UTILS
  {
    class MapExtractor;
    class InterfaceSplitStrategy;
  }  // namespace UTILS
  class Meshtying
  {
    friend class FluidEleParameter;

   public:
    //! Constructor
    Meshtying(Teuchos::RCP<Core::FE::Discretization> dis,       ///> actual discretisation
        Core::LinAlg::Solver& solver,                           ///> solver
        int msht,                                               ///> meshting parameter list
        int nsd,                                                ///> number space dimensions
        const UTILS::MapExtractor* surfacesplitter = nullptr);  ///> surface splitter

    const Epetra_Map* GetMergedMap();
    virtual ~Meshtying() = default;

    //! Set up mesh-tying framework
    void setup_meshtying(const std::vector<int>& coupleddof, const bool pcoupled = true);

    Teuchos::RCP<Core::LinAlg::SparseOperator> init_system_matrix() const;

    //! Applied Dirichlet values are adapted on the slave side of the internal interface
    //! in order to avoid an over-constraint problem setup
    void CheckOverlappingBC(Teuchos::RCP<Epetra_Map> map  ///> map of boundary condition
    );

    //! Old routine handling Dirichlet conditions on the master side of the internal interface
    /// During prepare_time_step() DC are projected from the master to the slave
    void project_master_to_slave_for_overlapping_bc(
        Teuchos::RCP<Epetra_Vector>& velnp,   ///> solution vector n+1
        Teuchos::RCP<const Epetra_Map> bmaps  ///> map of boundary condition
    );

    //! Check whether Dirichlet BC are defined on the master
    void DirichletOnMaster(Teuchos::RCP<const Epetra_Map> bmaps  ///> map of boundary condition
    );

    //! Preparation for including Dirichlet conditions in the condensation process
    void include_dirichlet_in_condensation(
        const Teuchos::RCP<Epetra_Vector>& velnp,  ///> solution vector n+1
        const Teuchos::RCP<Epetra_Vector>& veln    ///> solution vector n
    );

    //! evaluation of matrix P with potential mesh relocation in ALE case
    void evaluate_with_mesh_relocation(
        Teuchos::RCP<Epetra_Vector>& dispnp);  ///> current ALE displacement vector

    //! Prepare matrix, shapederivatives and residual for meshtying
    void PrepareMeshtying(Teuchos::RCP<Core::LinAlg::SparseOperator>&
                              sysmat,  ///> sysmat established by the element routine
        const Teuchos::RCP<Epetra_Vector>&
            residual,                              ///> residual established by the element routine
        const Teuchos::RCP<Epetra_Vector>& velnp,  ///> current ALE displacement vector
        Teuchos::RCP<Core::LinAlg::BlockSparseMatrixBase>&
            shapederivatives);  ///> shapederivatives established by the element routine

    //! Prepare matrix and residual for meshtying
    void prepare_meshtying_system(const Teuchos::RCP<Core::LinAlg::SparseOperator>&
                                      sysmat,  ///> sysmat established by the element routine
        const Teuchos::RCP<Epetra_Vector>&
            residual,                               ///> residual established by the element routine
        const Teuchos::RCP<Epetra_Vector>& velnp);  ///> current ALE displacement vector

    //! The residual has another length in case of bmat_merged --> residual has to be calculated in
    //! split form
    void ApplyPTToResidual(Teuchos::RCP<Core::LinAlg::SparseOperator> sysmat,
        Teuchos::RCP<Epetra_Vector> residual,
        Teuchos::RCP<Core::LinAlg::KrylovProjector> projector);

    //! Solve mesh-tying problem (including ALE case)
    void SolveMeshtying(Core::LinAlg::Solver& solver,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& sysmat,
        const Teuchos::RCP<Epetra_Vector>& incvel, const Teuchos::RCP<Epetra_Vector>& residual,
        const Teuchos::RCP<Epetra_Vector>& velnp, const int itnum,
        Core::LinAlg::SolverParams& solver_params);

    //! Adjust null-space for Krylov projector (slave node are in-active)
    Teuchos::RCP<Epetra_Vector> adapt_krylov_projector(Teuchos::RCP<Epetra_Vector> vec);

    //! Output: maps & projection matrix
    void OutputSetUp();

    //! Output: split sparse matrix
    void output_sparse_matrix_split(Teuchos::RCP<Core::LinAlg::SparseOperator> conmat);

    //! Output: single blocks of the block matrix
    void OutputBlockMatrix(Teuchos::RCP<Core::LinAlg::SparseOperator> blockmatrix,
        Teuchos::RCP<Epetra_Vector> residual);

    //! Output: split vector
    void OutputVectorSplit(Teuchos::RCP<Epetra_Vector> vector);

    //! Analyze system matrix
    void AnalyzeMatrix(
        Teuchos::RCP<Core::LinAlg::SparseMatrix> sparsematrix);  ///> sparse matrix to analyze

    //! Replace matrix entries
    /// Replace computed identity matrix by a real identity matrix
    void replace_matrix_entries(
        Teuchos::RCP<Core::LinAlg::SparseMatrix> sparsematrix);  ///> sparse matrix to analyze

    //! Compute and update the increments of the slave node (including ALE case)
    void UpdateSlaveDOF(
        const Teuchos::RCP<Epetra_Vector>& inc, const Teuchos::RCP<Epetra_Vector>& velnp);

    //! Set the flag for multifield problems
    void IsMultifield(Teuchos::RCP<std::set<int>> condelements,  ///< conditioned elements of fluid
        const Core::LinAlg::MultiMapExtractor&
            domainmaps,  ///< domain maps for split of fluid matrix
        const Core::LinAlg::MultiMapExtractor& rangemaps,  ///< range maps for split of fluid matrix
        Teuchos::RCP<std::set<int>> condelements_shape,    ///< conditioned elements
        const Core::LinAlg::MultiMapExtractor&
            domainmaps_shape,  ///< domain maps for split of shape deriv. matrix
        const Core::LinAlg::MultiMapExtractor&
            rangemaps_shape,  ///< domain maps for split of shape deriv. matrix
        bool splitmatrix,     ///< flag for split of matrices
        bool ismultifield     ///< flag for multifield problems
    );

    //! Use the split of the fluid mesh tying for the sysmat
    void MshtSplit(Teuchos::RCP<Core::LinAlg::SparseOperator>& sysmat,
        Teuchos::RCP<Core::LinAlg::BlockSparseMatrixBase>& shapederivatives);

    //! Use the split of the fluid mesh tying for the shape derivatives
    void MshtSplitShape(Teuchos::RCP<Core::LinAlg::BlockSparseMatrixBase>& shapederivatives);

    //! Use the split of the multifield problem for the sysmat
    void MultifieldSplit(Teuchos::RCP<Core::LinAlg::SparseOperator>& sysmat);

    //! Use the split of the multifield problem for the shape derivatives
    void multifield_split_shape(
        Teuchos::RCP<Core::LinAlg::BlockSparseMatrixBase>& shapederivatives);

    //! Prepare condensation of the shape derivatives
    void condensation_operation_block_matrix_shape(
        Teuchos::RCP<Core::LinAlg::BlockSparseMatrixBase>& shapederivatives);

   private:
    //! Prepare condensation for sparse matrix (including ALE case)
    void condensation_sparse_matrix(const Teuchos::RCP<Core::LinAlg::SparseOperator>&
                                        sysmat,  ///> sysmat established by the element routine
        const Teuchos::RCP<Epetra_Vector>&
            residual,  ///> residual established by the element routine
        const Teuchos::RCP<Epetra_Vector>& velnp);

    //! Prepare condensation for a block matrix (including ALE case)
    void condensation_block_matrix(const Teuchos::RCP<Core::LinAlg::SparseOperator>&
                                       sysmat,  ///> sysmat established by the element routine
        const Teuchos::RCP<Epetra_Vector>&
            residual,                               ///> residual established by the element routine
        const Teuchos::RCP<Epetra_Vector>& velnp);  ///> current velocity vector

    //! split sparse global system matrix into 3x3 block sparse matrix associated with interior,
    //! master, and slave dofs
    void split_matrix(Teuchos::RCP<Core::LinAlg::SparseOperator>
                          matrix,  //!< original sparse global system matrix before split
        Teuchos::RCP<Core::LinAlg::BlockSparseMatrixBase>&
            splitmatrix  //!< resulting block sparse matrix after split
    );

    //! Split vector and save parts in a std::vector<Teuchos::RCP<Epetra_Vector> >
    void split_vector(Teuchos::RCP<Epetra_Vector> vector,        ///> vector to split
        std::vector<Teuchos::RCP<Epetra_Vector>>& splitvector);  ///> container for the split vector

    //! Split vector and save parts in a std::vector<Teuchos::RCP<Epetra_Vector> >
    void split_vector_based_on3x3(
        Teuchos::RCP<Epetra_Vector> orgvector,  ///> original vector based on 3x3 blockmatrix
        Teuchos::RCP<Epetra_Vector> vectorbasedon2x2);  ///> split vector based on 2x2 blockmatrix

    //! Condensation operation for a sparse matrix (including ALE case):
    /// the sysmat is manipulated via a second sparse matrix
    /// Assembling is slower, since the graph cannot be saved
    void condensation_operation_sparse_matrix(
        const Teuchos::RCP<Core::LinAlg::SparseOperator>&
            sysmat,  ///> sysmat established by the element routine
        const Teuchos::RCP<Epetra_Vector>&
            residual,  ///> residual established by the element routine
        const Teuchos::RCP<Core::LinAlg::BlockSparseMatrixBase>&
            splitmatrix,  ///> container with split original sysmat
        const std::vector<Teuchos::RCP<Epetra_Vector>>&
            splitres,  ///> container with split original residual
        const std::vector<Teuchos::RCP<Epetra_Vector>>&
            splitvel  ///> container with split velocity vector
    );

    //! Condensation operation for a block matrix (including ALE case):
    /// the original blocks (nn, nm, mn, mm) are manipulated directly;
    /// the remaining blocks (ns, ms, ss, sn, sm) are not touched at all,
    /// since finally a 2x2 block matrix is solved
    void condensation_operation_block_matrix(
        const Teuchos::RCP<Core::LinAlg::SparseOperator>&
            sysmat,  ///> sysmat established by the element routine
        const Teuchos::RCP<Epetra_Vector>&
            residual,  ///> residual established by the element routine
        const std::vector<Teuchos::RCP<Epetra_Vector>>&
            splitres,  ///> container with split original residual
        const std::vector<Teuchos::RCP<Epetra_Vector>>&
            splitvel);  ///> container with split velocity vector

   private:
    //! discretisation
    Teuchos::RCP<Core::FE::Discretization> discret_;

    Core::LinAlg::Solver& solver_;  // standard solver object

    //! meshting options
    /// 0: no_meshtying     -> no mesh-tying
    /// 0: condensed_smat   -> condensation in a sparse matrix
    /// 1: condensed_bmat   -> condensation in a block matrix
    /// 2: condensed_bmat_merged   -> condensation in a block matrix

    /// deactivated:
    /// 3: sps_coupled      -> saddle point system in a sparse matrix
    /// 4: sps_pc           -> saddle point system in a block matrix
    int msht_;

    //! the processor ID from the communicator
    int myrank_;

    // interface splitter
    const UTILS::MapExtractor* surfacesplitter_;

    //! dof row map of the complete system
    const Epetra_Map* dofrowmap_;

    //! dof row map of the complete system
    Teuchos::RCP<Epetra_Map> problemrowmap_;

    //! dof rowmap of all nodes, which are not on the interface
    Teuchos::RCP<Epetra_Map> gndofrowmap_;

    //! slave & master dof rowmap
    Teuchos::RCP<Epetra_Map> gsmdofrowmap_;

    //! slave dof rowmap
    Teuchos::RCP<const Epetra_Map> gsdofrowmap_;

    //! master dof rowmap
    Teuchos::RCP<const Epetra_Map> gmdofrowmap_;

    //! merged map for saddle point system and 2x2 block matrix
    Teuchos::RCP<Epetra_Map> mergedmap_;

    //! vector containing time-depending values of the dirichlet condition
    /// valuesdc_ = (velnp after applying DC) - (veln)
    Teuchos::RCP<Epetra_Vector> valuesdc_;

    //! adapter to mortar framework
    Teuchos::RCP<Core::Adapter::CouplingMortar> adaptermeshtying_;

    //! 2x2 (3x3) block matrix for solving condensed system (3x3 block matrix)
    Teuchos::RCP<Core::LinAlg::SparseOperator> sysmatsolve_;
    Teuchos::RCP<Epetra_Vector> residual_;
    //! flag defining pressure coupling
    bool pcoupled_;

    //! flag defining if Dirichlet  or Dirichlet-like boundary conditions are defined on the master
    //! side of the internal interface
    bool dconmaster_;

    //! flag for identifying first Newton iteration in each time step
    bool firstnonliniter_;

    //! number of space dimensions
    int nsd_;

    //! conditioned elements of fluid in multifield simulation
    Teuchos::RCP<std::set<int>> multifield_condelements_;

    //! domain maps for split of fluid matrix in multifield simulation
    Core::LinAlg::MultiMapExtractor multifield_domainmaps_;

    //! range maps for split of fluid matrix in multifield simulation
    Core::LinAlg::MultiMapExtractor multifield_rangemaps_;

    //! conditioned elements in multifield simulation
    Teuchos::RCP<std::set<int>> multifield_condelements_shape_;

    //! domain maps for split of shape deriv. matrix in multifield simulation
    Core::LinAlg::MultiMapExtractor multifield_domainmaps_shape_;

    //! domain maps for split of shape deriv. matrix in multifield simulation
    Core::LinAlg::MultiMapExtractor multifield_rangemaps_shape_;

    //! flag for split of matrices in multifield simulation
    bool multifield_splitmatrix_;

    //! flag for multifield problems in multifield simulation
    bool is_multifield_;

  };  // end  class Meshtying
}  // end namespace FLD

FOUR_C_NAMESPACE_CLOSE

#endif
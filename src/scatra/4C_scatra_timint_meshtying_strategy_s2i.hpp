/*----------------------------------------------------------------------*/
/*! \file

\brief Scatra-scatra interface coupling strategy for standard scalar transport problems

\level 2


*----------------------------------------------------------------------*/
#ifndef FOUR_C_SCATRA_TIMINT_MESHTYING_STRATEGY_S2I_HPP
#define FOUR_C_SCATRA_TIMINT_MESHTYING_STRATEGY_S2I_HPP

#include "4C_config.hpp"

#include "4C_coupling_adapter.hpp"
#include "4C_coupling_adapter_mortar.hpp"
#include "4C_fem_condition.hpp"
#include "4C_fem_general_element.hpp"
#include "4C_fem_general_utils_local_connectivity_matrices.hpp"
#include "4C_inpar_s2i.hpp"
#include "4C_inpar_scatra.hpp"
#include "4C_io_runtime_csv_writer.hpp"
#include "4C_scatra_timint_meshtying_strategy_base.hpp"

#include <Epetra_FEVector.h>
#include <Epetra_IntVector.h>

FOUR_C_NAMESPACE_OPEN

// forward declarations
namespace Adapter
{
  class Coupling;
  class CouplingMortar;
}  // namespace Adapter

namespace Discret
{
  namespace ELEMENTS
  {
    class ScaTraEleParameterBoundary;
  }

}  // namespace Discret

namespace Core::FE
{
  template <const int NSD>
  class IntPointsAndWeights;
}

namespace Core::LinAlg
{
  class MatrixColTransform;
  class MatrixRowTransform;
  class MatrixRowColTransform;
  class BlockSparseMatrixBase;
  class MapExtractor;
  class MultiMapExtractor;
  class SparseMatrix;
  class Equilibration;
  enum class MatrixType;
}  // namespace Core::LinAlg

namespace Mortar
{
  class IntCell;
  class Element;
  class Node;
}  // namespace Mortar

namespace ScaTra
{
  // forward declaration
  class MortarCellAssemblyStrategy;

  /*!
  \brief Scatra-scatra interface coupling strategy for standard scalar transport problems

  To keep the scalar transport time integrator class and derived classes as plain as possible,
  several algorithmic parts have been encapsulated within separate meshtying strategy classes.
  These algorithmic parts include initializing the system matrix and other relevant objects,
  computing meshtying residual terms and their linearizations, and solving the resulting
  linear system of equations. By introducing a hierarchy of strategies for these algorithmic
  parts, a bunch of unhandy if-else selections within the time integrator classes themselves
  can be circumvented. This class contains the scatra-scatra interface coupling strategy for
  standard scalar transport problems.

  */

  class MeshtyingStrategyS2I : public MeshtyingStrategyBase
  {
   public:
    //! constructor
    explicit MeshtyingStrategyS2I(
        ScaTra::ScaTraTimIntImpl* scatratimint,  //!< scalar transport time integrator
        const Teuchos::ParameterList&
            parameters  //!< input parameters for scatra-scatra interface coupling
    );

    //! provide global state vectors for element evaluation
    void add_time_integration_specific_vectors() const override;

    //! compute time step size
    void compute_time_step_size(double& dt) override;

    //! return map extractor associated with blocks of auxiliary system matrix for master side
    const Core::LinAlg::MultiMapExtractor& BlockMapsMaster() const { return *blockmaps_master_; };

    //! return map extractor associated with blocks of auxiliary system matrix for slave side
    const Core::LinAlg::MultiMapExtractor& BlockMapsSlave() const { return *blockmaps_slave_; };

    //! compute time derivatives of discrete state variables
    void compute_time_derivative() const override;

    void CondenseMatAndRHS(const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix,
        const Teuchos::RCP<Epetra_Vector>& residual,
        const bool calcinittimederiv = false) const override;

    //! return interface coupling adapter
    Teuchos::RCP<const Core::Adapter::Coupling> CouplingAdapter() const { return icoup_; };

    //! return flag for meshtying method
    const Inpar::S2I::CouplingType& CouplingType() const { return couplingtype_; }

    //! return global map of degrees of freedom
    const Epetra_Map& dof_row_map() const override;

    //! compute meshtying residual terms and their linearizations
    void EvaluateMeshtying() override;

    /*!
     * @brief  evaluate mortar integration cells
     *
     * @param idiscret  interface discretization
     * @param params    parameter list for evaluation of mortar integration cells
     * @param strategy  assembly strategy for mortar integration cells
     */
    void evaluate_mortar_cells(const Core::FE::Discretization& idiscret,
        const Teuchos::ParameterList& params, ScaTra::MortarCellAssemblyStrategy& strategy) const;

    //! explicit predictor step to obtain better starting value for Newton-Raphson iteration
    void explicit_predictor() const override;

    //! extract selected rows from a sparse matrix
    static void ExtractMatrixRows(const Core::LinAlg::SparseMatrix& matrix,  //!< source matrix
        Core::LinAlg::SparseMatrix& rows,                                    //!< destination matrix
        const Epetra_Map& rowmap  //!< map of matrix rows to be extracted
    );

    /*!
     * @brief finite difference check for extended system matrix involving scatra-scatra interface
     * layer growth (for debugging only)
     *
     * @param extendedsystemmatrix  global system matrix
     * @param extendedresidual      global residual vector
     */
    void fd_check(const Core::LinAlg::BlockSparseMatrixBase& extendedsystemmatrix,
        const Teuchos::RCP<Epetra_Vector>& extendedresidual) const;

    //! return state vector of discrete scatra-scatra interface layer thicknesses at time n
    const Teuchos::RCP<Epetra_Vector>& GrowthVarN() const { return growthn_; };

    //! return state vector of discrete scatra-scatra interface layer thicknesses at time n+1
    const Teuchos::RCP<Epetra_Vector>& GrowthVarNp() const { return growthnp_; };

    //! perform initialization of scatra-scatra interface coupling
    void InitMeshtying() override;

    bool system_matrix_initialization_needed() const override { return false; }

    Teuchos::RCP<Core::LinAlg::SparseOperator> init_system_matrix() const override
    {
      FOUR_C_THROW(
          "This meshtying strategy does not need to initialize the system matrix, but relies "
          "instead on the initialization of the field. If this changes, you also need to change "
          "'system_matrix_initialization_needed()' to return true");
      // dummy return
      return Teuchos::null;
    }

    //! return interface map extractor
    Teuchos::RCP<Core::LinAlg::MultiMapExtractor> InterfaceMaps() const override
    {
      return interfacemaps_;
    }

    //! return flag for evaluation of scatra-scatra interface coupling involving interface layer
    //! growth
    const Inpar::S2I::GrowthEvaluation& int_layer_growth_evaluation() const
    {
      return intlayergrowth_evaluation_;
    };

    //! return the slave-side scatra-scatra interface kinetics conditions applied to a mesh tying
    //! interface
    const std::map<const int, Core::Conditions::Condition* const>&
    kinetics_conditions_meshtying_slave_side() const
    {
      return kinetics_conditions_meshtying_slaveside_;
    }

    //! corresponding master conditions to kinetics condiditions
    std::map<const int, Core::Conditions::Condition* const>& MasterConditions()
    {
      return master_conditions_;
    }

    //! return vector of Lagrange multiplier dofs
    Teuchos::RCP<const Epetra_Vector> LM() const { return lm_; };

    //! return constraint residual vector associated with Lagrange multiplier dofs
    Teuchos::RCP<const Epetra_Vector> LMResidual() const { return lmresidual_; };

    //! return constraint increment vector associated with Lagrange multiplier dofs
    Teuchos::RCP<const Epetra_Vector> LMIncrement() const { return lmincrement_; };

    //! return auxiliary system matrix for linearizations of slave fluxes w.r.t. master dofs
    const Teuchos::RCP<Core::LinAlg::SparseMatrix>& MasterMatrix() const { return imastermatrix_; };

    //! return type of global system matrix in global system of equations
    const Core::LinAlg::MatrixType& MatrixType() const { return matrixtype_; };

    //! return mortar interface discretization associated with particular condition ID
    Core::FE::Discretization& mortar_discretization(const int& condid) const;

    //! output solution for post-processing
    void Output() const override;

    void write_restart() const override;

    //! return mortar projector P
    const Teuchos::RCP<Core::LinAlg::SparseMatrix>& P() const { return P_; };

    void read_restart(
        const int step, Teuchos::RCP<Core::IO::InputControl> input = Teuchos::null) const override;

    //! set general parameters for element evaluation
    void set_element_general_parameters(Teuchos::ParameterList& parameters) const override;

    /*!
     * \brief Method sets the scatra-scatra interface condition specific values to the scatra
     * element interface condition.
     *
     * \note Parameters are stored to the parameter class using the evaluate call at the end of this
     * method.
     *
     * @param[in] s2icondition Scatra-scatra interface condition of which parameters are read and
     * stored to the parameter class
     */
    void set_condition_specific_sca_tra_parameters(Core::Conditions::Condition& s2icondition) const;

    /*!
     * \brief Writes S2IKinetics condition specific parameters to parameter list that is stored to
     * the boundary parameter class afterwards
     *
     * @param[in]  s2ikinetics_cond       ScaTra-ScaTra interface condition whose parameters are
     *                                    stored to the parameter list
     * @param[out] s2icouplingparameters  parameter list filled with condition specific parameters
     */
    static void write_s2_i_kinetics_specific_sca_tra_parameters_to_parameter_list(
        Core::Conditions::Condition& s2ikinetics_cond,
        Teuchos::ParameterList& s2icouplingparameters);

    //! compute history vector, i.e., the history part of the right-hand side vector with all
    //! contributions from the previous time step
    void SetOldPartOfRHS() const override;

    //! perform setup of scatra-scatra interface coupling
    void setup_meshtying() override;

    //! return auxiliary system matrix for linearizations of slave fluxes w.r.t. slave dofs
    //! (non-mortar case) or slave and master dofs (mortar case)
    const Teuchos::RCP<Core::LinAlg::SparseMatrix>& SlaveMatrix() const { return islavematrix_; };

    void Solve(const Teuchos::RCP<Core::LinAlg::Solver>& solver,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix,
        const Teuchos::RCP<Epetra_Vector>& increment, const Teuchos::RCP<Epetra_Vector>& residual,
        const Teuchos::RCP<Epetra_Vector>& phinp, const int iteration,
        Core::LinAlg::SolverParams& solver_params) const override;

    //! return linear solver for global system of linear equations
    const Core::LinAlg::Solver& Solver() const override;

    //! update solution after convergence of the nonlinear Newton-Raphson iteration
    void Update() const override;

    //! write integrated interface flux on slave side of s2i kintetics condition to csv file
    void OutputInterfaceFlux() const;

   protected:
    void equip_extended_solver_with_null_space_info() const override;

    //! instantiate strategy for Newton-Raphson convergence check
    void init_conv_check_strategy() override;

    //! interface map extractor (0: other, 1: slave, 2: master)
    Teuchos::RCP<Core::LinAlg::MultiMapExtractor> interfacemaps_;

    //! map extractor associated with scatra-scatra interface slave-side blocks of global system
    //! matrix
    Teuchos::RCP<Core::LinAlg::MultiMapExtractor> blockmaps_slave_;
    //! map extractor associated with scatra-scatra interface master-side blocks of global system
    //! matrix
    Teuchos::RCP<Core::LinAlg::MultiMapExtractor> blockmaps_master_;

    //! non-mortar interface coupling adapter
    Teuchos::RCP<Core::Adapter::Coupling> icoup_;

    //! mortar interface coupling adapters
    std::map<int, Teuchos::RCP<Core::Adapter::CouplingMortar>> icoupmortar_;

    //! mortar integration cells
    std::map<int, std::vector<std::pair<Teuchos::RCP<Mortar::IntCell>, Inpar::ScaTra::ImplType>>>
        imortarcells_;

    //! flag for parallel redistribution of mortar interfaces
    const bool imortarredistribution_;

    //! map of all slave-side degrees of freedom before parallel redistribution
    Teuchos::RCP<Epetra_Map> islavemap_;

    //! map of all master-side degrees of freedom before parallel redistribution
    Teuchos::RCP<Epetra_Map> imastermap_;

    //! vectors for node-to-segment connectivity, i.e., for pairings between slave nodes and master
    //! elements
    std::map<int, Teuchos::RCP<Epetra_IntVector>> islavenodestomasterelements_;

    //! vectors for physical implementation types of slave-side nodes
    std::map<int, Teuchos::RCP<Epetra_IntVector>> islavenodesimpltypes_;

    //! vectors for lumped interface area fractions associated with slave-side nodes
    std::map<int, Teuchos::RCP<Epetra_Vector>> islavenodeslumpedareas_;

    //! auxiliary system matrix for linearizations of slave fluxes w.r.t. slave dofs (non-mortar
    //! case) or for linearizations of slave fluxes w.r.t. slave and master dofs (mortar case)
    Teuchos::RCP<Core::LinAlg::SparseMatrix> islavematrix_;

    //! auxiliary system matrix for linearizations of slave fluxes w.r.t. master dofs (non-mortar
    //! case) or for linearizations of master fluxes w.r.t. slave and master dofs (mortar case)
    Teuchos::RCP<Core::LinAlg::SparseMatrix> imastermatrix_;

    //! auxiliary system matrix for linearizations of master fluxes w.r.t. slave dofs
    Teuchos::RCP<Core::LinAlg::SparseMatrix> imasterslavematrix_;

    //! flag for meshtying method
    const Inpar::S2I::CouplingType couplingtype_;

    //! mortar matrix D
    Teuchos::RCP<Core::LinAlg::SparseMatrix> D_;

    //! mortar matrix M
    Teuchos::RCP<Core::LinAlg::SparseMatrix> M_;

    //! mortar matrix E
    Teuchos::RCP<Core::LinAlg::SparseMatrix> E_;

    //! mortar projector P
    Teuchos::RCP<Core::LinAlg::SparseMatrix> P_;

    //! mortar projector Q
    Teuchos::RCP<Core::LinAlg::SparseMatrix> Q_;

    //! vector of Lagrange multiplier dofs
    Teuchos::RCP<Epetra_Vector> lm_;

    //! extended map extractor (0: standard dofs, 1: Lagrange multiplier dofs or scatra-scatra
    //! interface layer thickness variables)
    Teuchos::RCP<Core::LinAlg::MapExtractor> extendedmaps_;

    //! constraint residual vector associated with Lagrange multiplier dofs
    Teuchos::RCP<Epetra_Vector> lmresidual_;

    //! constraint increment vector associated with Lagrange multiplier dofs
    Teuchos::RCP<Epetra_Vector> lmincrement_;

    //! transformation operators for auxiliary system matrices
    Teuchos::RCP<Core::LinAlg::MatrixColTransform> islavetomastercoltransform_;
    Teuchos::RCP<Core::LinAlg::MatrixRowTransform> islavetomasterrowtransform_;
    Teuchos::RCP<Core::LinAlg::MatrixRowColTransform> islavetomasterrowcoltransform_;

    //! auxiliary residual vector for slave residuals
    Teuchos::RCP<Epetra_Vector> islaveresidual_;

    //! auxiliary residual vector for master residuals
    Teuchos::RCP<Epetra_FEVector> imasterresidual_;

    //! time derivative of slave dofs of scatra-scatra interface
    Teuchos::RCP<Epetra_Vector> islavephidtnp_;

    //! time derivative of master dofs transformed to slave side of scatra-scatra interface
    Teuchos::RCP<Epetra_Vector> imasterphidt_on_slave_side_np_;

    //! master dofs transformed to slave side of scatra-scatra interface
    Teuchos::RCP<Epetra_Vector> imasterphi_on_slave_side_np_;

    //! flag for interface side underlying Lagrange multiplier definition
    const Inpar::S2I::InterfaceSides lmside_;

    //! type of global system matrix in global system of equations
    const Core::LinAlg::MatrixType matrixtype_;

    //! node-to-segment projection tolerance
    const double ntsprojtol_;

    //! flag for evaluation of scatra-scatra interface coupling involving interface layer growth
    const Inpar::S2I::GrowthEvaluation intlayergrowth_evaluation_;

    //! local Newton-Raphson convergence tolerance for scatra-scatra interface coupling involving
    //! interface layer growth
    const double intlayergrowth_convtol_;

    //! maximum number of local Newton-Raphson iterations for scatra-scatra interface coupling
    //! involving interface layer growth
    const unsigned intlayergrowth_itemax_;

    //! modified time step size for scatra-scatra interface coupling involving interface layer
    //! growth
    const double intlayergrowth_timestep_;

    //! map extractor associated with all degrees of freedom for scatra-scatra interface layer
    //! growth
    Teuchos::RCP<const Core::LinAlg::MultiMapExtractor> blockmapgrowth_;

    //! extended map extractor associated with blocks of global system matrix for scatra-scatra
    //! interface coupling involving interface layer growth
    Teuchos::RCP<const Core::LinAlg::MultiMapExtractor> extendedblockmaps_;

    //! extended system matrix including rows and columns associated with scatra-scatra interface
    //! layer thickness variables
    Teuchos::RCP<Core::LinAlg::BlockSparseMatrixBase> extendedsystemmatrix_;

    //! linear solver for monolithic scatra-scatra interface coupling involving interface layer
    //! growth
    Teuchos::RCP<Core::LinAlg::Solver> extendedsolver_;

    //! state vector of discrete scatra-scatra interface layer thicknesses at time n
    Teuchos::RCP<Epetra_Vector> growthn_;

    //! state vector of discrete scatra-scatra interface layer thicknesses at time n+1
    Teuchos::RCP<Epetra_Vector> growthnp_;

    //! state vector of time derivatives of discrete scatra-scatra interface layer thicknesses at
    //! time n
    Teuchos::RCP<Epetra_Vector> growthdtn_;

    //! state vector of time derivatives of discrete scatra-scatra interface layer thicknesses at
    //! time n+1
    Teuchos::RCP<Epetra_Vector> growthdtnp_;

    //! state vector of history values associated with discrete scatra-scatra interface layer
    //! thicknesses
    Teuchos::RCP<Epetra_Vector> growthhist_;

    //! state vector of residual values associated with discrete scatra-scatra interface layer
    //! thicknesses
    Teuchos::RCP<Epetra_Vector> growthresidual_;

    //! state vector of Newton-Raphson increment values associated with discrete scatra-scatra
    //! interface layer thicknesses
    Teuchos::RCP<Epetra_Vector> growthincrement_;

    //! scatra-growth block of extended global system matrix (derivatives of discrete scatra
    //! residuals w.r.t. discrete scatra-scatra interface layer thicknesses)
    Teuchos::RCP<Core::LinAlg::SparseOperator> scatragrowthblock_;

    //! growth-scatra block of extended global system matrix (derivatives of discrete scatra-scatra
    //! interface layer growth residuals w.r.t. discrete scatra degrees of freedom)
    Teuchos::RCP<Core::LinAlg::SparseOperator> growthscatrablock_;

    //! growth-growth block of extended global system matrix (derivatives of discrete scatra-scatra
    //! interface layer growth residuals w.r.t. discrete scatra-scatra interface layer thicknesses)
    Teuchos::RCP<Core::LinAlg::SparseMatrix> growthgrowthblock_;

    //! all equilibration of global system matrix and RHS is done in here
    Teuchos::RCP<Core::LinAlg::Equilibration> equilibration_;

    //! output csv writer for interface flux for each slave side s2i condition
    std::optional<Core::IO::RuntimeCsvWriter> runtime_csvwriter_;

    //! write integrated interface flux on slave side of s2i kintetics condition to csv file
    const bool output_interface_flux_;

   private:
    //! copy constructor
    MeshtyingStrategyS2I(const MeshtyingStrategyS2I& old);

    //! build map extractors associated with blocks of global system matrix
    void build_block_map_extractors();

    //! evaluate and assemble all contributions due to capacitive fluxes at the scatra-scatra
    //! interface
    void evaluate_and_assemble_capacitive_contributions();

    /*!
     * @brief  evaluate single mortar integration cell
     *
     * @param idiscret       interface discretization
     * @param cell           mortar integration cell
     * @param impltype       physical implementation type of mortar integration cell
     * @param slaveelement   slave-side mortar element
     * @param masterelement  master-side mortar element
     * @param la_slave       slave-side location array
     * @param la_master      master-side location array
     * @param params         parameter list
     * @param cellmatrix1    cell matrix 1
     * @param cellmatrix2    cell matrix 2
     * @param cellmatrix3    cell matrix 3
     * @param cellmatrix4    cell matrix 4
     * @param cellvector1    cell vector 1
     * @param cellvector2    cell vector 2
     */
    void evaluate_mortar_cell(const Core::FE::Discretization& idiscret, Mortar::IntCell& cell,
        const Inpar::ScaTra::ImplType& impltype, Mortar::Element& slaveelement,
        Mortar::Element& masterelement, Core::Elements::Element::LocationArray& la_slave,
        Core::Elements::Element::LocationArray& la_master, const Teuchos::ParameterList& params,
        Core::LinAlg::SerialDenseMatrix& cellmatrix1, Core::LinAlg::SerialDenseMatrix& cellmatrix2,
        Core::LinAlg::SerialDenseMatrix& cellmatrix3, Core::LinAlg::SerialDenseMatrix& cellmatrix4,
        Core::LinAlg::SerialDenseVector& cellvector1,
        Core::LinAlg::SerialDenseVector& cellvector2) const;

    /*!
     * @brief  evaluate single slave-side node for node-to-segment coupling
     *
     * @param idiscret       interface discretization
     * @param slavenode      slave-side node
     * @param lumpedarea     lumped interface area fraction associated with slave-side node
     * @param impltype       physical implementation type of mortar integration cell
     * @param slaveelement   slave-side mortar element
     * @param masterelement  master-side mortar element
     * @param la_slave       slave-side location array
     * @param la_master      master-side location array
     * @param params         parameter list
     * @param ntsmatrix1     node-to-segment matrix 1
     * @param ntsmatrix2     node-to-segment matrix 2
     * @param ntsmatrix3     node-to-segment matrix 3
     * @param ntsmatrix4     node-to-segment matrix 4
     * @param ntsvector1     node-to-segment vector 1
     * @param ntsvector2     node-to-segment vector 2
     */
    void evaluate_slave_node(const Core::FE::Discretization& idiscret,
        const Mortar::Node& slavenode, const double& lumpedarea,
        const Inpar::ScaTra::ImplType& impltype, Mortar::Element& slaveelement,
        Mortar::Element& masterelement, Core::Elements::Element::LocationArray& la_slave,
        Core::Elements::Element::LocationArray& la_master, const Teuchos::ParameterList& params,
        Core::LinAlg::SerialDenseMatrix& ntsmatrix1, Core::LinAlg::SerialDenseMatrix& ntsmatrix2,
        Core::LinAlg::SerialDenseMatrix& ntsmatrix3, Core::LinAlg::SerialDenseMatrix& ntsmatrix4,
        Core::LinAlg::SerialDenseVector& ntsvector1,
        Core::LinAlg::SerialDenseVector& ntsvector2) const;

    /*!
     * @brief  evaluate single mortar element
     *
     * @param idiscret   interface discretization
     * @param element    mortar element
     * @param impltype   physical implementation type of mortar element
     * @param la         location array
     * @param params     parameter list
     * @param elematrix1 element matrix 1
     * @param elematrix2 element matrix 2
     * @param elematrix3 element matrix 3
     * @param elematrix4 element matrix 4
     * @param elevector1 element vector 1
     * @param elevector2 element vector 2
     */
    void evaluate_mortar_element(const Core::FE::Discretization& idiscret, Mortar::Element& element,
        const Inpar::ScaTra::ImplType& impltype, Core::Elements::Element::LocationArray& la,
        const Teuchos::ParameterList& params, Core::LinAlg::SerialDenseMatrix& elematrix1,
        Core::LinAlg::SerialDenseMatrix& elematrix2, Core::LinAlg::SerialDenseMatrix& elematrix3,
        Core::LinAlg::SerialDenseMatrix& elematrix4, Core::LinAlg::SerialDenseVector& elevector1,
        Core::LinAlg::SerialDenseVector& elevector2) const;

    /*!
     * @brief  evaluate mortar integration cells
     *
     * @param idiscret           interface discretization
     * @param params             parameter list for evaluation of mortar integration cells
     * @param systemmatrix1      system matrix 1
     * @param matrix1_side_rows  interface side associated with rows of system matrix 1
     * @param matrix1_side_cols  interface side associated with columns of system matrix 1
     * @param systemmatrix2      system matrix 2
     * @param matrix2_side_rows  interface side associated with rows of system matrix 2
     * @param matrix2_side_cols  interface side associated with columns of system matrix 2
     * @param systemmatrix3      system matrix 3
     * @param matrix3_side_rows  interface side associated with rows of system matrix 3
     * @param matrix3_side_cols  interface side associated with columns of system matrix 3
     * @param systemmatrix4      system matrix 4
     * @param matrix4_side_rows  interface side associated with rows of system matrix 4
     * @param matrix4_side_cols  interface side associated with columns of system matrix 4
     * @param systemvector1      system vector 1
     * @param vector1_side       interface side associated with system vector 1
     * @param systemvector2      system vector 2
     * @param vector2_side       interface side associated with system vector 2
     */
    void evaluate_mortar_cells(const Core::FE::Discretization& idiscret,
        const Teuchos::ParameterList& params,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix1,
        const Inpar::S2I::InterfaceSides matrix1_side_rows,
        const Inpar::S2I::InterfaceSides matrix1_side_cols,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix2,
        const Inpar::S2I::InterfaceSides matrix2_side_rows,
        const Inpar::S2I::InterfaceSides matrix2_side_cols,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix3,
        const Inpar::S2I::InterfaceSides matrix3_side_rows,
        const Inpar::S2I::InterfaceSides matrix3_side_cols,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix4,
        const Inpar::S2I::InterfaceSides matrix4_side_rows,
        const Inpar::S2I::InterfaceSides matrix4_side_cols,
        const Teuchos::RCP<Epetra_MultiVector>& systemvector1,
        const Inpar::S2I::InterfaceSides vector1_side,
        const Teuchos::RCP<Epetra_MultiVector>& systemvector2,
        const Inpar::S2I::InterfaceSides vector2_side) const;

    /*!
     * @brief  evaluate node-to-segment coupling
     *
     * @param islavenodestomasterelements  vector for node-to-segment connectivity
     * @param islavenodeslumpedareas       vector for lumped interface area fractions associated
     *                                     with slave-side nodes
     * @param islavenodesimpltypes         vector for physical implementation types of slave-side
     *                                     nodes
     * @param idiscret                     interface discretization
     * @param params                       parameter list for evaluation of mortar integration cells
     * @param systemmatrix1                system matrix 1
     * @param matrix1_side_rows            interface side associated with rows of system matrix 1
     * @param matrix1_side_cols            interface side associated with columns of system matrix 1
     * @param systemmatrix2                system matrix 2
     * @param matrix2_side_rows            interface side associated with rows of system matrix 2
     * @param matrix2_side_cols            interface side associated with columns of system matrix 2
     * @param systemmatrix3                system matrix 3
     * @param matrix3_side_rows            interface side associated with rows of system matrix 3
     * @param matrix3_side_cols            interface side associated with columns of system matrix 3
     * @param systemmatrix4                system matrix 4
     * @param matrix4_side_rows     interface side associated with rows of system matrix 4
     * @param matrix4_side_cols     interface side associated with columns of system matrix 4
     * @param systemvector1         system vector 1
     * @param vector1_side          interface side associated with system vector 1
     * @param systemvector2         system vector 2
     * @param vector2_side          interface side associated with system vector 2
     */
    void evaluate_nts(const Epetra_IntVector& islavenodestomasterelements,
        const Epetra_Vector& islavenodeslumpedareas, const Epetra_IntVector& islavenodesimpltypes,
        const Core::FE::Discretization& idiscret, const Teuchos::ParameterList& params,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix1,
        const Inpar::S2I::InterfaceSides matrix1_side_rows,
        const Inpar::S2I::InterfaceSides matrix1_side_cols,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix2,
        const Inpar::S2I::InterfaceSides matrix2_side_rows,
        const Inpar::S2I::InterfaceSides matrix2_side_cols,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix3,
        const Inpar::S2I::InterfaceSides matrix3_side_rows,
        const Inpar::S2I::InterfaceSides matrix3_side_cols,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix4,
        const Inpar::S2I::InterfaceSides matrix4_side_rows,
        const Inpar::S2I::InterfaceSides matrix4_side_cols,
        const Teuchos::RCP<Epetra_MultiVector>& systemvector1,
        const Inpar::S2I::InterfaceSides vector1_side,
        const Teuchos::RCP<Epetra_MultiVector>& systemvector2,
        const Inpar::S2I::InterfaceSides vector2_side) const;

    /*!
     * @brief  evaluate mortar elements
     *
     * @param ielecolmap         column map of mortar elements
     * @param ieleimpltypes      vector for physical implementation types of mortar elements
     * @param idiscret           interface discretization
     * @param params             parameter list for evaluation of mortar integration cells
     * @param systemmatrix1      system matrix 1
     * @param matrix1_side_rows  interface side associated with rows of system matrix 1
     * @param matrix1_side_cols  interface side associated with columns of system matrix 1
     * @param systemmatrix2      system matrix 2
     * @param matrix2_side_rows  interface side associated with rows of system matrix 2
     * @param matrix2_side_cols  interface side associated with columns of system matrix 2
     * @param systemmatrix3      system matrix 3
     * @param matrix3_side_rows  interface side associated with rows of system matrix 3
     * @param matrix3_side_cols  interface side associated with columns of system matrix 3
     * @param systemmatrix4      system matrix 4
     * @param matrix4_side_rows  interface side associated with rows of system matrix 4
     * @param matrix4_side_cols  interface side associated with columns of system matrix 4
     * @param systemvector1      system vector 1
     * @param vector1_side       interface side associated with system vector 1
     * @param systemvector2      system vector 2
     * @param vector2_side       interface side associated with system vector 2
     */
    void evaluate_mortar_elements(const Epetra_Map& ielecolmap,
        const Epetra_IntVector& ieleimpltypes, const Core::FE::Discretization& idiscret,
        const Teuchos::ParameterList& params,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix1,
        const Inpar::S2I::InterfaceSides matrix1_side_rows,
        const Inpar::S2I::InterfaceSides matrix1_side_cols,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix2,
        const Inpar::S2I::InterfaceSides matrix2_side_rows,
        const Inpar::S2I::InterfaceSides matrix2_side_cols,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix3,
        const Inpar::S2I::InterfaceSides matrix3_side_rows,
        const Inpar::S2I::InterfaceSides matrix3_side_cols,
        const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix4,
        const Inpar::S2I::InterfaceSides matrix4_side_rows,
        const Inpar::S2I::InterfaceSides matrix4_side_cols,
        const Teuchos::RCP<Epetra_MultiVector>& systemvector1,
        const Inpar::S2I::InterfaceSides vector1_side,
        const Teuchos::RCP<Epetra_MultiVector>& systemvector2,
        const Inpar::S2I::InterfaceSides vector2_side) const;

    //! flag indicating if we have capacitive interface flux contributions
    bool has_capacitive_contributions_;

    //! slave-side scatra-scatra interface kinetics conditions applied to a mesh tying interface
    std::map<const int, Core::Conditions::Condition* const>
        kinetics_conditions_meshtying_slaveside_;

    //! corresponding master conditions to kinetics condiditions
    std::map<const int, Core::Conditions::Condition* const> master_conditions_;

    //! flag for evaluation of interface linearizations and residuals on slave side only
    bool slaveonly_;

    //! flag indicating that mesh tying for different conditions should be setup independently
    const bool indepedent_setup_of_conditions_;

  };  // class meshtying_strategy_s2_i


  class MortarCellInterface
  {
   public:
    //! Virtual destructor.
    virtual ~MortarCellInterface() = default;

    //! evaluate single mortar integration cell of particular slave-side and master-side
    //! discretization types
    virtual void Evaluate(const Core::FE::Discretization& idiscret,  //!< interface discretization
        Mortar::IntCell& cell,                                       //!< mortar integration cell
        Mortar::Element& slaveelement,                               //!< slave-side mortar element
        Mortar::Element& masterelement,                              //!< master-side mortar element
        Core::Elements::Element::LocationArray& la_slave,            //!< slave-side location array
        Core::Elements::Element::LocationArray& la_master,           //!< master-side location array
        const Teuchos::ParameterList& params,                        //!< parameter list
        Core::LinAlg::SerialDenseMatrix& cellmatrix1,                //!< cell matrix 1
        Core::LinAlg::SerialDenseMatrix& cellmatrix2,                //!< cell matrix 2
        Core::LinAlg::SerialDenseMatrix& cellmatrix3,                //!< cell matrix 3
        Core::LinAlg::SerialDenseMatrix& cellmatrix4,                //!< cell matrix 4
        Core::LinAlg::SerialDenseVector& cellvector1,                //!< cell vector 1
        Core::LinAlg::SerialDenseVector& cellvector2                 //!< cell vector 2
        ) = 0;

    //! evaluate single slave-side node for node-to-segment coupling
    virtual void evaluate_nts(
        const Core::FE::Discretization& idiscret,  //!< interface discretization
        const Mortar::Node& slavenode,             //!< slave-side node
        const double&
            lumpedarea,  //!< lumped interface area fraction associated with slave-side node
        Mortar::Element& slaveelement,                      //!< slave-side mortar element
        Mortar::Element& masterelement,                     //!< master-side mortar element
        Core::Elements::Element::LocationArray& la_slave,   //!< slave-side location array
        Core::Elements::Element::LocationArray& la_master,  //!< master-side location array
        const Teuchos::ParameterList& params,               //!< parameter list
        Core::LinAlg::SerialDenseMatrix& ntsmatrix1,        //!< node-to-segment matrix 1
        Core::LinAlg::SerialDenseMatrix& ntsmatrix2,        //!< node-to-segment matrix 2
        Core::LinAlg::SerialDenseMatrix& ntsmatrix3,        //!< node-to-segment matrix 3
        Core::LinAlg::SerialDenseMatrix& ntsmatrix4,        //!< node-to-segment matrix 4
        Core::LinAlg::SerialDenseVector& ntsvector1,        //!< node-to-segment vector 1
        Core::LinAlg::SerialDenseVector& ntsvector2         //!< node-to-segment vector 2
        ) = 0;

    //! evaluate single mortar element
    virtual void evaluate_mortar_element(
        const Core::FE::Discretization& idiscret,     //!< interface discretization
        Mortar::Element& element,                     //!< mortar element
        Core::Elements::Element::LocationArray& la,   //!< location array
        const Teuchos::ParameterList& params,         //!< parameter list
        Core::LinAlg::SerialDenseMatrix& elematrix1,  //!< element matrix 1
        Core::LinAlg::SerialDenseMatrix& elematrix2,  //!< element matrix 2
        Core::LinAlg::SerialDenseMatrix& elematrix3,  //!< element matrix 3
        Core::LinAlg::SerialDenseMatrix& elematrix4,  //!< element matrix 4
        Core::LinAlg::SerialDenseVector& elevector1,  //!< element vector 1
        Core::LinAlg::SerialDenseVector& elevector2   //!< element vector 2
        ) = 0;

   protected:
    //! protected constructor for singletons
    MortarCellInterface(
        const Inpar::S2I::CouplingType& couplingtype,  //!< flag for meshtying method
        const Inpar::S2I::InterfaceSides&
            lmside,  //!< flag for interface side underlying Lagrange multiplier definition
        const int& numdofpernode_slave,  //!< number of slave-side degrees of freedom per node
        const int& numdofpernode_master  //!< number of master-side degrees of freedom per node
    );

    //! flag for interface side underlying Lagrange multiplier definition
    const Inpar::S2I::InterfaceSides lmside_;

    //! flag for meshtying method
    const Inpar::S2I::CouplingType couplingtype_;

    //! number of slave-side degrees of freedom per node
    const int numdofpernode_slave_;

    //! number of master-side degrees of freedom per node
    const int numdofpernode_master_;
  };


  template <Core::FE::CellType distypeS, Core::FE::CellType distypeM>
  class MortarCellCalc : public MortarCellInterface
  {
   public:
    //! singleton access method
    static MortarCellCalc<distypeS, distypeM>* Instance(
        const Inpar::S2I::CouplingType& couplingtype,  //!< flag for meshtying method
        const Inpar::S2I::InterfaceSides&
            lmside,  //!< flag for interface side underlying Lagrange multiplier definition
        const int& numdofpernode_slave,   //!< number of slave-side degrees of freedom per node
        const int& numdofpernode_master,  //!< number of master-side degrees of freedom per node
        const std::string& disname        //!< name of mortar discretization
    );

    //! evaluate single mortar integration cell of particular slave-side and master-side
    //! discretization types
    void Evaluate(const Core::FE::Discretization& idiscret,  //!< interface discretization
        Mortar::IntCell& cell,                               //!< mortar integration cell
        Mortar::Element& slaveelement,                       //!< slave-side mortar element
        Mortar::Element& masterelement,                      //!< master-side mortar element
        Core::Elements::Element::LocationArray& la_slave,    //!< slave-side location array
        Core::Elements::Element::LocationArray& la_master,   //!< master-side location array
        const Teuchos::ParameterList& params,                //!< parameter list
        Core::LinAlg::SerialDenseMatrix& cellmatrix1,        //!< cell matrix 1
        Core::LinAlg::SerialDenseMatrix& cellmatrix2,        //!< cell matrix 2
        Core::LinAlg::SerialDenseMatrix& cellmatrix3,        //!< cell matrix 3
        Core::LinAlg::SerialDenseMatrix& cellmatrix4,        //!< cell matrix 4
        Core::LinAlg::SerialDenseVector& cellvector1,        //!< cell vector 1
        Core::LinAlg::SerialDenseVector& cellvector2         //!< cell vector 2
        ) override;

    //! evaluate single slave-side node for node-to-segment coupling
    void evaluate_nts(const Core::FE::Discretization& idiscret,  //!< interface discretization
        const Mortar::Node& slavenode,                           //!< slave-side node
        const double&
            lumpedarea,  //!< lumped interface area fraction associated with slave-side node
        Mortar::Element& slaveelement,                      //!< slave-side mortar element
        Mortar::Element& masterelement,                     //!< master-side mortar element
        Core::Elements::Element::LocationArray& la_slave,   //!< slave-side location array
        Core::Elements::Element::LocationArray& la_master,  //!< master-side location array
        const Teuchos::ParameterList& params,               //!< parameter list
        Core::LinAlg::SerialDenseMatrix& ntsmatrix1,        //!< node-to-segment matrix 1
        Core::LinAlg::SerialDenseMatrix& ntsmatrix2,        //!< node-to-segment matrix 2
        Core::LinAlg::SerialDenseMatrix& ntsmatrix3,        //!< node-to-segment matrix 3
        Core::LinAlg::SerialDenseMatrix& ntsmatrix4,        //!< node-to-segment matrix 4
        Core::LinAlg::SerialDenseVector& ntsvector1,        //!< node-to-segment vector 1
        Core::LinAlg::SerialDenseVector& ntsvector2         //!< node-to-segment vector 2
        ) override;

    //! evaluate single mortar element
    void evaluate_mortar_element(
        const Core::FE::Discretization& idiscret,     //!< interface discretization
        Mortar::Element& element,                     //!< mortar element
        Core::Elements::Element::LocationArray& la,   //!< location array
        const Teuchos::ParameterList& params,         //!< parameter list
        Core::LinAlg::SerialDenseMatrix& elematrix1,  //!< element matrix 1
        Core::LinAlg::SerialDenseMatrix& elematrix2,  //!< element matrix 2
        Core::LinAlg::SerialDenseMatrix& elematrix3,  //!< element matrix 3
        Core::LinAlg::SerialDenseMatrix& elematrix4,  //!< element matrix 4
        Core::LinAlg::SerialDenseVector& elevector1,  //!< element vector 1
        Core::LinAlg::SerialDenseVector& elevector2   //!< element vector 2
        ) override;

   protected:
    //! number of slave element nodes
    static constexpr int nen_slave_ = Core::FE::num_nodes<distypeS>;

    //! number of master element nodes
    static constexpr int nen_master_ = Core::FE::num_nodes<distypeM>;

    //! spatial dimensionality of slave elements
    static constexpr int nsd_slave_ = Core::FE::dim<distypeS>;

    //! spatial dimensionality of master elements
    static constexpr int nsd_master_ = Core::FE::dim<distypeM>;

    //! protected constructor for singletons
    MortarCellCalc(const Inpar::S2I::CouplingType& couplingtype,  //!< flag for meshtying method
        const Inpar::S2I::InterfaceSides&
            lmside,  //!< flag for interface side underlying Lagrange multiplier definition
        const int& numdofpernode_slave,  //!< number of slave-side degrees of freedom per node
        const int& numdofpernode_master  //!< number of master-side degrees of freedom per node
    );

    //! evaluate mortar matrices
    void evaluate_mortar_matrices(Mortar::IntCell& cell,  //!< mortar integration cell
        Mortar::Element& slaveelement,                    //!< slave-side mortar element
        Mortar::Element& masterelement,                   //!< master-side mortar element
        Core::LinAlg::SerialDenseMatrix& D,               //!< mortar matrix D
        Core::LinAlg::SerialDenseMatrix& M,               //!< mortar matrix M
        Core::LinAlg::SerialDenseMatrix& E                //!< mortar matrix E
    );

    //! evaluate and assemble interface linearizations and residuals
    virtual void evaluate_condition(
        const Core::FE::Discretization& idiscret,           //!< interface discretization
        Mortar::IntCell& cell,                              //!< mortar integration cell
        Mortar::Element& slaveelement,                      //!< slave-side mortar element
        Mortar::Element& masterelement,                     //!< master-side mortar element
        Core::Elements::Element::LocationArray& la_slave,   //!< slave-side location array
        Core::Elements::Element::LocationArray& la_master,  //!< master-side location array
        const Teuchos::ParameterList& params,               //!< parameter list
        Core::LinAlg::SerialDenseMatrix&
            k_ss,  //!< linearizations of slave-side residuals w.r.t. slave-side dofs
        Core::LinAlg::SerialDenseMatrix&
            k_sm,  //!< linearizations of slave-side residuals w.r.t. master-side dofs
        Core::LinAlg::SerialDenseMatrix&
            k_ms,  //!< linearizations of master-side residuals w.r.t. slave-side dofs
        Core::LinAlg::SerialDenseMatrix&
            k_mm,  //!< linearizations of master-side residuals w.r.t. master-side dofs
        Core::LinAlg::SerialDenseVector& r_s,  //!< slave-side residual vector
        Core::LinAlg::SerialDenseVector& r_m   //!< master-side residual vector
    );

    //! evaluate and assemble interface linearizations and residuals for node-to-segment coupling
    virtual void evaluate_condition_nts(
        Core::Conditions::Condition& condition,  //!< scatra-scatra interface coupling condition
        const Mortar::Node& slavenode,           //!< slave-side node
        const double&
            lumpedarea,  //!< lumped interface area fraction associated with slave-side node
        Mortar::Element& slaveelement,   //!< slave-side mortar element
        Mortar::Element& masterelement,  //!< master-side mortar element
        const std::vector<Core::LinAlg::Matrix<nen_slave_, 1>>&
            ephinp_slave,  //!< state variables at slave-side nodes
        const std::vector<Core::LinAlg::Matrix<nen_master_, 1>>&
            ephinp_master,  //!< state variables at master-side nodes
        Core::LinAlg::SerialDenseMatrix&
            k_ss,  //!< linearizations of slave-side residuals w.r.t. slave-side dofs
        Core::LinAlg::SerialDenseMatrix&
            k_sm,  //!< linearizations of slave-side residuals w.r.t. master-side dofs
        Core::LinAlg::SerialDenseMatrix&
            k_ms,  //!< linearizations of master-side residuals w.r.t. slave-side dofs
        Core::LinAlg::SerialDenseMatrix&
            k_mm,  //!< linearizations of master-side residuals w.r.t. master-side dofs
        Core::LinAlg::SerialDenseVector& r_s,  //!< slave-side residual vector
        Core::LinAlg::SerialDenseVector& r_m   //!< master-side residual vector
    );

    //! evaluate and assemble lumped interface area fractions associated with slave-side element
    //! nodes
    void evaluate_nodal_area_fractions(
        Mortar::Element& slaveelement,  //!< slave-side mortar element
        Core::LinAlg::SerialDenseVector&
            areafractions  //!< lumped interface area fractions associated
                           //!< with slave-side element nodes
    );

    //! extract nodal state variables associated with mortar integration cell
    virtual void extract_node_values(
        const Core::FE::Discretization& idiscret,          //!< interface discretization
        Core::Elements::Element::LocationArray& la_slave,  //!< slave-side location array
        Core::Elements::Element::LocationArray& la_master  //!< master-side location array
    );

    /*!
     * @brief  extract nodal state variables associated with slave element
     *
     * @param estate_slave  state variables at slave-side nodes
     * @param idiscret      interface discretization
     * @param la_slave      slave-side location array
     * @param statename     name of relevant state
     * @param nds          number of relevant dofset
     */
    void extract_node_values(Core::LinAlg::Matrix<nen_slave_, 1>& estate_slave,
        const Core::FE::Discretization& idiscret, Core::Elements::Element::LocationArray& la_slave,
        const std::string& statename = "iphinp", const int& nds = 0) const;

    /*!
     * @brief extract nodal state variables associated with slave and master elements
     *
     * @param estate_slave   state variables at slave-side nodes
     * @param estate_master  state variables at master-side nodes
     * @param idiscret       interface discretization
     * @param la_slave      slave-side location array
     * @param la_master     master-side location array
     * @param statename      name of relevant state
     * @param nds            number of relevant dofset
     */
    void extract_node_values(std::vector<Core::LinAlg::Matrix<nen_slave_, 1>>& estate_slave,
        std::vector<Core::LinAlg::Matrix<nen_master_, 1>>& estate_master,
        const Core::FE::Discretization& idiscret, Core::Elements::Element::LocationArray& la_slave,
        Core::Elements::Element::LocationArray& la_master, const std::string& statename = "iphinp",
        const int& nds = 0) const;

    //! evaluate slave-side and master-side shape functions and domain integration factor at cell
    //! integration point
    double eval_shape_func_and_dom_int_fac_at_int_point(
        Mortar::Element& slaveelement,                               //!< slave-side mortar element
        Mortar::Element& masterelement,                              //!< master-side mortar element
        Mortar::IntCell& cell,                                       //!< mortar integration cell
        const Core::FE::IntPointsAndWeights<nsd_slave_>& intpoints,  //!< quadrature rule
        const int iquad                                              //!< ID of integration point
    );

    //! evaluate slave-side shape functions and domain integration factor at element integration
    //! point
    double eval_shape_func_and_dom_int_fac_at_int_point(
        Mortar::Element& element,                                    //!< mortar element
        const Core::FE::IntPointsAndWeights<nsd_slave_>& intpoints,  //!< quadrature rule
        const int iquad                                              //!< ID of integration point
    );

    //! evaluate shape functions at position of slave-side node
    void eval_shape_func_at_slave_node(const Mortar::Node& slavenode,  //!< slave-side node
        Mortar::Element& slaveelement,                                 //!< slave-side element
        Mortar::Element& masterelement                                 //!< master-side element
    );

    //! pointer to scatra boundary parameter list
    Discret::ELEMENTS::ScaTraEleParameterBoundary* scatraparamsboundary_;

    //! nodal, slave-side state variables associated with time t_{n+1} or t_{n+alpha_f}
    std::vector<Core::LinAlg::Matrix<nen_slave_, 1>> ephinp_slave_;

    //! nodal, master-side state variables associated with time t_{n+1} or t_{n+alpha_f}
    std::vector<Core::LinAlg::Matrix<nen_master_, 1>> ephinp_master_;

    //! shape and test function values associated with slave-side dofs at integration point
    Core::LinAlg::Matrix<nen_slave_, 1> funct_slave_;

    //! shape and test function values associated with master-side dofs at integration point
    Core::LinAlg::Matrix<nen_master_, 1> funct_master_;

    //! shape function values associated with slave-side Lagrange multipliers at integration point
    Core::LinAlg::Matrix<nen_slave_, 1> shape_lm_slave_;

    //! shape function values associated with master-side Lagrange multipliers at integration point
    Core::LinAlg::Matrix<nen_master_, 1> shape_lm_master_;

    //! test function values associated with slave-side Lagrange multipliers at integration point
    Core::LinAlg::Matrix<nen_slave_, 1> test_lm_slave_;

    //! test function values associated with master-side Lagrange multipliers at integration point
    Core::LinAlg::Matrix<nen_master_, 1> test_lm_master_;
  };  // class mortar_cell_calc


  class MortarCellFactory
  {
   public:
    //! provide instance of mortar cell evaluation class of particular slave-side discretization
    //! type
    static MortarCellInterface* mortar_cell_calc(
        const Inpar::ScaTra::ImplType&
            impltype,  //!< physical implementation type of mortar integration cell
        const Mortar::Element& slaveelement,           //!< slave-side mortar element
        const Mortar::Element& masterelement,          //!< master-side mortar element
        const Inpar::S2I::CouplingType& couplingtype,  //!< flag for meshtying method
        const Inpar::S2I::InterfaceSides&
            lmside,  //!< flag for interface side underlying Lagrange multiplier definition
        const std::string& disname  //!< name of interface discretization
    );

   private:
    //! provide instance of mortar cell evaluation class of particular slave-side and master-side
    //! discretization types
    template <Core::FE::CellType distypeS>
    static MortarCellInterface* mortar_cell_calc(
        const Inpar::ScaTra::ImplType&
            impltype,  //!< physical implementation type of mortar integration cell
        const Mortar::Element& masterelement,          //!< master-side mortar element
        const Inpar::S2I::CouplingType& couplingtype,  //!< flag for meshtying method
        const Inpar::S2I::InterfaceSides&
            lmside,  //!< flag for interface side underlying Lagrange multiplier definition
        const int& numdofpernode_slave,  //!< number of slave-side degrees of freedom per node
        const std::string& disname       //!< name of interface discretization
    );

    //! provide specific instance of mortar cell evaluation class
    template <Core::FE::CellType distypeS, Core::FE::CellType distypeM>
    static MortarCellInterface* mortar_cell_calc(
        const Inpar::ScaTra::ImplType&
            impltype,  //!< physical implementation type of mortar integration cell
        const Inpar::S2I::CouplingType& couplingtype,  //!< flag for meshtying method
        const Inpar::S2I::InterfaceSides&
            lmside,  //!< flag for interface side underlying Lagrange multiplier definition
        const int& numdofpernode_slave,   //!< number of slave-side degrees of freedom per node
        const int& numdofpernode_master,  //!< number of master-side degrees of freedom per node
        const std::string& disname        //!< name of interface discretization
    );
  };  // class MortarCellFactory


  class MortarCellAssemblyStrategy
  {
   public:
    //! constructor
    MortarCellAssemblyStrategy(
        Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix1,  //!< system matrix 1
        const Inpar::S2I::InterfaceSides
            matrix1_side_rows,  //!< interface side associated with rows of system matrix 1
        const Inpar::S2I::InterfaceSides
            matrix1_side_cols,  //!< interface side associated with columns of system matrix 1
        Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix2,  //!< system matrix 2
        const Inpar::S2I::InterfaceSides
            matrix2_side_rows,  //!< interface side associated with rows of system matrix 2
        const Inpar::S2I::InterfaceSides
            matrix2_side_cols,  //!< interface side associated with columns of system matrix 2
        Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix3,  //!< system matrix 3
        const Inpar::S2I::InterfaceSides
            matrix3_side_rows,  //!< interface side associated with rows of system matrix 3
        const Inpar::S2I::InterfaceSides
            matrix3_side_cols,  //!< interface side associated with columns of system matrix 3
        Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix4,  //!< system matrix 4
        const Inpar::S2I::InterfaceSides
            matrix4_side_rows,  //!< interface side associated with rows of system matrix 4
        const Inpar::S2I::InterfaceSides
            matrix4_side_cols,  //!< interface side associated with columns of system matrix 4
        Teuchos::RCP<Epetra_MultiVector> systemvector1,  //!< system vector 1
        const Inpar::S2I::InterfaceSides
            vector1_side,  //!< interface side associated with system vector 1
        Teuchos::RCP<Epetra_MultiVector> systemvector2,  //!< system vector 2
        const Inpar::S2I::InterfaceSides
            vector2_side,        //!< interface side associated with system vector 2
        const int nds_rows = 0,  //!< number of dofset associated with matrix rows
        const int nds_cols = 0   //!< number of dofset associated with matrix columns
    );

    /*!
     * @brief assemble cell matrices and vectors into system matrices and vectors
     *
     * @param la_slave   slave-side location array
     * @param la_master  master-side location array
     * @param assembler_pid_master  ID of processor performing master-side matrix and vector
     *                              assembly
     */
    void assemble_cell_matrices_and_vectors(Core::Elements::Element::LocationArray& la_slave,
        Core::Elements::Element::LocationArray& la_master, const int assembler_pid_master) const;

    //! bool flag for assembly of system matrix 1
    bool AssembleMatrix1() const { return systemmatrix1_ != Teuchos::null; };

    //! bool flag for assembly of system matrix 2
    bool AssembleMatrix2() const { return systemmatrix2_ != Teuchos::null; };

    //! bool flag for assembly of system matrix 3
    bool AssembleMatrix3() const { return systemmatrix3_ != Teuchos::null; };

    //! bool flag for assembly of system matrix 4
    bool AssembleMatrix4() const { return systemmatrix4_ != Teuchos::null; };

    //! bool flag for assembly of system vector 1
    bool AssembleVector1() const { return systemvector1_ != Teuchos::null; };

    //! bool flag for assembly of system vector 2
    bool AssembleVector2() const { return systemvector2_ != Teuchos::null; };

    //! return cell matrix 1
    Core::LinAlg::SerialDenseMatrix& CellMatrix1() { return cellmatrix1_; };

    //! return cell matrix 2
    Core::LinAlg::SerialDenseMatrix& CellMatrix2() { return cellmatrix2_; };

    //! return cell matrix 3
    Core::LinAlg::SerialDenseMatrix& CellMatrix3() { return cellmatrix3_; };

    //! return cell matrix 4
    Core::LinAlg::SerialDenseMatrix& CellMatrix4() { return cellmatrix4_; };

    //! return cell vector 1
    Core::LinAlg::SerialDenseVector& CellVector1() { return cellvector1_; };

    //! return cell vector 2
    Core::LinAlg::SerialDenseVector& CellVector2() { return cellvector2_; };

    //! initialize cell matrices and vectors
    void init_cell_matrices_and_vectors(
        Core::Elements::Element::LocationArray& la_slave,  //!< slave-side location array
        Core::Elements::Element::LocationArray& la_master  //!< master-side location array
    );

   private:
    /*!
     * @brief  assemble cell matrix into system matrix
     *
     * @param systemmatrix   system matrix
     * @param cellmatrix     cell matrix
     * @param side_rows      interface side associated with matrix rows
     * @param side_cols      interface side associated with matrix columns
     * @param la_slave       slave-side location array
     * @param la_master      master-side location array
     * @param assembler_pid_master   ID of processor performing master-side matrix assembly
     */
    void assemble_cell_matrix(const Teuchos::RCP<Core::LinAlg::SparseOperator>& systemmatrix,
        const Core::LinAlg::SerialDenseMatrix& cellmatrix,
        const Inpar::S2I::InterfaceSides side_rows, const Inpar::S2I::InterfaceSides side_cols,
        Core::Elements::Element::LocationArray& la_slave,
        Core::Elements::Element::LocationArray& la_master, const int assembler_pid_master) const;

    /*!
     * @brief  assemble cell vector into system vector
     *
     * @param systemvector  system vector
     * @param cellvector    cell vector
     * @param side          interface side associated with system and cell vectors
     * @param la_slave      slave-side location array
     * @param la_master     master-side location array
     * @param assembler_pid_master  ID of processor performing master-side vector assembly
     */
    void assemble_cell_vector(const Teuchos::RCP<Epetra_MultiVector>& systemvector,
        const Core::LinAlg::SerialDenseVector& cellvector, const Inpar::S2I::InterfaceSides side,
        Core::Elements::Element::LocationArray& la_slave,
        Core::Elements::Element::LocationArray& la_master, const int assembler_pid_master) const;

    /*!
     * @brief  initialize cell matrix
     *
     * @param cellmatrix  cell matrix
     * @param side_rows   interface side associated with rows of cell matrix
     * @param side_cols   interface side associated with columns of cell matrix
     * @param la_slave    slave-side location array
     * @param la_master  master-side location array
     */
    void init_cell_matrix(Core::LinAlg::SerialDenseMatrix& cellmatrix,
        const Inpar::S2I::InterfaceSides side_rows, const Inpar::S2I::InterfaceSides side_cols,
        Core::Elements::Element::LocationArray& la_slave,
        Core::Elements::Element::LocationArray& la_master) const;

    /*!
     * @brief  initialize cell vector
     *
     * @param cellvector  cell vector
     * @param side        interface side associated with cell vector
     * @param la_slave    slave-side location array
     * @param la_master   master-side location array
     */
    void init_cell_vector(Core::LinAlg::SerialDenseVector& cellvector,
        const Inpar::S2I::InterfaceSides side, Core::Elements::Element::LocationArray& la_slave,
        Core::Elements::Element::LocationArray& la_master) const;

    //! cell matrix 1
    Core::LinAlg::SerialDenseMatrix cellmatrix1_;

    //! cell matrix 2
    Core::LinAlg::SerialDenseMatrix cellmatrix2_;

    //! cell matrix 3
    Core::LinAlg::SerialDenseMatrix cellmatrix3_;

    //! cell matrix 4
    Core::LinAlg::SerialDenseMatrix cellmatrix4_;

    //! cell vector 1
    Core::LinAlg::SerialDenseVector cellvector1_;

    //! cell vector 2
    Core::LinAlg::SerialDenseVector cellvector2_;

    //! interface side associated with rows of system matrix 1
    const Inpar::S2I::InterfaceSides matrix1_side_rows_;

    //! interface side associated with columns of system matrix 1
    const Inpar::S2I::InterfaceSides matrix1_side_cols_;

    //! interface side associated with rows of system matrix 2
    const Inpar::S2I::InterfaceSides matrix2_side_rows_;

    //! interface side associated with columns of system matrix 2
    const Inpar::S2I::InterfaceSides matrix2_side_cols_;

    //! interface side associated with rows of system matrix 3
    const Inpar::S2I::InterfaceSides matrix3_side_rows_;

    //! interface side associated with columns of system matrix 3
    const Inpar::S2I::InterfaceSides matrix3_side_cols_;

    //! interface side associated with rows of system matrix 4
    const Inpar::S2I::InterfaceSides matrix4_side_rows_;

    //! interface side associated with columns of system matrix 4
    const Inpar::S2I::InterfaceSides matrix4_side_cols_;

    //! system matrix 1
    const Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix1_;

    //! system matrix 2
    const Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix2_;

    //! system matrix 3
    const Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix3_;

    //! system matrix 4
    const Teuchos::RCP<Core::LinAlg::SparseOperator> systemmatrix4_;

    //! system vector 1
    const Teuchos::RCP<Epetra_MultiVector> systemvector1_;

    //! system vector 2
    const Teuchos::RCP<Epetra_MultiVector> systemvector2_;

    //! interface side associated with system vector 1
    const Inpar::S2I::InterfaceSides vector1_side_;

    //! interface side associated with system vector 2
    const Inpar::S2I::InterfaceSides vector2_side_;

    //! number of dofset associated with matrix rows
    const int nds_rows_;

    //! number of dofset associated with matrix columns
    const int nds_cols_;
  };  // class MortarCellAssembleStrategy
}  // namespace ScaTra
FOUR_C_NAMESPACE_CLOSE

#endif
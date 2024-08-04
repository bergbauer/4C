/*----------------------------------------------------------------------*/
/*! \file
\brief Control routine for monolithic fluid-fluid-fsi (structuresplit)
using XFEM

\level 3


*----------------------------------------------------------------------*/

#ifndef FOUR_C_FSI_FLUIDFLUIDMONOLITHIC_STRUCTURESPLIT_NONOX_HPP
#define FOUR_C_FSI_FLUIDFLUIDMONOLITHIC_STRUCTURESPLIT_NONOX_HPP

#include "4C_config.hpp"

#include "4C_coupling_adapter_converter.hpp"
#include "4C_fsi_monolithic_nonox.hpp"

FOUR_C_NAMESPACE_OPEN

namespace FSI
{
  /// monolithic Fluid-Fluid FSI algorithm (structuresplit)
  /*!
    Here the structural matrix is split whereas the fluid matrix is taken as
    it is.

    \author Shadan Shahmiri
    \date  11/2011
  */
  class FluidFluidMonolithicStructureSplitNoNOX : public MonolithicNoNOX
  {
    friend class FSI::FSIResultTest;

   public:
    /// constructor
    explicit FluidFluidMonolithicStructureSplitNoNOX(
        const Epetra_Comm& comm, const Teuchos::ParameterList& timeparams);

    /*! do the setup for the monolithic system


    1.) setup coupling; right now, we use matching meshes at the interface
    2.) create combined map
    3.) create block system matrix


    */
    void setup_system() override;

   protected:
    //! @name Apply current field state to system

    /// setup composed right hand side from field solvers
    void setup_rhs(Epetra_Vector& f, bool firstcall) override;

    /// setup composed system block matrix
    void setup_system_matrix() override;
    //@}

    /// create merged map of DOF in the final system from all fields
    void create_combined_dof_row_map() override;

    /// Extract initial guess from fields
    void initial_guess(Teuchos::RCP<Epetra_Vector> ig) override;

    /// apply infnorm scaling to linear block system
    virtual void scale_system(Core::LinAlg::BlockSparseMatrixBase& mat, Epetra_Vector& b);

    /// undo infnorm scaling from scaled solution
    virtual void unscale_solution(
        Core::LinAlg::BlockSparseMatrixBase& mat, Epetra_Vector& x, Epetra_Vector& b);

    /// create merged map with Dirichlet-constrained DOF from all fields
    Teuchos::RCP<Epetra_Map> combined_dbc_map() override;

    //! Extract the three field vectors from a given composed vector
    //!
    //! In analogy to NOX, x is step increment \f$\Delta x\f$
    //! that brings us from \f$t^{n}\f$ to \f$t^{n+1}\f$:
    //! \f$x^{n+1} = x^{n} + \Delta x\f$
    //!
    //! Iteration increments, that are needed internally in the single fields,
    //! have to be computed somewhere else.
    //!
    //! \param x  (i) composed vector that contains all field vectors
    //! \param sx (o) structural displacements
    //! \param fx (o) fluid velocities and pressure
    //! \param ax (o) ale displacements
    void extract_field_vectors(Teuchos::RCP<const Epetra_Vector> x,
        Teuchos::RCP<const Epetra_Vector>& sx, Teuchos::RCP<const Epetra_Vector>& fx,
        Teuchos::RCP<const Epetra_Vector>& ax) override;

    /// compute the Lagrange multiplier (FSI stresses) for the current time step
    void recover_lagrange_multiplier() override;

    /// compute the residual and incremental norms required for convergence check
    void build_convergence_norms() override;

    /// read restart data
    void read_restart(int step) override;

    /// output of fluid, structure & ALE-quantities and Lagrange multiplier
    void output() override;

    /*!
     * In case of a change in the fluid DOF row maps during the Newton loop (full Newton approach),
     * reset vectors accordingly.
     * \author kruse
     * \date 05/14
     */
    void handle_fluid_dof_map_change_in_newton() override;

    /*!
     * Determine a change in fluid DOF map
     * \param (in) : DOF map of fluid increment vector
     * \return : true, in case of a mismatch between map of increment vector
     * and inner fluid DOF map after evaluation
     * \author kruse
     * \date 05/14
     */
    bool has_fluid_dof_map_changed(const Epetra_BlockMap& fluidincrementmap) override;

   private:
    /// build block vector from field vectors
    void setup_vector(Epetra_Vector& f, Teuchos::RCP<const Epetra_Vector> sv,
        Teuchos::RCP<const Epetra_Vector> fv, Teuchos::RCP<const Epetra_Vector> av,
        double fluidscale);

    /// access type-cast pointer to problem-specific fluid-wrapper
    const Teuchos::RCP<Adapter::FluidFluidFSI>& fluid_field() { return MonolithicNoNOX::fluid_; }

    /// block system matrix
    // Teuchos::RCP<OverlappingBlockMatrix> systemmatrix_;

    /// @name Matrix block transform objects
    /// Handle row and column map exchange for matrix blocks

    Teuchos::RCP<Coupling::Adapter::MatrixRowColTransform> sggtransform_;
    Teuchos::RCP<Coupling::Adapter::MatrixRowTransform> sgitransform_;
    Teuchos::RCP<Coupling::Adapter::MatrixColTransform> sigtransform_;
    Teuchos::RCP<Coupling::Adapter::MatrixColTransform> aigtransform_;

    Teuchos::RCP<Coupling::Adapter::MatrixColTransform> fmiitransform_;
    Teuchos::RCP<Coupling::Adapter::MatrixColTransform> fmgitransform_;

    Teuchos::RCP<Coupling::Adapter::MatrixColTransform> fsaigtransform_;
    Teuchos::RCP<Coupling::Adapter::MatrixColTransform> fsmgitransform_;

    ///@}

    /// @name infnorm scaling

    Teuchos::RCP<Epetra_Vector> srowsum_;
    Teuchos::RCP<Epetra_Vector> scolsum_;
    Teuchos::RCP<Epetra_Vector> arowsum_;
    Teuchos::RCP<Epetra_Vector> acolsum_;

    //@}

    /// @name Some quantities to recover the Langrange multiplier at the end of each time step

    //! Lagrange multiplier \f$\lambda_\Gamma^n\f$ at the interface (ie condensed forces onto the
    //! structure) evaluated at old time step \f$t_n\f$ but needed for next time step \f$t_{n+1}\f$
    // lambda lives at the slave side (here at stucture)
    Teuchos::RCP<Epetra_Vector> lambda_;

    //! interface force \f$f_{\Gamma,i+1}^{S,n+1}\f$ onto the structure at current iteration
    //! \f$i+1\f$
    // Teuchos::RCP<const Epetra_Vector> fgcur_;

    //! interface force \f$f_{\Gamma,i}^{S,n+1}\f$ onto the structure at previous iteration \f$i\f$
    // Teuchos::RCP<const Epetra_Vector> fgpre_;

    //! inner structural displacement increment \f$\Delta(\Delta d_{I,i+1}^{n+1})\f$ at current
    //! iteration \f$i+1\f$
    Teuchos::RCP<Epetra_Vector> ddiinc_;

    //! inner displacement solution of the structure at previous iteration
    Teuchos::RCP<const Epetra_Vector> solipre_;

    //! structural interface displacement increment \f$\Delta(\Delta d_{\Gamma,i+1}^{n+1})\f$ at
    //! current iteration \f$i+1\f$
    Teuchos::RCP<Epetra_Vector> ddginc_;

    //! interface displacement solution of the structure at previous iteration
    Teuchos::RCP<const Epetra_Vector> solgpre_;

    //! block \f$S_{\Gamma I,i+1}\f$ of structural matrix at current iteration \f$i+1\f$
    Teuchos::RCP<const Core::LinAlg::SparseMatrix> sgicur_;

    //! block \f$S_{\Gamma\Gamma,i+1}\f$ of structural matrix at current iteration \f$i+1\f$
    Teuchos::RCP<const Core::LinAlg::SparseMatrix> sggcur_;
    //@}
  };
}  // namespace FSI

FOUR_C_NAMESPACE_CLOSE

#endif

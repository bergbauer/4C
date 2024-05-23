/*----------------------------------------------------------------------*/
/*! \file

\brief Fluid field adapter for fsi. Can only be used in conjunction with XFluid!

\level 1


*/
/*----------------------------------------------------------------------*/
#ifndef FOUR_C_ADAPTER_FLD_FLUID_XFSI_HPP
#define FOUR_C_ADAPTER_FLD_FLUID_XFSI_HPP

#include "4C_config.hpp"

#include "4C_adapter_fld_wrapper.hpp"
#include "4C_fluid_utils_mapextractor.hpp"
#include "4C_fluid_xfluid.hpp"
#include "4C_linalg_mapextractor.hpp"

#include <Epetra_Map.h>
#include <Epetra_Vector.h>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN

// forward declarations

namespace CORE::LINALG
{
  class Solver;
  class MapExtractor;
}  // namespace CORE::LINALG

namespace IO
{
  class DiscretizationWriter;
}

namespace FLD
{
  class XFluid;
  namespace UTILS
  {
    class MapExtractor;
  }
}  // namespace FLD

namespace XFEM
{
  class MeshCouplingFSI;
}

namespace ADAPTER
{
  class XFluidFSI : public FluidWrapper
  {
   public:
    /// Constructor
    XFluidFSI(Teuchos::RCP<Fluid> fluid,
        const std::string coupling_name,  // name of the FSI coupling condition
        Teuchos::RCP<CORE::LINALG::Solver> solver, Teuchos::RCP<Teuchos::ParameterList> params,
        Teuchos::RCP<IO::DiscretizationWriter> output);

    /// initialize algorithm
    void Init() override;

    /// communication object at the interface
    virtual Teuchos::RCP<FLD::UTILS::MapExtractor> const& StructInterface() const
    {
      return structinterface_;
    }

    /// communication object at the interface
    Teuchos::RCP<FLD::UTILS::MapExtractor> const& Interface() const override { return interface_; }

    /// communication object at the interface without pressure dofs for FPSI problems
    Teuchos::RCP<FLD::UTILS::MapExtractor> const& FPSIInterface() const override
    {
      return fpsiinterface_;
    }

    /// Velocity-displacement conversion at the fsi interface
    double TimeScaling() const override;

    /// Return interface forces
    virtual Teuchos::RCP<Epetra_Vector> extract_struct_interface_forces();

    /// Return interface velocity at old time level n
    virtual Teuchos::RCP<Epetra_Vector> extract_struct_interface_veln();

    /// Return interface velocity at new time level n+1
    virtual Teuchos::RCP<Epetra_Vector> extract_struct_interface_velnp()
    {
      FOUR_C_THROW("Not implemented, yet!");
      return Teuchos::null;
    }

    /// apply the interface velocities to the fluid
    virtual void apply_struct_interface_velocities(Teuchos::RCP<Epetra_Vector> ivel);

    /// apply the interface displacements to the fluid
    virtual void apply_struct_mesh_displacement(Teuchos::RCP<const Epetra_Vector> interface_disp);

    /// convert increment of displacement to increment in velocity
    void displacement_to_velocity(Teuchos::RCP<Epetra_Vector> fcx) override;

    /// Apply initial mesh displacement
    void apply_initial_mesh_displacement(Teuchos::RCP<const Epetra_Vector> initfluiddisp) override
    {
      FOUR_C_THROW("Not implemented, yet!");
    }

    /// apply the interface displacements to the fluid
    void apply_mesh_displacement(Teuchos::RCP<const Epetra_Vector> fluiddisp) override;

    void SetMeshMap(Teuchos::RCP<const Epetra_Map> mm, const int nds_master = 0) override;

    /// return coupling matrix between fluid and structure as sparse matrices
    Teuchos::RCP<CORE::LINALG::SparseMatrix> c_struct_fluid_matrix();
    Teuchos::RCP<CORE::LINALG::SparseMatrix> c_fluid_struct_matrix();
    Teuchos::RCP<CORE::LINALG::SparseMatrix> c_struct_struct_matrix();

    Teuchos::RCP<const Epetra_Vector> RHS_Struct_Vec();

    Teuchos::RCP<FLD::XFluid> MyFluid() { return xfluid_; }

    /// return boundary discretization
    Teuchos::RCP<DRT::Discretization> boundary_discretization();

    bool newton_restart_monolithic() { return xfluid_->newton_restart_monolithic(); }

    Teuchos::RCP<std::map<int, int>> GetPermutationMap() { return xfluid_->GetPermutationMap(); }

    /// GmshOutput for background mesh and cut mesh
    void GmshOutput(const std::string& name,  ///< name for output file
        const int step,                       ///< step number
        const int count,                      ///< counter for iterations within a global time step
        Teuchos::RCP<Epetra_Vector> vel,      ///< vector holding velocity and pressure dofs
        Teuchos::RCP<Epetra_Vector> acc = Teuchos::null  ///< vector holding accelerations
    );

   protected:
    /// A casted pointer to the fluid itself
    Teuchos::RCP<FLD::XFluid> xfluid_;

    /// the interface map setup for fsi interface, free surface, interior translation
    Teuchos::RCP<FLD::UTILS::MapExtractor> interface_;

    /// the interface map setup for fsi interface, free surface, interior translation
    Teuchos::RCP<FLD::UTILS::MapExtractor> structinterface_;

    /// the interface map setup for fpsi interface
    Teuchos::RCP<FLD::UTILS::MapExtractor> fpsiinterface_;

    /// ALE dof map
    Teuchos::RCP<CORE::LINALG::MapExtractor> meshmap_;
    Teuchos::RCP<Epetra_Map> permfluidmap_;
    Teuchos::RCP<Epetra_Map> fullfluidmap_;

    //! @name local copies of input parameters
    std::string coupling_name_;  /// the name of the XFEM::MeshCoupling object
    Teuchos::RCP<XFEM::MeshCouplingFSI> mesh_coupling_fsi_;
    Teuchos::RCP<CORE::LINALG::Solver> solver_;
    Teuchos::RCP<Teuchos::ParameterList> params_;
    //@}
  };
}  // namespace ADAPTER

FOUR_C_NAMESPACE_CLOSE

#endif

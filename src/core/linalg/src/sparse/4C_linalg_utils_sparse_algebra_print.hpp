/*----------------------------------------------------------------------*/
/*! \file

\brief A collection of sparse matrix printing methods for namespace Core::LinAlg

\level 0
*/
/*----------------------------------------------------------------------*/

#ifndef FOUR_C_LINALG_UTILS_SPARSE_ALGEBRA_PRINT_HPP
#define FOUR_C_LINALG_UTILS_SPARSE_ALGEBRA_PRINT_HPP

#include "4C_config.hpp"

#include <Epetra_CrsGraph.h>
#include <Epetra_CrsMatrix.h>
#include <Epetra_Map.h>
#include <Epetra_MultiVector.h>
#include <Epetra_Vector.h>
#include <Teuchos_RCPDecl.hpp>

FOUR_C_NAMESPACE_OPEN

// Forward declarations
namespace Core::LinAlg
{
  class BlockSparseMatrixBase;
}  // namespace Core::LinAlg

namespace Core::LinAlg
{
  /*!
   \brief Print sparsity pattern of a matrix to postscript file

   creates a file Epetra::Matrix.ps in current directory where the exact
   name of file depends on the exact type of class.

   \note works in parallel and serial!
   */
  void PrintSparsityToPostscript(const Epetra_RowMatrix& A);

  //! Print content of @p sparsematrix in Matlab format to file @p filename. Create new file or
  //! overwrite exisiting one if @p newfile is true
  void PrintMatrixInMatlabFormat(
      const std::string& filename, const Epetra_CrsMatrix& sparsematrix, const bool newfile = true);

  //! Print content of @p blockmatrix in Matlab format to file @p filename
  void PrintBlockMatrixInMatlabFormat(
      const std::string& filename, const BlockSparseMatrixBase& blockmatrix);

  //! Print content of @p vector in Matlab format to file @p filename. Create new file or overwrite
  //! exisiting one if @p newfile is true
  void PrintVectorInMatlabFormat(
      const std::string& filename, const Epetra_Vector& vector, const bool newfile = true);

  //! Print content of @p map in Matlab format to file @p filename. Create new file or overwrite
  //! exisiting one if @p newfile is true
  void PrintMapInMatlabFormat(
      const std::string& filename, const Epetra_Map& map, const bool newfile = true);

  /*!
  \brief Write matrix to ASCII-file via Xpetra::IO

  Write using \c Xpetra::IO<>::Write() to be compatible with \c Xpetra::IO<>::Read().

  \note This is a utility function for debugging, e.g. to dump matrices and read them in some
  other code.

  @param[in] filename Name of output file
  @param[in] matrix Matrix to be written
  */
  void WriteEpetraCrsMatrixAsXpetra(
      const std::string& filename, Teuchos::RCP<Epetra_CrsMatrix> matrix);

  /*!
  \brief Write MultiVector to ASCII-file via Xpetra::IO

  Write using \c Xpetra::IO<>::Write() to be compatible with \c Xpetra::IO<>::Read().

  \note This is a utility function for debugging, e.g. to dump MultiVectors and read them in some
  other code.

  @param[in] filename Name of output file
  @param[in] vec MultiVector to be written
  */
  void WriteEpetraMultiVectorAsXpetra(
      const std::string& filename, Teuchos::RCP<Epetra_MultiVector> vec);

}  // namespace Core::LinAlg

FOUR_C_NAMESPACE_CLOSE

#endif
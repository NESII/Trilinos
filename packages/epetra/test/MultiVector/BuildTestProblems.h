//@HEADER
// ************************************************************************
// 
//          Trilinos: An Object-Oriented Solver Framework
//              Copyright (2001) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//   
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//   
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
// 
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
// 
// ************************************************************************
//@HEADER

#include "Epetra_Map.h"
#include "Epetra_MultiVector.h"

int  BuildMatrixTests (Epetra_MultiVector & C,
			     const char transa, const char transb, 
			     const double alpha, 
			     Epetra_MultiVector& A, 
			     Epetra_MultiVector& B,
			     const double beta,
			     Epetra_MultiVector& C_GEMM );

  
int  BuildMultiVectorTests (Epetra_MultiVector & C,
				const double alpha, 
				Epetra_MultiVector& A, 
				Epetra_MultiVector& sqrtA,
				Epetra_MultiVector& B,
				Epetra_MultiVector& C_alphaA,
				Epetra_MultiVector& C_alphaAplusB,
				Epetra_MultiVector& C_plusB,
				double* const dotvec_AB,
				double* const norm1_A,
				double* const norm2_sqrtA,
				double* const norminf_A,
				double* const normw_A,
				Epetra_MultiVector& Weights,
				double* const minval_A,
				double* const maxval_A,
				double* const meanval_A );  
  

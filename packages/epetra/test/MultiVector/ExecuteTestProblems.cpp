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

#include "Epetra_BLAS.h"
#include "ExecuteTestProblems.h"
#include "BuildTestProblems.h"
#include "Epetra_Comm.h"
#include "Epetra_Vector.h"
  int MatrixTests(const Epetra_BlockMap & Map, const Epetra_LocalMap & LocalMap, int NumVectors,
		      bool verbose)
  {
    const Epetra_Comm & Comm = Map.Comm();
    int ierr = 0, i;
    int IndexBase = 0;
    double *residual = new double[NumVectors];

    /* get ID of this processor */

    int MyPID   = Comm.MyPID();


    // Test GEMM first.  7 cases:
    
    //                                       Num
    //     OPERATIONS                        case  Notes
    // 1) C(local) = A^X(local) * B^X(local)  4   (X=Trans or Not, No Comm needed) 
    // 2) C(local) = A^T(distr) * B  (distr)  1   (2D dot product, replicate C)
    // 3) C(distr) = A  (distr) * B^X(local)  2   (2D vector update, no Comm needed)

    // ==================================================================
    // Case 1 through 4 (A, B, C all local) Strided and non-strided cases
    // ==================================================================

    // Construct MultiVectors

    {
    Epetra_MultiVector A(LocalMap, NumVectors);
    Epetra_MultiVector B(LocalMap, NumVectors);
    Epetra_LocalMap  Map2d(NumVectors, IndexBase, Comm);
    Epetra_MultiVector C(Map2d, NumVectors);
    Epetra_MultiVector C_GEMM(Map2d, NumVectors);

    double **App, **Bpp, **Cpp;
    
    Epetra_MultiVector *Ap, *Bp, *Cp;

    // For testing non-strided mode, create MultiVectors that are scattered throughout memory

    App = new double *[NumVectors];
    Bpp = new double *[NumVectors];
    Cpp = new double *[NumVectors];
    for (i=0; i<NumVectors; i++) App[i] = new double[A.MyLength()+i];
    for (i=0; i<NumVectors; i++) Bpp[i] = new double[B.MyLength()+i];
    for (i=0; i<NumVectors; i++) Cpp[i] = new double[C.MyLength()+i];
    
    Epetra_MultiVector A1(View, LocalMap, App, NumVectors);
    Epetra_MultiVector B1(View, LocalMap, Bpp, NumVectors);
    Epetra_MultiVector C1(View, Map2d, Cpp, NumVectors);

    for (int strided = 0; strided<2; strided++) {
      int ierr;
    // Loop through all trans cases using a variety of values for alpha and beta
    for (i=0; i<4; i++)  {
	ierr = 0;
	char transa = 'N'; if (i>1) transa = 'T';
	char transb = 'N'; if (i%2!=0) transb = 'T';
	double alpha = (double) i+1;
	double beta  = (double) (i/2);
	EPETRA_TEST_ERR(C.Random(),ierr);  // Fill C with random numbers
	ierr += BuildMatrixTests(C,transa, transb, alpha, A, B, beta, C_GEMM );
	if (strided)
	  {
	    Ap = &A; Bp = &B; Cp = &C;
	  }
	else
	  {
	    A.ExtractCopy(App); Ap = &A1;
	    B.ExtractCopy(Bpp); Bp = &B1;
	    C.ExtractCopy(Cpp); Cp = &C1;
	  }
	  
	ierr += Cp->Multiply(transa, transb, alpha, *Ap, *Bp, beta);
	ierr += Cp->Update(-1.0, C_GEMM, 1.0);
	ierr += Cp->Norm2(residual);

	if (verbose && ierr==0)
	  {
	    cout << "XXXXX Replicated Local MultiVector GEMM tests";
	    if (strided)
	    cout << " (Strided Multivectors)" << endl;
	    else
	    cout << " (Non-Strided Multivectors)" << endl;
	    cout << "  alpha = " << alpha << ",  beta = " << beta <<", transa = "<<transa
		 <<", transb = " << transb;
	  }
	if (ierr==0 && BadResidual(verbose,residual, NumVectors)) return(-1);
      }

      }
    for (i=0; i<NumVectors; i++)
      {
	delete [] App[i];
	delete [] Bpp[i];
	delete [] Cpp[i];
      }
    delete [] App;
    delete [] Bpp;
    delete [] Cpp;
    }
      
    // ====================================
    // Case 5  (A, B distributed C  local)
    // ====================================

    // Construct MultiVectors
  {
    Epetra_MultiVector A(Map, NumVectors);
    Epetra_MultiVector B(Map, NumVectors);
    Epetra_LocalMap Map2d(NumVectors, IndexBase, Comm);
    Epetra_MultiVector C(Map2d, NumVectors);
    Epetra_MultiVector C_GEMM(Map2d, NumVectors);

    char transa = 'T';
    char transb = 'N';
    double alpha = 2.0;
    double beta  = 1.0;
    EPETRA_TEST_ERR(C.Random(),ierr);  // Fill C with random numbers
    ierr += BuildMatrixTests(C, transa, transb, alpha, A, B, beta, C_GEMM );
    ierr += C.Multiply(transa, transb, alpha, A, B, beta);
    ierr += C.Update(-1.0, C_GEMM, 1.0);
    ierr += C.Norm2(residual);

    if (verbose && ierr==0)
      {
	cout << "XXXXX Generalized 2D dot product via GEMM call     " << endl;
	cout << "  alpha = " << alpha << ",  beta = " << beta <<", transa = "<<transa
	     <<", transb = " << transb;
      }
    if (BadResidual(verbose,residual, NumVectors)) return(-1);
    
    
  }      
    // ====================================
    // Case 6-7  (A, C distributed, B local)
    // ====================================

    // Construct MultiVectors
  {
    Epetra_MultiVector A(Map, NumVectors);
    Epetra_LocalMap Map2d(NumVectors, IndexBase, Comm);
    Epetra_MultiVector B(Map2d, NumVectors);
    Epetra_MultiVector C(Map, NumVectors);
    Epetra_MultiVector C_GEMM(Map, NumVectors);

    for (i=0; i<2; i++)
      {
	char transa = 'N';
	char transb = 'N'; if (i>0) transb = 'T';
	double alpha = 2.0;
	double beta  = 1.1;
	EPETRA_TEST_ERR(C.Random(),ierr);  // Fill C with random numbers
	ierr += BuildMatrixTests(C,transa, transb, alpha, A, B, beta, C_GEMM );
	ierr += C.Multiply(transa, transb, alpha, A, B, beta);
	ierr += C.Update(-1.0, C_GEMM, 1.0);
	ierr += C.Norm2(residual);
	
	if (verbose)
	  {
	    cout << "XXXXX Generalized 2D vector update via GEMM call     " << endl;
	    cout << "  alpha = " << alpha << ",  beta = " << beta <<", transa = "<<transa
		 <<", transb = " << transb;
	  }
	if (BadResidual(verbose,residual, NumVectors)) return(-1);
      }

    delete [] residual;
    
    return(ierr);
  }
  }

int MultiVectorTests(const Epetra_BlockMap & Map, int NumVectors, bool verbose)
{
  const Epetra_Comm & Comm = Map.Comm();
  int ierr = 0, i;
  double *residual = new double[NumVectors];
  
  Epetra_BLAS BLAS;
  /* get number of processors and the name of this processor */
  
  // int NumProc = Comm.getNumProc();
  int MyPID   = Comm.MyPID();
  
  // Construct MultiVectors
  
  Epetra_MultiVector A(Map, NumVectors);
  Epetra_MultiVector sqrtA(Map, NumVectors);
  Epetra_MultiVector B(Map, NumVectors);
  Epetra_MultiVector C(Map, NumVectors);
  Epetra_MultiVector C_alphaA(Map, NumVectors);
  Epetra_MultiVector C_alphaAplusB(Map, NumVectors);
  Epetra_MultiVector C_plusB(Map, NumVectors);
  Epetra_MultiVector Weights(Map, NumVectors);
  
  // Construct double vectors
  double *dotvec_AB   = new double[NumVectors];
  double *norm1_A     = new double[NumVectors];
  double *norm2_sqrtA = new double[NumVectors];
  double *norminf_A = new double[NumVectors];
  double *normw_A = new double[NumVectors];
  double *minval_A = new double[NumVectors];
  double *maxval_A = new double[NumVectors];
  double *meanval_A = new double[NumVectors];
  
  // Generate data 

  
  EPETRA_TEST_ERR(C.Random(),ierr); // Fill C with random numbers.
  double alpha = 2.0;
  BuildMultiVectorTests (C,alpha, A, sqrtA, B, C_alphaA, C_alphaAplusB,
			     C_plusB, dotvec_AB, norm1_A, norm2_sqrtA, norminf_A, 
			     normw_A, Weights, minval_A, maxval_A, meanval_A);

  int err = 0;
  if (verbose) cout << "XXXXX Testing alpha * A     ";
  // Test alpha*A
  Epetra_MultiVector alphaA(A); // Copy of A
  EPETRA_TEST_ERR(alphaA.Scale(alpha),err);
  EPETRA_TEST_ERR(alphaA.Update(-1.0, C_alphaA, 1.0),err);
  EPETRA_TEST_ERR(alphaA.Norm2(residual),err);

  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }

  err = 0;
  if (verbose) cout << "XXXXX Testing C = alpha * A + B      ";
  // Test alpha*A + B
  Epetra_MultiVector alphaAplusB(A); // Copy of A
  EPETRA_TEST_ERR(alphaAplusB.Update(1.0, B, alpha, A, 0.0),err);
  EPETRA_TEST_ERR(alphaAplusB.Update(-1.0, C_alphaAplusB, 1.0),err);
  EPETRA_TEST_ERR(alphaAplusB.Norm2(residual),err);
  
  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }

  err = 0;
  if (verbose) cout << "XXXXX Testing C += B      ";
  // Test + B
  Epetra_MultiVector plusB(C); // Copy of C
  EPETRA_TEST_ERR(plusB.Update(1.0, B, 1.0),err);
  EPETRA_TEST_ERR(plusB.Update(-1.0, C_plusB, 1.0),err);
  EPETRA_TEST_ERR(plusB.Norm2(residual),err);
  
  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }

  err = 0;
  if (verbose) cout << "XXXXX Testing A.dotProd(B)     ";
  // Test A.dotvec(B)
  double *dotvec = residual;
  EPETRA_TEST_ERR(A.Dot(B,dotvec),err);
  BLAS.AXPY(NumVectors,-1.0,dotvec_AB,dotvec);
  
  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }

  err = 0;
  if (verbose) cout << "XXXXX Testing norm1_A      ";
  // Test A.norm1()
  double *norm1 = residual;
  EPETRA_TEST_ERR(A.Norm1(norm1),err);
  BLAS.AXPY(NumVectors,-1.0,norm1_A,norm1);
  
  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }  

  err = 0;
  if (verbose) cout << "XXXXX Testing norm2_sqrtA     ";
  // Test sqrtA.norm2()
  double *norm2 = residual;
  EPETRA_TEST_ERR(sqrtA.Norm2(norm2),err);
  BLAS.AXPY(NumVectors,-1.0,norm2_sqrtA,norm2);
  
  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }

  err = 0;
  if (verbose) cout << "XXXXX Testing norminf_A     ";
  // Test A.norminf()
  double *norminf = residual;
  EPETRA_TEST_ERR(A.NormInf(norminf),err);
  BLAS.AXPY(NumVectors,-1.0,norminf_A,norminf);
  
  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }
  
  err = 0;
  if (verbose) cout << "XXXXX Testing normw_A     ";
  // Test A.NormWeighted()
  double *normw = residual;
  EPETRA_TEST_ERR(A.NormWeighted(Weights, normw),err);
  BLAS.AXPY(NumVectors,-1.0,normw_A,normw);
  
  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }
  
  err = 0;
  if (verbose) cout << "XXXXX Testing minval_A     ";
  // Test A.MinValue()
  double *minval = residual;
  EPETRA_TEST_ERR(A.MinValue(minval),err);
  BLAS.AXPY(NumVectors,-1.0,minval_A,minval);
  
  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }

  err = 0;
  if (verbose) cout << "XXXXX Testing maxval_A     ";
  // Test A.MaxValue()
  double *maxval = residual;
  EPETRA_TEST_ERR(A.MaxValue(maxval),err);
  BLAS.AXPY(NumVectors,-1.0,maxval_A,maxval);

  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }

  err = 0;
  if (verbose) cout << "XXXXX Testing meanval_A     ";
  // Test A.MeanValue()
  double *meanval = residual;
  EPETRA_TEST_ERR(A.MeanValue(meanval),err);
  BLAS.AXPY(NumVectors,-1.0,meanval_A,meanval);
  
  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }

  err = 0;
  if (verbose) cout << "XXXXX Testing abs_A     ";
  // Test A.Abs()
  Epetra_MultiVector Abs_A = A;
  EPETRA_TEST_ERR(Abs_A.Abs(A),err);
  EPETRA_TEST_ERR(Abs_A.Update(1.0, A, -1.0),err); // Abs_A = A - Abs_A (should be zero since A > 0)
  EPETRA_TEST_ERR(Abs_A.Norm2(residual),err);

  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual(verbose,residual, NumVectors),ierr);
  }
  
  err = 0;
  if (verbose) cout << "XXXXX Testing random_A     ";
  // Test A.Random()
  Epetra_MultiVector Rand1_A(A);
  Epetra_MultiVector Rand2_A(A);
  EPETRA_TEST_ERR(Rand1_A.Random(),err);
  EPETRA_TEST_ERR(Rand2_A.Random(),err);
  // Rand2_A = Rand1_A - Rand2_A (should be nonzero since Random() should give different vectors > 0)
  EPETRA_TEST_ERR(Rand2_A.Update(1.0, Rand1_A, -1.0),err); 
  EPETRA_TEST_ERR(Rand2_A.Norm2(residual),err);

  if (err) ierr += err;
  else {
    EPETRA_TEST_ERR(BadResidual1(verbose,residual, NumVectors),ierr);
  }
  
  // Delete everything
  
  delete [] dotvec_AB;
  delete [] norm1_A;
  delete [] norm2_sqrtA;
  delete [] norminf_A;
  delete [] normw_A;
  delete [] minval_A;
  delete [] maxval_A;
  delete [] meanval_A;
  delete [] residual;

  //*******************************************************************
  // Post-construction modification tests
  //*******************************************************************
  
  if (verbose) cout <<  "\n\nXXXXX Testing Post-construction modification of a multivector"
		    <<endl<<endl;

  err = 0;

  Epetra_MultiVector X(Map, NumVectors);
  X.Random();

  // Pick middle range values for GID, LID and Vector Index
  int testGID = Map.NumGlobalElements()/2;
  int testVecIndex = NumVectors/2;

  int GIDSize = 1;
  int LIDOfGID = 0;
  int FirstEntryOfGID = 0;

  if (Map.MyGID(testGID)) {
    LIDOfGID = Map.LID(testGID);
    GIDSize = Map.ElementSize(LIDOfGID);
    FirstEntryOfGID = Map.FirstPointInElement(LIDOfGID);
  }

  // ========================================================================
  // Test int ReplaceGlobalValue (int GlobalRow, int VectorIndex, double ScalarValue)
  // ========================================================================

  double newGIDValue = 4.0;
  int locerr = X.ReplaceGlobalValue(testGID, testVecIndex, newGIDValue);

  if (Map.MyGID(testGID)) {
    if (X[testVecIndex][FirstEntryOfGID]!=newGIDValue) err++;
    if (verbose) cout << "X["<<testVecIndex<<"]["<<FirstEntryOfGID<<"] = "
		      <<  X[testVecIndex][FirstEntryOfGID]
		      << " should = " << newGIDValue << endl;
  }
  else
    if (locerr!=1) err++; // Test for GID out of range error (=1)

  // ========================================================================
  // Test int ReplaceGlobalValue (int GlobalRow, intBlockRowOffset, int VectorIndex, double ScalarValue)
  // ========================================================================
  newGIDValue = 8.0;
  locerr = X.ReplaceGlobalValue(testGID, GIDSize-1, testVecIndex, newGIDValue);

  if (Map.MyGID(testGID)) {
    if (X[testVecIndex][FirstEntryOfGID+GIDSize-1]!=newGIDValue) err++;
    if (verbose) cout << "X["<<testVecIndex<<"]["<<FirstEntryOfGID+GIDSize-1<<"] = "
		      <<  X[testVecIndex][FirstEntryOfGID+GIDSize-1]
		      << " should = " << newGIDValue << endl;
  }
  else
    if (locerr!=1) err++; // Test for GID out of range error (=1)
  
  // ========================================================================
  // Test int SumIntoGlobalValue (int GlobalRow, int VectorIndex, double ScalarValue)
  // ========================================================================

  newGIDValue = 1.0;
  locerr = X.ReplaceGlobalValue(testGID, testVecIndex, newGIDValue);
  locerr = X.SumIntoGlobalValue(testGID, testVecIndex, newGIDValue);
  if (Map.MyGID(testGID)) {
    if (X[testVecIndex][FirstEntryOfGID]!=(newGIDValue+newGIDValue)) err++;
    if (verbose) cout << "X["<<testVecIndex<<"]["<<FirstEntryOfGID<<"] = "
		      <<  X[testVecIndex][FirstEntryOfGID]
		      << " should = " << newGIDValue << endl;
  }
  else 
    if (locerr!=1) err++; // Test for GID out of range error (=1)
    
  // ========================================================================
  // Test int SumIntoGlobalValue (int GlobalRow, intBlockRowOffset, int VectorIndex, double ScalarValue)
  // ========================================================================
    
  newGIDValue = 1.0;
  locerr = X.ReplaceGlobalValue(testGID, GIDSize-1, testVecIndex, newGIDValue);
  locerr = X.SumIntoGlobalValue(testGID, GIDSize-1, testVecIndex, newGIDValue);

  if (Map.MyGID(testGID)) {
    if (X[testVecIndex][FirstEntryOfGID+GIDSize-1]!=(newGIDValue+newGIDValue)) err++;
    if (verbose) cout << "X["<<testVecIndex<<"]["<<FirstEntryOfGID+GIDSize-1<<"] = "
		      <<  X[testVecIndex][FirstEntryOfGID+GIDSize-1]
		      << " should = " << newGIDValue << endl;
  }
  else
    if (locerr!=1) err++; // Test for GID out of range error (=1)

  // ========================================================================
  // Test Local "My" versions of same routine (less complicated)
  // ========================================================================
  
  // Pick middle range values for LID
  int testLID = Map.NumMyElements()/2;

  int LIDSize = Map.ElementSize(testLID);
  int FirstEntryOfLID = Map.FirstPointInElement(testLID);


  double newLIDValue = 4.0;
  locerr = X.ReplaceMyValue(testLID, testVecIndex, newLIDValue);

  if (X[testVecIndex][FirstEntryOfLID]!=newLIDValue) err++;
  if (verbose) cout << "X["<<testVecIndex<<"]["<<FirstEntryOfLID<<"] = "
		    <<  X[testVecIndex][FirstEntryOfLID]
		    << " should = " << newLIDValue << endl;
  
  newLIDValue = 8.0;
  locerr = X.ReplaceMyValue(testLID, LIDSize-1, testVecIndex, newLIDValue);
  if (X[testVecIndex][FirstEntryOfLID+LIDSize-1]!=newLIDValue) err++;
  if (verbose) cout << "X["<<testVecIndex<<"]["<<FirstEntryOfLID+LIDSize-1<<"] = "
		    <<  X[testVecIndex][FirstEntryOfLID+LIDSize-1]
		    << " should = " << newLIDValue << endl;
  newLIDValue = 1.0;
  locerr = X.ReplaceMyValue(testLID, testVecIndex, newLIDValue);
  locerr = X.SumIntoMyValue(testLID, testVecIndex, newLIDValue);
  if (X[testVecIndex][FirstEntryOfLID]!=(newLIDValue+newLIDValue)) err++;
  if (verbose) cout << "X["<<testVecIndex<<"]["<<FirstEntryOfLID<<"] = "
		    <<  X[testVecIndex][FirstEntryOfLID]
		    << " should = " << newLIDValue << endl;
  newLIDValue = 2.0;
  locerr = X.ReplaceMyValue(testLID, LIDSize-1, testVecIndex, newLIDValue);
  locerr = X.SumIntoMyValue(testLID, LIDSize-1, testVecIndex, newLIDValue);
  if (verbose) cout << "X["<<testVecIndex<<"]["<<FirstEntryOfLID+LIDSize-1<<"] = "
		    <<  X[testVecIndex][FirstEntryOfLID+LIDSize-1]
		    << " should = " << newLIDValue << endl;
  if (X[testVecIndex][FirstEntryOfLID+LIDSize-1]!=(newLIDValue+newLIDValue)) err++;

  ierr += err;

  // ========================================================================
  // Test Post-construction modification of an Epetra_Vector using a vector
  // our multivector X
  // ========================================================================

  if (verbose) cout <<  "\n\nXXXXX Testing Post-construction modification of a vector"
		    << endl << endl;

  Epetra_Vector * x = X(testVecIndex);

  int NumEntries = 2;
  double * VecValues = new double[NumEntries];
  int * VecGIDs = new int[NumEntries];
  VecGIDs[0] = testGID;
  VecGIDs[1] = testGID+1; // Some pathological chance that these GIDs are not valid

  // ========================================================================
  // Test int ReplaceGlobalValues (int NumEntries, double *Values, int *Indices)
  // ========================================================================

  VecValues[0] = 2.0; VecValues[1] = 4.0;
  locerr = x->ReplaceGlobalValues(NumEntries, VecValues, VecGIDs);

  for (i=0; i<NumEntries; i++) {
    testGID = VecGIDs[i];
    if (Map.MyGID(testGID)) {
      LIDOfGID = Map.LID(testGID);
      GIDSize = EPETRA_MIN(GIDSize,Map.ElementSize(LIDOfGID)); // Need this value below
      FirstEntryOfGID = Map.FirstPointInElement(LIDOfGID);
      if ((*x)[FirstEntryOfGID]!=VecValues[i]) err++;
      if (verbose) cout << "x["<<FirstEntryOfGID<<"] = "
			<< (*x)[FirstEntryOfGID] 
			<< " should = " << VecValues[i] << endl;
    }
    else
      if (locerr!=1) err++; // Test for GID out of range error (=1)
  }


  // ========================================================================
  // Test int ReplaceGlobalValues (int NumEntries, int BlockOffset, double *Values, int *Indices)
  // ========================================================================

  VecValues[0] = 4.0; VecValues[1] = 8.0;
  locerr = x->ReplaceGlobalValues(NumEntries, GIDSize-1, VecValues, VecGIDs);

  for (i=0; i<NumEntries; i++) {
    testGID = VecGIDs[i];
    if (Map.MyGID(testGID)) {
      LIDOfGID = Map.LID(testGID);
      FirstEntryOfGID = Map.FirstPointInElement(LIDOfGID);
      if ((*x)[FirstEntryOfGID+GIDSize-1]!=VecValues[i]) err++;
      if (verbose) cout << "x["<<FirstEntryOfGID+GIDSize-1<<"] = "
			<< (*x)[FirstEntryOfGID+GIDSize-1] 
			<< " should = " << VecValues[i] << endl;
    }
    else
      if (locerr!=1) err++; // Test for GID out of range error (=1)
  }

  // ========================================================================
  // Test int SumIntoGlobalValues (int NumEntries, double *Values, int *Indices)
  // ========================================================================

  VecValues[0] = 1.0; VecValues[1] = 2.0;
  locerr = x->ReplaceGlobalValues(NumEntries, VecValues, VecGIDs);
  locerr = x->SumIntoGlobalValues(NumEntries, VecValues, VecGIDs);

  for (i=0; i<NumEntries; i++) {
    testGID = VecGIDs[i];
    if (Map.MyGID(testGID)) {
      LIDOfGID = Map.LID(testGID);
      FirstEntryOfGID = Map.FirstPointInElement(LIDOfGID);
      if ((*x)[FirstEntryOfGID]!=(VecValues[i]+VecValues[i])) err++;
      if (verbose) cout << "x["<<FirstEntryOfGID<<"] = "
			<< (*x)[FirstEntryOfGID] 
			<< " should = " << (VecValues[i]+VecValues[i]) << endl;
    }
    else
      if (locerr!=1) err++; // Test for GID out of range error (=1)
  }
  // ========================================================================
  // Test int ReplaceGlobalValues (int NumEntries, int BlockOffset, double *Values, int *Indices)
  // ========================================================================

  VecValues[0] = 1.0; VecValues[1] = 2.0;
  locerr = x->ReplaceGlobalValues(NumEntries, GIDSize-1, VecValues, VecGIDs);
  locerr = x->SumIntoGlobalValues(NumEntries, GIDSize-1, VecValues, VecGIDs);

  for (i=0; i<NumEntries; i++) {
    testGID = VecGIDs[i];
    if (Map.MyGID(testGID)) {
      LIDOfGID = Map.LID(testGID);
      FirstEntryOfGID = Map.FirstPointInElement(LIDOfGID);
      if ((*x)[FirstEntryOfGID+GIDSize-1]!=(VecValues[i]+VecValues[i])) err++;
      if (verbose) cout << "x["<<FirstEntryOfGID+GIDSize-1<<"] = "
			<< (*x)[FirstEntryOfGID+GIDSize-1] 
			<< " should = " << (VecValues[i]+VecValues[i]) << endl;
    }
    else
      if (locerr!=1) err++; // Test for GID out of range error (=1)
  }

  // ========================================================================
  // Test Local "My" versions of same routine (less complicated)
  // ========================================================================
  int * VecLIDs = new int[NumEntries];
  VecLIDs[0] = testLID;
  VecLIDs[1] = testLID+1; // Some pathological chance that these LIDs are not valid

  VecValues[0] = 2.0; VecValues[1] = 4.0;
  locerr = x->ReplaceMyValues(NumEntries, VecValues, VecLIDs);

  for (i=0; i<NumEntries; i++) {
    testLID = VecLIDs[i];
    LIDSize = EPETRA_MIN(LIDSize,Map.ElementSize(testLID)); // Need this value below
    FirstEntryOfLID = Map.FirstPointInElement(testLID);
    if ((*x)[FirstEntryOfLID]!=VecValues[i]) err++;
    if (verbose) cout << "x["<<FirstEntryOfLID<<"] = "
		      << (*x)[FirstEntryOfLID] 
		      << " should = " << VecValues[i] << endl;
  }

  VecValues[0] = 4.0; VecValues[1] = 8.0;
  locerr = x->ReplaceMyValues(NumEntries, LIDSize-1, VecValues, VecLIDs);

  for (i=0; i<NumEntries; i++) {
    testLID = VecLIDs[i];
    LIDSize = EPETRA_MIN(LIDSize,Map.ElementSize(testLID)); // Need this value below
    FirstEntryOfLID = Map.FirstPointInElement(testLID);
    if ((*x)[FirstEntryOfLID+LIDSize-1]!=VecValues[i]) err++;
    if (verbose) cout << "x["<<FirstEntryOfLID+LIDSize-1<<"] = "
		      << (*x)[FirstEntryOfLID+LIDSize-1] 
		      << " should = " << VecValues[i] << endl;
  }

  VecValues[0] = 1.0; VecValues[1] = 1.0;
  locerr = x->ReplaceMyValues(NumEntries, VecValues, VecLIDs);
  locerr = x->SumIntoMyValues(NumEntries, VecValues, VecLIDs);

  for (i=0; i<NumEntries; i++) {
    testLID = VecLIDs[i];
    LIDSize = EPETRA_MIN(LIDSize,Map.ElementSize(testLID)); // Need this value below
    FirstEntryOfLID = Map.FirstPointInElement(testLID);
    if ((*x)[FirstEntryOfLID]!=(VecValues[i]+VecValues[i])) err++;
    if (verbose) cout << "x["<<FirstEntryOfLID<<"] = "
		      << (*x)[FirstEntryOfLID] 
		      << " should = " << (VecValues[i]+VecValues[i]) << endl;
  }

  VecValues[0] = 2.0; VecValues[1] = 4.0;
  locerr = x->ReplaceMyValues(NumEntries, LIDSize-1, VecValues, VecLIDs);
  locerr = x->SumIntoMyValues(NumEntries, LIDSize-1, VecValues, VecLIDs);

  for (i=0; i<NumEntries; i++) {
    testLID = VecLIDs[i];
    LIDSize = EPETRA_MIN(LIDSize,Map.ElementSize(testLID)); // Need this value below
    FirstEntryOfLID = Map.FirstPointInElement(testLID);
    if ((*x)[FirstEntryOfLID+LIDSize-1]!=(VecValues[i]+VecValues[i])) err++;
    if (verbose) cout << "x["<<FirstEntryOfLID+LIDSize-1<<"] = "
		      << (*x)[FirstEntryOfLID+LIDSize-1] 
		      << " should = " << (VecValues[i]+VecValues[i]) << endl;
  }

    delete [] VecValues;
    delete [] VecGIDs;
    delete [] VecLIDs;

  return(ierr);
}

int BadResidual(bool verbose, double * Residual, int NumVectors)
{
  double threshold = 5.0E-6;
  int ierr = 0;
  for (int i=0; i<NumVectors; i++) {
    if (Residual[i]>threshold) {
      ierr = 1;// Output will be more useful after returning from this method
      if (verbose) cout << endl << "     Residual[" << i <<"] = " << Residual[i];
    }
  }
  if (verbose)
    if (ierr==0) cout << "\t Checked OK" << endl;
  
  return(ierr);
}

// This version tests to make sure residuals are large (when we want vectors to be different)
int BadResidual1(bool verbose, double * Residual, int NumVectors)
{
  double threshold = 5.0E-6;
  int ierr = 0;
  for (int i=0; i<NumVectors; i++) {
    if (Residual[i]<threshold) {
      ierr = 1;// Output will be more useful after returning from this method
      if (verbose) cout << endl << "     Residual[" << i <<"] = " << Residual[i] << "  Should be larger";
    }
  }
  if (verbose)
    if (ierr==0) cout << "\t Checked OK" << endl;
  
  return(ierr);
}


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

#include "Epetra_Time.h"


//=============================================================================
Epetra_Time::Epetra_Time(const Epetra_Comm& Comm) 
  : Comm_(&Comm)
{
  StartTime_ = WallTime();
}
//=============================================================================
Epetra_Time::Epetra_Time(const Epetra_Time& Time) 
  : Comm_(Time.Comm_)
{
}
//=============================================================================
Epetra_Time::~Epetra_Time(void)  
{
}
//=============================================================================
double Epetra_Time::WallTime(void) const
{
#ifdef EPETRA_MPI

  return(MPI_Wtime());

#elif ICL

   clock_t start;
   //double duration;

   start = clock();
  return (double)( start ) / CLOCKS_PER_SEC;

#else

#ifndef MINGW
   struct timeval tp;
   static long start=0, startu;
   if (!start)
   {
      gettimeofday(&tp, NULL);
      start = tp.tv_sec;
      startu = tp.tv_usec;
      return(0.0);
   }
   gettimeofday(&tp, NULL);
   return( ((double) (tp.tv_sec - start)) + (tp.tv_usec-startu)/1000000.0 );
#else
   return( (double) clock() / CLOCKS_PER_SEC );
#endif

#endif

}
//=============================================================================
void Epetra_Time::ResetStartTime(void)
{
  StartTime_ = WallTime();
  return;
}
//=============================================================================
double Epetra_Time::ElapsedTime(void) const
{
  return(WallTime()-StartTime_);
}

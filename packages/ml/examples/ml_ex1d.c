#include <math.h>
#include "ml_include.h"
#ifdef ML_MPI
#include "mpi.h"
#endif

/*************************************************************************/
/*       Sample User programs                                            */
/*************************************************************************/

extern int myinterp(void *mydata, int, double p[], int, double ap[]);
extern int mymatvec(void *mydata, int, double p[], int, double ap[]);
extern int myrestrict(void *mydata, int, double p[], int, double ap[]);
extern int mysmooth(void *mydata, int, double x[], int, double rhs[]);
extern int myAgetrow(void *data, int N_requested_rows, int requested_rows[],
   int allocated_space, int columns[], double values[], int row_lengths[]);
extern int myRgetrow(void *data, int N_requested_rows, int requested_rows[],
   int allocated_space, int columns[], double values[], int row_lengths[]);
extern int myPgetrow(void *data, int N_requested_rows, int requested_rows[],
   int allocated_space, int columns[], double values[], int row_lengths[]);
extern double get_boundary(double x[], int size, int *processor_info);

#define PROC_ID   0
#define NUM_PROCS 1
#define COARSE_GRID 1
#define FINE_GRID 0

struct data {
      int *processor_info;
      int size, ntimes;
      int location;
      int from_size, to_size;
      int coarse_or_fine;
};

int myinterp(void *mydata, int leng1, double p[], int leng2, double ap[])
{
   int i, fine_i, fine_size, coarse_size, proc_id;
   double ghost;
   struct data *data;

   data = (struct data *) mydata;
   coarse_size = data->from_size;
   fine_size   = data->to_size;
   proc_id     = data->processor_info[PROC_ID];

   for (i = 0; i < fine_size; i++) ap[i] = 0.0;
   fine_i = 1 - proc_id;
   for (i = 0; i < coarse_size; i++) {
      ap[fine_i] += p[i];
      ap[fine_i+1] += .5*p[i];
      if (fine_i != 0) ap[fine_i-1] += .5*p[i];
      fine_i += 2;
   }
   ghost = get_boundary(p, coarse_size, data->processor_info);
   if (proc_id == 0) ap[fine_i - 1] += .5*ghost;

   return 0;
}
int my_comm(double *vec, void *idata)
{
   struct data *data;
   double ghost_value;
   int    proc_id;

   data    = (struct data *) idata;
   if (data->processor_info[NUM_PROCS] < 2 ) return 0;
   proc_id = data->processor_info[PROC_ID];
   ghost_value = get_boundary(vec, data->from_size, data->processor_info);

   if (proc_id == 0) {
      if ( data->from_size <=  data->to_size) 
         vec[data->from_size] = ghost_value;
   }
   else {
      if ( data->from_size >=  data->to_size) 
         vec[data->from_size] = ghost_value;
   }
   return 0;
}
int mynvertices(void *mydata) 
{
   struct data *data;
   data = (struct data *) mydata;
   if (data->coarse_or_fine == FINE_GRID) return(data->to_size);
   else return(data->to_size+2);
}
int myvertex_global_num(void *mydata, int vert) {
   struct data *data;
   data = (struct data *) mydata;
   if (data->processor_info[NUM_PROCS] > 1) 
      pr_error("myvertex_global_num only works with 1 processor\n");
   return(vert);
}
int myelement_global_num(void *mydata, int ele) { return(ele); }

int myvertex_coordinate(void *mydata, int vert, double list[]) { 
   struct data *data;
   int offset = 1;
   data = (struct data *) mydata;

   if (data->coarse_or_fine == COARSE_GRID) offset = 0;
   list[0] = ((double)(vert+offset))/ ((double)(data->to_size+1 )); 
   list[1] = 0.0;
   return(1); 
}
int mybasis_coefficients(void *mydata, int index, double coords[], int n_pts, double coefs[], int ptrs[])
{
   struct data *data;
   int i,count, oldcount;
   double left, right, dist, dtemp;
   data = (struct data *) mydata;

   left  = ((double)(index))/ ((double)(data->to_size+1)); 
   right = ((double)(index+1))/ ((double)(data->to_size+1)); 
   dist = right - left;
   count = 0; oldcount = count;
   for (i = 0; i < n_pts; i++) {
      if ((coords[2*i] < left) || (coords[2*i] > right)) {
         coefs[count++] = -1.;
      }
      else {
         dtemp = (coords[2*i] - left)/dist;
if (index == 0) coefs[count++] = 1.e-5;
else coefs[count++] = 1.-dtemp;
if (right > .99999999) coefs[count++] = 1.e-5;
else coefs[count++] = dtemp;
      }
      ptrs[i] = count - oldcount;
      oldcount = count;
   }
}


int mynelements(void *mydata) { return(mynvertices(mydata) +1 ); }
int myelement_nvertices(void *mydata) { return(2); }
int mydimension(void *mydata) { return(2); }
int myelement_vlist(void *mydata, int i,int list[]) { list[0] = i; list[1] = i+1; 
return(2); }

int mymatvec(void *mydata, int leng1, double p[], int leng2, double ap[])
{
   int i, size, proc_id;
   double ghost;
   struct data *data;

   data = (struct data *) mydata;
   size = data->to_size;
   proc_id   = data->processor_info[PROC_ID];
   for (i = 0; i < size; i++ ) {
      ap[i] = 2*p[i];
      if (i !=      0) ap[i] -= p[i-1];
      if (i != size-1) ap[i] -= p[i+1];
   }
   ghost = get_boundary(p, size, data->processor_info);
   if (proc_id == 0) ap[size-1] -= ghost;
   else              ap[0]      -= ghost;

   return 0;
}

int myrestrict(void *mydata, int leng1, double p[], int leng2, double ap[])
{
   int i, fine_i, coarse_size, proc_id;
   struct data *data;
   double ghost;


   data = (struct data *) mydata;
   coarse_size = data->to_size;
   proc_id   = data->processor_info[PROC_ID];
   fine_i = 1 - proc_id;
   for (i = 0; i < coarse_size; i++) {
      ap[i] = .5*p[fine_i] + .25*p[fine_i+1];
      if (fine_i != 0) ap[i] += .25*p[fine_i-1];
      ap[i] *= 4.;
      fine_i += 2;
   }
   ghost = get_boundary(p, fine_i, data->processor_info);
   if (proc_id == 1) ap[0] += ghost;
   return 0;
}
int mysmooth(void *mydata, int leng1, double x[], int leng2, double rhs[])
{
   int i, size, ntimes, j, k, color, proc_id;
   struct data *data;
   double ghost;

   data    = (struct data *) mydata;
   size    = data->size;
   ntimes  = data->ntimes;
   proc_id = data->processor_info[PROC_ID];

   if (size == 0) return 0;

   for (j = 0; j < ntimes; j++) {
      color = proc_id;
      for (k = 0; k < 2; k++) {
         for (i = color; i < size; i += 2) {
            x[i] = rhs[i]/4. + x[i]/2.;
            if (i !=      0) x[i] += x[i-1]/4.;
            if (i != size-1) x[i] += x[i+1]/4.;
         }
         ghost = get_boundary(x, size, data->processor_info);
         if (color == 0) {
            if (proc_id == 0) x[size-1] += ghost/4.;
            else              x[     0] += ghost/4.;
         }
         color = 1 - color;
      }
   }
   return 0;
}
double get_boundary(double x[], int size, int *processor_info)
{
   double ghost;
#ifdef ML_MPI
   int proc_id;
MPI_Status status;
MPI_Request request;
#endif

   ghost = 0.0;
   if (processor_info[NUM_PROCS] == 2) {
#ifdef ML_MPI
      proc_id   = processor_info[PROC_ID];
      if (proc_id == 0) {
         MPI_Irecv(&ghost,1,MPI_DOUBLE,1,123,MPI_COMM_WORLD,&request);
         MPI_Send(&(x[size-1]), 1, MPI_DOUBLE, 1, 123, MPI_COMM_WORLD);
         MPI_Wait(&request, &status);
            }
      if (proc_id == 1) {
         MPI_Irecv(&ghost,1,MPI_DOUBLE,0,123,MPI_COMM_WORLD,&request);
         MPI_Send(&(x[0]), 1, MPI_DOUBLE, 0, 123, MPI_COMM_WORLD);
         MPI_Wait(&request, &status);
      }
#else
         printf("Error: Program not compiled with -DML_MPI\n");
         exit(1);
#endif
   }
   return(ghost);
}




int myAgetrow(void *data, int N_requested_rows, int requested_rows[],
   int allocated_space, int columns[], double values[], int row_lengths[])
{
   int ncount = 0, i, row, Nrow, proc, Nprocs;
   struct data *mydata;

   mydata = (struct data *) data; 
   Nrow = mydata->to_size;
   proc   = mydata->processor_info[PROC_ID];
   Nprocs = mydata->processor_info[NUM_PROCS];
   for (i = 0; i < N_requested_rows; i++) {
      row = requested_rows[i];
      row_lengths[i] = 1;
      if (row !=       0) row_lengths[i]++;
      else if (proc == 1) row_lengths[i]++;

      if (row !=  Nrow-1) row_lengths[i]++;
      else if ((Nprocs==2)&&(proc==0)) row_lengths[i]++;
      if (allocated_space < ncount+row_lengths[i]) return(0);
      
      columns[ncount] = row; values[ncount++] = 2.;
      if (row !=      0 ) { columns[ncount] = row-1; values[ncount++] = -1.;}
      else if (proc == 1) { columns[ncount] =  Nrow; values[ncount++] = -1.;}
      if (row !=  Nrow-1) { columns[ncount] = row+1; values[ncount++] = -1.;}
      else if ((Nprocs==2)&&(proc==0))
			  { columns[ncount] =  Nrow; values[ncount++] = -1.;}
   }
   return(1);
}
int myRgetrow(void *data, int N_requested_rows, int requested_rows[],
   int allocated_space, int columns[], double values[], int row_lengths[])
{
   int ncount = 0, i, row, Nrow;
   int col;
   struct data *mydata;
   int proc;

   mydata = (struct data *) data; 
   Nrow = mydata->to_size;
   proc   = mydata->processor_info[PROC_ID];

   for (i = 0; i < N_requested_rows; i++) {
      row = requested_rows[i];
      col = 2*row+1-proc;
      row_lengths[i] = 3;
      if (allocated_space < ncount+row_lengths[i]) return(0);
      columns[ncount] = col  ; values[ncount++] = 2.;
      if (col == 0)  {columns[ncount] = 2*Nrow; values[ncount++] = 1.;}
      else {columns[ncount] = col-1; values[ncount++] = 1.;}
      columns[ncount] = col+1; values[ncount++] = 1.;
   }
   return(1);
}
int myPgetrow(void *data, int N_requested_rows, int requested_rows[],
   int allocated_space, int columns[], double values[], int row_lengths[])
{
   int ncount = 0, i, row, Nrow, col, proc, oldcount, right_neighbor;
   struct data *mydata;

   mydata = (struct data *) data; 
   Nrow = mydata->to_size;
   proc   = mydata->processor_info[PROC_ID];
   right_neighbor = 0;
   if ((mydata->processor_info[NUM_PROCS] == 2) && (proc == 0))
      right_neighbor = 1;

   for (i = 0; i < N_requested_rows; i++) {
      oldcount = ncount;
      row = requested_rows[i];
      col = (row+proc)/2 ;

      if ( ((row+proc)%2) == 1) {
         if (allocated_space <= ncount) return(0);
         columns[ncount] = col; values[ncount++] = 1.;
      }
      else {
         if (row != 0)  {
            if (allocated_space <= ncount) return(0);
            columns[ncount] = col-1; values[ncount++] = .5;
         }
         if ((row != Nrow-1) || (right_neighbor)) {
            if (allocated_space <= ncount) return(0);
            columns[ncount] = col; values[ncount++] = .5;
         }
      }
      row_lengths[i] = ncount - oldcount;
   }
   return(1);
}


extern void sample1(struct data *Afine_data, struct data *Acoarse_data,
	     struct data  *Rmat_data, struct data    *Pmat_data,
	     double *sol, double *rhs );
extern void sample2(struct data *Afine_data, struct data *Acoarse_data,
	     struct data  *Rmat_data, struct data    *Pmat_data,
	     double *sol, double *rhs );
extern void sample3(struct data *Afine_data, struct data *Acoarse_data,
	     struct data  *Rmat_data, struct data    *Pmat_data,
	     double *sol, double *rhs );

int main(int argc, char *argv[]) 
{
   struct data  Rmat_data, Pmat_data, Afine_data, Acoarse_data;
   int          processor_info[2], i;
   double       *sol, *rhs;
   int          Nfine, Ncoarse;

   Nfine       = 1023;  /* must be an odd number */

#ifdef ML_MPI
   MPI_Init(&argc,&argv);
   MPI_Comm_size(MPI_COMM_WORLD, &(processor_info[NUM_PROCS]) );
   MPI_Comm_rank(MPI_COMM_WORLD, &(processor_info[PROC_ID  ]) );    
#else
   processor_info[PROC_ID  ] = 0;
   processor_info[NUM_PROCS] = 1;
#endif

   ML_Set_PrintLevel(10);

   /* partition across processors */

   if (processor_info[NUM_PROCS] == 2)
      Nfine       =   Nfine/2 + processor_info[PROC_ID];
   Ncoarse           = Nfine/2;

   /* initial guess and righthand side */

   sol = (double *) malloc(Nfine*sizeof(double));
   rhs = (double *) malloc(Nfine*sizeof(double));
   for (i = 0; i < Nfine; i++) rhs[i] = 0.0;
   for (i = 0; i < Nfine; i++) sol[i] = (double) i;
   if (processor_info[PROC_ID] == 1)
      for (i = 0; i < Nfine; i++) sol[i] += (double) (Nfine - 1);

   Afine_data.to_size        = Nfine;
   Afine_data.from_size      = Nfine;
   Afine_data.processor_info = processor_info;
   Afine_data.coarse_or_fine = FINE_GRID;

   Acoarse_data.to_size        = Ncoarse;
   Acoarse_data.from_size      = Ncoarse;
   Acoarse_data.processor_info = processor_info;
   Acoarse_data.coarse_or_fine = COARSE_GRID;

   Rmat_data.from_size         = Nfine;
   Rmat_data.to_size           = Ncoarse;
   Rmat_data.processor_info    = processor_info;

   Pmat_data.from_size         = Ncoarse;
   Pmat_data.to_size           = Nfine;
   Pmat_data.processor_info    = processor_info;

    sample2(&Afine_data, &Acoarse_data, &Rmat_data, &Pmat_data, sol, rhs);   

   free(sol); free(rhs);
#ifdef ML_MPI
   MPI_Finalize();
#endif
   return 0;
}


void sample3(struct data *Afine_data, struct data *Acoarse_data,
	     struct data  *Rmat_data, struct data    *Pmat_data,
	     double *sol, double *rhs )
{
   ML *my_ml;
   double *diagonal;
   int    i, fine_grid, output_level = 10, N_grids = 2, grid0 = 1, grid1 = 0;
   int    Nfine, Ncoarse;

   Nfine    = Rmat_data->from_size;
   Ncoarse  = Rmat_data->to_size;
   diagonal = (double *) malloc(Nfine*sizeof(double));
   for (i = 0; i < Nfine; i++) diagonal[i] = 2.;

   fine_grid = grid1;
   ML_Create         (&my_ml, N_grids);
   ML_Set_OutputLevel(my_ml, output_level);

   ML_Init_Amatrix      (my_ml, grid1, Nfine, Nfine, (void *) Afine_data);
   ML_Set_Amatrix_Matvec(my_ml, grid1, mymatvec  );
   ML_Set_Amatrix_Diag  (my_ml, grid1, Nfine, diagonal);
   ML_Gen_Smoother_Jacobi(my_ml, grid1, ML_PRESMOOTHER, 2, ML_DEFAULT);

   ML_Init_Amatrix      (my_ml, grid0, Ncoarse, Ncoarse, (void *) Acoarse_data);
   ML_Set_Amatrix_Matvec(my_ml, grid0, mymatvec);
   ML_Set_Amatrix_Diag  (my_ml, grid0, Ncoarse, diagonal);
   ML_Gen_Smoother_Jacobi(my_ml, grid0, ML_PRESMOOTHER, 200, ML_DEFAULT);


   ML_Init_Prolongator(my_ml, grid0, grid1, Ncoarse, Nfine, (void*)Pmat_data);
   ML_Set_Prolongator_Matvec(my_ml,  grid0, myinterp);

   ML_Init_Restrictor(my_ml, grid1, grid0, Nfine, Ncoarse,(void *)Rmat_data);
   ML_Set_Restrictor_Matvec(my_ml,  grid1, myrestrict);

   ML_Gen_Solver    (my_ml, 0, fine_grid, grid0);
   free(diagonal);

   ML_Iterate(my_ml, sol, rhs);
   
}

void sample1(struct data *Afine_data, struct data *Acoarse_data,
	     struct data  *Rmat_data, struct data    *Pmat_data,
	     double *sol, double *rhs )
{
   ML  *my_ml;
   int i;
   int fine_grid, output_level = 10, N_grids = 2, grid0 = 0, grid1 = 1;
   int Nfine, Ncoarse;
   double *diagonal;

   Nfine    = Rmat_data->from_size;
   Ncoarse  = Rmat_data->to_size;
   diagonal = (double *) malloc(Nfine*sizeof(double));
   for (i = 0; i < Nfine; i++) diagonal[i] = 2.;

   fine_grid   = grid1;
   ML_Create         (&my_ml, N_grids);
   ML_Set_OutputLevel( my_ml, output_level);

   ML_Init_Amatrix      (my_ml, grid1,  Nfine, Nfine,(void *) Afine_data);
   ML_Set_Amatrix_Getrow(my_ml, grid1,  myAgetrow, my_comm, Nfine+1);
   ML_Set_Amatrix_Matvec(my_ml, grid1,  mymatvec);
   ML_Set_Amatrix_Diag  (my_ml, grid1,  Nfine, diagonal);
   ML_Gen_Smoother_Jacobi(my_ml, grid1,  ML_PRESMOOTHER, 2, ML_DEFAULT);

   ML_Init_Prolongator(my_ml, grid0, grid1, Ncoarse,Nfine,(void *)Pmat_data);
   ML_Set_Prolongator_Getrow(my_ml,  grid0, myPgetrow, my_comm, Ncoarse+1);
   ML_Set_Prolongator_Matvec(my_ml,  grid0, myinterp);

   ML_Init_Restrictor(my_ml, grid1, grid0, Nfine, Ncoarse,(void *)Rmat_data);
   ML_Set_Restrictor_Getrow(my_ml,  grid1, myRgetrow, my_comm, Nfine+1);
   ML_Set_Restrictor_Matvec(my_ml,  grid1, myrestrict);


   ML_Gen_AmatrixRAP(my_ml,grid1, grid0);
   ML_Gen_CoarseSolverSuperLU(my_ml, grid0);

/* ML_Gen_Smoother_Jacobi(my_ml, grid0, ML_PRESMOOTHER, 200, ML_DEFAULT); */
/* ML_Gen_SmootherGaussSeidel(my_ml, grid0, ML_PRESMOOTHER, 200);        */
   ML_Gen_Solver    (my_ml, 0, fine_grid, grid0);
   ML_Iterate(my_ml, sol, rhs);

}

void sample2(struct data *Afine_data, struct data *Acoarse_data,
	     struct data  *Rmat_data, struct data    *Pmat_data,
	     double *sol, double *rhs)
{
   ML *my_ml;
   struct data fsmooth, csmooth;
   int fine_grid, output_level = 10, N_grids = 10, grid0 = 1, grid1 = 0;
   int Nfine, Ncoarse, i, N_mglevels;
   double *diagonal;

   Nfine   = Rmat_data->from_size;
   Ncoarse = Rmat_data->to_size;

   fsmooth.size           = Nfine;
   fsmooth.ntimes         = 4;
   fsmooth.processor_info = Afine_data->processor_info;
   csmooth.size           = Ncoarse;
   csmooth.processor_info = Afine_data->processor_info;
   csmooth.ntimes         = 1000;
   fine_grid = grid1;


   ML_Create         (&my_ml, N_grids);
/* Example 1: Basic, we give all the operators

   ML_Init_Amatrix      (my_ml, grid1,   Nfine,   Nfine, (void *) Afine_data);
   ML_Set_Amatrix_Matvec(my_ml, grid1,  mymatvec);
   ML_Set_Smoother   (my_ml, grid1,    ML_PRESMOOTHER, (void *)&fsmooth,mysmooth,NULL);

   ML_Init_Amatrix      (my_ml, grid0, Ncoarse, Ncoarse, (void *) Acoarse_data);
   ML_Set_Amatrix_Matvec(my_ml, grid0,  mymatvec);
   ML_Set_Smoother   (my_ml, grid0,    ML_PRESMOOTHER, (void *)&csmooth,mysmooth,NULL);

   ML_Init_Restrictor(my_ml, grid1, grid0, Nfine, Ncoarse,(void *)Rmat_data);
   ML_Set_Restrictor_Matvec(my_ml,  grid1, myrestrict);

   ML_Init_Prolongator(my_ml, grid0, grid1, Ncoarse, Nfine,(void *)Pmat_data);
   ML_Set_Prolongator_Matvec(my_ml,  grid0, myinterp);


   ML_Gen_Solver    (my_ml, 0, fine_grid, grid0 );
   ML_Iterate(my_ml, sol, rhs);
*/
/* Example 2: We give diagonal and let ML do Jacobi
   diagonal = (double *) malloc(Nfine*sizeof(double));
   for (i = 0; i < Nfine; i++) diagonal[i] = 2.;

   ML_Init_Amatrix      (my_ml, grid1,   Nfine,   Nfine, (void *) Afine_data);
   ML_Set_Amatrix_Matvec(my_ml, grid1,  mymatvec);
   ML_Set_Amatrix_Diag  (my_ml, grid1, Nfine, diagonal);
   ML_Gen_Smoother_Jacobi(my_ml, grid1, ML_PRESMOOTHER, 2, ML_DEFAULT);

   ML_Init_Amatrix      (my_ml, grid0, Ncoarse, Ncoarse, (void *) Acoarse_data);
   ML_Set_Amatrix_Matvec(my_ml, grid0,  mymatvec);
   ML_Set_Smoother   (my_ml, grid0,    ML_PRESMOOTHER, (void *)&csmooth,mysmooth,NULL);

   ML_Init_Restrictor(my_ml, grid1, grid0, Nfine, Ncoarse,(void *)Rmat_data);
   ML_Set_Restrictor_Matvec(my_ml,  grid1, myrestrict);

   ML_Init_Prolongator(my_ml, grid0, grid1, Ncoarse, Nfine,(void *)Pmat_data);
   ML_Set_Prolongator_Matvec(my_ml,  grid0, myinterp);


   ML_Gen_Solver    (my_ml, 0, fine_grid, grid0 );
   ML_Iterate(my_ml, sol, rhs);
*/

/* Example 3: We give getrow and let ML do GS
   ML_Init_Amatrix      (my_ml, grid1,   Nfine,   Nfine, (void *) Afine_data);
   ML_Set_Amatrix_Matvec(my_ml, grid1,  mymatvec);
   ML_Set_Amatrix_Getrow(my_ml, grid1,  myAgetrow, my_comm, Nfine+1);
   ML_Gen_SmootherGaussSeidel(my_ml, grid1, ML_PRESMOOTHER, 2); 

   ML_Init_Amatrix      (my_ml, grid0, Ncoarse, Ncoarse, (void *) Acoarse_data);
   ML_Set_Amatrix_Matvec(my_ml, grid0,  mymatvec);
   ML_Set_Smoother   (my_ml, grid0,    ML_PRESMOOTHER, (void *)&csmooth,mysmooth, NULL);

   ML_Init_Restrictor(my_ml, grid1, grid0, Nfine, Ncoarse,(void *)Rmat_data);
   ML_Set_Restrictor_Matvec(my_ml,  grid1, myrestrict);

   ML_Init_Prolongator(my_ml, grid0, grid1, Ncoarse, Nfine,(void *)Pmat_data);
   ML_Set_Prolongator_Matvec(my_ml,  grid0, myinterp);


   ML_Gen_Solver    (my_ml, 0, fine_grid, grid0 );
   ML_Iterate(my_ml, sol, rhs);
*/
/* Example 4: We give all getrows and let ML do GS, and RAP
   ML_Init_Amatrix      (my_ml, grid1,   Nfine,   Nfine, (void *) Afine_data);
   ML_Set_Amatrix_Matvec(my_ml, grid1,  mymatvec);
   ML_Set_Amatrix_Getrow(my_ml, grid1,  myAgetrow, my_comm, Nfine+1);
   ML_Gen_SmootherGaussSeidel(my_ml, grid1, ML_PRESMOOTHER, 2); 

   ML_Init_Restrictor(my_ml, grid1, grid0, Nfine, Ncoarse,(void *)Rmat_data);
   ML_Set_Restrictor_Matvec(my_ml,  grid1, myrestrict);
   ML_Set_Restrictor_Getrow(my_ml,  grid1, myRgetrow, my_comm, Nfine+1);

   ML_Init_Prolongator(my_ml, grid0, grid1, Ncoarse, Nfine,(void *)Pmat_data);
   ML_Set_Prolongator_Matvec(my_ml,  grid0, myinterp);
   ML_Set_Prolongator_Getrow(my_ml,  grid0, myPgetrow, my_comm, Ncoarse+1);

   ML_Gen_AmatrixRAP(my_ml,grid1, grid0);
   ML_Gen_SmootherGaussSeidel(my_ml, ML_ALL_LEVELS, ML_PRESMOOTHER, 2); 

   ML_Gen_Solver    (my_ml, 0, fine_grid, grid0 );
   ML_Iterate(my_ml, sol, rhs);
*/
/* Example 5: We give all getrows and let ML do GS/SuperLU and RAP
   ML_Init_Amatrix      (my_ml, grid1,   Nfine,   Nfine, (void *) Afine_data);
   ML_Set_Amatrix_Matvec(my_ml, grid1,  mymatvec);
   ML_Set_Amatrix_Getrow(my_ml, grid1,  myAgetrow, my_comm, Nfine+1);

   ML_Init_Restrictor(my_ml, grid1, grid0, Nfine, Ncoarse,(void *)Rmat_data);
   ML_Set_Restrictor_Matvec(my_ml,  grid1, myrestrict);
   ML_Set_Restrictor_Getrow(my_ml,  grid1, myRgetrow, my_comm, Nfine+1);

   ML_Init_Prolongator(my_ml, grid0, grid1, Ncoarse, Nfine,(void *)Pmat_data);
   ML_Set_Prolongator_Matvec(my_ml,  grid0, myinterp);
   ML_Set_Prolongator_Getrow(my_ml,  grid0, myPgetrow, my_comm, Ncoarse+1);

   ML_Gen_AmatrixRAP(my_ml,grid1, grid0);
   ML_Gen_SmootherGaussSeidel(my_ml, ML_ALL_LEVELS, ML_PRESMOOTHER, 2); 
   ML_Gen_CoarseSolverSuperLU(my_ml, grid0);

   ML_Gen_Solver    (my_ml, 0, fine_grid, grid0 );
   ML_Iterate(my_ml, sol, rhs);
*/
/* Example 6: We give matrix getrows and let ML do Vanek
   ML_Init_Amatrix      (my_ml, grid1,   Nfine,   Nfine, (void *) Afine_data);
   ML_Set_Amatrix_Matvec(my_ml, grid1,  mymatvec);
   ML_Set_Amatrix_Getrow(my_ml, grid1,  myAgetrow, my_comm, Nfine+1);
   N_mglevels = ML_Gen_MGHierarchyVanek(my_ml, grid1, ML_INCREASING);

   ML_Gen_SmootherGaussSeidel(my_ml, ML_ALL_LEVELS, ML_POSTSMOOTH, 2);
   ML_Gen_Solver    (my_ml, 0, fine_grid, grid0 );
   ML_Iterate(my_ml, sol, rhs);
*/
/* Example 7: We give matrix getrow, two grids, and let ML do FE basis
   ML_Init_Amatrix      (my_ml, grid1,   Nfine,   Nfine, (void *) Afine_data);
   ML_Set_Amatrix_Matvec(my_ml, grid1,  mymatvec);
   ML_Set_Amatrix_Getrow(my_ml, grid1,  myAgetrow, my_comm, Nfine+1);

   ML_Init_Grid(my_ml, grid1, (void *) Afine_data);
   ML_Init_Grid(my_ml, grid0, (void *) Acoarse_data);
   ML_Set_Grid_GetNVert(my_ml, grid0, mynvertices);
   ML_Set_Grid_GetNVert(my_ml, grid1, mynvertices);
   ML_Set_Grid_GetDimension(my_ml, grid0, mydimension);
   ML_Set_Grid_GetDimension(my_ml, grid1, mydimension);
   ML_Set_Grid_GetNElmnt(my_ml, grid0, mynelements);
   ML_Set_Grid_GetElmntNVert(my_ml, grid0, myelement_nvertices);
   ML_Set_Grid_GetElmntVertList(my_ml, grid0, myelement_vlist);
   ML_Set_Grid_GetVertGlobalNum(my_ml, grid0, myvertex_global_num);
   ML_Set_Grid_GetElmntGlobalNum(my_ml, grid0, myelement_global_num);
   ML_Set_Grid_GetVertCoordinate(my_ml, grid0, myvertex_coordinate);
   ML_Set_Grid_GetVertCoordinate(my_ml, grid1, myvertex_coordinate);
   ML_Set_Grid_ComputeBasisCoef(my_ml, grid0, mybasis_coefficients);

   ML_Gen_GridXsferUsingFEBasis(my_ml,grid1,grid0,1);
   ML_Gen_AmatrixRAP(my_ml,grid1, grid0);
   ML_Gen_SmootherGaussSeidel(my_ml, ML_ALL_LEVELS, ML_PRESMOOTHER, 2);
   ML_Gen_CoarseSolverSuperLU(my_ml, grid0);

   ML_Gen_Solver    (my_ml, 0, fine_grid, grid0 );
   ML_Iterate(my_ml, sol, rhs);
*/

/* ML_GenSmootherAztec(....) */
}

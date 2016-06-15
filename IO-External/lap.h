
  #define BIG 100000
  typedef int row;
  typedef int col;
  typedef int cost;


extern int lap(int dim, int **assigncost,
               int *rowsol, int *colsol, int *u, int *v);

extern void checklap(int dim, int **assigncost,
                     int *rowsol, int *colsol, int *u, int *v);


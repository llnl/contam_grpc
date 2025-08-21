#include "contam_client.h"
#if defined(USE_MPI)
#include <mpi.h>
#endif

int main (int argc, char* argv[]) {

  Contam_gRPC::pickContamArgs(argc, argv);

  int my_rank = 0;

#if defined(USE_MPI)
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
#endif

  Contam_gRPC::initContamClientState(my_rank);

#if defined(USE_MPI)
  MPI_Finalize();
#endif
  return 0;
}

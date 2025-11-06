#include "utils.h"

#include "contam_client.h"
#if defined(USE_MPI)
#include <mpi.h>
#endif

int main(int argc, char* argv[])
{
  Contam_gRPC::pickContamArgs(argc, argv);

  int my_rank = 0;

#if defined(USE_MPI)
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
#endif

  double t_start = Contam_gRPC::get_time();
  Contam_gRPC::initContamClientState(my_rank);
  Contam_gRPC::contamClient();
  std::cout << "Time: " << Contam_gRPC::get_time() - t_start << std::endl;

  Contam_gRPC::contamClientShutDown();

#if defined(USE_MPI)
  MPI_Finalize();
#endif

  return 0;
}

/*
 * Copyright 2026 Lawrence Livermore National Security, LLC.
 * See the top-level LICENSE file for details.

 * SPDX-License-Identifier: BSD-3
 */

#include "utils.h"

#include <cstdlib>

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
  if (!Contam_gRPC::initContamClientState(my_rank)) {
    return EXIT_FAILURE;
  }
  Contam_gRPC::runContamClient();
  std::cout << "Time: " << Contam_gRPC::get_time() - t_start << std::endl;

#if defined(USE_MPI)
  MPI_Finalize();
#endif

  return EXIT_SUCCESS;
}

#ifndef CONTAM_GRPC_CONTAM_DRIVER_H
#define CONTAM_GRPC_CONTAM_DRIVER_H
#include <string>

namespace Contam_gRPC {

void runContam(std::string const& sim_id,
               std::string const& prj_fname,
               std::string const& out_fname,
               std::string const& err_fname,
               bool verbose);

}  // namespace Contam_gRPC

#endif  // CONTAM_GRPC_CONTAM_DRIVER_H

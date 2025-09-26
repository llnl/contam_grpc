#ifndef CONTAM_GRPC_CONTAM_DRIVER_H
#define CONTAM_GRPC_CONTAM_DRIVER_H
#include <string>

namespace Contam_gRPC {

void runContam(const std::string& sim_id, const std::string& prj_fname,
               const std::string& out_fname, const std::string& err_fname);

} // end of namespace

#endif // CONTAM_GRPC_CONTAM_DRIVER_H

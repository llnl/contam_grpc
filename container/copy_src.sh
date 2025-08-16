# Run this script before building the container using Dockerfile
# This will copy the source code and make a tarball that will be used inside of the container

#!/bin/sh
tarball=contam_grpc
mkdir -p ${tarball}
echo ${tarball}
src=$PWD/${tarball}
pushd .. 2> /dev/null
ff=`ls -1 *.{h,cpp,prj,cmake,txt,md}`
for f in $ff
do
  cp $f ${src}
done
cp -r proto ${src}
popd 2> /dev/null
tar zcvf `basename ${tarball}`.tar.gz ${tarball}
rm -rf ${tarball}

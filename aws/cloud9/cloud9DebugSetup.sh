#!/bin/bash

mkdir ~/environment/.c9/runners
echo 'creating pea.run'

cat <<EOF >~/environment/.c9/runners/pea.run
// This file overrides the built-in AWS C++ runner
// For more information see http://docs.aws.amazon.com/console/cloud9/change-runner
{
  "script": [
    "mkdir -p /data/acs/ginan/src/build/",
    "cd /data/acs/ginan/src/build/",
    "cmake ../",
    "make -j8 pea",
  
  //edit this line to set executable to run  
    "node $HOME/.c9/bin/c9gdbshim.js pea --config /data/acs/ginan/examples/ex11_pea_pp_user_gps.yaml"
  ],
  "info": "Running cpp $file",
  "debugger": "gdb",
  "$debugDefaultState": true,
  "selector": "^.*\\.(cpp|cc|hpp|h)$",
  "trackId": "Cplusplus"
}
EOF
echo 'Done'
echo 'To debug: Open a cpp file,'
echo 'Click Run - Run With - (Wait) - Pea,'
echo 'Will compile and run in debug mode'
echo 'edit ~/environment/.c9/runners/pea.run to set config options etc'

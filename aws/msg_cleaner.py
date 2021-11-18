import os

with open("..\CMakeLists.txt", "r") as file:
    filedata = file.read()

# replace the target string
# comment out fortran flags
filedata = filedata.replace(
    'set (CMAKE_Fortran_FLAGS "-g -frecursive -fall-intrinsics -Wmaybe-uninitialized -fcheck=all -fbacktrace")',
    '#set (CMAKE_Fortran_FLAGS "-g -frecursive -fall-intrinsics -Wmaybe-uninitialized -fcheck=all -fbacktrace")',
)
# uncomment github flags
with open("CMakeLists.txt", "w") as file:
    file.write(filedata)

filedata = filedata.replace(
    '#set (CMAKE_Fortran_FLAGS "-g -frecursive -fall-intrinsics -Wmaybe-uninitialized -Wunused-dummy-argument -fcheck=all -fbacktrace")',
    'set (CMAKE_Fortran_FLAGS "-g -frecursive -fall-intrinsics -Wmaybe-uninitialized -Wunused-dummy-argument -fcheck=all -fbacktrace")',
)

# Write the file out again
with open("CMakeLists.txt", "w") as file:
    file.write(filedata)

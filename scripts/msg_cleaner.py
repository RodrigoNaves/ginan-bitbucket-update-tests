import os
#This code is only to clean messages originated in CMakeListst.txt it should not be changed
#This code will not be run during operation of Ginan source code

#Read in the file

#path = os.path.dirname(__file__)
#print("this is the path", path)
with open('..\src\CMakeLists.txt','r+') as file:
    filedata = file.read()
    filedata = filedata.replace('set (CMAKE_Fortran_FLAGS "-g -frecursive -fall-intrinsics -Wmaybe-uninitialized -fcheck=all -fbacktrace")','#set (CMAKE_Fortran_FLAGS "-g -frecursive -fall-intrinsics -Wmaybe-uninitialized -fcheck=all -fbacktrace")')
    file.write(filedata)
    filedata = filedata.replace('#set (CMAKE_Fortran_FLAGS "-g -frecursive -fall-intrinsics -Wmaybe-uninitialized -Wunused-dummy-argument -fcheck=all -fbacktrace")','set (CMAKE_Fortran_FLAGS "-g -frecursive -fall-intrinsics -Wmaybe-uninitialized -Wunused-dummy-argument -fcheck=all -fbacktrace")')
    file.write(filedata)
#replace the target string
#comment out fortran flags

#uncomment github flags



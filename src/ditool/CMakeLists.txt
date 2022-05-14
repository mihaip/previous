project (ditool)

add_executable (ditool ditool.cpp DiskImage.cpp Partition.cpp UFS.cpp VirtualFS.cpp)
if(WIN32)
	target_link_libraries(ditool ws2_32 Iphlpapi)
endif(WIN32)

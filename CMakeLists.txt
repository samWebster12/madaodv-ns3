set(source_files
    model/madaodv-dpd.cc
    model/madaodv-id-cache.cc
    model/madaodv-neighbor.cc
    model/madaodv-packet.cc
    model/madaodv-routing-protocol.cc
    model/madaodv-rqueue.cc
    model/madaodv-rtable.cc
    model/hybrid-wifi-mac.cc
    helper/madaodv-helper.cc
)

set(header_files
    model/madaodv-dpd.h
    model/madaodv-id-cache.h
    model/madaodv-neighbor.h
    model/madaodv-packet.h
    model/madaodv-routing-protocol.h
    model/madaodv-rqueue.h
    model/madaodv-rtable.h
    model/hybrid-wifi-mac.h
    helper/madaodv-helper.h
)

build_lib(
  LIBNAME madaodv
  SOURCE_FILES ${source_files}
  HEADER_FILES ${header_files}
  LIBRARIES_TO_LINK
    ${libwifi}
    ${libnetwork}
    ${libinternet}
    ${libmobility}
  TEST_SOURCES
    
)


build_lib_example(
  NAME madaodv-example1
  SOURCE_FILES madaodv-example.cc
  LIBRARIES_TO_LINK
  ${libmobility} ${libnetwork} ${libmobility} ${libwifi} ${libinternet-apps} ${libpoint-to-point}
)

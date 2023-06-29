build_lib_example(
  NAME madaodv
  SOURCE_FILES madaodv.cc
  LIBRARIES_TO_LINK
    ${libwifi}
    ${libinternet}
    ${libmadaodv}
    ${libinternet-apps}
)

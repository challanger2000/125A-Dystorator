# JUCE 8.0.4 VST3 moduleinfo.json may contain trailing commas in objects/arrays.
# Steinberg validator may pass while strict JSON parsers reject that manifest.
# Canonicalize only metadata after VST3 link: DSP executable bytes unchanged.
if(NOT DEFINED MODULE_INFO_PATH OR NOT EXISTS "${MODULE_INFO_PATH}")
  message(FATAL_ERROR "Missing VST3 moduleinfo.json: ${MODULE_INFO_PATH}")
endif()
file(READ "${MODULE_INFO_PATH}" moduleinfo)
string(REGEX REPLACE ",([ \t\r\n]*)}" "\\1}" moduleinfo "${moduleinfo}")
string(REGEX REPLACE ",([ \t\r\n]*)\\]" "\\1]" moduleinfo "${moduleinfo}")
string(JSON _name ERROR_VARIABLE err1 GET "${moduleinfo}" Name)
string(JSON _vendor ERROR_VARIABLE err2 GET "${moduleinfo}" "Factory Info" Vendor)
string(JSON _category ERROR_VARIABLE err3 GET "${moduleinfo}" Classes 0 Category)
string(JSON _className ERROR_VARIABLE err4 GET "${moduleinfo}" Classes 0 Name)
foreach(_error IN ITEMS "${err1}" "${err2}" "${err3}" "${err4}")
  if(NOT _error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Invalid strict moduleinfo JSON: ${_error}")
  endif()
endforeach()
if(NOT _name STREQUAL "125A Dystorator" OR
   NOT _vendor STREQUAL "125A Systems" OR
   NOT _className STREQUAL "125A Dystorator" OR
   NOT _category STREQUAL "Audio Module Class")
  message(FATAL_ERROR "Unexpected VST3 factory/class identity")
endif()
file(WRITE "${MODULE_INFO_PATH}" "${moduleinfo}")
message(STATUS "Strict JSON VST3 metadata validated: ${_name}, ${_vendor}")
# Install script for directory: /home/elchaschab/devel/PitchDetect/third/aquila/examples

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/utility_functions/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/frame_iteration/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/wave_info/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/wave_iteration/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/window_plot/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/window_usage/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/text_plot/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/sine_generator/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/square_generator/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/triangle_generator/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/generators/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/am_modulation/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/fft_comparison/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/fft_simple_spectrum/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/fft_filter/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/spectrogram/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/mfcc_calculation/cmake_install.cmake")
  include("/home/elchaschab/devel/PitchDetect/third/aquila/examples/dtw_path_recovery/cmake_install.cmake")

endif()


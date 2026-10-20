# CMake generated Testfile for 
# Source directory: /home/hugo/perso/morgane/tests
# Build directory: /home/hugo/perso/morgane/build/linux-debug/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[match crowd]=] "/usr/bin/python3" "/home/hugo/perso/morgane/tests/match_test.py" "crowd" "/home/hugo/perso/morgane/build/linux-debug/bin/r-type_server" "/home/hugo/perso/morgane/build/linux-debug/bin/r-type_bot")
set_tests_properties([=[match crowd]=] PROPERTIES  LABELS "network" TIMEOUT "120" _BACKTRACE_TRIPLES "/home/hugo/perso/morgane/tests/CMakeLists.txt;8;add_test;/home/hugo/perso/morgane/tests/CMakeLists.txt;0;")
add_test([=[match crash]=] "/usr/bin/python3" "/home/hugo/perso/morgane/tests/match_test.py" "crash" "/home/hugo/perso/morgane/build/linux-debug/bin/r-type_server" "/home/hugo/perso/morgane/build/linux-debug/bin/r-type_bot")
set_tests_properties([=[match crash]=] PROPERTIES  LABELS "network" TIMEOUT "120" _BACKTRACE_TRIPLES "/home/hugo/perso/morgane/tests/CMakeLists.txt;8;add_test;/home/hugo/perso/morgane/tests/CMakeLists.txt;0;")
add_test([=[match restart]=] "/usr/bin/python3" "/home/hugo/perso/morgane/tests/match_test.py" "restart" "/home/hugo/perso/morgane/build/linux-debug/bin/r-type_server" "/home/hugo/perso/morgane/build/linux-debug/bin/r-type_bot")
set_tests_properties([=[match restart]=] PROPERTIES  LABELS "network" TIMEOUT "120" _BACKTRACE_TRIPLES "/home/hugo/perso/morgane/tests/CMakeLists.txt;8;add_test;/home/hugo/perso/morgane/tests/CMakeLists.txt;0;")
